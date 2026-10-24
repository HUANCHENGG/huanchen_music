package com.huanchen.music.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable

/**
 * 主题入口(最小骨架版)。
 *
 * 说明:这里只是让 APP 能跑起来的占位主题。
 * 正式主题引擎会按 CLAUDE.md 第六节演进为「三态」:
 *   - 预设主题(几套精调 ColorScheme)
 *   - 跟随封面(Palette 取色 → material-color-utilities 生成 ColorScheme)
 * 并用 AnimatedColorScheme 做逐角色色渐变(不重建 MaterialTheme)。
 * 当前先用静态明暗两套,占位即可。
 */

// 占位暗色/亮色方案(后续由主题引擎接管,精调成 Apple Music 质感)
private val DarkColors = darkColorScheme()
private val LightColors = lightColorScheme()

@Composable
fun HuanchenTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = if (darkTheme) DarkColors else LightColors,
        content = content
    )
}
