#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QStringList>
#include <QVariantList>
#include <memory>
#include <vector>

#include "expression.h"

// The Graphing page's plot: equations in x drawn over a grid that pans with
// a drag and zooms with the wheel around the pointer. Each equation is parsed
// once when it changes and evaluated once per pixel column when painting.
class GraphView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QStringList equations READ equations WRITE setEquations NOTIFY equationsChanged)
    Q_PROPERTY(QVariantList colors READ colors WRITE setColors NOTIFY appearanceChanged)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY appearanceChanged)
    Q_PROPERTY(QColor ink READ ink WRITE setInk NOTIFY appearanceChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY appearanceChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY appearanceChanged)
    Q_PROPERTY(int angleUnit READ angleUnit WRITE setAngleUnit NOTIFY appearanceChanged)
    Q_PROPERTY(bool tracing READ tracing WRITE setTracing NOTIFY traceChanged)
    Q_PROPERTY(qreal traceX READ traceX WRITE setTraceX NOTIFY traceChanged)
    // Errors per equation, "" when it parsed (or is blank).
    Q_PROPERTY(QStringList errors READ errors NOTIFY equationsChanged)
    Q_PROPERTY(QString viewText READ viewText NOTIFY viewChanged)

public:
    explicit GraphView(QQuickItem *parent = nullptr);

    QStringList equations() const { return m_equations; }
    void setEquations(const QStringList &equations);
    QVariantList colors() const { return m_colors; }
    void setColors(const QVariantList &colors);
    QColor background() const { return m_background; }
    void setBackground(const QColor &color);
    QColor ink() const { return m_ink; }
    void setInk(const QColor &color);
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &family);
    int fontPixelSize() const { return m_fontPixelSize; }
    void setFontPixelSize(int size);
    int angleUnit() const { return m_angleUnit; }
    void setAngleUnit(int unit);
    bool tracing() const { return m_tracing; }
    void setTracing(bool tracing);
    qreal traceX() const { return m_traceX; }
    void setTraceX(qreal x);
    QStringList errors() const { return m_errors; }
    QString viewText() const;

    Q_INVOKABLE void panBy(qreal dx, qreal dy);
    Q_INVOKABLE void zoomAt(qreal factor, qreal px, qreal py);
    Q_INVOKABLE void zoom(qreal factor);
    Q_INVOKABLE void resetView();

    void paint(QPainter *painter) override;

    // "y = 2x", "f(x) = 2x" and "2x" all mean the same thing.
    static QString stripLeftSide(const QString &equation);
    // Grid spacing near `target` units: 1, 2 or 5 times a power of ten.
    static double niceStep(double target);

signals:
    void equationsChanged();
    void appearanceChanged();
    void traceChanged();
    void viewChanged();

private:
    double toScreenX(double x) const;
    double toScreenY(double y) const;
    double toWorldX(double px) const;
    double toWorldY(double py) const;
    QString tickLabel(double value, double step) const;
    QColor colorFor(int index) const;

    QStringList m_equations;
    std::vector<std::unique_ptr<Expression>> m_parsed;
    QStringList m_errors;
    QVariantList m_colors;
    QColor m_background = QColor(QStringLiteral("#101010"));
    QColor m_ink = QColor(QStringLiteral("#eeeeee"));
    QString m_fontFamily;
    int m_fontPixelSize = 12;
    int m_angleUnit = 1;
    bool m_tracing = false;
    qreal m_traceX = 0;
    double m_centerX = 0;
    double m_centerY = 0;
    double m_pixelsPerUnit = 40;
};
