import subprocess
from pathlib import Path

SRC_DIR = Path("src/shaders")
OUT_DIR = Path("res/shaders")

for slang_file in SRC_DIR.rglob("*.slang"):
    rel_path = slang_file.relative_to(SRC_DIR)
    out_file = OUT_DIR / rel_path.with_suffix(".spv")

    out_file.parent.mkdir(parents=True, exist_ok=True)

    subprocess.run(["slangc", str(slang_file), "-target", "spirv", "-profile", "spirv_1_4", "-emit-spirv-directly", "-fvk-use-entrypoint-name", "-o", str(out_file)], check=True)
