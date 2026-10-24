package com.huanchen.music.ui.player

import android.content.ContentResolver
import android.net.Uri
import android.provider.OpenableColumns
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.huanchen.music.player.PlayState
import com.huanchen.music.player.PlaybackController

/**
 * 骨架版「最小可发声」验证页。
 *
 * 功能:选文件(你的真实 FLAC/DSD/APE/WAV)→ FFmpeg 解码 → AAudio 系统通路放声。
 * 目的:让你装上真机后,一键验证整条音频链路是否打通。
 *
 * 关键实现:Android SAF 文件选择器返回的是 content:// URI,native 层(FFmpeg)
 *   无法直接打开。这里用 ContentResolver.openFileDescriptor 拿到 fd,再把
 *   /proc/self/fd/<N> 这条路径交给 FFmpeg——这是 Android 上让 C 库访问 SAF 文件的标准做法。
 */
@Composable
fun BootstrapPlayerScreen(
    controller: PlaybackController,
    contentResolver: ContentResolver
) {
    val ui by controller.uiState.collectAsState()

    // SAF 文件选择器:不限定 MIME(audio/* 会漏掉 .dsf/.dff 等非常见后缀),改为全类型再自行过滤
    val picker = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        uri ?: return@rememberLauncherForActivityResult
        val name = queryDisplayName(contentResolver, uri)
        // 关键:把 content URI 打开成 fd(不立即关闭),交给 controller 管理生命周期
        val fd = openFd(contentResolver, uri)
        if (fd >= 0) {
            controller.loadFd(fd, name)
        }
    }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(24.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "幻辰音乐",
                style = MaterialTheme.typography.headlineLarge,
                fontWeight = FontWeight.Bold
            )
            Text(
                text = "最小可发声 · 链路自检版",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(top = 4.dp)
            )

            Spacer(Modifier.height(32.dp))

            // —— 选文件按钮 ——
            Button(onClick = { picker.launch(arrayOf("*/*")) }) {
                Text("选择音频文件(FLAC/DSD/APE/WAV)")
            }

            Spacer(Modifier.height(24.dp))

            // —— 当前文件与格式 ——
            if (ui.fileName.isNotEmpty()) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceVariant
                    )
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Text(
                            text = ui.fileName,
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.SemiBold
                        )
                        if (ui.formatDesc.isNotEmpty()) {
                            Text(
                                text = ui.formatDesc,
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                modifier = Modifier.padding(top = 4.dp)
                            )
                        }
                        Text(
                            text = "状态: ${stateText(ui.state)}",
                            style = MaterialTheme.typography.bodySmall,
                            modifier = Modifier.padding(top = 4.dp)
                        )
                    }
                }

                Spacer(Modifier.height(16.dp))

                // —— 播放控制 ——
                Row {
                    when (ui.state) {
                        PlayState.PLAYING -> {
                            OutlinedButton(onClick = { controller.pause() }) { Text("暂停") }
                        }
                        PlayState.PAUSED, PlayState.STOPPED, PlayState.IDLE -> {
                            Button(onClick = { controller.play() }) { Text("播放") }
                        }
                        else -> {}
                    }
                    Spacer(Modifier.width(12.dp))
                    OutlinedButton(onClick = { controller.stop() }) { Text("停止") }
                }
            }

            // —— 错误提示 ——
            if (ui.error.isNotEmpty()) {
                Spacer(Modifier.height(16.dp))
                Text(
                    text = "出错: ${ui.error}",
                    color = MaterialTheme.colorScheme.error,
                    style = MaterialTheme.typography.bodySmall,
                    textAlign = TextAlign.Center
                )
            }

            Spacer(Modifier.height(32.dp))

            // —— 链路自检信息(FFmpeg 版本,证明 native 链路通) ——
            Text(
                text = ui.ffmpegVersion,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                textAlign = TextAlign.Center
            )
        }
    }
}

private fun stateText(s: PlayState): String = when (s) {
    PlayState.IDLE -> "空闲"
    PlayState.LOADING -> "加载中…"
    PlayState.PLAYING -> "▶ 播放中"
    PlayState.PAUSED -> "⏸ 已暂停"
    PlayState.STOPPED -> "已就绪/播完"
    PlayState.ERROR -> "错误"
}

/** 查询选中文件的显示名。 */
private fun queryDisplayName(resolver: ContentResolver, uri: Uri): String {
    resolver.query(uri, null, null, null, null)?.use { c ->
        val idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        if (idx >= 0 && c.moveToFirst()) return c.getString(idx)
    }
    return uri.lastPathSegment ?: "未知文件"
}

/**
 * 把 content:// URI 打开成一个文件描述符(fd),返回给调用方管理。
 * 返回 -1 表示无法打开(该文件不可读)。
 *
 * 生命周期约定(骨架版):
 *   调用方(controller.loadFd)会把它转成 /proc/self/fd/N 交给 native 的 FFmpeg。
 *   FFmpeg open 该路径等价于 dup(fd),得到独立副本;
 *   因此 native 加载完成后,controller 负责关闭这个原始 fd,避免泄漏。
 */
private fun openFd(resolver: ContentResolver, uri: Uri): Int {
    return runCatching {
        resolver.openFileDescriptor(uri, "r")?.use { pfd ->
            // detachFd 让 fd 脱离 ParcelFileDescriptor 生命周期,由 controller 接管关闭
            pfd.detachFd()
        } ?: -1
    }.getOrDefault(-1)
}
