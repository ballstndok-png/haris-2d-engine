package com.haris.engine

import android.content.Context
import android.graphics.*
import android.view.MotionEvent
import android.view.View
import kotlin.math.min

class HarisGameView(context: Context) : View(context) {
    companion object {
        private const val MAX_COMMANDS = 4096
        private const val FLOATS_PER_COMMAND = 8
        private const val META_PER_COMMAND = 4
        private const val BLOB_BYTES = 262144
        private const val CMD_CLEAR = 1
        private const val CMD_RECT = 2
        private const val CMD_CIRCLE = 3
        private const val CMD_LINE = 4
        private const val CMD_TEXT = 5
        private const val CMD_SPRITE = 6
        private const val CMD_SOUND = 7
        private const val CMD_VIBRATE = 8
        private const val CMD_SPRITE_FRAME = 9
    }

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG).apply {
        strokeCap = Paint.Cap.SQUARE
        isSubpixelText = true
    }
    private val commandData = FloatArray(MAX_COMMANDS * FLOATS_PER_COMMAND)
    private val commandMeta = IntArray(MAX_COMMANDS * META_PER_COMMAND)
    private val commandBlob = ByteArray(BLOB_BYTES)
    private val sprites = linkedMapOf<String, Bitmap>()
    private var logicalW = 1280
    private var logicalH = 720
    private var scale = 1f
    private var offsetX = 0f
    private var offsetY = 0f
    private var lastFrameNanos = 0L
    private var initialized = false
    private var onSound: ((String, Float) -> Unit)? = null
    private var onVibrate: ((Long) -> Unit)? = null
    private var runtimeError: String = ""

    init {
        isFocusable = true
        isFocusableInTouchMode = true
    }

    fun setSprite(name: String, bitmap: Bitmap) { sprites[name] = bitmap }
    fun setSoundHandler(handler: (String, Float) -> Unit) { onSound = handler }
    fun setVibrateHandler(handler: (Long) -> Unit) { onVibrate = handler }
    fun setRuntimeError(message: String) { runtimeError = message; invalidate() }

    fun refreshLogicalSize() {
        val w = NativeBridge.logicalWidth().coerceAtLeast(64)
        val h = NativeBridge.logicalHeight().coerceAtLeast(64)
        if (w != logicalW || h != logicalH) {
            logicalW = w
            logicalH = h
            invalidateViewport()
        }
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        NativeBridge.setScreenSize(w, h)
        invalidateViewport()
    }

    private fun invalidateViewport() {
        val sx = if (logicalW == 0) 1f else width.toFloat() / logicalW
        val sy = if (logicalH == 0) 1f else height.toFloat() / logicalH
        scale = min(sx, sy).coerceAtLeast(0.0001f)
        offsetX = (width - logicalW * scale) * 0.5f
        offsetY = (height - logicalH * scale) * 0.5f
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val now = System.nanoTime()
        val dt = if (lastFrameNanos == 0L) 1f / 60f else ((now - lastFrameNanos) / 1_000_000_000f).coerceIn(1f / 240f, 0.25f)
        lastFrameNanos = now

        if (!initialized) initialized = true
        if (NativeBridge.frame(dt) == 0) {
            runtimeError = NativeBridge.error()
        }
        refreshLogicalSize()
        val count = NativeBridge.getCommands(commandData, commandMeta, commandBlob).coerceAtMost(MAX_COMMANDS)

        canvas.drawColor(Color.BLACK)
        canvas.save()
        canvas.translate(offsetX, offsetY)
        canvas.scale(scale, scale)
        for (i in 0 until count) drawCommand(canvas, i)
        canvas.restore()

        if (runtimeError.isNotBlank()) {
            val overlay = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xE6000000.toInt(); style = Paint.Style.FILL }
            canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), overlay)
            val errorPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.rgb(255, 90, 90); textSize = 16f; isSubpixelText = true }
            canvas.drawText("Haris runtime error", 24f, 42f, errorPaint)
            canvas.drawText(runtimeError.take(180), 24f, 70f, errorPaint)
        }

        postInvalidateOnAnimation()
    }

    private fun blobString(offset: Int): String? {
        if (offset < 0 || offset >= commandBlob.size) return null
        var end = offset
        while (end < commandBlob.size && commandBlob[end].toInt() != 0) end++
        return commandBlob.decodeToString(offset, end)
    }

    private fun drawCommand(canvas: Canvas, index: Int) {
        val d = index * FLOATS_PER_COMMAND
        val m = index * META_PER_COMMAND
        val type = commandMeta[m]
        val color = commandMeta[m + 1]
        val flag = commandMeta[m + 2]
        val blobOffset = commandMeta[m + 3]

        when (type) {
            CMD_CLEAR -> {
                paint.color = color
                paint.style = Paint.Style.FILL
                canvas.drawRect(0f, 0f, logicalW.toFloat(), logicalH.toFloat(), paint)
            }
            CMD_RECT -> {
                paint.color = color
                paint.style = if (flag != 0) Paint.Style.FILL else Paint.Style.STROKE
                paint.strokeWidth = 1f
                canvas.drawRect(commandData[d], commandData[d + 1], commandData[d] + commandData[d + 2], commandData[d + 1] + commandData[d + 3], paint)
            }
            CMD_CIRCLE -> {
                paint.color = color
                paint.style = if (flag != 0) Paint.Style.FILL else Paint.Style.STROKE
                paint.strokeWidth = 1f
                canvas.drawCircle(commandData[d], commandData[d + 1], commandData[d + 2], paint)
            }
            CMD_LINE -> {
                paint.color = color
                paint.style = Paint.Style.STROKE
                paint.strokeWidth = commandData[d + 4].coerceAtLeast(1f)
                canvas.drawLine(commandData[d], commandData[d + 1], commandData[d + 2], commandData[d + 3], paint)
            }
            CMD_TEXT -> {
                val text = blobString(blobOffset) ?: return
                paint.color = color
                paint.style = Paint.Style.FILL
                paint.textSize = commandData[d + 2]
                canvas.drawText(text, commandData[d], commandData[d + 1], paint)
            }
            CMD_SPRITE -> {
                val name = blobString(blobOffset) ?: return
                val bitmap = sprites[name] ?: return
                paint.alpha = (commandData[d + 4].coerceIn(0f, 1f) * 255f).toInt()
                val dst = RectF(commandData[d], commandData[d + 1], commandData[d] + commandData[d + 2], commandData[d + 1] + commandData[d + 3])
                canvas.drawBitmap(bitmap, null, dst, paint)
                paint.alpha = 255
            }
            CMD_SPRITE_FRAME -> {
                val name = blobString(blobOffset) ?: return
                val bitmap = sprites[name] ?: return
                val frame = commandData[d + 4].toInt().coerceAtLeast(0)
                val cols = commandData[d + 5].toInt().coerceAtLeast(1)
                var rows = commandData[d + 6].toInt()
                val cellW = (bitmap.width / cols).coerceAtLeast(1)
                if (rows <= 0) rows = (bitmap.height / cellW).coerceAtLeast(1)
                val cellH = (bitmap.height / rows).coerceAtLeast(1)
                val col = frame % cols
                val row = frame / cols
                val src = Rect(col * cellW, row * cellH, (col + 1) * cellW, (row + 1) * cellH)
                val dst = RectF(commandData[d], commandData[d + 1], commandData[d] + commandData[d + 2], commandData[d + 1] + commandData[d + 3])
                canvas.drawBitmap(bitmap, src, dst, paint)
            }
            CMD_SOUND -> {
                val name = blobString(blobOffset) ?: return
                onSound?.invoke(name, commandData[d].coerceIn(0f, 1f))
            }
            CMD_VIBRATE -> onVibrate?.invoke(commandData[d].toLong())
        }
    }

    fun handleKey(keyCode: Int, down: Boolean): Boolean {
        NativeBridge.key(keyCode, down)
        return true
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = event.actionIndex
                val id = event.getPointerId(i).coerceIn(0, 9)
                val (x, y) = toLogical(event.getX(i), event.getY(i))
                NativeBridge.touch(0, x, y, id)
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    val id = event.getPointerId(i).coerceIn(0, 9)
                    val (x, y) = toLogical(event.getX(i), event.getY(i))
                    NativeBridge.touch(1, x, y, id)
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val i = event.actionIndex
                val id = event.getPointerId(i).coerceIn(0, 9)
                val (x, y) = toLogical(event.getX(i), event.getY(i))
                NativeBridge.touch(2, x, y, id)
            }
            MotionEvent.ACTION_CANCEL -> NativeBridge.touch(3, 0f, 0f, 0)
        }
        return true
    }

    private fun toLogical(x: Float, y: Float): Pair<Float, Float> {
        val lx = ((x - offsetX) / scale).coerceIn(0f, logicalW.toFloat())
        val ly = ((y - offsetY) / scale).coerceIn(0f, logicalH.toFloat())
        return lx to ly
    }
}
