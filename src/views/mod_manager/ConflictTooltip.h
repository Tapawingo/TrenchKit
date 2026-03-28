/// @file ConflictTooltip.h
/// @brief Custom tooltip widget used to display conflict and notice details on mod rows.
#ifndef CONFLICTTOOLTIP_H
#define CONFLICTTOOLTIP_H

#include "common/widgets/GradientFrame.h"

class QLabel;

/// @brief Styled popup that shows a text message near the widget that triggered it.
class ConflictTooltip : public GradientFrame {
    Q_OBJECT

public:
    explicit ConflictTooltip(QWidget *parent = nullptr);
    void setText(const QString &text);
    /// @brief Positions and shows the tooltip near @p globalPos.
    void showAt(const QPoint &globalPos);

private:
    QLabel *m_label = nullptr;
};

#endif // CONFLICTTOOLTIP_H
