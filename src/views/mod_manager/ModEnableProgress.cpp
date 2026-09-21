#include "ModEnableProgress.h"
#include "common/modals/ModalManager.h"
#include "common/modals/ProgressModal.h"
#include "core/managers/ModManager.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QPointer>
#include <memory>

namespace {

/// Shared by the completion handler and the Cancel button; cleans up even if the handler never runs.
struct Busy {
    QPointer<ProgressModal> modal;
    bool userCancelled = false;
    bool cursorSet = false;

    void release() {
        if (modal && !userCancelled) {
            modal->accept();
        }
        modal.clear();
        if (cursorSet) {
            cursorSet = false;
            if (QGuiApplication::instance()) {
                QGuiApplication::restoreOverrideCursor();
            }
        }
    }

    ~Busy() {
        if (QGuiApplication::instance()) {
            release();
        }
    }
};

} // namespace

void ModEnableProgress::run(ModManager *manager, ModalManager *modals, int modCount, const QString &label,
                            const std::function<CancelTokenPtr(Done)> &start) {
    auto busy = std::make_shared<Busy>();

    if (modCount > 1 && modals && manager) {
        auto *modal = new ProgressModal(label, QCoreApplication::translate("ModEnableProgress", "Cancel"),
                                        0, modCount);
        busy->modal = modal;
        QObject::connect(manager, &ModManager::enableProgress, modal, [modal](int done, int total) {
            modal->setRange(0, total);
            modal->setValue(done);
        });
        modals->showModal(modal);
    } else {
        QGuiApplication::setOverrideCursor(Qt::BusyCursor);
        busy->cursorSet = true;
    }

    const CancelTokenPtr token = start([busy]() { busy->release(); });

    // A job that finished at once has already dismissed the modal.
    if (busy->modal && token) {
        QObject::connect(busy->modal, &ProgressModal::canceled, busy->modal, [token, busy]() {
            busy->userCancelled = true;
            token->cancel();
        });
    }
}
