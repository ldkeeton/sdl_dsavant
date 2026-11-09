#!/usr/bin/env python3
"""Rewrite preprocessed game C so it keeps Aztec C86's integer sizes.

    c16.py IN.i OUT.i [--defs DEFS] [--protos PROTOS] [--host-dir DIR ...]

Aztec C86 has a 16-bit int and a 32-bit long; the host has a 32-bit int.
After `gcc -E`, this rewrites the tokens of game code:

  * int -> short, long -> int, a bare unsigned/signed -> unsigned/signed short
    (char and short stay; `long int` -> int, `short int` -> short)
  * an L/l suffix on a number is dropped (a host long is 64 bits on Linux)
  * (unsigned)N for N in 32768..65535 becomes ((short)N): the same 16 bits,
    which compare equal to an int (short) holding them, as on DOS
  * a declaration with no type at all (implicit int: `main()`, `static x;`,
    `register i;`) gets an explicit `short`
  * a K&R parameter that has no declaration gets `short NAME;`

Code from system headers (cpp line-marker flag 3) and from files under a
--host-dir is left alone.

--defs writes one line per external function this file defines:
    NAME<TAB>DECLARATION
for the build to collect into the prototype header of every file.
--protos inserts such a header at the top of the output, leaving out the
names this file declares static.
"""

import re
import sys

KEYWORDS = {
    'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
    'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if', 'int',
    'long', 'register', 'return', 'short', 'signed', 'sizeof', 'static',
    'struct', 'switch', 'typedef', 'union', 'unsigned', 'void', 'volatile',
    'while', 'inline', '__inline', '__inline__', '__attribute__',
    '__extension__', '__asm__', 'asm', '_Bool', '__restrict', 'restrict',
}
INT_WORDS = {'int', 'long', 'short', 'unsigned', 'signed', 'char'}
STORAGE = {'static', 'extern', 'register', 'auto', 'typedef', 'inline',
           '__inline', '__inline__'}
QUALS = {'const', 'volatile'}
TAGS = {'struct', 'union', 'enum'}
OTHER_TYPES = {'void', 'float', 'double', '_Bool'}
# Parameter types that a call promotes to int.
PROMOTED = {'short', 'unsigned short', 'signed short', 'char', 'unsigned char',
            'signed char', 'const short', 'const char'}

TOKEN = re.compile(r'''
    (?P<ws>[ \t\r\f\v]+)
  | (?P<nl>\n)
  | (?P<str>"(?:[^"\\\n]|\\.)*")
  | (?P<chr>'(?:[^'\\\n]|\\.)*')
  | (?P<num>\.?[0-9](?:[eEpP][+-]|[0-9a-zA-Z_.])*)
  | (?P<id>[A-Za-z_$][A-Za-z_0-9$]*)
  | (?P<op>\.\.\.|<<=|>>=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|.)
''', re.X | re.S)

LINE_MARKER = re.compile(r'#\s*(?:line\s+)?(\d+)\s+"((?:[^"\\]|\\.)*)"(.*)')


class Tok:
    __slots__ = ('kind', 'text', 'pos', 'mine')

    def __init__(self, kind, text, pos, mine):
        self.kind, self.text, self.pos, self.mine = kind, text, pos, mine

    def __repr__(self):
        return self.text


def tokenize(src, host_dirs):
    """Tokens of src (no whitespace, no line markers), each marked `mine`
    when it comes from game code."""
    toks = []
    mine = True
    pos = 0
    at_bol = True
    n = len(src)
    while pos < n:
        if at_bol:
            # A directive line: line markers set where the code comes from;
            # #pragma and the rest are skipped whole.
            m = re.compile(r'[ \t]*#').match(src, pos)
            if m:
                end = src.find('\n', pos)
                end = n if end < 0 else end
                line = src[m.end() - 1:end]
                lm = LINE_MARKER.match(line)
                if lm:
                    path = lm.group(2)
                    flags = lm.group(3).split()
                    mine = not ('3' in flags or path.startswith('<')
                                or any(path.startswith(d) for d in host_dirs))
                pos = end
                continue
        m = TOKEN.match(src, pos)
        kind = m.lastgroup
        if kind == 'nl':
            at_bol = True
        elif kind != 'ws':
            at_bol = False
            toks.append(Tok(kind, m.group(), pos, mine))
        pos = m.end()
    return toks


class Edits:
    def __init__(self):
        self.list = []      # (pos, length, text)

    def replace(self, tok, text):
        self.list.append((tok.pos, len(tok.text), text))

    def insert(self, pos, text):
        self.list.append((pos, 0, text))

    def apply(self, src):
        out = []
        last = 0
        for pos, length, text in sorted(self.list, key=lambda e: (e[0], -e[1] == 0)):
            if pos < last:
                continue
            out.append(src[last:pos])
            out.append(text)
            last = pos + length
        out.append(src[last:])
        return ''.join(out)


def rewrite_int_words(toks, edits):
    """int/long/unsigned/signed -> the host types of the same size."""
    n = len(toks)
    for i in range(n - 3):
        if (toks[i].mine and toks[i].text == '(' and toks[i + 1].text == 'unsigned'
                and toks[i + 2].text == ')' and toks[i + 3].kind == 'num'):
            m = NUM_SUFFIX.match(toks[i + 3].text) or re.match(r'^(0[xX][0-9a-fA-F]+|[0-9]+)$', toks[i + 3].text)
            if m:
                value = int(m.group(1), 0) if not re.match(r'^0[0-7]+$', m.group(1)) else int(m.group(1), 8)
                if 32768 <= value <= 65535:
                    # A 16-bit pattern such as (unsigned)65281 (0xff01): on
                    # the host an unsigned short promotes to int and would
                    # not compare equal to an int holding the same bits.
                    end = toks[i + 3].pos + len(toks[i + 3].text)
                    edits.list.append((toks[i].pos, end - toks[i].pos, '((short)%d)' % value))
    i = 0
    while i < n:
        t = toks[i]
        if not (t.mine and t.kind == 'id' and t.text in INT_WORDS):
            if t.mine and t.kind == 'num':
                rewrite_number(t, edits)
            i += 1
            continue
        # A run of integer type words, maybe with qualifiers between.
        j = i
        words = []
        while j < n and toks[j].kind == 'id' and (toks[j].text in INT_WORDS or toks[j].text in QUALS):
            words.append(toks[j])
            j += 1
        names = [w.text for w in words if w.text in INT_WORDS]
        quals = [w.text for w in words if w.text in QUALS]
        if 'char' in names:
            i = j
            continue
        sign = 'unsigned ' if 'unsigned' in names else ('signed ' if 'signed' in names else '')
        size = 'int' if 'long' in names else 'short'
        if 'long' in names and names.count('long') > 1:
            size = 'long long'
        new = ' '.join(quals + [(sign + size).strip()])
        first = words[0]
        last = words[-1]
        edits.list.append((first.pos, last.pos + len(last.text) - first.pos, new))
        i = j


NUM_SUFFIX = re.compile(r'^(0[xX][0-9a-fA-F]+|[0-9]+)([uUlL]+)$')


def rewrite_number(t, edits):
    m = NUM_SUFFIX.match(t.text)
    if not m:
        return
    suffix = m.group(2)
    if 'l' in suffix.lower():
        new = m.group(1) + ('u' if 'u' in suffix.lower() else '')
        edits.replace(t, new)


def match_close(toks, i):
    """Index of the bracket that closes toks[i]."""
    pairs = {'(': ')', '[': ']', '{': '}'}
    open_, close = toks[i].text, pairs[toks[i].text]
    depth = 0
    for j in range(i, len(toks)):
        if toks[j].kind != 'op':
            continue
        if toks[j].text == open_:
            depth += 1
        elif toks[j].text == close:
            depth -= 1
            if depth == 0:
                return j
    return len(toks) - 1


class Scope:
    def __init__(self, parent=None):
        self.typedefs = set()
        self.parent = parent

    def is_typedef(self, name):
        s = self
        while s:
            if name in s.typedefs:
                return True
            s = s.parent
        return False


def split_top(toks, sep):
    """Split a token list at the given separators outside brackets."""
    parts = [[]]
    depth = 0
    for t in toks:
        if t.kind == 'op' and t.text in '([{':
            depth += 1
        elif t.kind == 'op' and t.text in ')]}':
            depth -= 1
        if depth == 0 and t.kind == 'op' and t.text == sep:
            parts.append([])
        else:
            parts[-1].append(t)
    return parts


def specifier_end(decl, scope):
    """(index past the declaration specifiers, has_type, storage words)."""
    i = 0
    has_type = False
    storage = []
    n = len(decl)
    while i < n:
        t = decl[i]
        if t.kind != 'id':
            break
        w = t.text
        if w in STORAGE:
            storage.append(w)
            i += 1
        elif w in QUALS or w in ('__extension__', '__restrict', 'restrict'):
            i += 1
        elif w == '__attribute__':
            i += 1
            if i < n and decl[i].text == '(':
                i = match_close(decl, i) + 1
        elif w in INT_WORDS or w in OTHER_TYPES:
            has_type = True
            i += 1
        elif w in TAGS:
            has_type = True
            i += 1
            if i < n and decl[i].kind == 'id':
                i += 1
            if i < n and decl[i].text == '{':
                i = match_close(decl, i) + 1
        elif not has_type and scope.is_typedef(w):
            has_type = True
            i += 1
        else:
            break
    return i, has_type, storage


def declarator_name(toks):
    """The identifier a declarator declares (None for an abstract one)."""
    for k, t in enumerate(toks):
        if t.kind == 'id' and t.text not in KEYWORDS:
            return t, k
        if t.kind == 'op' and t.text in ('[', '='):
            break
    return None, None


class Rewriter:
    """Walks the file-scope declarations; inside function bodies only the
    storage-class rule applies (`register i;`)."""

    def __init__(self, toks, edits):
        self.toks = toks
        self.edits = edits
        self.defs = []          # (name, declaration text)
        self.statics = set()
        self.decls = {}         # function name -> return type, as declared here
        self.scope = Scope()

    def run(self):
        toks = self.toks
        i = 0
        n = len(toks)
        while i < n:
            j = self.declaration_end(i)
            self.declaration(i, j)
            i = j

    def declaration_end(self, i):
        """End (exclusive) of the file-scope declaration starting at i."""
        toks = self.toks
        j = i
        n = len(toks)
        while j < n:
            t = toks[j]
            if t.kind == 'op':
                if t.text in '([':
                    j = match_close(toks, j) + 1
                    continue
                if t.text == '{':
                    k = match_close(toks, j)
                    if self.is_function_body(i, j):
                        return k + 1
                    j = k + 1
                    continue
                if t.text == ';':
                    if self.knr_header(i, j):
                        j += 1
                        continue
                    return j + 1
            j += 1
        return n

    def is_function_body(self, i, j):
        """Whether the `{` at j opens a function body (vs an initializer or
        a struct, union or enum body)."""
        toks = self.toks
        if j <= i:
            return False
        prev = toks[j - 1]
        return prev.text == ')' or (prev.text == ';' and self.knr_header(i, j - 1))

    def knr_header(self, i, j):
        """Whether toks[i:j] (ending before a `;`) is a K&R function header
        whose parameter declarations are still going."""
        toks = self.toks
        k = i
        while k < j:
            if toks[k].text == '(':
                c = match_close(toks, k)
                inner = toks[k + 1:c]
                if (inner and k > i and toks[k - 1].kind == 'id'
                        and all(t.kind == 'id' or t.text == ',' for t in inner)
                        and all(t.text not in KEYWORDS and not self.scope.is_typedef(t.text)
                                for t in inner if t.kind == 'id')
                        and c + 1 < j):
                    return True
                k = c + 1
                continue
            if toks[k].text in ('=', '{'):
                return False
            k += 1
        return False

    def declaration(self, i, j):
        toks = self.toks
        decl = toks[i:j]
        if not decl:
            return
        body = None
        if decl[-1].text == ';':
            decl_toks = decl[:-1]
        elif decl[-1].text == '}':
            # A function definition: find where its body opens.
            k = len(decl) - 1
            depth = 0
            while k >= 0:
                if decl[k].text == '}':
                    depth += 1
                elif decl[k].text == '{':
                    depth -= 1
                    if depth == 0:
                        break
                k -= 1
            if self.is_function_body(i, i + k):
                body = (i + k, j - 1)
                decl_toks = decl[:k]
            else:
                decl_toks = decl
        else:
            decl_toks = decl
        if not decl_toks:
            return
        n_spec, has_type, storage = specifier_end(decl_toks, self.scope)
        rest = decl_toks[n_spec:]
        if decl_toks[0].mine and not has_type and rest:
            first = rest[0]
            if first.kind == 'id' or first.text in ('*', '('):
                self.edits.insert(first.pos, 'short ')
        if 'typedef' in storage:
            for part in split_top(rest, ','):
                name, _ = declarator_name(part)
                if name is not None:
                    self.scope.typedefs.add(name.text)
            return
        if body is not None:
            self.function(decl_toks, n_spec, has_type, storage, body)
            return
        self.record_decls(decl_toks, n_spec, has_type, storage)

    def record_decls(self, decl_toks, n_spec, has_type, storage):
        """Note the functions a declaration declares, with their return
        types, and the static names."""
        rest = decl_toks[n_spec:]
        ret = None
        for part in split_top(rest, ','):
            name, idx = declarator_name(part)
            if name is None:
                continue
            if 'static' in storage:
                self.statics.add(name.text)
            if idx + 1 < len(part) and part[idx + 1].text == '(':
                if ret is None:
                    ret = render(decl_toks[:n_spec], drop=STORAGE, edits=self.edits)
                    if not has_type:
                        ret = (ret + ' short').strip()
                stars = render(part[:idx], edits=self.edits)
                self.decls.setdefault(name.text, (ret + ' ' + stars).strip())

    def function(self, decl_toks, n_spec, has_type, storage, body):
        rest = decl_toks[n_spec:]
        name, k = declarator_name(rest)
        if name is None:
            return
        if 'static' in storage:
            self.statics.add(name.text)
        p = k + 1
        while p < len(rest) and rest[p].text != '(':
            p += 1
        if p < len(rest):
            pc = p + match_close(rest[p:], 0)
            params = rest[p + 1:pc]
            knr = rest[pc + 1:]
            if knr or (params and all(t.kind == 'id' or t.text == ',' for t in params)
                       and not any(t.text in KEYWORDS or self.scope.is_typedef(t.text)
                                   for t in params if t.kind == 'id')):
                types = self.knr_params(params, knr, body, name.mine)
                if any(re.match(r'(struct|union) \w+$', ty) for ty in types):
                    # A struct by value: callers pass look-alike structs of
                    # other tags, so leave the arguments unchecked.
                    proto = '()'
                else:
                    proto = '(' + (', '.join(types) if types else '') + ')'
            elif not params:
                proto = '()'
            else:
                proto = '(' + render(params, edits=self.edits) + ')'
            if 'static' not in storage and name.mine:
                ret = render(decl_toks[:n_spec], drop=STORAGE, edits=self.edits)
                if not has_type:
                    ret = (ret + ' short').strip()
                stars = render(rest[:k], edits=self.edits)
                decl = ('%s %s' % (ret, stars)).strip(), proto
                # A typedef name means nothing at the top of another file.
                uses_typedef = any(t.kind == 'id' and self.scope.is_typedef(t.text)
                                   for t in list(decl_toks) + list(knr))
                if not uses_typedef:
                    self.defs.append((name.text,) + decl)
        self.local_storage(body[0] + 1, body[1])

    def knr_params(self, params, knr, body, mine):
        """Declare the K&R parameters that have no declaration, give an
        implicit type to `register x;` among the declarations, and return
        each parameter's type as a prototype would have it (promoted)."""
        names = [t.text for t in params if t.kind == 'id']
        types = {}
        for d in split_top(knr, ';'):
            if not d:
                continue
            ns, has_type, _ = specifier_end(d, self.scope)
            if d[0].mine and not has_type and ns < len(d):
                self.edits.insert(d[ns].pos, 'short ')
            spec = render(d[:ns], drop=STORAGE, edits=self.edits)
            if not has_type:
                spec = (spec + ' short').strip()
            for part in split_top(d[ns:], ','):
                nm, idx = declarator_name(part)
                if nm is None:
                    continue
                abstract = part[:idx] + part[idx + 1:]
                if not abstract and spec in PROMOTED:
                    types[nm.text] = 'int'
                else:
                    types[nm.text] = (spec + ' ' + render(abstract, edits=self.edits)).strip()
        missing = [nme for nme in names if nme not in types]
        if missing and mine:
            text = ''.join('short %s; ' % nme for nme in missing)
            self.edits.insert(self.toks[body[0]].pos, text)
        return [types.get(nme, 'int') for nme in names]

    def local_storage(self, start, end):
        """Declarations inside a body: one that starts with a storage class
        and has no type (`register i;`) gets `short`, and the functions they
        declare are noted."""
        toks = self.toks
        k = start
        while k < end:
            t = toks[k]
            starts = (k == start or toks[k - 1].text in (';', '{', '}'))
            if not (starts and t.kind == 'id' and (t.text in STORAGE or t.text in INT_WORDS
                                                   or t.text in OTHER_TYPES or t.text in TAGS
                                                   or t.text in QUALS)):
                k += 1
                continue
            j = k
            while j < end and toks[j].text != ';':
                if toks[j].text in '([{':
                    j = match_close(toks, j)
                j += 1
            decl = toks[k:j]
            ns, has_type, storage = specifier_end(decl, self.scope)
            if t.mine and not has_type and ns < len(decl):
                nxt = decl[ns]
                if nxt.kind == 'id' or nxt.text in ('*', '('):
                    self.edits.insert(nxt.pos, 'short ')
            if 'typedef' not in storage:
                saved = set(self.statics)
                self.record_decls(decl, ns, has_type, storage)
                self.statics = saved
            k = j + 1

def render(toks, drop=(), edits=None):
    """The text of a token list as rewritten, on one line."""
    toks = [t for t in toks if not (t.kind == 'id' and t.text in drop)]
    out = []
    for t in toks:
        out.append(edited_text(t, edits))
    return ' '.join(x for x in out if x)


def edited_text(t, edits):
    """A token's text after this file's edits (an edit that spans several
    tokens shows on the first and blanks the rest)."""
    if edits is None:
        return t.text
    text = t.text
    for pos, length, new in edits.list:
        if length == 0 and pos == t.pos:
            text = new + text
        elif length and pos == t.pos:
            text = new
        elif length and pos < t.pos < pos + length:
            return ''
    return text


def main(argv):
    args = argv[1:]
    host_dirs = []
    defs_out = protos_in = None
    pos = []
    while args:
        a = args.pop(0)
        if a == '--host-dir':
            host_dirs.append(args.pop(0))
        elif a == '--defs':
            defs_out = args.pop(0)
        elif a == '--protos':
            protos_in = args.pop(0)
        else:
            pos.append(a)
    if len(pos) != 2:
        sys.exit(__doc__)
    src = open(pos[0], encoding='latin-1').read()
    toks = tokenize(src, host_dirs)
    edits = Edits()
    rewrite_int_words(toks, edits)
    rw = Rewriter(toks, edits)
    rw.run()
    out = edits.apply(src)
    if protos_in:
        lines = []
        for line in open(protos_in, encoding='latin-1'):
            name, _, decl = line.rstrip('\n').partition('\t')
            if name in rw.statics:
                continue
            lines.append('extern %s;\n' % decl)
        out = ''.join(lines) + out
    open(pos[1], 'w', encoding='latin-1').write(out)
    if defs_out:
        with open(defs_out, 'w', encoding='latin-1') as f:
            for name, ret, proto in rw.defs:
                f.write('%s\t%s %s%s\n' % (name, ret, name, proto))


if __name__ == '__main__':
    main(sys.argv)
