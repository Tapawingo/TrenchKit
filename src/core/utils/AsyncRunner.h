/**
 * @file AsyncRunner.h
 * @brief Runs cancellable work on the thread pool and delivers the result to a QObject's thread.
 */
#ifndef ASYNCRUNNER_H
#define ASYNCRUNNER_H

#include "CancelToken.h"

#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrent/QtConcurrentRun>
#include <functional>
#include <memory>

/**
 * @brief Runs @p work on a worker thread and calls @p onFinished with its result on the caller's thread.
 *
 * @p onFinished is only invoked while @p context is alive. If @p context is destroyed first, the
 * token is cancelled and @p onAbandoned receives the result on a worker thread once the work has
 * stopped, so it can release whatever the work produced. @p onAbandoned must not touch UI or shared state.
 *
 * @param token Optional token to share with other stages; a new one is created if null.
 * @param futureOut Optional; receives the running work, so an owner that is shutting down can wait for it.
 * @returns The token; cancel it to make the work stop at its next checkpoint.
 */
template <typename T>
CancelTokenPtr runCancellable(QObject *context,
                              std::function<T(const CancelToken &)> work,
                              std::function<void(const T &)> onFinished,
                              std::function<void(const T &)> onAbandoned,
                              CancelTokenPtr token = {},
                              QFuture<T> *futureOut = nullptr) {
    if (!token) {
        token = std::make_shared<CancelToken>();
    }

    auto *watcher = new QFutureWatcher<T>(context);
    auto delivered = std::make_shared<bool>(false);

    QFuture<T> future = QtConcurrent::run([work = std::move(work), token]() -> T { return work(*token); });
    if (futureOut) {
        *futureOut = future;
    }

    QObject::connect(watcher, &QFutureWatcherBase::finished, context,
                     [watcher, delivered, onFinished = std::move(onFinished)] {
        *delivered = true;
        const T result = watcher->result();
        watcher->deleteLater();
        onFinished(result);
    });

    QObject::connect(watcher, &QObject::destroyed,
                     [future, delivered, token, onAbandoned = std::move(onAbandoned)]() mutable {
        if (*delivered) {
            return;
        }
        token->cancel();
        if (onAbandoned) {
            future.then([onAbandoned](T result) { onAbandoned(result); });
        }
    });

    watcher->setFuture(future);
    return token;
}

#endif // ASYNCRUNNER_H
