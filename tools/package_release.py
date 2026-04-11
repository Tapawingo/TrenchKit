import argparse
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import List, Optional, Tuple
from zipfile import ZIP_DEFLATED, ZipFile

APP_EXE_NAME = "TrenchKit.exe"
HELPER_EXE_NAME = "TrenchKitUpdater.exe"
QT_RUNTIME_DLLS = (
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Widgets.dll",
    "Qt6Network.dll",
    "Qt6Concurrent.dll",
    "Qt6WebSockets.dll",
)
MINGW_RUNTIME_DLLS = (
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
)


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

    exe_names = {APP_EXE_NAME, HELPER_EXE_NAME}
    for exe_name in exe_names:
        exe_path = build_dir / exe_name
        if not exe_path.exists():
            raise RuntimeError(f"Missing required executable: {exe_path}")
        shutil.copy2(exe_path, staging_dir / exe_name)

    for dll_name in QT_RUNTIME_DLLS + MINGW_RUNTIME_DLLS:
        dll_path = build_dir / dll_name
        if dll_path.exists():
            shutil.copy2(dll_path, staging_dir / dll_path.name)

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


def find_signtool(explicit_path: Optional[str] = None) -> Optional[Path]:
    candidates: List[Path] = []

    if explicit_path:
        candidates.append(Path(explicit_path))

    env_path = os.environ.get("SIGNTOOL_PATH")
    if env_path:
        candidates.append(Path(env_path))

    resolved = shutil.which("signtool.exe") or shutil.which("signtool")
    if resolved:
        candidates.append(Path(resolved))

    if os.name == "nt":
        program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
        windows_kits_root = Path(program_files_x86) / "Windows Kits" / "10" / "bin"
        if windows_kits_root.exists():
            for arch in ("x64", "x86"):
                for candidate in sorted(windows_kits_root.glob(f"*/{arch}/signtool.exe"), reverse=True):
                    candidates.append(candidate)

    for candidate in candidates:
        if candidate.exists():
            return candidate

    return None


def resolve_signing_inputs(args: argparse.Namespace) -> Tuple[Optional[Path], Optional[Path], Optional[str], Optional[str]]:
    sign_pfx = args.sign_pfx or os.environ.get("TRENCHKIT_SIGN_PFX")
    if not sign_pfx:
        return None, None, None, None

    signtool_path = find_signtool(args.signtool_path)
    sign_pfx_path = Path(sign_pfx)
    if not sign_pfx_path.exists():
        raise RuntimeError(f"Signing certificate was not found: {sign_pfx_path}")

    if not signtool_path:
        raise RuntimeError("signtool.exe was not found. Install the Windows SDK or pass --signtool-path.")

    password = os.environ.get(args.sign_password_env) if args.sign_password_env else None
    timestamp_url = args.timestamp_url or os.environ.get("TRENCHKIT_TIMESTAMP_URL")
    return signtool_path, sign_pfx_path, password, timestamp_url


def sign_file(signtool_path: Path,
              file_path: Path,
              sign_pfx_path: Path,
              password: Optional[str],
              timestamp_url: Optional[str]) -> None:
    command = [
        str(signtool_path),
        "sign",
        "/fd",
        "SHA256",
        "/f",
        str(sign_pfx_path),
        "/d",
        "TrenchKit",
        "/du",
        "https://github.com/Tapawingo/TrenchKit",
    ]
    if password:
        command.extend(["/p", password])
    if timestamp_url:
        command.extend(["/tr", timestamp_url, "/td", "SHA256"])
    command.append(str(file_path))
    subprocess.run(command, check=True)


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
    parser.add_argument("--signtool-path",
                        help="Path to signtool.exe for Authenticode signing.")
    parser.add_argument("--sign-pfx",
                        help="Path to a PFX certificate used to sign staged binaries and the installer.")
    parser.add_argument("--sign-password-env", default="TRENCHKIT_SIGN_PFX_PASSWORD",
                        help="Environment variable that contains the PFX password.")
    parser.add_argument("--timestamp-url",
                        help="RFC3161 timestamp server URL used during signing.")
    parser.add_argument("--require-signing", action="store_true",
                        help="Fail if signing inputs are unavailable.")
    args = parser.parse_args()

    if args.skip_installer and args.require_installer:
        raise RuntimeError("--skip-installer and --require-installer cannot be used together.")

    project_root = Path(__file__).resolve().parents[1]
    build_dir = Path(args.build_dir).resolve()
    dist_dir = project_root / "dist"
    staging_dir = dist_dir / "app"

    version = read_version(project_root)
    stage_release_payload(build_dir, staging_dir)

    sign_pfx_defined = bool(args.sign_pfx or os.environ.get("TRENCHKIT_SIGN_PFX"))
    if sign_pfx_defined:
        signtool_path, sign_pfx_path, password, timestamp_url = resolve_signing_inputs(args)
        sign_file(signtool_path, staging_dir / APP_EXE_NAME, sign_pfx_path, password, timestamp_url)
        sign_file(signtool_path, staging_dir / HELPER_EXE_NAME, sign_pfx_path, password, timestamp_url)
    elif args.require_signing:
        raise RuntimeError("Signing was required, but no PFX certificate was configured.")

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
    if sign_pfx_defined:
        sign_file(signtool_path, installer_path, sign_pfx_path, password, timestamp_url)
    print(f"Packaged installer: {installer_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
