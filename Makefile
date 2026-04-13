# Simple Makefile wrapper for CMake/Ninja workflows

BUILD_DIR_DEBUG=build/debug
BUILD_DIR_RELEASE=build/release
GENERATOR=Ninja
PACKAGE_RELEASE_ARGS=
BUILD_LINUX_ARGS=

ifeq ($(OS),Windows_NT)
EXE_EXT = .exe
else
EXE_EXT =
endif

.PHONY: all configure-debug configure-release build-debug build-release package-release package-release-no-installer package-release-require-installer package-linux-release test test-debug run-debug run-release translations-template clean

all: build-debug

configure-debug:
	cmake -S . -B $(BUILD_DIR_DEBUG) -G $(GENERATOR) -DCMAKE_BUILD_TYPE=Debug

configure-release:
	cmake -S . -B $(BUILD_DIR_RELEASE) -G $(GENERATOR) -DCMAKE_BUILD_TYPE=Release

build-debug: configure-debug
	cmake --build $(BUILD_DIR_DEBUG)

build-release: configure-release
	cmake --build $(BUILD_DIR_RELEASE)
	python tools/package_release.py --build-dir $(BUILD_DIR_RELEASE) $(PACKAGE_RELEASE_ARGS)

package-release: build-release

package-release-no-installer: configure-release
	cmake --build $(BUILD_DIR_RELEASE)
	python tools/package_release.py --build-dir $(BUILD_DIR_RELEASE) --skip-installer

package-release-require-installer: configure-release
	cmake --build $(BUILD_DIR_RELEASE)
	python tools/package_release.py --build-dir $(BUILD_DIR_RELEASE) --require-installer

package-linux-release:
	python tools/build_linux.py $(BUILD_LINUX_ARGS)

test: test-debug

test-debug: build-debug
	ctest --test-dir $(BUILD_DIR_DEBUG) --output-on-failure

run-debug: build-debug
	$(BUILD_DIR_DEBUG)/TrenchKit$(EXE_EXT)

run-release: build-release
	$(BUILD_DIR_RELEASE)/TrenchKit$(EXE_EXT)

translations-template: configure-debug
	cmake --build $(BUILD_DIR_DEBUG) --target update_translation_template

clean:
	cmake --build $(BUILD_DIR_DEBUG) --target clean || true
	cmake --build $(BUILD_DIR_RELEASE) --target clean || true
