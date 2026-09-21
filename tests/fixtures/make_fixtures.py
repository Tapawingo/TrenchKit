"""Regenerates the archive fixtures used by tests_utilities.

Needs 7-Zip on PATH (or at "C:/Program Files/7-Zip/7z.exe") for the .7z fixtures.
The .rar fixture is written by hand (RAR5, "store" method) because RAR creators
are not freely available; it is verified with `7z t` and libarchive.
"""
import os
import shutil
import io
import struct
import subprocess
import tarfile
import tempfile
import zipfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

PAK_MAGIC = 0x5A6F12E1


def make_pak() -> bytes:
    """A compressible pseudo pak: filler followed by a 44 byte UE4 footer."""
    body = (b"TRENCHKIT-FIXTURE-PAK-" * 200)
    footer = struct.pack("<IIQQ", PAK_MAGIC, 3, 0, 0) + bytes(20)
    return body + footer


def vint(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def rar5_block(header_type: int, header_flags: int, body: bytes, data: bytes = b"") -> bytes:
    fields = vint(header_type) + vint(header_flags)
    if header_flags & 0x2:
        fields += vint(len(data))
    fields += body
    header = vint(len(fields)) + fields
    return struct.pack("<I", zlib.crc32(header) & 0xFFFFFFFF) + header + data


def make_rar5_stored(name: str, content: bytes, solid: bool = False) -> bytes:
    signature = b"Rar!\x1a\x07\x01\x00"
    main = rar5_block(1, 0, vint(0x4 if solid else 0))
    file_flags = 0x4  # CRC32 present
    body = (vint(file_flags)
            + vint(len(content))              # unpacked size
            + vint(0x20)                      # attributes
            + struct.pack("<I", zlib.crc32(content) & 0xFFFFFFFF)
            + vint(0)                         # compression info: version 0, store
            + vint(0)                         # host OS: Windows
            + vint(len(name.encode())) + name.encode())
    file_block = rar5_block(2, 0x2, body, content)
    end = rar5_block(5, 0, vint(0))
    return signature + main + file_block + end


def find_7z() -> str:
    found = shutil.which("7z") or shutil.which("7zz")
    if found:
        return found
    default = "C:/Program Files/7-Zip/7z.exe"
    if os.path.exists(default):
        return default
    raise SystemExit("7-Zip not found")


def main():
    pak = make_pak()

    with tempfile.TemporaryDirectory() as tmp:
        os.makedirs(os.path.join(tmp, "Mods"))
        with open(os.path.join(tmp, "Mods", "Fixture.pak"), "wb") as f:
            f.write(pak)
        with open(os.path.join(tmp, "readme.txt"), "w") as f:
            f.write("not a pak\n")

        target = os.path.join(HERE, "mod_lzma.7z")
        if os.path.exists(target):
            os.remove(target)
        subprocess.run([find_7z(), "a", "-t7z", "-m0=lzma2", "-mx=5", target, "Mods", "readme.txt"],
                       cwd=tmp, check=True, stdout=subprocess.DEVNULL)

    with open(os.path.join(HERE, "mod_stored.rar"), "wb") as f:
        f.write(make_rar5_stored("Mods/Fixture.pak", pak))

    for mode, suffix in (("gz", "tar.gz"), ("bz2", "tar.bz2"), ("xz", "tar.xz")):
        with tarfile.open(os.path.join(HERE, "mod." + suffix), "w:" + mode) as tar:
            info = tarfile.TarInfo("Mods/Fixture.pak")
            info.size = len(pak)
            info.mtime = 0
            tar.addfile(info, io.BytesIO(pak))

    with zipfile.ZipFile(os.path.join(HERE, "mod_deflate.zip"), "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(zipfile.ZipInfo("Mods/Fixture.pak", (2020, 1, 1, 0, 0, 0)), pak)

    with open(os.path.join(HERE, "not_a_pak.pak"), "wb") as f:
        f.write(b"PK\x03\x04 this is really an archive, not a pak" + bytes(200))


if __name__ == "__main__":
    main()
