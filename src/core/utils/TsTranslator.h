/**
 * @file TsTranslator.h
 * @brief QTranslator subclass that loads .ts XML files directly without .qm compilation.
 */
#ifndef TSTRANSLATOR_H
#define TSTRANSLATOR_H

#include <QHash>
#include <QString>
#include <QTranslator>

/**
 * @brief Loads and serves translations from a Qt .ts XML file at runtime.
 *
 * Avoids the need to compile .ts files to .qm binaries. Loaded via
 * @c TranslationManager; not intended for direct use.
 */
class TsTranslator : public QTranslator {
    Q_OBJECT

public:
    explicit TsTranslator(QObject *parent = nullptr);

    /**
     * @brief Parses @p filePath as a Qt .ts XML file; returns false on parse error.
     */
    bool loadTsFile(const QString &filePath);

    /**
     * @brief Overrides @c QTranslator::translate() to serve strings from the loaded .ts file.
     */
    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override;
    bool isEmpty() const override;
    QString language() const;

private:
    /**
     * @brief Composite lookup key for a single message entry.
     */
    struct MessageKey {
        QString context;
        QString source;
        QString disambiguation;

        bool operator==(const MessageKey &other) const = default;
    };

    /**
     * @brief Translation value for a single message entry.
     */
    struct MessageValue {
        QString translation;
        QStringList pluralForms; ///< Indexed plural forms; used when @c isPlural is true.
        bool isPlural = false;
    };

    friend size_t qHash(const TsTranslator::MessageKey &key, size_t seed);

    QHash<MessageKey, MessageValue> m_messages;
    QString m_language;
};

inline size_t qHash(const TsTranslator::MessageKey &key, size_t seed = 0) {
    return qHash(key.context, seed) ^ qHash(key.source, seed) ^ qHash(key.disambiguation, seed);
}

#endif // TSTRANSLATOR_H
