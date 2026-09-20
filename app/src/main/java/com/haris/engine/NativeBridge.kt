package com.haris.engine

object NativeBridge {
    init { System.loadLibrary("haris_game") }

    @JvmStatic external fun create()
    @JvmStatic external fun loadSource(source: String): Boolean
    @JvmStatic external fun frame(dt: Float): Int
    @JvmStatic external fun setScreenSize(width: Int, height: Int)
    @JvmStatic external fun setSaveDir(dir: String)
    @JvmStatic external fun logicalWidth(): Int
    @JvmStatic external fun logicalHeight(): Int
    @JvmStatic external fun getCommands(data: FloatArray, meta: IntArray, blob: ByteArray): Int
    @JvmStatic external fun touch(action: Int, x: Float, y: Float, pointerId: Int)
    @JvmStatic external fun key(keyCode: Int, down: Boolean)
    @JvmStatic external fun error(): String
}
