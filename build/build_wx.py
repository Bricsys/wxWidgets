#!/usr/bin/env python3

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

def main():
    parser = argparse.ArgumentParser(description='Build Qt from source.')
    parser.add_argument(
        '--action',
        default='all',
        help='Comma-separated actions: checkout, generate, build, all'
    )
    parser.add_argument('--platform', required='True', help='Platform: windows, linux, mac')
    parser.add_argument('--cmake_generator', help='The CMake Generator to use')
    parser.add_argument('--build_type', default='debug', help='Build type: release, debug, release_de    bug')
    parser.add_argument('--wx_src_dir', default='../../', help='Qt source directory (default: qt/src)')
    parser.add_argument('--wx_build_dir', help='Qt build directory (default: build/bricsys-{PLATFORM})')
    parser.add_argument('--wx_install_dir', default='../../', help='Wx install directory (default: ../)')
    args = parser.parse_args()

    # Apply conditional default
    if args.cmake_generator is None:
        if args.platform == 'windows':
            args.cmake_generator = f'"Visual Studio 17"'
    if args.wx_build_dir is None:
        args.wx_build_dir = args.wx_src_dir + f'/build/bricsys-{args.platform}'

    # Configurable Constants
    WXGIT_REPO_URL = 'git@github.com:Bricsys/wxWidgets.git'
    SUBMODULES = ''
    SKIP_MODULES = ''
    PLATFORM = args.platform # windows, linux, mac
    CMAKE_GENERATOR =  args.cmake_generator # Adjust based on your platform and compiler

   
    # Build type
    if args.build_type == "debug":
        BUILD_TYPE = 'Debug'
    elif args.build_type == "release":
        BUILD_TYPE = 'Release'
    else:
        print(f"Unknown build type: {args.build_type}")
        sys.exit(1)

    CMAKE_SOURCE_PATH = 'cmake'

    # Paths
    SRC_DIR = Path(args.wx_src_dir).resolve()
    BUILD_DIR = Path(args.wx_build_dir).resolve()
    INSTALL_DIR = Path(args.wx_install_dir).resolve()

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

    print(f"==============================================")
    print(f"Running script with the following config:")
    #print(f"WX VERSION: {QT_VERSION}")
    print(f"ACTION: {args.action}")
    print(f"CMAKE GENERATOR: {CMAKE_GENERATOR}")
    print(f"PLATFORM: {PLATFORM}")
    print(f"BUILD TYPE: {BUILD_TYPE}")
    print(f"WX REPO URL: {WXGIT_REPO_URL}")
    print(f"SRC DIR: {SRC_DIR}")
    print(f"BUILD DIR: {BUILD_DIR}")
    print(f"INSTALL DIR: {INSTALL_DIR}")
    print(f"==============================================", flush=True)

    # Prepare environment variables for subprocesses
    env = os.environ.copy()

    # Configure the build
    configure_command = (
        f'{CMAKE_SOURCE_PATH} '
        f'-G {CMAKE_GENERATOR} '
        f'--install-prefix="{INSTALL_DIR}" '
        f'"{SRC_DIR}" '
        f'-DCMAKE_TOOLCHAIN_FILE="{SRC_DIR}/build/bricsys/toolchain.cmake" '
        f'-DCMAKE_BUILD_TYPE={BUILD_TYPE} '
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
        f'-DwxBUILD_USE_PRIVATE_HEADERS=ON '
        f'-DwxBUILD_SAMPLES=OFF ' 
        f'-DwxBUILD_TESTS=OFF '
        f'-DwxBUILD_INSTALL=ON '
    )

    if Action.GENERATE in ACTION:
        run_command(configure_command, cwd=os.getcwd(), env=env)

    if Action.BUILD in ACTION:
        run_command(f'cmake --build {BUILD_DIR}', cwd=os.getcwd(), env=env)
        run_command(f'cmake --install {os.getcwd()} --config {BUILD_TYPE}', cwd=os.getcwd(), env=env)

if __name__ == '__main__':
    start = time.time()
    main()
    interval = time.time() - start
    print("total deployment took", math.floor(interval / 60), "minutes and", math.floor(interval % 60), "seconds")

