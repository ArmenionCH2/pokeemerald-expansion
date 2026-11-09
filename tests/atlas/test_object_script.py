"""Execute the native script lookup with missing and permanent map templates."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / 'src/event_object_movement.c').read_text()
start = source.index('static const u8 *GetObjectEventScriptPointerByLocalIdAndMap(')
brace = source.index('{', start)
depth = 1
end = brace + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
function = source[start:end]
out = root / 'build/atlas-tests'
out.mkdir(parents=True, exist_ok=True)
fixture = out / 'object_script.c'
fixture.write_text('''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8;
struct ObjectEventTemplate { const u8 *script; };
static struct ObjectEventTemplate object;
static const struct ObjectEventTemplate *lookup;
static const struct ObjectEventTemplate *GetObjectEventTemplateByLocalIdAndMap(u8 localId, u8 mapNum, u8 mapGroup)
{
    assert(localId == 100 && mapNum == 5 && mapGroup == 7);
    return lookup;
}
''' + function + '''
int main(void)
{
    static const u8 script[] = {2};
    lookup = NULL;
    assert(GetObjectEventScriptPointerByLocalIdAndMap(100,5,7) == NULL);
    lookup = &object;
    object.script = NULL;
    assert(GetObjectEventScriptPointerByLocalIdAndMap(100,5,7) == NULL);
    object.script = script;
    assert(GetObjectEventScriptPointerByLocalIdAndMap(100,5,7) == script);
    return 0;
}
''')
exe = out / 'object_script'
subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-Wall', '-Wextra',
                '-Werror', '-fsanitize=address,undefined', str(fixture), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
print('PASS native script lookup: transient object, null script, permanent script')
