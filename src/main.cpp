#include <QApplication>
#include "MainWindow.h"
#include "core/utils/UpdateCleanup.h"
#include "core/utils/Logger.h"
#include "core/utils/TranslationManager.h"
#include <QTimer>
#include <QCoreApplication>
#include <QThreadPool>
#ifdef _WIN32
#include <windows.h>
#endif

static int run(int argc, char *argv[]);

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return run(__argc, __argv);
}
#endif

int main(int argc, char *argv[]) {
    return run(argc, argv);
}

static int run(int argc, char *argv[]) {
#ifdef _WIN32
    // Create named mutex to signal app is running (used by updater to wait for exit)
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"Global\\TrenchKitRunning");
#endif

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("TrenchKit"));
    QCoreApplication::setApplicationName(QStringLiteral("TrenchKit"));

    Logger::instance().initialize();
    qInstallMessageHandler(Logger::messageHandler);

    qInfo() << "TrenchKit version:" << TRENCHKIT_VERSION;
    qInfo() << "Qt version:" << QT_VERSION_STR;
    qInfo() << "Log directory:" << Logger::instance().logDirectory();

    TranslationManager::instance().initialize();

    // Set application icon (for taskbar, alt-tab, etc.)
    app.setWindowIcon(QIcon(":/icon.png"));

    MainWindow w;
    w.show();

    QTimer::singleShot(0, &app, []() {
        QThreadPool::globalInstance()->start([]() { UpdateCleanup::run(); });
    });

    if (app.arguments().contains("--smoke-test")) {
        QTimer::singleShot(200, &app, &QCoreApplication::quit);
    }

    int result = app.exec();

#ifdef _WIN32
    if (hMutex) {
        CloseHandle(hMutex);
    }
#endif

    Logger::instance().shutdown();
    return result;
}
