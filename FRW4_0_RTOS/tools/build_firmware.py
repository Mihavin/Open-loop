"""Clean, out-of-tree firmware build; no stale Debug objects or absolute IDE paths.

python tools/build_firmware.py --toolchain <directory containing arm-none-eabi-gcc>
"""
from pathlib import Path
import argparse
import concurrent.futures
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'build-validation')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if sys.platform == 'win32' else ''
    cc = args.toolchain / ('arm-none-eabi-gcc' + suffix)
    cxx = args.toolchain / ('arm-none-eabi-g++' + suffix)
    includes = [ROOT / p for p in [
        'Core/Inc', 'Drivers/STM32H7xx_HAL_Driver/Inc', 'Drivers/STM32H7xx_HAL_Driver/Inc/Legacy',
        'Drivers/CMSIS/Device/ST/STM32H7xx/Include', 'Drivers/CMSIS/Include', 'Drivers/CMSIS/RTOS2/Include',
        'Middlewares/Third_Party/FreeRTOS/Source/include',
        'Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2',
        'Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F',
        'Middlewares/ST/touchgfx/framework/include', 'TouchGFX/App', 'TouchGFX/target',
        'TouchGFX/target/generated', 'TouchGFX/gui/include',
        *['TouchGFX/generated/' + p + '/include' for p in ['fonts','gui_generated','images','texts','videos']],
    ]]
    common = ['-mcpu=cortex-m7', '-mthumb', '-mfpu=fpv5-d16', '-mfloat-abi=hard',
              '-DDEBUG', '-DUSE_PWR_LDO_SUPPLY', '-DUSE_HAL_DRIVER', '-DSTM32H743xx',
              '-O0', '-g3', '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '--specs=nano.specs']
    common += [arg for p in includes for arg in ['-I', str(p)]]
    sources = []
    # The IDE production manifest excludes the simulator and its nested build outputs.
    for line in (ROOT / 'Debug/objects.list').read_text().splitlines():
        base = ROOT / line.strip().strip('"').removeprefix('./')
        choices = [base.with_suffix(ext) for ext in ['.c', '.cpp', '.s', '.S']]
        source = next((p for p in choices if p.exists()), None)
        if source is None: raise RuntimeError('Source missing for ' + str(base))
        sources.append(source)
    def compile_one(source):
        relative = source.relative_to(ROOT)
        obj = out / relative.with_suffix('.o')
        obj.parent.mkdir(parents=True, exist_ok=True)
        cpp = source.suffix == '.cpp'
        flags = ['-std=gnu++14', '-fno-exceptions', '-fno-rtti', '-fno-use-cxa-atexit'] if cpp else ['-std=gnu11']
        command = [str(cxx if cpp else cc), *common, *flags, '-c', str(source), '-o', str(obj)]
        result = subprocess.run(command, capture_output=True, text=True)
        return {'source': str(relative), 'exit': result.returncode,
                'diagnostics': result.stdout + result.stderr, 'object': str(obj)}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(compile_one, sources))
    failures = [r for r in results if r['exit']]
    for r in failures:
        print(r['source'], r['diagnostics'], flush=True)
    summary = {'compiler': subprocess.run([str(cc), '--version'], capture_output=True, text=True).stdout.splitlines()[0],
               'translation_units': len(results), 'compile_failures': len(failures), 'results': results}
    if not failures:
        response = out / 'objects.rsp'
        response.write_text('\n'.join('"' + r['object'].replace('\\','/') + '"' for r in results))
        library = ROOT / 'Middlewares/ST/touchgfx/lib/core/cortex_m7/gcc/libtouchgfx-float-abi-hard.a'
        command = [str(cxx), '@' + str(response), '-mcpu=cortex-m7', '-mthumb', '-mfpu=fpv5-d16',
                   '-mfloat-abi=hard', '-T' + str(ROOT / 'STM32H743IITX_FLASH.ld'), '--specs=nosys.specs',
                   '--specs=nano.specs', '-Wl,--gc-sections', '-Wl,-Map=' + str(out / 'firmware.map'),
                   '-o', str(out / 'firmware.elf'), '-Wl,--start-group', str(library), '-lc', '-lm',
                   '-lstdc++', '-lsupc++', '-Wl,--end-group']
        result = subprocess.run(command, capture_output=True, text=True)
        summary['link_exit'] = result.returncode
        summary['link_diagnostics'] = result.stdout + result.stderr
        print(summary['link_diagnostics'])
        if not result.returncode:
            size = args.toolchain / ('arm-none-eabi-size' + suffix)
            summary['size'] = subprocess.run([str(size), str(out / 'firmware.elf')], capture_output=True, text=True).stdout
            print(summary['size'])
    (out / 'build-results.json').write_text(json.dumps(summary, indent=2))
    print(f"Compiled {len(results)} translation units; failures: {len(failures)}; link: {summary.get('link_exit', 'not attempted')}")
    return 1 if failures or summary.get('link_exit', 1) else 0

if __name__ == '__main__':
    sys.exit(main())
