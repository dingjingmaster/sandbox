//
// Created by dingjing on 23-5-25.
//

#include "vfs-manager.h"

#include <glib.h>
#include <gio/gio.h>

#include "sandbox-vfs-file.h"

VFSManager* VFSManager::gInstance = nullptr;


static GFile* sandbox_vfs_parse_name (GVfs* vfs, const char* parseName, gpointer udata)
{
    (void) vfs;
    (void) udata;

    return sandbox_vfs_file_new_for_uri(parseName);
}

static GFile * sandbox_vfs_lookup (GVfs* vfs, const char* uri, gpointer udata)
{
    return sandbox_vfs_parse_name(vfs, uri, udata);
}


VFSManager::VFSManager(QObject *parent)
    : QObject (parent)
{
    auto vfs = g_vfs_get_default();

    auto schemas = g_vfs_get_supported_uri_schemes (vfs);
    for (auto s = 0; nullptr != schemas[s]; ++s) {
        mSchemaList << schemas[s];
    }

    // register
    registerUriSchema ("dsm", sandbox_vfs_lookup, sandbox_vfs_parse_name);
}

VFSManager *VFSManager::getInstance()
{
    static gsize init = 0;

    if (g_once_init_enter(&init)) {
        if (!gInstance) {
            gInstance = new VFSManager(nullptr);
        }
        g_once_init_leave(&init, 1);
    }

    return gInstance;
}

bool VFSManager::registerUriSchema(const QString &schema, GVfsFileLookupFunc lookupCB, GVfsFileLookupFunc parseNameCB)
{
    if (!mSchemaList.contains (schema)) {
        auto vfs = g_vfs_get_default();
        return g_vfs_register_uri_scheme (vfs, schema.toUtf8().constData(), lookupCB, nullptr, nullptr, parseNameCB, nullptr, nullptr);
    }

    return false;
}
