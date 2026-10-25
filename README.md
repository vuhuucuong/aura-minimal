# Aura Minimal — Ink Edition 1.0.0

A modification of [Aura by Surrey Homeware](https://github.com/Surrey-Homeware/Aura) for the **ESP32-2432S028R with an ILI9341 display and XPT2046 resistive touch**. Based on upstream commit `ded60e3cd5c9fc41e2f049ff1da70c26820ee9e9`.

## Install locally

1. Extract the complete package into a folder on your computer.
2. On Windows, double-click **start-installer.cmd**. This uses Node.js, already present on the computer where this package was created. On other systems, run `node server.cjs` from this folder.
3. Open **http://localhost:8080** in desktop Chrome or Edge on the same computer.
4. Connect the board with a USB data cable. Keep **Standard ILI9341** selected first.
5. Choose **Connect & install**, select the board's USB serial port, and follow the installer.
6. After flashing, join the board's temporary **Aura** Wi-Fi network and enter your home Wi-Fi details. Choose your city using the display's settings.

The included firmware images are factory images: installing them resets the board's existing Wi-Fi and saved preferences. The inverted variant is for a board whose black and white are reversed; it is not firmware for ST7789 or another controller. The server only listens on this computer's loopback address. Stop it with Ctrl+C or close its window.

The website, installer scripts, firmware, icons and sample previews are all included locally. A public website, domain and internet connection are not required to flash. Weather fetching and initial clock synchronization need internet afterward. The original Aura installer is linked at the bottom of the local page if you want to restore the original firmware.

## Design and use

- Pure white background, black primary text, and dark gray secondary values.
- Main temperature increased from 42px to 48px.
- Nine original ink weather drawings, rendered separately at 20px and 48px. Small icons omit fine hatching to stay legible.
- All seven daily rows and seven hourly entries remain available.
- Dedicated daily, hourly and settings buttons; the forecast list also switches views when tapped.
- Units appear in the large reading and hourly heading rather than being repeated in every row.
- Location and time now have their own header. Long location names truncate instead of overlapping the clock.
- Settings use the same light palette and larger controls. Swipe upward in settings for language, location and Wi-Fi reset.
- Existing weather provider, city search, seven languages, brightness and night mode remain.

Primary text is black on white (21:1 calculated contrast). Secondary text `#454545` on white is about 9.6:1. Actual readability also depends on panel brightness and viewing angle. Preview images use the firmware's glyph data and artwork with sample readings; they are layout previews, not photographs or a full LVGL emulator.

## Included reliability changes

- The keyboard is attached to LVGL's top layer so a language change does not leave a deleted keyboard pointer.
- Location search results are bounded to the dropdown buffer.
- Weather responses are checked for the expected entries before their dates are read.
- Network connection/read waits are bounded. Network requests still run synchronously; this release does not claim fully asynchronous networking.
- LVGL timing reads the real millisecond clock rather than counting fixed loop delays.

## Build the firmware again

The precompiled binaries are in `website/bin/`. You do not need a compiler to install them.

For rebuilding, install Arduino CLI from Arduino's official distribution, then run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1 -InstallDependencies
```

The script keeps its compiler packages and libraries inside this project's `.build/` directory. It uses ESP32 Arduino core **3.2.0**, **ESP32 Dev Module**, **Huge App (3 MB)**, and the versions listed in `UPSTREAM-README.md`. The included `config/User_Setup.h` goes into the TFT_eSPI library root; `config/lv_conf.h` goes into `lvgl/src/`.

The sketch is `firmware/aura_minimal/aura_minimal.ino`; its display layout is in `minimal_ui.h`. `AURA_INVERT_COLORS=1` builds the alternate display inversion version. The application uses Aura's original saved preference keys, though installing a merged factory image resets them.

Original icon vector sources are in `website/assets/icons/`; their LVGL image data is in `manga_icons.c`. To regenerate icon pixels, install Sharp with `npm install --ignore-scripts --no-audit --no-fund`, then run `npm run icons`. The browser installer is ESP Web Tools 10.1.1, vendored under `website/vendor/`.

## Verification and limitations

Both firmware variants must pass compilation before release. See `release.json` for the exact source and binary hashes, build sizes, and verification status. The website's manifests point to the corresponding merged factory images at address zero. Physical color order, display output, touch calibration, Wi-Fi onboarding, and live weather still require a test on the actual board. No device has been flashed as part of creating this package.

## Credits and licenses

Aura's application source is offered by its author under GPLv3. This modification and the new ink drawings are provided under GPL-3.0-only; see `LICENSE`. Attribution and upstream instructions are retained in `UPSTREAM-README.md`. The original Google weather artwork has been replaced with original drawings and is not included in this package. The retained Latin font additions and LVGL Montserrat fallback fonts retain their upstream licensing. ESP Web Tools and bundled dependencies retain their own licenses; see `THIRD-PARTY-NOTICES.md` and `website/vendor/`.
