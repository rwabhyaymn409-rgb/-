// ============================================================
// FILE: app/src/main/java/com/kanha/ff/OverlayService.kt
// Transparent system overlay for ESP + menu toggle button
// Drawn on top of FF via TYPE_APPLICATION_OVERLAY
// ============================================================

package com.kanha.ff

import android.app.Service
import android.content.Intent
import android.graphics.*
import android.os.IBinder
import android.view.*

class OverlayService : Service() {

    private lateinit var wm: WindowManager
    private lateinit var overlay: OverlayView

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        wm = getSystemService(WindowManager::class.java)

        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
            WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH,
            PixelFormat.TRANSLUCENT
        )
        params.gravity = Gravity.TOP or Gravity.START

        overlay = OverlayView(this)
        wm.addView(overlay, params)
    }

    override fun onDestroy() {
        super.onDestroy()
        wm.removeView(overlay)
    }
}

class OverlayView(context: android.content.Context)
    : View(context) {

    private val boxPaint   = Paint().apply {
        color = Color.RED; style = Paint.Style.STROKE
        strokeWidth = 2f; isAntiAlias = true
    }
    private val skelPaint  = Paint().apply {
        color = Color.WHITE; style = Paint.Style.STROKE
        strokeWidth = 1.5f; isAntiAlias = true
    }
    private val hpPaint    = Paint().apply {
        color = Color.GREEN; style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val textPaint  = Paint().apply {
        color = Color.YELLOW; textSize = 28f; isAntiAlias = true
    }

    // Called every frame from cheat loop via JNI callback
    external fun getPlayerCount(): Int
    external fun getPlayerBox(idx: Int): FloatArray   // [x,y,w,h]
    external fun getPlayerHP(idx: Int): Float
    external fun getPlayerBones(idx: Int): FloatArray // flat [x0,y0,x1,y1,...]
    external fun isPlayerVisible(idx: Int): Boolean

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val count = try { getPlayerCount() } catch (e: Exception) { 0 }

        for (i in 0 until count) {
            val box = try { getPlayerBox(i) } catch (e: Exception) { continue }
            val hp  = try { getPlayerHP(i)  } catch (e: Exception) { 0f }
            val vis = try { isPlayerVisible(i) } catch (e: Exception) { false }

            boxPaint.color = if (vis) Color.RED else Color.YELLOW

            // Bounding box
            canvas.drawRect(box[0], box[1],
                box[0]+box[2], box[1]+box[3], boxPaint)

            // HP bar
            val hpH   = box[3] * (hp / 200f)
            val hpX   = box[0] - 8f
            hpPaint.color = if (hp > 100) Color.GREEN else Color.RED
            canvas.drawLine(hpX, box[1]+box[3],
                hpX, box[1]+box[3]-hpH, hpPaint)

            // HP text
            canvas.drawText("${hp.toInt()}", box[0], box[1]-4f, textPaint)

            // Skeleton
            val bones = try { getPlayerBones(i) } catch (e: Exception) { continue }
            var j = 0
            while (j + 3 < bones.size) {
                canvas.drawLine(bones[j], bones[j+1],
                    bones[j+2], bones[j+3], skelPaint)
                j += 4
            }
        }

        // Refresh at display rate
        postInvalidateOnAnimation()
    }
}