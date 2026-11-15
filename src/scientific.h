#pragma once

#include <QObject>
#include <QString>

// The Scientific calculator. Keys build an expression as text, the way it
// reads on the expression line ("2 × sin(30) + 5²"), and = hands it to
// Expression. Function keys act on the number or group just entered, so
// 30 then sin gives sin(30), and with nothing entered they open a call.
class ScientificCalc : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString expression READ expression NOTIFY changed)
    Q_PROPERTY(QString display READ display NOTIFY changed)
    Q_PROPERTY(bool errored READ errored NOTIFY changed)
    Q_PROPERTY(int openParens READ openParens NOTIFY changed)
    // 0 degrees, 1 radians, 2 gradians, as in Expression::AngleUnit.
    Q_PROPERTY(int angleUnit READ angleUnit WRITE setAngleUnit NOTIFY changed)
    Q_PROPERTY(bool scientificNotation READ scientificNotation WRITE setScientificNotation NOTIFY changed)

public:
    explicit ScientificCalc(QObject *parent = nullptr);

    QString expression() const;
    QString display() const;
    bool errored() const { return m_errored; }
    int openParens() const;
    int angleUnit() const { return m_angleUnit; }
    void setAngleUnit(int unit);
    bool scientificNotation() const { return m_scientificNotation; }
    void setScientificNotation(bool on);

    // The raw text, for tests and for copying the expression.
    QString text() const { return m_text; }

    // Digits, ".", "+", "-", "*", "/", "^", "mod", "root", "logb", "(", ")",
    // "=", "backspace", "clear", "clearEntry", "sign", "exp" (the E of
    // scientific notation), "pi", "e", "rand", "angle" (cycles the unit),
    // "fe" (toggles notation).
    Q_INVOKABLE void pressKey(const QString &key);
    // Functions that wrap what was just entered: "sin" … "acoth", "sqrt",
    // "cbrt", "abs", "floor", "ceil", "ln", "log", "exp", "dms", "deg", and
    // "square", "cube", "reciprocal", "factorial", "tenPow", "twoPow", "ePow".
    Q_INVOKABLE void applyFunction(const QString &name);

    Q_INVOKABLE double currentNumber() const;
    Q_INVOKABLE void enterNumber(double value);
    Q_INVOKABLE void copyResult() const;
    Q_INVOKABLE void paste();
    // Appends any expression Expression understands, for paste and tests.
    Q_INVOKABLE bool insertText(const QString &text);

    QString formatValue(double value) const;
    static QString literal(double value);

signals:
    void changed();
    void evaluated(const QString &expression, const QString &result, double value);

private:
    void startFresh();
    void continueFromResult();
    void pressDigit(const QString &digit);
    void pressDecimal();
    void pressOperator(const QString &op);
    void pressOpenParen();
    void pressCloseParen();
    void pressEquals();
    void pressBackspace();
    void pressClearEntry();
    void pressSign();
    void pressExp();
    void insertOperand(const QString &operand);

    bool endsWithOperand() const;
    bool endsWithOperator() const;
    int trailingNumberStart() const;
    int trailingOperandStart() const;
    QString completedText() const;
    double preview(bool *ok) const;
    QString pretty(const QString &text) const;

    QString m_text;
    QString m_lastExpression;
    QString m_error;
    double m_resultValue = 0;
    bool m_justEvaluated = false;
    bool m_errored = false;
    int m_angleUnit = 0;
    bool m_scientificNotation = false;
};
