"""
Pre-build script: rename lv_blend_helium.S so PlatformIO's assembler skips it.
The file is ARM Helium (MVE) specific and cannot be assembled on Xtensa.
"""
Import("env")
import os, glob

pattern = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"),
                       "**", "lv_blend_helium.S")
for path in glob.glob(pattern, recursive=True):
    disabled = path + ".disabled"
    if not os.path.exists(disabled):
        os.rename(path, disabled)
        print(f"[exclude_helium] renamed {path}")
