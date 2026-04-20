# scripts/merge_firmware.py
#
# Registers a PlatformIO custom target `build_merged` that produces a single
# merged-flash.bin containing bootloader + partitions + boot_app0 + firmware,
# ready to flash to a blank ESP32 in one shot.
#
# Invoke with: pio run -e <env> -t build_merged
#
# Result lands at: .pio/build/<env>/merged-flash.bin

Import("env")
import os

def merge_bin_action(target, source, env):
    build_dir       = env.subst("$BUILD_DIR")
    project_dir     = env.subst("$PROJECT_DIR")
    flash_size      = env.BoardConfig().get("upload.flash_size", "4MB")

    # Component binaries produced by a normal `pio run` build
    bootloader      = os.path.join(build_dir, "bootloader.bin")
    partitions      = os.path.join(build_dir, "partitions.bin")
    boot_app0       = os.path.join(env.PioPlatform().get_package_dir(
                                   "framework-arduinoespressif32") or "",
                                   "tools", "partitions", "boot_app0.bin")
    firmware        = os.path.join(build_dir, "${PROGNAME}.bin")
    firmware        = env.subst(firmware)
    merged          = os.path.join(build_dir, "merged-flash.bin")

    # Sanity-check each input exists before handing them to esptool.
    for f in (bootloader, partitions, boot_app0, firmware):
        if not os.path.isfile(f):
            print("[build_merged] missing input: %s" % f)
            env.Exit(1)

    # Standard ESP32 flash offsets for the Arduino-ESP32 layout.
    cmd = " ".join([
        "$PYTHONEXE", "-m", "esptool",
        "--chip", "esp32",
        "merge_bin",
        "-o", merged,
        "--flash_mode", "dio",
        "--flash_freq", "40m",
        "--flash_size", flash_size,
        "0x1000",  bootloader,
        "0x8000",  partitions,
        "0xe000",  boot_app0,
        "0x10000", firmware,
    ])
    env.Execute(cmd)
    print("[build_merged] produced %s" % merged)

env.AddCustomTarget(
    name="build_merged",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=[merge_bin_action],
    title="Build merged flash image",
    description="Creates merged-flash.bin (bootloader + partitions + app)"
)
