package com.huanchen.music

import android.app.Application

/**
 * Application 入口 —— 全局初始化挂点。
 *
 * 当前为最小骨架。后续在这里挂:
 * - 主题引擎的全局状态(封面取色/预设主题,见 CLAUDE.md 第六节)
 * - PlayerService 绑定、媒体库扫描触发、能力探测初始化
 */
class HuanchenApp : Application() {

    override fun onCreate() {
        super.onCreate()
        // 最小骨架阶段:暂不做全局初始化,后续按需补充
    }
}
