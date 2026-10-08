"""PlatformIO extra script: export the actual linker invocation without rebuilding."""
Import('env')
from pathlib import Path
import json

def capture(source, target, env):
    elf = env.File(env.subst('$BUILD_DIR/${PROGNAME}.elf'))
    previous = env.get('MAXLINELENGTH', 2048)
    env['MAXLINELENGTH'] = 1000000
    try:
        args = [str(x) for x in env.subst_list('$LINKCOM', target=[elf], source=elf.sources)[0]]
    finally:
        env['MAXLINELENGTH'] = previous
    if not elf.sources:
        raise RuntimeError('No linker source nodes; cannot export a trustworthy relink command')
    out = Path(env.subst('$PROJECT_DIR'))/'test-results/relink-link.json'
    out.parent.mkdir(exist_ok=True)
    out.write_text(json.dumps({'argv':args, 'elf_sources':[str(s) for s in elf.sources],
                              'arduino_source':env.PioPlatform().get_package_dir('framework-arduinoespressif32')},indent=2),encoding='utf-8')
    print('Captured actual linker arguments:', len(args))

env.AddCustomTarget('export_relink', None, capture, title='Export existing firmware linker inputs')
env.AddPostAction('$BUILD_DIR/${PROGNAME}.elf', capture)
