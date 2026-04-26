package com.example.vulkanpractice

import android.os.Bundle
import android.view.WindowManager
import com.google.androidgamesdk.GameActivity

class VulkanActivity : GameActivity() {
    protected override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Keep the screen on while the app is running
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    companion object {
        // Load the native library
        init {
            System.loadLibrary("Vulkan_Learn_Android")
        }
    }
}