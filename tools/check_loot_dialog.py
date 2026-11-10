#!/usr/bin/env python3
"""Regression check for returning from the treasure item/loot dialog.
Run after ./build.sh (or TARGET=win ./build.sh):
    python3 tools/check_loot_dialog.py [linux|win]
The UI is scripted; the dialog object is the actual rewritten game code.
"""
import os
import subprocess
import sys

import build


def main():
    target = build.Target(sys.argv[1] if len(sys.argv) > 1 else 'linux')
    exe = os.path.join(target.out, 'test_loot_dialog' + ('.exe' if target.name == 'win' else ''))
    obj = os.path.join(target.obj, 'src_DSAVANT/vtrea/item_or_loot_dialog.o')
    subprocess.run([target.cc, '-std=gnu11', '-g', '-Wall', '-fstack-protector-all',
                    'tools/test_loot_dialog.c', obj, '-o', exe] +
                   (['-no-pie'] if target.name == 'linux' else ['-static']),
                   cwd=build.ROOT, check=True)
    subprocess.run(([exe] if target.name == 'linux' else ['wine', exe]),
                   cwd=build.ROOT, check=True)


if __name__ == '__main__':
    main()
