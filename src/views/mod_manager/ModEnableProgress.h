/**
 * @file ModEnableProgress.h
 * @brief Busy indication for background mod-enable jobs: a cancellable progress modal or a busy cursor.
 */
#ifndef MODENABLEPROGRESS_H
#define MODENABLEPROGRESS_H

#include "core/utils/CancelToken.h"
#include <QString>
#include <functional>

class ModManager;
class ModalManager;

/**
 * @brief Wraps an asynchronous enable job (see @c ModManager::setModsEnabledAsync()) with UI feedback.
 */
class ModEnableProgress {
public:
    /// Call this from the job's completion handler to dismiss the indication.
    using Done = std::function<void()>;

    /**
     * @brief Shows feedback while the job started by @p start runs.
     *
     * For more than one mod a progress modal with a Cancel button follows @c ModManager::enableProgress();
     * for a single mod a busy cursor is shown. @p start must begin the job, invoke the given @c Done
     * when it finishes, and return the job's cancel token.
     */
    static void run(ModManager *manager, ModalManager *modals, int modCount, const QString &label,
                    const std::function<CancelTokenPtr(Done)> &start);
};

#endif // MODENABLEPROGRESS_H
