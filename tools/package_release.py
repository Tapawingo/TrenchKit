import argparse
import re
import shutil
import sys
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED


def read_version(project_root: Path) -> str:
    cmake_file = project_root / "cmake" / "version.cmake"
    content = cmake_file.read_text(encoding="utf-8")
    major = re.search(r"TRENCHKIT_VERSION_MAJOR\s+(\d+)", content).group(1)
    minor = re.search(r"TRENCHKIT_VERSION_MINOR\s+(\d+)", content).group(1)
    patch = re.search(r"TRENCHKIT_VERSION_PATCH\s+(\d+)", content).group(1)
    pre   = re.search(r'TRENCHKIT_VERSION_PRERELEASE\s+"([^"]*)"', content)
    version = f"{major}.{minor}.{patch}"
    if pre and pre.group(1):
        version += f"-{pre.group(1)}"
    return version


def copy_tree(src: Path, dst: Path) -> None:
    if not src.exists():
        return
    if src.is_dir():
        shutil.copytree(src, dst, dirs_exist_ok=True)
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def main() -> int:
    parser = argparse.ArgumentParser(description="Package TrenchKit release build.")
    parser.add_argument("--build-dir", required=True, help="CMake build directory")
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[1]
    build_dir = Path(args.build_dir).resolve()
    dist_dir = project_root / "dist"
    staging_dir = dist_dir / "app"

    version = read_version(project_root)

    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    staging_dir.mkdir(parents=True, exist_ok=True)

    exe_names = {"TrenchKit.exe", "updater.exe"}
    for exe_name in exe_names:
        exe_path = build_dir / exe_name
        if not exe_path.exists():
            raise RuntimeError(f"Missing required executable: {exe_path}")
        shutil.copy2(exe_path, staging_dir / exe_name)

    for dll in build_dir.glob("*.dll"):
        shutil.copy2(dll, staging_dir / dll.name)

    zip_dll = build_dir / "_deps" / "zip-build" / "libzip.dll"
    if zip_dll.exists():
        shutil.copy2(zip_dll, staging_dir / zip_dll.name)

    copy_tree(build_dir / "platforms", staging_dir / "platforms")
    copy_tree(build_dir / "tls", staging_dir / "tls")

    archive_path = dist_dir / f"windows-{version}.zip"
    dist_dir.mkdir(parents=True, exist_ok=True)

    with ZipFile(archive_path, "w", compression=ZIP_DEFLATED) as archive:
        for path in staging_dir.rglob("*"):
            if path.is_file():
                archive.write(path, path.relative_to(staging_dir))

    print(f"Packaged release: {archive_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
