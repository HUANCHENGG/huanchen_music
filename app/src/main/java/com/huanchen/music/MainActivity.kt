package com.huanchen.music

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.huanchen.music.player.PlaybackController
import com.huanchen.music.ui.player.BootstrapPlayerScreen
import com.huanchen.music.ui.theme.HuanchenTheme

/**
 * 主 Activity —— 单 Activity 架构。
 *
 * 骨架版:承载「最小可发声」验证页(文件选择 → FFmpeg 解码 → AAudio 放声)。
 * 后续会换成 Navigation Compose 承载资料库/播放页等正式页面(见 docs/plan.md)。
 */
class MainActivity : ComponentActivity() {

    // 播放控制器:控制层单例,UI 通过它驱动 native(见 CLAUDE.md 第四节)
    private val playbackController = PlaybackController()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // 沉浸式边到边(Apple Music 风需要内容延伸到大圆角屏/刘海下)
        enableEdgeToEdge()
        setContent {
            HuanchenTheme {
                BootstrapPlayerScreen(
                    controller = playbackController,
                    contentResolver = contentResolver
                )
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        // 退出时停止播放,释放 native 资源
        playbackController.stop()
    }
}
