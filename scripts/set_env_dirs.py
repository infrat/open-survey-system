"""
PlatformIO pre-build extra script for monorepo root build.

PlatformIO 6.x does not support src_dir / include_dir / data_dir in [env:]
sections — only in [platformio].  This script adds per-environment include
and data paths that cannot be expressed in platformio.ini.

Framework-bundled libraries not auto-detected by the LDF (LittleFS, Wire, WiFi
...) are declared explicitly in each env's lib_deps instead.
"""
Import("env")  # noqa: F821 — injected by PlatformIO SCons runtime
import os

PROJECT_DIR = env["PROJECT_DIR"]
PIOENV = env["PIOENV"]

ENV_CONFIG = {
    "oss-transmitter": {
        "include": "firmware/transmitter/include",
        "data":    "firmware/transmitter/webui/dist-esp32",
    },
    "oss-receiver": {
        "include": "firmware/receiver/include",
    },
    "oss-rover": {
        "include": "firmware/rover/include",
    },
}

cfg = ENV_CONFIG.get(PIOENV, {})

if "include" in cfg:
    inc_path = os.path.join(PROJECT_DIR, cfg["include"])
    env.Replace(PROJECTINCLUDE_DIR=inc_path)
    env.Append(CPPPATH=[inc_path])
    print(f"[set_env_dirs] CPPPATH += {inc_path}")

if "data" in cfg:
    data_path = os.path.join(PROJECT_DIR, cfg["data"])
    env.Replace(PROJECTDATA_DIR=data_path)
    print(f"[set_env_dirs] PROJECTDATA_DIR = {data_path}")

# In a monorepo root build, WiFi ends up as a top-level dependency instead of
# a sub-dependency of WebServer.  As a result, WiFi/src is NOT added to
# WebServer's compilation environment, causing:
#   WebServer.cpp: fatal error: WiFiServer.h: No such file or directory
# Same applies to FS (WebServer also includes FS.h).
# Fix: inject these paths into the project CPPPATH *before* SCons clones
# per-library envs, so WebServer (and every other library) sees them.
try:
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    if framework_dir:
        for lib in ("WiFi", "FS"):
            lib_src = os.path.join(framework_dir, "libraries", lib, "src")
            if os.path.isdir(lib_src):
                env.Append(CPPPATH=[lib_src])
                print(f"[set_env_dirs] CPPPATH += {lib_src}  (WebServer compat)")
except Exception as exc:
    print(f"[set_env_dirs] Warning: could not inject framework lib paths: {exc}")
