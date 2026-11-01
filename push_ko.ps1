param(
    [string]$KoPath = ".\kernel\ff_kdriver.ko"
)

$ADB = "adb"
if (-not (Get-Command "adb" -ErrorAction SilentlyContinue)) {
    $fallback = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
    if (Test-Path $fallback) { $ADB = $fallback }
}

Write-Host "[*] Checking device connection..." -ForegroundColor Yellow
$deviceState = (& $ADB get-state 2>&1)
if ($LASTEXITCODE -ne 0 -or $deviceState -notmatch "device") {
    Write-Error "No device connected via ADB or USB debugging is disabled!"
    exit 1
}

$deviceCodename = (& $ADB shell getprop ro.product.device).Trim()
$deviceModel = (& $ADB shell getprop ro.product.model).Trim()
Write-Host "[+] Connected: $deviceModel ($deviceCodename)" -ForegroundColor Green

if (-not (Test-Path $KoPath)) {
    Write-Error "ff_kdriver.ko not found at: $KoPath"
    exit 1
}

Write-Host "[*] Pushing ff_kdriver.ko to /data/local/tmp/..." -ForegroundColor Yellow
& $ADB push $KoPath /data/local/tmp/ff_kdriver.ko
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to push driver via ADB"
    exit 1
}
Write-Host "[+] File pushed successfully." -ForegroundColor Green

Write-Host "[*] Loading kernel module into device..." -ForegroundColor Yellow

$remoteScript = @'
rmmod ff_kdriver 2>/dev/null
echo 0 > /proc/sys/kernel/module_sig_enforce 2>/dev/null
insmod /data/local/tmp/ff_kdriver.ko
if [ $? -eq 0 ]; then
    chmod 666 /dev/ff_cheat
    echo "SUCCESS"
    echo "Device node: $(ls -la /dev/ff_cheat 2>/dev/null)"
else
    echo "FAILED"
    dmesg | tail -10
fi
'@

$result = (& $ADB shell su -c "$remoteScript")
Write-Host $result

if ($result -match "SUCCESS") {
    Write-Host ""
    Write-Host "==================================" -ForegroundColor Green
    Write-Host "  [+] Driver loaded on $deviceModel" -ForegroundColor Green
    Write-Host "  Node /dev/ff_cheat is ready!" -ForegroundColor Green
    Write-Host "==================================" -ForegroundColor Green
} else {
    Write-Host "[!] Module load failed. Review dmesg output above." -ForegroundColor Red
}
