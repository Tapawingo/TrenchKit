/**
 * @file CancelToken.h
 * @brief Thread-safe flag for cancelling long-running background work.
 */
#ifndef CANCELTOKEN_H
#define CANCELTOKEN_H

#include <atomic>
#include <memory>

/**
 * @brief Set by the UI thread, polled by the worker; work stops at its next checkpoint.
 */
class CancelToken {
public:
    void cancel() { m_cancelled.store(true, std::memory_order_relaxed); }
    [[nodiscard]] bool isCancelled() const { return m_cancelled.load(std::memory_order_relaxed); }

private:
    std::atomic_bool m_cancelled{false};
};

using CancelTokenPtr = std::shared_ptr<CancelToken>;

#endif // CANCELTOKEN_H
