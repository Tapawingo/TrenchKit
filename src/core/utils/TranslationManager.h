/**
 * @file TranslationManager.h
 * @brief Singleton that manages application translations from .ts files.
 */
#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QList>
#include <QObject>
#include <QTranslator>

class TsTranslator;

/**
 * @brief Singleton that discovers available languages and installs QTranslator instances.
 *
 * Call @c initialize() once at startup to scan the @c locales/ directory.
 * @c setLanguage() swaps out the installed translators on the QApplication.
 */
class TranslationManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Available language entry.
     */
    struct LanguageInfo {
        QString code;        ///< BCP 47 language code (e.g. "es", "fr").
        QString displayName; ///< Human-readable name for display in the settings UI.
    };

    static TranslationManager &instance();

    /**
     * @brief Scans @c locales/ for .ts files and populates the available language list.
     */
    void initialize();
    /**
     * @brief Installs translators for @p language (BCP 47 code); pass empty string for English.
     */
    void setLanguage(const QString &language);
    QString currentLanguage() const { return m_currentLanguage; }
    QList<LanguageInfo> availableLanguages() const { return m_availableLanguages; }
    /**
     * @brief Returns the path to the directory containing .ts locale files.
     */
    QString localesPath() const;

private:
    explicit TranslationManager(QObject *parent = nullptr);

    void discoverLanguages();
    void removeTranslators();
    bool installTranslators(const QString &locale);

    QTranslator *m_appTranslator = nullptr;
    QTranslator *m_qtTranslator = nullptr;
    TsTranslator *m_tsTranslator = nullptr;
    QString m_currentLanguage;
    QList<LanguageInfo> m_availableLanguages;
};

#endif // TRANSLATIONMANAGER_H
