/**
 * @file BaseModalContent.h
 * @brief Base class for all modal dialog content widgets.
 */
#ifndef BASEMODALCONTENT_H
#define BASEMODALCONTENT_H

#include <QWidget>
#include <QSize>

class QLabel;
class QVBoxLayout;
class QHBoxLayout;

/**
 * @brief Abstract base for content shown inside a @c ModalOverlay.
 *
 * Subclasses populate @c bodyLayout() and @c footerLayout() in their
 * constructor, then emit @c finished(result) when done. Do not parent a
 * @c BaseModalContent to any widget — pass it directly to
 * @c ModalManager::showModal().
 */
class BaseModalContent : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Standardized result codes passed to @c finished().
     */
    enum class Result {
        Rejected = 0, ///< User cancelled or closed the dialog.
        Accepted = 1, ///< User confirmed the primary action.
        Custom   = 2  ///< Subclass-defined result (check subclass docs).
    };

    explicit BaseModalContent(QWidget *parent = nullptr);
    ~BaseModalContent() override = default;

    Result result() const { return m_result; }

    void setTitle(const QString &title);
    void setHeaderVisible(bool visible);

    /**
     * @brief Hints the preferred initial size to @c ModalOverlay; default is 500×400.
     */
    void setPreferredSize(const QSize &size) { m_preferredSize = size; }
    QSize preferredSize() const { return m_preferredSize; }

public slots:
    virtual void accept();
    virtual void reject();

signals:
    void accepted();
    void rejected();
    /**
     * @brief Emitted when the modal is done; @p result is one of the @c Result enum values.
     */
    void finished(int result);

protected:
    void showEvent(QShowEvent *event) override;

    /**
     * @brief Layout for the main content area; add widgets here in subclasses.
     */
    QVBoxLayout* bodyLayout() { return m_bodyLayout; }
    /**
     * @brief Layout for action buttons at the bottom; add buttons here in subclasses.
     */
    QHBoxLayout* footerLayout() { return m_footerLayout; }
    QWidget* headerWidget() { return m_headerWidget; }
    QWidget* bodyWidget() { return m_bodyWidget; }
    QWidget* footerWidget() { return m_footerWidget; }

    void setResult(Result result) { m_result = result; }

private:
    void setupLayout();

    Result m_result = Result::Rejected;
    QLabel *m_titleLabel;
    QWidget *m_headerWidget;
    QWidget *m_bodyWidget;
    QWidget *m_footerWidget;
    QVBoxLayout *m_bodyLayout;
    QHBoxLayout *m_footerLayout;
    QSize m_preferredSize = QSize(500, 400);
};

#endif // BASEMODALCONTENT_H
