plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.example.vulkanpractice"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }

    defaultConfig {
        applicationId = "com.example.vulkanpractice"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters += setOf("arm64-v8a", "x86_64")
        }
        // C++ configurations (C++20 for designated initializers)
        externalNativeBuild {
            cmake {
                cppFlags("-std=c++20")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    buildFeatures {
        prefab = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    ndkVersion = "30.0.14904198 rc1"

    sourceSets {
        getByName("main") {
            assets {
                srcDirs(
                    // Point to the main project's assets
                    "../../assets",
                    // Use locally compiled shaders from the build directory
                    ".externalNativeBuild/cmake/debug/arm64-v8a/shaders",
                    ".externalNativeBuild/cmake/debug/armeabi-v7a/shaders",
                    ".externalNativeBuild/cmake/debug/x86/shaders",
                    ".externalNativeBuild/cmake/debug/x86_64/shaders",
                    ".externalNativeBuild/cmake/release/arm64-v8a/shaders",
                    ".externalNativeBuild/cmake/release/armeabi-v7a/shaders",
                    ".externalNativeBuild/cmake/release/x86/shaders",
                    ".externalNativeBuild/cmake/release/x86_64/shaders"
                )
            }
        }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.games.activity)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}