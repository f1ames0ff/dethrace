import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

SHADERS = (
    ("world.vert", "rvk_world_vert_spv"),
    ("world.frag", "rvk_world_frag_spv"),
    ("overlay.vert", "rvk_overlay_vert_spv"),
    ("overlay.frag", "rvk_overlay_frag_spv"),
)

SPIRV_MAGIC = 0x07230203
GLSL_VULKAN_VERSION = "vulkan1.0"


def default_shader_dir():
    return Path(__file__).resolve().parent.parent / "src" / "render_vk" / "shaders"


def find_glslc(explicit):
    if explicit:
        located = shutil.which(explicit)
        if located:
            return Path(located)
        path = Path(explicit)
        if path.is_file():
            return path
        raise FileNotFoundError(f"glslc not found: {explicit}")

    vulkan_sdk = os.environ.get("VULKAN_SDK")
    if vulkan_sdk:
        candidates = (
            Path(vulkan_sdk) / "Bin" / "glslc.exe",
            Path(vulkan_sdk) / "bin" / "glslc",
        )
        for candidate in candidates:
            if candidate.is_file():
                return candidate

    located = shutil.which("glslc")
    if located:
        return Path(located)

    raise FileNotFoundError("glslc not found; pass --glslc or set VULKAN_SDK")


def compile_shader(glslc, source, output):
    command = [
        str(glslc),
        f"--target-env={GLSL_VULKAN_VERSION}",
        "-O",
        "-o",
        str(output),
        str(source),
    ]
    completed = subprocess.run(command, capture_output=True, text=True)
    if completed.returncode != 0:
        message = completed.stderr.strip() or completed.stdout.strip()
        raise RuntimeError(f"glslc failed for {source.name}: {message}")


def read_spirv(path):
    data = path.read_bytes()
    if not data or len(data) % 4 != 0:
        raise RuntimeError(f"invalid SPIR-V size for {path.name}: {len(data)}")
    words = struct.unpack(f"<{len(data) // 4}I", data)
    if words[0] != SPIRV_MAGIC:
        raise RuntimeError(f"invalid SPIR-V magic for {path.name}: 0x{words[0]:08x}")
    return words


def format_block(name, words):
    lines = [f"static const uint32_t {name}[] = {{"]
    for start in range(0, len(words), 8):
        chunk = ", ".join(f"0x{word:08x}" for word in words[start:start + 8])
        lines.append(f"    {chunk},")
    lines.append("};")
    lines.append("")
    lines.append(f"static const uint32_t {name}_size = {len(words) * 4};")
    return "\n".join(lines)


def build_header(compiled):
    sections = [
        "#ifndef RVK_SHADERS_H",
        "#define RVK_SHADERS_H",
        "",
        "#include <stdint.h>",
        "",
    ]
    for name, words in compiled:
        sections.append(format_block(name, words))
        sections.append("")
    sections.append("#endif")
    return "\n".join(sections) + "\n"


def main():
    parser = argparse.ArgumentParser(
        description="Compile the render_vk GLSL shaders into rvk_shaders.h."
    )
    parser.add_argument(
        "shader_dir",
        nargs="?",
        type=Path,
        default=default_shader_dir(),
        help="directory containing world.vert, world.frag, overlay.vert and overlay.frag",
    )
    parser.add_argument(
        "--glslc",
        default=None,
        help="path to glslc; discovered from VULKAN_SDK or PATH when omitted",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="output header path; defaults to <shader_dir>/../src/rvk_shaders.h",
    )
    args = parser.parse_args()

    shader_dir = args.shader_dir.resolve()
    if not shader_dir.is_dir():
        raise FileNotFoundError(f"shader directory not found: {shader_dir}")

    output = args.output
    if output is None:
        output = shader_dir.parent / "src" / "rvk_shaders.h"
    output = output.resolve()

    glslc = find_glslc(args.glslc)
    sources = []
    for filename, _ in SHADERS:
        source = shader_dir / filename
        if not source.is_file():
            raise FileNotFoundError(f"shader not found: {source}")
        sources.append(source)

    compiled = []
    with tempfile.TemporaryDirectory(prefix="rvk_shaders_") as temporary:
        temporary_dir = Path(temporary)
        for source, (_, name) in zip(sources, SHADERS):
            spirv_path = temporary_dir / (source.name + ".spv")
            compile_shader(glslc, source, spirv_path)
            compiled.append((name, read_spirv(spirv_path)))

    header = build_header(compiled)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary_output = output.with_name(output.name + ".tmp")
    with temporary_output.open("w", encoding="ascii", newline="\n") as handle:
        handle.write(header)
    temporary_output.replace(output)

    print(f"Wrote {output}")
    for name, words in compiled:
        print(f"{name}: {len(words) * 4} bytes ({len(words)} words)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
