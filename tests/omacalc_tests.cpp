#include <QtTest>
#include <QClipboard>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRect>
#include <QSettings>
#include <QVariant>
#include <cmath>

#include "backend.h"
#include "converter.h"
#include "datecalc.h"
#include "expression.h"
#include "graphview.h"
#include "programmer.h"
#include "scientific.h"
#include "tape.h"
#include "systemtheme.h"
#include "translator.h"

namespace {
void press(Backend &calculator, const QString &keys) {
    const QStringList sequence = keys.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &key : sequence)
        calculator.pressKey(key);
}

// Keys separated by spaces; a word in brackets is a function: "30 [sin] =".
void press(ScientificCalc &calculator, const QString &keys) {
    for (const QString &key : keys.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (key.startsWith(QLatin1Char('[')) && key.endsWith(QLatin1Char(']')))
            calculator.applyFunction(key.mid(1, key.size() - 2));
        else
            calculator.pressKey(key);
    }
}

void press(ProgrammerCalc &calculator, const QString &keys) {
    for (const QString &key : keys.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        calculator.pressKey(key);
}

double eval(const QString &text, Expression::AngleUnit unit = Expression::Degrees, QString *error = nullptr) {
    QString local;
    return Expression::evaluate(text, unit, error ? error : &local);
}
}

class OmacalcTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void parsesDates() {
        DateCalc dates;
        dates.setToday(QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("2026-11-14"), QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("11/14/2026"), QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("11/14/26"), QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("7/4/76"), QDate(1976, 7, 4));
        QCOMPARE(dates.parseDate("Nov 14, 2026"), QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("november 14th 2026"), QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("14 Nov 2026"), QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("20261114"), QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("Dec 25"), QDate(2026, 12, 25));
        QCOMPARE(dates.parseDate("today"), QDate(2026, 11, 14));
        QCOMPARE(dates.parseDate("yesterday"), QDate(2026, 11, 13));
        QCOMPARE(dates.parseDate("+30"), QDate(2026, 12, 14));
        QCOMPARE(dates.parseDate("-2w"), QDate(2026, 10, 31));
        QCOMPARE(dates.parseDate("today + 3m"), QDate(2027, 2, 14));
        QCOMPARE(dates.parseDate("+1y"), QDate(2027, 11, 14));
        QVERIFY(!dates.parseDate("2026-02-30").isValid());
        QVERIFY(!dates.parseDate("banana").isValid());
    }

    void spansCalendarMonths() {
        int y = 0, m = 0, d = 0;
        DateCalc::yearsMonthsDays(QDate(2026, 1, 31), QDate(2026, 3, 1), &y, &m, &d);
        QCOMPARE(QList<int>({y, m, d}), QList<int>({0, 1, 1}));
        DateCalc::yearsMonthsDays(QDate(1990, 5, 20), QDate(2026, 11, 14), &y, &m, &d);
        QCOMPARE(QList<int>({y, m, d}), QList<int>({36, 5, 25}));
        DateCalc::yearsMonthsDays(QDate(2024, 2, 29), QDate(2025, 2, 28), &y, &m, &d);
        QCOMPARE(QList<int>({y, m, d}), QList<int>({1, 0, 0}));
    }

    void countsWeekdays() {
        // Mon Nov 9 → Fri Nov 13, 2026
        QCOMPARE(DateCalc::weekdaysBetween(QDate(2026, 11, 9), QDate(2026, 11, 13)), 4);
        QCOMPARE(DateCalc::weekdaysBetween(QDate(2026, 11, 13), QDate(2026, 11, 16)), 1);
        QCOMPARE(DateCalc::weekdaysBetween(QDate(2026, 11, 16), QDate(2026, 11, 13)), -1);
        QCOMPARE(DateCalc::weekdaysBetween(QDate(2026, 1, 1), QDate(2027, 1, 1)), 261);
        // Friday + 1 business day is Monday; Saturday + 1 is Monday too.
        QCOMPARE(DateCalc::addWeekdays(QDate(2026, 11, 13), 1), QDate(2026, 11, 16));
        QCOMPARE(DateCalc::addWeekdays(QDate(2026, 11, 14), 1), QDate(2026, 11, 16));
        QCOMPARE(DateCalc::addWeekdays(QDate(2026, 11, 16), -1), QDate(2026, 11, 13));
        QCOMPARE(DateCalc::addWeekdays(QDate(2026, 11, 9), 10), QDate(2026, 11, 23));
        QCOMPARE(DateCalc::addWeekdays(QDate(2026, 11, 9), 5), QDate(2026, 11, 16));
        for (int n = -40; n <= 40; ++n) {
            const QDate start(2026, 11, 9);
            QCOMPARE(DateCalc::weekdaysBetween(start, DateCalc::addWeekdays(start, n)), qint64(n));
        }
    }

    void computesDifferences() {
        DateCalc dates;
        dates.setToday(QDate(2026, 11, 14));
        QVariantMap result = dates.difference("2026-11-09", "2026-11-13", false);
        QVERIFY(result.value("ok").toBool());
        QCOMPARE(result.value("headline").toString(), QStringLiteral("4 days"));
        result = dates.difference("2026-11-09", "2026-11-13", true);
        QCOMPARE(result.value("headline").toString(), QStringLiteral("5 days"));
        const QVariantList rows = result.value("rows").toList();
        QCOMPARE(rows.at(2).toMap().value("value").toString(), QStringLiteral("5"));
        result = dates.difference("2000-01-01", "today", false);
        QCOMPARE(result.value("headline").toString(), QStringLiteral("9,814 days"));
        result = dates.difference("2026-11-14", "2026-11-01", false);
        QCOMPARE(result.value("headline").toString(), QStringLiteral("13 days earlier"));
        QVERIFY(!dates.difference("", "today", false).value("ok").toBool());
    }

    void shiftsDates() {
        DateCalc dates;
        dates.setToday(QDate(2026, 11, 14));
        QCOMPARE(dates.shift("2026-11-14", false, 0, 0, 0, 100, false).value("iso").toString(),
                 QStringLiteral("2027-02-22"));
        QCOMPARE(dates.shift("2026-01-31", false, 0, 1, 0, 0, false).value("iso").toString(),
                 QStringLiteral("2026-02-28"));
        QCOMPARE(dates.shift("2026-11-14", true, 1, 2, 1, 3, false).value("iso").toString(),
                 QStringLiteral("2025-09-04"));
        QCOMPARE(dates.shift("2026-11-13", false, 0, 0, 0, 1, true).value("iso").toString(),
                 QStringLiteral("2026-11-16"));
    }

    void expressionsFollowPrecedence() {
        QCOMPARE(eval("2 + 3 × 4"), 14.0);
        QCOMPARE(eval("(2 + 3) × 4"), 20.0);
        QCOMPARE(eval("2 ^ 3 ^ 2"), 512.0);
        QCOMPARE(eval("−2^2"), -4.0);
        QCOMPARE(eval("2^−1"), 0.5);
        QCOMPARE(eval("2π"), 2 * M_PI);
        QCOMPARE(eval("3(4)"), 12.0);
        QCOMPARE(eval("5!"), 120.0);
        QCOMPARE(eval("3!²"), 36.0);
        QCOMPARE(eval("|−4| + 1"), 5.0);
        QCOMPARE(eval("8 root 3"), 2.0);
        QCOMPARE(eval("−8 root 3"), -2.0);
        QCOMPARE(eval("8 logb 2"), 3.0);
        QCOMPARE(eval("−7 mod 3"), 2.0);
        QCOMPARE(eval("7 mod −3"), -2.0);
        QCOMPARE(eval("1.5E3"), 1500.0);
        QCOMPARE(eval("2E−2"), 0.02);
        QCOMPARE(eval("√9 + ∛27"), 6.0);
        QCOMPARE(eval("log(1000) + ln(e)"), 4.0);
        QCOMPARE(eval("10^2"), 100.0);
        QVERIFY(std::fabs(eval("0.5!") - std::sqrt(M_PI) / 2) < 1e-12);
    }

    void expressionsReportErrors() {
        QString error;
        eval("1 ÷ 0", Expression::Degrees, &error);
        QCOMPARE(error, QStringLiteral("Can't divide by zero"));
        error.clear();
        eval("√(−1)", Expression::Degrees, &error);
        QCOMPARE(error, QStringLiteral("Invalid input"));
        error.clear();
        eval("tan(90)", Expression::Degrees, &error);
        QCOMPARE(error, QStringLiteral("Invalid input"));
        error.clear();
        eval("171!", Expression::Degrees, &error);
        QCOMPARE(error, QStringLiteral("Overflow"));
        error.clear();
        eval("2 +", Expression::Degrees, &error);
        QVERIFY(!error.isEmpty());
        error.clear();
        eval("(2 + 3", Expression::Degrees, &error);
        QCOMPARE(error, QStringLiteral("Missing )"));
        QVERIFY(!Expression::parse("2 + y").isValid());
    }

    void trigonometryHitsExactValues() {
        QCOMPARE(eval("sin(180)"), 0.0);
        QCOMPARE(eval("sin(30)"), 0.5);
        QCOMPARE(eval("cos(90)"), 0.0);
        QCOMPARE(eval("cos(60)"), 0.5);
        QCOMPARE(eval("tan(45)"), 1.0);
        QCOMPARE(eval("sin(π)", Expression::Radians), 0.0);
        QCOMPARE(eval("sin(100)", Expression::Gradians), 1.0);
        QCOMPARE(eval("asin(1)"), 90.0);
        QCOMPARE(eval("acos(0)", Expression::Gradians), 100.0);
        QCOMPARE(eval("atan(1)", Expression::Radians), M_PI / 4);
        QCOMPARE(eval("acot(0)"), 90.0);
        QCOMPARE(eval("sec(60)"), 2.0);
        QVERIFY(std::fabs(eval("cosh(0)") - 1) < 1e-15);
        QVERIFY(std::fabs(eval("asinh(sinh(2))") - 2) < 1e-12);
        QVERIFY(std::fabs(eval("dms(1.5)") - 1.3) < 1e-12);
        QVERIFY(std::fabs(eval("deg(1.3)") - 1.5) < 1e-12);
        QVERIFY(std::fabs(eval("dms(10.2525)") - 10.1509) < 1e-9);
    }

    void scientificWrapsTheLastNumber() {
        ScientificCalc calc;
        press(calc, "3 0 [sin]");
        QCOMPARE(calc.text(), QStringLiteral("sin(30)"));
        QCOMPARE(calc.display(), QStringLiteral("0.5"));
        press(calc, "+ 4 [square] =");
        QCOMPARE(calc.expression(), QStringLiteral("sin(30) + 4² ="));
        QCOMPARE(calc.display(), QStringLiteral("16.5"));

        press(calc, "clear [sqrt] 1 6 ) =");
        QCOMPARE(calc.display(), QStringLiteral("4"));

        press(calc, "clear 5 sign [square] =");
        QCOMPARE(calc.expression(), QStringLiteral("(−5)² ="));
        QCOMPARE(calc.display(), QStringLiteral("25"));

        press(calc, "clear ( 2 + 3 ) [factorial] =");
        QCOMPARE(calc.display(), QStringLiteral("120"));

        press(calc, "clear 2 ^ 1 0 [reciprocal] =");
        QCOMPARE(calc.expression(), QStringLiteral("2 ^ (1/10) ="));

        press(calc, "clear 3 [tenPow] =");
        QCOMPARE(calc.display(), QStringLiteral("1,000"));

        press(calc, "clear angle");
        QCOMPARE(calc.angleUnit(), 1);
        press(calc, "pi [cos] =");
        QCOMPARE(calc.display(), QStringLiteral("−1"));
    }

    void scientificChainsAndEdits() {
        ScientificCalc calc;
        press(calc, "1 / 3 = * 3 =");
        QCOMPARE(calc.display(), QStringLiteral("1"));

        press(calc, "clear 2 * ( 3 + 4");
        QCOMPARE(calc.openParens(), 1);
        QCOMPARE(calc.display(), QStringLiteral("4"));
        press(calc, "+");
        QCOMPARE(calc.display(), QStringLiteral("14"));
        press(calc, "backspace");
        QCOMPARE(calc.text(), QStringLiteral("2 × (3 + 4"));
        press(calc, "clearEntry");
        QCOMPARE(calc.text(), QStringLiteral("2 × (3 + "));
        press(calc, "5 =");
        QCOMPARE(calc.display(), QStringLiteral("16"));

        press(calc, "clear 2 exp 5 sign =");
        QCOMPARE(calc.expression(), QStringLiteral("2E−5 ="));
        QCOMPARE(calc.display(), QStringLiteral("2E−5"));

        press(calc, "fe");
        QCOMPARE(calc.display(), QStringLiteral("2E−5"));
        press(calc, "fe clear 1 2 3 4 5 =");
        QCOMPARE(calc.display(), QStringLiteral("12,345"));

        press(calc, "clear 5 - - +");
        QCOMPARE(calc.text(), QStringLiteral("5 + "));
        press(calc, "clear [sin] 3 0");
        QCOMPARE(calc.text(), QStringLiteral("sin(30"));
        press(calc, "backspace backspace backspace");
        QCOMPARE(calc.text(), QString());

        press(calc, "clear 8 root 3 =");
        QCOMPARE(calc.display(), QStringLiteral("2"));
        press(calc, "clear 1 / 0 =");
        QVERIFY(calc.errored());
        QCOMPARE(calc.display(), QStringLiteral("Can't divide by zero"));
        press(calc, "7");
        QVERIFY(!calc.errored());
        QCOMPARE(calc.display(), QStringLiteral("7"));
    }

    void scientificReportsHistory() {
        ScientificCalc calc;
        Tape tape;
        connect(&calc, &ScientificCalc::evaluated, &tape, &Tape::addHistory);
        press(calc, "2 + 2 =");
        QCOMPARE(tape.history().size(), 1);
        const QVariantMap entry = tape.history().first().toMap();
        QCOMPARE(entry.value("expression").toString(), QStringLiteral("2 + 2"));
        QCOMPARE(entry.value("result").toString(), QStringLiteral("4"));
        calc.enterNumber(entry.value("value").toDouble());
        press(calc, "* 3 =");
        QCOMPARE(calc.display(), QStringLiteral("12"));
    }

    void standardHasUnaryKeysAndClearEntry() {
        Backend calc;
        press(calc, "9 sqrt");
        QCOMPARE(calc.display(), QStringLiteral("3"));
        press(calc, "clear 4 reciprocal");
        QCOMPARE(calc.display(), QStringLiteral("0.25"));
        press(calc, "clear 1 2 square");
        QCOMPARE(calc.display(), QStringLiteral("144"));
        press(calc, "clear 1 2 + 3 4 clearEntry 5 =");
        QCOMPARE(calc.display(), QStringLiteral("17"));
        press(calc, "clear 0 reciprocal");
        QCOMPARE(calc.display(), QStringLiteral("Error"));
        QCOMPARE(calc.expression(), QStringLiteral("1/(0)"));
        press(calc, "clear 5 + 4 sqrt =");
        QCOMPARE(calc.display(), QStringLiteral("7"));
    }

    void standardFeedsMemoryAndHistory() {
        Backend calc;
        Tape tape;
        connect(&calc, &Backend::evaluated, &tape, &Tape::addHistory);
        press(calc, "6 * 7 =");
        QCOMPARE(tape.history().size(), 1);
        QCOMPARE(tape.history().first().toMap().value("result").toString(), QStringLiteral("42"));
        tape.memoryStore(calc.currentNumber());
        tape.memoryAdd(8);
        QCOMPARE(tape.memoryRecall(), 50.0);
        tape.memoryStore(1);
        tape.memorySubtract(3);
        QCOMPARE(tape.memoryValues(), QList<double>({-2, 50}));
        tape.memoryItemAdd(1, 50);
        QCOMPARE(tape.memory().at(1).toMap().value("text").toString(), QStringLiteral("100"));
        calc.enterNumber(tape.memoryRecall());
        QCOMPARE(calc.display(), QStringLiteral("-2"));
        tape.memoryItemClear(0);
        QCOMPARE(tape.memoryRecall(), 100.0);
        tape.memoryClear();
        QVERIFY(!tape.hasMemory());
        tape.memorySubtract(5);
        QCOMPARE(tape.memoryRecall(), -5.0);
    }

    void programmerShowsEveryBase() {
        ProgrammerCalc calc;
        press(calc, "2 5 5");
        QCOMPARE(calc.hex(), QStringLiteral("FF"));
        QCOMPARE(calc.oct(), QStringLiteral("377"));
        QCOMPARE(calc.bin(), QStringLiteral("1111 1111"));
        QCOMPARE(calc.dec(), QStringLiteral("255"));
        press(calc, "sign");
        QCOMPARE(calc.dec(), QStringLiteral("−255"));
        QCOMPARE(calc.hex(), QStringLiteral("FFFF FFFF FFFF FF01"));
        calc.setWordBits(8);
        QCOMPARE(calc.hex(), QStringLiteral("1"));
        QCOMPARE(calc.dec(), QStringLiteral("1"));

        press(calc, "clear");
        calc.setBase(16);
        press(calc, "7 F");
        QCOMPARE(calc.dec(), QStringLiteral("127"));
        press(calc, "+ 1 =");
        QCOMPARE(calc.dec(), QStringLiteral("−128"));
        QCOMPARE(calc.display(), QStringLiteral("80"));
        calc.setBase(10);
        QCOMPARE(calc.display(), QStringLiteral("−128"));
        QVERIFY(!calc.digitAllowed("A"));
    }

    void programmerDoesBitwiseArithmetic() {
        ProgrammerCalc calc;
        press(calc, "1 2 and 1 0 =");
        QCOMPARE(calc.dec(), QStringLiteral("8"));
        press(calc, "clear 1 2 or 3 =");
        QCOMPARE(calc.dec(), QStringLiteral("15"));
        press(calc, "clear 1 2 xor 1 0 =");
        QCOMPARE(calc.dec(), QStringLiteral("6"));
        press(calc, "clear 0 not");
        QCOMPARE(calc.dec(), QStringLiteral("−1"));
        press(calc, "clear 1 + 2 * 3 =");
        QCOMPARE(calc.dec(), QStringLiteral("7"));
        press(calc, "clear ( 1 + 2 ) * 3 =");
        QCOMPARE(calc.dec(), QStringLiteral("9"));
        QCOMPARE(calc.expression(), QStringLiteral("(1 + 2) × 3 ="));
        press(calc, "clear 7 / 2 =");
        QCOMPARE(calc.dec(), QStringLiteral("3"));
        press(calc, "clear 7 sign % 3 =");
        QCOMPARE(calc.dec(), QStringLiteral("−1"));
        press(calc, "clear 1 / 0 =");
        QVERIFY(calc.errored());
        press(calc, "clear 1 lsh 4 =");
        QCOMPARE(calc.dec(), QStringLiteral("16"));
        press(calc, "clear 1 2 nand 1 0 =");
        QCOMPARE(calc.dec(), QStringLiteral("−9"));
    }

    void programmerShiftsByMode() {
        ProgrammerCalc calc;
        calc.setWordBits(8);
        calc.setBase(2);
        press(calc, "1 0 0 0 0 0 0 1");
        QCOMPARE(calc.dec(), QStringLiteral("−127"));
        press(calc, "rsh 1 =");
        QCOMPARE(calc.bin(), QStringLiteral("1100 0000"));  // arithmetic keeps the sign
        calc.setShiftMode(1);
        press(calc, "clear 1 0 0 0 0 0 0 1 rsh 1 =");
        QCOMPARE(calc.bin(), QStringLiteral("100 0000"));
        calc.setShiftMode(2);
        press(calc, "clear 1 0 0 0 0 0 0 1 rsh 1 =");
        QCOMPARE(calc.bin(), QStringLiteral("1100 0000"));
        press(calc, "clear 1 0 0 0 0 0 0 1 lsh 1 =");
        QCOMPARE(calc.bin(), QStringLiteral("11"));
        calc.setShiftMode(3);
        press(calc, "clear 1 0 0 0 0 0 0 1 lsh 1 =");
        QCOMPARE(calc.bin(), QStringLiteral("10"));
        QVERIFY(calc.carry());
        press(calc, "lsh 1 =");
        QCOMPARE(calc.bin(), QStringLiteral("101"));
        QVERIFY(!calc.carry());
    }

    void programmerTogglesBits() {
        ProgrammerCalc calc;
        calc.toggleBit(0);
        calc.toggleBit(3);
        QCOMPARE(calc.dec(), QStringLiteral("9"));
        QVERIFY(calc.bit(3));
        calc.toggleBit(63);
        QCOMPARE(calc.hex(), QStringLiteral("8000 0000 0000 0009"));
        calc.setWordBits(16);
        QCOMPARE(calc.hex(), QStringLiteral("9"));
        calc.toggleBit(20);
        QCOMPARE(calc.dec(), QStringLiteral("9"));
        press(calc, "9");  // continues the typed number
        QCOMPARE(calc.dec(), QStringLiteral("99"));
    }

    void convertsUnits() {
        UnitConverter converter(nullptr, m_settingsDirectory.path() + QStringLiteral("/no-cache"));
        QCOMPARE(converter.convertText("length", "in", "cm", "1"), QStringLiteral("2.54"));
        QCOMPARE(converter.convertText("length", "mi", "km", "1"), QStringLiteral("1.609344"));
        QCOMPARE(converter.convertText("temperature", "c", "f", "100"), QStringLiteral("212"));
        QCOMPARE(converter.convertText("temperature", "f", "c", "−40"), QStringLiteral("−40"));
        QCOMPARE(converter.convertText("temperature", "c", "k", "0"), QStringLiteral("273.15"));
        QCOMPARE(converter.convertText("volume", "galUs", "l", "1"), QStringLiteral("3.785411784"));
        QCOMPARE(converter.convertText("volume", "cupUs", "flozUs", "1"), QStringLiteral("8"));
        QCOMPARE(converter.convertText("weight", "lb", "oz", "1"), QStringLiteral("16"));
        QCOMPARE(converter.convertText("data", "gibyte", "mibyte", "1"), QStringLiteral("1,024"));
        QCOMPARE(converter.convertText("data", "byte", "bit", "1"), QStringLiteral("8"));
        QCOMPARE(converter.convertText("time", "d", "h", "1"), QStringLiteral("24"));
        QCOMPARE(converter.convertText("angle", "rad", "deg", "3.14159265358979"), QStringLiteral("180"));
        QCOMPARE(converter.convertText("area", "ac", "ft2", "1"), QStringLiteral("43,560"));
        QCOMPARE(converter.convertText("speed", "kn", "kmh", "1"), QStringLiteral("1.852"));
        QCOMPARE(converter.convertText("pressure", "atm", "pa", "1"), QStringLiteral("101,325"));
        QCOMPARE(converter.convertText("length", "m", "ft", "1,000"), QStringLiteral("3,280.83989501312"));
        QCOMPARE(converter.convertText("length", "m", "ft", "abc"), QString());
        QCOMPARE(converter.allConversions("temperature", "c", "0").size(), 3);
        QCOMPARE(converter.categories().size(), 13);
        QVERIFY(!converter.currencyReady());
    }

    void convertsCurrencyFromRates() {
        UnitConverter converter(nullptr, m_settingsDirectory.path() + QStringLiteral("/no-cache"));
        converter.setRates({{"USD", 1.25}, {"GBP", 0.8}}, {{"USD", "US Dollar"}, {"EUR", "Euro"}}, "2026-11-13");
        QVERIFY(converter.currencyReady());
        QCOMPARE(converter.convertText("currency", "USD", "EUR", "10"), QStringLiteral("8.00"));
        QCOMPARE(converter.convertText("currency", "GBP", "USD", "100"), QStringLiteral("156.25"));
        QCOMPARE(converter.convertText("currency", "EUR", "GBP", "0.01"), QStringLiteral("0.008"));
        QCOMPARE(converter.rateLine("USD", "GBP"), QStringLiteral("1 USD = 0.64 GBP"));
        QCOMPARE(converter.currencyStatus(), QStringLiteral("Rates from 2026-11-13"));
        QCOMPARE(converter.units("currency").size(), 3);
    }

    void graphParsesEquations() {
        QCOMPARE(GraphView::stripLeftSide("y = x^2"), QStringLiteral("x^2"));
        QCOMPARE(GraphView::stripLeftSide("f(x)=sin(x)"), QStringLiteral("sin(x)"));
        QCOMPARE(GraphView::niceStep(0.3), 0.2);
        QCOMPARE(GraphView::niceStep(7), 5.0);
        QCOMPARE(GraphView::niceStep(80), 100.0);
        const Expression parabola = Expression::parse("2x^2 − 3");
        QVERIFY(parabola.usesVariable());
        QCOMPARE(parabola.evaluate(2), 5.0);
        const Expression exponential = Expression::parse("ex");
        QCOMPARE(exponential.evaluate(2), 2 * std::exp(1.0));
        GraphView view;
        view.setEquations({"x^2", "", "y = sin("});
        QCOMPARE(view.errors().at(0), QString());
        QCOMPARE(view.errors().at(1), QString());
        QVERIFY(!view.errors().at(2).isEmpty());
    }

    void startsAtZero() {
        Backend calculator;
        QCOMPARE(calculator.display(), QStringLiteral("0"));
        QCOMPARE(calculator.expression(), QString());
    }

    void calculatesPowers() {
        Backend calculator;
        press(calculator, "2 ^ 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("4"));

        press(calculator, "clear 2 × 3 ^ 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("18"));

        press(calculator, "clear 2 ^ 3 × 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("16"));

        press(calculator, "clear 2 ^ 3 ^ 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("512"));

        press(calculator, "clear 2 ^ 0 =");
        QCOMPARE(calculator.display(), QStringLiteral("1"));

        press(calculator, "clear 2 ^ 3 = ^ 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("64"));

        press(calculator, "clear 2 sign ^ 3 =");
        QCOMPARE(calculator.display(), QStringLiteral("-8"));

        press(calculator, "clear 0 ^ 0 =");
        QCOMPARE(calculator.display(), QStringLiteral("1"));

        press(calculator, "clear 0 ^ 1 sign =");
        QCOMPARE(calculator.display(), QStringLiteral("Error"));

        press(calculator, "clear 2 sign ^ 0 . 5 =");
        QCOMPARE(calculator.display(), QStringLiteral("Error"));
        press(calculator, "5");
        QCOMPARE(calculator.display(), QStringLiteral("5"));
    }

    void calculatesWithPrecedence() {
        Backend calculator;
        press(calculator, "4 2 × 3 + 7 =");
        QCOMPARE(calculator.display(), QStringLiteral("133"));
        QCOMPARE(calculator.expression(), QStringLiteral("42 × 3 + 7"));

        press(calculator, "clear 2 + 3 × 4 =");
        QCOMPARE(calculator.display(), QStringLiteral("14"));

        press(calculator, "clear 1 0 - 4 ÷ 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("8"));
    }

    void showsEntryWhileTyping() {
        Backend calculator;
        press(calculator, "4 2 ×");
        QCOMPARE(calculator.display(), QStringLiteral("42"));
        QCOMPARE(calculator.expression(), QStringLiteral("42 ×"));

        press(calculator, "3");
        QCOMPARE(calculator.display(), QStringLiteral("3"));
    }

    void handlesDecimals() {
        Backend calculator;
        press(calculator, ". 5 + . 2 5 =");
        QCOMPARE(calculator.display(), QStringLiteral("0.75"));

        // Binary-float noise stays out of the display.
        press(calculator, "clear 0 . 1 + 0 . 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("0.3"));

        // A second decimal point in one number is ignored.
        press(calculator, "clear 1 . 5 . 5");
        QCOMPARE(calculator.display(), QStringLiteral("1.55"));
    }

    void divisionByZeroErrors() {
        Backend calculator;
        press(calculator, "1 ÷ 0 =");
        QCOMPARE(calculator.display(), QStringLiteral("Error"));

        // Digits recover from an error without an explicit clear.
        press(calculator, "5");
        QCOMPARE(calculator.display(), QStringLiteral("5"));
    }

    void percentOfRunningTotal() {
        Backend calculator;

        // With a pending + or −, x% means x percent of the running total.
        press(calculator, "2 0 0 + 1 0 % =");
        QCOMPARE(calculator.display(), QStringLiteral("220"));

        press(calculator, "clear 2 0 0 - 1 0 % =");
        QCOMPARE(calculator.display(), QStringLiteral("180"));

        // With ×, ÷, or ^, or standalone, x% is simply x ÷ 100.
        press(calculator, "clear 2 0 0 × 1 0 % =");
        QCOMPARE(calculator.display(), QStringLiteral("20"));

        press(calculator, "clear 2 ^ 1 0 % =");
        QCOMPARE(calculator.display(), Backend::formatNumber(std::pow(2.0, 0.1)));

        press(calculator, "clear 5 0 %");
        QCOMPARE(calculator.display(), QStringLiteral("0.5"));

        // After equals, percent picks up from the result.
        press(calculator, "clear 4 0 + 1 0 = %");
        QCOMPARE(calculator.display(), QStringLiteral("0.5"));
    }

    void percentAndSign() {
        Backend calculator;
        press(calculator, "8 sign");
        QCOMPARE(calculator.display(), QStringLiteral("-8"));
        press(calculator, "sign");
        QCOMPARE(calculator.display(), QStringLiteral("8"));

        press(calculator, "clear 4 + 8 sign =");
        QCOMPARE(calculator.display(), QStringLiteral("-4"));
    }

    void signStartsNewOperand() {
        Backend calculator;

        // Sign with nothing typed starts a fresh negative operand rather than
        // negating the previous one: 4 + ± 2 = is 4 + (-2), not 4 + (-42).
        press(calculator, "4 + sign 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("2"));
        QCOMPARE(calculator.expression(), QStringLiteral("4 + -2"));

        press(calculator, "clear sign");
        QCOMPARE(calculator.display(), QStringLiteral("-0"));
        press(calculator, "5");
        QCOMPARE(calculator.display(), QStringLiteral("-5"));
    }

    void chainsWithFullPrecision() {
        Backend calculator;

        // Chaining continues from the exact value, not the rounded display.
        press(calculator, "1 ÷ 3 = × 3 =");
        QCOMPARE(calculator.display(), QStringLiteral("1"));

        // Integers within the 15-digit entry limit survive exactly.
        press(calculator, "clear 9 9 9 9 9 9 9 9 9 9 9 9 9 9 =");
        QCOMPARE(calculator.display(), QStringLiteral("99,999,999,999,999"));
    }

    void capsEntryAtFifteenDigits() {
        Backend calculator;
        press(calculator, "1 2 3 4 5 6 7 8 9 1 2 3 4 5 6 7 8");
        QCOMPARE(calculator.display(), QStringLiteral("123,456,789,123,456"));

        // The decimal point does not count against the digit cap.
        press(calculator, "clear . 1 2 3 4 5 6 7 8 9 1 2 3 4 5 6 7");
        QCOMPARE(calculator.display(), QStringLiteral("0.123456789123456"));
    }

    void backspaceEdits() {
        Backend calculator;
        press(calculator, "1 2 3 backspace");
        QCOMPARE(calculator.display(), QStringLiteral("12"));

        press(calculator, "backspace backspace backspace");
        QCOMPARE(calculator.display(), QStringLiteral("0"));
    }

    void treatsExponentFormAsAWholeValue() {
        Backend calculator;

        // A digit after a tiny percent starts a new number rather than
        // extending the exponent of 1e-05 into 1e-055.
        press(calculator, ". 0 0 1 % 5");
        QCOMPARE(calculator.display(), QStringLiteral("5"));

        press(calculator, "clear . 0 0 1 % .");
        QCOMPARE(calculator.display(), QStringLiteral("0."));

        // Backspace on 1e+15 clears it rather than leaving 1e+1, which is 10.
        press(calculator, "clear 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 × 1 0 = backspace");
        QCOMPARE(calculator.display(), QStringLiteral("0"));

        press(calculator, "clear . 0 0 1 % sign × 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("-2e-05"));

        // A fresh number replaces only the operand, not the pending sum, and
        // a decimal point starts one too rather than landing after 1e-05.
        press(calculator, "clear 2 + . 0 0 1 % 5 =");
        QCOMPARE(calculator.display(), QStringLiteral("7"));
        press(calculator, "clear 2 + . 0 0 1 % . 5 =");
        QCOMPARE(calculator.display(), QStringLiteral("2.5"));

        // Backspace leaves a zero operand, not the 2 before it.
        press(calculator, "clear 2 + . 0 0 1 % backspace");
        QCOMPARE(calculator.display(), QStringLiteral("0"));
        QCOMPARE(calculator.expression(), QStringLiteral("2 +"));
    }

    void percentOfAHugeTotal() {
        Backend calculator;

        // 1e308 is near the largest double; taking 100 % of it must not
        // overflow on the way there.
        press(calculator, "1 0 ^ 3 0 8 = - 1 0 0 % =");
        QCOMPARE(calculator.display(), QStringLiteral("0"));

        // A percentage beyond any double is an error, not an "inf" entry.
        press(calculator, "clear 1 0 ^ 3 0 8 = + 1 0 0 0 %");
        QCOMPARE(calculator.display(), QStringLiteral("Error"));
        QCOMPARE(calculator.expression(), QStringLiteral("1e+308 + 1,000 %"));
    }

    void chainsFromResult() {
        Backend calculator;
        press(calculator, "6 × 7 = × 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("84"));
        QCOMPARE(calculator.expression(), QStringLiteral("42 × 2"));

        // A digit after equals starts fresh instead of appending to the result.
        press(calculator, "9");
        QCOMPARE(calculator.display(), QStringLiteral("9"));
        QCOMPARE(calculator.expression(), QString());
    }

    void replacesDanglingOperator() {
        Backend calculator;
        press(calculator, "4 + × 2 =");
        QCOMPARE(calculator.display(), QStringLiteral("8"));

        // Equals with a trailing operator drops it.
        press(calculator, "clear 9 + =");
        QCOMPARE(calculator.display(), QStringLiteral("9"));
    }

    void pastesNumbers() {
        Backend calculator;
        QClipboard *clipboard = QGuiApplication::clipboard();

        clipboard->setText(QStringLiteral(" 42.5 "));
        calculator.pasteNumber();
        QCOMPARE(calculator.display(), QStringLiteral("42.5"));

        // A pasted number is a normal entry that calculates like any other.
        press(calculator, "+ . 5 =");
        QCOMPARE(calculator.display(), QStringLiteral("43"));

        // Decimal commas are welcome; garbage is ignored.
        clipboard->setText(QStringLiteral("1,5"));
        calculator.pasteNumber();
        QCOMPARE(calculator.display(), QStringLiteral("1.5"));

        clipboard->setText(QStringLiteral("not a number"));
        calculator.pasteNumber();
        QCOMPARE(calculator.display(), QStringLiteral("1.5"));

        // Thousands separators, as the display shows them, paste back in.
        clipboard->setText(QStringLiteral("1,234,567.89"));
        calculator.pasteNumber();
        QCOMPARE(calculator.display(), QStringLiteral("1,234,567.89"));

        clipboard->setText(QStringLiteral("−12,000"));
        calculator.pasteNumber();
        QCOMPARE(calculator.display(), QStringLiteral("-12,000"));
    }

    void groupsThousands() {
        Backend calculator;
        press(calculator, "1 2 3 4");
        QCOMPARE(calculator.display(), QStringLiteral("1,234"));

        // The fraction is not grouped, and a trailing point stays visible.
        press(calculator, ". 5 6 7 8");
        QCOMPARE(calculator.display(), QStringLiteral("1,234.5678"));

        press(calculator, "× 1 0 0 0 =");
        QCOMPARE(calculator.display(), QStringLiteral("1,234,567.8"));
        QCOMPARE(calculator.expression(), QStringLiteral("1,234.5678 × 1,000"));

        // Chaining and editing after a result work from the plain value.
        press(calculator, "+ 1 =");
        QCOMPARE(calculator.display(), QStringLiteral("1,234,568.8"));
        press(calculator, "backspace backspace");
        QCOMPARE(calculator.display(), QStringLiteral("1,234,568"));

        press(calculator, "clear 1 0 0 0 0 0 0 sign");
        QCOMPARE(calculator.display(), QStringLiteral("-1,000,000"));
    }

    void copiesWithoutSeparators() {
        Backend calculator;
        press(calculator, "1 2 3 4 5 6 7 . 5");
        calculator.copyResult();
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("1234567.5"));
    }

    void groupsDigits() {
        QCOMPARE(Backend::groupDigits(QStringLiteral("0")), QStringLiteral("0"));
        QCOMPARE(Backend::groupDigits(QStringLiteral("999")), QStringLiteral("999"));
        QCOMPARE(Backend::groupDigits(QStringLiteral("1000")), QStringLiteral("1,000"));
        QCOMPARE(Backend::groupDigits(QStringLiteral("-100000")), QStringLiteral("-100,000"));
        QCOMPARE(Backend::groupDigits(QStringLiteral("0.123456")), QStringLiteral("0.123456"));
        QCOMPARE(Backend::groupDigits(QStringLiteral("1.5e+20")), QStringLiteral("1.5e+20"));
        QCOMPARE(Backend::groupDigits(QStringLiteral("Error")), QStringLiteral("Error"));
    }

    void evaluatesTokens() {
        bool ok = false;
        QCOMPARE(Backend::evaluateTokens({"2", "+", "3", "×", "4"}, &ok), 14.0);
        QVERIFY(ok);

        QCOMPARE(Backend::evaluateTokens({"2", "^", "3"}, &ok), 8.0);
        QVERIFY(ok);

        QCOMPARE(Backend::evaluateTokens({"2", "^", "3", "^", "2"}, &ok), 512.0);
        QVERIFY(ok);

        Backend::evaluateTokens({"2", "+"}, &ok);
        QVERIFY(!ok);

        Backend::evaluateTokens({"1", "÷", "0"}, &ok);
        QVERIFY(!ok);
    }

    void opensAtTheDesignSizeForTheTextScale() {
        Backend calculator;
        calculator.setTextScale(1);
        QVariantMap size = calculator.defaultWindowSize();
        QCOMPARE(size.value(QStringLiteral("width")).toInt(), 400);
        QCOMPARE(size.value(QStringLiteral("height")).toInt(), 568);

        calculator.setTextScale(1.5);
        size = calculator.defaultWindowSize();
        QCOMPARE(size.value(QStringLiteral("width")).toInt(), 600);
        QCOMPARE(size.value(QStringLiteral("height")).toInt(), 852);

        // Below the design size the window stays at Main.qml's minimum.
        calculator.setTextScale(0.5);
        size = calculator.defaultWindowSize();
        QCOMPARE(size.value(QStringLiteral("width")).toInt(), 300);
        QCOMPARE(size.value(QStringLiteral("height")).toInt(), 430);

        calculator.setTextScale(qQNaN());
        size = calculator.defaultWindowSize();
        QCOMPARE(size.value(QStringLiteral("width")).toInt(), 400);
        QCOMPARE(size.value(QStringLiteral("height")).toInt(), 568);
    }

    void dropsASavedWindowRectangle() {
        QSettings written;
        written.setValue(QStringLiteral("window/geometry"), QRect(0, 0, 1896, 1030));
        written.setValue(QStringLiteral("window/maximized"), true);
        written.setValue(QStringLiteral("unrelated"), QStringLiteral("kept"));
        written.sync();

        Backend calculator;
        Q_UNUSED(calculator);

        QSettings stored;
        QVERIFY(!stored.contains(QStringLiteral("window/geometry")));
        QVERIFY(!stored.contains(QStringLiteral("window/maximized")));
        QCOMPARE(stored.value(QStringLiteral("unrelated")).toString(), QStringLiteral("kept"));
    }

    void normalizesThePortalTextScale() {
        bool known = false;
        QCOMPARE(SystemTheme::normalizedTextScale(QVariant(1.25), &known), 1.25);
        QVERIFY(known);

        known = false;
        QCOMPARE(SystemTheme::normalizedTextScale(QVariant(4.0), &known), 3.0);
        QVERIFY(known);

        known = false;
        QCOMPARE(SystemTheme::normalizedTextScale(QVariant(0.1), &known), 0.5);
        QVERIFY(known);

        known = false;
        QCOMPARE(SystemTheme::normalizedTextScale(QVariant(0.0), &known), 1.0);
        QVERIFY(!known);

        known = false;
        QCOMPARE(SystemTheme::normalizedTextScale(QVariant(QStringLiteral("nope")), &known), 1.0);
        QVERIFY(!known);
    }

    void readsThePortalTextScaleWithoutStalling() {
        QElapsedTimer timer;
        timer.start();
        SystemTheme theme;
        QVERIFY(timer.elapsed() < 2000);
        QVERIFY(theme.textScale() >= 0.5);
        QVERIFY(theme.textScale() <= 3.0);
    }

    void everyCatalogParses() {
        // A broken catalog loads as nothing, which shows English rather than failing.
        const QString catalogs = QFINDTESTDATA("../src/i18n");
        QVERIFY(!catalogs.isEmpty());
        const QStringList files = QDir(catalogs).entryList({QStringLiteral("*.json")});
        QVERIFY(!files.isEmpty());
        for (const QString &name : files) {
            QFile file(catalogs + QLatin1Char('/') + name);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QJsonParseError error;
            const QJsonDocument catalog = QJsonDocument::fromJson(file.readAll(), &error);
            QVERIFY2(error.error == QJsonParseError::NoError && catalog.isObject(),
                     qPrintable(name + QStringLiteral(": ") + error.errorString()));
            for (const QJsonValue &value : catalog.object())
                QVERIFY2(value.isString() && !value.toString().isEmpty(), qPrintable(name));
        }
    }

    void showsErrorInTheOmarchyLanguage() {
        QTemporaryDir directory;
        QFile language(directory.filePath(QStringLiteral("language")));
        QVERIFY(language.open(QIODevice::WriteOnly));
        language.write("da\n");
        language.close();

        Translator translator(language.fileName(), QFINDTESTDATA("../src/i18n"));
        QCoreApplication::installTranslator(&translator);
        Backend calculator;
        press(calculator, "1 ÷ 0 =");
        const QString display = calculator.display();
        QCoreApplication::removeTranslator(&translator);
        QGuiApplication::setLayoutDirection(Qt::LeftToRight);
        QLocale::setDefault(QLocale::c());

        QCOMPARE(display, QStringLiteral("Fejl"));
    }

    void formatsNumbers() {
        QCOMPARE(Backend::formatNumber(133), QStringLiteral("133"));
        QCOMPARE(Backend::formatNumber(0.1 + 0.2), QStringLiteral("0.3"));
        QCOMPARE(Backend::formatNumber(-0.0), QStringLiteral("0"));
        QCOMPARE(Backend::formatNumber(1e15), QStringLiteral("1e+15"));
    }

    void loadsCurrentOmarchyTheme() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString themeDirectory = homeDirectory.path()
            + QStringLiteral("/.local/state/omarchy/current/theme");
        QVERIFY(QDir().mkpath(themeDirectory));

        QFile colorsFile(themeDirectory + QStringLiteral("/colors.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\" # a light theme\n"
            "accent = \"#112233\" # button accent\n"
            "selection = \"#445566\" # selection color\n"
            "background = \"#fefefe\" # canvas\n"
            "foreground = \"#101010\" # text\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend calculator;
        QCOMPARE(calculator.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(calculator.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(calculator.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(calculator.themeSelection(), QStringLiteral("#445566"));
        QVERIFY(!calculator.darkMode());

        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
        const QByteArray invalidPalette(
            "mode = \"light\"\n"
            "background = \"not a color\" # invalid canvas\n"
            "foreground = #101010 # bare hex text\n");
        QCOMPARE(colorsFile.write(invalidPalette), qint64(invalidPalette.size()));
        colorsFile.close();

        Backend withInvalidColor;
        QCOMPARE(withInvalidColor.themeBackground(), QStringLiteral("#ffffff"));
        QCOMPARE(withInvalidColor.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(withInvalidColor.themeAccent(), QStringLiteral("#2077b2"));
        QCOMPARE(withInvalidColor.themeSelection(), QStringLiteral("#2077b2"));
        QVERIFY(!withInvalidColor.darkMode());
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmacalcTest)
#include "omacalc_tests.moc"
