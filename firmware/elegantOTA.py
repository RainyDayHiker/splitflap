import os
import sys
import requests
from pathlib import Path

try:
    import requests
except ImportError:
    print("Installing requests library...")
    import subprocess

    subprocess.check_call([sys.executable, "-m", "pip", "install", "requests"])
    import requests

# Get environment variables from PlatformIO
Import("env")  # type: ignore

# OTA configuration
OTA_HOST = env.GetProjectOption("upload_port")  # type: ignore

for flag in env.GetProjectOption("upload_flags", []):  # type: ignore
    if flag.startswith("--username="):
        OTA_USERNAME = flag.split("=", 1)[1]
    elif flag.startswith("--password="):
        OTA_PASSWORD = flag.split("=", 1)[1]


def upload_ota(source, target, env):
    """Upload image via ElegantOTA"""

    # Uploading a firmware if target == upload and filesystem if target == uploadfs - error and return otherwise
    target = str(target[0])
    mode = "fs" if target == "uploadfs" else "firmware" if target == "upload" else None
    if mode is None:
        print(f"OTA Upload: Unknown target {target}")
        return "Upload failed"

    filepath = str(source[0])
    host = OTA_HOST

    if not host.startswith("http"):
        host = f"http://{host}"

    if not Path(filepath).exists():
        print(f"✗ Error: File not found: {filepath}")
        return "Upload failed"

    file_size = Path(filepath).stat().st_size
    filename = Path(filepath).name

    print(f"╔════════════════════════════════════════════════")
    print(f"║ ElegantOTA Upload")
    print(f"╠════════════════════════════════════════════════")
    print(f"║ Target:  {host}")
    print(f"║ File:    {filepath}")
    print(f"║ Size:    {file_size:,} bytes")
    print(f"║ Mode:    {mode}")
    if OTA_USERNAME:
        print(f"║ Auth:    {OTA_USERNAME}:***")
    print(f"╚════════════════════════════════════════════════")

    # Setup authentication if provided
    auth = None
    if OTA_USERNAME and OTA_PASSWORD:
        auth = (OTA_USERNAME, OTA_PASSWORD)

    try:
        # Step 1: Start OTA update
        start_url = f"{host}/ota/start?mode={mode}"
        print(f"\n[1/2] Starting OTA update ({mode})...\n", end="", flush=True)
        response = requests.get(start_url, timeout=10, auth=auth)
        if response.status_code != 200:
            print(f"   ....Failed!")
            print(f"✗ Start request failed with status code: {response.status_code}")
            print(f"Response: {response.text}")
            return "Upload failed"
        print("   ....OK")

        # Step 2: Upload file
        upload_url = f"{host}/ota/upload"
        print(f"[2/2] Uploading {file_size:,} bytes...\n", end="", flush=True)

        with open(filepath, "rb") as f:
            # ElegantOTA expects the file in the 'update' field
            files = {"update": (filename, f, "application/octet-stream")}
            # Increase timeout to 10 minutes for large files and disable connection pooling
            response = requests.post(
                upload_url,
                files=files,
                timeout=600,  # 10 minutes
                headers={"Connection": "close"},  # Close connection after upload
                auth=auth,  # Include authentication
            )

        print("   ....Done!")

        if response.status_code == 200:
            print("✓ Upload successful! Device will reboot...")
            return None
        else:
            print(f"✗ Upload failed with status code: {response.status_code}")
            print(f"Response: {response.text}")
            return "Upload failed"

    except requests.exceptions.RequestException as e:
        print(f"✗ Upload failed: {e}")
        return "Upload failed"


env_name = env.get("PIOENV")  # type: ignore
if "_OTA" in env_name:
    print(f"Configuring ElegantOTA upload for environment: {env_name}")
    # Hook into PlatformIO's upload process
    env.Replace(UPLOADCMD=upload_ota)  # type: ignore
