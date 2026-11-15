#pragma once

#include <QDate>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// Date arithmetic behind the Dates page: the span between two dates, a date
// moved by years, months, weeks and days, and facts about a single date.
// Every invokable takes the dates as the text the user typed, so QML only
// binds fields to results; parseDate() decides what that text means.
class DateCalc : public QObject {
    Q_OBJECT

public:
    explicit DateCalc(QObject *parent = nullptr);

    // "today" can be pinned so tests (and relative input) are deterministic.
    void setToday(const QDate &today) { m_today = today; }
    QDate today() const { return m_today.isValid() ? m_today : QDate::currentDate(); }

    // Accepts ISO (2026-11-14), US (11/14/2026, 11/14/26, 11-14-2026),
    // month names (Nov 14 2026, November 14, 2026, 14 Nov 2026), compact
    // 20261114, the words today / tomorrow / yesterday, and offsets from
    // today such as +30, -2w, today + 3m, +1y.
    QDate parseDate(const QString &text) const;

    // Calendar span from a to b: whole months that fit, then leftover days.
    // Jan 31 → Mar 1 is 1 month 1 day, because Jan 31 + 1 month is Feb 28.
    static void yearsMonthsDays(QDate a, QDate b, int *years, int *months, int *days);
    // Monday–Friday days in the half-open range (a, b], signed like daysTo.
    static qint64 weekdaysBetween(const QDate &a, const QDate &b);
    static QDate addWeekdays(const QDate &start, qint64 count);

    // {ok, pretty, iso} for the preview under a date field.
    Q_INVOKABLE QVariantMap describe(const QString &text) const;
    Q_INVOKABLE QString todayText() const;
    Q_INVOKABLE QString isoText(const QString &text) const;

    // {ok, headline, rows: [{label, value}]}
    Q_INVOKABLE QVariantMap difference(const QString &from, const QString &to,
                                       bool includeEndDay) const;
    Q_INVOKABLE QVariantMap shift(const QString &start, bool subtract, int years, int months,
                                  int weeks, int days, bool businessDays) const;
    Q_INVOKABLE QVariantMap info(const QString &date) const;

    static QString prettyDate(const QDate &date);
    static QString count(qint64 value);
    static QString unit(qint64 value, const QString &one, const QString &many);

private:
    QString spanText(int years, int months, int days) const;
    QString relativeText(qint64 days) const;
    static QVariantMap row(const QString &label, const QString &value);
    static QVariantMap failure(const QString &message);

    QDate m_today;
};
