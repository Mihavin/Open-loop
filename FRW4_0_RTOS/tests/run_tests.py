"""Run production C regression scenarios with a native C compiler and fake HAL."""
from pathlib import Path
import argparse
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cc',default='gcc')
args=parser.parse_args()
build=root/'tests/build';build.mkdir(exist_ok=True)
executable=build/('firmware_tests.exe' if sys.platform=='win32' else 'firmware_tests')
command=[args.cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'tests/mocks'),
         '-I'+str(root/'Core/Inc'),str(root/'tests/test_firmware.c'),'-lm','-o',str(executable)]
subprocess.run(command,check=True)
subprocess.run([str(executable)],check=True)
guard=subprocess.run([args.cc,'-x','c','-fsyntax-only','-DMOTOR_FOC_AUTOSTART=1',
                      '-I'+str(root/'Core/Inc'),'-'],input='#include "motor_foc.h"\n',
                     capture_output=True,text=True)
if guard.returncode==0 or 'Production firmware requires explicit arming' not in guard.stderr:
    raise RuntimeError('Automatic arming must be rejected at compile time')
print('PASS: compile-time automatic-arming guard',flush=True)
subprocess.run([sys.executable,str(root/'tests/test_configuration.py')],check=True)
