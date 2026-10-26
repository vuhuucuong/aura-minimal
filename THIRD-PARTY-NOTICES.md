# Third-party notices

## Aura

Source: https://github.com/Surrey-Homeware/Aura

Application based on commit ded60e3cd5c9fc41e2f049ff1da70c26820ee9e9. The author's README offers weather.ino under GPLv3. The retained source and fonts keep their existing notices. See UPSTREAM-README.md and LICENSE. Original Google weather icon files are not redistributed here.

## ESP Web Tools 10.1.1

Source: https://github.com/esphome/esp-web-tools

The prebuilt browser files are copied from the official npm package's dist/web directory. License: Apache-2.0. See website/vendor/ESP-WEB-TOOLS-LICENSE.txt. The bundle also incorporates its upstream dependencies, including Lit (BSD-3-Clause), Material Web (Apache-2.0), esptool-js (Apache-2.0), Improv Wi-Fi Serial SDK (Apache-2.0), pako (MIT/Zlib), and tslib (0BSD). Full package license files are retained in website/vendor/licenses/.

Local modifications in `website/vendor/install-dialog-CLL1RKXl.js` restore the default seven connection attempts (the bundled call incorrectly used the ROM baud rate as the attempt count), release serial reader/writer locks during cleanup, display the current preparation step and a live installation log, and report installation errors. Serial reopening waits until flashing and cleanup finish. These patches do not change the firmware images.

## Firmware dependencies

ESP32 Arduino core 3.2.0 and its ESP-IDF components, LVGL 9.2.2 (MIT), TFT_eSPI 2.5.43 (FreeBSD), WiFiManager 2.0.17 (MIT), XPT2046_Touchscreen 1.4 (MIT), and ArduinoJson 7.4.1 (MIT). See their upstream distributions for component notices. The build script retrieves the original packages from Arduino's registry and Espressif's board package index.

Montserrat is provided under the SIL Open Font License 1.1. Font Awesome symbols incorporated by LVGL's built-in font data retain Font Awesome's upstream font license. No license is replaced by this notice.
