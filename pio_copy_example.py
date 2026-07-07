# PlatformIO can't set src_dir per-environment in a single platformio.ini, so each
# env's pre-build step copies that example's .ino into build_src/main.cpp before
# compiling - env name matches the examples/<name>/<name>.ino folder/file name.
#
# Deliberately NOT named "src": this repo is also self-referenced as a library via
# `lib_deps = symlink://.` (see platformio.ini), and Arduino's library spec treats any
# library containing a "src/" folder as "1.5-style" (headers must live under src/) -
# since PoweredUp.h lives at the repo root instead, a literal "src/" folder here would
# make the symlinked library stop finding its own header.
Import("env")

import os
import shutil

pioenv = env["PIOENV"]
source = os.path.join("examples", pioenv, pioenv + ".ino")
dest = os.path.join("build_src", "main.cpp")
os.makedirs("build_src", exist_ok=True)

if os.path.isfile(source):
    shutil.copyfile(source, dest)
else:
    print("pio_copy_example.py: no examples/%s/%s.ino found for env '%s'" % (pioenv, pioenv, pioenv))
