#!/usr/bin/env python3
"""Build factory images with the pinned Arduino toolchain on Linux, macOS or Windows."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / '.build'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arduino-cli', default='arduino-cli')
    parser.add_argument('--variant', choices=('standard', 'inverted', 'both'), default='both')
    parser.add_argument('--install-dependencies', action='store_true')
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    config = BUILD / 'arduino-cli.yaml'
    config.write_text('board_manager:\n  additional_urls:\n'
                      '    - https://espressif.github.io/arduino-esp32/package_esp32_index.json\n'
                      'directories:\n'
                      + ''.join(f'  {key}: {json.dumps(str(BUILD / folder))}\n'
                                for key, folder in (('data', 'data'), ('downloads', 'downloads'), ('user', 'sketchbook')))
                      + 'network:\n  connection_timeout: 300s\n')
    cli = [args.arduino_cli, '--config-file', str(config)]
    def run(command):
        subprocess.run([str(item) for item in command], check=True, cwd=ROOT)
    if args.install_dependencies:
        run([*cli, 'core', 'update-index'])
        run([*cli, 'core', 'install', 'esp32:esp32@3.2.0'])
        run([*cli, 'lib', 'install', 'ArduinoJson@7.4.1', 'TFT_eSPI@2.5.43',
             'WiFiManager@2.0.17', 'XPT2046_Touchscreen@1.4.0', 'lvgl@9.2.2'])
    libs = BUILD / 'sketchbook' / 'libraries'
    shutil.copyfile(ROOT / 'config' / 'User_Setup.h', libs / 'TFT_eSPI' / 'User_Setup.h')
    shutil.copyfile(ROOT / 'config' / 'lv_conf.h', libs / 'lvgl' / 'src' / 'lv_conf.h')
    esptool_dir = BUILD / 'data' / 'packages' / 'esp32' / 'tools' / 'esptool_py' / '4.9.dev3'
    esptool = esptool_dir / ('esptool.exe' if (esptool_dir / 'esptool.exe').exists() else 'esptool')
    for variant in ('standard', 'inverted') if args.variant == 'both' else (args.variant,):
        export = BUILD / f'export-{variant}'
        run([*cli, 'compile', '--fqbn', 'esp32:esp32:esp32:PartitionScheme=huge_app', '--jobs', '4',
             '--build-property', f'compiler.cpp.extra_flags=-DAURA_INVERT_COLORS={int(variant == "inverted")}',
             '--build-path', BUILD / f'compile-{variant}', '--output-dir', export, ROOT / 'firmware' / 'aura_minimal'])
        filename = f'aura-minimal{"-inverted" if variant == "inverted" else ""}.bin'
        run([esptool, '--chip', 'esp32', 'merge_bin', '-o', ROOT / 'website' / 'bin' / filename,
             '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', '4MB',
             '0x1000', export / 'aura_minimal.ino.bootloader.bin',
             '0x8000', export / 'aura_minimal.ino.partitions.bin',
             '0xe000', BUILD / 'data' / 'packages' / 'esp32' / 'hardware' / 'esp32' / '3.2.0' / 'tools' / 'partitions' / 'boot_app0.bin',
             '0x10000', export / 'aura_minimal.ino.bin'])
    print('Factory images are ready in website/bin/. No device was flashed.')


if __name__ == '__main__':
    main()
