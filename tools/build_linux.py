"""
Build a self-contained Linux AppImage using Docker and package it into dist/.

Usage:
    python tools/build_linux.py [--qt-version 6.10.1] [--no-cache] [--reinstall-qt]

Output:
    dist/TrenchKit-{version}-x86_64.AppImage

Qt and AppImage tools (linuxdeploy, appimagetool) are cached in a named Docker
volume (trenchkit-qt-<version>) so subsequent builds are fast. Pass --reinstall-qt
to force a fresh Qt install (also re-downloads AppImage tools).

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


def tools_install_script() -> str:
    """Downloads linuxdeploy, linuxdeploy-plugin-qt, and appimagetool into /qt/tools/."""
    return textwrap.dedent("""\
        set -e
        TOOLS=/qt/tools
        mkdir -p "$TOOLS"
        dl() {
            local name="$1" url="$2"
            if [ ! -f "$TOOLS/$name" ]; then
                echo "==> Downloading $name..."
                wget -q --show-progress -O "$TOOLS/$name" "$url"
                chmod +x "$TOOLS/$name"
            else
                echo "==> $name already cached, skipping."
            fi
        }
        dl linuxdeploy           https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
        dl linuxdeploy-plugin-qt https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
        dl appimagetool          https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
        echo "==> AppImage tools ready."
    """)


def build_package_script(qt_dir: str, version: str, archive_name: str) -> str:
    """Configures, builds, and packages TrenchKit as an AppImage inside the container."""
    return textwrap.dedent(f"""\
        set -e
        export PATH="{qt_dir}/bin:$PATH"
        export CMAKE_PREFIX_PATH="{qt_dir}"
        export APPIMAGE_EXTRACT_AND_RUN=1

        echo "==> Configuring..."
        cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5

        echo "==> Building..."
        cmake --build /build

        echo "==> Staging AppDir..."
        APPDIR=/tmp/AppDir
        rm -rf "$APPDIR"
        mkdir -p "$APPDIR/usr/bin" \\
                 "$APPDIR/usr/share/applications" \\
                 "$APPDIR/usr/share/icons/hicolor/256x256/apps"

        cp /build/src/TrenchKit            "$APPDIR/usr/bin/"
        cp /build/updater/TrenchKitUpdater "$APPDIR/usr/bin/"

        cp /src/extras/linux/io.github.tapawingo.trenchkit.desktop \\
           "$APPDIR/usr/share/applications/"
        cp /src/extras/logo/logo_transparent.png \\
           "$APPDIR/usr/share/icons/hicolor/256x256/apps/io.github.tapawingo.trenchkit.png"

        # TLS plugin — place it where Qt expects it inside the AppDir
        TLS_SRC=/build/src/tls/libqopensslbackend.so
        if [ -f "$TLS_SRC" ]; then
            mkdir -p "$APPDIR/usr/plugins/tls"
            cp "$TLS_SRC" "$APPDIR/usr/plugins/tls/"
        fi

        echo "==> Running linuxdeploy with Qt plugin..."
        export QMAKE="{qt_dir}/bin/qmake"
        # Run the Qt plugin standalone first so it can set up Qt-specific dirs
        /qt/tools/linuxdeploy-plugin-qt --appdir "$APPDIR"
        # Then run linuxdeploy to bundle all remaining libs and finalize the AppDir
        /qt/tools/linuxdeploy \\
            --appdir "$APPDIR" \\
            --plugin qt \\
            --executable "$APPDIR/usr/bin/TrenchKit" \\
            --desktop-file "$APPDIR/usr/share/applications/io.github.tapawingo.trenchkit.desktop" \\
            --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/io.github.tapawingo.trenchkit.png"

        echo "==> Building AppImage..."
        ARCH=x86_64 /qt/tools/appimagetool "$APPDIR" "/dist/{archive_name}"

        echo "==> Done."
    """)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build a Linux AppImage via Docker and package it into dist/."
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
        help="Delete the Qt cache volume and re-download Qt and AppImage tools"
    )
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[1]
    dist_dir     = project_root / "dist"
    dist_dir.mkdir(parents=True, exist_ok=True)

    dockerfile  = project_root / "tools" / "Dockerfile.linux-builder"
    qt_volume   = f"trenchkit-qt-{args.qt_version}"
    qt_dir      = f"/qt/{args.qt_version}/gcc_64"

    # ── 1. Build Docker image ────────────────────────────────────────────────
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

    # ── 4. Install AppImage tools into the same volume (skipped if cached) ───
    print(f"\n==> Ensuring AppImage tools are in cache volume {qt_volume} ...\n")
    run([
        "docker", "run", "--rm",
        "-v", f"{qt_volume}:/qt",
        IMAGE_TAG,
        "bash", "-c", tools_install_script(),
    ])

    # ── 5. Build + package ───────────────────────────────────────────────────
    version      = read_version(project_root)
    archive_name = f"TrenchKit-{version}-x86_64.AppImage"
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
        "bash", "-c", build_package_script(qt_dir, version, archive_name),
    ])

    print(f"\nPackaged release: {archive_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
