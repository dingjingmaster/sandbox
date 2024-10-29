//
// Created by dingjing on 10/29/24.
//

#ifndef sandbox_SANDBOX_VIEW_H
#define sandbox_SANDBOX_VIEW_H
#include <QTreeView>


class SandboxView : public QTreeView
{
    Q_OBJECT
public:
    explicit SandboxView(QWidget *parent = nullptr);

Q_SIGNALS:
    void selectDir(const QString& uri);
};



#endif // sandbox_SANDBOX_VIEW_H
