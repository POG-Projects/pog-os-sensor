Import("env")

import os
import re
from pathlib import Path

project_dir = Path(env["PROJECT_DIR"])
version = os.environ.get("POGSENSOR_BUILD_VERSION")
if version is None:
    version = (project_dir / "version.txt").read_text(encoding="utf-8").strip()

if not re.fullmatch(r"\d+\.\d+\.\d+", version):
    raise ValueError(f"Invalid POG Sensor firmware version: {version!r}")

print(f"POG Sensor firmware version: {version}")
env.Append(CPPDEFINES=[("POGSENSOR_FW_VERSION", env.StringifyMacro(version))])
