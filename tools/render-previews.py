#!/usr/bin/env python3
"""Render sample screens with native LVGL 9.2.2; requires gcc, g++, ar and build dependencies."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import os
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / '.build' / 'previews'
LVGL = ROOT / '.build' / 'sketchbook' / 'libraries' / 'lvgl'
FIRMWARE = ROOT / 'firmware' / 'aura_minimal'


def run(command):
    subprocess.run([str(arg) for arg in command], check=True, cwd=ROOT,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def png_from_ppm(source, target):
    # Encode the native renderer's RGB framebuffer without an imaging dependency.
    header, width_height, maximum, rgb = source.read_bytes().split(b'\n', 3)
    assert header == b'P6' and maximum == b'255'
    width, height = map(int, width_height.split())
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
    scanlines = b''.join(b'\0' + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    target.write_bytes(b'\x89PNG\r\n\x1a\n'
                      + chunk(b'IHDR', struct.pack('!IIBBBBB', width, height, 8, 2, 0, 0, 0))
                      + chunk(b'IDAT', zlib.compress(scanlines, 9)) + chunk(b'IEND', b''))


def main():
    if not (LVGL / 'lvgl.h').exists():
        raise SystemExit('Install firmware build dependencies first (LVGL 9.2.2 required).')
    BUILD.mkdir(parents=True, exist_ok=True)
    config = (ROOT / 'config' / 'lv_conf.h').read_text()
    config = config.replace('#define LV_USE_TFT_ESPI         1', '#define LV_USE_TFT_ESPI         0')
    config = config.replace('#define LV_TICK_CUSTOM 1', '#define LV_TICK_CUSTOM 0')
    config_path = BUILD / 'lv_conf.h'
    if not config_path.exists() or config_path.read_text() != config:
        config_path.write_text(config)
    flags = ['-O1', '-ffunction-sections', '-fdata-sections', '-I', LVGL, '-I', FIRMWARE,
             f'-DLV_CONF_PATH={config_path}']
    sources = list((LVGL / 'src').rglob('*.c')) + list(FIRMWARE.glob('*.c'))
    def compile_source(source):
        name = str(source.relative_to(ROOT)).replace('/', '_') + '.o'
        obj = BUILD / name
        if not obj.exists() or obj.stat().st_mtime < max(source.stat().st_mtime, config_path.stat().st_mtime):
            run(['gcc', '-std=c99', *flags, '-c', source, '-o', obj])
        return obj
    with ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
        objects = list(pool.map(compile_source, sources))
    library = BUILD / 'lvgl.a'
    run(['ar', 'rcs', library, *objects])
    binary = BUILD / 'render'
    run(['g++', '-std=c++17', *flags, ROOT / 'tools' / 'render-previews.cpp', library,
         '-Wl,--gc-sections', '-lm', '-o', binary])
    previews = ROOT / 'website' / 'assets' / 'previews'
    for view in ('daily', 'hourly'):
        for condition in ('partly', 'sun', 'rain', 'storm'):
            for state in ('rain', 'clear', 'snow', 'stale'):
                name = f'{view}-{condition}' + (f'-{state}' if state != 'rain' else '')
                ppm = BUILD / f'{name}.ppm'
                run([binary, view, condition, state, ppm])
                png_from_ppm(ppm, previews / f'{name}.png')
    print('Rendered 32 sample screens; checked the outlook tap and translated text widths.')


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(error.stderr.decode(errors='replace'))
        raise SystemExit(error.returncode)
