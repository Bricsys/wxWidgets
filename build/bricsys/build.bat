@echo off
SET PATH=%PATH%;%THIRDPARTY_PATH%\cmake\win64\bin

:: Default build type
set BUILD_TYPE=release
set ACTION_TYPE=build

:: Parse command line arguments
:parse_args
if not "%~1" == "" set ACTION_TYPE=%~1
if not "%~2" == "" set BUILD_TYPE=%~2

:: Map "release" to "release_debug" to use RelWithDebInfo
set PYTHON_BUILD_TYPE=%BUILD_TYPE%
if "%BUILD_TYPE%" == "release" set PYTHON_BUILD_TYPE=release_debug

if "%ACTION_TYPE%" == "generate" (
    python build/bricsys/build_wx.py --wx_src_dir=. --wx_build_dir=./build_bsys --wx_install_dir=./install_bsys --platform=windows --arch=x64 --toolset=v143 --action=generate --cmake_config_args="-DCMAKE_SYSTEM_VERSION=10.0.19041.0"
)

if "%ACTION_TYPE%" == "build" (
    python build/bricsys/build_wx.py --wx_src_dir=. --wx_build_dir=./build_bsys --wx_install_dir=./install_bsys --platform=windows --action=build --build_type=%PYTHON_BUILD_TYPE%
)

