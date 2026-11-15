#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

// Unit conversion for the converter pages: volume, length, weight and mass,
// temperature, energy, area, speed, time, power, data, pressure, angle, and
// currency. Units are factors to one base unit per category, except
// temperature, which is affine. Currency rates are the European Central
// Bank's daily reference rates, fetched from frankfurter.dev and cached.
class UnitConverter : public QObject {
    Q_OBJECT
    // [{id, name}] in menu order.
    Q_PROPERTY(QVariantList categories READ categories CONSTANT)
    Q_PROPERTY(QString currencyStatus READ currencyStatus NOTIFY currencyChanged)
    Q_PROPERTY(bool currencyLoading READ currencyLoading NOTIFY currencyChanged)
    Q_PROPERTY(bool currencyReady READ currencyReady NOTIFY currencyChanged)

public:
    explicit UnitConverter(QObject *parent = nullptr, const QString &cacheDirectory = {});

    QVariantList categories() const;
    // [{id, name, symbol}] for one category.
    Q_INVOKABLE QVariantList units(const QString &category) const;
    Q_INVOKABLE QString categoryName(const QString &category) const;
    // The default pair a category opens with, as [from, to].
    Q_INVOKABLE QStringList defaultUnits(const QString &category) const;

    // NaN when either unit is unknown or the rates aren't loaded.
    Q_INVOKABLE double convert(const QString &category, const QString &from, const QString &to,
                               double value) const;
    // The same, taking and giving the text the fields show.
    Q_INVOKABLE QString convertText(const QString &category, const QString &from, const QString &to,
                                    const QString &text) const;
    // [{name, value}] of the value in every other unit, for the list below.
    Q_INVOKABLE QVariantList allConversions(const QString &category, const QString &from,
                                            const QString &text) const;
    Q_INVOKABLE QString rateLine(const QString &from, const QString &to) const;

    static double parseNumber(const QString &text, bool *ok);
    static QString formatValue(double value);

    QString currencyStatus() const { return m_currencyStatus; }
    bool currencyLoading() const { return m_loading; }
    bool currencyReady() const { return !m_rates.isEmpty(); }

    // Fetches when the cached rates are older than a few hours; the page calls
    // it when Currency opens, so nothing goes over the network otherwise.
    Q_INVOKABLE void refreshCurrency(bool force = false);
    // For tests: rates per one euro.
    void setRates(const QHash<QString, double> &perEuro, const QHash<QString, QString> &names,
                  const QString &date);

signals:
    void currencyChanged();

private:
    struct Unit {
        QString id;
        QString name;
        QString symbol;
        double factor;  // base units per one of this unit
    };
    struct Category {
        QString id;
        QString name;
        QList<Unit> units;
        QString from, to;
    };

    void buildCategories();
    const Category *category(const QString &id) const;
    const Unit *unit(const Category *category, const QString &id) const;
    double toBase(const QString &category, const Unit *unit, double value) const;
    double fromBase(const QString &category, const Unit *unit, double value) const;
    void loadCache();
    void saveCache() const;
    void applyRates(const QByteArray &ratesJson, const QByteArray &namesJson, bool fromNetwork);
    void rebuildCurrencyUnits();

    QList<Category> m_categories;
    QString m_cacheDirectory;
    QNetworkAccessManager *m_network = nullptr;
    QHash<QString, double> m_rates;  // per one euro
    QHash<QString, QString> m_currencyNames;
    QString m_ratesDate;
    QDateTime m_fetchedAt;
    QString m_currencyStatus;
    QString m_fetchError;
    bool m_loading = false;
    int m_pending = 0;
    QByteArray m_pendingRates, m_pendingNames;
};
