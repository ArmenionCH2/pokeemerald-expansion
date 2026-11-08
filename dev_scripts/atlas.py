#!/usr/bin/env python3
"""Small Atlas workspace utility: destination guard and exact source recovery."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TARGET = 'ArmenionCH2/pokeemerald-expansion'
MIGRATION = ROOT / 'migration/atlas'
ALLOWED = {f'https://github.com/{TARGET}', f'https://github.com/{TARGET}.git',
           f'git@github.com:{TARGET}.git', f'ssh://git@github.com/{TARGET}.git'}

def git(*args, cwd=ROOT):
    return subprocess.check_output(['git', *args], cwd=cwd, stderr=subprocess.PIPE).decode().strip()

def guard(url):
    if url not in ALLOWED:
        raise SystemExit('Refusing write destination: only ' + TARGET + ' is allowed.')

def setup():
    urls = git('remote', 'get-url', '--push', '--all', 'origin').splitlines()
    for url in urls: guard(url)
    subprocess.run(['git','config','--local','core.hooksPath','.githooks'],cwd=ROOT,check=True)
    subprocess.run(['git','config','--local','remote.pushDefault','origin'],cwd=ROOT,check=True)
    subprocess.run(['git','config','--local','push.default','simple'],cwd=ROOT,check=True)
    subprocess.run(['git','config','--local','remote.origin.gh-resolved','base'],cwd=ROOT,check=True)
    print('Push guard installed; origin is the only allowed write repository.')

def verify():
    lock=json.loads((MIGRATION/'integrity.json').read_text())
    for name, digest in lock.items():
        assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest, name
    for url in git('remote','get-url','--push','--all','origin').splitlines(): guard(url)
    print('PASS immutable recovery inputs, vision, licensed source art and origin destination')

def recover(checkout, pristine):
    verify()
    ref=json.loads((MIGRATION/'SOURCE_REF.json').read_text())
    record=json.loads((MIGRATION/'EXTRACTED_SOURCE_PROVENANCE.json').read_text())
    checkout=Path(checkout).resolve()
    base=ref['pristine_upstream_commit'] if pristine else ref['pristine_import_commit']
    git('cat-file','-e',base+'^{commit}',cwd=checkout)
    out=ROOT/'build/atlas-recovery'
    if out.exists():
        raise SystemExit('Recovery output already exists; inspect or move build/atlas-recovery first.')
    with tempfile.TemporaryDirectory(prefix='atlas-recover-') as d:
        temp=Path(d)
        for entry in record['entries']:
            if entry['status'] != 'modified': continue
            source=entry['path'] if pristine else entry['source']
            data=subprocess.check_output(['git','show',base+':'+source],cwd=checkout)
            path=temp/entry['source'];path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        subprocess.run(['git','apply','--binary',str(MIGRATION/'original.patch')],cwd=temp,check=True)
        for entry in record['entries']:
            if entry['status']=='deleted_from_original': continue
            data=(temp/entry['source']).read_bytes()
            assert len(data)==entry['bytes']
            assert hashlib.sha256(data).hexdigest()==entry['sha256'],entry['path']
        # No output is published until all original bytes have been verified.
        for entry in record['entries']:
            if entry['status']=='deleted_from_original': continue
            dest=out/'game'/entry['path'];dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(temp/entry['source'],dest)
        shutil.copyfile(ROOT/'docs/atlas/FULL_PLAN.md',out/'FULL_PLAN.md')
        shutil.copyfile(MIGRATION/'EXTRACTED_SOURCE_PROVENANCE.json',out/'game/EXTRACTED_SOURCE_PROVENANCE.json')
    with zipfile.ZipFile(out/'AtlasGBA_FULL_GAME_SOURCE.zip','w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(out.rglob('*')):
            if p.is_file() and p.suffix!='.zip': z.write(p,p.relative_to(out))
    print(f'PASS all {len(record["entries"])} exact source files recovered: {out}')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='cmd',required=True)
    sub.add_parser('setup');sub.add_parser('verify')
    p=sub.add_parser('guard');p.add_argument('url')
    p=sub.add_parser('recover');p.add_argument('--checkout',required=True);p.add_argument('--pristine',action='store_true')
    args=parser.parse_args()
    if args.cmd=='setup':setup()
    elif args.cmd=='verify':verify()
    elif args.cmd=='guard':guard(args.url)
    else:recover(args.checkout,args.pristine)

if __name__=='__main__': main()
