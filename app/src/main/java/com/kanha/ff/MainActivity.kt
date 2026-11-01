// ============================================================
// FILE: app/src/main/java/com/kanha/ff/MainActivity.kt
// Android App UI — Kotlin
// Loads kernel module, shows overlay service, menu controls
// ============================================================

package com.kanha.ff

import android.app.*
import android.content.*
import android.graphics.*
import android.os.*
import android.provider.Settings
import android.view.*
import android.view.WindowManager.LayoutParams
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import java.io.File

class MainActivity : AppCompatActivity() {

    companion object {
        init { System.loadLibrary("ff_cheat") }
        external fun nativeInit(): Boolean
        external fun setFeature(id: Int, enabled: Boolean)
        external fun setSlider(id: Int, value: Float)
        external fun getFeature(id: Int): Boolean
    }

    // Feature IDs — match MenuState fields
    object Feat {
        const val AIMBOT      = 0
        const val SILENT_AIM  = 1
        const val ESP         = 2
        const val ESP_BOX     = 3
        const val ESP_SKEL    = 4
        const val ESP_HP      = 5
        const val NO_RECOIL   = 6
        const val FAST_RELOAD = 7
        const val GHOST_MODE  = 8
        const val SPEED_HACK  = 9
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Load kernel module first (requires root)
        loadKernelDriver()

        // Check overlay permission
        if (!Settings.canDrawOverlays(this)) {
            startActivity(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION))
            return
        }

        setContentView(R.layout.activity_main)
        setupUI()

        // Start overlay service
        startService(Intent(this, OverlayService::class.java))
    }

    private fun loadKernelDriver() {
        try {
            if (File("/dev/ff_cheat").exists()) return

            val koPath = "${filesDir.absolutePath}/ff_kdriver.ko"
            val hasAsset = try {
                assets.open("ff_kdriver.ko").use { inp ->
                    File(koPath).outputStream().use { out -> inp.copyTo(out) }
                }
                true
            } catch (e: Exception) {
                false
            }

            val targetKo = if (hasAsset) koPath else "/data/local/tmp/ff_kdriver.ko"
            Runtime.getRuntime().exec(arrayOf("su", "-c",
                "insmod $targetKo 2>/dev/null; chmod 666 /dev/ff_cheat 2>/dev/null")).waitFor()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun setupUI() {
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(32, 32, 32, 32)
            setBackgroundColor(Color.parseColor("#CC0D0D0D"))
        }

        val title = TextView(this).apply {
            text = "KANHA // FREE FIRE"
            textSize = 20f
            setTextColor(Color.parseColor("#00FF88"))
            typeface = Typeface.DEFAULT_BOLD
        }
        layout.addView(title)

        // Tabs
        val tabBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
        }
        val tabs = listOf("AIMBOT", "ESP", "MISC")
        val panels = mutableListOf<LinearLayout>()

        val tabGroup = RadioGroup(this).apply {
            orientation = LinearLayout.HORIZONTAL
        }

        val aimPanel  = buildAimPanel()
        val espPanel  = buildESPPanel()
        val miscPanel = buildMiscPanel()
        panels.addAll(listOf(aimPanel, espPanel, miscPanel))

        tabs.forEachIndexed { i, name ->
            val btn = Button(this).apply {
                text = name
                textSize = 12f
                setTextColor(Color.WHITE)
                setBackgroundColor(if (i==0) Color.parseColor("#0055FF")
                                   else Color.parseColor("#222222"))
                setOnClickListener {
                    panels.forEach { it.visibility = View.GONE }
                    panels[i].visibility = View.VISIBLE
                    for (j in 0 until tabBar.childCount)
                        (tabBar.getChildAt(j) as Button)
                            .setBackgroundColor(Color.parseColor("#222222"))
                    setBackgroundColor(Color.parseColor("#0055FF"))
                }
            }
            tabBar.addView(btn, LinearLayout.LayoutParams(0,
                LayoutParams.WRAP_CONTENT, 1f))
        }

        layout.addView(tabBar)
        panels.forEachIndexed { i, p ->
            p.visibility = if (i==0) View.VISIBLE else View.GONE
            layout.addView(p)
        }

        setContentView(ScrollView(this).apply { addView(layout) })
    }

    private fun makeToggle(label: String, featId: Int): LinearLayout {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0,8,0,8)
        }
        val tv = TextView(this).apply {
            text = label
            setTextColor(Color.WHITE)
            textSize = 14f
            layoutParams = LinearLayout.LayoutParams(0,
                LayoutParams.WRAP_CONTENT, 1f)
        }
        val sw = Switch(this).apply {
            isChecked = getFeature(featId)
            setOnCheckedChangeListener { _, on -> setFeature(featId, on) }
        }
        row.addView(tv); row.addView(sw)
        return row
    }

    private fun makeSlider(label: String, sliderId: Int,
                           min: Float, max: Float, def: Float): LinearLayout {
        val col = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0,4,0,4)
        }
        val tv = TextView(this).apply {
            text = "$label: $def"
            setTextColor(Color.LTGRAY)
            textSize = 12f
        }
        val sb = SeekBar(this).apply {
            progressDrawable.setTint(Color.parseColor("#00FF88"))
            max = 100
            progress = ((def - min) / (max - min) * 100).toInt()
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar, p: Int, u: Boolean) {
                    val v = min + (max - min) * p / 100f
                    tv.text = "$label: %.1f".format(v)
                    setSlider(sliderId, v)
                }
                override fun onStartTrackingTouch(s: SeekBar) {}
                override fun onStopTrackingTouch(s: SeekBar) {}
            })
        }
        col.addView(tv); col.addView(sb)
        return col
    }

    private fun buildAimPanel() = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        addView(makeToggle("Aimbot",     Feat.AIMBOT))
        addView(makeToggle("Silent Aim", Feat.SILENT_AIM))
        addView(makeSlider("FOV",    0, 20f, 200f, 80f))
        addView(makeSlider("Smooth", 1, 1f,  20f,  5f))

        val boneLabel = TextView(this@MainActivity).apply {
            text = "Aim Bone"; setTextColor(Color.LTGRAY); textSize=12f
        }
        val boneSpin = Spinner(this@MainActivity).apply {
            adapter = ArrayAdapter(this@MainActivity,
                android.R.layout.simple_spinner_item,
                listOf("Head","Chest","Hip")).also {
                it.setDropDownViewResource(
                    android.R.layout.simple_spinner_dropdown_item) }
            onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(a: AdapterView<*>,
                    v: View?, pos: Int, id: Long) = setSlider(2, pos.toFloat())
                override fun onNothingSelected(a: AdapterView<*>) {}
            }
        }
        addView(boneLabel); addView(boneSpin)
    }

    private fun buildESPPanel() = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        addView(makeToggle("ESP",         Feat.ESP))
        addView(makeToggle("Box",         Feat.ESP_BOX))
        addView(makeToggle("Skeleton",    Feat.ESP_SKEL))
        addView(makeToggle("HP Bar",      Feat.ESP_HP))
    }

    private fun buildMiscPanel() = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        addView(makeToggle("No Recoil",   Feat.NO_RECOIL))
        addView(makeToggle("Fast Reload", Feat.FAST_RELOAD))
        addView(makeToggle("Ghost Mode",  Feat.GHOST_MODE))
        addView(makeToggle("Speed Hack",  Feat.SPEED_HACK))
        addView(makeSlider("Speed Mult",  3, 1f, 5f, 1.5f))
    }
}