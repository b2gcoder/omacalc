#pragma once

#include <QString>
#include <memory>

// A parsed arithmetic expression, shared by the Scientific page (one value)
// and the Graphing page (evaluated once per pixel across x, so parsing happens
// once and evaluate() only walks the tree).
//
// The grammar is what the Scientific keypad types: + − × ÷ mod, ^ and the
// infix "root" (8 root 3 = 2) and "logb" (8 logb 2 = 3), postfix !, unary
// minus, parentheses, |x| bars, implicit multiplication (2π, 3(4), 2x), the
// constants π and e, the variable x, and the functions listed in
// expression.cpp.
class Expression {
public:
    enum AngleUnit { Degrees = 0, Radians = 1, Gradians = 2 };

    struct Node;

    Expression();
    ~Expression();
    Expression(Expression &&) noexcept;
    Expression &operator=(Expression &&) noexcept;

    static Expression parse(const QString &text);

    bool isValid() const { return m_root != nullptr; }
    QString error() const { return m_error; }
    bool usesVariable() const { return m_usesVariable; }

    // NaN when the expression is undefined there; *error says why for the
    // Scientific display ("Can't divide by zero"), and stays empty for a
    // plain NaN the graph simply skips.
    double evaluate(double x = 0, AngleUnit unit = Radians, QString *error = nullptr) const;

    // Parse and evaluate in one go.
    static double evaluate(const QString &text, AngleUnit unit, QString *error);

    // Known names, longest first, so "exp" wins over "e" followed by "xp".
    static bool isFunctionName(const QString &name);

private:
    std::unique_ptr<Node> m_root;
    QString m_error;
    bool m_usesVariable = false;
};
