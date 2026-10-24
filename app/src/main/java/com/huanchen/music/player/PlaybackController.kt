package com.huanchen.music.player

import com.huanchen.music.bridge.NativeBridge
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * 播放状态(与 native 侧 PlayState 枚举序数一一对应)。
 */
enum class PlayState(val nativeValue: Int) {
    IDLE(0), LOADING(1), PLAYING(2), PAUSED(3), STOPPED(4), ERROR(5);

    companion object {
        fun fromNative(v: Int): PlayState = entries.firstOrNull { it.nativeValue == v } ?: ERROR
    }
}

/**
 * 播放状态快照(UI 渲染用)。
 */
data class PlaybackUiState(
    val state: PlayState = PlayState.IDLE,
    val formatDesc: String = "",      // 当前文件格式描述
    val fileName: String = "",        // 当前文件名
    val error: String = "",           // 最近一次错误
    val ffmpegVersion: String = ""    // FFmpeg 版本(链路自检)
)

/**
 * 播放控制器 —— 控制层(见 CLAUDE.md 第四节)。
 *
 * 职责:封装 NativeBridge,把 native 播放状态轮询为 StateFlow 供 UI 订阅;
 *      UI 通过这里调播放控制,不直接碰 JNI。
 *
 * 骨架版说明:状态用「轮询」而非回调(native→Kotlin 主动推送)是刻意简化——
 *   骨架版先把链路打通;正式版会改为 native 回调或 MediaSession。
 */
class PlaybackController {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)

    private val _uiState = MutableStateFlow(PlaybackUiState())
    val uiState: StateFlow<PlaybackUiState> = _uiState.asStateFlow()

    private var polling = false

    init {
        // 初始化时自检 native 引擎与 FFmpeg 链路
        val ffmpeg = runCatching { NativeBridge.nativeGetFFmpegVersion() }
            .getOrElse { "FFmpeg 自检失败: ${it.message}" }
        _uiState.value = _uiState.value.copy(ffmpegVersion = ffmpeg)
    }

    /**
     * 用文件描述符加载并准备播放一个文件(骨架版主入口)。
     *
     * @param fd SAF 打开的文件描述符。native 侧 FFmpeg 会以 /proc/self/fd/N 打开它
     *           (等价于 dup,得到独立副本);加载完成后本方法负责关闭原始 fd,防泄漏。
     * @param displayName 用于 UI 显示的文件名
     */
    fun loadFd(fd: Int, displayName: String) {
        scope.launch {
            val fdPath = "/proc/self/fd/$fd"
            val ok = NativeBridge.nativeLoadFile(fdPath)
            // native 已 open 出自己的 fd 副本(或失败),原始 fd 使命完成,立即关闭防泄漏。
            // 用 adoptFd 重新包装再 close,会正确关闭底层 fd(int)。
            runCatching { android.os.ParcelFileDescriptor.adoptFd(fd).close() }

            if (ok) {
                _uiState.value = _uiState.value.copy(
                    fileName = displayName,
                    formatDesc = NativeBridge.nativeGetFormatDesc(),
                    error = ""
                )
                startPolling()
            } else {
                _uiState.value = _uiState.value.copy(
                    fileName = displayName,
                    state = PlayState.ERROR,
                    error = NativeBridge.nativeGetLastError()
                )
            }
        }
    }

    /**
     * 用绝对路径加载(备用入口,用于后续媒体库的本地真实路径)。
     */
    fun load(filePath: String, displayName: String) {
        scope.launch {
            val ok = NativeBridge.nativeLoadFile(filePath)
            if (ok) {
                _uiState.value = _uiState.value.copy(
                    fileName = displayName,
                    formatDesc = NativeBridge.nativeGetFormatDesc(),
                    error = ""
                )
                startPolling()
            } else {
                _uiState.value = _uiState.value.copy(
                    fileName = displayName,
                    state = PlayState.ERROR,
                    error = NativeBridge.nativeGetLastError()
                )
            }
        }
    }

    fun play() {
        scope.launch {
            NativeBridge.nativePlay()
            startPolling()
        }
    }

    fun pause() {
        scope.launch { NativeBridge.nativePause() }
    }

    fun stop() {
        scope.launch { NativeBridge.nativeStop() }
    }

    /** 轮询 native 状态刷新 UI(骨架版方案)。 */
    private fun startPolling() {
        if (polling) return
        polling = true
        scope.launch {
            while (isActive && polling) {
                val st = PlayState.fromNative(NativeBridge.nativeGetState())
                _uiState.value = _uiState.value.copy(
                    state = st,
                    error = if (st == PlayState.ERROR) NativeBridge.nativeGetLastError() else _uiState.value.error
                )
                delay(200)  // 200ms 刷新,骨架版够用
            }
        }
    }
}
