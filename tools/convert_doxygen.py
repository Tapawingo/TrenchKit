"""Convert /// Doxygen comments to /** */ block style.

Leaves ///<  inline member documentation unchanged.
Run from the repository root: python tools/convert_doxygen.py
"""
import re
from pathlib import Path

DOC_LINE   = re.compile(r'^(\s*)/// ?(.*)$')
INLINE_DOC = re.compile(r'^(\s*)///<')


def convert(lines):
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        # Inline member docs (///< ...) are left as-is
        if INLINE_DOC.match(line):
            out.append(line)
            i += 1
            continue
        m = DOC_LINE.match(line)
        if not m:
            out.append(line)
            i += 1
            continue
        # Collect the full /// block (same indentation level)
        indent = m.group(1)
        block = []
        while i < len(lines):
            if INLINE_DOC.match(lines[i]):
                break
            bm = DOC_LINE.match(lines[i])
            if not bm or bm.group(1) != indent:
                break
            block.append(bm.group(2))  # text after "/// " (or "" for blank lines)
            i += 1
        # Emit /** */ block
        out.append(indent + '/**\n')
        for content in block:
            out.append(indent + ' *' + (' ' + content if content else '') + '\n')
        out.append(indent + ' */\n')
    return out


src = Path('src')
changed = 0
for path in sorted(src.rglob('*')):
    if path.suffix not in ('.h', '.cpp'):
        continue
    original = path.read_text(encoding='utf-8')
    lines = original.splitlines(keepends=True)
    result = convert(lines)
    new_text = ''.join(result)
    if new_text != original:
        path.write_text(new_text, encoding='utf-8')
        changed += 1
        print(f'  {path}')

print(f'\nDone — {changed} files updated.')
