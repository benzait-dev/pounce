#include "resultsmodel.h"

#include <QSet>

#include "roles.h"

ResultsModel::ResultsModel(QObject *parent) : QAbstractListModel(parent) {}

int ResultsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_ranked.size());
}

QVariant ResultsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_ranked.size())
        return {};
    const Ranker::Ranked &ranked = m_ranked[index.row()];
    const Element &element = m_elements[ranked.index];
    switch (role) {
    case LabelRole: return element.label().isEmpty() ? element.roleName : element.label();
    case LabelHtmlRole:
        return element.name.isEmpty()
                   ? highlight(element.label().isEmpty() ? element.roleName : element.label(), {}, m_accent)
                   : highlight(element.name, ranked.positions, m_accent);
    case RoleNameRole: return element.roleName;
    case GlyphRole: return Roles::glyph(element.role);
    case AppRole: return element.app;
    case HintRole: return ranked.hint;
    case XRole: return element.rect.x();
    case YRole: return element.rect.y();
    case WidthRole: return element.rect.width();
    case HeightRole: return element.rect.height();
    case ActiveWindowRole: return element.inActiveWindow;
    case CheckedRole: return element.checked;
    case EditableRole: return element.editable;
    default: return {};
    }
}

QHash<int, QByteArray> ResultsModel::roleNames() const {
    return {{LabelRole, "label"},       {LabelHtmlRole, "labelHtml"},   {RoleNameRole, "roleName"},
            {GlyphRole, "glyph"},       {AppRole, "app"},               {HintRole, "hint"},
            {XRole, "elementX"},        {YRole, "elementY"},            {WidthRole, "elementWidth"},
            {HeightRole, "elementHeight"}, {ActiveWindowRole, "inActiveWindow"}, {CheckedRole, "checked"},
            {EditableRole, "editable"}};
}

void ResultsModel::set(const Elements &elements, const QList<Ranker::Ranked> &ranked) {
    beginResetModel();
    m_elements = elements;
    m_ranked = ranked;
    endResetModel();
}

void ResultsModel::setAccent(const QString &accent) {
    if (accent == m_accent)
        return;
    m_accent = accent;
    if (!m_ranked.isEmpty())
        emit dataChanged(index(0), index(int(m_ranked.size()) - 1), {LabelHtmlRole});
}

const Element *ResultsModel::element(int row) const {
    if (row < 0 || row >= m_ranked.size())
        return nullptr;
    return &m_elements[m_ranked[row].index];
}

const Ranker::Ranked *ResultsModel::ranked(int row) const {
    if (row < 0 || row >= m_ranked.size())
        return nullptr;
    return &m_ranked[row];
}

QString ResultsModel::highlight(const QString &text, const QList<int> &positions, const QString &color) {
    const QSet<int> marked(positions.cbegin(), positions.cend());
    QString html;
    bool open = false;
    for (int i = 0; i < text.size(); ++i) {
        const bool hit = marked.contains(i);
        if (hit && !open) {
            html += QStringLiteral("<font color=\"%1\"><b>").arg(color);
            open = true;
        } else if (!hit && open) {
            html += QStringLiteral("</b></font>");
            open = false;
        }
        html += QString(text.at(i)).toHtmlEscaped();
    }
    if (open)
        html += QStringLiteral("</b></font>");
    return html;
}
