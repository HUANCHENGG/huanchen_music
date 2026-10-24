package com.huanchen.music

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.huanchen.music.bridge.NativeBridge
import com.huanchen.music.ui.theme.HuanchenTheme

/**
 * 主 Activity —— 单 Activity 架构,内部用 Navigation Compose 切页面。
 *
 * 当前为最小骨架:验证「Kotlin → JNI → native」整条链路通
 * (调 NativeBridge.nativeInit() 并把 native 返回的字符串显示出来)。
 * 后续主页会换成资料库/播放页(见 docs/plan.md)。
 */
class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // 沉浸式边到边(Apple Music 风需要内容延伸到大圆角屏/刘海下)
        enableEdgeToEdge()
        setContent {
            HuanchenTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    BootstrapScreen()
                }
            }
        }
    }
}

/**
 * 启动验证页(最小骨架用,验证构建链路与 JNI 链路)。
 * 后续会被正式的资料库主页替换。
 */
@Composable
fun BootstrapScreen() {
    // 调 native 引擎,验证 JNI 链路;失败会抛 UnsatisfiedLinkError 或返回异常串
    val engineStatus = runCatching { NativeBridge.nativeInit() }
        .getOrElse { "native 加载失败: ${it.message}" }
    // 验证 FFmpeg 链接(FFmpeg 接入的核心验收点)
    val ffmpegStatus = runCatching { NativeBridge.nativeGetFFmpegVersion() }
        .getOrElse { "FFmpeg 链接失败: ${it.message}" }

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
            text = "构建链路验证",
            style = MaterialTheme.typography.bodyLarge,
            modifier = Modifier.padding(top = 8.dp)
        )
        Text(
            text = "native 引擎: $engineStatus",
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 24.dp)
        )
        Text(
            text = ffmpegStatus,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 8.dp)
        )
    }
}
