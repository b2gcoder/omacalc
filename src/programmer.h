#pragma once

#include <QList>
#include <QObject>
#include <QString>

// The Programmer calculator: whole numbers in two's complement at a chosen
// word size (8, 16, 32 or 64 bits), shown and typed in hex, decimal, octal
// or binary. Arithmetic wraps at the word size like the hardware does.
class ProgrammerCalc : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString expression READ expression NOTIFY changed)
    Q_PROPERTY(QString display READ display NOTIFY changed)
    Q_PROPERTY(QString hex READ hex NOTIFY changed)
    Q_PROPERTY(QString dec READ dec NOTIFY changed)
    Q_PROPERTY(QString oct READ oct NOTIFY changed)
    Q_PROPERTY(QString bin READ bin NOTIFY changed)
    Q_PROPERTY(int base READ base WRITE setBase NOTIFY changed)
    Q_PROPERTY(int wordBits READ wordBits WRITE setWordBits NOTIFY changed)
    // 0 arithmetic, 1 logical, 2 rotate, 3 rotate through carry.
    Q_PROPERTY(int shiftMode READ shiftMode WRITE setShiftMode NOTIFY changed)
    Q_PROPERTY(bool carry READ carry NOTIFY changed)
    Q_PROPERTY(bool errored READ errored NOTIFY changed)
    Q_PROPERTY(int openParens READ openParens NOTIFY changed)

public:
    explicit ProgrammerCalc(QObject *parent = nullptr);

    QString expression() const;
    QString display() const;
    QString hex() const { return format(currentBits(), 16); }
    QString dec() const { return format(currentBits(), 10); }
    QString oct() const { return format(currentBits(), 8); }
    QString bin() const { return format(currentBits(), 2); }
    int base() const { return m_base; }
    void setBase(int base);
    int wordBits() const { return m_wordBits; }
    void setWordBits(int bits);
    int shiftMode() const { return m_shiftMode; }
    void setShiftMode(int mode);
    bool carry() const { return m_carry; }
    bool errored() const { return m_errored; }
    int openParens() const;

    // Digits 0–9 and A–F (those valid in the current base), "+", "-", "*",
    // "/", "%", "and", "or", "xor", "nand", "nor", "lsh", "rsh", "(", ")",
    // "=", "not", "sign", "backspace", "clear", "clearEntry", "word"
    // (cycles the word size).
    Q_INVOKABLE void pressKey(const QString &key);
    Q_INVOKABLE bool bit(int index) const;
    Q_INVOKABLE void toggleBit(int index);
    Q_INVOKABLE void copyResult() const;
    Q_INVOKABLE void paste();
    Q_INVOKABLE bool digitAllowed(const QString &digit) const;

    // Raw bits of the value on the display, masked to the word size.
    quint64 currentBits() const;
    qint64 currentSigned() const { return toSigned(currentBits()); }
    void enterBits(quint64 bits);

    quint64 mask() const;
    qint64 toSigned(quint64 bits) const;
    QString format(quint64 bits, int base, bool grouped = true) const;

signals:
    void changed();
    void evaluated(const QString &expression, const QString &result, double value);

private:
    struct Token {
        enum Kind { Number, Operator, Open, Close } kind;
        quint64 value = 0;
        QString op;
    };

    void pressDigit(const QString &digit);
    void pressOperator(const QString &op);
    void pressEquals();
    void pressBackspace();
    void pressUnary(const QString &kind);
    void clearAll();
    void commitEntry();
    bool parseEntry(const QString &text, quint64 *bits) const;
    quint64 evaluate(QList<Token> tokens, bool *ok, QString *error, bool *carry) const;
    quint64 apply(const QString &op, quint64 a, quint64 b, bool *ok, QString *error, bool *carry) const;
    QString prettyTokens(const QList<Token> &tokens) const;
    QList<Token> finalTokens() const;

    QList<Token> m_tokens;
    QString m_entry;             // digits being typed, in the current base
    bool m_hasEntryValue = false;  // m_entryBits came from NOT, ±, a bit flip or a result
    quint64 m_entryBits = 0;
    quint64 m_resultBits = 0;
    QString m_lastExpression;
    QString m_error;
    bool m_justEvaluated = false;
    bool m_errored = false;
    bool m_carry = false;
    int m_base = 10;
    int m_wordBits = 64;
    int m_shiftMode = 0;
};
