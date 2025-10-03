import subprocess
import sys
import os

def run_platformio_command(command):
    result = subprocess.run(command, shell=True)
    if result.returncode != 0:
        print(f"Command failed: {' '.join(command)}")
        sys.exit(result.returncode)

def main():
    # Ensure main.cpp exists
    cpp_path = os.path.join(os.getcwd(), "main.cpp")
    if not os.path.isfile(cpp_path):
        print("main.cpp not found in current directory.")
        sys.exit(1)

    # Compile (build) the project
    print("Compiling main.cpp using PlatformIO...")
    run_platformio_command("pio run")

    # Upload the firmware
    #print("Uploading firmware using PlatformIO...")
    #run_platformio_command("platformio run --target upload")

if __name__ == "__main__":
    main()