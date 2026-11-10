"""A busy native QA workspace must fail before overwriting mapped test files."""
from pathlib import Path
import fcntl,shutil,subprocess,sys,tempfile
R=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='atlas-emulator-lock-') as directory:
    stage=Path(directory);(stage/'dev_scripts').mkdir()
    script=stage/'dev_scripts/test_atlas_emulator.py'
    shutil.copy2(R/'dev_scripts/test_atlas_emulator.py',script)
    out=stage/'build/atlas-emulator';out.mkdir(parents=True)
    sentinels=['atlas.gba','atlas.sav','symbols.txt','emulator']
    for name in sentinels:(out/name).write_bytes(('mapped fixture '+name).encode())
    before={name:(out/name).read_bytes() for name in sentinels}
    with (out/'.run.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        result=subprocess.run([sys.executable,str(script)],capture_output=True,text=True)
        assert result.returncode==2 and 'Another Atlas emulator check is running' in result.stderr
    assert before=={name:(out/name).read_bytes() for name in sentinels}
print('PASS native emulator lock rejects an overlapping run before ROM/save/symbol/executable mutation')
