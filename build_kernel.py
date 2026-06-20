import argparse
import subprocess
import os
import shutil
import re
import time
from datetime import datetime
import zipfile
import multiprocessing

debug_popen_impl = False

def popen_impl(command: list[str], env=None):
    if debug_popen_impl:
        print('Execute command: "%s"...' % ' '.join(command), end=' ')
    s = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    out, err = s.communicate()
    def write_logs(out, err):
        out = out.decode("utf-8")
        err = err.decode("utf-8")
        stdout_log = str(s.pid) + "_stdout.log"
        stderr_log = str(s.pid) + "_stderr.log"
        with open(stdout_log, "w") as f:
            f.write(out)
        with open(stderr_log, "w") as f:
            f.write(err)
        print(f"Output log files: {stdout_log}, {stderr_log}")

    if s.returncode != 0:
        if debug_popen_impl:
            print('failed')
        write_logs(out, err)
        raise RuntimeError(f"Command failed: {command}. Exitcode: {s.returncode}")
    if debug_popen_impl:
        print(f'result: {s.returncode == 0}')
        write_logs(out, err)

def check_file(filename):
    print(f"Checking file if exists: {filename}...", end=' ')
    if not os.path.exists(filename):
        print("Not found")
    else:
        print("Found")
    return os.path.exists(filename)

def match_and_get(regex: str, pattern: str):
    matched = re.search(regex, pattern)
    if not matched:
        raise AssertionError('Failed to match: for pattern: %s regex: %s' % pattern, regex)
    return matched.group(1)

def print_dictinfo(info: dict[str, str]):
    print('================================')
    for k, v in info.items():
        print(f"{k}={v}")
    print('================================')

def zip_files(zipfilename: str, files: list[str]):
    print(f"Zipping {len(files)} files to {zipfilename}...")
    with zipfile.ZipFile(zipfilename, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for f in files:
            zf.write(f)
    print("OK")

class CompilerClang:
    @staticmethod
    def test_executable():
        try:
            popen_impl(['./toolchain/bin/clang', '-v'])
        except RuntimeError as e:
            print("Failed to execute clang, something went wrong")
            raise e

    @staticmethod
    def get_version():
        clangversionRegex = r"(.*?clang version \d+(\.\d+)*).*"
        s = subprocess.Popen(['./toolchain/bin/clang', '-v'], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
        _, tcversion = s.communicate()
        tcversion = tcversion.decode('utf-8')
        return match_and_get(clangversionRegex, tcversion)

def get_cpu_count():
    """Get optimal thread count for building"""
    try:
        return max(2, multiprocessing.cpu_count())
    except:
        return 4

def main():
    parser = argparse.ArgumentParser(description="Build Raiden Kernel with specified arguments")

    parser.add_argument('--allow-dirty', action='store_true', help="Allow dirty build")
    parser.add_argument('--thin', action='store_true', help="Use ThinLTO for build")
    parser.add_argument('-j', '--jobs', type=int, default=0,
                        help="Number of parallel jobs (default: auto-detect)")
    parser.add_argument('--no-lto', action='store_true', help="Disable LTO for faster builds")
    parser.add_argument('--debug', action='store_true', help="Enable debug logging")

    # Parse the arguments
    args = parser.parse_args()

    global debug_popen_impl
    debug_popen_impl = args.debug

    # Determine parallelism
    jobs = args.jobs if args.jobs > 0 else get_cpu_count()

    # Check files
    if not check_file("AnyKernel3/anykernel.sh"):
        popen_impl(['git', 'submodule', 'update', '--init'])
    if not check_file("toolchain"):
        print(f"Please make toolchain available at {os.getcwd()}")
        return

    CompilerClang.test_executable()

    # Print build info
    print_dictinfo({
        'TARGET_KERNEL': 'Raiden',
        'TARGET_DEVICE': 'Samsung Galaxy A90 5G (SM-A908N/R3Q)',
        'TARGET_SOC': 'Snapdragon 855 (SM8150)',
        'TARGET_USES_LLVM': 'True',
        'BUILD_THREADS': str(jobs),
        'TOOLCHAIN': CompilerClang.get_version(),
        'BUILD_TYPE': 'ThinLTO' if args.thin else ('No-LTO' if args.no_lto else 'Full LTO'),
    })

    # Add toolchain in PATH environment variable
    tcPath = os.path.join(os.getcwd(), 'toolchain', 'bin')
    if tcPath not in os.environ['PATH'].split(os.pathsep):
        os.environ["PATH"] = tcPath + ':' + os.environ["PATH"]

    # Set build environment
    build_env = os.environ.copy()
    build_env["ARCH"] = "arm64"
    build_env["CROSS_COMPILE"] = "aarch64-linux-gnu-"
    build_env["LLVM"] = "1"

    # Additional KCFLAGS for optimization
    if not args.no_lto:
        build_env["KCFLAGS"] = "-O2 -fno-semantic-interposition"

    outDir = 'out'
    if os.path.exists(outDir) and not args.allow_dirty:
        print('Make clean...')
        shutil.rmtree(outDir)

    make_defconfig = []
    make_common = ['make', 'O=out', 'LLVM=1', 'ARCH=arm64',
                   'CROSS_COMPILE=aarch64-linux-gnu-', f'-j{jobs}']
    
    if args.no_lto:
        make_common.append('LTO=n')
        make_common.append('LLVM_IAS=0')
    
    make_defconfig += make_common
    make_defconfig += ['r3q_defconfig']
    if args.thin:
        make_defconfig += ['thinlto.config']

    t = datetime.now()
    print(f'\n[1/2] Building defconfig...')
    popen_impl(make_defconfig, env=build_env)
    print(f'\n[2/2] Building kernel (using {jobs} threads)...')
    popen_impl(make_common, env=build_env)
    print('\nBuild completed successfully!')
    t = datetime.now() - t

    with open(os.path.join(outDir, 'include', 'generated', 'utsrelease.h')) as f:
        kver = match_and_get(r'"([^"]+)"', f.read())

    shutil.copyfile('out/arch/arm64/boot/Image', 'AnyKernel3/Image')
    zipname = 'RaidenKernel_r3q_{}.zip'.format(datetime.today().strftime('%Y-%m-%d'))
    os.chdir('AnyKernel3/')
    zip_files(zipname, [
        'Image', 
        'META-INF/com/google/android/update-binary',
        'META-INF/com/google/android/updater-script',
        'tools/ak3-core.sh',
        'tools/busybox',
        'tools/magiskboot',
        'anykernel.sh'])
    newZipName = os.path.join(os.getcwd(), '..', zipname)
    try:
        os.remove(newZipName)
    except:
        pass
    shutil.move(zipname, newZipName)
    os.chdir('..')
    
    minutes = int(t.total_seconds() // 60)
    seconds = int(t.total_seconds() % 60)
    print_dictinfo({
        'OUT_ZIPNAME': zipname,
        'KERNEL_VERSION': kver,
        'BUILD_TIME': f'{minutes}m {seconds}s',
    })
    print(f"\nKernel package ready: {newZipName}")

if __name__ == '__main__':
    main()
