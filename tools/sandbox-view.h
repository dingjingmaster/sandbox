//
// Created by dingjing on 10/29/24.
//

#ifndef sandbox_SANDBOX_VIEW_H
#define sandbox_SANDBOX_VIEW_H
#include <QTreeView>
#include <QStyledItemDelegate>

class QTimer;
class QPainter;
class SandboxItem;
class SandboxView : public QTreeView
{
    Q_OBJECT
public:
    explicit SandboxView(bool inSandbox = false, QWidget *parent = nullptr);

    QString getSelectedDir() const;
    QString getSelectedUri();
    SandboxItem* getSelectedItem() const;

Q_SIGNALS:
    void selectDir (const QString& uri);
    void selectedUriChanged (const QString& uri);

    void copyFrom (const QString& uri);

protected:
    void currentChanged(const QModelIndex & current, const QModelIndex & previous) override;

private:
    SandboxItem*                        mCurrentItem;
    QModelIndex                         mCurrentSelectedIdx;
    QString                             mCurrentSelectedUri;
};

#if 1
class SandboxViewDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SandboxViewDelegate (QObject* parent = nullptr);

    void paint (QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    int                         mAngle;
    QObject*                    mObj;
    QTimer*                     mTimer;
};
#endif

#endif // sandbox_SANDBOX_VIEW_H
