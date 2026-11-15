#include "scientific.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHash>
#include <QLocale>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <cmath>

#include "backend.h"
#include "expression.h"

namespace {
const QString minus = QStringLiteral("−");

// Binary operators sit between spaces in the text, which is how backspace and
// the "expecting a number" checks tell them from a number's own minus sign.
const QHash<QString, QString> &operatorText() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("+"), QStringLiteral(" + ")}, {QStringLiteral("-"), QStringLiteral(" − ")},
        {QStringLiteral("−"), QStringLiteral(" − ")}, {QStringLiteral("*"), QStringLiteral(" × ")},
        {QStringLiteral("×"), QStringLiteral(" × ")}, {QStringLiteral("/"), QStringLiteral(" ÷ ")},
        {QStringLiteral("÷"), QStringLiteral(" ÷ ")}, {QStringLiteral("^"), QStringLiteral(" ^ ")},
        {QStringLiteral("mod"), QStringLiteral(" mod ")}, {QStringLiteral("root"), QStringLiteral(" root ")},
        {QStringLiteral("logb"), QStringLiteral(" logb ")},
    };
    return map;
}

const QRegularExpression &trailingOperatorPattern() {
    static const QRegularExpression pattern(QStringLiteral("\\s(?:[+−×÷^]|mod|root|logb)\\s$"));
    return pattern;
}

const QRegularExpression &trailingCallPattern() {
    static const QRegularExpression pattern(QStringLiteral("(?:[a-z]+2?|√|∛)\\($"));
    return pattern;
}

// The start of the number literal that ends at `end`, or -1. "5", "5.",
// "0.25", "2E", "2E−", and "2E−7" all count, so typing can continue.
int numberStartBefore(const QString &text, int end) {
    static const QRegularExpression number(QStringLiteral("(?:\\d+\\.?\\d*|\\.\\d+)(?:E[−+]?\\d*)?$"));
    const QRegularExpressionMatch match = number.match(text.left(end));
    if (!match.hasMatch())
        return -1;
    const int start = match.capturedStart();
    // The 2 in log2 belongs to the name.
    if (start > 0 && text.at(start - 1).isLetter())
        return -1;
    return start;
}
}

ScientificCalc::ScientificCalc(QObject *parent) : QObject(parent) {}

QString ScientificCalc::expression() const {
    if (m_errored || m_justEvaluated)
        return m_lastExpression + QStringLiteral(" =");
    return pretty(m_text);
}

QString ScientificCalc::display() const {
    if (m_errored)
        return m_error;
    if (m_justEvaluated)
        return formatValue(m_resultValue);
    if (m_text.isEmpty())
        return QStringLiteral("0");

    // While a number is being typed, show it as typed ("0.50", "2E−").
    const int start = numberStartBefore(m_text, m_text.size());
    if (start >= 0) {
        QString typed = m_text.mid(start);
        if (start > 0 && m_text.at(start - 1) == minus[0]
                && (start == 1 || m_text.at(start - 2) == QLatin1Char(' ') || m_text.at(start - 2) == QLatin1Char('(')))
            typed.prepend(minus);
        return Backend::groupDigits(typed.replace(minus, QStringLiteral("-"))).replace(QLatin1Char('-'), minus);
    }

    bool ok = false;
    const double value = preview(&ok);
    return ok ? formatValue(value) : QStringLiteral("0");
}

int ScientificCalc::openParens() const {
    if (m_justEvaluated || m_errored)
        return 0;
    return int(m_text.count(QLatin1Char('('))) - int(m_text.count(QLatin1Char(')')));
}

void ScientificCalc::setAngleUnit(int unit) {
    unit = qBound(0, unit, 2);
    if (unit == m_angleUnit)
        return;
    m_angleUnit = unit;
    emit changed();
}

void ScientificCalc::setScientificNotation(bool on) {
    if (on == m_scientificNotation)
        return;
    m_scientificNotation = on;
    emit changed();
}

// ---- Reading the text --------------------------------------------------

// Waiting for a number: at the start, after an operator, after "(" or a
// function's opening, after a negative sign, or inside an exponent.
bool ScientificCalc::endsWithOperator() const {
    if (m_text.isEmpty())
        return true;
    const QChar last = m_text.back();
    return last == QLatin1Char(' ') || last == QLatin1Char('(') || last == minus[0]
        || last == QLatin1Char('E') || last == QLatin1Char('^');
}

bool ScientificCalc::endsWithOperand() const {
    return !endsWithOperator();
}

int ScientificCalc::trailingNumberStart() const {
    if (m_text.isEmpty())
        return -1;
    return numberStartBefore(m_text, m_text.size());
}

// Where the last complete operand starts: a number, a constant, a
// parenthesized group or function call, with any !, ², ³ after it and a
// negative sign attached in front.
int ScientificCalc::trailingOperandStart() const {
    if (endsWithOperator())
        return -1;
    int i = m_text.size();
    while (i > 0 && QStringLiteral("!²³").contains(m_text.at(i - 1)))
        --i;
    if (i == 0)
        return -1;

    const QChar last = m_text.at(i - 1);
    if (last == QLatin1Char(')')) {
        int depth = 0;
        int j = i - 1;
        for (; j >= 0; --j) {
            if (m_text.at(j) == QLatin1Char(')'))
                ++depth;
            else if (m_text.at(j) == QLatin1Char('(') && --depth == 0)
                break;
        }
        if (j < 0)
            return -1;
        i = j;
        if (m_text.left(i).endsWith(QStringLiteral("log2")))
            i -= 4;
        else
            while (i > 0 && (m_text.at(i - 1).isLetter() || m_text.at(i - 1) == QStringLiteral("√")[0]
                             || m_text.at(i - 1) == QStringLiteral("∛")[0]))
                --i;
    } else if (last.isDigit() || last == QLatin1Char('.')) {
        i = numberStartBefore(m_text, i);
        if (i < 0)
            return -1;
    } else if (last == QStringLiteral("π")[0] || last == QLatin1Char('e')) {
        --i;
    } else {
        return -1;
    }

    if (i > 0 && m_text.at(i - 1) == minus[0]
            && (i == 1 || m_text.at(i - 2) == QLatin1Char(' ') || m_text.at(i - 2) == QLatin1Char('(')))
        --i;
    return i;
}

// The text as = would read it: anything left dangling at the end dropped and
// open parentheses closed, so "2 × (3 + " previews as 2 × (3).
QString ScientificCalc::completedText() const {
    QString text = m_text;
    for (;;) {
        const QString before = text;
        const QRegularExpressionMatch op = trailingOperatorPattern().match(text);
        if (op.hasMatch())
            text.truncate(op.capturedStart());
        const QRegularExpressionMatch call = trailingCallPattern().match(text);
        if (call.hasMatch())
            text.truncate(call.capturedStart());
        while (!text.isEmpty() && (text.back() == QLatin1Char('(') || text.back() == minus[0]
                                   || text.back() == QLatin1Char('E') || text.back() == QLatin1Char('^')
                                   || text.back() == QLatin1Char(' ')))
            text.chop(1);
        if (text == before)
            break;
    }
    const int open = int(text.count(QLatin1Char('('))) - int(text.count(QLatin1Char(')')));
    for (int i = 0; i < open; ++i)
        text += QLatin1Char(')');
    return text;
}

double ScientificCalc::preview(bool *ok) const {
    const QString text = completedText();
    *ok = false;
    if (text.isEmpty())
        return 0;
    QString error;
    const double value = Expression::evaluate(text, Expression::AngleUnit(m_angleUnit), &error);
    *ok = error.isEmpty() && std::isfinite(value);
    return value;
}

// Long literals carried over from a result show at display precision, whole
// numbers get thousands separators, and inverse functions read as sin⁻¹.
QString ScientificCalc::pretty(const QString &text) const {
    static const QRegularExpression number(QStringLiteral("(?<![A-Za-z\\d.])(\\d+\\.?\\d*|\\.\\d+)(E[−+]?\\d+)?"));
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = number.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        out += text.mid(last, match.capturedStart() - last);
        QString literal = match.captured(0);
        int digits = 0;
        for (const QChar c : match.captured(1))
            digits += c.isDigit() ? 1 : 0;
        if (digits > 15) {
            QString plain = literal;
            plain.replace(minus, QStringLiteral("-"));
            literal = Backend::formatNumber(plain.toDouble()).replace(QLatin1Char('e'), QLatin1Char('E'));
        }
        if (!literal.contains(QLatin1Char('E')))
            literal = Backend::groupDigits(literal);
        out += literal;
        last = match.capturedEnd();
    }
    out += text.mid(last);

    static const QRegularExpression inverse(QStringLiteral("\\ba(sin|cos|tan|sec|csc|cot)(h?)\\("));
    out.replace(inverse, QStringLiteral("\\1\\2⁻¹("));
    return out;
}

QString ScientificCalc::formatValue(double value) const {
    if (value == 0)
        value = 0;
    if (m_scientificNotation) {
        QString text = QString::number(value, 'e', 14);
        const int e = text.indexOf(QLatin1Char('e'));
        QString mantissa = text.left(e);
        if (mantissa.contains(QLatin1Char('.'))) {
            while (mantissa.endsWith(QLatin1Char('0')))
                mantissa.chop(1);
            if (mantissa.endsWith(QLatin1Char('.')))
                mantissa.chop(1);
        }
        const int exponent = text.mid(e + 1).toInt();
        return (mantissa + QStringLiteral("E") + (exponent < 0 ? QStringLiteral("−") : QStringLiteral("+"))
                + QString::number(std::abs(exponent)))
            .replace(QLatin1Char('-'), minus);
    }
    QString text = Backend::formatNumber(value);
    const int e = text.indexOf(QLatin1Char('e'));
    if (e < 0)
        return Backend::groupDigits(text).replace(QLatin1Char('-'), minus);
    // 2e-05 reads as 2E−5.
    const int exponent = text.mid(e + 1).toInt();
    text = text.left(e) + QStringLiteral("E") + (exponent < 0 ? minus : QStringLiteral("+"))
        + QString::number(std::abs(exponent));
    return text.replace(QLatin1Char('-'), minus);
}

// Seventeen significant digits round-trip a double, so 1 ÷ 3 = × 3 is 1.
QString ScientificCalc::literal(double value) {
    if (value == 0)
        value = 0;
    QString text = QString::number(value, 'g', 17);
    text.replace(QLatin1Char('e'), QLatin1Char('E'));
    text.replace(QStringLiteral("E+"), QStringLiteral("E"));
    text.replace(QLatin1Char('-'), minus);
    return text;
}

// ---- Keys --------------------------------------------------------------

void ScientificCalc::startFresh() {
    m_text.clear();
    m_justEvaluated = false;
    m_errored = false;
    m_error.clear();
    m_lastExpression.clear();
}

void ScientificCalc::continueFromResult() {
    const double value = m_resultValue;
    startFresh();
    m_text = literal(value);
}

void ScientificCalc::pressKey(const QString &key) {
    if (key.size() == 1 && key.at(0).isDigit())
        pressDigit(key);
    else if (key == QStringLiteral("."))
        pressDecimal();
    else if (operatorText().contains(key))
        pressOperator(key);
    else if (key == QStringLiteral("("))
        pressOpenParen();
    else if (key == QStringLiteral(")"))
        pressCloseParen();
    else if (key == QStringLiteral("="))
        pressEquals();
    else if (key == QStringLiteral("backspace"))
        pressBackspace();
    else if (key == QStringLiteral("clear"))
        startFresh();
    else if (key == QStringLiteral("clearEntry"))
        pressClearEntry();
    else if (key == QStringLiteral("sign"))
        pressSign();
    else if (key == QStringLiteral("exp"))
        pressExp();
    else if (key == QStringLiteral("pi"))
        insertOperand(QStringLiteral("π"));
    else if (key == QStringLiteral("e"))
        insertOperand(QStringLiteral("e"));
    else if (key == QStringLiteral("rand"))
        insertOperand(literal(QRandomGenerator::global()->generateDouble()));
    else if (key == QStringLiteral("angle"))
        setAngleUnit((m_angleUnit + 1) % 3);
    else if (key == QStringLiteral("fe"))
        setScientificNotation(!m_scientificNotation);
    else
        return;
    emit changed();
}

void ScientificCalc::insertOperand(const QString &operand) {
    if (m_errored || m_justEvaluated)
        startFresh();
    if (endsWithOperand())
        m_text += QStringLiteral(" × ");
    m_text += operand;
}

void ScientificCalc::pressDigit(const QString &digit) {
    if (m_errored || m_justEvaluated)
        startFresh();
    const int start = trailingNumberStart();
    if (start < 0) {
        if (endsWithOperand())
            m_text += QStringLiteral(" × ");
        m_text += digit;
        return;
    }
    const QString number = m_text.mid(start);
    if (number == QStringLiteral("0")) {
        m_text.replace(start, 1, digit);
        return;
    }
    // Fifteen significant digits, as on the Standard keypad; an exponent
    // takes up to three.
    const int e = number.indexOf(QLatin1Char('E'));
    if (e >= 0) {
        int exponentDigits = 0;
        for (const QChar c : number.mid(e))
            exponentDigits += c.isDigit() ? 1 : 0;
        if (exponentDigits < 3)
            m_text += digit;
        return;
    }
    QString mantissa = number;
    if (mantissa.startsWith(QStringLiteral("0.")))
        mantissa.remove(0, 1);
    int digits = 0;
    for (const QChar c : mantissa)
        digits += c.isDigit() ? 1 : 0;
    if (digits < 15)
        m_text += digit;
}

void ScientificCalc::pressDecimal() {
    if (m_errored || m_justEvaluated)
        startFresh();
    const int start = trailingNumberStart();
    if (start < 0) {
        if (endsWithOperand())
            m_text += QStringLiteral(" × ");
        m_text += QStringLiteral("0.");
        return;
    }
    const QString number = m_text.mid(start);
    if (!number.contains(QLatin1Char('.')) && !number.contains(QLatin1Char('E')))
        m_text += QLatin1Char('.');
}

void ScientificCalc::pressOperator(const QString &key) {
    if (m_errored)
        return;
    if (m_justEvaluated)
        continueFromResult();

    const bool isMinus = key == QStringLiteral("-") || key == minus;
    if (m_text.endsWith(QLatin1Char('E'))) {
        if (isMinus)
            m_text += minus;
        return;
    }
    if (m_text.isEmpty()) {
        if (isMinus) {
            m_text = minus;
            return;
        }
        m_text = QStringLiteral("0");
    }
    const QChar last = m_text.back();
    if (last == QLatin1Char('(') || last == QLatin1Char('^')) {
        // A negative number can follow "(" or 10^; another operator can't.
        if (isMinus)
            m_text += minus;
        return;
    }
    if (last == minus[0])
        return;
    const QRegularExpressionMatch op = trailingOperatorPattern().match(m_text);
    if (op.hasMatch())
        m_text.truncate(op.capturedStart());
    m_text += operatorText().value(key);
}

void ScientificCalc::pressOpenParen() {
    if (m_errored || m_justEvaluated)
        startFresh();
    if (endsWithOperand())
        m_text += QStringLiteral(" × ");
    m_text += QLatin1Char('(');
}

void ScientificCalc::pressCloseParen() {
    if (m_errored || m_justEvaluated)
        return;
    if (openParens() > 0 && endsWithOperand())
        m_text += QLatin1Char(')');
}

void ScientificCalc::pressEquals() {
    if (m_errored || m_justEvaluated)
        return;
    const QString text = completedText();
    if (text.isEmpty())
        return;

    QString error;
    const double value = Expression::evaluate(text, Expression::AngleUnit(m_angleUnit), &error);
    m_lastExpression = pretty(text);
    if (!error.isEmpty() || !std::isfinite(value)) {
        m_errored = true;
        m_error = error.isEmpty() ? tr("Invalid input") : error;
        return;
    }
    m_resultValue = value;
    m_justEvaluated = true;
    emit evaluated(m_lastExpression, formatValue(value), value);
}

void ScientificCalc::pressBackspace() {
    if (m_errored) {
        startFresh();
        return;
    }
    if (m_justEvaluated || m_text.isEmpty())
        return;
    const QRegularExpressionMatch op = trailingOperatorPattern().match(m_text);
    if (op.hasMatch()) {
        m_text.truncate(op.capturedStart());
        return;
    }
    const QRegularExpressionMatch call = trailingCallPattern().match(m_text);
    if (call.hasMatch()) {
        m_text.truncate(call.capturedStart());
        return;
    }
    m_text.chop(1);
}

void ScientificCalc::pressClearEntry() {
    if (m_errored || m_justEvaluated) {
        startFresh();
        return;
    }
    int start = trailingNumberStart();
    if (start < 0)
        return;
    if (start > 0 && m_text.at(start - 1) == minus[0])
        --start;
    m_text.truncate(start);
}

void ScientificCalc::pressSign() {
    if (m_errored)
        return;
    if (m_justEvaluated) {
        const double value = -m_resultValue;
        startFresh();
        m_text = literal(value);
        return;
    }

    // In an exponent, ± flips the exponent's sign: 2E5 → 2E−5.
    const int number = trailingNumberStart();
    if (number >= 0) {
        const int e = m_text.indexOf(QLatin1Char('E'), number);
        if (e >= 0) {
            if (e + 1 < m_text.size() && m_text.at(e + 1) == minus[0])
                m_text.remove(e + 1, 1);
            else
                m_text.insert(e + 1, minus);
            return;
        }
    }

    const int start = trailingOperandStart();
    if (start < 0) {
        if (m_text.endsWith(minus) && endsWithOperator() && !m_text.endsWith(QStringLiteral(" − ")))
            m_text.chop(1);
        else
            m_text += minus;
        return;
    }
    if (m_text.at(start) == minus[0])
        m_text.remove(start, 1);
    else
        m_text.insert(start, minus);
}

void ScientificCalc::pressExp() {
    if (m_errored || m_justEvaluated)
        startFresh();
    const int start = trailingNumberStart();
    if (start < 0) {
        insertOperand(QStringLiteral("1E"));
        return;
    }
    QString number = m_text.mid(start);
    if (number.contains(QLatin1Char('E')))
        return;
    if (m_text.endsWith(QLatin1Char('.')))
        m_text.chop(1);
    m_text += QLatin1Char('E');
}

void ScientificCalc::applyFunction(const QString &name) {
    if (m_errored)
        return;
    if (m_justEvaluated)
        continueFromResult();

    static const QHash<QString, QString> postfix = {
        {QStringLiteral("square"), QStringLiteral("²")},
        {QStringLiteral("cube"), QStringLiteral("³")},
        {QStringLiteral("factorial"), QStringLiteral("!")},
    };
    static const QHash<QString, QString> power = {
        {QStringLiteral("tenPow"), QStringLiteral("10^")},
        {QStringLiteral("twoPow"), QStringLiteral("2^")},
        {QStringLiteral("ePow"), QStringLiteral("e^")},
    };
    static const QStringList calls = {
        QStringLiteral("sin"), QStringLiteral("cos"), QStringLiteral("tan"),
        QStringLiteral("sec"), QStringLiteral("csc"), QStringLiteral("cot"),
        QStringLiteral("asin"), QStringLiteral("acos"), QStringLiteral("atan"),
        QStringLiteral("asec"), QStringLiteral("acsc"), QStringLiteral("acot"),
        QStringLiteral("sinh"), QStringLiteral("cosh"), QStringLiteral("tanh"),
        QStringLiteral("sech"), QStringLiteral("csch"), QStringLiteral("coth"),
        QStringLiteral("asinh"), QStringLiteral("acosh"), QStringLiteral("atanh"),
        QStringLiteral("asech"), QStringLiteral("acsch"), QStringLiteral("acoth"),
        QStringLiteral("sqrt"), QStringLiteral("cbrt"), QStringLiteral("abs"),
        QStringLiteral("floor"), QStringLiteral("ceil"), QStringLiteral("ln"),
        QStringLiteral("log"), QStringLiteral("exp"), QStringLiteral("dms"),
        QStringLiteral("deg"),
    };
    const bool isPostfix = postfix.contains(name);
    const bool isPower = power.contains(name);
    const bool isReciprocal = name == QStringLiteral("reciprocal");
    if (!isPostfix && !isPower && !isReciprocal && !calls.contains(name))
        return;

    QString call = name;
    if (name == QStringLiteral("sqrt"))
        call = QStringLiteral("√");
    else if (name == QStringLiteral("cbrt"))
        call = QStringLiteral("∛");

    const int start = trailingOperandStart();
    if (start < 0) {
        // Nothing to act on yet: open the call (or power) and wait for it.
        if (isPower)
            m_text += power.value(name);
        else if (!isPostfix && !isReciprocal)
            m_text += call + QLatin1Char('(');
        emit changed();
        return;
    }

    const QString operand = m_text.mid(start);
    const QString wrapped = operand.startsWith(minus) ? QStringLiteral("(%1)").arg(operand) : operand;
    QString replacement;
    if (isPostfix)
        replacement = wrapped + postfix.value(name);
    else if (isPower)
        replacement = power.value(name) + wrapped;
    else if (isReciprocal)
        replacement = QStringLiteral("(1/%1)").arg(operand);
    else
        replacement = QStringLiteral("%1(%2)").arg(call, operand);
    m_text.replace(start, operand.size(), replacement);
    emit changed();
}

// ---- Numbers in and out ------------------------------------------------

double ScientificCalc::currentNumber() const {
    if (m_errored)
        return 0;
    if (m_justEvaluated)
        return m_resultValue;
    const int start = trailingNumberStart();
    if (start >= 0) {
        QString number = m_text.mid(start);
        if (number.endsWith(QLatin1Char('E')) || number.endsWith(minus))
            number = number.left(number.indexOf(QLatin1Char('E')));
        number.replace(minus, QStringLiteral("-"));
        double value = number.toDouble();
        if (start > 0 && m_text.at(start - 1) == minus[0]
                && (start == 1 || m_text.at(start - 2) == QLatin1Char(' ') || m_text.at(start - 2) == QLatin1Char('(')))
            value = -value;
        return value;
    }
    bool ok = false;
    const double value = preview(&ok);
    return ok ? value : 0;
}

void ScientificCalc::enterNumber(double value) {
    if (!std::isfinite(value))
        return;
    if (m_errored || m_justEvaluated)
        startFresh();
    int start = trailingNumberStart();
    if (start >= 0) {
        if (start > 0 && m_text.at(start - 1) == minus[0]
                && (start == 1 || m_text.at(start - 2) == QLatin1Char(' ') || m_text.at(start - 2) == QLatin1Char('(')))
            --start;
        m_text.truncate(start);
    }
    insertOperand(literal(value));
    emit changed();
}

void ScientificCalc::copyResult() const {
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        QString text = display();
        text.remove(QLatin1Char(','));
        text.replace(minus, QStringLiteral("-"));
        clipboard->setText(text);
    }
}

void ScientificCalc::paste() {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return;
    QString text = clipboard->text().trimmed();
    static const QRegularExpression grouped(QStringLiteral("^[-−]?\\d{1,3}(,\\d{3})+(\\.\\d*)?$"));
    if (grouped.match(text).hasMatch())
        text.remove(QLatin1Char(','));
    bool ok = false;
    const double value = QString(text).replace(minus, QStringLiteral("-")).toDouble(&ok);
    if (ok) {
        enterNumber(value);
        return;
    }
    if (insertText(text))
        emit changed();
}

bool ScientificCalc::insertText(const QString &text) {
    const Expression parsed = Expression::parse(text);
    if (!parsed.isValid() || parsed.usesVariable())
        return false;
    insertOperand(QStringLiteral("(%1)").arg(text.trimmed()));
    return true;
}
