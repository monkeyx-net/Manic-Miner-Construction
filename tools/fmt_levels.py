#!/usr/bin/env python3
import json, re, sys

PATH = 'levels.json'

def fmt(obj):
    s = json.dumps(obj, indent=2, ensure_ascii=False)
    s = re.sub(r'\[\s+([^\[\]\{]*?)\s+\]', lambda m: '[' + re.sub(r'\s+', ' ', m.group(1)).strip() + ']', s)
    return s + '\n'

with open(PATH) as f:
    cur = f.read()
out = fmt(json.loads(cur))
if '--check' in sys.argv[1:]:
    if cur != out:
        print(f'{PATH} is not canonically formatted: run make fmt-levels')
        sys.exit(1)
elif cur != out:
    with open(PATH, 'w') as f:
        f.write(out)
