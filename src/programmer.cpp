#include "programmer.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHash>
#include <QRegularExpression>

#include "backend.h"

namespace {
const QString minus = QStringLiteral("−");

int precedence(const QString &op) {
    static const QHash<QString, int> table = {
        {QStringLiteral("*"), 6}, {QStringLiteral("/"), 6}, {QStringLiteral("%"), 6},
        {QStringLiteral("+"), 5}, {QStringLiteral("-"), 5},
        {QStringLiteral("lsh"), 4}, {QStringLiteral("rsh"), 4},
        {QStringLiteral("and"), 3}, {QStringLiteral("nand"), 3},
        {QStringLiteral("xor"), 2},
        {QStringLiteral("or"), 1}, {QStringLiteral("nor"), 1},
    };
    return table.value(op, 0);
}

int digitValue(QChar c) {
    if (c.isDigit())
        return c.digitValue();
    const QChar upper = c.toUpper();
    if (upper >= QLatin1Char('A') && upper <= QLatin1Char('F'))
        return upper.unicode() - 'A' + 10;
    return -1;
}

// Spaces every `size` digits from the right: FFFF FFFF, 0101 1010, 777 777.
QString groupFromRight(const QString &digits, int size) {
    QString out;
    const int n = digits.size();
    for (int i = 0; i < n; ++i) {
        if (i > 0 && (n - i) % size == 0)
            out += QLatin1Char(' ');
        out += digits.at(i);
    }
    return out;
}
}

ProgrammerCalc::ProgrammerCalc(QObject *parent) : QObject(parent) {}

quint64 ProgrammerCalc::mask() const {
    return m_wordBits >= 64 ? ~quint64(0) : ((quint64(1) << m_wordBits) - 1);
}

qint64 ProgrammerCalc::toSigned(quint64 bits) const {
    bits &= mask();
    if (m_wordBits < 64 && (bits & (quint64(1) << (m_wordBits - 1))))
        bits |= ~mask();
    return qint64(bits);
}

QString ProgrammerCalc::format(quint64 bits, int base, bool grouped) const {
    bits &= mask();
    if (base == 10) {
        const QString text = QString::number(toSigned(bits));
        return grouped ? Backend::groupDigits(text).replace(QLatin1Char('-'), minus) : text;
    }
    const QString digits = QString::number(bits, base).toUpper();
    if (!grouped)
        return digits;
    return groupFromRight(digits, base == 8 ? 3 : 4);
}

int ProgrammerCalc::openParens() const {
    int open = 0;
    for (const Token &token : m_tokens) {
        if (token.kind == Token::Open)
            ++open;
        else if (token.kind == Token::Close)
            --open;
    }
    return open;
}

bool ProgrammerCalc::digitAllowed(const QString &digit) const {
    if (digit.size() != 1)
        return false;
    const int value = digitValue(digit.at(0));
    return value >= 0 && value < m_base;
}

// Typed digits in the current base. Decimal reads as signed and must fit the
// word's signed range; the other bases may use every bit.
bool ProgrammerCalc::parseEntry(const QString &text, quint64 *bits) const {
    QString digits = text;
    bool negative = false;
    if (m_base == 10 && (digits.startsWith(QLatin1Char('-')) || digits.startsWith(minus))) {
        negative = true;
        digits.remove(0, 1);
    }
    if (digits.isEmpty()) {
        *bits = 0;
        return true;
    }
    quint64 value = 0;
    for (const QChar c : digits) {
        const int d = digitValue(c);
        if (d < 0 || d >= m_base)
            return false;
        // Overflow of 64 bits before masking.
        if (value > (~quint64(0) - quint64(d)) / quint64(m_base))
            return false;
        value = value * quint64(m_base) + quint64(d);
    }
    if (m_base == 10) {
        const quint64 limit = quint64(1) << (m_wordBits - 1);  // |min| of the signed range
        if (negative ? value > limit : value > limit - 1)
            return false;
        *bits = (negative ? (~value + 1) : value) & mask();
        return true;
    }
    if (value > mask())
        return false;
    *bits = value;
    return true;
}

quint64 ProgrammerCalc::currentBits() const {
    if (m_errored)
        return 0;
    if (!m_entry.isEmpty()) {
        quint64 bits = 0;
        parseEntry(m_entry, &bits);
        return bits;
    }
    if (m_hasEntryValue)
        return m_entryBits & mask();
    if (m_justEvaluated)
        return m_resultBits & mask();
    for (int i = m_tokens.size() - 1; i >= 0; --i) {
        if (m_tokens.at(i).kind == Token::Number)
            return m_tokens.at(i).value & mask();
    }
    return 0;
}

QString ProgrammerCalc::display() const {
    if (m_errored)
        return m_error;
    return format(currentBits(), m_base);
}

QString ProgrammerCalc::prettyTokens(const QList<Token> &tokens) const {
    QStringList parts;
    for (const Token &token : tokens) {
        switch (token.kind) {
        case Token::Number: parts << format(token.value, m_base); break;
        case Token::Open: parts << QStringLiteral("("); break;
        case Token::Close: parts << QStringLiteral(")"); break;
        case Token::Operator: {
            static const QHash<QString, QString> names = {
                {QStringLiteral("+"), QStringLiteral("+")}, {QStringLiteral("-"), QStringLiteral("−")},
                {QStringLiteral("*"), QStringLiteral("×")}, {QStringLiteral("/"), QStringLiteral("÷")},
                {QStringLiteral("%"), QStringLiteral("%")}, {QStringLiteral("and"), QStringLiteral("AND")},
                {QStringLiteral("or"), QStringLiteral("OR")}, {QStringLiteral("xor"), QStringLiteral("XOR")},
                {QStringLiteral("nand"), QStringLiteral("NAND")}, {QStringLiteral("nor"), QStringLiteral("NOR")},
            };
            if (token.op == QStringLiteral("lsh") || token.op == QStringLiteral("rsh")) {
                const bool left = token.op == QStringLiteral("lsh");
                static const char *shiftNames[4][2] = {{"<<", ">>"}, {"<<", ">>"}, {"RoL", "RoR"}, {"RcL", "RcR"}};
                parts << QString::fromLatin1(shiftNames[m_shiftMode][left ? 0 : 1]);
            } else {
                parts << names.value(token.op, token.op);
            }
            break;
        }
        }
    }
    // Parentheses hug what they enclose: (1 + 2) × 3.
    QString text = parts.join(QLatin1Char(' '));
    text.replace(QStringLiteral("( "), QStringLiteral("("));
    text.replace(QStringLiteral(" )"), QStringLiteral(")"));
    return text;
}

QString ProgrammerCalc::expression() const {
    if (m_errored || m_justEvaluated)
        return m_lastExpression;
    return prettyTokens(m_tokens);
}

void ProgrammerCalc::setBase(int base) {
    if (base != 2 && base != 8 && base != 10 && base != 16)
        return;
    if (base == m_base)
        return;
    // A number being typed keeps its value and continues in the new base.
    const bool typing = !m_entry.isEmpty();
    const quint64 bits = currentBits();
    m_base = base;
    if (typing)
        m_entry = format(bits, base, false);
    emit changed();
}

void ProgrammerCalc::setWordBits(int bits) {
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64)
        return;
    if (bits == m_wordBits)
        return;
    // Narrowing truncates to the low bits, as a cast would.
    const bool typing = !m_entry.isEmpty();
    const quint64 value = currentBits();
    m_wordBits = bits;
    for (Token &token : m_tokens)
        token.value &= mask();
    m_entryBits &= mask();
    m_resultBits &= mask();
    if (typing)
        m_entry = format(value, m_base, false);
    emit changed();
}

void ProgrammerCalc::setShiftMode(int mode) {
    mode = qBound(0, mode, 3);
    if (mode == m_shiftMode)
        return;
    m_shiftMode = mode;
    emit changed();
}

bool ProgrammerCalc::bit(int index) const {
    if (index < 0 || index >= m_wordBits)
        return false;
    return (currentBits() >> index) & 1;
}

void ProgrammerCalc::toggleBit(int index) {
    if (index < 0 || index >= m_wordBits || m_errored)
        return;
    const quint64 value = currentBits() ^ (quint64(1) << index);
    if (m_justEvaluated) {
        m_justEvaluated = false;
        m_lastExpression.clear();
    }
    m_hasEntryValue = false;
    m_entry = format(value, m_base, false);
    emit changed();
}

void ProgrammerCalc::enterBits(quint64 bits) {
    if (m_errored || m_justEvaluated)
        clearAll();
    m_hasEntryValue = false;
    m_entry = format(bits & mask(), m_base, false);
    emit changed();
}

void ProgrammerCalc::clearAll() {
    m_tokens.clear();
    m_entry.clear();
    m_hasEntryValue = false;
    m_entryBits = 0;
    m_resultBits = 0;
    m_lastExpression.clear();
    m_error.clear();
    m_justEvaluated = false;
    m_errored = false;
}

void ProgrammerCalc::pressKey(const QString &key) {
    const QString lower = key.toLower();
    if (key.size() == 1 && digitValue(key.at(0)) >= 0)
        pressDigit(key.toUpper());
    else if (precedence(lower) > 0 || lower == QStringLiteral("−") || lower == QStringLiteral("×")
             || lower == QStringLiteral("÷"))
        pressOperator(lower == QStringLiteral("−") ? QStringLiteral("-")
                      : lower == QStringLiteral("×") ? QStringLiteral("*")
                      : lower == QStringLiteral("÷") ? QStringLiteral("/") : lower);
    else if (key == QStringLiteral("(")) {
        if (m_errored || m_justEvaluated)
            clearAll();
        commitEntry();
        if (!m_tokens.isEmpty() && (m_tokens.last().kind == Token::Number || m_tokens.last().kind == Token::Close))
            m_tokens << Token{Token::Operator, 0, QStringLiteral("*")};
        m_tokens << Token{Token::Open, 0, {}};
    } else if (key == QStringLiteral(")")) {
        if (m_errored || m_justEvaluated || openParens() <= 0)
            return;
        commitEntry();
        if (m_tokens.isEmpty() || m_tokens.last().kind == Token::Operator || m_tokens.last().kind == Token::Open)
            return;
        m_tokens << Token{Token::Close, 0, {}};
    } else if (key == QStringLiteral("="))
        pressEquals();
    else if (lower == QStringLiteral("not") || key == QStringLiteral("sign"))
        pressUnary(lower);
    else if (key == QStringLiteral("backspace"))
        pressBackspace();
    else if (key == QStringLiteral("clear"))
        clearAll();
    else if (key == QStringLiteral("clearEntry")) {
        if (m_errored || m_justEvaluated) {
            clearAll();
        } else {
            m_entry.clear();
            m_hasEntryValue = false;
        }
    } else if (key == QStringLiteral("word")) {
        static const QList<int> sizes = {64, 32, 16, 8};
        setWordBits(sizes.at((sizes.indexOf(m_wordBits) + 1) % sizes.size()));
        return;
    } else
        return;
    emit changed();
}

void ProgrammerCalc::pressDigit(const QString &digit) {
    if (!digitAllowed(digit))
        return;
    if (m_errored || m_justEvaluated)
        clearAll();
    if (m_hasEntryValue) {
        m_hasEntryValue = false;
        m_entry.clear();
    }
    // A number can't follow ")" directly.
    if (m_entry.isEmpty() && !m_tokens.isEmpty() && m_tokens.last().kind == Token::Close)
        return;
    QString candidate = m_entry + digit;
    if (candidate.startsWith(QLatin1Char('0')) && candidate.size() > 1)
        candidate.remove(0, 1);
    else if (candidate.startsWith(QStringLiteral("-0")) && candidate.size() > 2)
        candidate.remove(1, 1);
    quint64 bits = 0;
    if (parseEntry(candidate, &bits))
        m_entry = candidate;
}

// Moves what is on the display into the expression, if anything is there.
void ProgrammerCalc::commitEntry() {
    if (m_entry.isEmpty() && !m_hasEntryValue)
        return;
    m_tokens << Token{Token::Number, currentBits(), {}};
    m_entry.clear();
    m_hasEntryValue = false;
}

void ProgrammerCalc::pressOperator(const QString &op) {
    if (m_errored)
        return;
    if (m_justEvaluated) {
        const quint64 result = m_resultBits;
        clearAll();
        m_tokens << Token{Token::Number, result, {}};
    }
    commitEntry();
    if (m_tokens.isEmpty()) {
        m_tokens << Token{Token::Number, 0, {}};
    } else if (m_tokens.last().kind == Token::Operator) {
        m_tokens.last().op = op;
        return;
    } else if (m_tokens.last().kind == Token::Open) {
        return;
    }
    m_tokens << Token{Token::Operator, 0, op};
}

void ProgrammerCalc::pressUnary(const QString &kind) {
    if (m_errored)
        return;
    if (m_entry.isEmpty() && !m_hasEntryValue && !m_justEvaluated
            && !m_tokens.isEmpty() && m_tokens.last().kind == Token::Close)
        return;
    const quint64 value = currentBits();
    const quint64 result = (kind == QStringLiteral("not") ? ~value : (~value + 1)) & mask();
    if (m_justEvaluated) {
        m_justEvaluated = false;
        m_lastExpression.clear();
    }
    if (kind == QStringLiteral("not")) {
        // The next digit starts a new number rather than extending this one.
        m_entry.clear();
        m_hasEntryValue = true;
        m_entryBits = result;
    } else {
        m_hasEntryValue = false;
        m_entry = format(result, m_base, false);
    }
}

void ProgrammerCalc::pressBackspace() {
    if (m_errored) {
        clearAll();
        return;
    }
    if (m_justEvaluated)
        return;
    if (!m_entry.isEmpty()) {
        m_entry.chop(1);
        if (m_entry == QStringLiteral("-"))
            m_entry.clear();
    } else if (m_hasEntryValue) {
        m_hasEntryValue = false;
    } else if (!m_tokens.isEmpty() && m_tokens.last().kind != Token::Number) {
        m_tokens.removeLast();
    }
}

QList<ProgrammerCalc::Token> ProgrammerCalc::finalTokens() const {
    QList<Token> tokens = m_tokens;
    if (!m_entry.isEmpty() || m_hasEntryValue)
        tokens << Token{Token::Number, currentBits(), {}};
    while (!tokens.isEmpty() && (tokens.last().kind == Token::Operator || tokens.last().kind == Token::Open))
        tokens.removeLast();
    int open = 0;
    for (const Token &token : tokens)
        open += token.kind == Token::Open ? 1 : (token.kind == Token::Close ? -1 : 0);
    for (int i = 0; i < open; ++i)
        tokens << Token{Token::Close, 0, {}};
    return tokens;
}

void ProgrammerCalc::pressEquals() {
    if (m_errored || m_justEvaluated)
        return;
    const QList<Token> tokens = finalTokens();
    if (tokens.isEmpty())
        return;
    bool ok = false;
    QString error;
    bool carry = m_carry;
    const quint64 result = evaluate(tokens, &ok, &error, &carry);
    m_lastExpression = prettyTokens(tokens) + QStringLiteral(" =");
    m_tokens.clear();
    m_entry.clear();
    m_hasEntryValue = false;
    if (!ok) {
        m_errored = true;
        m_error = error;
        return;
    }
    m_carry = carry;
    m_resultBits = result;
    m_justEvaluated = true;
    emit evaluated(prettyTokens(tokens), format(result, m_base), double(toSigned(result)));
}

quint64 ProgrammerCalc::apply(const QString &op, quint64 a, quint64 b, bool *ok, QString *error,
                              bool *carry) const {
    const quint64 m = mask();
    const int bits = m_wordBits;
    const qint64 sa = toSigned(a);
    const qint64 sb = toSigned(b);
    const qint64 minimum = toSigned(quint64(1) << (bits - 1));
    *ok = true;

    if (op == QStringLiteral("+")) return (a + b) & m;
    if (op == QStringLiteral("-")) return (a - b) & m;
    if (op == QStringLiteral("*")) return (a * b) & m;
    if (op == QStringLiteral("/") || op == QStringLiteral("%")) {
        if (sb == 0) {
            *ok = false;
            *error = tr("Can't divide by zero");
            return 0;
        }
        // The one signed division that overflows wraps back to the minimum.
        if (sa == minimum && sb == -1)
            return op == QStringLiteral("/") ? quint64(minimum) & m : 0;
        return quint64(op == QStringLiteral("/") ? sa / sb : sa % sb) & m;
    }
    if (op == QStringLiteral("and")) return a & b;
    if (op == QStringLiteral("or")) return a | b;
    if (op == QStringLiteral("xor")) return a ^ b;
    if (op == QStringLiteral("nand")) return ~(a & b) & m;
    if (op == QStringLiteral("nor")) return ~(a | b) & m;

    // Shifts take the count as unsigned; a count past the word clears it.
    const bool left = op == QStringLiteral("lsh");
    const quint64 count = sb < 0 ? quint64(bits) : quint64(sb);
    switch (m_shiftMode) {
    case 0:  // Arithmetic: right shifts copy the sign bit in.
        if (left)
            return count >= quint64(bits) ? 0 : (a << count) & m;
        return count >= quint64(bits) ? (sa < 0 ? m : 0) : quint64(sa >> count) & m;
    case 1:  // Logical: zeros come in from either end.
        if (count >= quint64(bits))
            return 0;
        return left ? (a << count) & m : (a & m) >> count;
    case 2: {  // Rotate within the word.
        const int n = int(count % quint64(bits));
        if (n == 0)
            return a & m;
        return left ? ((a << n) | (a >> (bits - n))) & m : ((a >> n) | (a << (bits - n))) & m;
    }
    default: {  // Rotate through carry: the carry flag is a bit beyond the word.
        const int n = int(count % quint64(bits + 1));
        quint64 value = a & m;
        bool c = *carry;
        for (int i = 0; i < n; ++i) {
            if (left) {
                const bool out = (value >> (bits - 1)) & 1;
                value = ((value << 1) | quint64(c)) & m;
                c = out;
            } else {
                const bool out = value & 1;
                value = (value >> 1) | (quint64(c) << (bits - 1));
                c = out;
            }
        }
        *carry = c;
        return value & m;
    }
    }
}

// Shunting-yard over numbers, binary operators and parentheses, with the
// C precedence of the operators: × ÷ %, then + −, shifts, AND, XOR, OR.
quint64 ProgrammerCalc::evaluate(QList<Token> tokens, bool *ok, QString *error, bool *carry) const {
    QList<quint64> values;
    QStringList ops;
    *ok = true;

    auto reduce = [&]() -> bool {
        if (values.size() < 2 || ops.isEmpty())
            return false;
        const quint64 b = values.takeLast();
        const quint64 a = values.takeLast();
        bool applied = false;
        values << apply(ops.takeLast(), a, b, &applied, error, carry);
        return applied;
    };

    for (const Token &token : tokens) {
        switch (token.kind) {
        case Token::Number:
            values << (token.value & mask());
            break;
        case Token::Open:
            ops << QStringLiteral("(");
            break;
        case Token::Close:
            while (!ops.isEmpty() && ops.last() != QStringLiteral("(")) {
                if (!reduce()) {
                    *ok = false;
                    return 0;
                }
            }
            if (!ops.isEmpty())
                ops.removeLast();
            break;
        case Token::Operator:
            while (!ops.isEmpty() && ops.last() != QStringLiteral("(")
                   && precedence(ops.last()) >= precedence(token.op)) {
                if (!reduce()) {
                    *ok = false;
                    return 0;
                }
            }
            ops << token.op;
            break;
        }
    }
    while (!ops.isEmpty()) {
        if (ops.last() == QStringLiteral("(")) {
            ops.removeLast();
            continue;
        }
        if (!reduce()) {
            *ok = false;
            return 0;
        }
    }
    if (values.size() != 1) {
        *ok = false;
        if (error->isEmpty())
            *error = tr("Invalid input");
        return 0;
    }
    return values.first();
}

void ProgrammerCalc::copyResult() const {
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(format(currentBits(), m_base, false));
}

// Accepts digits in the current base, or a C-style 0x / 0o / 0b prefix for
// another one, ignoring spaces, underscores and thousands separators.
void ProgrammerCalc::paste() {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return;
    QString text = clipboard->text().trimmed();
    text.remove(QRegularExpression(QStringLiteral("[\\s_,']")));
    text.replace(minus, QStringLiteral("-"));
    int base = m_base;
    const QString lower = text.toLower();
    if (lower.startsWith(QStringLiteral("0x"))) { base = 16; text.remove(0, 2); }
    else if (lower.startsWith(QStringLiteral("0b"))) { base = 2; text.remove(0, 2); }
    else if (lower.startsWith(QStringLiteral("0o"))) { base = 8; text.remove(0, 2); }
    if (text.isEmpty())
        return;
    bool ok = false;
    quint64 bits = 0;
    if (base == 10) {
        const qint64 value = text.toLongLong(&ok, 10);
        bits = quint64(value);
        if (!ok) {
            bits = text.toULongLong(&ok, 10);
        }
    } else {
        bits = text.toULongLong(&ok, base);
    }
    if (ok)
        enterBits(bits);
}
