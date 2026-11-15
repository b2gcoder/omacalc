#include "datecalc.h"

#include <QLocale>
#include <QRegularExpression>
#include <QStringList>
#include <QTimeZone>
#include <cstdlib>

#include "backend.h"

DateCalc::DateCalc(QObject *parent) : QObject(parent) {}

namespace {
bool isWeekend(const QDate &date) {
    return date.dayOfWeek() >= Qt::Saturday;
}

QDate applyOffset(const QDate &base, qint64 amount, QChar unit) {
    switch (unit.toLower().unicode()) {
    case 'w': return base.addDays(amount * 7);
    case 'm': return base.addMonths(int(amount));
    case 'y': return base.addYears(int(amount));
    default: return base.addDays(amount);
    }
}
}

QDate DateCalc::parseDate(const QString &text) const {
    QString s = text.simplified();
    if (s.isEmpty())
        return {};
    const QString lower = s.toLower();

    if (lower == QLatin1String("today") || lower == QLatin1String("now"))
        return today();
    if (lower == QLatin1String("tomorrow"))
        return today().addDays(1);
    if (lower == QLatin1String("yesterday"))
        return today().addDays(-1);

    // Offsets from today: +30, -2w, today + 3m, today-1y.
    static const QRegularExpression offset(
        QStringLiteral("^(?:today|now)?\\s*([+-])\\s*(\\d{1,6})\\s*(d|days?|w|weeks?|m|months?|y|years?)?$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch relative = offset.match(s);
    if (relative.hasMatch()) {
        qint64 amount = relative.captured(2).toLongLong();
        if (relative.captured(1) == QLatin1String("-"))
            amount = -amount;
        const QString unit = relative.captured(3);
        return applyOffset(today(), amount, unit.isEmpty() ? QChar('d') : unit.at(0));
    }

    // Normalize separators and drop ordinal suffixes ("Nov 14th, 2026").
    s.replace(QRegularExpression(QStringLiteral("(\\d)(st|nd|rd|th)\\b"),
                                 QRegularExpression::CaseInsensitiveOption),
              QStringLiteral("\\1"));
    s.replace(QLatin1Char(','), QLatin1Char(' '));
    s.replace(QLatin1Char('.'), QLatin1Char('/'));
    s = s.simplified();

    const QLocale english(QLocale::English, QLocale::UnitedStates);
    static const QStringList fourDigitYear = {
        QStringLiteral("yyyy-M-d"), QStringLiteral("yyyy/M/d"), QStringLiteral("yyyyMMdd"),
        QStringLiteral("M/d/yyyy"), QStringLiteral("M-d-yyyy"),
        QStringLiteral("MMM d yyyy"), QStringLiteral("MMMM d yyyy"),
        QStringLiteral("d MMM yyyy"), QStringLiteral("d MMMM yyyy"),
        QStringLiteral("ddd MMM d yyyy"), QStringLiteral("dddd MMMM d yyyy"),
        QStringLiteral("ddd MMMM d yyyy"), QStringLiteral("dddd MMM d yyyy"),
    };
    for (const QString &format : fourDigitYear) {
        QDate date = english.toDate(s, format);
        if (!date.isValid() && format.contains(QLatin1String("MMM")))
            date = english.toDate(s.toLower().replace(0, 1, s.at(0).toUpper()), format);
        if (date.isValid())
            return date;
    }

    static const QStringList twoDigitYear = {
        QStringLiteral("M/d/yy"), QStringLiteral("M-d-yy"),
    };
    for (const QString &format : twoDigitYear) {
        QDate date = english.toDate(s, format);
        if (date.isValid()) {
            // Qt reads yy as 19yy; 00–49 more likely means this century.
            if (date.year() < 1950)
                date = date.addYears(100);
            return date;
        }
    }

    // Month and day without a year: this year.
    static const QStringList noYear = {
        QStringLiteral("M/d"), QStringLiteral("MMM d"), QStringLiteral("MMMM d"),
        QStringLiteral("d MMM"), QStringLiteral("d MMMM"),
    };
    for (const QString &format : noYear) {
        const QDate date = english.toDate(s + QStringLiteral(" ") + QString::number(today().year()),
                                          format + QStringLiteral(" yyyy"));
        if (date.isValid())
            return date;
    }

    return {};
}

void DateCalc::yearsMonthsDays(QDate a, QDate b, int *years, int *months, int *days) {
    if (a > b)
        std::swap(a, b);
    int total = (b.year() - a.year()) * 12 + (b.month() - a.month());
    while (total > 0 && a.addMonths(total) > b)
        --total;
    *years = total / 12;
    *months = total % 12;
    *days = int(a.addMonths(total).daysTo(b));
}

qint64 DateCalc::weekdaysBetween(const QDate &a, const QDate &b) {
    if (a > b)
        return -weekdaysBetween(b, a);
    const qint64 span = a.daysTo(b);
    qint64 result = (span / 7) * 5;
    QDate cursor = a.addDays((span / 7) * 7);
    while (cursor < b) {
        cursor = cursor.addDays(1);
        if (!isWeekend(cursor))
            ++result;
    }
    return result;
}

QDate DateCalc::addWeekdays(const QDate &start, qint64 count) {
    const int step = count < 0 ? -1 : 1;
    qint64 remaining = std::llabs(count);
    QDate cursor = start;
    // Whole weeks jump at once; a week always holds five weekdays.
    if (remaining > 5) {
        const qint64 weeks = (remaining - 1) / 5;
        cursor = cursor.addDays(step * weeks * 7);
        remaining -= weeks * 5;
    }
    while (remaining > 0) {
        cursor = cursor.addDays(step);
        if (!isWeekend(cursor))
            --remaining;
    }
    return cursor;
}

QString DateCalc::prettyDate(const QDate &date) {
    return QLocale().toString(date, QStringLiteral("dddd, MMMM d, yyyy"));
}

QString DateCalc::count(qint64 value) {
    return Backend::groupDigits(QString::number(value));
}

QString DateCalc::unit(qint64 value, const QString &one, const QString &many) {
    return count(value) + QLatin1Char(' ') + (std::llabs(value) == 1 ? one : many);
}

QVariantMap DateCalc::row(const QString &label, const QString &value) {
    return {{QStringLiteral("label"), label}, {QStringLiteral("value"), value}};
}

QVariantMap DateCalc::failure(const QString &message) {
    return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), message}};
}

QString DateCalc::spanText(int years, int months, int days) const {
    QStringList parts;
    if (years)
        parts << unit(years, tr("year"), tr("years"));
    if (months)
        parts << unit(months, tr("month"), tr("months"));
    if (days || parts.isEmpty())
        parts << unit(days, tr("day"), tr("days"));
    return parts.join(QStringLiteral(", "));
}

QString DateCalc::relativeText(qint64 days) const {
    if (days == 0)
        return tr("Today");
    if (days == 1)
        return tr("Tomorrow");
    if (days == -1)
        return tr("Yesterday");
    if (days > 0)
        return tr("In %1 days").arg(count(days));
    return tr("%1 days ago").arg(count(-days));
}

QVariantMap DateCalc::describe(const QString &text) const {
    const QDate date = parseDate(text);
    if (!date.isValid())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("pretty"), text.trimmed().isEmpty() ? QString() : tr("Unrecognized date")}};
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("pretty"), prettyDate(date)},
            {QStringLiteral("iso"), date.toString(Qt::ISODate)}};
}

QString DateCalc::todayText() const {
    return today().toString(Qt::ISODate);
}

QString DateCalc::isoText(const QString &text) const {
    const QDate date = parseDate(text);
    return date.isValid() ? date.toString(Qt::ISODate) : text;
}

QVariantMap DateCalc::difference(const QString &fromText, const QString &toText,
                                 bool includeEndDay) const {
    const QDate from = parseDate(fromText);
    const QDate to = parseDate(toText);
    if (!from.isValid() || !to.isValid())
        return failure(tr("Enter two dates"));

    qint64 days = from.daysTo(to);
    qint64 weekdays = weekdaysBetween(from, to);
    // Counting both ends adds the start day itself.
    if (includeEndDay) {
        const QDate before = from.addDays(days < 0 ? 1 : -1);
        days = before.daysTo(to);
        weekdays = weekdaysBetween(before, to);
    }
    const qint64 magnitude = std::llabs(days);

    int years = 0, months = 0, leftover = 0;
    yearsMonthsDays(from, to, &years, &months, &leftover);
    if (includeEndDay)
        ++leftover;

    QVariantList rows;
    rows << row(tr("Calendar"), spanText(years, months, leftover));
    rows << row(tr("Weeks"), magnitude >= 7
                ? unit(magnitude / 7, tr("week"), tr("weeks")) + QStringLiteral(", ") + unit(magnitude % 7, tr("day"), tr("days"))
                : unit(magnitude, tr("day"), tr("days")));
    rows << row(tr("Weekdays"), count(std::llabs(weekdays)));
    rows << row(tr("Weekend days"), count(magnitude - std::llabs(weekdays)));
    rows << row(tr("Months"), count(qint64(years) * 12 + months));
    rows << row(tr("Hours"), count(magnitude * 24));
    rows << row(tr("Minutes"), count(magnitude * 24 * 60));
    rows << row(tr("Seconds"), count(magnitude * 24 * 60 * 60));

    QString headline = unit(magnitude, tr("day"), tr("days"));
    if (days < 0)
        headline += QStringLiteral(" ") + tr("earlier");

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("headline"), headline},
            {QStringLiteral("rows"), rows}};
}

QVariantMap DateCalc::shift(const QString &startText, bool subtract, int years, int months,
                            int weeks, int days, bool businessDays) const {
    const QDate start = parseDate(startText);
    if (!start.isValid())
        return failure(tr("Enter a start date"));

    const int sign = subtract ? -1 : 1;
    QDate result = start.addYears(sign * years).addMonths(sign * months);
    if (businessDays)
        result = addWeekdays(result.addDays(qint64(sign) * weeks * 7), qint64(sign) * days);
    else
        result = result.addDays(qint64(sign) * (qint64(weeks) * 7 + days));
    if (!result.isValid())
        return failure(tr("Out of range"));

    QVariantList rows;
    rows << row(tr("ISO"), result.toString(Qt::ISODate));
    rows << row(tr("US"), result.toString(QStringLiteral("MM/dd/yyyy")));
    rows << row(tr("Days moved"), count(start.daysTo(result)));
    rows << row(tr("From today"), relativeText(today().daysTo(result)));

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("headline"), prettyDate(result)},
            {QStringLiteral("iso"), result.toString(Qt::ISODate)},
            {QStringLiteral("rows"), rows}};
}

QVariantMap DateCalc::info(const QString &text) const {
    const QDate date = parseDate(text);
    if (!date.isValid())
        return failure(tr("Enter a date"));

    int isoYear = 0;
    const int isoWeek = date.weekNumber(&isoYear);
    const QDate now = today();
    const qint64 fromToday = now.daysTo(date);

    QVariantList rows;
    rows << row(tr("Relative"), relativeText(fromToday));
    rows << row(tr("Day of year"), tr("%1 of %2").arg(date.dayOfYear()).arg(date.daysInYear()));
    rows << row(tr("Days left in year"), count(date.daysInYear() - date.dayOfYear()));
    rows << row(tr("ISO week"), isoYear == date.year()
                ? QString::number(isoWeek)
                : QStringLiteral("%1 (%2)").arg(isoWeek).arg(isoYear));
    rows << row(tr("Quarter"), QStringLiteral("Q%1").arg((date.month() - 1) / 3 + 1));
    rows << row(tr("Days in month"), QString::number(date.daysInMonth()));
    rows << row(tr("Leap year"), QDate::isLeapYear(date.year()) ? tr("Yes") : tr("No"));

    if (date < now) {
        int years = 0, months = 0, days = 0;
        yearsMonthsDays(date, now, &years, &months, &days);
        rows << row(tr("Age"), spanText(years, months, days));
    }

    // The next time this month and day comes around (Feb 29 lands on Feb 28).
    if (date != now) {
        int yearsAhead = now.year() - date.year();
        QDate next = date.addYears(yearsAhead);
        if (next < now) {
            ++yearsAhead;
            next = date.addYears(yearsAhead);
        }
        if (yearsAhead > 0)
            rows << row(tr("Next anniversary"),
                        tr("%1 (#%2) in %3 days")
                            .arg(next.toString(Qt::ISODate))
                            .arg(yearsAhead)
                            .arg(count(now.daysTo(next))));
    }

    rows << row(tr("Unix time"), QString::number(date.startOfDay(QTimeZone::UTC).toSecsSinceEpoch()));

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("headline"), prettyDate(date)},
            {QStringLiteral("rows"), rows}};
}
