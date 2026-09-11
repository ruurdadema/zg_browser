#!/usr/bin/env python3 -u
"""Build (and package) ZG Browser on macOS, Windows and Linux.

Usage:
    python3 build.py                      # build for this machine
    python3 build.py --debug --run        # debug build, then launch it
    python3 build.py --package            # build + zip up a distributable
    python3 build.py --with-test-server   # also build zg_choir's tree_server

Only the standard library is required; CMake (>= 3.22) and a C++17 toolchain do
the actual work.  Dependencies (muscle, zg_choir, JUCE) are fetched by CPM
during the configure step.
"""

import argparse
import multiprocessing
import os
import platform
import re
import shutil
import subprocess
import sys
from enum import Enum
from pathlib import Path

app_target_name = 'zg_browser'
app_artefacts_dir = f'{app_target_name}_artefacts'
app_name = 'ZG Browser'

# Script location matters, cwd does not
script_path = Path(__file__).resolve()
script_dir = script_path.parent


# ---------------------------------------------------------------------------
#  A thin CMake wrapper
# ---------------------------------------------------------------------------

class Config(Enum):
    debug = 'Debug'
    release_with_debug_info = 'RelWithDebInfo'


class CMake:
    """Just enough of a CMake front-end to configure and build a project."""

    def __init__(self):
        self._path_to_build = Path('build')
        self._path_to_source = Path('.')
        self._build_config = Config.release_with_debug_info
        self._generator = None
        self._architecture = None
        self._parallel = multiprocessing.cpu_count()
        self._targets = []
        self._options = {}
        self._env = dict(os.environ)

    def path_to_build(self, path: Path):    self._path_to_build = Path(path)
    def path_to_source(self, path: Path):   self._path_to_source = Path(path)
    def build_config(self, config: Config): self._build_config = config
    def generator(self, generator: str):    self._generator = generator
    def architecture(self, architecture):   self._architecture = architecture
    def parallel(self, jobs: int):          self._parallel = jobs
    def target(self, target: str):          self._targets.append(target)
    def option(self, key: str, value):      self._options[key] = value
    def env(self, key: str, value: str):    self._env[key] = value

    @staticmethod
    def _run(command):
        print('> ' + ' '.join(str(part) for part in command), flush=True)
        return command

    def configure(self):
        command = ['cmake', '-S', str(self._path_to_source), '-B', str(self._path_to_build)]

        if self._generator:
            command += ['-G', self._generator]
        if self._architecture:
            command += ['-A', self._architecture]

        command.append(f'-DCMAKE_BUILD_TYPE={self._build_config.value}')
        command += [f'-D{key}={value}' for key, value in self._options.items()]

        subprocess.run(self._run(command), check=True, env=self._env)

    def build(self):
        command = ['cmake', '--build', str(self._path_to_build),
                   '--config', self._build_config.value,
                   '--parallel', str(self._parallel)]

        for target in self._targets:
            command += ['--target', target]

        subprocess.run(self._run(command), check=True, env=self._env)


def pick_generator():
    """Ninja where we have it; on Windows the Visual Studio default is less fussy."""
    if platform.system() == 'Windows':
        return None
    return 'Ninja' if shutil.which('ninja') else None


def project_version():
    """`git describe` if this is a tagged checkout, else the version in CMakeLists.txt."""
    try:
        described = subprocess.run(['git', 'describe', '--tags', '--match', 'v*', '--dirty'],
                                   cwd=script_dir, check=True, capture_output=True, text=True)
        return described.stdout.strip().lstrip('v')
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    match = re.search(r'project\s*\(\s*\w+\s+VERSION\s+([0-9][0-9.]*)', (script_dir / 'CMakeLists.txt').read_text())
    return match.group(1) if match else '0.0.0'


def common_options(args, cmake: CMake):
    cmake.path_to_source(script_dir)
    cmake.build_config(Config.debug if args.debug else Config.release_with_debug_info)
    cmake.parallel(multiprocessing.cpu_count())
    cmake.option('ZG_BROWSER_BUILD_TEST_SERVER', 'ON' if args.with_test_server else 'OFF')

    # Share CPM's downloads between build trees (the macOS universal build alone
    # configures twice), unless the caller already has a cache of their own.
    if 'CPM_SOURCE_CACHE' not in os.environ:
        cmake.env('CPM_SOURCE_CACHE', str(Path.home() / '.cache' / 'CPM'))


# ---------------------------------------------------------------------------
#  Per-platform builds
# ---------------------------------------------------------------------------

def build_macos_for_arch(args, path_to_build: Path, arch: str):
    """Configures and builds one architecture into (path_to_build)."""
    if args.skip_build:
        return path_to_build

    path_to_build.mkdir(parents=True, exist_ok=True)

    cmake = CMake()
    common_options(args, cmake)
    cmake.path_to_build(path_to_build)
    cmake.generator(pick_generator())
    cmake.option('CMAKE_OSX_ARCHITECTURES', arch)
    cmake.option('CMAKE_OSX_DEPLOYMENT_TARGET', args.macos_deployment_target)

    cmake.configure()
    cmake.build()

    return path_to_build


def lipo_app_bundle(x86_64_build: Path, arm64_build: Path, path_to_build: Path, config: Config, identity: str):
    """Merges two single-architecture .app bundles into one universal bundle."""
    relative_bundle = Path(app_artefacts_dir) / config.value / f'{app_name}.app'
    executable = Path('Contents') / 'MacOS' / app_name

    universal_bundle = path_to_build / relative_bundle
    if universal_bundle.exists():
        shutil.rmtree(universal_bundle)
    universal_bundle.parent.mkdir(parents=True, exist_ok=True)

    # Start from the arm64 bundle (resources are identical), then replace the binary.
    shutil.copytree(arm64_build / relative_bundle, universal_bundle, symlinks=True)

    subprocess.run(['lipo', '-create',
                    str(x86_64_build / relative_bundle / executable),
                    str(arm64_build / relative_bundle / executable),
                    '-output', str(universal_bundle / executable)], check=True)

    # lipo invalidates the code signature, and macOS refuses to launch an
    # arm64 binary without one -- so always re-sign (ad-hoc by default).
    subprocess.run(['codesign', '--force', '--sign', identity, str(universal_bundle)], check=True)

    subprocess.run(['lipo', '-info', str(universal_bundle / executable)], check=True)
    return universal_bundle


def build_macos(args):
    config = Config.debug if args.debug else Config.release_with_debug_info
    path_to_build = Path(args.path_to_build) / f'macos-{args.macos_arch}'

    if args.macos_arch != 'universal':
        build_macos_for_arch(args, path_to_build, args.macos_arch)
        return path_to_build / app_artefacts_dir / config.value / f'{app_name}.app'

    x86_64 = build_macos_for_arch(args, path_to_build / 'x86_64', 'x86_64')
    arm64 = build_macos_for_arch(args, path_to_build / 'arm64', 'arm64')

    if args.skip_build:
        return path_to_build / app_artefacts_dir / config.value / f'{app_name}.app'

    return lipo_app_bundle(x86_64, arm64, path_to_build, config, args.macos_signing_identity)


def build_windows(args, arch: str):
    config = Config.debug if args.debug else Config.release_with_debug_info
    path_to_build = Path(args.path_to_build) / f'windows-{arch}'

    if not args.skip_build:
        path_to_build.mkdir(parents=True, exist_ok=True)

        cmake = CMake()
        common_options(args, cmake)
        cmake.path_to_build(path_to_build)
        cmake.generator(pick_generator())
        cmake.architecture(arch)

        cmake.configure()
        cmake.build()

    return path_to_build / app_artefacts_dir / config.value / f'{app_name}.exe'


def build_linux(args, arch: str):
    config = Config.debug if args.debug else Config.release_with_debug_info
    path_to_build = Path(args.path_to_build) / f'linux-{arch}'

    if not args.skip_build:
        path_to_build.mkdir(parents=True, exist_ok=True)

        cmake = CMake()
        common_options(args, cmake)
        cmake.path_to_build(path_to_build)
        cmake.generator(pick_generator())

        cmake.configure()
        cmake.build()

    return path_to_build / app_artefacts_dir / config.value / app_name


# ---------------------------------------------------------------------------
#  Packaging
# ---------------------------------------------------------------------------

def package(args, artefact: Path, platform_tag: str):
    """Zips the built app into <path-to-build>/artefacts/. No signing, no upload."""
    path_to_artefacts = Path(args.path_to_build) / 'artefacts'
    path_to_artefacts.mkdir(parents=True, exist_ok=True)

    name = f'zg-browser-{project_version()}-{args.build_number}-{platform_tag}'
    zip_path = path_to_artefacts / f'{name}.zip'
    zip_path.unlink(missing_ok=True)

    if artefact.is_dir():
        # ditto keeps bundle symlinks and permissions intact; make_archive would not.
        subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', str(artefact), str(zip_path)],
                       check=True)
    else:
        staging = path_to_artefacts / name
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir()
        shutil.copy2(artefact, staging)
        shutil.make_archive(str(staging), 'zip', staging)
        shutil.rmtree(staging)

    print(f'Packaged {zip_path}')
    return zip_path


def run_app(artefact: Path):
    if platform.system() == 'Darwin':
        command = [str(artefact / 'Contents' / 'MacOS' / app_name)]
    else:
        command = [str(artefact)]

    print('> ' + ' '.join(command), flush=True)
    subprocess.run(command, check=False)


# ---------------------------------------------------------------------------

def host_arch():
    machine = platform.machine().lower()
    if machine in ('aarch64', 'arm64'):
        return 'arm64'
    if machine in ('x86_64', 'amd64'):
        return 'x64'
    raise Exception(f'Unsupported architecture: {platform.machine()}')


def build(args):
    system = platform.system()

    if system == 'Darwin':
        artefact = build_macos(args)
        platform_tag = f'macos-{args.macos_arch}'
    elif system == 'Windows':
        artefact = build_windows(args, args.windows_arch)
        platform_tag = f'windows-{args.windows_arch}'
    elif system == 'Linux':
        artefact = build_linux(args, host_arch())
        platform_tag = f'linux-{host_arch()}'
    else:
        raise Exception(f'Unsupported platform: {system}')

    print(f'Built {artefact}')

    if args.package:
        package(args, artefact, platform_tag)

    if args.run:
        run_app(artefact)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument("--debug",
                        help="Enable debug builds",
                        action='store_true')

    parser.add_argument("--path-to-build",
                        help="The folder to build the project in",
                        default="build")

    parser.add_argument("--build-number",
                        help="Specifies the build number (used in artefact names)",
                        default="0")

    parser.add_argument("--skip-build",
                        help="Skip configuring and building (package/run what is already there)",
                        action='store_true')

    parser.add_argument("--package",
                        help="Zip the built application into <path-to-build>/artefacts",
                        action='store_true')

    parser.add_argument("--run",
                        help="Launch the application after building",
                        action='store_true')

    parser.add_argument("--with-test-server",
                        help="Also build zg_choir's tree_server, to have something to browse",
                        action='store_true')

    if platform.system() == 'Darwin':
        parser.add_argument("--macos-arch",
                            help="Which architecture(s) to build (macOS only)",
                            choices=['universal', 'arm64', 'x86_64'],
                            default='universal')

        parser.add_argument("--macos-deployment-target",
                            help="Minimum supported macOS version (macOS only)",
                            default="11.0")  # Big Sur: the first release on Apple silicon

        parser.add_argument("--macos-signing-identity",
                            help="Identity to re-sign the universal bundle with; '-' means ad-hoc (macOS only)",
                            default="-")

    elif platform.system() == 'Windows':
        parser.add_argument("--windows-arch",
                            help="Which architecture to build (Windows only)",
                            choices=['x64', 'Win32', 'ARM64'],
                            default='x64')

    build(parser.parse_args())


if __name__ == '__main__':
    print(f'Invoke {script_path} as script. Script dir: {script_dir}')
    sys.exit(main())
