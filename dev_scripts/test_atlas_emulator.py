#!/usr/bin/env python3
"""Build/run the mGBA key-driven Atlas smoke test against the current ELF/ROM.
Requires libmgba-dev and arm-none-eabi-nm. Output and test save stay in build/.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
R=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rom',type=Path,default=R/'pokeemerald.gba')
parser.add_argument('--elf',type=Path,default=R/'pokeemerald.elf')
parser.add_argument('--save',type=Path,help='Copy a test save; never alter the supplied file')
parser.add_argument('--tour',choices=['materials','mesa','headwaters'],help='Run a focused real-key geography tour instead of the complete smoke test')
parser.add_argument('--snapshot',type=Path,help='Dump the live journal after world entry instead of the full smoke test')
args=parser.parse_args()
if args.tour and args.snapshot:parser.error('--tour and --snapshot are mutually exclusive')
out=R/'build/atlas-emulator';out.mkdir(parents=True,exist_ok=True)
rom=out/'atlas.gba'
shutil.copyfile(args.rom,rom)
save_data=args.save.read_bytes() if args.save else None
# Remove only this harness's previous save, not a developer/player save.
(out/'atlas.sav').unlink(missing_ok=True)
if save_data is not None:(out/'atlas.sav').write_bytes(save_data)
symbols=out/'symbols.txt'
symbols.write_bytes(subprocess.check_output(['arm-none-eabi-nm',str(args.elf)]))
exe=out/'emulator'
subprocess.run([os.environ.get('CC','cc'),'-O2',str(R/'tests/atlas/emulator.c'),'-lmgba','-o',str(exe)],check=True)
subprocess.run([str(exe),str(rom),str(symbols)]+([str(args.snapshot)] if args.snapshot else ['--'+args.tour] if args.tour else []),cwd=R,check=True,timeout=600)
