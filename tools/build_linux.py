"""
Build a self-contained Linux release binary using Docker and package it into dist/.

Usage:
    python tools/build_linux.py [--qt-version 6.10.1] [--no-cache] [--reinstall-qt]

Output:
    dist/linux-{version}.zip

Qt is installed once into a named Docker volume (trenchkit-qt-<version>) so
subsequent builds are fast. Pass --reinstall-qt to force a fresh Qt install.

Requires Docker to be installed and running.
"""

import argparse
import re
import subprocess
import textwrap
from pathlib import Path


DEFAULT_QT_VERSION = "6.10.1"
IMAGE_TAG = "trenchkit-linux-builder:latest"


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


def run(cmd: list, **kwargs) -> None:
    print(f"$ {' '.join(str(c) for c in cmd)}", flush=True)
    subprocess.run(cmd, check=True, **kwargs)


def qt_install_script(qt_version: str, qt_dir: str) -> str:
    """Installs Qt into the volume if qmake isn't already present."""
    return textwrap.dedent(f"""\
        set -e
        if [ -f "{qt_dir}/bin/qmake" ]; then
            echo "==> Qt {qt_version} already installed in cache volume, skipping."
        else
            echo "==> Installing Qt {qt_version} into cache volume..."
            aqt install-qt linux desktop {qt_version} linux_gcc_64 \\
                --outputdir /qt \\
                -m qtwebsockets
            echo "==> Qt installed."
        fi
    """)


def build_package_script(qt_dir: str, archive_name: str) -> str:
    """Configures, builds, and packages TrenchKit inside the container."""
    return textwrap.dedent(f"""\
        set -e
        export PATH="{qt_dir}/bin:$PATH"
        export CMAKE_PREFIX_PATH="{qt_dir}"

        echo "==> Configuring..."
        cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5

        echo "==> Building..."
        cmake --build /build

        echo "==> Packaging..."
        STAGE=/tmp/stage
        rm -rf "$STAGE"
        mkdir -p "$STAGE/lib" "$STAGE/platforms" "$STAGE/tls"

        # Binaries
        cp /build/src/TrenchKit    "$STAGE/"
        cp /build/updater/TrenchKitUpdater  "$STAGE/"

        # TLS plugin
        TLS_SRC=/build/src/tls/libqopensslbackend.so
        [ -f "$TLS_SRC" ] && cp "$TLS_SRC" "$STAGE/tls/"

        # xcb platform plugin (keep XCB_SRC for the ldd sweep below)
        XCB_SRC="{qt_dir}/plugins/platforms/libqxcb.so"
        [ -f "$XCB_SRC" ] && cp "$XCB_SRC" "$STAGE/platforms/"

        # Qt shared libraries needed by the binary and plugins
        QT_LIB_DIR="{qt_dir}/lib"
        for lib in \\
            libQt6Core libQt6Gui libQt6Widgets \\
            libQt6Network libQt6Concurrent libQt6WebSockets \\
            libQt6XcbQpa libQt6DBus libQt6OpenGL
        do
            src=$(find "$QT_LIB_DIR" -maxdepth 1 -name "${{lib}}.so.*" ! -name "*.debug" | sort | tail -1)
            [ -n "$src" ] && cp "$src" "$STAGE/lib/"
        done

        # Sweep all Qt source libs for any deps Qt bundles in its own dir
        # (covers ICU 73, libzstd, and anything else Qt ships alongside its modules)
        for qt_src in "$QT_LIB_DIR"/libQt6*.so.*; do
            [ -f "$qt_src" ] || continue
            ldd "$qt_src" 2>/dev/null | awk '/=> \\// {{print $3}}' | while read -r dep; do
                [[ "$dep" == /qt/* ]] || continue
                base=$(basename "$dep")
                [ -f "$dep" ] && [ ! -f "$STAGE/lib/$base" ] && cp "$dep" "$STAGE/lib/"
            done
        done
        # Same sweep for the xcb plugin
        if [ -f "$STAGE/platforms/libqxcb.so" ]; then
            ldd "$XCB_SRC" 2>/dev/null | awk '/=> \\// {{print $3}}' | while read -r dep; do
                [[ "$dep" == /qt/* ]] || continue
                base=$(basename "$dep")
                [ -f "$dep" ] && [ ! -f "$STAGE/lib/$base" ] && cp "$dep" "$STAGE/lib/"
            done
        fi

        # Create SONAME symlinks (e.g. libQt6Core.so.6 -> libQt6Core.so.6.10.1)
        # Without these the ELF loader cannot find the bundled libs and falls back to system Qt
        for f in "$STAGE/lib/"*.so.*; do
            [ -L "$f" ] && continue
            soname=$(readelf -d "$f" 2>/dev/null | awk '/SONAME/ {{gsub(/[\\[\\]]/, "", $NF); print $NF}}')
            [ -n "$soname" ] && [ ! -e "$STAGE/lib/$soname" ] && ln -sf "$(basename "$f")" "$STAGE/lib/$soname"
        done

        # Launcher script (sets LD_LIBRARY_PATH and QT_PLUGIN_PATH)
        cat > "$STAGE/TrenchKit.sh" << 'LAUNCHER'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$DIR"
exec "$DIR/TrenchKit" "$@"
LAUNCHER
        chmod +x "$STAGE/TrenchKit.sh"

        echo "==> Creating archive /dist/{archive_name}..."
        cd "$STAGE"
        zip -ry "/dist/{archive_name}" .

        echo "==> Done."
    """)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build a Linux release binary via Docker and package it into dist/."
    )
    parser.add_argument(
        "--qt-version", default=DEFAULT_QT_VERSION,
        help=f"Qt version to build against (default: {DEFAULT_QT_VERSION})"
    )
    parser.add_argument(
        "--no-cache", action="store_true",
        help="Rebuild the Docker image from scratch (ignores layer cache)"
    )
    parser.add_argument(
        "--reinstall-qt", action="store_true",
        help="Delete the Qt cache volume and re-download Qt"
    )
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[1]
    dist_dir     = project_root / "dist"
    dist_dir.mkdir(parents=True, exist_ok=True)

    dockerfile  = project_root / "tools" / "Dockerfile.linux-builder"
    qt_volume   = f"trenchkit-qt-{args.qt_version}"
    qt_dir      = f"/qt/{args.qt_version}/gcc_64"

    # ── 1. Build Docker image (system packages + cmake + aqtinstall only) ───
    print(f"\n==> Building Docker image {IMAGE_TAG} ...\n")
    docker_build = [
        "docker", "build",
        "-t", IMAGE_TAG,
        "-f", str(dockerfile),
        str(project_root),
    ]
    if args.no_cache:
        docker_build.append("--no-cache")
    run(docker_build)

    # ── 2. Optionally wipe the Qt cache volume ───────────────────────────────
    if args.reinstall_qt:
        print(f"\n==> Removing Qt cache volume {qt_volume} ...\n")
        subprocess.run(["docker", "volume", "rm", qt_volume], check=False)

    # ── 3. Install Qt into named volume (skipped if already installed) ───────
    print(f"\n==> Ensuring Qt {args.qt_version} is in cache volume {qt_volume} ...\n")
    run([
        "docker", "run", "--rm",
        "-v", f"{qt_volume}:/qt",
        IMAGE_TAG,
        "bash", "-c", qt_install_script(args.qt_version, qt_dir),
    ])

    # ── 4. Build + package ───────────────────────────────────────────────────
    version      = read_version(project_root)
    archive_name = f"linux-{version}.zip"
    archive_path = dist_dir / archive_name

    if archive_path.exists():
        archive_path.unlink()

    print(f"\n==> Building TrenchKit {version} ...\n")
    run([
        "docker", "run", "--rm",
        "-v", f"{qt_volume}:/qt:ro",
        "-v", f"{project_root}:/src:ro",
        "-v", f"{dist_dir}:/dist",
        IMAGE_TAG,
        "bash", "-c", build_package_script(qt_dir, archive_name),
    ])

    print(f"\nPackaged release: {archive_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
