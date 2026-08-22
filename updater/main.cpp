#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <ctime>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#else
#include <cerrno>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

struct InstallArgs {
    bool install = false;
    fs::path appDir;
    fs::path newDir;
    fs::path updatesDir;
    std::wstring exeName;
    // AppImage mode (Linux)
    fs::path appImagePath;  ///< Path to the current AppImage file
    fs::path newFilePath;   ///< Path to the new AppImage to install
    // Cross-platform
    int waitPid = 0;        ///< PID of main app to wait for before installing
};

static fs::path g_updatesDir;

// ── Logging ─────────────────────────────────────────────────────────────────

enum class LogLevel { Info, Warn, Error };

static const char *levelTag(LogLevel l) {
    switch (l) {
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        default:              return "INFO ";
    }
}

static std::string logTimestamp() {
    const auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

static void appendLog(const fs::path &appDir, const std::string &message,
                      LogLevel level = LogLevel::Info) {
    std::error_code ec;
    fs::path updatesDir = g_updatesDir.empty() ? (appDir / "updates") : g_updatesDir;
    fs::create_directories(updatesDir, ec);
    const fs::path logPath = updatesDir / "updater.log";
    std::ofstream logFile(logPath, std::ios::app);
    if (logFile.is_open()) {
        logFile << "[" << logTimestamp() << "] [" << levelTag(level) << "] " << message << "\n";
    }
}

static void logAndStderr(const fs::path &appDir, const std::string &message,
                         LogLevel level = LogLevel::Error) {
    appendLog(appDir, message, level);
    std::cerr << "[" << levelTag(level) << "] " << message << "\n";
}

// ── Error dialogs ────────────────────────────────────────────────────────────

#if defined(_WIN32)
static void showErrorDialog(const fs::path & /*logPath*/, const std::wstring &message) {
    MessageBoxW(nullptr, message.c_str(), L"TrenchKit Updater", MB_OK | MB_ICONERROR);
}
#else
/// Tries zenity, kdialog, or xmessage to show a modal error to the user.
/// Falls back silently if none are available (logAndStderr already wrote to stderr).
static void showErrorDialog(const fs::path &logPath, const std::string &message) {
    const std::string full = message + "\n\nSee log for details:\n" + logPath.string();

    struct Tool { const char *exe; std::vector<const char *> args; };
    // Build argv lists; placeholders replaced below
    const std::string zenityText  = "--text=" + full;
    const std::string kdialogText = full;
    const Tool tools[] = {
        {"zenity",   {"zenity",   "--error", "--title=TrenchKit Updater", zenityText.c_str(),  nullptr}},
        {"kdialog",  {"kdialog",  "--error", kdialogText.c_str(), "--title", "TrenchKit Updater", nullptr}},
        {"xmessage", {"xmessage", "-center", full.c_str(), nullptr}},
    };

    for (const auto &tool : tools) {
        pid_t p = fork();
        if (p == 0) {
            execvp(tool.exe, const_cast<char *const *>(tool.args.data()));
            _exit(127);
        } else if (p > 0) {
            int status = 0;
            waitpid(p, &status, 0);
            // execvp returns 127 when the binary isn't found; 0 means the dialog was shown
            if (WIFEXITED(status) && WEXITSTATUS(status) != 127) return;
        }
    }
}
#endif

// ── Helpers ──────────────────────────────────────────────────────────────────

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
        } else if (arg == "--appimage-path" && i + 1 < argc) {
            out.appImagePath = fs::path(argv[++i]);
        } else if (arg == "--new-file" && i + 1 < argc) {
            out.newFilePath = fs::path(argv[++i]);
        } else if (arg == "--pid" && i + 1 < argc) {
            try { out.waitPid = std::stoi(argv[++i]); } catch (...) {}
        }
    }
    const bool directoryMode = out.install && !out.appDir.empty()
                                && !out.newDir.empty() && !out.exeName.empty();
    const bool appImageMode  = out.install && !out.appImagePath.empty()
                                && !out.newFilePath.empty();
    return directoryMode || appImageMode;
}

#if defined(_WIN32)
static void waitForAppExit(const fs::path &appDir, int /*pid*/) {
    HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Global\\TrenchKitRunning");
    if (hMutex) {
        appendLog(appDir, "Waiting for mutex release (app exit)...");
        const DWORD result = WaitForSingleObject(hMutex, 30000);
        CloseHandle(hMutex);
        if (result == WAIT_TIMEOUT)
            appendLog(appDir, "Timed out waiting for app mutex (30s). Proceeding anyway.", LogLevel::Warn);
        else
            appendLog(appDir, "Mutex released — app has exited.");
    } else {
        appendLog(appDir, "Mutex not found, assuming app already exited.");
    }
    Sleep(500);
}
#else
static void waitForAppExit(const fs::path &logDir, int pid) {
    if (pid > 0) {
        appendLog(logDir, "Waiting for PID " + std::to_string(pid) + " to exit...");
        int elapsed = 0;
        bool timedOut = true;
        for (; elapsed < 30; ++elapsed) {
            if (kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH) {
                timedOut = false;
                break;
            }
            sleep(1);
        }
        if (timedOut)
            appendLog(logDir, "Timed out waiting for PID " + std::to_string(pid)
                      + " after 30s. Proceeding anyway.", LogLevel::Warn);
        else
            appendLog(logDir, "PID " + std::to_string(pid) + " exited after "
                      + std::to_string(elapsed) + "s.");
    } else {
        appendLog(logDir, "No PID provided, waiting briefly...");
        sleep(1);
    }
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
        appendLog(to, "Failed to create target directory: " + ec.message(), LogLevel::Error);
        return false;
    }

    for (const auto &entry : fs::recursive_directory_iterator(from)) {
        const auto relPath = fs::relative(entry.path(), from, ec);
        if (ec) {
            appendLog(to, "Failed to resolve relative path: " + ec.message(), LogLevel::Error);
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
                appendLog(to, "Failed to create directory " + destPath.string() + ": " + ec.message(),
                          LogLevel::Error);
                return false;
            }
            continue;
        }
        if (entry.is_regular_file()) {
            fs::create_directories(destPath.parent_path(), ec);
            if (ec) {
                appendLog(to, "Failed to create parent directory " + destPath.parent_path().string()
                               + ": " + ec.message(), LogLevel::Error);
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
                    appendLog(to, "Skipping locked file: " + destPath.string(), LogLevel::Warn);
                    ec.clear();
                    continue;
                }
#endif
                if (ec) {
#if defined(_WIN32)
                    if (shouldSkipCopy(name)) {
                        appendLog(to, "Skipping copy failure for runtime file " + destPath.string()
                                       + ": " + ec.message(), LogLevel::Warn);
                        ec.clear();
                        continue;
                    }
#endif
                    appendLog(to, "Failed to copy " + entry.path().string() + " to "
                                   + destPath.string() + ": " + ec.message(), LogLevel::Error);
                    return false;
                }
            }
        }
    }

    return true;
}

static void removeOldAppFiles(const fs::path &appDir, const fs::path &newDir, const fs::path &helperName) {
    std::error_code ec;

    // Only remove entries that are actually part of the new release, so any
    // file or folder the user added next to the exe (e.g. a locally-tested
    // `locales/` folder, see src/locales/TRANSLATING.md) survives an update.
    std::vector<fs::path> newVersionEntries;
    for (const auto &entry : fs::directory_iterator(newDir, ec)) {
        newVersionEntries.push_back(entry.path().filename());
    }
    auto isShipped = [&](const fs::path &name) {
        for (const auto &shipped : newVersionEntries) {
#if defined(_WIN32)
            if (equalsIgnoreCase(name.wstring(), shipped.wstring())) return true;
#else
            if (name == shipped) return true;
#endif
        }
        return false;
    };

    for (const auto &entry : fs::directory_iterator(appDir)) {
        const fs::path name = entry.path().filename();
        if (isHelperExecutable(name, helperName)) {
            continue;
        }
#if defined(_WIN32)
        if (equalsIgnoreCase(name.wstring(), L"updates")) {
            continue;
        }
#else
        if (name == "updates") {
            continue;
        }
#endif
        if (!isShipped(name)) {
            continue;
        }
        fs::remove_all(entry.path(), ec);
        if (ec) {
            appendLog(appDir, "Failed to remove " + entry.path().string() + ": " + ec.message(),
                      LogLevel::Warn);
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
        std::cerr << "Usage:\n"
                  << "  Directory mode: TrenchKitUpdater --install --app-dir <dir> --new-dir <dir> "
                     "--exe-name <name> [--updates-dir <dir>] [--pid <pid>]\n"
                  << "  AppImage mode:  TrenchKitUpdater --install --appimage-path <file> "
                     "--new-file <file> [--updates-dir <dir>] [--pid <pid>]\n";
        return 1;
    }
    const fs::path helperName = helperExecutableName(argv[0]);

    if (!args.updatesDir.empty()) {
        g_updatesDir = args.updatesDir;
    }

    // Determine a path to use for logging
    const fs::path logDir = !args.appDir.empty() ? args.appDir
                          : !args.appImagePath.empty() ? args.appImagePath.parent_path()
                          : fs::path(".");

    // Resolve the log file path so we can include it in error dialogs
    const fs::path resolvedLog = (g_updatesDir.empty() ? (logDir / "updates") : g_updatesDir) / "updater.log";

    appendLog(logDir, "Updater started.");
    appendLog(logDir, "Log file: " + resolvedLog.string());

    // Log all arguments for diagnostics
    if (!args.appDir.empty())       appendLog(logDir, "  --app-dir       " + args.appDir.string());
    if (!args.newDir.empty())       appendLog(logDir, "  --new-dir       " + args.newDir.string());
    if (!args.updatesDir.empty())   appendLog(logDir, "  --updates-dir   " + args.updatesDir.string());
    if (!args.exeName.empty())      appendLog(logDir, "  --exe-name      "
                                              + std::string(args.exeName.begin(), args.exeName.end()));
    if (!args.appImagePath.empty()) appendLog(logDir, "  --appimage-path " + args.appImagePath.string());
    if (!args.newFilePath.empty())  appendLog(logDir, "  --new-file      " + args.newFilePath.string());
    if (args.waitPid > 0)           appendLog(logDir, "  --pid           " + std::to_string(args.waitPid));

#if !defined(_WIN32)
    // ── AppImage mode (Linux) ────────────────────────────────────────────────
    if (!args.appImagePath.empty()) {
        appendLog(logDir, "Mode: AppImage");
        appendLog(logDir, "Current AppImage: " + args.appImagePath.string());
        appendLog(logDir, "New AppImage:     " + args.newFilePath.string());

        waitForAppExit(logDir, args.waitPid);

        if (!fs::exists(args.newFilePath)) {
            logAndStderr(logDir, "New AppImage not found: " + args.newFilePath.string());
            showErrorDialog(resolvedLog, "Update failed: new AppImage not found at:\n" + args.newFilePath.string());
            return 1;
        }

        std::error_code ec;
        // Prefer atomic rename (works when src and dst are on the same filesystem)
        fs::rename(args.newFilePath, args.appImagePath, ec);
        if (ec) {
            appendLog(logDir, "rename failed (" + ec.message() + "), falling back to copy.");
            ec.clear();
            fs::copy_file(args.newFilePath, args.appImagePath,
                          fs::copy_options::overwrite_existing, ec);
            if (ec) {
                logAndStderr(logDir, "Failed to replace AppImage: " + ec.message());
                showErrorDialog(resolvedLog, "Update failed: could not replace the AppImage.\n" + ec.message());
                return 1;
            }
            fs::remove(args.newFilePath, ec);
        }

        // Ensure executable bit is set
        fs::permissions(args.appImagePath,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add, ec);
        if (ec) {
            appendLog(logDir, "Warning: failed to set AppImage executable permissions: " + ec.message(),
                      LogLevel::Warn);
        }

        appendLog(logDir, "Update applied. Relaunching: " + args.appImagePath.string());
        execl(args.appImagePath.c_str(), args.appImagePath.filename().c_str(), nullptr);
        logAndStderr(logDir, "execl failed — please relaunch manually.");
        showErrorDialog(resolvedLog, "Update applied, but TrenchKit failed to relaunch.\nPlease start it manually.");
        return 1;
    }
#endif

    // ── Directory mode ───────────────────────────────────────────────────────
    appendLog(logDir, "Mode: directory");
    appendLog(logDir, "App dir: " + args.appDir.string());
    appendLog(logDir, "New dir: " + args.newDir.string());
    appendLog(logDir, "Exe name: " + std::string(args.exeName.begin(), args.exeName.end()));

#if defined(_WIN32)
    waitForAppExit(args.appDir, args.waitPid);
#else
    waitForAppExit(logDir, args.waitPid);
#endif

    if (!fs::exists(args.newDir)) {
        logAndStderr(logDir, "New version directory not found: " + args.newDir.string());
#if defined(_WIN32)
        showErrorDialog(resolvedLog, L"Update failed: new version directory not found.");
#else
        showErrorDialog(resolvedLog, "Update failed: new version directory not found:\n" + args.newDir.string());
#endif
        return 1;
    }

    appendLog(logDir, "Removing old app files.");
    removeOldAppFiles(args.appDir, args.newDir, helperName);

    appendLog(logDir, "Copying new version files.");
    if (!copyRecursive(args.newDir, args.appDir, helperName)) {
        logAndStderr(logDir, "Failed to copy new version files.");
#if defined(_WIN32)
        showErrorDialog(resolvedLog, L"Update failed while copying new version files.");
#else
        showErrorDialog(resolvedLog, "Update failed while copying new version files.");
#endif
        return 1;
    }

#if defined(_WIN32)
    if (!launchApp(args.appDir, args.exeName)) {
        const fs::path exePath = args.appDir / args.exeName;
        logAndStderr(logDir, "Failed to relaunch application.");
        const std::wstring message = L"Update applied, but TrenchKit failed to relaunch.\n"
                                     L"Please start it manually from:\n" + exePath.wstring();
        showErrorDialog(resolvedLog, message);
        return 1;
    }
#else
    const fs::path exePath = args.appDir / std::string(args.exeName.begin(), args.exeName.end());

    // fs::copy_file does not preserve execute bits — set them explicitly.
    {
        std::error_code permEc;
        fs::permissions(exePath,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add, permEc);
        if (permEc) {
            logAndStderr(logDir, "Warning: failed to set executable permissions: " + permEc.message(),
                         LogLevel::Warn);
        }
    }

    // Also restore exec bit on the newly copied updater binary (sibling of the main exe).
    const fs::path updaterPath = exePath.parent_path() / "TrenchKitUpdater";
    if (fs::exists(updaterPath)) {
        std::error_code updPermEc;
        fs::permissions(updaterPath,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add, updPermEc);
        if (updPermEc) {
            logAndStderr(logDir, "Warning: failed to set updater permissions: " + updPermEc.message(),
                         LogLevel::Warn);
        }
    }

    appendLog(logDir, "Relaunching application: " + exePath.string());
    pid_t pid = fork();
    if (pid == 0) {
        execl(exePath.c_str(), exePath.filename().c_str(), nullptr);
        _exit(1);
    } else if (pid < 0) {
        logAndStderr(logDir, "Failed to fork for relaunch.");
        showErrorDialog(resolvedLog, "Update applied, but TrenchKit failed to relaunch.\nPlease start it manually.");
        return 1;
    }
#endif

    appendLog(logDir, "Update completed successfully.");
    return 0;
}
