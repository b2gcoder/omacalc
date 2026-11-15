#include "expression.h"

#include <QCoreApplication>
#include <QList>
#include <QStringList>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr double pi = 3.14159265358979323846;
const double notANumber = std::numeric_limits<double>::quiet_NaN();

enum class Fn {
    Sin, Cos, Tan, Sec, Csc, Cot,
    Asin, Acos, Atan, Asec, Acsc, Acot,
    Sinh, Cosh, Tanh, Sech, Csch, Coth,
    Asinh, Acosh, Atanh, Asech, Acsch, Acoth,
    Sqrt, Cbrt, Abs, Floor, Ceil, Round, Ln, Log, Log2, Exp, Dms, Deg, Sign,
};

struct FunctionName {
    const char *name;
    Fn fn;
};

// Longest names first: the tokenizer takes the first one that matches, so
// "asinh" is not read as "a" + "sinh", nor "sech" as "sec" + "h".
const std::array<FunctionName, 39> functionNames = {{
    {"asinh", Fn::Asinh}, {"acosh", Fn::Acosh}, {"atanh", Fn::Atanh},
    {"asech", Fn::Asech}, {"acsch", Fn::Acsch}, {"acoth", Fn::Acoth},
    {"floor", Fn::Floor}, {"round", Fn::Round},
    {"asin", Fn::Asin}, {"acos", Fn::Acos}, {"atan", Fn::Atan},
    {"asec", Fn::Asec}, {"acsc", Fn::Acsc}, {"acot", Fn::Acot},
    {"sinh", Fn::Sinh}, {"cosh", Fn::Cosh}, {"tanh", Fn::Tanh},
    {"sech", Fn::Sech}, {"csch", Fn::Csch}, {"coth", Fn::Coth},
    {"sqrt", Fn::Sqrt}, {"cbrt", Fn::Cbrt}, {"ceil", Fn::Ceil},
    {"log2", Fn::Log2}, {"sign", Fn::Sign},
    {"sin", Fn::Sin}, {"cos", Fn::Cos}, {"tan", Fn::Tan},
    {"sec", Fn::Sec}, {"csc", Fn::Csc}, {"cot", Fn::Cot},
    {"abs", Fn::Abs}, {"log", Fn::Log}, {"exp", Fn::Exp},
    {"dms", Fn::Dms}, {"deg", Fn::Deg},
    {"ln", Fn::Ln}, {"√", Fn::Sqrt}, {"∛", Fn::Cbrt},
}};

// Words that are not functions: operators spelled out, constants, the variable.
const QStringList keywordNames = {
    QStringLiteral("logb"), QStringLiteral("root"), QStringLiteral("mod"),
    QStringLiteral("pi"), QStringLiteral("π"), QStringLiteral("e"), QStringLiteral("x"),
};

QString tr(const char *text) {
    return QCoreApplication::translate("Expression", text);
}
}

struct Expression::Node {
    enum Kind { Number, Variable, Negate, Add, Subtract, Multiply, Divide, Modulo,
                Power, Root, LogBase, Factorial, Call };
    Kind kind = Number;
    double value = 0;
    Fn fn = Fn::Sin;
    std::unique_ptr<Node> left, right;
};

namespace {
using Node = Expression::Node;

std::unique_ptr<Node> leaf(double value) {
    auto node = std::make_unique<Node>();
    node->kind = Node::Number;
    node->value = value;
    return node;
}

std::unique_ptr<Node> join(Node::Kind kind, std::unique_ptr<Node> left, std::unique_ptr<Node> right = {}) {
    auto node = std::make_unique<Node>();
    node->kind = kind;
    node->left = std::move(left);
    node->right = std::move(right);
    return node;
}

struct Token {
    enum Kind { End, Number, Name, Function, Symbol } kind = End;
    double number = 0;
    QString text;
    Fn fn = Fn::Sin;
};

class Parser {
public:
    explicit Parser(const QString &text) : m_text(text) { advance(); }

    std::unique_ptr<Node> parseAll() {
        auto node = parseSum();
        if (!node)
            return {};
        if (m_token.kind != Token::End)
            return fail(m_token.text == QLatin1String(")") ? tr("Too many )")
                                                          : tr("Unexpected \"%1\"").arg(m_token.text));
        return node;
    }

    QString error;
    bool usesVariable = false;

private:
    std::unique_ptr<Node> fail(const QString &message) {
        if (error.isEmpty())
            error = message;
        return {};
    }

    // UTF-8, since some symbols (², ³) are not Latin-1.
    bool isSymbol(const char *symbol) const {
        return m_token.kind == Token::Symbol && m_token.text == QString::fromUtf8(symbol);
    }
    bool isName(const char *name) const {
        return m_token.kind == Token::Name && m_token.text == QString::fromUtf8(name);
    }

    void advance() {
        while (m_pos < m_text.size() && m_text.at(m_pos).isSpace())
            ++m_pos;
        m_token = Token();
        if (m_pos >= m_text.size())
            return;

        const QChar c = m_text.at(m_pos);
        if (c.isDigit() || (c == QLatin1Char('.') && m_pos + 1 < m_text.size() && m_text.at(m_pos + 1).isDigit())) {
            readNumber();
            return;
        }

        // Typographic operators from the keypad and plain ASCII alike.
        static const QList<QPair<QString, QString>> symbols = {
            {QStringLiteral("×"), QStringLiteral("*")}, {QStringLiteral("·"), QStringLiteral("*")},
            {QStringLiteral("÷"), QStringLiteral("/")}, {QStringLiteral("−"), QStringLiteral("-")},
            {QStringLiteral("²"), QStringLiteral("²")}, {QStringLiteral("³"), QStringLiteral("³")},
        };
        for (const auto &symbol : symbols) {
            if (m_text.mid(m_pos, symbol.first.size()) == symbol.first) {
                m_pos += symbol.first.size();
                m_token.kind = Token::Symbol;
                m_token.text = symbol.second;
                return;
            }
        }
        if (QStringLiteral("+-*/^!()|,").contains(c)) {
            ++m_pos;
            m_token.kind = Token::Symbol;
            m_token.text = c;
            return;
        }

        const QString rest = m_text.mid(m_pos).toLower();
        // Spelled-out operators first, so "logb" isn't log followed by b.
        for (const QString &word : {QStringLiteral("logb"), QStringLiteral("root"), QStringLiteral("mod")}) {
            if (rest.startsWith(word)) {
                m_pos += word.size();
                m_token.kind = Token::Name;
                m_token.text = word;
                return;
            }
        }
        for (const FunctionName &function : functionNames) {
            const QString name = QString::fromUtf8(function.name);
            if (rest.startsWith(name)) {
                m_pos += name.size();
                m_token.kind = Token::Function;
                m_token.text = name;
                m_token.fn = function.fn;
                return;
            }
        }
        for (const QString &name : keywordNames) {
            if (rest.startsWith(name)) {
                m_pos += name.size();
                m_token.kind = Token::Name;
                m_token.text = name == QStringLiteral("π") ? QStringLiteral("pi") : name;
                return;
            }
        }

        // Unknown: report the whole word, not just its first letter.
        int end = m_pos + 1;
        while (end < m_text.size() && m_text.at(end).isLetter())
            ++end;
        m_token.kind = Token::Symbol;
        m_token.text = m_text.mid(m_pos, end - m_pos);
        m_pos = end;
    }

    // Digits, a decimal point, and an exponent written with a capital E
    // (2E5, 1.5E-3), as the keypad's Exp key types it. A lowercase e after
    // a number is the constant: 2e is 2 × e.
    void readNumber() {
        const int start = m_pos;
        while (m_pos < m_text.size() && (m_text.at(m_pos).isDigit() || m_text.at(m_pos) == QLatin1Char('.')))
            ++m_pos;
        if (m_pos < m_text.size() && m_text.at(m_pos) == QLatin1Char('E')) {
            int cursor = m_pos + 1;
            if (cursor < m_text.size() && (m_text.at(cursor) == QLatin1Char('+') || m_text.at(cursor) == QLatin1Char('-')))
                ++cursor;
            else if (m_text.mid(cursor, 1) == QStringLiteral("−"))
                ++cursor;
            if (cursor < m_text.size() && m_text.at(cursor).isDigit()) {
                while (cursor < m_text.size() && m_text.at(cursor).isDigit())
                    ++cursor;
                m_pos = cursor;
            }
        }
        QString literal = m_text.mid(start, m_pos - start);
        literal.replace(QStringLiteral("−"), QStringLiteral("-"));
        bool ok = false;
        m_token.kind = Token::Number;
        m_token.text = literal;
        m_token.number = literal.toDouble(&ok);
        if (!ok || literal.count(QLatin1Char('.')) > 1) {
            m_token.kind = Token::Symbol;  // Reported as unexpected.
        }
    }

    // sum := product (('+' | '-') product)*
    std::unique_ptr<Node> parseSum() {
        auto left = parseProduct();
        while (left && (isSymbol("+") || isSymbol("-"))) {
            const Node::Kind kind = isSymbol("+") ? Node::Add : Node::Subtract;
            advance();
            auto right = parseProduct();
            if (!right)
                return {};
            left = join(kind, std::move(left), std::move(right));
        }
        return left;
    }

    bool startsImplicitFactor() const {
        switch (m_token.kind) {
        case Token::Number:
        case Token::Function:
            return true;
        case Token::Name:
            return m_token.text != QLatin1String("mod") && m_token.text != QLatin1String("root")
                && m_token.text != QLatin1String("logb");
        case Token::Symbol:
            return m_token.text == QLatin1String("(") || (m_token.text == QLatin1String("|") && m_barDepth == 0);
        default:
            return false;
        }
    }

    // product := signed (('*' | '/' | 'mod' | implicit) signed)*
    std::unique_ptr<Node> parseProduct() {
        auto left = parseSigned();
        while (left) {
            Node::Kind kind;
            if (isSymbol("*")) {
                kind = Node::Multiply;
                advance();
            } else if (isSymbol("/")) {
                kind = Node::Divide;
                advance();
            } else if (isName("mod")) {
                kind = Node::Modulo;
                advance();
            } else if (startsImplicitFactor()) {
                kind = Node::Multiply;
            } else {
                break;
            }
            auto right = parseSigned();
            if (!right)
                return {};
            left = join(kind, std::move(left), std::move(right));
        }
        return left;
    }

    // signed := ('-' | '+') signed | power, so -2^2 is −4 and 2^-1 is 0.5.
    std::unique_ptr<Node> parseSigned() {
        if (isSymbol("-")) {
            advance();
            auto operand = parseSigned();
            return operand ? join(Node::Negate, std::move(operand)) : nullptr;
        }
        if (isSymbol("+")) {
            advance();
            return parseSigned();
        }
        return parsePower();
    }

    // power := postfix (('^' | 'root' | 'logb') signed)?, right-associative.
    std::unique_ptr<Node> parsePower() {
        auto base = parsePostfix();
        if (!base)
            return {};
        Node::Kind kind;
        if (isSymbol("^"))
            kind = Node::Power;
        else if (isName("root"))
            kind = Node::Root;
        else if (isName("logb"))
            kind = Node::LogBase;
        else
            return base;
        advance();
        auto exponent = parseSigned();
        if (!exponent)
            return {};
        return join(kind, std::move(base), std::move(exponent));
    }

    std::unique_ptr<Node> parsePostfix() {
        auto node = parsePrimary();
        while (node) {
            if (isSymbol("!")) {
                node = join(Node::Factorial, std::move(node));
            } else if (isSymbol("²")) {
                node = join(Node::Power, std::move(node), leaf(2));
            } else if (isSymbol("³")) {
                node = join(Node::Power, std::move(node), leaf(3));
            } else {
                break;
            }
            advance();
        }
        return node;
    }

    std::unique_ptr<Node> parsePrimary() {
        if (m_token.kind == Token::End)
            return fail(tr("Incomplete expression"));

        if (m_token.kind == Token::Number) {
            auto node = leaf(m_token.number);
            advance();
            return node;
        }

        if (m_token.kind == Token::Name) {
            const QString name = m_token.text;
            if (name == QLatin1String("pi") || name == QLatin1String("e") || name == QLatin1String("x")) {
                advance();
                if (name == QLatin1String("x")) {
                    usesVariable = true;
                    auto node = std::make_unique<Node>();
                    node->kind = Node::Variable;
                    return node;
                }
                return leaf(name == QLatin1String("pi") ? pi : std::exp(1.0));
            }
            return fail(tr("Incomplete expression"));
        }

        if (m_token.kind == Token::Function) {
            const Fn fn = m_token.fn;
            const bool radical = m_token.text == QStringLiteral("√") || m_token.text == QStringLiteral("∛");
            advance();
            std::unique_ptr<Node> argument;
            if (isSymbol("(")) {
                advance();
                argument = parseSum();
                if (!argument)
                    return {};
                if (!isSymbol(")"))
                    return fail(tr("Missing )"));
                advance();
            } else if (radical) {
                // √2 and √x read naturally without parentheses.
                argument = parsePostfix();
                if (!argument)
                    return {};
            } else {
                return fail(tr("Missing ( after a function"));
            }
            auto node = join(Node::Call, std::move(argument));
            node->fn = fn;
            return node;
        }

        if (isSymbol("(")) {
            advance();
            auto inner = parseSum();
            if (!inner)
                return {};
            if (!isSymbol(")"))
                return fail(tr("Missing )"));
            advance();
            return inner;
        }

        if (isSymbol("|")) {
            advance();
            ++m_barDepth;
            auto inner = parseSum();
            --m_barDepth;
            if (!inner)
                return {};
            if (!isSymbol("|"))
                return fail(tr("Missing |"));
            advance();
            auto node = join(Node::Call, std::move(inner));
            node->fn = Fn::Abs;
            return node;
        }

        return fail(tr("Unexpected \"%1\"").arg(m_token.text));
    }

    QString m_text;
    int m_pos = 0;
    int m_barDepth = 0;
    Token m_token;
};

// Trigonometry in degrees or gradians lands exactly on 0 and ±1 at the
// quarter turns, so sin 180° is 0 rather than 1.2246e-16.
struct Turn {
    double radians;
    double degrees;  // NaN when the angle was entered in radians.
};

Turn toTurn(double angle, Expression::AngleUnit unit) {
    switch (unit) {
    case Expression::Degrees: return {angle * pi / 180.0, angle};
    case Expression::Gradians: return {angle * pi / 200.0, angle * 0.9};
    default: return {angle, notANumber};
    }
}

double fromRadians(double radians, Expression::AngleUnit unit) {
    switch (unit) {
    case Expression::Degrees: return radians * 180.0 / pi;
    case Expression::Gradians: return radians * 200.0 / pi;
    default: return radians;
    }
}

double exactSin(const Turn &turn) {
    if (!std::isnan(turn.degrees)) {
        double reduced = std::fmod(turn.degrees, 360.0);
        if (reduced < 0)
            reduced += 360.0;
        if (reduced == 0 || reduced == 180) return 0;
        if (reduced == 90) return 1;
        if (reduced == 270) return -1;
        if (reduced == 30 || reduced == 150) return 0.5;
        if (reduced == 210 || reduced == 330) return -0.5;
        return std::sin(reduced * pi / 180.0);
    }
    // Multiples of π typed in radians.
    const double half = std::remainder(turn.radians, pi);
    if (std::fabs(half) < 1e-12 * std::max(1.0, std::fabs(turn.radians)))
        return 0;
    return std::sin(turn.radians);
}

double exactCos(const Turn &turn) {
    if (!std::isnan(turn.degrees))
        return exactSin({turn.radians + pi / 2, turn.degrees + 90});
    const double offset = std::remainder(turn.radians - pi / 2, pi);
    if (std::fabs(offset) < 1e-12 * std::max(1.0, std::fabs(turn.radians)))
        return 0;
    return std::cos(turn.radians);
}

struct Evaluator {
    double x;
    Expression::AngleUnit unit;
    QString *error;

    double invalid() const {
        if (error && error->isEmpty())
            *error = tr("Invalid input");
        return notANumber;
    }
    double divideByZero() const {
        if (error && error->isEmpty())
            *error = tr("Can't divide by zero");
        return notANumber;
    }
    double checked(double value) const {
        if (std::isinf(value)) {
            if (error && error->isEmpty())
                *error = tr("Overflow");
            return notANumber;
        }
        return value;
    }

    double factorial(double n) const {
        if (n < 0 && n == std::floor(n))
            return invalid();
        if (n > 170)
            return checked(INFINITY);
        if (n == std::floor(n)) {
            double result = 1;
            for (int i = 2; i <= int(n); ++i)
                result *= i;
            return result;
        }
        // Fractional factorials follow the gamma function: 0.5! = √π / 2.
        return checked(std::tgamma(n + 1));
    }

    double call(Fn fn, double v) const {
        switch (fn) {
        case Fn::Sin: return exactSin(toTurn(v, unit));
        case Fn::Cos: return exactCos(toTurn(v, unit));
        case Fn::Tan: {
            const Turn turn = toTurn(v, unit);
            const double c = exactCos(turn);
            return c == 0 ? invalid() : exactSin(turn) / c;
        }
        case Fn::Sec: {
            const double c = exactCos(toTurn(v, unit));
            return c == 0 ? invalid() : 1 / c;
        }
        case Fn::Csc: {
            const double s = exactSin(toTurn(v, unit));
            return s == 0 ? invalid() : 1 / s;
        }
        case Fn::Cot: {
            const Turn turn = toTurn(v, unit);
            const double s = exactSin(turn);
            return s == 0 ? invalid() : exactCos(turn) / s;
        }
        case Fn::Asin: return std::fabs(v) > 1 ? invalid() : fromRadians(std::asin(v), unit);
        case Fn::Acos: return std::fabs(v) > 1 ? invalid() : fromRadians(std::acos(v), unit);
        case Fn::Atan: return fromRadians(std::atan(v), unit);
        case Fn::Asec: return std::fabs(v) < 1 ? invalid() : fromRadians(std::acos(1 / v), unit);
        case Fn::Acsc: return std::fabs(v) < 1 ? invalid() : fromRadians(std::asin(1 / v), unit);
        case Fn::Acot: return fromRadians(pi / 2 - std::atan(v), unit);
        case Fn::Sinh: return checked(std::sinh(v));
        case Fn::Cosh: return checked(std::cosh(v));
        case Fn::Tanh: return std::tanh(v);
        case Fn::Sech: return 1 / std::cosh(v);
        case Fn::Csch: return v == 0 ? invalid() : checked(1 / std::sinh(v));
        case Fn::Coth: return v == 0 ? invalid() : 1 / std::tanh(v);
        case Fn::Asinh: return std::asinh(v);
        case Fn::Acosh: return v < 1 ? invalid() : std::acosh(v);
        case Fn::Atanh: return std::fabs(v) >= 1 ? invalid() : std::atanh(v);
        case Fn::Asech: return (v <= 0 || v > 1) ? invalid() : std::acosh(1 / v);
        case Fn::Acsch: return v == 0 ? invalid() : std::asinh(1 / v);
        case Fn::Acoth: return std::fabs(v) <= 1 ? invalid() : std::atanh(1 / v);
        case Fn::Sqrt: return v < 0 ? invalid() : std::sqrt(v);
        case Fn::Cbrt: return std::cbrt(v);
        case Fn::Abs: return std::fabs(v);
        case Fn::Floor: return std::floor(v);
        case Fn::Ceil: return std::ceil(v);
        case Fn::Round: return std::round(v);
        case Fn::Sign: return v > 0 ? 1 : (v < 0 ? -1 : 0);
        case Fn::Ln: return v <= 0 ? invalid() : std::log(v);
        case Fn::Log: return v <= 0 ? invalid() : std::log10(v);
        case Fn::Log2: return v <= 0 ? invalid() : std::log2(v);
        case Fn::Exp: return checked(std::exp(v));
        case Fn::Dms: {
            // Decimal degrees to degrees.minutesseconds: 1.5 → 1.3 (1° 30′).
            const double sign = v < 0 ? -1 : 1;
            const double a = std::fabs(v);
            double degrees = std::floor(a);
            double minutesFull = (a - degrees) * 60;
            double minutes = std::floor(minutesFull + 1e-9);
            double seconds = std::round((minutesFull - minutes) * 60 * 1e6) / 1e6;
            if (seconds >= 60) { seconds -= 60; minutes += 1; }
            if (minutes >= 60) { minutes -= 60; degrees += 1; }
            return sign * (degrees + minutes / 100 + seconds / 10000);
        }
        case Fn::Deg: {
            const double sign = v < 0 ? -1 : 1;
            const double a = std::fabs(v);
            const double degrees = std::floor(a);
            const double rest = (a - degrees) * 100;
            const double minutes = std::floor(rest + 1e-9);
            const double seconds = (rest - minutes) * 100;
            return sign * (degrees + minutes / 60 + seconds / 3600);
        }
        }
        return invalid();
    }

    double eval(const Node *node) const {
        switch (node->kind) {
        case Node::Number: return node->value;
        case Node::Variable: return x;
        case Node::Negate: return -eval(node->left.get());
        case Node::Factorial: {
            const double v = eval(node->left.get());
            return std::isnan(v) ? v : factorial(v);
        }
        case Node::Call: {
            const double v = eval(node->left.get());
            return std::isnan(v) ? v : call(node->fn, v);
        }
        default:
            break;
        }

        const double a = eval(node->left.get());
        const double b = eval(node->right.get());
        if (std::isnan(a) || std::isnan(b))
            return notANumber;
        switch (node->kind) {
        case Node::Add: return checked(a + b);
        case Node::Subtract: return checked(a - b);
        case Node::Multiply: return checked(a * b);
        case Node::Divide: return b == 0 ? divideByZero() : checked(a / b);
        case Node::Modulo: {
            if (b == 0)
                return divideByZero();
            // Floored, so the result takes the divisor's sign: −7 mod 3 = 2.
            const double r = std::fmod(a, b);
            return (r != 0 && ((r < 0) != (b < 0))) ? r + b : r;
        }
        case Node::Power: {
            if (a == 0 && b < 0)
                return divideByZero();
            if (a < 0 && b != std::floor(b))
                return invalid();
            return checked(std::pow(a, b));
        }
        case Node::Root: {
            // a root b is the b-th root of a; odd roots of negatives are real.
            if (b == 0)
                return invalid();
            if (a < 0) {
                const bool oddInteger = b == std::floor(b) && std::fmod(std::fabs(b), 2.0) == 1.0;
                return oddInteger ? -std::pow(-a, 1 / b) : invalid();
            }
            return checked(std::pow(a, 1 / b));
        }
        case Node::LogBase:
            // a logb b is log base b of a: 8 logb 2 = 3.
            if (a <= 0 || b <= 0 || b == 1)
                return invalid();
            return std::log(a) / std::log(b);
        default:
            return invalid();
        }
    }
};
}

Expression::Expression() = default;
Expression::~Expression() = default;
Expression::Expression(Expression &&) noexcept = default;
Expression &Expression::operator=(Expression &&) noexcept = default;

Expression Expression::parse(const QString &text) {
    Expression expression;
    if (text.trimmed().isEmpty()) {
        expression.m_error = tr("Incomplete expression");
        return expression;
    }
    Parser parser(text);
    expression.m_root = parser.parseAll();
    expression.m_usesVariable = parser.usesVariable;
    if (!expression.m_root)
        expression.m_error = parser.error.isEmpty() ? tr("Invalid input") : parser.error;
    return expression;
}

double Expression::evaluate(double x, AngleUnit unit, QString *error) const {
    if (!m_root) {
        if (error)
            *error = m_error;
        return notANumber;
    }
    QString local;
    Evaluator evaluator{x, unit, error ? error : &local};
    const double value = evaluator.eval(m_root.get());
    if (std::isnan(value) && error && error->isEmpty())
        *error = tr("Invalid input");
    return value;
}

double Expression::evaluate(const QString &text, AngleUnit unit, QString *error) {
    return parse(text).evaluate(0, unit, error);
}

bool Expression::isFunctionName(const QString &name) {
    for (const FunctionName &function : functionNames) {
        if (name == QString::fromUtf8(function.name))
            return true;
    }
    return false;
}
