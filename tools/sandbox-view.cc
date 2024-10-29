//
// Created by dingjing on 10/29/24.
//

#include "sandbox-view.h"

#include <QHeaderView>

#include <QMenu>

#include "sandbox-model.h"

SandboxView::SandboxView(QWidget * parent)
    : QTreeView(parent)
{
    header()->setVisible (false);
    setContentsMargins(0, 0, 0, 0);
    setContextMenuPolicy (Qt::CustomContextMenu);

    connect(this, &SandboxView::customContextMenuRequested, [&] (const QPoint &pos) ->void {
        QModelIndex idx = indexAt(pos);
        if (!idx.isValid() || idx.row() < 0 || idx.row() < 0) {
            return;
        }

        auto item = static_cast<SandboxItem*> (idx.internalPointer());
        g_return_if_fail (item != NULL && !item->getUri().isNull() && !item->getUri().isEmpty());

        QMenu menu(this);
        connect(menu.addAction("同步"), &QAction::triggered, [&] () -> void {

        });
        menu.exec(QCursor::pos());
    });
}
