#!/usr/bin/env python3
"""Run preserved Atlas host contracts against live source, with sanitizers.
These tests use host mocks, not the GBA ABI or an emulator.
"""
import os
from pathlib import Path
import subprocess
R = Path(__file__).resolve().parents[1]
OUT = R / 'build/atlas-tests'
OUT.mkdir(parents=True, exist_ok=True)
world = ['terrain','mountain','tiles','tiles_data','stream','plan','habitat','water','geo']
groups = {
    'place': ['place'],
    'state weather': ['state','state_codec','weather'],
    'pokemon': ['atlas_pokemon', 'atlas_pokemon_boundaries'],
    'actor_identity': ['atlas_actor_identity', 'actor_identity_property'],
    'actor_identity actor_admission': ['actor_admission'],
    'actor_identity actor_admission actor_runtime': ['actor_runtime'],
    ' '.join(world): ['terrain','plan','geo','stream','stream_wrap','ground_presentation','landing_repair','lake_inlets','confluences','connected_relief','foothills'],
    'habitat': ['habitat_equivalence'],
    'mountain tiles tiles_data': ['atlas_tiles'],
    '': ['stream_polar','stream_frontier_native','stream_poison_native','water_equivalence','water_junctions'],
    'plan': ['plan_cache_native'],
}
for modules, cases in groups.items():
    for case in cases:
        exe = OUT / case
        cmd = [os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
               '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
               '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections',
               '-Wl,--gc-sections', '-Itests/atlas/mocks', '-Iinclude']
        test_text = (R / ('tests/atlas/test_' + case + '.c')).read_text()
        cmd += ['src/atlas_' + name + '.c' for name in modules.split()
                if ('/atlas_' + name + '.c\"') not in test_text]
        cmd += ['tests/atlas/test_' + case + '.c', '-o', str(exe)]
        subprocess.run(cmd, cwd=R, check=True)
        subprocess.run([str(exe)], cwd=R, check=True)
        print('PASS:', case, flush=True)
subprocess.run(['python3', str(R / 'tests/atlas/test_object_script.py')], cwd=R, check=True)
