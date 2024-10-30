//
// Created by dingjing on 10/30/24.
//

#ifndef sandbox_HEADER_VIEW_H
#define sandbox_HEADER_VIEW_H
#include <QHeaderView>

class HeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit HeaderView(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent=nullptr);

    void paintSection (QPainter* p, const QRect& rect, int logicalIndex) const override;

private:
};

#endif // sandbox_HEADER_VIEW_H
