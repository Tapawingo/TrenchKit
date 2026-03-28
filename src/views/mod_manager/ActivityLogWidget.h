/// @file ActivityLogWidget.h
/// @brief Scrollable activity log panel showing recent mod-manager operations.
#ifndef ACTIVITYLOGWIDGET_H
#define ACTIVITYLOGWIDGET_H

#include <QWidget>
#include <QString>
#include <QEvent>
#include "LogEntryWidget.h"

class QLabel;
class QListWidget;

/// @brief Displays a capped list of @c LogEntryWidget rows for recent activity.
///
/// Automatically removes the oldest entry when the list exceeds @c MAX_LOG_ENTRIES.
class ActivityLogWidget : public QWidget {
    Q_OBJECT

public:
    using LogLevel = LogEntryWidget::LogLevel;

    explicit ActivityLogWidget(QWidget *parent = nullptr);
    ~ActivityLogWidget() override = default;

    /// @brief Appends a new log row; oldest rows are discarded beyond @c MAX_LOG_ENTRIES.
    void addLogEntry(const QString &message, LogLevel level = LogLevel::Info);
    void clearLog();

protected:
    void changeEvent(QEvent *event) override;

private:
    void setupUi();
    void retranslateUi();

    static constexpr int MAX_LOG_ENTRIES = 1000; ///< Maximum number of rows kept in the log.

    QLabel *m_titleLabel = nullptr;
    QListWidget *m_logList;
};

#endif // ACTIVITYLOGWIDGET_H
