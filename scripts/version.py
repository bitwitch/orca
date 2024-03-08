import os
import subprocess
import sys

from .log import *
from .utils import pushd


def src_dir():
    # Fragile path adjustments! Yay!
    return os.path.normpath(os.path.join(os.path.abspath(__file__), "../../src"))


def dev_version():
    with pushd(src_dir()):
        try:
            res = subprocess.run(["git", "rev-parse", "--short", "HEAD"], check=True, capture_output=True, text=True)
            version = res.stdout.strip()
            return f"dev-{version}"
        except subprocess.CalledProcessError:
            log_warning("failed to look up current git hash for version number")
            return "dev-unknown"

def current_sdk_version():
    try:
        res = subprocess.run(["orca", "sdk-path"], check=True, capture_output=True, text=True)
        sdk_path = res.stdout.strip()
        return os.path.basename(sdk_path)
    except subprocess.CalledProcessError:
        print("You must install the Orca cli tool and add the directory where you")
        print("installed it to your PATH before the dev tooling can determine the")
        print("current Orca version. You can download the cli tool from:")
        print("https://github.com/orca-app/orca/releases/latest")
        exit(1)
