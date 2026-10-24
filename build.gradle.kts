// 根构建脚本 —— 只做插件声明(apply false),具体配置在各模块
// 版本统一来自 gradle/libs.versions.toml(见 CLAUDE.md 第七节)
plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.kotlin.compose) apply false
    alias(libs.plugins.ksp) apply false
}
