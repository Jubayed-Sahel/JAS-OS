#!/usr/bin/env python3
"""Compile JAS OS with the bundled i686-elf toolchain (or PATH gcc)."""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
TOOLS = ROOT / "tools" / "bin"

SOURCES = [
    "start.S",
    "klib.c", "hw.c", "input.c", "gfx.c", "gui.c", "kernel.c",
    "task.c", "scheduler.c", "memory.c", "paging.c", "banker.c",
    "filesystem.c", "sync.c", "demos.c", "event_log.c", "storage.c", "lecture.c",
    "commands.c",
]


def find_tool(name: str) -> str:
    bundled = TOOLS / f"i686-elf-{name}.exe"
    if bundled.exists():
        return str(bundled)
    bundled = TOOLS / f"i686-elf-{name}"
    if bundled.exists():
        return str(bundled)
    for candidate in (f"i686-elf-{name}", name):
        found = shutil.which(candidate)
        if found:
            return found
    raise SystemExit(f"missing tool: {name}")


def run(cmd: list[str]) -> None:
    print("+", " ".join(cmd))
    subprocess.check_call(cmd)


def main() -> None:
    BUILD.mkdir(exist_ok=True)
    cc = find_tool("gcc")
    ld = find_tool("ld")
    objcopy = find_tool("objcopy")

    cflags = [
        "-ffreestanding", "-fno-pie", "-fno-pic", "-fno-stack-protector",
        "-mno-mmx", "-mno-sse", "-mno-sse2", "-Wall", "-Wextra", "-O2",
        "-fno-asynchronous-unwind-tables", "-I", str(ROOT / "kernel" / "include"),
    ]

    run([cc, "-c", "-fno-asynchronous-unwind-tables",
         str(ROOT / "boot" / "boot.S"), "-o", str(BUILD / "boot.o")])
    run([ld, "-Ttext", "0x7C00", "--oformat", "binary",
         "-o", str(BUILD / "boot.bin"), str(BUILD / "boot.o")])
    boot = (BUILD / "boot.bin").read_bytes()
    if len(boot) > 4096:
        raise SystemExit(f"bootloader too large: {len(boot)}")
    (BUILD / "boot.bin").write_bytes(boot + b"\x00" * (4096 - len(boot)))
    print("boot.bin", 4096, "bytes")

    objs = []
    for src in SOURCES:
        src_path = ROOT / "kernel" / "src" / src
        obj = BUILD / (Path(src).stem + ".o")
        cmd = [cc, *cflags, "-c", str(src_path), "-o", str(obj)]
        run(cmd)
        objs.append(str(obj))

    elf = BUILD / "kernel.elf"
    run([ld, "-T", str(ROOT / "linker.ld"), "-nostdlib", "-o", str(elf), *objs])
    kernel_bin = BUILD / "kernel.bin"
    run([objcopy, "-O", "binary", str(elf), str(kernel_bin)])
    print("kernel.bin", kernel_bin.stat().st_size, "bytes")

    iso_script = ROOT / "scripts" / "make_iso.py"
    iso = BUILD / "jas-os.iso"
    run([sys.executable, str(iso_script), str(BUILD / "boot.bin"), str(kernel_bin), str(iso)])
    print("ISO ready:", iso)


if __name__ == "__main__":
    os.chdir(ROOT)
    main()
