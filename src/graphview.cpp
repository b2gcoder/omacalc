#include "graphview.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <cmath>

#include "backend.h"

namespace {
QColor mix(const QColor &a, const QColor &b, double amount) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount,
                            a.greenF() + (b.greenF() - a.greenF()) * amount,
                            a.blueF() + (b.blueF() - a.blueF()) * amount);
}
}

GraphView::GraphView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &GraphView::appearanceChanged, this, [this] { update(); });
    connect(this, &GraphView::traceChanged, this, [this] { update(); });
    connect(this, &GraphView::viewChanged, this, [this] { update(); });
    connect(this, &GraphView::equationsChanged, this, [this] { update(); });
}

QString GraphView::stripLeftSide(const QString &equation) {
    static const QRegularExpression left(QStringLiteral("^\\s*(?:y|f\\s*\\(\\s*x\\s*\\))\\s*=\\s*"),
                                         QRegularExpression::CaseInsensitiveOption);
    QString text = equation;
    text.remove(left);
    return text.trimmed();
}

void GraphView::setEquations(const QStringList &equations) {
    if (equations == m_equations)
        return;
    m_equations = equations;
    m_parsed.clear();
    m_errors.clear();
    for (const QString &equation : equations) {
        const QString body = stripLeftSide(equation);
        if (body.isEmpty()) {
            m_parsed.push_back(nullptr);
            m_errors << QString();
            continue;
        }
        auto parsed = std::make_unique<Expression>(Expression::parse(body));
        m_errors << (parsed->isValid() ? QString() : parsed->error());
        m_parsed.push_back(parsed->isValid() ? std::move(parsed) : nullptr);
    }
    emit equationsChanged();
}

void GraphView::setColors(const QVariantList &colors) {
    m_colors = colors;
    emit appearanceChanged();
}

void GraphView::setBackground(const QColor &color) {
    if (color == m_background)
        return;
    m_background = color;
    emit appearanceChanged();
}

void GraphView::setInk(const QColor &color) {
    if (color == m_ink)
        return;
    m_ink = color;
    emit appearanceChanged();
}

void GraphView::setFontFamily(const QString &family) {
    if (family == m_fontFamily)
        return;
    m_fontFamily = family;
    emit appearanceChanged();
}

void GraphView::setFontPixelSize(int size) {
    if (size == m_fontPixelSize || size <= 0)
        return;
    m_fontPixelSize = size;
    emit appearanceChanged();
}

void GraphView::setAngleUnit(int unit) {
    if (unit == m_angleUnit)
        return;
    m_angleUnit = qBound(0, unit, 2);
    emit appearanceChanged();
}

void GraphView::setTracing(bool tracing) {
    if (tracing == m_tracing)
        return;
    m_tracing = tracing;
    emit traceChanged();
}

void GraphView::setTraceX(qreal x) {
    if (qFuzzyCompare(x, m_traceX))
        return;
    m_traceX = x;
    emit traceChanged();
}

QString GraphView::viewText() const {
    const double step = niceStep(80 / m_pixelsPerUnit);
    return tr("x %1 … %2, y %3 … %4")
        .arg(tickLabel(toWorldX(0), step), tickLabel(toWorldX(width()), step),
             tickLabel(toWorldY(height()), step), tickLabel(toWorldY(0), step));
}

double GraphView::toScreenX(double x) const { return width() / 2 + (x - m_centerX) * m_pixelsPerUnit; }
double GraphView::toScreenY(double y) const { return height() / 2 - (y - m_centerY) * m_pixelsPerUnit; }
double GraphView::toWorldX(double px) const { return m_centerX + (px - width() / 2) / m_pixelsPerUnit; }
double GraphView::toWorldY(double py) const { return m_centerY - (py - height() / 2) / m_pixelsPerUnit; }

void GraphView::panBy(qreal dx, qreal dy) {
    m_centerX -= dx / m_pixelsPerUnit;
    m_centerY += dy / m_pixelsPerUnit;
    emit viewChanged();
}

// Keeps the point under the pointer fixed while the scale changes.
void GraphView::zoomAt(qreal factor, qreal px, qreal py) {
    const double before = m_pixelsPerUnit;
    const double after = qBound(1e-6, before * factor, 1e9);
    if (after == before)
        return;
    const double wx = toWorldX(px);
    const double wy = toWorldY(py);
    m_pixelsPerUnit = after;
    m_centerX = wx - (px - width() / 2) / after;
    m_centerY = wy + (py - height() / 2) / after;
    emit viewChanged();
}

void GraphView::zoom(qreal factor) {
    zoomAt(factor, width() / 2, height() / 2);
}

void GraphView::resetView() {
    m_centerX = 0;
    m_centerY = 0;
    // About ±5 across the narrower side.
    m_pixelsPerUnit = std::max(10.0, std::min(width(), height()) / 10.0);
    emit viewChanged();
}

double GraphView::niceStep(double target) {
    if (!(target > 0) || !std::isfinite(target))
        return 1;
    const double power = std::pow(10.0, std::floor(std::log10(target)));
    const double fraction = target / power;
    const double nice = fraction < 1.5 ? 1 : (fraction < 3.5 ? 2 : (fraction < 7.5 ? 5 : 10));
    return nice * power;
}

QString GraphView::tickLabel(double value, double step) const {
    if (std::fabs(value) < step * 1e-6)
        value = 0;
    if (std::fabs(value) >= 1e7 || (step < 1e-5 && value != 0)) {
        QString text = QString::number(value, 'g', 4);
        return text.replace(QLatin1Char('-'), QStringLiteral("−"));
    }
    const int decimals = std::max(0, int(-std::floor(std::log10(step) + 1e-9)));
    return Backend::groupDigits(QString::number(value, 'f', decimals)).replace(QLatin1Char('-'), QStringLiteral("−"));
}

QColor GraphView::colorFor(int index) const {
    if (index < m_colors.size()) {
        const QColor color = m_colors.at(index).value<QColor>();
        if (color.isValid())
            return color;
    }
    static const QList<QColor> fallback = {QColor("#4f8cff"), QColor("#e5534b"), QColor("#3fb950"),
                                           QColor("#d29922"), QColor("#a371f7"), QColor("#39c5cf")};
    return fallback.at(index % fallback.size());
}

void GraphView::paint(QPainter *painter) {
    const double w = width();
    const double h = height();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(QRectF(0, 0, w, h), m_background);

    QFont font(m_fontFamily);
    font.setPixelSize(m_fontPixelSize);
    painter->setFont(font);
    const QFontMetrics metrics(font);

    // Grid: major lines about every 80 px, four minor lines between.
    const double step = niceStep(80 / m_pixelsPerUnit);
    const double minor = step / 5;
    const QColor minorColor = mix(m_background, m_ink, 0.06);
    const QColor majorColor = mix(m_background, m_ink, 0.14);
    const QColor axisColor = mix(m_background, m_ink, 0.55);
    const QColor labelColor = mix(m_background, m_ink, 0.6);

    auto drawLines = [&](double spacing, const QColor &color) {
        painter->setPen(QPen(color, 1));
        for (double x = std::ceil(toWorldX(0) / spacing) * spacing; x <= toWorldX(w); x += spacing) {
            const double px = std::round(toScreenX(x)) + 0.5;
            painter->drawLine(QPointF(px, 0), QPointF(px, h));
        }
        for (double y = std::ceil(toWorldY(h) / spacing) * spacing; y <= toWorldY(0); y += spacing) {
            const double py = std::round(toScreenY(y)) + 0.5;
            painter->drawLine(QPointF(0, py), QPointF(w, py));
        }
    };
    if (minor * m_pixelsPerUnit >= 6)
        drawLines(minor, minorColor);
    drawLines(step, majorColor);

    // Axes, held at the edge when the origin is off screen so labels stay put.
    const double axisX = std::clamp(toScreenX(0), 0.0, w);
    const double axisY = std::clamp(toScreenY(0), 0.0, h);
    painter->setPen(QPen(axisColor, 1.5));
    if (toScreenX(0) >= 0 && toScreenX(0) <= w)
        painter->drawLine(QPointF(axisX, 0), QPointF(axisX, h));
    if (toScreenY(0) >= 0 && toScreenY(0) <= h)
        painter->drawLine(QPointF(0, axisY), QPointF(w, axisY));

    painter->setPen(labelColor);
    const double labelY = std::clamp(axisY + 4, 2.0, h - metrics.height() - 2);
    for (double x = std::ceil(toWorldX(0) / step) * step; x <= toWorldX(w); x += step) {
        if (std::fabs(x) < step * 1e-6)
            continue;
        const QString text = tickLabel(x, step);
        const double px = toScreenX(x) - metrics.horizontalAdvance(text) / 2.0;
        painter->drawText(QPointF(px, labelY + metrics.ascent()), text);
    }
    for (double y = std::ceil(toWorldY(h) / step) * step; y <= toWorldY(0); y += step) {
        if (std::fabs(y) < step * 1e-6)
            continue;
        const QString text = tickLabel(y, step);
        const double textWidth = metrics.horizontalAdvance(text);
        double px = axisX - textWidth - 5;
        if (px < 2)
            px = axisX + 5;
        px = std::clamp(px, 2.0, w - textWidth - 2);
        painter->drawText(QPointF(px, toScreenY(y) + metrics.ascent() / 2.0 - 1), text);
    }

    // Curves: one sample per half pixel, broken where the function is
    // undefined or leaps across the view (an asymptote, as in tan x).
    const auto unit = Expression::AngleUnit(m_angleUnit);
    for (size_t i = 0; i < m_parsed.size(); ++i) {
        const Expression *expression = m_parsed.at(i).get();
        if (!expression)
            continue;
        QPainterPath path;
        bool drawing = false;
        double lastY = 0;
        for (double px = 0; px <= w; px += 0.5) {
            const double y = expression->evaluate(toWorldX(px), unit);
            if (!std::isfinite(y)) {
                drawing = false;
                continue;
            }
            const double py = toScreenY(y);
            if (drawing && std::fabs(py - lastY) > h * 2) {
                drawing = false;
            }
            const double clamped = std::clamp(py, -h * 4, h * 5);
            if (drawing)
                path.lineTo(px, clamped);
            else
                path.moveTo(px, clamped);
            drawing = true;
            lastY = py;
        }
        QPen pen(colorFor(int(i)), 2.5);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    }

    // Trace: a line under the pointer and each curve's value where it crosses.
    if (m_tracing && m_traceX >= 0 && m_traceX <= w) {
        QPen dashed(mix(m_background, m_ink, 0.4), 1, Qt::DashLine);
        painter->setPen(dashed);
        painter->drawLine(QPointF(m_traceX, 0), QPointF(m_traceX, h));
        const double x = toWorldX(m_traceX);
        const double precision = niceStep(1 / m_pixelsPerUnit) / 10;
        double labelTop = 8;
        for (size_t i = 0; i < m_parsed.size(); ++i) {
            const Expression *expression = m_parsed.at(i).get();
            if (!expression)
                continue;
            const double y = expression->evaluate(x, unit);
            if (!std::isfinite(y))
                continue;
            const QColor color = colorFor(int(i));
            const double py = toScreenY(y);
            if (py >= -10 && py <= h + 10) {
                painter->setPen(QPen(m_background, 2));
                painter->setBrush(color);
                painter->drawEllipse(QPointF(m_traceX, py), 5, 5);
            }
            const QString text = QStringLiteral("(%1, %2)").arg(tickLabel(x, precision), tickLabel(y, precision));
            const QRectF box(0, 0, metrics.horizontalAdvance(text) + 14, metrics.height() + 8);
            QRectF placed = box.translated(m_traceX + 10, labelTop);
            if (placed.right() > w - 4)
                placed.moveRight(m_traceX - 10);
            painter->setPen(QPen(color, 1.5));
            painter->setBrush(m_background);
            painter->drawRoundedRect(placed, 4, 4);
            painter->setPen(m_ink);
            painter->drawText(placed, Qt::AlignCenter, text);
            labelTop += box.height() + 6;
        }
    }
}
