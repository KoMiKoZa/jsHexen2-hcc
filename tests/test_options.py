"""Check the compiler stamp, the printed output path, -progdefs and -noot with release/hcc.exe.

Run after build_vs2022.bat:  python tests/test_options.py
"""
import ctypes
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HCC = os.path.join(ROOT, 'release', 'hcc.exe')
WARN = os.path.join(ROOT, 'tests', 'warn')
FTE = os.path.join(ROOT, 'tests', 'fte')
VERSION = re.search(r'#define HCC_VERSION "([^"]+)"', open(os.path.join(ROOT, 'hcc.H')).read()).group(1)
failures = []


def check(ok, what):
    print(('ok    ' if ok else 'FAIL  ') + what)
    if not ok:
        failures.append(what)


def long_path(path):
    buf = ctypes.create_unicode_buffer(1024)
    n = ctypes.windll.kernel32.GetLongPathNameW(os.path.abspath(path), buf, 1024)
    return buf.value if n else os.path.abspath(path)


def short_path(path):
    buf = ctypes.create_unicode_buffer(1024)
    n = ctypes.windll.kernel32.GetShortPathNameW(path, buf, 1024)
    return buf.value if n else path


def compile_in(src, flags, out_name):
    """Compiles a copy of src; returns the result, the progs and the files left in the folder."""
    work = tempfile.mkdtemp(prefix='hcc options ')
    for name in os.listdir(src):
        shutil.copy(os.path.join(src, name), work)
    before = set(os.listdir(work))
    r = subprocess.run([HCC] + flags, cwd=short_path(work), capture_output=True, text=True)
    out = os.path.join(work, out_name)
    data = open(out, 'rb').read() if os.path.exists(out) else None
    files = {n: open(os.path.join(work, n), 'rb').read() for n in set(os.listdir(work)) - before}
    shutil.rmtree(work)
    return r, data, files


def header(data):
    return struct.unpack_from('<15i', data)


def strings(data):
    h = header(data)
    return data[h[10]:h[10] + h[11]]


def stamp_of(data):
    table = strings(data).rstrip(b'\0')
    return table[table.rindex(b'\0') + 1:].decode('latin-1'), table.rindex(b'\0') + 1


def referenced(data, ofs):
    h = header(data)
    for t, o, n in struct.iter_unpack('<HHi', data[h[4]:h[4] + h[5] * 8]):
        if n == ofs or (t & 0x7fff == 1 and struct.unpack_from('<i', data, h[12] + 4 * o)[0] == ofs):
            return True
    for t, o, n in struct.iter_unpack('<HHi', data[h[6]:h[6] + h[7] * 8]):
        if n == ofs:
            return True
    for f in struct.iter_unpack('<7i8B', data[h[8]:h[8] + h[9] * 36]):
        if ofs in (f[4], f[5]):
            return True
    return False


def crc16(text):
    crc = 0xffff
    for b in text:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xffff
    return crc


# The compiler stamp
r1, a, files = compile_in(WARN, [], 'warn.dat')
r2, b, _ = compile_in(WARN, [], 'warn.dat')
check(r1.returncode == 0 and a is not None, 'warn.hc compiles with no options')
stamp, ofs = stamp_of(a)
check(stamp == 'jsHexen2-hcc %s -ot' % VERSION, 'the stamp names the version and -ot: %r' % stamp)
check(a == b, 'two builds of the same source are byte-identical')
check(not referenced(a, ofs), 'nothing in the progs refers to the stamp')
for flags, expect in ((['-fte'], '-fte -ot'), (['-noot', '-noretfix', '-noifvecfix', '-oi', '-on'],
                                               '-noot -noretfix -noifvecfix -oi -on'),
                      (['-on', '-oi', '-noot'], '-noot -oi -on'),
                      (['-ot', '-quiet', '-nowarnings', '-o', 'other.dat'], '-ot')):
    src = FTE if '-fte' in flags else WARN
    out = 'other.dat' if '-o' in flags else os.path.basename(src) + '.dat'
    r, data, _ = compile_in(src, flags, out)
    check(data is not None and stamp_of(data)[0] == 'jsHexen2-hcc %s %s' % (VERSION, expect),
          'stamp for %s is "... %s"' % (' '.join(flags), expect))

# The printed output path
work = tempfile.mkdtemp(prefix='hcc options ')
os.mkdir(os.path.join(work, 'out folder'))
for name in os.listdir(WARN):
    shutil.copy(os.path.join(WARN, name), work)
for flags, rel in (([], 'warn.dat'), (['-o', r'out folder\..\out folder\x.dat'], r'out folder\x.dat')):
    r = subprocess.run([HCC] + flags, cwd=short_path(work), capture_output=True, text=True)
    printed = re.search(r'^object file (.*)$', r.stdout, re.M)
    expect = long_path(os.path.join(work, rel))
    check(printed is not None and printed.group(1) == expect,
          'prints the full long path (%s): %s' % (' '.join(flags) or 'no options',
                                                  printed.group(1) if printed else None))
if short_path(work) == long_path(work):
    print('note  this folder has no 8.3 name, so only the absolute path is tested')
shutil.rmtree(work)

# progdefs.h only with -progdefs
check('progdefs.h' not in files and 'files.dat' in files, 'no progdefs.h without -progdefs')
r, c, files = compile_in(WARN, ['-progdefs'], 'warn.dat')
check('progdefs.h' in files, '-progdefs writes progdefs.h')
check(c == a, '-progdefs leaves the progs unchanged')
text = files.get('progdefs.h', b'').replace(b'\r\n', b'\n')
body, _, define = text.rpartition(b'#define PROGHEADER_CRC ')
check(define.strip() == str(header(a)[1]).encode() and crc16(body) == header(a)[1],
      'the header CRC is the CRC of progdefs.h, with or without -progdefs')

# -ot is the default; -noot gives Raven's allocation
r, d, _ = compile_in(WARN, ['-ot'], 'warn.dat')
check(d == a, '-ot gives the same progs as no options')
r, e, _ = compile_in(WARN, ['-noot'], 'warn.dat')
check(e is not None and e != a, '-noot gives a different progs')
_, f, _ = compile_in(FTE, ['-fte'], 'fte.dat')
_, g, _ = compile_in(FTE, ['-fte', '-noot'], 'fte.dat')
check(f is not None and g is not None and header(g)[13] > header(f)[13],
      '-noot gives every intermediate result its own global (fte.hc: %d globals, %d by default)'
      % (header(g)[13] if g else 0, header(f)[13] if f else 0))
check('temps:' in r1.stdout and 'temps:' not in r.stdout, 'the temps line is printed only when they are shared')

sys.exit(1 if failures else 0)
