#!/usr/bin/env python3
"""Run preserved Atlas host contracts against live source, with sanitizers.
These tests use host mocks, not the GBA ABI or an emulator.
"""
import os
import argparse
from pathlib import Path
import subprocess
R = Path(__file__).resolve().parents[1]
OUT = R / 'build/atlas-tests'
OUT.mkdir(parents=True, exist_ok=True)
world = ['terrain','mountain','tiles','tiles_data','stream','plan','climate','region','habitat','water','geo','ocean']
groups = {
    'climate': ['climate'],
    'region': ['region'],
    'place': ['place'],
    'state weather': ['state','state_codec','weather'],
    'pokemon': ['atlas_pokemon', 'atlas_pokemon_boundaries'],
    'actor_identity': ['atlas_actor_identity', 'actor_identity_property'],
    'actor_identity actor_admission': ['actor_admission'],
    'actor_identity actor_admission actor_runtime': ['actor_runtime'],
    ' '.join(world): ['terrain','plan','geo','stream','stream_wrap','ground_presentation','landing_repair','lake_inlets','confluences','connected_relief','foothills','ocean_generation','relief_generation','bank_generation','erosion_generation','bathymetry','island_generation','volcano','rock_ground','estuary_generation','small_network','floodplain_generation','creek_generation','dune_tiles','dune_generation','material_edges','rock_sand','rock_snow','snow_sand','mesa','headwater_generation','headwater_stream','coast_tiles','coast_generation','coast_stream','karst_stream','karst_search','karst','canyon','canyon_stream','wet_floodplain','wet_floodplain_stream','distributary','distributary_stream','frame_division','climate_transport','vegetation','undergrowth','blooms','ice','sand_corners'],
    'habitat': ['habitat_equivalence'],
    'mountain tiles tiles_data': ['atlas_tiles'],
    '': ['stream_polar','stream_frontier_native','stream_poison_native','water_equivalence','water_junctions','bank_profile','canyon_profile','wet_floodplain_profile','estuary_profile','floodplain_profile','creek_profile','creek_budget','headwater_budget','distributary_budget'],
    'plan climate': ['plan_cache_native'],
    ' '.join(world)+' place': ['lake_identity','river_identity'],
}
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--case',action='append',choices=sorted(case for cases in groups.values() for case in cases),
                    help='Run a named contract; repeat for multiple cases. Default: complete suite.')
selected=parser.parse_args().case
for modules, cases in groups.items():
    for case in cases:
        if selected and case not in selected:continue
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
subprocess.run(['python3', str(R / 'tests/atlas/test_native_assets.py')], cwd=R, check=True)

subprocess.run(['python3', str(R / 'tests/atlas/test_emulator_lock.py')], cwd=R, check=True)

if not selected or 'blooms' in selected:
    subprocess.run(['python3', str(R / 'tests/atlas/test_bloom_animation.py')], cwd=R, check=True)
