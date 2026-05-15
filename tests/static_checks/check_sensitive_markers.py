from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
markers=[
    'Toy' + ' Castle',
    'Alkar' + 'net',
    'github' + '_pat_',
    'g' + 'hp_',
    'g' + 'ho_',
    'BEGIN ' + 'PRIVATE ' + 'KEY',
]
bad=[]
for p in root.rglob('*'):
    if p.is_file() and '.git' not in p.parts:
        try:
            text=p.read_text(encoding='utf-8',errors='ignore')
        except Exception:
            continue
        if any(m in text for m in markers):
            bad.append(str(p.relative_to(root)))
if bad:
    print('Potential secret markers found:')
    print('\n'.join(bad))
    sys.exit(1)
print('No known secret markers found.')
