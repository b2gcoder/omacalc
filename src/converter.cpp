#include "converter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <limits>

#include "backend.h"

namespace {
constexpr double pi = 3.14159265358979323846;
const QString ratesUrl = QStringLiteral("https://api.frankfurter.dev/v1/latest?base=EUR");
const QString namesUrl = QStringLiteral("https://api.frankfurter.dev/v1/currencies");
// The ECB publishes once a working day; refetching more often gains nothing.
constexpr qint64 refreshAfterSeconds = 6 * 60 * 60;

QString translated(const QString &source) {
    return QCoreApplication::translate("UnitConverter", source.toUtf8().constData());
}
}

UnitConverter::UnitConverter(QObject *parent, const QString &cacheDirectory)
    : QObject(parent),
      m_cacheDirectory(cacheDirectory.isEmpty()
                           ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                           : cacheDirectory) {
    buildCategories();
    loadCache();
}

// Factors are base units per one of the unit, from the exact definitions
// where there is one (the inch is 0.0254 m, the US gallon 231 in³, the
// international foot-pound 1.3558179483314004 J).
void UnitConverter::buildCategories() {
    auto add = [this](const QString &id, const QString &name, const QString &from, const QString &to,
                      const QList<Unit> &units) {
        m_categories << Category{id, name, units, from, to};
    };

    add(QStringLiteral("volume"), QStringLiteral("Volume"), QStringLiteral("ml"), QStringLiteral("cupUs"), {
        {QStringLiteral("ml"), QStringLiteral("Milliliters"), QStringLiteral("mL"), 1e-3},
        {QStringLiteral("cc"), QStringLiteral("Cubic centimeters"), QStringLiteral("cm³"), 1e-3},
        {QStringLiteral("l"), QStringLiteral("Liters"), QStringLiteral("L"), 1},
        {QStringLiteral("m3"), QStringLiteral("Cubic meters"), QStringLiteral("m³"), 1000},
        {QStringLiteral("tspUs"), QStringLiteral("Teaspoons (US)"), QStringLiteral("tsp"), 3.785411784 / 768},
        {QStringLiteral("tbspUs"), QStringLiteral("Tablespoons (US)"), QStringLiteral("tbsp"), 3.785411784 / 256},
        {QStringLiteral("flozUs"), QStringLiteral("Fluid ounces (US)"), QStringLiteral("fl oz"), 3.785411784 / 128},
        {QStringLiteral("cupUs"), QStringLiteral("Cups (US)"), QStringLiteral("cup"), 3.785411784 / 16},
        {QStringLiteral("pintUs"), QStringLiteral("Pints (US)"), QStringLiteral("pt"), 3.785411784 / 8},
        {QStringLiteral("quartUs"), QStringLiteral("Quarts (US)"), QStringLiteral("qt"), 3.785411784 / 4},
        {QStringLiteral("galUs"), QStringLiteral("Gallons (US)"), QStringLiteral("gal"), 3.785411784},
        {QStringLiteral("in3"), QStringLiteral("Cubic inches"), QStringLiteral("in³"), 0.016387064},
        {QStringLiteral("ft3"), QStringLiteral("Cubic feet"), QStringLiteral("ft³"), 28.316846592},
        {QStringLiteral("yd3"), QStringLiteral("Cubic yards"), QStringLiteral("yd³"), 764.554857984},
        {QStringLiteral("tspUk"), QStringLiteral("Teaspoons (UK)"), QStringLiteral("tsp"), 4.54609 / 768},
        {QStringLiteral("tbspUk"), QStringLiteral("Tablespoons (UK)"), QStringLiteral("tbsp"), 4.54609 / 256},
        {QStringLiteral("flozUk"), QStringLiteral("Fluid ounces (UK)"), QStringLiteral("fl oz"), 4.54609 / 160},
        {QStringLiteral("pintUk"), QStringLiteral("Pints (UK)"), QStringLiteral("pt"), 4.54609 / 8},
        {QStringLiteral("quartUk"), QStringLiteral("Quarts (UK)"), QStringLiteral("qt"), 4.54609 / 4},
        {QStringLiteral("galUk"), QStringLiteral("Gallons (UK)"), QStringLiteral("gal"), 4.54609},
    });

    add(QStringLiteral("length"), QStringLiteral("Length"), QStringLiteral("cm"), QStringLiteral("in"), {
        {QStringLiteral("nm"), QStringLiteral("Nanometers"), QStringLiteral("nm"), 1e-9},
        {QStringLiteral("um"), QStringLiteral("Microns"), QStringLiteral("µm"), 1e-6},
        {QStringLiteral("mm"), QStringLiteral("Millimeters"), QStringLiteral("mm"), 1e-3},
        {QStringLiteral("cm"), QStringLiteral("Centimeters"), QStringLiteral("cm"), 1e-2},
        {QStringLiteral("m"), QStringLiteral("Meters"), QStringLiteral("m"), 1},
        {QStringLiteral("km"), QStringLiteral("Kilometers"), QStringLiteral("km"), 1000},
        {QStringLiteral("in"), QStringLiteral("Inches"), QStringLiteral("in"), 0.0254},
        {QStringLiteral("ft"), QStringLiteral("Feet"), QStringLiteral("ft"), 0.3048},
        {QStringLiteral("yd"), QStringLiteral("Yards"), QStringLiteral("yd"), 0.9144},
        {QStringLiteral("mi"), QStringLiteral("Miles"), QStringLiteral("mi"), 1609.344},
        {QStringLiteral("nmi"), QStringLiteral("Nautical miles"), QStringLiteral("NM"), 1852},
        {QStringLiteral("au"), QStringLiteral("Astronomical units"), QStringLiteral("au"), 149597870700},
        {QStringLiteral("ly"), QStringLiteral("Light-years"), QStringLiteral("ly"), 9460730472580800},
    });

    add(QStringLiteral("weight"), QStringLiteral("Weight and mass"), QStringLiteral("kg"), QStringLiteral("lb"), {
        {QStringLiteral("ct"), QStringLiteral("Carats"), QStringLiteral("ct"), 0.0002},
        {QStringLiteral("mg"), QStringLiteral("Milligrams"), QStringLiteral("mg"), 1e-6},
        {QStringLiteral("cg"), QStringLiteral("Centigrams"), QStringLiteral("cg"), 1e-5},
        {QStringLiteral("dg"), QStringLiteral("Decigrams"), QStringLiteral("dg"), 1e-4},
        {QStringLiteral("g"), QStringLiteral("Grams"), QStringLiteral("g"), 1e-3},
        {QStringLiteral("dag"), QStringLiteral("Dekagrams"), QStringLiteral("dag"), 1e-2},
        {QStringLiteral("hg"), QStringLiteral("Hectograms"), QStringLiteral("hg"), 0.1},
        {QStringLiteral("kg"), QStringLiteral("Kilograms"), QStringLiteral("kg"), 1},
        {QStringLiteral("t"), QStringLiteral("Metric tonnes"), QStringLiteral("t"), 1000},
        {QStringLiteral("oz"), QStringLiteral("Ounces"), QStringLiteral("oz"), 0.028349523125},
        {QStringLiteral("lb"), QStringLiteral("Pounds"), QStringLiteral("lb"), 0.45359237},
        {QStringLiteral("st"), QStringLiteral("Stone"), QStringLiteral("st"), 6.35029318},
        {QStringLiteral("tonUs"), QStringLiteral("Short tons (US)"), QStringLiteral("ton"), 907.18474},
        {QStringLiteral("tonUk"), QStringLiteral("Long tons (UK)"), QStringLiteral("ton"), 1016.0469088},
    });

    // Factors unused: temperature converts through kelvin in toBase/fromBase.
    add(QStringLiteral("temperature"), QStringLiteral("Temperature"), QStringLiteral("c"), QStringLiteral("f"), {
        {QStringLiteral("c"), QStringLiteral("Celsius"), QStringLiteral("°C"), 1},
        {QStringLiteral("f"), QStringLiteral("Fahrenheit"), QStringLiteral("°F"), 1},
        {QStringLiteral("k"), QStringLiteral("Kelvin"), QStringLiteral("K"), 1},
        {QStringLiteral("r"), QStringLiteral("Rankine"), QStringLiteral("°R"), 1},
    });

    add(QStringLiteral("energy"), QStringLiteral("Energy"), QStringLiteral("j"), QStringLiteral("kcal"), {
        {QStringLiteral("ev"), QStringLiteral("Electron volts"), QStringLiteral("eV"), 1.602176634e-19},
        {QStringLiteral("j"), QStringLiteral("Joules"), QStringLiteral("J"), 1},
        {QStringLiteral("kj"), QStringLiteral("Kilojoules"), QStringLiteral("kJ"), 1000},
        {QStringLiteral("cal"), QStringLiteral("Thermal calories"), QStringLiteral("cal"), 4.184},
        {QStringLiteral("kcal"), QStringLiteral("Food calories"), QStringLiteral("kcal"), 4184},
        {QStringLiteral("ftlb"), QStringLiteral("Foot-pounds"), QStringLiteral("ft·lbf"), 1.3558179483314004},
        {QStringLiteral("btu"), QStringLiteral("British thermal units"), QStringLiteral("BTU"), 1055.05585262},
        {QStringLiteral("wh"), QStringLiteral("Watt-hours"), QStringLiteral("Wh"), 3600},
        {QStringLiteral("kwh"), QStringLiteral("Kilowatt-hours"), QStringLiteral("kWh"), 3.6e6},
    });

    add(QStringLiteral("area"), QStringLiteral("Area"), QStringLiteral("m2"), QStringLiteral("ft2"), {
        {QStringLiteral("mm2"), QStringLiteral("Square millimeters"), QStringLiteral("mm²"), 1e-6},
        {QStringLiteral("cm2"), QStringLiteral("Square centimeters"), QStringLiteral("cm²"), 1e-4},
        {QStringLiteral("m2"), QStringLiteral("Square meters"), QStringLiteral("m²"), 1},
        {QStringLiteral("ha"), QStringLiteral("Hectares"), QStringLiteral("ha"), 1e4},
        {QStringLiteral("km2"), QStringLiteral("Square kilometers"), QStringLiteral("km²"), 1e6},
        {QStringLiteral("in2"), QStringLiteral("Square inches"), QStringLiteral("in²"), 0.00064516},
        {QStringLiteral("ft2"), QStringLiteral("Square feet"), QStringLiteral("ft²"), 0.09290304},
        {QStringLiteral("yd2"), QStringLiteral("Square yards"), QStringLiteral("yd²"), 0.83612736},
        {QStringLiteral("ac"), QStringLiteral("Acres"), QStringLiteral("ac"), 4046.8564224},
        {QStringLiteral("mi2"), QStringLiteral("Square miles"), QStringLiteral("mi²"), 2589988.110336},
    });

    add(QStringLiteral("speed"), QStringLiteral("Speed"), QStringLiteral("kmh"), QStringLiteral("mph"), {
        {QStringLiteral("cms"), QStringLiteral("Centimeters per second"), QStringLiteral("cm/s"), 0.01},
        {QStringLiteral("ms"), QStringLiteral("Meters per second"), QStringLiteral("m/s"), 1},
        {QStringLiteral("kmh"), QStringLiteral("Kilometers per hour"), QStringLiteral("km/h"), 1 / 3.6},
        {QStringLiteral("fts"), QStringLiteral("Feet per second"), QStringLiteral("ft/s"), 0.3048},
        {QStringLiteral("mph"), QStringLiteral("Miles per hour"), QStringLiteral("mph"), 0.44704},
        {QStringLiteral("kn"), QStringLiteral("Knots"), QStringLiteral("kn"), 1852.0 / 3600},
        // Mach at sea level in the standard atmosphere (15 °C).
        {QStringLiteral("mach"), QStringLiteral("Mach"), QStringLiteral("M"), 340.29},
    });

    add(QStringLiteral("time"), QStringLiteral("Time"), QStringLiteral("h"), QStringLiteral("min"), {
        {QStringLiteral("us"), QStringLiteral("Microseconds"), QStringLiteral("µs"), 1e-6},
        {QStringLiteral("ms"), QStringLiteral("Milliseconds"), QStringLiteral("ms"), 1e-3},
        {QStringLiteral("s"), QStringLiteral("Seconds"), QStringLiteral("s"), 1},
        {QStringLiteral("min"), QStringLiteral("Minutes"), QStringLiteral("min"), 60},
        {QStringLiteral("h"), QStringLiteral("Hours"), QStringLiteral("h"), 3600},
        {QStringLiteral("d"), QStringLiteral("Days"), QStringLiteral("d"), 86400},
        {QStringLiteral("wk"), QStringLiteral("Weeks"), QStringLiteral("wk"), 604800},
        // The Gregorian mean year, 365.2425 days.
        {QStringLiteral("yr"), QStringLiteral("Years"), QStringLiteral("yr"), 31556952},
    });

    add(QStringLiteral("power"), QStringLiteral("Power"), QStringLiteral("kw"), QStringLiteral("hp"), {
        {QStringLiteral("w"), QStringLiteral("Watts"), QStringLiteral("W"), 1},
        {QStringLiteral("kw"), QStringLiteral("Kilowatts"), QStringLiteral("kW"), 1000},
        {QStringLiteral("hp"), QStringLiteral("Horsepower (US)"), QStringLiteral("hp"), 745.69987158227022},
        {QStringLiteral("ps"), QStringLiteral("Metric horsepower"), QStringLiteral("PS"), 735.49875},
        {QStringLiteral("ftlbmin"), QStringLiteral("Foot-pounds per minute"), QStringLiteral("ft·lbf/min"), 1.3558179483314004 / 60},
        {QStringLiteral("btumin"), QStringLiteral("BTUs per minute"), QStringLiteral("BTU/min"), 1055.05585262 / 60},
    });

    QList<Unit> data = {
        {QStringLiteral("bit"), QStringLiteral("Bits"), QStringLiteral("b"), 1},
        {QStringLiteral("nibble"), QStringLiteral("Nibbles"), QStringLiteral("nibble"), 4},
        {QStringLiteral("byte"), QStringLiteral("Bytes"), QStringLiteral("B"), 8},
    };
    const QStringList decimalNames = {QStringLiteral("Kilo"), QStringLiteral("Mega"), QStringLiteral("Giga"),
                                      QStringLiteral("Tera"), QStringLiteral("Peta"), QStringLiteral("Exa")};
    const QStringList binaryNames = {QStringLiteral("Kibi"), QStringLiteral("Mebi"), QStringLiteral("Gibi"),
                                     QStringLiteral("Tebi"), QStringLiteral("Pebi"), QStringLiteral("Exbi")};
    const QString prefixes = QStringLiteral("KMGTPE");
    for (int i = 0; i < decimalNames.size(); ++i) {
        const double decimal = std::pow(1000.0, i + 1);
        const double binary = std::pow(1024.0, i + 1);
        const QString p = prefixes.at(i);
        data << Unit{p.toLower() + QStringLiteral("bit"), decimalNames.at(i) + QStringLiteral("bits"), p + QStringLiteral("b"), decimal};
        data << Unit{p.toLower() + QStringLiteral("ibit"), binaryNames.at(i) + QStringLiteral("bits"), p + QStringLiteral("ib"), binary};
        data << Unit{p.toLower() + QStringLiteral("byte"), decimalNames.at(i) + QStringLiteral("bytes"), p + QStringLiteral("B"), decimal * 8};
        data << Unit{p.toLower() + QStringLiteral("ibyte"), binaryNames.at(i) + QStringLiteral("bytes"), p + QStringLiteral("iB"), binary * 8};
    }
    add(QStringLiteral("data"), QStringLiteral("Data"), QStringLiteral("gbyte"), QStringLiteral("mbyte"), data);

    add(QStringLiteral("pressure"), QStringLiteral("Pressure"), QStringLiteral("atm"), QStringLiteral("bar"), {
        {QStringLiteral("atm"), QStringLiteral("Atmospheres"), QStringLiteral("atm"), 101325},
        {QStringLiteral("bar"), QStringLiteral("Bars"), QStringLiteral("bar"), 1e5},
        {QStringLiteral("kpa"), QStringLiteral("Kilopascals"), QStringLiteral("kPa"), 1000},
        {QStringLiteral("mmhg"), QStringLiteral("Millimeters of mercury"), QStringLiteral("mmHg"), 133.322387415},
        {QStringLiteral("pa"), QStringLiteral("Pascals"), QStringLiteral("Pa"), 1},
        {QStringLiteral("psi"), QStringLiteral("Pounds per square inch"), QStringLiteral("psi"), 6894.757293168361},
    });

    add(QStringLiteral("angle"), QStringLiteral("Angle"), QStringLiteral("deg"), QStringLiteral("rad"), {
        {QStringLiteral("deg"), QStringLiteral("Degrees"), QStringLiteral("°"), 1},
        {QStringLiteral("rad"), QStringLiteral("Radians"), QStringLiteral("rad"), 180 / pi},
        {QStringLiteral("grad"), QStringLiteral("Gradians"), QStringLiteral("grad"), 0.9},
        {QStringLiteral("arcmin"), QStringLiteral("Arcminutes"), QStringLiteral("′"), 1.0 / 60},
        {QStringLiteral("arcsec"), QStringLiteral("Arcseconds"), QStringLiteral("″"), 1.0 / 3600},
        {QStringLiteral("turn"), QStringLiteral("Turns"), QStringLiteral("turn"), 360},
    });

    add(QStringLiteral("currency"), QStringLiteral("Currency"), QStringLiteral("USD"), QStringLiteral("EUR"), {});
}

QVariantList UnitConverter::categories() const {
    QVariantList list;
    for (const Category &c : m_categories)
        list << QVariantMap{{QStringLiteral("id"), c.id}, {QStringLiteral("name"), translated(c.name)}};
    return list;
}

const UnitConverter::Category *UnitConverter::category(const QString &id) const {
    for (const Category &c : m_categories) {
        if (c.id == id)
            return &c;
    }
    return nullptr;
}

const UnitConverter::Unit *UnitConverter::unit(const Category *c, const QString &id) const {
    if (!c)
        return nullptr;
    for (const Unit &u : c->units) {
        if (u.id == id)
            return &u;
    }
    return nullptr;
}

QString UnitConverter::categoryName(const QString &id) const {
    const Category *c = category(id);
    return c ? translated(c->name) : QString();
}

QVariantList UnitConverter::units(const QString &id) const {
    QVariantList list;
    const Category *c = category(id);
    if (!c)
        return list;
    for (const Unit &u : c->units) {
        // Currency names come from the rates service, already in English.
        const QString name = id == QStringLiteral("currency") ? u.name : translated(u.name);
        list << QVariantMap{{QStringLiteral("id"), u.id}, {QStringLiteral("name"), name},
                            {QStringLiteral("symbol"), u.symbol}};
    }
    return list;
}

QStringList UnitConverter::defaultUnits(const QString &id) const {
    const Category *c = category(id);
    return c ? QStringList{c->from, c->to} : QStringList();
}

double UnitConverter::toBase(const QString &cat, const Unit *u, double value) const {
    if (cat == QStringLiteral("temperature")) {
        if (u->id == QStringLiteral("c")) return value + 273.15;
        if (u->id == QStringLiteral("f")) return (value + 459.67) * 5.0 / 9.0;
        if (u->id == QStringLiteral("r")) return value * 5.0 / 9.0;
        return value;
    }
    return value * u->factor;
}

double UnitConverter::fromBase(const QString &cat, const Unit *u, double value) const {
    if (cat == QStringLiteral("temperature")) {
        if (u->id == QStringLiteral("c")) return value - 273.15;
        if (u->id == QStringLiteral("f")) return value * 9.0 / 5.0 - 459.67;
        if (u->id == QStringLiteral("r")) return value * 9.0 / 5.0;
        return value;
    }
    return value / u->factor;
}

double UnitConverter::convert(const QString &cat, const QString &from, const QString &to, double value) const {
    const Category *c = category(cat);
    const Unit *a = unit(c, from);
    const Unit *b = unit(c, to);
    if (!a || !b)
        return std::numeric_limits<double>::quiet_NaN();
    if (a == b)
        return value;
    // Float noise such as 211.99999999999997 for 100 °C in °F rounds away in
    // formatValue's fifteen significant digits.
    return fromBase(cat, b, toBase(cat, a, value));
}

double UnitConverter::parseNumber(const QString &input, bool *ok) {
    QString text = input.trimmed();
    text.replace(QStringLiteral("−"), QStringLiteral("-"));
    static const QRegularExpression grouped(QStringLiteral("^-?\\d{1,3}(,\\d{3})+(\\.\\d*)?$"));
    if (grouped.match(text).hasMatch())
        text.remove(QLatin1Char(','));
    bool parsed = false;
    double value = QLocale::c().toDouble(text, &parsed);
    if (!parsed) {
        text.replace(QLatin1Char(','), QLatin1Char('.'));
        value = QLocale::c().toDouble(text, &parsed);
    }
    if (ok)
        *ok = parsed && std::isfinite(value);
    return value;
}

QString UnitConverter::formatValue(double value) {
    if (!std::isfinite(value))
        return QString();
    QString text = Backend::formatNumber(value);
    text.replace(QLatin1Char('e'), QLatin1Char('E'));
    if (!text.contains(QLatin1Char('E')))
        text = Backend::groupDigits(text);
    return text.replace(QLatin1Char('-'), QStringLiteral("−"));
}

QString UnitConverter::convertText(const QString &cat, const QString &from, const QString &to,
                                   const QString &text) const {
    bool ok = false;
    const double value = parseNumber(text, &ok);
    if (!ok)
        return QString();
    double result = convert(cat, from, to, value);
    if (cat == QStringLiteral("currency") && std::isfinite(result)) {
        // Money reads in cents; below one unit, four significant digits.
        if (std::fabs(result) < 1 && result != 0)
            return QString::number(result, 'g', 4).replace(QLatin1Char('-'), QStringLiteral("−"));
        result = std::round(result * 100) / 100;
        return Backend::groupDigits(QString::number(result, 'f', 2)).replace(QLatin1Char('-'), QStringLiteral("−"));
    }
    return formatValue(result);
}

QVariantList UnitConverter::allConversions(const QString &cat, const QString &from, const QString &text) const {
    QVariantList list;
    const Category *c = category(cat);
    bool ok = false;
    parseNumber(text, &ok);
    if (!c || !ok)
        return list;
    for (const Unit &u : c->units) {
        if (u.id == from)
            continue;
        list << QVariantMap{{QStringLiteral("name"), cat == QStringLiteral("currency") ? u.name : translated(u.name)},
                            {QStringLiteral("symbol"), u.symbol},
                            {QStringLiteral("value"), convertText(cat, from, u.id, text)}};
    }
    return list;
}

QString UnitConverter::rateLine(const QString &from, const QString &to) const {
    if (!m_rates.contains(from) || !m_rates.contains(to))
        return QString();
    const double rate = m_rates.value(to) / m_rates.value(from);
    return QStringLiteral("1 %1 = %2 %3").arg(from, formatValue(std::round(rate * 1e6) / 1e6), to);
}

// ---- Currency ----------------------------------------------------------

void UnitConverter::setRates(const QHash<QString, double> &perEuro, const QHash<QString, QString> &names,
                             const QString &date) {
    m_rates = perEuro;
    m_rates.insert(QStringLiteral("EUR"), 1.0);
    m_currencyNames = names;
    m_ratesDate = date;
    rebuildCurrencyUnits();
    m_currencyStatus = tr("Rates from %1").arg(date);
    emit currencyChanged();
}

void UnitConverter::rebuildCurrencyUnits() {
    for (Category &c : m_categories) {
        if (c.id != QStringLiteral("currency"))
            continue;
        c.units.clear();
        QStringList codes = m_rates.keys();
        codes.sort();
        for (const QString &code : codes)
            c.units << Unit{code, m_currencyNames.value(code, code), code, 1.0 / m_rates.value(code)};
        std::sort(c.units.begin(), c.units.end(), [](const Unit &a, const Unit &b) {
            return QString::localeAwareCompare(a.name, b.name) < 0;
        });
    }
}

void UnitConverter::loadCache() {
    QFile file(m_cacheDirectory + QStringLiteral("/rates.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject rates = root.value(QStringLiteral("rates")).toObject();
    if (rates.isEmpty())
        return;
    QHash<QString, double> perEuro;
    for (auto it = rates.begin(); it != rates.end(); ++it)
        perEuro.insert(it.key(), it.value().toDouble());
    QHash<QString, QString> names;
    const QJsonObject nameObject = root.value(QStringLiteral("names")).toObject();
    for (auto it = nameObject.begin(); it != nameObject.end(); ++it)
        names.insert(it.key(), it.value().toString());
    m_fetchedAt = QDateTime::fromString(root.value(QStringLiteral("fetched")).toString(), Qt::ISODate);
    setRates(perEuro, names, root.value(QStringLiteral("date")).toString());
}

// Written beside the target and renamed over it, so a crash mid-write leaves
// the previous rates rather than half a file.
void UnitConverter::saveCache() const {
    QDir().mkpath(m_cacheDirectory);
    QJsonObject rates, names;
    for (auto it = m_rates.begin(); it != m_rates.end(); ++it)
        rates.insert(it.key(), it.value());
    for (auto it = m_currencyNames.begin(); it != m_currencyNames.end(); ++it)
        names.insert(it.key(), it.value());
    const QJsonObject root{{QStringLiteral("date"), m_ratesDate},
                           {QStringLiteral("fetched"), m_fetchedAt.toString(Qt::ISODate)},
                           {QStringLiteral("rates"), rates},
                           {QStringLiteral("names"), names}};
    QSaveFile file(m_cacheDirectory + QStringLiteral("/rates.json"));
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

void UnitConverter::refreshCurrency(bool force) {
    if (m_loading)
        return;
    if (!force && !m_rates.isEmpty() && m_fetchedAt.isValid()
            && m_fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) < refreshAfterSeconds)
        return;

    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    m_loading = true;
    m_fetchError.clear();
    m_pending = 2;
    m_pendingRates.clear();
    m_pendingNames.clear();
    m_currencyStatus = tr("Updating rates…");
    emit currencyChanged();

    auto fetch = [this](const QString &url, QByteArray *into) {
        QNetworkRequest request{QUrl(url)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("omacalc"));
        request.setTransferTimeout(15000);
        QNetworkReply *reply = m_network->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, into]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
                *into = reply->readAll();
            else if (m_fetchError.isEmpty())
                m_fetchError = reply->errorString();
            if (--m_pending == 0)
                applyRates(m_pendingRates, m_pendingNames, true);
        });
    };
    fetch(ratesUrl, &m_pendingRates);
    fetch(namesUrl, &m_pendingNames);
}

void UnitConverter::applyRates(const QByteArray &ratesJson, const QByteArray &namesJson, bool fromNetwork) {
    m_loading = false;
    const QJsonObject root = QJsonDocument::fromJson(ratesJson).object();
    const QJsonObject rates = root.value(QStringLiteral("rates")).toObject();
    if (rates.isEmpty()) {
        const QString reason = m_fetchError.isEmpty() ? tr("the service sent no rates") : m_fetchError;
        m_currencyStatus = m_rates.isEmpty()
            ? tr("Couldn't load exchange rates: %1").arg(reason)
            : tr("Couldn't update rates (%1). Showing rates from %2.").arg(reason, m_ratesDate);
        emit currencyChanged();
        return;
    }

    QHash<QString, double> perEuro;
    for (auto it = rates.begin(); it != rates.end(); ++it)
        perEuro.insert(it.key(), it.value().toDouble());
    QHash<QString, QString> names = m_currencyNames;
    const QJsonObject nameObject = QJsonDocument::fromJson(namesJson).object();
    for (auto it = nameObject.begin(); it != nameObject.end(); ++it)
        names.insert(it.key(), it.value().toString());
    if (fromNetwork)
        m_fetchedAt = QDateTime::currentDateTimeUtc();
    setRates(perEuro, names, root.value(QStringLiteral("date")).toString());
    saveCache();
}
