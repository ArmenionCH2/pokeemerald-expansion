#!/usr/bin/env python3
"""Build/run the mGBA key-driven Atlas smoke test against the current ELF/ROM.
Requires libmgba-dev and arm-none-eabi-nm. Output and test save stay in build/.
"""
from pathlib import Path
import os
import shutil
import subprocess
R=Path(__file__).resolve().parents[1]
out=R/'build/atlas-emulator';out.mkdir(parents=True,exist_ok=True)
rom=out/'atlas.gba'
shutil.copyfile(R/'pokeemerald.gba',rom)
# Remove only this harness's previous save, not a developer/player save.
(out/'atlas.sav').unlink(missing_ok=True)
symbols={}
for line in subprocess.check_output(['arm-none-eabi-nm',str(R/'pokeemerald.elf')]).decode().splitlines():
    fields=line.split()
    if len(fields)==3:symbols[fields[2]]=fields[0]
exe=out/'emulator'
subprocess.run([os.environ.get('CC','cc'),'-O2',str(R/'tests/atlas/emulator.c'),'-lmgba','-o',str(exe)],check=True)
subprocess.run([str(exe),str(rom),symbols['gAtlasCreationState'],symbols['gSaveBlock1Ptr'],symbols['gSaveBlock3Ptr'],symbols['gAtlasMapActive']],cwd=R,check=True,timeout=300)
