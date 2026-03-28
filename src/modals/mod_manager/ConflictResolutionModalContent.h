/// @file ConflictResolutionModalContent.h
/// @brief Modal for resolving a pak file conflict during profile import.
#ifndef CONFLICTRESOLUTIONMODALCONTENT_H
#define CONFLICTRESOLUTIONMODALCONTENT_H

#include "common/modals/BaseModalContent.h"
#include "core/models/ModInfo.h"

class QLabel;
class QPushButton;

/// @brief Presents Ignore / Overwrite / Duplicate options when an imported profile mod
/// conflicts with an existing local mod.
///
/// @c checksumMatch is true when both pak files are bit-identical; in that case the
/// UI can suggest Ignore as the default action.
class ConflictResolutionModalContent : public BaseModalContent {
    Q_OBJECT

public:
    /// @brief Resolution action chosen by the user.
    enum class Action {
        Ignore,     ///< Keep the existing mod, discard the incoming one.
        Overwrite,  ///< Replace the existing mod with the incoming one.
        Duplicate   ///< Import the incoming mod alongside the existing one.
    };

    explicit ConflictResolutionModalContent(const ModInfo &incoming,
                                           const ModInfo &existing,
                                           bool checksumMatch,
                                           QWidget *parent = nullptr);

    /// @brief Returns the action the user selected; valid after @c finished() is emitted.
    Action selectedAction() const { return m_action; }

private:
    void setupUi();
    QString buildMessage() const;

    ModInfo m_incoming;
    ModInfo m_existing;
    bool m_checksumMatch = false;
    Action m_action = Action::Ignore;
};

#endif // CONFLICTRESOLUTIONMODALCONTENT_H
