/*
 * os-ramfs.h — m2s-cfs static RAM file system core (step 04b, docs/DECISIONS.md D19, D25).
 *
 * A volume lives entirely inside one caller-provided memory area:
 *
 *     [header][node table][block table (FAT)][data blocks]
 *
 * Nodes are files or directories; a directory's children are the nodes whose parent is that
 * directory (no directory data blocks). Node 0 is the root directory. A file's data is a
 * chain of fixed-size blocks linked through the block table. Bytes past a file's size are
 * always zero, so a write past the end leaves zeros in the gap.
 *
 * Plain C, no OSAL, no FreeRTOS, no dynamic memory, not thread-safe: the caller serialises
 * all calls on one volume. Paths are relative to the volume root: components separated by
 * any number of '/', "." components are ignored, ".." is rejected.
 */
#ifndef OS_RAMFS_H
#define OS_RAMFS_H

#include <stddef.h>
#include <stdint.h>

/* Name buffer per node, including the terminating NUL (OSAL: OS_MAX_FILE_NAME). */
#ifndef RAMFS_NAME_LEN
#define RAMFS_NAME_LEN 20
#endif

#define RAMFS_OK               0
#define RAMFS_ERR_NOT_FOUND    (-1)
#define RAMFS_ERR_EXISTS       (-2)
#define RAMFS_ERR_NOT_DIR      (-3)
#define RAMFS_ERR_IS_DIR       (-4)
#define RAMFS_ERR_NOT_EMPTY    (-5)
#define RAMFS_ERR_NO_SPACE     (-6) /* no free data block */
#define RAMFS_ERR_NO_NODES     (-7) /* no free node */
#define RAMFS_ERR_NAME         (-8) /* component too long, or ".." */
#define RAMFS_ERR_INVALID      (-9) /* bad argument, or operation on the root */
#define RAMFS_ERR_CORRUPT      (-10)

#define RAMFS_TYPE_FILE 1U
#define RAMFS_TYPE_DIR  2U

typedef struct RamFS_Header RamFS_t;

typedef struct
{
    uint32_t type;  /* RAMFS_TYPE_FILE or RAMFS_TYPE_DIR */
    uint32_t size;  /* bytes (0 for directories) */
    uint32_t mode;  /* opaque to the core; set at create and by RamFS_SetMode */
    int64_t  mtime; /* opaque to the core; set by RamFS_SetTime */
} RamFS_Info_t;

/* Format a new, empty volume in mem[0..bytes). Needs room for the header, num_nodes nodes and
 * at least one block. block_size >= 16. */
int32_t RamFS_Format(void *mem, size_t bytes, uint32_t block_size, uint32_t num_nodes, uint32_t root_mode);

/* Return the volume in mem[0..bytes) if its header is valid, else NULL. Contents are kept. */
RamFS_t *RamFS_Attach(void *mem, size_t bytes);

/* Full consistency check of an attached volume: RAMFS_OK or RAMFS_ERR_CORRUPT. */
int32_t RamFS_Check(const RamFS_t *fs);

void RamFS_Usage(const RamFS_t *fs, uint32_t *block_size, uint32_t *total_blocks, uint32_t *free_blocks);

int32_t RamFS_Lookup(const RamFS_t *fs, const char *path, uint32_t *node);
int32_t RamFS_Create(RamFS_t *fs, const char *path, uint32_t type, uint32_t mode, uint32_t *node);
int32_t RamFS_Remove(RamFS_t *fs, const char *path, uint32_t type); /* a directory must be empty */
int32_t RamFS_Rename(RamFS_t *fs, const char *old_path, const char *new_path);

int32_t RamFS_GetInfo(const RamFS_t *fs, uint32_t node, RamFS_Info_t *info);
int32_t RamFS_SetMode(RamFS_t *fs, uint32_t node, uint32_t mode);
int32_t RamFS_SetTime(RamFS_t *fs, uint32_t node, int64_t mtime);

/* File data. Read returns bytes read (0 at or past the end). Write returns bytes written; if the
 * volume fills up it returns the bytes written so far, or RAMFS_ERR_NO_SPACE if none. */
int32_t RamFS_Read(const RamFS_t *fs, uint32_t node, uint32_t pos, void *buf, uint32_t len);
int32_t RamFS_Write(RamFS_t *fs, uint32_t node, uint32_t pos, const void *buf, uint32_t len);
int32_t RamFS_Truncate(RamFS_t *fs, uint32_t node); /* to zero length */

/* Directory listing: start with *cursor = 0; RAMFS_ERR_NOT_FOUND at the end. */
int32_t RamFS_DirNext(const RamFS_t *fs, uint32_t dir, uint32_t *cursor, char name[RAMFS_NAME_LEN]);

#endif /* OS_RAMFS_H */
