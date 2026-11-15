#pragma once

#include <QList>
#include <QObject>
#include <QVariantList>

// History and memory, shared by the Standard and Scientific calculators the
// way a desk calculator's tape and memory register outlive a mode switch.
// Both live only as long as the window: nothing is written to disk.
class Tape : public QObject {
    Q_OBJECT
    // [{expression, result, value}], newest first.
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    // [{text, value}], newest first; the first entry is what MR recalls.
    Q_PROPERTY(QVariantList memory READ memory NOTIFY memoryChanged)
    Q_PROPERTY(bool hasMemory READ hasMemory NOTIFY memoryChanged)

public:
    explicit Tape(QObject *parent = nullptr);

    QVariantList history() const;
    QVariantList memory() const;
    bool hasMemory() const { return !m_memory.isEmpty(); }
    QList<double> memoryValues() const { return m_memory; }

    Q_INVOKABLE void addHistory(const QString &expression, const QString &result, double value);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void removeHistory(int index);

    // MS pushes a new slot; M+ and M− change the newest slot, or start one
    // when memory is empty; MR reads the newest.
    Q_INVOKABLE void memoryStore(double value);
    Q_INVOKABLE void memoryAdd(double value);
    Q_INVOKABLE void memorySubtract(double value);
    Q_INVOKABLE double memoryRecall() const;
    Q_INVOKABLE void memoryClear();
    // The same, for one slot in the memory list.
    Q_INVOKABLE void memoryItemAdd(int index, double value);
    Q_INVOKABLE void memoryItemSubtract(int index, double value);
    Q_INVOKABLE void memoryItemClear(int index);

signals:
    void historyChanged();
    void memoryChanged();

private:
    struct Entry {
        QString expression;
        QString result;
        double value;
    };
    QList<Entry> m_history;
    QList<double> m_memory;
};
