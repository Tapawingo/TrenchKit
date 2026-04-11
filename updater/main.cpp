#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

struct InstallArgs {
    bool install = false;
    fs::path appDir;
    fs::path newDir;
    fs::path updatesDir;
    std::wstring exeName;
};

static fs::path g_updatesDir;

static fs::path helperExecutableName(const char *argv0) {
    if (argv0 && *argv0) {
        return fs::path(argv0).filename();
    }
#if defined(_WIN32)
    return fs::path(L"TrenchKitUpdater.exe");
#else
    return fs::path("TrenchKitUpdater");
#endif
}

static void appendLog(const fs::path &appDir, const std::string &message) {
    std::error_code ec;
    fs::path updatesDir = g_updatesDir.empty() ? (appDir / "updates") : g_updatesDir;
    fs::create_directories(updatesDir, ec);
    const fs::path logPath = updatesDir / "updater.log";
    std::ofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << message << "\n";
    }
}

static void logAndStderr(const fs::path &appDir, const std::string &message) {
    appendLog(appDir, message);
    std::cerr << message << "\n";
}

#if defined(_WIN32)
static void showErrorDialog(const std::wstring &message) {
    MessageBoxW(nullptr, message.c_str(), L"TrenchKit Updater", MB_OK | MB_ICONERROR);
}
#endif

static bool equalsIgnoreCase(const std::wstring &a, const std::wstring &b) {
#if defined(_WIN32)
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}

#if defined(_WIN32)
static bool shouldSkipCopy(const fs::path &name) {
    const std::wstring lower = name.wstring();
    return equalsIgnoreCase(lower, L"libgcc_s_seh-1.dll")
        || equalsIgnoreCase(lower, L"libstdc++-6.dll")
        || equalsIgnoreCase(lower, L"libwinpthread-1.dll");
}
#endif

static bool parseArgs(int argc, char **argv, InstallArgs &out) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--install") {
            out.install = true;
        } else if (arg == "--app-dir" && i + 1 < argc) {
            out.appDir = fs::path(argv[++i]);
        } else if (arg == "--new-dir" && i + 1 < argc) {
            out.newDir = fs::path(argv[++i]);
        } else if (arg == "--updates-dir" && i + 1 < argc) {
            out.updatesDir = fs::path(argv[++i]);
        } else if (arg == "--exe-name" && i + 1 < argc) {
            const std::string exe = argv[++i];
            out.exeName = std::wstring(exe.begin(), exe.end());
        }
    }
    return out.install && !out.appDir.empty() && !out.newDir.empty() && !out.exeName.empty();
}

#if defined(_WIN32)
static void waitForAppExit(const fs::path &appDir) {
    // Try to open the mutex created by the main app
    // This blocks until the main app releases it (exits)
    HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Global\\TrenchKitRunning");
    if (hMutex) {
        appendLog(appDir, "Waiting for mutex release (app exit)...");
        WaitForSingleObject(hMutex, 30000);  // 30 second timeout
        CloseHandle(hMutex);
        appendLog(appDir, "Mutex released, app has exited.");
    } else {
        // Mutex doesn't exist = app already exited, or running old version without mutex
        appendLog(appDir, "Mutex not found, assuming app already exited.");
    }
    // Brief delay for file handles to close
    Sleep(500);
}
#endif

static bool isHelperExecutable(const fs::path &name, const fs::path &helperName) {
#if defined(_WIN32)
    return equalsIgnoreCase(name.wstring(), helperName.wstring());
#else
    return name == helperName;
#endif
}

static bool copyRecursive(const fs::path &from, const fs::path &to, const fs::path &helperName) {
    std::error_code ec;
    fs::create_directories(to, ec);
    if (ec) {
        appendLog(to, "Failed to create target directory: " + ec.message());
        return false;
    }

    for (const auto &entry : fs::recursive_directory_iterator(from)) {
        const auto relPath = fs::relative(entry.path(), from, ec);
        if (ec) {
            appendLog(to, "Failed to resolve relative path: " + ec.message());
            return false;
        }
        const fs::path name = relPath.filename();
        if (isHelperExecutable(name, helperName)) {
            continue;
        }
        const fs::path destPath = to / relPath;
        if (entry.is_directory()) {
            fs::create_directories(destPath, ec);
            if (ec) {
                appendLog(to, "Failed to create directory " + destPath.string() + ": " + ec.message());
                return false;
            }
            continue;
        }
        if (entry.is_regular_file()) {
            fs::create_directories(destPath.parent_path(), ec);
            if (ec) {
                appendLog(to, "Failed to create parent directory " + destPath.parent_path().string()
                               + ": " + ec.message());
                return false;
            }
#if defined(_WIN32)
            if (fs::exists(destPath, ec) && shouldSkipCopy(name)) {
                appendLog(to, "Skipping locked runtime file: " + destPath.string());
                continue;
            }
#endif
            bool copied = fs::copy_file(entry.path(), destPath, fs::copy_options::overwrite_existing, ec);
            if (ec || !copied) {
#if defined(_WIN32)
                if (ec.value() == static_cast<int>(std::errc::file_exists)) {
                    appendLog(to, "Destination exists, retrying remove+copy: " + destPath.string());
                    ec.clear();
                    fs::remove(destPath, ec);
                    if (!ec) {
                        copied = fs::copy_file(entry.path(), destPath, fs::copy_options::overwrite_existing, ec);
                    }
                }
                if (ec.value() == static_cast<int>(std::errc::permission_denied)) {
                    appendLog(to, "Skipping locked file: " + destPath.string());
                    ec.clear();
                    continue;
                }
#endif
                if (ec) {
#if defined(_WIN32)
                    if (shouldSkipCopy(name)) {
                        appendLog(to, "Skipping copy failure for runtime file " + destPath.string()
                                       + ": " + ec.message());
                        ec.clear();
                        continue;
                    }
#endif
                    appendLog(to, "Failed to copy " + entry.path().string() + " to "
                                   + destPath.string() + ": " + ec.message());
                    return false;
                }
            }
        }
    }

    return true;
}

static void removeOldAppFiles(const fs::path &appDir, const fs::path &helperName) {
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(appDir)) {
        const fs::path name = entry.path().filename();
#if defined(_WIN32)
        const std::wstring entryName = name.wstring();
        if (equalsIgnoreCase(entryName, helperName.wstring())) {
            continue;
        }
        if (equalsIgnoreCase(entryName, L"updates")) {
            continue;
        }
#else
        if (name == helperName || name == "updates") {
            continue;
        }
#endif
        fs::remove_all(entry.path(), ec);
        if (ec) {
            appendLog(appDir, "Failed to remove " + entry.path().string() + ": " + ec.message());
        }
    }
}

#if defined(_WIN32)
static bool launchApp(const fs::path &appDir, const std::wstring &exeName) {
    const fs::path exePath = appDir / exeName;
    const HINSTANCE result = ShellExecuteW(
        nullptr,
        L"open",
        exePath.c_str(),
        nullptr,
        appDir.c_str(),
        SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}
#endif

int main(int argc, char **argv) {
    InstallArgs args;
    if (!parseArgs(argc, argv, args)) {
        std::cerr << "Usage: TrenchKitUpdater --install --app-dir <dir> --new-dir <dir> "
                     "--exe-name <name> [--updates-dir <dir>]\n";
        return 1;
    }
    const fs::path helperName = helperExecutableName(argv[0]);

    if (!args.updatesDir.empty()) {
        g_updatesDir = args.updatesDir;
    }
    appendLog(args.appDir, "Updater started.");
    appendLog(args.appDir, "App dir: " + args.appDir.string());
    appendLog(args.appDir, "New dir: " + args.newDir.string());
    appendLog(args.appDir, "Exe name: " + std::string(args.exeName.begin(), args.exeName.end()));

#if defined(_WIN32)
    waitForAppExit(args.appDir);
#else
    // On non-Windows, just wait briefly for the app to exit
    // The app should have already quit before launching the updater
    appendLog(args.appDir, "Waiting briefly for app to release files...");
    sleep(1);
#endif

    if (!fs::exists(args.newDir)) {
        logAndStderr(args.appDir, "New version directory not found.");
#if defined(_WIN32)
        showErrorDialog(L"Update failed: new version directory not found.");
#endif
        return 1;
    }

    appendLog(args.appDir, "Copying new version files.");
    if (!copyRecursive(args.newDir, args.appDir, helperName)) {
        logAndStderr(args.appDir, "Failed to copy new version files.");
#if defined(_WIN32)
        showErrorDialog(L"Update failed while copying new version files.");
#endif
        return 1;
    }

#if defined(_WIN32)
    if (!launchApp(args.appDir, args.exeName)) {
        const fs::path exePath = args.appDir / args.exeName;
        logAndStderr(args.appDir, "Failed to relaunch application.");
        const std::wstring message = L"Update applied, but TrenchKit failed to relaunch.\n"
                                     L"Please start it manually from:\n" + exePath.wstring();
        showErrorDialog(message);
        return 1;
    }
#else
    // Relaunch on non-Windows using fork + exec
    const fs::path exePath = args.appDir / std::string(args.exeName.begin(), args.exeName.end());
    appendLog(args.appDir, "Relaunching application: " + exePath.string());
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - exec the app
        execl(exePath.c_str(), exePath.filename().c_str(), nullptr);
        // If execl returns, it failed
        _exit(1);
    } else if (pid < 0) {
        logAndStderr(args.appDir, "Failed to fork for relaunch.");
    }
#endif

    appendLog(args.appDir, "Update completed successfully.");
    return 0;
}
