# Contributing

Thanks for helping improve this project!

## Ground rules
- Be respectful and constructive.
- Keep changes focused and easy to review.
- Prefer clarity over cleverness.

## How to contribute

### Bug reports
When opening an issue, please include:
- What you expected to happen
- What actually happened
- Steps to reproduce
- Logs/screenshots if relevant
- Your setup (OS, compiler/toolchain, Qt version, CMake version)

### Feature requests
Describe the user problem first, then the proposed solution.
If it touches packaging/licensing-sensitive areas (Qt modules, deployment, bundling), mention that upfront.

### Pull requests
- Fork the repo and create a feature branch: `feature/<name>` or `fix/<name>`
- Keep PRs small where possible
- Update docs if behavior changes
- Ensure it configures and builds with CMake + Ninja

## Build instructions (Ninja)

### Configure (Debug)
```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

### Build
```sh
cmake --build build/debug
```

### Configure (Release)
```sh
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
```

### Build
```sh
cmake --build build/release
```

### Package a Windows release
```sh
python tools/package_release.py --build-dir build/release
```

With Inno Setup 6 installed, this generates both the portable zip and the Windows installer.
Use `--require-installer` when you want packaging to fail instead of silently skipping the installer.

## If Qt isn’t found
Prefer one of these approaches:

### `CMAKE_PREFIX_PATH`
```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="</path/to/Qt>"
```

### `Qt6_DIR`
```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DQt6_DIR="</path/to/Qt>/lib/cmake/Qt6"
```

### vcpkg toolchain
```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="</path/to/vcpkg>/scripts/buildsystems/vcpkg.cmake"
```

## Code style
- Keep it consistent with existing code
- Prefer modern C++ (C++20), RAII, and Qt idioms
- Avoid unnecessary dependencies
- UI: keep the main window simple and responsive

## PR titles and commit messages
PR titles appear directly in the changelog. Use the format `area - verb changes`:
- `Profiles - Add save/load support`
- `Mod list - Fix refresh when folder missing`
- `Installer - Improve path detection`

Verbs: `Add`, `Fix`, `Improve`, `Change`, `Make`, `Remove`

Use the same style for commit messages.

## How the changelog works
Release notes are generated automatically from merged PRs using [release-drafter](https://github.com/release-drafter/release-drafter). PRs are sorted into sections based on their labels:

| Label | Section |
|---|---|
| `type: feature` | Added |
| `type: bug` | Fixed |
| `type: enhancement`, `type: performance`, `type: refactor` | Improved |
| `type: translation` | Translations |

PRs labeled `type: chore`, `type: docs`, or `ignore-changelog` are excluded from the release notes.

Each PR should have exactly one `type:` label. `area:` labels (e.g. `area: translations`, `area: ui-ux`) can be added alongside a `type:` label - they don't affect the changelog.

## Licensing notes
- Repository code is MIT.
- Avoid adding Qt modules that are GPL-only unless the project explicitly opts into GPL terms or uses a commercial Qt license.
- If you add third-party code, ensure its license is compatible and add attribution where required.

## By contributing
You agree that your contributions may be redistributed under this repository’s license (MIT).
