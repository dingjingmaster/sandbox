//
// Created by dingjing on 6/10/24.
//

#include "fs.h"

typedef struct _FSVolume        FSVolume;
typedef struct _MkfsSession     MkfsSession;

struct _MkfsSession
{
    CRand*                  rand;
};


static MkfsSession* mkfs_init();
static void mkfs_destroy(MkfsSession* session);


int fs_mkfs(MkfsOptions* opts)
{
    MkfsSession* session = mkfs_init();
    if (!session) {
        mkfs_destroy(session);
        C_LOG_ERROR("mkfs_init failed!");
        return -1;
    }

}






static MkfsSession* mkfs_init()
{
    MkfsSession* session = (MkfsSession*) c_malloc0 (sizeof(MkfsSession));

    session->rand = c_rand_new();

    return session;
}

static void mkfs_destroy(MkfsSession* session)
{
    c_return_if_fail(session);

    c_rand_free(session->rand);

    c_free(session);
}
