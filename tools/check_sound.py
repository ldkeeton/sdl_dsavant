#!/usr/bin/env python3
"""Build and exercise PLAY with a deterministic PIT and real ymfm synthesis.
Usage: tools/check_sound.py [linux|win] (Windows checks run under Wine).
"""
import os
import subprocess
import sys

import build


def main():
    target = build.Target(sys.argv[1] if len(sys.argv) > 1 else 'linux')
    env = dict(os.environ, TARGET=target.name, STUBS='0')
    subprocess.run([sys.executable, os.path.join(build.HERE, 'build.py')], env=env,
                   cwd=build.ROOT, check=True)
    obj = os.path.join(target.obj, 'test_sound.o')
    exe = os.path.join(target.out, 'test_sound' + ('.exe' if target.name == 'win' else ''))
    subprocess.run([target.cc, '-std=gnu11', '-g', '-Wall', '-Ihost',
                    '-iquotesrc_DSAVANT/include', '-c', 'tools/test_sound.c', '-o', obj],
                   cwd=build.ROOT, check=True)
    sources = [f'src_DSAVANT/play/{name}.o' for name in
               ('play_music', 'play_opl', 'play_sample', 'play_tables')]
    sources += ['host/opl.o']
    sources += [f'third_party/ymfm/src/ymfm_{name}.o' for name in ('opl', 'adpcm', 'pcm')]
    command = [target.cxx, '-g', '-o', exe, obj]
    command += [os.path.join(target.obj, src) for src in sources]
    command += ['-static-libgcc', '-static-libstdc++'] if target.name == 'win' else ['-no-pie']
    subprocess.run(command, cwd=build.ROOT, check=True)
    command = ([exe, os.path.join(build.ROOT, 'apps/DSAVANT')] if target.name == 'linux'
               else ['wine', exe, 'Z:' + os.path.join(build.ROOT, 'apps/DSAVANT')])
    subprocess.run(command, cwd=build.ROOT, check=True)


if __name__ == '__main__':
    main()
