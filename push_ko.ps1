# ============================================================
# push_ko.ps1 — ادفع ff_kdriver.ko للهاتف وحمّله
# شغّله بعد ما تنزّل الـ artifact من GitHub Actions
# ============================================================
param(
    [string]$KoPath = ".\kernel\ff_kdriver.ko"
)

$ADB = "adb"

Write-Host "[*] تحقق من الهاتف..." -ForegroundColor Yellow
$deviceState = & $ADB get-state 2>&1
if ($LASTEXITCODE -ne 0 -or $deviceState -notmatch "device") {
    Write-Error "لا يوجد هاتف موصول عبر ADB أو التصحيح غير مفعّل!"
    exit 1
}

$deviceCodename = (& $ADB shell getprop ro.product.device).Trim()
$deviceModel = (& $ADB shell getprop ro.product.model).Trim()
Write-Host "[+] الهاتف موصول: $deviceModel ($deviceCodename)" -ForegroundColor Green

# تحقق من الملف
if (-not (Test-Path $KoPath)) {
    Write-Error "ff_kdriver.ko غير موجود في: $KoPath"
    Write-Host "نزّل الـ artifact من GitHub Actions وضعه في kernel/"
    exit 1
}

Write-Host "[*] نقل ff_kdriver.ko للهاتف..." -ForegroundColor Yellow
& $ADB push $KoPath /data/local/tmp/ff_kdriver.ko
Write-Host "[+] تم النقل" -ForegroundColor Green

Write-Host "[*] تحميل الكيرنل موديول..." -ForegroundColor Yellow
$result = & $ADB shell su -c @"
    # أزل النسخة القديمة لو موجودة
    rmmod ff_kdriver 2>/dev/null || true
    
    # الغ signature enforcement مؤقتاً (Magisk يسمح هذا)
    echo 0 > /proc/sys/kernel/module_sig_enforce 2>/dev/null || true
    
    # حمّل الموديول
    insmod /data/local/tmp/ff_kdriver.ko
    RESULT=$?
    
    if [ $RESULT -eq 0 ]; then
        chmod 666 /dev/ff_cheat
        echo "SUCCESS"
        echo "Driver: $(lsmod 2>/dev/null | grep ff || echo '(hidden - ok)')"
        echo "Device: $(ls -la /dev/ff_cheat 2>/dev/null || echo 'not found')"
    else
        echo "FAILED: $RESULT"
        dmesg | tail -5
    fi
"@

Write-Host $result

if ($result -match "SUCCESS") {
    Write-Host ""
    Write-Host "══════════════════════════════════" -ForegroundColor Green
    Write-Host "  [✓] Driver loaded on Pixel 6 Pro" -ForegroundColor Green
    Write-Host "  شغّل Free Fire الآن ثم افتح KANHA" -ForegroundColor Green
    Write-Host "══════════════════════════════════" -ForegroundColor Green
} else {
    Write-Host "[!] فشل التحميل — راجع dmesg" -ForegroundColor Red
    & $ADB shell su -c "dmesg | tail -10"
}
