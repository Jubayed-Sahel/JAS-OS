#!/usr/bin/env python3
"""Build a BIOS-bootable El Torito ISO that contains a 1.44 MB floppy image."""
from __future__ import annotations

import os
import struct
import sys

SECTOR = 2048
FLOPPY_SIZE = 1474560  # 80 * 2 * 18 * 512


def pad(data: bytes, size: int) -> bytes:
    if len(data) > size:
        raise SystemExit(f"data is {len(data)} bytes, max {size}")
    return data + b"\x00" * (size - len(data))


def make_floppy(boot: bytes, kernel: bytes) -> bytes:
    boot = bytearray(pad(boot, 4096))
    if boot[510] != 0x55 or boot[511] != 0xAA:
        raise SystemExit("boot.bin is missing the 0x55 0xAA signature at offset 510")
    sectors = (len(kernel) + 511) // 512
    marker = bytes([0x08, 0x00, 0x00, 0x00, 0x00, 0x10])  # cur_lba, left, dest_seg
    idx = boot.find(marker)
    if idx < 0:
        raise SystemExit("could not find dest_seg/kernel_sectors in boot.bin")
    boot[idx + 6:idx + 8] = struct.pack("<H", max(sectors + 2, 16))
    image = bytearray(FLOPPY_SIZE)
    image[0:4096] = boot
    kernel_off = 4096
    if kernel_off + len(kernel) > FLOPPY_SIZE:
        raise SystemExit("kernel.bin is too large for a 1.44 MB floppy")
    image[kernel_off:kernel_off + len(kernel)] = kernel
    return bytes(image)


def both_endian32(value: int) -> bytes:
    return struct.pack("<I", value) + struct.pack(">I", value)


def both_endian16(value: int) -> bytes:
    return struct.pack("<H", value) + struct.pack(">H", value)


def dirname_record(name: bytes, lba: int, size: int, is_dir: bool) -> bytes:
    flags = 0x02 if is_dir else 0x00
    rec_len = 33 + len(name)
    if rec_len % 2:
        rec_len += 1
    rec = bytearray(rec_len)
    rec[0] = rec_len
    rec[2:10] = both_endian32(lba)
    rec[10:18] = both_endian32(size)
    rec[18:25] = bytes([126, 8, 19, 12, 0, 0, 0])
    rec[25] = flags
    rec[32] = len(name)
    rec[33:33 + len(name)] = name
    return bytes(rec)


def make_iso(floppy: bytes, out_path: str) -> None:
    catalog_lba = 19
    path_l_lba = 20
    path_m_lba = 21
    root_lba = 22
    readme_lba = 23
    floppy_lba = 24
    floppy_sectors = (len(floppy) + SECTOR - 1) // SECTOR
    readme_text = (
        b"JAS OS x86 lecture-concept kernel\n"
        b"Boot this ISO in VirtualBox/QEMU (BIOS, not EFI).\n"
        b"Set pointing device to PS/2 Mouse.\n"
    )
    readme = pad(readme_text, SECTOR)
    root_size = SECTOR
    total_sectors = floppy_lba + floppy_sectors
    date17 = b"2026081912000000\x00"
    assert len(date17) == 17

    pvd = bytearray(SECTOR)
    pvd[0] = 1
    pvd[1:6] = b"CD001"
    pvd[6] = 1
    pvd[8:40] = b" ".ljust(32)  # system id
    pvd[40:72] = b"JAS_OS_X86".ljust(32)
    pvd[80:88] = both_endian32(total_sectors)
    pvd[120:124] = both_endian16(1)
    pvd[124:128] = both_endian16(1)
    pvd[128:132] = both_endian16(SECTOR)
    path_table = bytearray(10)
    path_table[0] = 1
    path_table[2:6] = struct.pack("<I", root_lba)
    path_table[6:8] = struct.pack("<H", 1)
    path_table[8] = 0
    path_table_be = bytearray(10)
    path_table_be[0] = 1
    path_table_be[2:6] = struct.pack(">I", root_lba)
    path_table_be[6:8] = struct.pack(">H", 1)
    path_table_be[8] = 0
    pvd[132:140] = both_endian32(len(path_table))
    pvd[140:144] = struct.pack("<I", path_l_lba)
    pvd[148:152] = struct.pack(">I", path_m_lba)
    root = dirname_record(b"\x00", root_lba, root_size, True)
    assert len(root) == 34
    pvd[156:190] = root
    pvd[813:830] = date17
    pvd[830:847] = date17
    pvd[847:864] = date17
    pvd[864:881] = date17
    pvd[882] = 1  # file structure version
    assert len(pvd) == SECTOR

    boot_rec = bytearray(SECTOR)
    boot_rec[0] = 0
    boot_rec[1:6] = b"CD001"
    boot_rec[6] = 1
    boot_rec[7:39] = b"EL TORITO SPECIFICATION".ljust(32)
    boot_rec[71:75] = struct.pack("<I", catalog_lba)
    assert len(boot_rec) == SECTOR

    terminator = bytearray(SECTOR)
    terminator[0] = 255
    terminator[1:6] = b"CD001"
    terminator[6] = 1
    assert len(terminator) == SECTOR

    catalog = bytearray(SECTOR)
    catalog[0] = 0x01
    catalog[1] = 0x00
    catalog[30] = 0x55
    catalog[31] = 0xAA
    words = struct.unpack("<16H", bytes(catalog[:32]))
    catalog[28:30] = struct.pack("<H", (-sum(words)) & 0xFFFF)
    catalog[32] = 0x88
    catalog[33] = 0x02
    catalog[34:36] = struct.pack("<H", 0)
    catalog[38:40] = struct.pack("<H", 1)
    catalog[40:44] = struct.pack("<I", floppy_lba)
    assert len(catalog) == SECTOR

    path_l = pad(bytes(path_table), SECTOR)
    path_m = pad(bytes(path_table_be), SECTOR)

    rec_dot = dirname_record(b"\x00", root_lba, root_size, True)
    rec_dotdot = dirname_record(b"\x01", root_lba, root_size, True)
    rec_file = dirname_record(b"README.TXT;1", readme_lba, len(readme_text), False)
    root_dir = bytearray(SECTOR)
    off = 0
    for rec in (rec_dot, rec_dotdot, rec_file):
        root_dir[off:off + len(rec)] = rec
        off += len(rec)
    assert len(root_dir) == SECTOR

    chunks = [
        bytes(16 * SECTOR),
        bytes(pvd),
        bytes(boot_rec),
        bytes(terminator),
        bytes(catalog),
        bytes(path_l),
        bytes(path_m),
        bytes(root_dir),
        bytes(readme),
        pad(floppy, floppy_sectors * SECTOR),
    ]
    for i, chunk in enumerate(chunks):
        if i == 0:
            assert len(chunk) == 16 * SECTOR
        elif i == len(chunks) - 1:
            assert len(chunk) % SECTOR == 0
        else:
            assert len(chunk) == SECTOR, f"chunk {i} is {len(chunk)} bytes"
    image = b"".join(chunks)

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "wb") as handle:
        handle.write(image)
    print(f"Wrote {out_path} ({len(image)} bytes, floppy LBA {floppy_lba})")
    root = os.path.dirname(os.path.dirname(out_path))
    compatibility_paths = [
        os.path.join(root, "jas-os.iso"),
        os.path.join(root, "minios-x86.iso"),
        os.path.join(os.path.dirname(out_path), "minios-x86.iso"),
    ]
    for copy_path in compatibility_paths:
        if os.path.abspath(copy_path) == os.path.abspath(out_path):
            continue
        with open(copy_path, "wb") as handle:
            handle.write(image)
        print(f"Copied {copy_path}")


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: make_iso.py boot.bin kernel.bin out.iso")
    boot = open(sys.argv[1], "rb").read()
    kernel = open(sys.argv[2], "rb").read()
    make_iso(make_floppy(boot, kernel), sys.argv[3])


if __name__ == "__main__":
    main()
