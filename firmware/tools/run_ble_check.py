from pathlib import Path
import subprocess
base=Path(__file__).resolve().parents[1]
lib=base/'freeink-sdk/libs/network/BleKeyboardHost'
test=lib/'test/host'
exe=base/'test-results/host-build/ble-check'
cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-variable','-pthread',f'-I{test}/stubs',f'-I{lib}/include',f'-I{lib}/src']
cmd += [str(test/'test_ble_keyboard_host.cpp'),str(test/'fake_ble.cpp'),str(lib/'src/BleKeyboardHost.cpp'),str(lib/'src/HidKeymap.cpp'),'-o',str(exe)]
subprocess.run(cmd,check=True)
result=subprocess.run([str(exe)],capture_output=True,text=True)
(base/'test-results/ble-check.txt').write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr);result.check_returncode()
