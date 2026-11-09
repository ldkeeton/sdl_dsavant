#!/usr/bin/env python3
"""Host build of src_DSAVANT (called by ../build.sh; see there for usage).

What goes into DS
  DS is every .c under ds/, vga/ and play/, the host code under host/, and
  the members of the shared library (every .c under shared/) that those
  need.  Each overlay NAME is every .c under NAME/ plus the shared members it
  needs that DS does not have; tools/ovlink.py makes it one object whose
  symbols are local, so overlays do not clash with each other.

Game C ("16-bit C")
  gcc -E, then tools/c16.py (int 16 bits, long 32), then gcc on the result.
  Before compiling, each file gets a prototype for every external function
  the game C defines (promoted K&R types), so calls are checked across files.

Stubs
  With STUBS=1 (the default for now), symbols nothing defines get a stub
  (a function that reports itself, or a zeroed data block) so DS links while
  the .s files are still being replaced.
"""

import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import c16  # noqa: E402

GAME_SRC = 'src_DSAVANT'
HOST = 'host'
DS_DIRS = ['ds', 'vga', 'play']
LIB_DIRS = ['shared']
OVERLAYS = ['vbase', 'vdopt', 'vinit', 'vmaze', 'vmele', 'vmexe', 'vmext',
            'vmnpc', 'vpclv', 'vpcmk', 'vpcvw', 'vpops', 'vtrea']
SKIP_DIRS = {'build_out_new', 'dos_compilers', '.cache'}
# Functions whose arguments are not checked against their definition: the
# generated declaration has no parameter list.
UNCHECKED = {
    'aztec_open',               # open(name, flags[, mode]): the mode is optional
    'play_intro_scene_seven',   # called with an extra argument it ignores
    'kill_party_with_message',  # called with an extra argument it ignores
    'trade_with_slot',          # fight_slot passes 2 of its 3
    'display_member_message',   # update_party_status_animation passes 2 of 3
    'show_combat_action_text',  # run_combat_round passes 3 or 4 of 5
    'apply_spell_to_npc',       # called with an extra argument it ignores
    'select_character_portrait',  # called with 2 of its 3
}

GAME_CFLAGS = [
    '-std=gnu89', '-O0', '-g', '-fno-builtin', '-fno-common',
    '-fno-toplevel-reorder', '-fwrapv', '-fno-strict-aliasing',
    # K&R code: these are how it was written, not bugs.
    '-Wno-implicit-int', '-Wno-return-type', '-Wno-builtin-declaration-mismatch',
    '-Wno-parentheses', '-Wno-return-mismatch',
    # A pointer to one struct passed where another is declared: the code
    # does it on purpose, and both are pointers.
    '-Wno-incompatible-pointer-types',
    # Integers and pointers mixed: on the host a pointer is 64 bits.
    '-Werror=int-conversion', '-Wpointer-to-int-cast',
    # Calls with no declaration: functions still in .s files, for now.
    '-Wimplicit-function-declaration',
]
HOST_CFLAGS = ['-std=gnu11', '-O2', '-g', '-Wall', '-Wno-unused-function']


def die(msg):
    print('build: ' + msg, file=sys.stderr)
    sys.exit(1)


def run(argv, **kw):
    return subprocess.run(argv, capture_output=True, text=True, errors='replace', **kw)


class Target:
    def __init__(self, name):
        self.name = name
        self.out = os.path.join(ROOT, 'build_out', name)
        self.obj = os.path.join(self.out, 'obj')
        if name == 'linux':
            self.cc, self.cxx = 'gcc', 'g++'
            self.ld, self.objcopy, self.nm = 'ld', 'objcopy', 'nm'
            self.exe = 'DS'
            # Not PIE: overlay data stays in plain .data/.bss sections.
            self.cflags = ['-fno-pie']
            env = dict(os.environ)
        elif name == 'win':
            pre = 'x86_64-w64-mingw32-'
            self.cc, self.cxx = pre + 'gcc', pre + 'g++'
            self.ld, self.objcopy, self.nm = pre + 'ld', pre + 'objcopy', pre + 'nm'
            self.exe = 'DS.exe'
            self.cflags = []
            prefix = os.environ.get('MINGW_SDL3_PREFIX', '/usr/local/x86_64-w64-mingw32')
            env = dict(os.environ, PKG_CONFIG_LIBDIR=prefix + '/lib/pkgconfig',
                       PKG_CONFIG_SYSROOT_DIR='')
            self.sdl_dll = os.path.join(prefix, 'bin', 'SDL3.dll')
        else:
            die('no target %s (linux or win)' % name)
        r = run(['pkg-config', '--cflags', 'sdl3'], env=env)
        if r.returncode:
            die('pkg-config cannot find sdl3 for %s: %s' % (name, r.stderr.strip()))
        self.sdl_cflags = shlex.split(r.stdout)
        r = run(['pkg-config', '--libs', 'sdl3'], env=env)
        self.sdl_libs = shlex.split(r.stdout)


# ---- sources -----------------------------------------------------------------

def c_files(top):
    """Every .c under top (relative to ROOT), sorted."""
    found = []
    base = os.path.join(ROOT, top)
    for d, dirs, files in os.walk(base):
        dirs[:] = sorted(x for x in dirs if x not in SKIP_DIRS)
        for f in files:
            if f.endswith('.c'):
                found.append(os.path.relpath(os.path.join(d, f), ROOT))
    return sorted(found)


def host_files():
    found = []
    for top in (HOST, 'third_party'):
        base = os.path.join(ROOT, top)
        if not os.path.isdir(base):
            continue
        for d, dirs, files in os.walk(base):
            dirs.sort()
            for f in files:
                if f.endswith(('.c', '.cpp')) and not f.startswith('test_'):
                    found.append(os.path.relpath(os.path.join(d, f), ROOT))
    return sorted(found)


def game(*parts):
    return os.path.join(GAME_SRC, *parts)


# ---- incremental helpers -----------------------------------------------------

def mtime(path):
    try:
        return os.stat(path).st_mtime_ns
    except FileNotFoundError:
        return -1


def deps_newer(dfile, target_time):
    """Whether any dependency listed in a make-style .d file is newer."""
    try:
        text = open(dfile).read()
    except FileNotFoundError:
        return True
    text = text.replace('\\\n', ' ')
    _, _, deps = text.partition(':')
    for dep in deps.split():
        if mtime(dep) > target_time or mtime(dep) < 0:
            return True
    return False


def write_if_changed(path, text):
    try:
        if open(path, encoding='latin-1').read() == text:
            return False
    except FileNotFoundError:
        pass
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, 'w', encoding='latin-1').write(text)
    return True


# ---- game C ------------------------------------------------------------------

class GameFile:
    def __init__(self, t, src):
        self.src = src
        base = os.path.join(t.obj, os.path.splitext(src)[0])
        self.pre = base + '.cpp.i'      # gcc -E
        self.rw = base + '.i'           # after c16
        self.defs = base + '.defs'      # json: defs, statics
        self.full = base + '.full.i'    # with prototypes
        self.dep = base + '.d'
        self.o = base + '.o'
        self.log = base + '.log'


def preprocess(t, g, force):
    """gcc -E and the c16 rewrite.  Returns an error text or None."""
    if not force and mtime(g.defs) > max(mtime(c16.__file__), mtime(__file__)) and \
            not deps_newer(g.dep, mtime(g.rw)):
        return None
    os.makedirs(os.path.dirname(g.pre), exist_ok=True)
    argv = [t.cc, '-E', '-std=gnu89', '-MD', '-MF', g.dep, '-MT', g.rw,
            '-I', game('include'), '-I', HOST,
            '-include', os.path.join(HOST, 'host16.h'),
            g.src, '-o', g.pre]
    r = run(argv, cwd=ROOT)
    if r.returncode:
        return r.stderr
    src = open(os.path.join(ROOT, g.pre), encoding='latin-1').read()
    toks = c16.tokenize(src, [HOST + '/', os.path.join(ROOT, HOST) + '/'])
    edits = c16.Edits()
    c16.rewrite_int_words(toks, edits)
    rw = c16.Rewriter(toks, edits)
    rw.run()
    open(os.path.join(ROOT, g.rw), 'w', encoding='latin-1').write(edits.apply(src))
    json.dump({'defs': rw.defs, 'statics': sorted(rw.statics), 'decls': rw.decls},
              open(os.path.join(ROOT, g.defs), 'w'))
    return None


def load_defs(g):
    return json.load(open(os.path.join(ROOT, g.defs)))


def proto_text(protos, statics, decls, notes):
    """The prototype header for one file.  A function the file declares
    itself keeps the file's return type (`void f()` for an implicit-int
    definition is harmless); the parameters are still checked."""
    tags = set()
    lines = []
    for name in sorted(protos):
        if name in statics:
            continue
        ret, params = protos[name]
        if name in UNCHECKED:
            params = '()'
        if name in decls and decls[name] != ret:
            if ('*' in decls[name]) != ('*' in ret):
                notes.append((name, ret, decls[name]))
            ret = decls[name]
        decl = '%s %s%s' % (ret, name, params)
        for m in re.finditer(r'\b(struct|union)\s+([A-Za-z_]\w*)', decl):
            tags.add((m.group(1), m.group(2)))
        lines.append('extern %s;\n' % decl)
    head = ''.join('%s %s;\n' % tg for tg in sorted(tags))
    return '/* prototypes of the game C */\n' + head + ''.join(lines)


def compile_game(t, g, protos, force, notes):
    """Prepend the prototypes and compile.  Returns (error text or None).
    Adds (function, defined type, declared type, file) to notes for each
    pointer/integer mismatch in a return type."""
    d = load_defs(g)
    mine = []
    text = proto_text(protos, set(d['statics']), d.get('decls', {}), mine)
    notes.extend(n + (g.src,) for n in mine)
    full = os.path.join(ROOT, g.full)
    body = open(os.path.join(ROOT, g.rw), encoding='latin-1').read()
    changed = write_if_changed(full, text + body)
    if not force and not changed and mtime(g.o) > mtime(full):
        return None
    argv = [t.cc, '-c', '-x', 'cpp-output'] + GAME_CFLAGS + t.cflags + [full, '-o', g.o]
    r = run(argv, cwd=ROOT)
    out = r.stderr
    open(os.path.join(ROOT, g.log), 'w').write(out)
    if r.returncode:
        try:
            os.remove(os.path.join(ROOT, g.o))
        except FileNotFoundError:
            pass
        return out or 'failed'
    return None


def compile_host(t, src, force):
    base = os.path.join(t.obj, os.path.splitext(src)[0])
    o, dep = base + '.o', base + '.d'
    if not force and mtime(o) > 0 and not deps_newer(dep, mtime(o)):
        return o, None
    os.makedirs(os.path.dirname(o), exist_ok=True)
    if src.endswith('.cpp'):
        argv = [t.cxx, '-std=c++17', '-O2', '-g', '-Wall'] + t.cflags
    else:
        argv = [t.cc] + HOST_CFLAGS + t.cflags
    argv += ['-MD', '-MF', dep, '-I', HOST, '-I', 'third_party/ymfm/src'] + \
        t.sdl_cflags + ['-c', src, '-o', o]
    r = run(argv, cwd=ROOT)
    if r.returncode:
        return o, r.stderr
    if r.stderr.strip():
        print(r.stderr, end='')
    return o, None


# ---- symbols and members -----------------------------------------------------

def symbols(t, obj):
    """(defined globals, undefined) of an object."""
    r = run([t.nm, '-P', obj], cwd=ROOT)
    defined, undef = set(), set()
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        name, kind = parts[0], parts[1]
        if t.name == 'win' and name.startswith('.'):
            continue
        if kind == 'U':
            undef.add(name)
        elif kind.isupper() and kind not in 'UN':
            defined.add(name)
    return defined, undef


def closure(t, objs, library, syms):
    """Library members (in library order) needed by objs."""
    defined, undef = set(), set()
    for o in objs:
        d, u = syms[o]
        defined |= d
        undef |= u
    provides = {}
    for m in library:
        for s in syms[m][0]:
            provides.setdefault(s, m)
    chosen = []
    need = undef - defined
    while True:
        add = sorted({provides[s] for s in need if s in provides} - set(chosen),
                     key=library.index)
        if not add:
            break
        for m in add:
            chosen.append(m)
            d, u = syms[m]
            defined |= d
            undef |= u
        need = undef - defined
    return [m for m in library if m in chosen]


# ---- main --------------------------------------------------------------------

def main():
    target = Target(os.environ.get('TARGET', 'linux'))
    force = os.environ.get('FORCE') == '1'
    jobs = int(os.environ.get('JOBS') or os.cpu_count() or 4)
    stubs = os.environ.get('STUBS', '1') == '1'
    only_compile = os.environ.get('COMPILE_ONLY') == '1'

    groups = {}         # group -> list of sources
    groups['ds'] = [s for d in DS_DIRS for s in c_files(game(d))]
    groups['shared'] = [s for d in LIB_DIRS for s in c_files(game(d))]
    for ov in OVERLAYS:
        groups[ov] = c_files(game(ov))
    files = {s: GameFile(target, s) for g in groups.values() for s in g}

    errors = []
    with ThreadPoolExecutor(jobs) as pool:
        for s, err in zip(files, pool.map(lambda s: preprocess(target, files[s], force), files)):
            if err:
                errors.append((s, err))
    if errors:
        for s, err in errors:
            print('%s: preprocessing failed\n%s' % (s, err))
        die('%d file(s) failed to preprocess' % len(errors))

    # Prototypes: what DS (ds, drivers, shared) defines, plus an overlay's own.
    defs = {s: load_defs(files[s]) for s in files}

    def collect(sources):
        found = {}
        clash = set()
        for s in sources:
            for name, ret, params in defs[s]['defs']:
                decl = (ret, params)
                if name in found and found[name] != decl:
                    clash.add(name)
                found.setdefault(name, decl)
        return found, clash

    base, base_clash = collect(groups['ds'] + groups['shared'])
    for name in sorted(base_clash):
        print('build: %s is defined with different types; no prototype' % name)
        base.pop(name, None)
    protos = {}
    for g, sources in groups.items():
        p = dict(base)
        if g in OVERLAYS:
            own, own_clash = collect(sources)
            p.update(own)
            for name in own_clash:
                p.pop(name, None)
        protos[g] = p

    work = [(s, g) for g, sources in groups.items() for s in sources]
    errors = []
    with ThreadPoolExecutor(jobs) as pool:
        notes = []
        results = pool.map(lambda sg: compile_game(target, files[sg[0]], protos[sg[1]], force,
                                                   notes), work)
        for (s, g), err in zip(work, results):
            if err:
                errors.append((s, err))
    by_fn = {}
    for name, ret, decl, src in notes:
        by_fn.setdefault((name, ret, decl), []).append(src)
    for (name, ret, decl), srcs in sorted(by_fn.items()):
        print('note: %s returns %s; declared %s in %s' % (
            name, ret, decl, srcs[0] if len(srcs) == 1 else '%d files' % len(srcs)))
    hosts = host_files()
    host_objs = []
    with ThreadPoolExecutor(jobs) as pool:
        for src, (o, err) in zip(hosts, pool.map(lambda s: compile_host(target, s, force), hosts)):
            host_objs.append(o)
            if err:
                errors.append((src, err))
    if errors:
        for s, err in errors:
            print('== %s\n%s' % (s, err.rstrip()))
        die('%d file(s) failed to compile' % len(errors))
    print('compiled %d game C files, %d host files' % (len(files), len(hosts)))
    if only_compile:
        return

    link(target, groups, files, host_objs, stubs, jobs)


def link(t, groups, files, host_objs, stubs, jobs):
    obj = {s: files[s].o for s in files}
    all_objs = [obj[s] for s in files] + host_objs
    with ThreadPoolExecutor(jobs) as pool:
        syms = dict(zip(all_objs, pool.map(lambda o: symbols(t, o), all_objs)))

    ds_objs = [obj[s] for s in groups['ds']] + host_objs
    library = [obj[s] for s in groups['shared']]
    ds_members = closure(t, ds_objs, library, syms)
    ds_set = ds_objs + ds_members
    ds_defined = set().union(*(syms[o][0] for o in ds_set))

    import ovlink
    ov_objs = []
    for ov in OVERLAYS:
        own = [obj[s] for s in groups[ov]]
        rest = [m for m in library if m not in ds_members]
        # Members the overlay needs that DS does not define.
        defined = set().union(*(syms[o][0] for o in own))
        undef = set().union(*(syms[o][1] for o in own)) - defined - ds_defined
        provides = {}
        for m in rest:
            for s in syms[m][0]:
                provides.setdefault(s, m)
        chosen = []
        need = set(undef)
        while True:
            add = sorted({provides[s] for s in need if s in provides} - set(chosen), key=rest.index)
            if not add:
                break
            for m in add:
                chosen.append(m)
                defined |= syms[m][0]
                need |= syms[m][1]
            need = need - defined - ds_defined
        members = [m for m in rest if m in chosen]
        out = os.path.join(t.obj, 'overlays', ov.upper() + '.o')
        ovlink.make(t, ov.upper(), own + members, out)
        ov_objs.append(out)
    table = os.path.join(t.obj, 'overlays', 'ovtable.c')
    write_if_changed(table, ovlink.table_source(t, [o.upper() for o in OVERLAYS]))
    table_o, err = compile_host(t, os.path.relpath(table, ROOT), False)
    if err:
        die('ovtable.c: ' + err)

    exe = os.path.join(t.out, t.exe)
    objs = ds_set + ov_objs + [table_o]
    stub_c = os.path.join(t.obj, 'stubs.c')
    stub_o = os.path.join(t.obj, 'stubs.o')
    stub_names = {}
    if stubs and os.path.exists(stub_c):
        os.remove(stub_c)
    for attempt in range(3):
        extra = [stub_o] if stub_names else []
        argv = [t.cc, '-g', '-o', exe] + objs + extra + t.sdl_libs
        if t.name == 'linux':
            argv += ['-no-pie', '-lm', '-lstdc++', '-pthread']
        else:
            argv += ['-static-libgcc', '-static-libstdc++', '-mconsole']
        r = run(argv, cwd=ROOT)
        if r.returncode == 0:
            break
        missing = set(re.findall(r"undefined reference to `([^']+)'", r.stderr))
        if not stubs or not missing:
            print(r.stderr)
            die('link failed')
        new = missing - set(stub_names)
        if not new:
            print(r.stderr)
            die('link failed')
        for name in new:
            stub_names[name] = symbol_kind(t, name, objs)
        write_stubs(stub_c, stub_names)
        r2 = run([t.cc] + HOST_CFLAGS + t.cflags + ['-I', HOST, '-c', stub_c, '-o', stub_o], cwd=ROOT)
        if r2.returncode:
            print(r2.stderr)
            die('stubs.c failed to compile')
    else:
        die('link failed after stubbing')
    if stub_names:
        funcs = sorted(n for n, k in stub_names.items() if k == 'func')
        data = sorted(n for n, k in stub_names.items() if k == 'data')
        print('stubbed %d functions, %d data symbols (see %s)' % (
            len(funcs), len(data), os.path.relpath(stub_c, ROOT)))
    print('linked %s' % os.path.relpath(exe, ROOT))
    copy_game(t)


def symbol_kind(t, name, objs):
    """'func' or 'data', from how the objects refer to the symbol: on ELF a
    call goes through a PLT32 relocation; mingw reaches a variable through a
    .refptr.NAME slot."""
    if t.name == 'win':
        for o in objs:
            r = run([t.nm, '-P', o], cwd=ROOT)
            if ('.refptr.' + name + ' ') in r.stdout:
                return 'data'
        return 'func'
    for o in objs:
        r = run(['objdump', '-r', o], cwd=ROOT)
        for line in r.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[2].split('-')[0].split('+')[0] == name:
                return 'func' if 'PLT32' in parts[1] else 'data'
    return 'func'


def write_stubs(path, names):
    lines = ['/* generated by tools/build.py: symbols nothing defines yet */',
             '#include "stubs.h"']
    for name in sorted(names):
        if names[name] == 'func':
            lines.append('long %s(void) { return host_stub("%s"); }' % (name, name))
        else:
            lines.append('_Alignas(16) char %s[65536];' % name)
    write_if_changed(path, '\n'.join(lines) + '\n')


def copy_game(t):
    """The original game's files into the output directory, keeping what is
    there (saves)."""
    src = os.path.join(ROOT, 'apps', 'DSAVANT')
    if not os.path.isdir(src):
        print('build: no %s: the output has no game data' % os.path.relpath(src, ROOT))
        return
    import shutil
    skip = {'DS.EXE', 'VGA.DRV', 'EGA.DRV', 'PLAY.DRV'}
    copied = 0
    for name in sorted(os.listdir(src)):
        if name.upper() in skip or name.upper().endswith('.OVR'):
            continue
        dst = os.path.join(t.out, name)
        if os.path.exists(dst):
            continue
        p = os.path.join(src, name)
        if os.path.isdir(p):
            shutil.copytree(p, dst)
        else:
            shutil.copy2(p, dst)
        copied += 1
    if copied:
        print('copied %d game file(s) from apps/DSAVANT' % copied)
    if t.name == 'win':
        dll = os.path.join(t.out, 'SDL3.dll')
        if mtime(dll) < mtime(t.sdl_dll):
            shutil.copy2(t.sdl_dll, dll)


if __name__ == '__main__':
    main()
