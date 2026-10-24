// app 模块构建脚本
// 版本统一来自 gradle/libs.versions.toml(见 CLAUDE.md 第七节)
import java.util.Properties

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.ksp)
}

// —— Release 签名配置:从 keystore.properties 读取(该文件不入库,见 .gitignore) ——
// 若文件不存在(如 CI 或换机器),release 构建会跳过签名,仅 debug 可正常构建
val keystorePropertiesFile = rootProject.file("keystore.properties")
val keystoreProperties = Properties().apply {
    if (keystorePropertiesFile.exists()) {
        keystorePropertiesFile.inputStream().use { load(it) }
    }
}
val hasReleaseSigning = keystorePropertiesFile.exists()

android {
    namespace = "com.huanchen.music"
    compileSdk = 35
    // 显式指定 NDK 版本,与 local.properties 的 ndk.dir 保持一致(避免 AGP 默认 27.x 冲突)
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.huanchen.music"
        minSdk = 26                     // 见 CLAUDE.md 第八节(路线A实测后再定是否上调)
        targetSdk = 35
        versionCode = 2
        versionName = "0.2.0-alpha"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // NDK:一期只打 arm64-v8a(见 CLAUDE.md 第八节)
        ndk {
            abiFilters += listOf("arm64-v8a")
        }

        // 指定 CMake 版本(用我们刚装的 3.22.1)
        externalNativeBuild {
            cmake {
                version = "3.22.1"
            }
        }
    }

    // NDK 引擎层:CMake 构建,脚本在 src/main/cpp/CMakeLists.txt
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    // 打包 FFmpeg 预编译 .so 进 APK
    // 结构必须为 <srcDir>/<abi>/*.so:此处指向 prebuilt/jniLibs,其下 arm64-v8a/ 即 ABI 目录
    // (arm64-v8a/*.so 是指向 ffmpeg 实际产物的符号链接,见 third_party/ffmpeg/build_android.sh)
    sourceSets {
        getByName("main") {
            jniLibs.srcDir("../third_party/ffmpeg/prebuilt/jniLibs")
        }
    }

    // —— 签名配置:release 用专属 keystore(密码来自 keystore.properties,不入库) ——
    signingConfigs {
        if (hasReleaseSigning) {
            create("release") {
                storeFile = rootProject.file(keystoreProperties["storeFile"] as String)
                storePassword = keystoreProperties["storePassword"] as String
                keyAlias = keystoreProperties["keyAlias"] as String
                keyPassword = keystoreProperties["keyPassword"] as String
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            // 有 keystore 才用 release 签名,否则回退 debug 签名(保证无 keystore 也能编 release)
            signingConfig = if (hasReleaseSigning) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
        debug {
            // 调试包默认不混淆,便于排查
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    // Compose 编译器扩展(与 Kotlin 版本匹配,由 kotlin-compose 插件管理)
    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

dependencies {
    // —— AndroidX 基础 ——
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)

    // —— Compose(BOM 统一版本) ——
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.ui.graphics)
    implementation(libs.compose.ui.tooling.preview)
    implementation(libs.compose.material3)
    implementation(libs.compose.material.icons.extended)
    implementation(libs.compose.foundation)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.navigation.compose)
    debugImplementation(libs.compose.ui.tooling)

    // —— 数据 ——
    implementation(libs.androidx.room.runtime)
    implementation(libs.androidx.room.ktx)
    ksp(libs.androidx.room.compiler)
    implementation(libs.androidx.datastore.preferences)

    // —— UI 辅助 ——
    implementation(libs.coil.compose)                 // 封面加载
    implementation(libs.androidx.palette.ktx)         // 封面取色
    implementation(libs.material.color.utilities)     // 封面种子色 → M3 ColorScheme

    // —— 协程 ——
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.kotlinx.coroutines.core)

    // —— 测试 ——
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.compose.bom))
}
