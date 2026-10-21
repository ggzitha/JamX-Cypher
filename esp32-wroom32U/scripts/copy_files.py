Import("env")
import os
import shutil

def copy_all_binaries(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    env_name = env.subst("$PIOENV")

    target_folder = os.path.join(project_dir, "bin", env_name)
    os.makedirs(target_folder, exist_ok=True)

    files_to_copy = [
        "firmware.bin",
        "partitions.bin",
        "bootloader.bin"
    ]

    for filename in files_to_copy:
        src_path = os.path.join(build_dir, filename)
        if os.path.exists(src_path):
            dst_path = os.path.join(target_folder, filename)
            shutil.copy(src_path, dst_path)
            print(f"Copied {filename} -> {dst_path}")
        else:
            print(f"Warning: {filename} not found in {build_dir}")

    # Copy boot_app0.bin (OTA initialisation image required at offset 0xe000)
    try:
        pkg_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
        boot_app0_src = os.path.join(pkg_dir, "tools", "partitions", "boot_app0.bin")
        if os.path.exists(boot_app0_src):
            dst_path = os.path.join(target_folder, "boot_app0.bin")
            shutil.copy(boot_app0_src, dst_path)
            print(f"Copied boot_app0.bin -> {dst_path}")
    except Exception as e:
        print(f"Note: boot_app0 copy skipped: {e}")

    # Generate single merged binary for easy 1-file flashing at offset 0x0
    try:
        bootloader_path = os.path.join(target_folder, "bootloader.bin")
        partitions_path = os.path.join(target_folder, "partitions.bin")
        boot_app0_path  = os.path.join(target_folder, "boot_app0.bin")
        firmware_path   = os.path.join(target_folder, "firmware.bin")
        merged_path     = os.path.join(target_folder, "firmware_merged.bin")

        if os.path.exists(bootloader_path) and os.path.exists(partitions_path) and os.path.exists(boot_app0_path) and os.path.exists(firmware_path):
            py_exe = env.get("PYTHONEXE")
            esptool_pkg = env.PioPlatform().get_package_dir("tool-esptoolpy")
            esptool_script = os.path.join(esptool_pkg, "esptool.py")
            import subprocess
            cmd = [
                py_exe, esptool_script, "--chip", "esp32", "merge_bin",
                "-o", merged_path,
                "--flash_mode", "dio",
                "--flash_freq", "40m",
                "--flash_size", "4MB",
                "0x1000", bootloader_path,
                "0x8000", partitions_path,
                "0xe000", boot_app0_path,
                "0x10000", firmware_path
            ]
            res = subprocess.run(cmd, capture_output=True, text=True)
            if res.returncode == 0:
                print(f"Generated merged binary -> {merged_path} (flash at 0x0)")
            else:
                print(f"Warning: merge_bin failed: {res.stderr}")
    except Exception as e:
        print(f"Note: merge_bin skipped: {e}")

copy_target = env.Alias("copy_bin", "$BUILD_DIR/${PROGNAME}.bin", copy_all_binaries)
env.AlwaysBuild(copy_target)
env.Default(copy_target)