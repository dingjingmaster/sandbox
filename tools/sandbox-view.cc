//
// Created by dingjing on 10/29/24.
//

#include "sandbox-view.h"

#include <QHeaderView>

SandboxView::SandboxView(QWidget * parent)
    : QTreeView(parent)
{
    header()->setVisible (false);
    setContentsMargins(0, 0, 0, 0);
    setContextMenuPolicy (Qt::CustomContextMenu);
}
