package com.haris.engine

import android.app.Activity
import android.graphics.BitmapFactory
import android.media.AudioAttributes
import android.media.SoundPool
import android.os.Build
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import android.view.KeyEvent
import android.view.View
import android.view.Window
import android.view.WindowManager

class MainActivity : Activity() {
    private lateinit var gameView: HarisGameView
    private lateinit var soundPool: SoundPool
    private val sounds = linkedMapOf<String, Int>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestFullscreen(window)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        NativeBridge.create()
        NativeBridge.setSaveDir(filesDir.absolutePath)

        gameView = HarisGameView(this)
        loadSprites()
        setupAudio()
        gameView.setVibrateHandler(::vibrate)
        setContentView(gameView)

        val source = assets.open("game/main.hr").bufferedReader(Charsets.UTF_8).use { it.readText() }
        if (!NativeBridge.loadSource(source)) {
            val message = NativeBridge.error()
            android.util.Log.e("Haris", "Game failed to load: $message")
            gameView.setRuntimeError(message)
        }
        gameView.requestFocus()
    }

    private fun requestFullscreen(window: Window) {
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.hide(android.view.WindowInsets.Type.statusBars() or android.view.WindowInsets.Type.navigationBars())
            window.insetsController?.systemBarsBehavior = android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or View.SYSTEM_UI_FLAG_FULLSCREEN or
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                )
        }
    }

    private fun loadSprites() {
        val names = assets.list("game/sprites") ?: return
        for (name in names) {
            if (!name.endsWith(".png", true) && !name.endsWith(".jpg", true) && !name.endsWith(".webp", true)) continue
            assets.open("game/sprites/$name").use { stream ->
                val bitmap = BitmapFactory.decodeStream(stream) ?: return@use
                gameView.setSprite(name.substringBeforeLast('.'), bitmap)
            }
        }
    }

    private fun setupAudio() {
        val attrs = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build()
        soundPool = SoundPool.Builder().setMaxStreams(16).setAudioAttributes(attrs).build()
        val names = assets.list("game/audio") ?: emptyArray()
        for (name in names) {
            if (!name.endsWith(".wav", true) && !name.endsWith(".ogg", true)) continue
            try {
                assets.openFd("game/audio/$name").use { afd ->
                    sounds[name.substringBeforeLast('.')] = soundPool.load(afd, 1)
                }
            } catch (_: Exception) {
                // Compressed assets that do not expose an AssetFileDescriptor are ignored.
            }
        }
        gameView.setSoundHandler { name, volume ->
            val sound = sounds[name] ?: return@setSoundHandler
            soundPool.play(sound, volume, volume, 0, 0, 1f)
        }
    }

    private fun vibrate(ms: Long) {
        val vibrator = getSystemService(Vibrator::class.java) ?: return
        if (!vibrator.hasVibrator()) return
        if (Build.VERSION.SDK_INT >= 26) vibrator.vibrate(VibrationEffect.createOneShot(ms.coerceIn(1, 5000), VibrationEffect.DEFAULT_AMPLITUDE))
        else @Suppress("DEPRECATION") vibrator.vibrate(ms.coerceIn(1, 5000))
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) return gameView.handleKey(event.keyCode, true)
        if (event.action == KeyEvent.ACTION_UP) return gameView.handleKey(event.keyCode, false)
        return super.dispatchKeyEvent(event)
    }

    override fun onResume() { super.onResume(); requestFullscreen(window) }
    override fun onDestroy() { if (::soundPool.isInitialized) soundPool.release(); super.onDestroy() }

}
