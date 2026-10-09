import subprocess
from pathlib import Path

SRC_DIR = Path("src/shaders")
OUT_DIR = Path("res/shaders")

for slang_file in SRC_DIR.rglob("*.slang"):
    rel_path = slang_file.relative_to(SRC_DIR)
    out_file = OUT_DIR / rel_path.with_suffix(".spv")

    out_file.parent.mkdir(parents=True, exist_ok=True)

    try:
        subprocess.run(["slangc", str(slang_file), "-target", "spirv", "-matrix-layout-column-major", "-emit-spirv-directly", "-g2", "-fvk-use-entrypoint-name", "-o", str(out_file)], check=True)
    except subprocess.CalledProcessError:
        pass
