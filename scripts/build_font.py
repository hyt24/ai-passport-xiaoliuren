#!/usr/bin/env python3
"""Regenerate the existing 16px subset, retaining glyphs and adding UI strings.
Usage: python3 scripts/build_font.py /path/WenYuanSerifSC-Regular.ttf /path/lv_font_conv.js
"""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
font = root / 'main/xlr_font_16.c'
symbols = {chr(int(code, 16)) for code in re.findall(r'/\* U\+([0-9A-Fa-f]+)', font.read_text())}
for name in ('xlr_app.c', 'xlr_calendar.c', 'xlr_logic.c'):
    for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', (root / 'main' / name).read_text()):
        symbols.update(re.sub(r'\\.', '', literal))
symbols.update(chr(code) for code in range(32, 127))
subprocess.run(['node', sys.argv[2], '--font', sys.argv[1], '--size', '16', '--bpp', '2',
                '--format', 'lvgl', '--output', str(font), '--symbols', ''.join(sorted(symbols)),
                '--no-compress', '--no-kerning', '--lv-font-name', 'xlr_font_16',
                '--lv-include', 'lvgl.h'], check=True)
