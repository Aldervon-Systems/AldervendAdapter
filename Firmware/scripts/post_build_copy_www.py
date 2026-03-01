# Copy built .bin to www/firmware.bin after build
Import("env")
import os
import shutil

def copy_firmware_to_www(source, target, env):
    src = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    dest = os.path.join(env.subst("$PROJECT_DIR"), "www", "firmware.bin")
    if os.path.isfile(src):
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copy2(src, dest)

if not env.IsIntegrationDump():
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware_to_www)
