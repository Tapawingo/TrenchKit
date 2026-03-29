/**
 * @file Logger.h
 * @brief Singleton file logger with automatic rotation, installed as the Qt message handler.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QString>
#include <QDateTime>

/**
 * @brief Thread-safe singleton that writes Qt log messages to a rotating log file.
 *
 * Call @c initialize() once at startup to open the log file and install
 * @c messageHandler as the Qt message handler via @c qInstallMessageHandler.
 * Logs rotate at 10 MB and are purged after 7 days.
 */
class Logger : public QObject {
    Q_OBJECT

public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Critical
    };

    static Logger& instance();

    /**
     * @brief Opens the log file under AppData/TrenchKit/logs/ and installs the Qt message handler.
     */
    void initialize();
    /**
     * @brief Flushes and closes the log file; restores the default Qt message handler.
     */
    void shutdown();

    QString logDirectory() const;
    QString currentLogFile() const;

    /**
     * @brief Qt message handler; install via @c qInstallMessageHandler(Logger::messageHandler).
     */
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private:
    Logger();
    ~Logger() override;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void writeToFile(const QString &message);
    /**
     * @brief Rotates the log file when it exceeds @c m_maxFileSize bytes.
     */
    void rotateIfNeeded();
    /**
     * @brief Deletes log files older than @c m_maxDays days.
     */
    void cleanOldLogs();
    QString formatMessage(QtMsgType type, const QString &msg, const QMessageLogContext &context);
    QString logLevelToString(QtMsgType type);

    QMutex m_mutex;
    QFile m_logFile;
    QTextStream m_stream;
    QString m_logDir;
    qint64 m_maxFileSize = 10 * 1024 * 1024; ///< Rotate threshold (10 MB).
    int m_maxDays = 7;                        ///< Purge logs older than this many days.
};

#endif // LOGGER_H
