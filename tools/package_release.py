import argparse
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import List, Optional
from zipfile import ZIP_DEFLATED, ZipFile


def read_version(project_root: Path) -> str:
    cmake_file = project_root / "cmake" / "version.cmake"
    content = cmake_file.read_text(encoding="utf-8")
    major = re.search(r"TRENCHKIT_VERSION_MAJOR\s+(\d+)", content).group(1)
    minor = re.search(r"TRENCHKIT_VERSION_MINOR\s+(\d+)", content).group(1)
    patch = re.search(r"TRENCHKIT_VERSION_PATCH\s+(\d+)", content).group(1)
    pre = re.search(r'TRENCHKIT_VERSION_PRERELEASE\s+"([^"]*)"', content)
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


def stage_release_payload(build_dir: Path, staging_dir: Path) -> None:
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


def create_portable_archive(staging_dir: Path, archive_path: Path) -> None:
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(archive_path, "w", compression=ZIP_DEFLATED) as archive:
        for path in staging_dir.rglob("*"):
            if path.is_file():
                archive.write(path, path.relative_to(staging_dir))


def find_inno_setup_compiler(explicit_path: Optional[str] = None) -> Optional[Path]:
    candidates: List[Path] = []

    if explicit_path:
        candidates.append(Path(explicit_path))

    env_path = os.environ.get("ISCC_PATH")
    if env_path:
        candidates.append(Path(env_path))

    for name in ("ISCC.exe", "iscc.exe", "iscc"):
        resolved = shutil.which(name)
        if resolved:
            candidates.append(Path(resolved))

    if os.name == "nt":
        program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        program_files = os.environ.get("ProgramFiles", r"C:\Program Files")
        candidates.extend([
            Path(program_files_x86) / "Inno Setup 6" / "ISCC.exe",
            Path(program_files) / "Inno Setup 6" / "ISCC.exe",
        ])

    for candidate in candidates:
        if candidate.exists():
            return candidate

    return None


def build_windows_installer(project_root: Path,
                            staging_dir: Path,
                            dist_dir: Path,
                            version: str,
                            iscc_path: Path) -> Path:
    installer_script = project_root / "tools" / "installer.iss"
    output_base_name = f"TrenchKit-Setup-{version}"
    command = [
        str(iscc_path),
        f"/DAppVersion={version}",
        f"/DSourceDir={staging_dir}",
        f"/DOutputDir={dist_dir}",
        f"/DOutputBaseFilename={output_base_name}",
        f"/DProjectRoot={project_root}",
        str(installer_script),
    ]
    subprocess.run(command, check=True)
    return dist_dir / f"{output_base_name}.exe"


def main() -> int:
    parser = argparse.ArgumentParser(description="Package TrenchKit release build.")
    parser.add_argument("--build-dir", required=True, help="CMake build directory")
    parser.add_argument("--skip-installer", action="store_true",
                        help="Only build the portable zip artifact.")
    parser.add_argument("--require-installer", action="store_true",
                        help="Fail if the Windows installer cannot be generated.")
    parser.add_argument("--iscc-path",
                        help="Path to the Inno Setup compiler executable.")
    args = parser.parse_args()

    if args.skip_installer and args.require_installer:
        raise RuntimeError("--skip-installer and --require-installer cannot be used together.")

    project_root = Path(__file__).resolve().parents[1]
    build_dir = Path(args.build_dir).resolve()
    dist_dir = project_root / "dist"
    staging_dir = dist_dir / "app"

    version = read_version(project_root)
    stage_release_payload(build_dir, staging_dir)

    archive_path = dist_dir / f"windows-{version}.zip"
    create_portable_archive(staging_dir, archive_path)
    print(f"Packaged release: {archive_path}")

    if args.skip_installer:
        return 0

    iscc_path = find_inno_setup_compiler(args.iscc_path)
    if not iscc_path:
        if args.require_installer:
            raise RuntimeError(
                "Inno Setup compiler was not found. Install Inno Setup 6 or pass --iscc-path."
            )
        print("Skipping installer: Inno Setup compiler was not found.")
        return 0

    installer_path = build_windows_installer(project_root, staging_dir, dist_dir, version, iscc_path)
    print(f"Packaged installer: {installer_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
