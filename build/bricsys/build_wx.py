#!/usr/bin/env python3

# wxWidgets is moving towards using wxUSE_STL=ON from 3.3 (see https://wxwidgets.org/blog/2023/04/separate-stl-build-is-no-more/). So then we should it too starting from now.

import os
import subprocess
import shutil
from pathlib import Path
import argparse
import time
import sys
import math
from enum import IntFlag

def run_command(command, cwd=None, env=None):
    """Run a shell command and handle errors."""
    print(f"Running command: {command} (in {cwd})", flush=True)
    result = subprocess.run(command, shell=True, cwd=cwd, env=env)
    result.check_returncode()

class Action(IntFlag):
    NONE = 0
    CHECKOUT = 1 << 0
    GENERATE = 1 << 1
    BUILD = 1 << 2
    ALL = CHECKOUT | GENERATE | BUILD

def initialize_and_update_submodules(sub_modules, cwd, env):
    """ Check if submodules are already initialized by checking if the submodule
        directories exist and have git content """
    needs_init = False

    for submodule in sub_modules:
        submodule_path = cwd / submodule
        if not submodule_path.exists() or not (submodule_path / '.git').exists():
            needs_init = True
            break

    if needs_init:
        print(f"Submodules not initialized. Running 'git submodule update --init ...'")
        command_text = 'git submodule update --init ' + " ".join(sub_modules)
        run_command(command_text, cwd=cwd, env=env)
    else:
        print("Submodules already initialized. Skipping git submodule update --init.")

def main():
    parser = argparse.ArgumentParser(description='Build Wx from source.')
    parser.add_argument(
        '--action',
        default='all',
        help='Comma-separated actions: checkout, generate, build, all'
    )
    parser.add_argument('--platform', required='True', help='Platform: windows, linux, mac')
    parser.add_argument('--arch', default='x64', help='Windows arch: x64 or Win32 (ignored on non-Windows)')
    parser.add_argument('--cmake_generator', help='The CMake Generator to use')
    parser.add_argument('--cmake_config_args', help='Extra cmake configure arguments')
    parser.add_argument('--build_type', default='all', help='Build type: all, release, debug, release_debug')
    parser.add_argument('--wx_src_dir', default='../../', help='Wx source directory (default: qt/src)')
    parser.add_argument('--wx_build_dir', help='Wx build directory (default: $wx_src_dir/build_bsys)')
    parser.add_argument('--wx_install_dir', help='Wx install directory (default: ../)')
    parser.add_argument('-j', '--jobs', type=int, default=None, help='Number of parallel jobs for building (default: 12 jobs)')
    args = parser.parse_args()

    # Apply conditional default
    if args.cmake_generator is None:
        if args.platform == 'windows':
            args.cmake_generator = f'"Visual Studio 17"'
        elif args.platform == 'linux':
            args.cmake_generator = f'"Ninja"'
        else:
            args.cmake_generator = f'"Unix Makefiles"'

    # Configurable Constants
    WXGIT_REPO_URL = 'git@github.com:Bricsys/wxWidgets.git'
    PLATFORM = args.platform # windows, linux, mac
    SUBMODULES = {'3rdparty/nanosvg', '3rdparty/pcre', 'src/jpeg', 'src/png', 'src/zlib', 'src/expat', 'src/tiff' }
    SKIP_MODULES = ''
    CMAKE_GENERATOR = args.cmake_generator # Adjust based on your platform and compiler
    CMAKE_SOURCE_PATH = 'cmake'

    # Paths
    SRC_DIR = Path(args.wx_src_dir).resolve()

    # Determine which build types to process
    # On Windows (multi-config): single build_bsys dir, both Debug and RelWithDebInfo install to install_bsys
    # On Linux/Mac (single-config): build_bsys for Release, build_bsys_debug for Debug
    if PLATFORM == 'windows':
        # Windows: RelWithDebInfo is the "release" config
        RELEASE_BUILD_TYPE = 'RelWithDebInfo'
    else:
        # Linux/Mac: Release is the "release" config
        RELEASE_BUILD_TYPE = 'Release'

    if args.build_type == 'all':
        build_types = [RELEASE_BUILD_TYPE, 'Debug']
    elif args.build_type == 'debug':
        build_types = ['Debug']
    elif args.build_type == 'release':
        build_types = [RELEASE_BUILD_TYPE]
    elif args.build_type == 'release_debug':
        build_types = ['RelWithDebInfo']
    else:
        print(f"Unknown build type: {args.build_type}")
        sys.exit(1)

    # Compute build/install directories for each build type
    def get_build_dirs(build_type):
        """Return (build_dir, install_dir, cwd) for a given build type."""
        # On Windows (multi-config), Debug and Release share a single build dir and install dir.
        # On Linux/Mac (single-config), Debug gets a separate _debug suffixed dir.
        if build_type == 'Debug' and PLATFORM != 'windows':
            dir_suffix = '_debug'
        else:
            dir_suffix = ''

        if args.wx_build_dir is not None:
            # User-specified build dir: append suffix for debug (non-Windows only)
            build_dir_str = args.wx_build_dir + dir_suffix
        else:
            build_dir_str = args.wx_src_dir + f'/build_bsys{dir_suffix}/'

        cwd = build_dir_str

        if args.wx_install_dir is not None:
            install_dir_str = args.wx_install_dir + dir_suffix
        else:
            if PLATFORM == 'linux':
                install_dir_str = f'{cwd}/install_bsys{dir_suffix}'
            else:
                install_dir_str = f'../../install_bsys{dir_suffix}'

        build_dir = Path(build_dir_str).resolve()
        install_path = Path(install_dir_str)
        install_dir = install_path if install_path.is_absolute() else (SRC_DIR / install_path).resolve()
        return build_dir, install_dir, cwd

    # Parse actions
    action_str = args.action.lower()
    ACTION = Action.NONE

    if action_str == 'all':
        ACTION = Action.ALL
    else:
        actions = action_str.split(',')
        for act in actions:
            act = act.strip()
            if act == 'checkout':
                ACTION |= Action.CHECKOUT
            elif act == 'generate':
                ACTION |= Action.GENERATE
            elif act == 'build':
                ACTION |= Action.BUILD
            else:
                print(f"Unknown action: {act}")
                sys.exit(1)

    # Prepare environment variables for subprocesses
    ENV = os.environ.copy()

    # Prepend cmake from thirdparty to PATH (mirrors build.sh / build.bat)
    thirdparty_path = ENV.get('THIRDPARTY_PATH', '')
    if thirdparty_path:
        if PLATFORM == 'windows':
            cmake_bin = os.path.join(thirdparty_path, 'cmake', 'win64', 'bin')
        elif PLATFORM == 'mac':
            cmake_bin = os.path.join(thirdparty_path, 'cmake', 'mac', 'bin')
        else:
            cmake_bin = os.path.join(thirdparty_path, 'cmake', 'lin64', 'bin')
        ENV['PATH'] = cmake_bin + os.pathsep + ENV.get('PATH', '')

    # Determine parallelism level
    if args.jobs is not None:
        jobs = args.jobs
    elif 'CMAKE_BUILD_PARALLEL_LEVEL' in os.environ:
        jobs = int(os.environ['CMAKE_BUILD_PARALLEL_LEVEL'])
    else:
        jobs = 12

    def make_configure_command(install_dir, build_type=None):
        """Build the cmake configure command. build_type is used for single-config generators."""
        cmd = (
            f'{CMAKE_SOURCE_PATH} '
            f'-G {CMAKE_GENERATOR} '
            f'--install-prefix="{install_dir}" '
            f'"{SRC_DIR}" '
            f'-C "{SRC_DIR}/build/bricsys/toolchain.cmake" '
            f'-DwxBUILD_SHARED=ON '
            f'-DwxUSE_STL=ON '
            f'-DwxUSE_XRC=ON '
            f'-DwxUSE_AUI=ON '
            f'-DwxUSE_STC=ON '
            f'-DwxUSE_REGEX=builtin '
            f'-DwxUSE_LIBPNG=builtin '
            f'-DwxUSE_LIBJPEG=builtin '
            f'-DwxUSE_GLCANVAS_EGL=OFF '
            f'-DwxUSE_WEBVIEW=OFF '
            f'-DwxUSE_RIBBON=OFF '
            f'-DwxUSE_LIBSDL=OFF '
            f'-DwxUSE_WEBREQUEST_CURL=OFF '
            f'-DwxUSE_WEBREQUEST=OFF '
            f'-DwxUSE_LIBNOTIFY=OFF '
            f'-DwxUSE_MEDIACTRL=OFF '
            f'-DwxBUILD_SAMPLES=OFF '
            f'-DwxBUILD_TESTS=OFF '
            f'-DwxBUILD_INSTALL=ON '
        )
        if PLATFORM == 'windows':
            cmd += f'-DCMAKE_GENERATOR_PLATFORM={args.arch} '
            cmd += f'-DwxUSE_LIBTIFF=builtin '
            cmd += f'-DCMAKE_CONFIGURATION_TYPES="Debug;RelWithDebInfo" '
        elif PLATFORM == 'mac':
            cmd += f'-DCMAKE_BUILD_TYPE={build_type} '
            cmd += f'-DCMAKE_INSTALL_LIBDIR=lib '
            cmd += f'-DCMAKE_OSX_ARCHITECTURES="arm64" '
            cmd += f'-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 '
            cmd += f'-DCMAKE_MACOSX_RPATH=OFF '
            cmd += f'-DCMAKE_BUILD_WITH_INSTALL_NAME_DIR=TRUE '
            cmd += f'-DCMAKE_INSTALL_NAME_DIR="@executable_path" '
        elif PLATFORM == 'linux':
            cmd += f'-DCMAKE_BUILD_TYPE={build_type} '
            cmd += f'-DCMAKE_INSTALL_LIBDIR=lib '
        if args.cmake_config_args is not None:
            cmd += args.cmake_config_args
        return cmd

    def print_config(build_types_info):
        print(f"==============================================")
        print(f"Running script with the following config:")
        print(f"ACTION: {args.action}")
        print(f"CMAKE GENERATOR: {CMAKE_GENERATOR}")
        print(f"PLATFORM: {PLATFORM}")
        if PLATFORM == 'windows':
            print(f"ARCH: {args.arch}")
        print(f"WX REPO URL: {WXGIT_REPO_URL}")
        print(f"SRC DIR: {SRC_DIR}")
        for label, build_dir, install_dir in build_types_info:
            print(f"BUILD TYPE: {label}")
            print(f"  BUILD DIR:   {build_dir}")
            print(f"  INSTALL DIR: {install_dir}")
        print(f"==============================================", flush=True)

    def iter_build_configs():
        """Yield (build_type, BUILD_DIR, INSTALL_DIR, CWD) for each build configuration.
        On Windows (multi-config), all build types share the same dirs.
        On Linux/Mac (single-config), each build type gets its own dirs."""
        if PLATFORM == 'windows':
            BUILD_DIR, INSTALL_DIR, CWD = get_build_dirs(RELEASE_BUILD_TYPE)
            for bt in build_types:
                yield bt, BUILD_DIR, INSTALL_DIR, CWD
        else:
            for bt in build_types:
                yield bt, *get_build_dirs(bt)

    def do_generate():
        seen_build_dirs = set()
        for bt, BUILD_DIR, INSTALL_DIR, CWD in iter_build_configs():
            if BUILD_DIR in seen_build_dirs:
                continue
            seen_build_dirs.add(BUILD_DIR)
            BUILD_DIR.mkdir(parents=True, exist_ok=True)
            run_command(make_configure_command(INSTALL_DIR, bt), cwd=CWD, env=ENV)

    def do_build():
        for bt, BUILD_DIR, INSTALL_DIR, CWD in iter_build_configs():
            run_command(f'cmake --build {BUILD_DIR} --config {bt} --parallel {jobs}', cwd=CWD, env=ENV)
            run_command(f'cmake --install {BUILD_DIR} --config {bt}', cwd=CWD, env=ENV)

    # Compute dirs for the summary printout
    build_types_info = [(bt, build_dir, install_dir) for bt, build_dir, install_dir, _ in iter_build_configs()]
    print_config(build_types_info)

    if Action.CHECKOUT in ACTION or Action.GENERATE in ACTION:
        initialize_and_update_submodules(SUBMODULES, SRC_DIR, ENV)

    if Action.GENERATE in ACTION:
        do_generate()

    if Action.BUILD in ACTION:
        do_build()

if __name__ == '__main__':
    start = time.time()
    main()
    interval = time.time() - start
    print("total deployment took", math.floor(interval / 60), "minutes and", math.floor(interval % 60), "seconds")

