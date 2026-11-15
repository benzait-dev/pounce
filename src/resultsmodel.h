#pragma once

#include <QAbstractListModel>

#include "element.h"
#include "ranker.h"

// The ranked elements as QML sees them: one row per match, best first, with
// its label, where it is on screen, and how the typed text matched it.
class ResultsModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum RoleNames {
        LabelRole = Qt::UserRole + 1,
        LabelHtmlRole,
        RoleNameRole,
        GlyphRole,
        AppRole,
        HintRole,
        XRole,
        YRole,
        WidthRole,
        HeightRole,
        ActiveWindowRole,
        CheckedRole,
        EditableRole,
    };

    explicit ResultsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void set(const Elements &elements, const QList<Ranker::Ranked> &ranked);
    void setAccent(const QString &accent);

    const Element *element(int row) const;
    const Ranker::Ranked *ranked(int row) const;
    const QList<Ranker::Ranked> &all() const { return m_ranked; }

    // The label with the matched characters picked out, as Qt StyledText.
    static QString highlight(const QString &text, const QList<int> &positions, const QString &color);

private:
    Elements m_elements;
    QList<Ranker::Ranked> m_ranked;
    QString m_accent = "#FFD60A";
};
