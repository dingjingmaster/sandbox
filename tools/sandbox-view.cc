//
// Created by dingjing on 10/29/24.
//

#include "sandbox-view.h"

#include <QIcon>
#include <QMenu>
#include <QDebug>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QHeaderView>

#include "sandbox-model.h"

SandboxView::SandboxView(bool inSandbox, QWidget * parent)
    : QTreeView(parent), mTimer(new QTimer(this))
{
    header()->setVisible (true);
    header()->setStretchLastSection (true);

    setContentsMargins(0, 0, 0, 0);
    setColumnWidth(2, 30);
    setContextMenuPolicy (Qt::CustomContextMenu);
    setSelectionMode(QAbstractItemView::NoSelection);

    connect(mTimer, &QTimer::timeout, this, [=] () {
        update();
    });

    connect(this, &SandboxView::customContextMenuRequested, this, [=] (const QPoint &pos) ->void {
        QModelIndex idx = indexAt(pos);
        if (!idx.isValid() || idx.row() < 0 || idx.row() < 0) {
            return;
        }

        auto item = static_cast<SandboxItem*> (idx.internalPointer());
        g_return_if_fail (item != nullptr && !item->getUri().isNull() && !item->getUri().isEmpty());

        QMenu menu(this);

        auto actionTip = (inSandbox ? "复制到宿主机" : "复制收到沙盒");
        connect(menu.addAction(actionTip), &QAction::triggered, this, [&] (bool) -> void {
            Q_EMIT copyFrom(getSelectedUri());
        });

        menu.exec(QCursor::pos());
    });
}

QString SandboxView::getSelectedDir() const
{
    QString dir;
    GFile* file = g_file_new_for_uri(mCurrentSelectedUri.toUtf8().constData());
    if (G_IS_FILE (file)) {
        if (G_FILE_TYPE_DIRECTORY == g_file_query_file_type(file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, nullptr)) {
            char* path = g_file_get_uri(file);
            if (path) {
                dir = path;
                g_free(path);
            }
        }
        else {
            GFile* parentFile = g_file_get_parent(file);
            if (G_IS_FILE (parentFile)) {
                char* path = g_file_get_uri(parentFile);
                if (path) {
                    dir = path;
                    g_free(path);
                }
                g_object_unref(parentFile);
            }
        }
        g_object_unref(file);
    }

    return dir;
}

QString SandboxView::getSelectedUri()
{
    return mCurrentSelectedUri;
}

void SandboxView::currentChanged(const QModelIndex & current, const QModelIndex & previous)
{
    auto item = static_cast<SandboxItem*> (current.internalPointer());

    g_return_if_fail (item != nullptr && !item->getUri().isNull() && !item->getUri().isEmpty());

    mCurrentSelectedUri = item->getUri();

    Q_EMIT selectedUriChanged(mCurrentSelectedUri);

    QTreeView::currentChanged(current, previous);

    for (int i = 0; i < model()->columnCount(); i++) {
        QModelIndex idx = model()->index(current.row(), i, current.parent());
        update(idx);
    }
}

SandboxViewDelegate::SandboxViewDelegate(QObject * parent)
    : QStyledItemDelegate (parent), mObj {parent}
{
    mTimer = new QTimer(this);
    mTimer->setInterval (100);
    connect (mTimer, &QTimer::timeout, this, [=] () {
        mAngle = (mAngle + 20) % 360;
    });
    mTimer->start();
}

void SandboxViewDelegate::paint(QPainter * p, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    Q_UNUSED(index);

    g_return_if_fail(index.isValid());

    p->save();

    QRect rect = option.rect;
    QPalette pal;

    // 绘制背景
    QColor color(255, 138, 140);
    auto item = static_cast<SandboxItem*>(index.internalPointer());

    // qInfo() << index << "--" << item->getUri();

    switch (index.column()) {
        case 1: {
            // switch (item->status()) {
            //     case SandboxItem::SI_STATUS_PREPARE: {
            //         printf("prepare\n");
            //         p->save();
            //         int width = qMin(rect.width(), rect.height() - 3);
            //         p->setRenderHint(QPainter::Antialiasing);
            //
            //         int outerRadius = (int) ((width - 1) * 0.5);
            //         int innerRadius = (int) ((width - 1) * 0.5 * 0.38);
            //
            //         int capsuleHeight = outerRadius - innerRadius;
            //         int capsuleWidth  = (width > 32 ) ? (int) (capsuleHeight * .23) : (int) (capsuleHeight * .35);
            //         int capsuleRadius = capsuleWidth / 2;
            //
            //         for (int i = 0; i < 12; ++i) {
            //             QColor color = Qt::black; //(255, 255, 225);
            //             color.setAlphaF(1.0f - ((float)i / 12.0f));
            //             p->setPen(Qt::NoPen);
            //             p->setBrush(color);
            //             p->save();
            //             p->translate(rect.center());
            //             p->rotate((float) mAngle - (float) i * 30.0f);
            //             p->drawRoundedRect((int) (-capsuleWidth * 0.5), -(innerRadius+capsuleHeight), capsuleWidth, capsuleHeight, capsuleRadius, capsuleRadius);
            //             p->restore();
            //         }
            //         p->restore();
            //         break;
            //     }
            //     case SandboxItem::SI_STATUS_FINISHED: {
            //         p->drawPixmap(rect, QIcon(":/icons/OK.png").pixmap(20, 20));
            //         break;
            //     }
            //     case SandboxItem::SI_STATUS_PROCESSING: {
            //         p->drawPixmap(rect, QIcon(":/icons/OK.png").pixmap(20, 20));
            //         break;
            //     }
            //     default: {
            //         QStyledItemDelegate::paint (p, option, index);
            //         break;
            //     }
            // }
            break;
        }
        default: {
            QStyledItemDelegate::paint (p, option, index);
            break;
        }
    }

    p->restore();
}

