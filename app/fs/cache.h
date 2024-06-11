//
// Created by dingjing on 6/11/24.
//

#ifndef sandbox_CACHE_H
#define sandbox_CACHE_H

#include <c/clib.h>

#include "types.h"
#include "layout.h"

C_BEGIN_EXTERN_C

struct CACHED_GENERIC
{
        struct CACHED_GENERIC *next;
        struct CACHED_GENERIC *previous;
        void *variable;
        size_t varsize;
        union ALIGNMENT payload[0];
} ;

struct CACHED_INODE
{
        struct CACHED_INODE *next;
        struct CACHED_INODE *previous;
        const char *pathname;
        size_t varsize;
        union ALIGNMENT payload[0];
                /* above fields must match "struct CACHED_GENERIC" */
        u64 inum;
} ;

struct CACHED_NIDATA
{
        struct CACHED_NIDATA *next;
        struct CACHED_NIDATA *previous;
        const char *pathname;   /* not used */
        size_t varsize;         /* not used */
        union ALIGNMENT payload[0];
                /* above fields must match "struct CACHED_GENERIC" */
        u64 inum;
        FSInode* ni;
} ;

struct CACHED_LOOKUP
{
        struct CACHED_LOOKUP *next;
        struct CACHED_LOOKUP *previous;
        const char *name;
        size_t namesize;
        union ALIGNMENT payload[0];
                /* above fields must match "struct CACHED_GENERIC" */
        u64 parent;
        u64 inum;
} ;

enum
{
        CACHE_FREE = 1,
        CACHE_NOHASH = 2
} ;

typedef int (*cache_compare)(const struct CACHED_GENERIC *cached, const struct CACHED_GENERIC *item);
typedef void (*cache_free)(const struct CACHED_GENERIC *cached);
typedef int (*cache_hash)(const struct CACHED_GENERIC *cached);

struct HASH_ENTRY
{
        struct HASH_ENTRY *next;
        struct CACHED_GENERIC *entry;
} ;

struct CACHE_HEADER
{
        const char *name;
        struct CACHED_GENERIC *most_recent_entry;
        struct CACHED_GENERIC *oldest_entry;
        struct CACHED_GENERIC *free_entry;
        struct HASH_ENTRY *free_hash;
        struct HASH_ENTRY **first_hash;
        cache_free dofree;
        cache_hash dohash;
        unsigned long reads;
        unsigned long writes;
        unsigned long hits;
        int fixed_size;
        int max_hash;
        struct CACHED_GENERIC entry[0];
} ;

        /* cast to generic, avoiding gcc warnings */
#define GENERIC(pstr) ((const struct CACHED_GENERIC*)(const void*)(pstr))

struct CACHED_GENERIC *fs_fetch_cache(struct CACHE_HEADER *cache, const struct CACHED_GENERIC *wanted, cache_compare compare);
struct CACHED_GENERIC *fs_enter_cache(struct CACHE_HEADER *cache, const struct CACHED_GENERIC *item, cache_compare compare);
int fs_invalidate_cache(struct CACHE_HEADER *cache, const struct CACHED_GENERIC *item, cache_compare compare, int flags);
int fs_remove_cache(struct CACHE_HEADER *cache, struct CACHED_GENERIC *item, int flags);
void fs_create_lru_caches(FSVolume *vol);
void fs_free_lru_caches(FSVolume *vol);


C_END_EXTERN_C

#endif // sandbox_CACHE_H
