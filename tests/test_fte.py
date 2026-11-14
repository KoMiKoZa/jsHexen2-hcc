"""Compile tests/fte with release/hcc.exe and check the -fte compatibility layer.

Run after build_vs2022.bat:  python tests/test_fte.py
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HCC = os.path.join(ROOT, 'release', 'hcc.exe')
SRC = os.path.join(ROOT, 'tests', 'fte')
SAVE_GLOBAL = 0x8000
failures = []


def check(ok, what):
    print(('ok    ' if ok else 'FAIL  ') + what)
    if not ok:
        failures.append(what)


def compile_fte(flags):
    work = tempfile.mkdtemp(prefix='hcc_fte_')
    for name in os.listdir(SRC):
        shutil.copy(os.path.join(SRC, name), work)
    r = subprocess.run([HCC] + flags, cwd=work, capture_output=True, text=True)
    out = os.path.join(work, 'fte.dat')
    data = open(out, 'rb').read() if os.path.exists(out) else None
    shutil.rmtree(work)
    return r, data


class Progs:
    def __init__(self, data):
        h = struct.unpack_from('<15i', data)
        self.data = data
        self.ofs_str, self.ofs_glob = h[10], h[12]
        self.statements = list(struct.iter_unpack('<Hhhh', data[h[2]:h[2] + h[3] * 8]))
        self.defs = {}
        self.all_defs = []
        for t, o, n in struct.iter_unpack('<HHi', data[h[4]:h[4] + h[5] * 8]):
            self.defs.setdefault(self.s(n), (t, o))
            self.all_defs.append((self.s(n), t, o))

    def s(self, ofs):
        start = self.ofs_str + ofs
        return self.data[start:self.data.index(b'\0', start)].decode('latin-1')

    def float(self, name):
        return struct.unpack_from('<f', self.data, self.ofs_glob + 4 * self.defs[name][1])[0]

    def string(self, name):
        return self.s(struct.unpack_from('<i', self.data, self.ofs_glob + 4 * self.defs[name][1])[0])


r, data = compile_fte(['-ot'])
check(r.returncode != 0, 'without -fte the FTEQCC code is rejected')

for flags in (['-fte', '-ot'], ['-fte']):
    r, data = compile_fte(flags)
    check(r.returncode == 0 and data is not None, 'compiles with %s' % ' '.join(flags))
    if data is None:
        print(r.stdout)
        continue
    p = Progs(data)
    check(p.string('GREETING') == 'Hello', 'a string constant keeps its own value')
    check(p.float('LIMIT') == 10, 'a constant defined twice with the same value')
    check(p.float('big') == 1e10, 'a large integer is read as a float')
    check(p.float('NEG') == -0.5 and p.float('NEG2') == -1, 'a minus sign in a global value')
    arr = p.defs['NEGARR'][1]
    check(struct.unpack_from('<2f', p.data, p.ofs_glob + 4 * arr) == (-0.5, -1.0), 'a minus sign in array values')
    check(struct.unpack_from('<I', p.data, p.ofs_glob + 4 * p.defs['NEGZERO'][1])[0] == 0, '- 0 is +0, not -0')
    check(p.defs['g_inc'][0] & 0x7fff == 2, 'x++ leaves an unrelated field read alone (g_inc stays a float)')
    check(not p.defs['NOT_SAVED'][0] & SAVE_GLOBAL, 'nosave keeps a global out of saves')
    hidden = [(t, o) for n, t, o in p.all_defs if n == 'other_count']
    check(len(hidden) == 2 and len({o for t, o in hidden}) == 2,
          'a local named like a global is a separate variable')
    check('defined again with the same value' in r.stdout, 'warns about the repeated constant')

sys.exit(1 if failures else 0)
