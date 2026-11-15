#include "tape.h"

#include <QVariantMap>
#include <cmath>

#include "backend.h"

namespace {
// Long sessions shouldn't grow without bound; a hundred lines is plenty to
// scroll back through.
constexpr int historyLimit = 100;
}

Tape::Tape(QObject *parent) : QObject(parent) {}

QVariantList Tape::history() const {
    QVariantList list;
    for (const Entry &entry : m_history)
        list << QVariantMap{{QStringLiteral("expression"), entry.expression},
                            {QStringLiteral("result"), entry.result},
                            {QStringLiteral("value"), entry.value}};
    return list;
}

QVariantList Tape::memory() const {
    QVariantList list;
    for (double value : m_memory)
        list << QVariantMap{{QStringLiteral("text"), Backend::groupDigits(Backend::formatNumber(value))},
                            {QStringLiteral("value"), value}};
    return list;
}

void Tape::addHistory(const QString &expression, const QString &result, double value) {
    m_history.prepend({expression, result, value});
    while (m_history.size() > historyLimit)
        m_history.removeLast();
    emit historyChanged();
}

void Tape::clearHistory() {
    if (m_history.isEmpty())
        return;
    m_history.clear();
    emit historyChanged();
}

void Tape::removeHistory(int index) {
    if (index < 0 || index >= m_history.size())
        return;
    m_history.removeAt(index);
    emit historyChanged();
}

void Tape::memoryStore(double value) {
    if (!std::isfinite(value))
        return;
    m_memory.prepend(value);
    emit memoryChanged();
}

void Tape::memoryAdd(double value) {
    if (m_memory.isEmpty())
        memoryStore(value);
    else
        memoryItemAdd(0, value);
}

void Tape::memorySubtract(double value) {
    if (m_memory.isEmpty())
        memoryStore(-value);
    else
        memoryItemSubtract(0, value);
}

double Tape::memoryRecall() const {
    return m_memory.isEmpty() ? 0 : m_memory.first();
}

void Tape::memoryClear() {
    if (m_memory.isEmpty())
        return;
    m_memory.clear();
    emit memoryChanged();
}

void Tape::memoryItemAdd(int index, double value) {
    if (index < 0 || index >= m_memory.size() || !std::isfinite(value + m_memory.at(index)))
        return;
    m_memory[index] += value;
    emit memoryChanged();
}

void Tape::memoryItemSubtract(int index, double value) {
    memoryItemAdd(index, -value);
}

void Tape::memoryItemClear(int index) {
    if (index < 0 || index >= m_memory.size())
        return;
    m_memory.removeAt(index);
    emit memoryChanged();
}
