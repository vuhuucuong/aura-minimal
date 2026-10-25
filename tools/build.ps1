param(
  [string]$ArduinoCli = 'arduino-cli',
  [ValidateSet('standard', 'inverted', 'both')][string]$Variant = 'both',
  [switch]$InstallDependencies
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$localBuild = Join-Path $projectRoot '.build'
$configFile = Join-Path $localBuild 'arduino-cli.yaml'
New-Item -ItemType Directory -Force -Path $localBuild | Out-Null
$dataPath = Join-Path $localBuild 'data'
$downloadPath = Join-Path $localBuild 'downloads'
$sketchbookPath = Join-Path $localBuild 'sketchbook'
@"
board_manager:
  additional_urls:
    - https://espressif.github.io/arduino-esp32/package_esp32_index.json
directories:
  data: '$dataPath'
  downloads: '$downloadPath'
  user: '$sketchbookPath'
network:
  connection_timeout: 300s
"@ | Set-Content -LiteralPath $configFile -Encoding utf8
if ($InstallDependencies) {
  & $ArduinoCli --config-file $configFile core update-index
  if ($LASTEXITCODE -ne 0) { throw 'Could not update Arduino package indexes.' }
  & $ArduinoCli --config-file $configFile core install esp32:esp32@3.2.0
  if ($LASTEXITCODE -ne 0) { throw 'Could not install the ESP32 core.' }
  & $ArduinoCli --config-file $configFile lib install 'ArduinoJson@7.4.1' 'TFT_eSPI@2.5.43' 'WiFiManager@2.0.17' 'XPT2046_Touchscreen@1.4.0' 'lvgl@9.2.2'
  if ($LASTEXITCODE -ne 0) { throw 'Could not install the firmware libraries.' }
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'config/User_Setup.h') -Destination (Join-Path $sketchbookPath 'libraries/TFT_eSPI/User_Setup.h')
Copy-Item -LiteralPath (Join-Path $projectRoot 'config/lv_conf.h') -Destination (Join-Path $sketchbookPath 'libraries/lvgl/src/lv_conf.h')
$variants = if ($Variant -eq 'both') { @('standard','inverted') } else { @($Variant) }
foreach ($selectedVariant in $variants) {
  $flag = if ($selectedVariant -eq 'inverted') { '-DAURA_INVERT_COLORS=1' } else { '-DAURA_INVERT_COLORS=0' }
  $export = Join-Path $localBuild "export-$selectedVariant"
  & $ArduinoCli --config-file $configFile compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --jobs 4 --build-property "compiler.cpp.extra_flags=$flag" --build-path (Join-Path $localBuild "compile-$selectedVariant") --output-dir $export (Join-Path $projectRoot 'firmware/aura_minimal')
  if ($LASTEXITCODE -ne 0) { throw "The $selectedVariant firmware did not compile." }
  $esptool = Join-Path $dataPath 'packages/esp32/tools/esptool_py/4.9.dev3/esptool.exe'
  $filename = if ($selectedVariant -eq 'inverted') { 'aura-minimal-inverted.bin' } else { 'aura-minimal.bin' }
  & $esptool --chip esp32 merge_bin -o (Join-Path $projectRoot "website/bin/$filename") --flash_mode dio --flash_freq 40m --flash_size 4MB 0x1000 (Join-Path $export 'aura_minimal.ino.bootloader.bin') 0x8000 (Join-Path $export 'aura_minimal.ino.partitions.bin') 0xe000 (Join-Path $dataPath 'packages/esp32/hardware/esp32/3.2.0/tools/partitions/boot_app0.bin') 0x10000 (Join-Path $export 'aura_minimal.ino.bin')
  if ($LASTEXITCODE -ne 0) { throw "Could not merge the $selectedVariant factory image." }
}
Write-Host 'Firmware files are ready in website/bin/. Start the local installer with start-installer.cmd.'
