// app 模块构建脚本
// 版本统一来自 gradle/libs.versions.toml(见 CLAUDE.md 第七节)
plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.ksp)
}

android {
    namespace = "com.huanchen.music"
    compileSdk = 35
    // 显式指定 NDK 版本,与 local.properties 的 ndk.dir 保持一致(避免 AGP 默认 27.x 冲突)
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.huanchen.music"
        minSdk = 26                     // 见 CLAUDE.md 第八节(路线A实测后再定是否上调)
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

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

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
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
