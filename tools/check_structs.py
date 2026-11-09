#!/usr/bin/env python3
"""Checks the game's structs against their DOS layout.

The headers in src_DSAVANT/include/structs give each member's DOS offset in
a comment (/* +0x26 */, /* 0x0090: ... */, /* +0x26, +0x2a, +0x2e */ for a
declaration of several) and some give the struct's size (/* 24 bytes */,
/* 0x248 bytes */).  These are read from disk files and laid over DOS
memory, so the host must lay them out the same.  This preprocesses and
rewrites the headers as the build does game C (host16.h, c16), turns every
such comment into a _Static_assert after them, and compiles the lot.

    tools/check_structs.py [TARGET]     linux (default) or win
"""
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build  # noqa: E402
import c16  # noqa: E402

STRUCTS = os.path.join(build.ROOT, build.game('include'), 'structs')
OFF = r'(?:\+(?:0x[0-9a-fA-F]+|\d+)|0x[0-9a-fA-F]+)'


def parse(path):
    """[(kind, tag, member, offset)] and [(kind, tag, size)] for one header."""
    members, sizes = [], []
    stack = []      # (kind, tag or None) of each open struct/union
    text = open(path, encoding='latin-1').read()
    text = re.sub(r'/\*.*?\*/', lambda m: m.group(0).replace('\n', ' '), text, flags=re.S)
    for line in text.split('\n'):
        code, _, comment = line.partition('/*')
        comment = comment.split('*/')[0].strip()
        m = re.match(r'\s*(?:typedef\s+)?(struct|union)\s*(\w+)?\s*\{', code)
        if m:
            stack.append((m.group(1), m.group(2)))
            s = re.match(r'(0x[0-9a-fA-F]+|\d+) bytes', comment)
            if s and m.group(2):
                sizes.append((m.group(1), m.group(2), int(s.group(1), 0)))
            continue
        if re.match(r'\s*\}', code):
            if stack:
                stack.pop()
            if not stack:
                continue
            # "} name; /* +off */": a member of the enclosing struct
            # whose type was defined in place.
            code = re.sub(r'^\s*\}', 'x', code)
        if not stack or stack[-1][1] is None or ';' not in code:
            continue
        offs = re.match(r'(%s(?:\s*,\s*%s)*)\s*(?::|$|\.\.|\s)' % (OFF, OFF), comment)
        if not offs:
            continue
        offs = [int(o.lstrip('+'), 0) for o in re.findall(OFF, offs.group(1))]
        decl = code.split(';')[0]
        # The declarators after the type: a, *b, c[4][2].
        decl = re.sub(r'\[[^]]*\]', '', decl)
        names = [re.findall(r'\w+', d)[-1] for d in decl.split(',')]
        if len(names) != len(offs):
            names = names[:1]
            offs = offs[:1]
        kind, tag = stack[-1]
        for n, o in zip(names, offs):
            members.append((kind, tag, n, o))
    return members, sizes


def main():
    tname = sys.argv[1] if len(sys.argv) > 1 else 'linux'
    t = build.Target(tname)
    headers = sorted(f for f in os.listdir(STRUCTS) if f.endswith('.h'))
    src = ['#include "structs/%s"\n' % h for h in headers]
    checks = []
    for h in headers:
        members, sizes = parse(os.path.join(STRUCTS, h))
        for kind, tag, m, o in members:
            checks.append('_Static_assert(__builtin_offsetof(%s %s, %s) == %d, "%s: %s %s.%s at 0x%x");\n'
                          % (kind, tag, m, o, h, kind, tag, m, o))
        for kind, tag, size in sizes:
            checks.append('_Static_assert(sizeof(%s %s) == %d, "%s: %s %s is 0x%x bytes");\n'
                          % (kind, tag, size, h, kind, tag, size))
    with tempfile.TemporaryDirectory() as d:
        c = os.path.join(d, 'check.c')
        pre = os.path.join(d, 'check.i')
        open(c, 'w').write(''.join(src))
        r = subprocess.run([t.cc, '-E', '-std=gnu89', '-I', os.path.join(build.ROOT, build.game('include')),
                            '-I', os.path.join(build.ROOT, build.HOST),
                            '-include', os.path.join(build.ROOT, build.HOST, 'host16.h'), c, '-o', pre],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(r.stderr)
        text = open(pre, encoding='latin-1').read()
        toks = c16.tokenize(text, [build.HOST + '/', os.path.join(build.ROOT, build.HOST) + '/'])
        edits = c16.Edits()
        c16.rewrite_int_words(toks, edits)
        rw = c16.Rewriter(toks, edits)
        rw.run()
        # The asserts go after the rewrite: c16 would take them for
        # implicit-int declarations.
        open(pre, 'w', encoding='latin-1').write(edits.apply(text) + ''.join(checks))
        r = subprocess.run([t.cc, '-fsyntax-only', '-x', 'cpp-output', '-std=gnu11', '-w'] + t.cflags + [pre],
                           capture_output=True, text=True)
    bad = re.findall(r'static assertion failed: "([^"]*)"', r.stderr)
    for b in bad:
        print(b)
    if r.returncode and not bad:
        sys.exit(r.stderr)
    print('%s: %d of %d struct offsets and sizes differ from DOS' % (tname, len(bad), len(checks)))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
