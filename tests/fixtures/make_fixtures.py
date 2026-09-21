"""Regenerates the archive fixtures used by tests_utilities.

Needs 7-Zip on PATH (or at "C:/Program Files/7-Zip/7z.exe") for the .7z fixtures.
"""
import os
import shutil
import io
import struct
import subprocess
import tarfile
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))

PAK_MAGIC = 0x5A6F12E1


def make_pak() -> bytes:
    """A compressible pseudo pak: filler followed by a 44 byte UE4 footer."""
    body = (b"TRENCHKIT-FIXTURE-PAK-" * 200)
    footer = struct.pack("<IIQQ", PAK_MAGIC, 3, 0, 0) + bytes(20)
    return body + footer


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

    for mode, suffix in (("gz", "tar.gz"), ("bz2", "tar.bz2"), ("xz", "tar.xz")):
        with tarfile.open(os.path.join(HERE, "mod." + suffix), "w:" + mode) as tar:
            info = tarfile.TarInfo("Mods/Fixture.pak")
            info.size = len(pak)
            info.mtime = 0
            tar.addfile(info, io.BytesIO(pak))

    with zipfile.ZipFile(os.path.join(HERE, "mod_deflate.zip"), "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(zipfile.ZipInfo("Mods/Fixture.pak", (2020, 1, 1, 0, 0, 0)), pak)


if __name__ == "__main__":
    main()
