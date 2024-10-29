//
// Created by dingjing on 23-5-25.
//

#ifndef VFS_MANAGER_H
#define VFS_MANAGER_H
#include <QSet>
#include <QObject>
#include <gio/gio.h>

class VFSManager : public QObject
{
    Q_OBJECT
public:
    VFSManager()=delete;
    VFSManager(VFSManager&)=delete;
    static VFSManager* getInstance ();

    bool registerUriSchema (const QString& schema, GVfsFileLookupFunc lookupCB, GVfsFileLookupFunc parseNameCB);

private:
    explicit VFSManager(QObject* parent = nullptr);

private:
    QSet<QString>           mSchemaList;
    static VFSManager*      gInstance;
};


#endif //VFS_MANAGER_H
