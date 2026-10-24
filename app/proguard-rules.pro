# ProGuard 规则(release 混淆用)
# 默认规则已由 proguard-android-optimize.txt 提供,这里只加项目特定保留规则。

# —— JNI 保留:native 方法对应的 Kotlin/Java 类不能被混淆,否则 JNI 找不到符号 ——
# NativeBridge 的 external fun 与 cpp 侧 JNI 签名严格对应,必须 keep
-keep class com.huanchen.music.bridge.** { *; }

# —— Compose 通常无需额外规则,编译器已处理 ——

# —— 第三方库保留规则会随其引入在此补充(FFmpeg/libusb 等通过 CMake 编译为 native 库,不受 Java 混淆影响) ——
