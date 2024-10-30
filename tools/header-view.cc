//
// Created by dingjing on 10/30/24.
//

#include "header-view.h"

#include <QRect>
#include <QPainter>
#include <QApplication>


HeaderView::HeaderView(Qt::Orientation orientation, QWidget * parent)
    : QHeaderView(orientation, parent)
{
    setContentsMargins(0, 0, 0, 0);
}

void HeaderView::paintSection(QPainter * p, const QRect & rect, int logicalIndex) const
{
    p->save();

    if (0 == logicalIndex) {
        QApplication::style()->drawItemText(p, rect, Qt::AlignCenter, QPalette(), true, "文件");
    }
    else if (1 == logicalIndex) {
        QApplication::style()->drawItemText(p, rect, Qt::AlignCenter, QPalette(), true, "状态");
    }

    p->restore();
}
