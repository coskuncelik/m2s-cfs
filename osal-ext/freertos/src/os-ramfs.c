/*
 * os-ramfs.c — m2s-cfs static RAM file system core. Layout and rules: os-ramfs.h.
 */
#include <string.h>

#include "os-ramfs.h"

#define RAMFS_MAGIC   0x4D32524DU /* "M2RM" */
#define RAMFS_VERSION 1U

#define RAMFS_FAT_FREE 0xFFFFFFFEU
#define RAMFS_FAT_END  0xFFFFFFFFU
#define RAMFS_NONE     0xFFFFFFFFU /* "no node" from resolve() */

#define RAMFS_ALIGN8(x) (((x) + 7U) & ~(size_t)7U)

struct RamFS_Header
{
    uint32_t magic;
    uint32_t version;
    uint32_t total_bytes; /* usable bytes from the header start */
    uint32_t block_size;
    uint32_t num_blocks;
    uint32_t num_nodes;
    uint32_t node_off; /* byte offsets from the header start */
    uint32_t fat_off;
    uint32_t data_off;
    uint32_t alloc_hint; /* where the next free-block search starts */
};

typedef struct
{
    int64_t  mtime;
    uint32_t parent;
    uint32_t first; /* first data block, or RAMFS_FAT_END */
    uint32_t size;
    uint32_t mode;
    uint8_t  type; /* 0 = free, RAMFS_TYPE_FILE, RAMFS_TYPE_DIR */
    uint8_t  pad[3];
    char     name[RAMFS_NAME_LEN];
} RamFS_Node_t;

/* ---- layout helpers ---------------------------------------------------------------------- */

static RamFS_Node_t *ramfs_nodes(const RamFS_t *fs)
{
    return (RamFS_Node_t *)((uintptr_t)fs + fs->node_off);
}

static uint32_t *ramfs_fat(const RamFS_t *fs)
{
    return (uint32_t *)((uintptr_t)fs + fs->fat_off);
}

static uint8_t *ramfs_block(const RamFS_t *fs, uint32_t block)
{
    return (uint8_t *)((uintptr_t)fs + fs->data_off + ((size_t)block * fs->block_size));
}

/* Header start for a memory area: aligned to 8 bytes. Returns NULL if the area is too small. */
static RamFS_t *ramfs_base(void *mem, size_t bytes, size_t *usable)
{
    uintptr_t addr = (uintptr_t)mem;
    size_t    skip = (size_t)(((addr + 7U) & ~(uintptr_t)7U) - addr);

    if (mem == NULL || bytes < skip + sizeof(RamFS_t))
    {
        return NULL;
    }
    *usable = bytes - skip;
    if (*usable > UINT32_MAX)
    {
        *usable = UINT32_MAX; /* offsets are 32-bit */
    }
    return (RamFS_t *)(addr + skip);
}

static RamFS_Node_t *ramfs_node(const RamFS_t *fs, uint32_t index)
{
    RamFS_Node_t *node;

    if (index >= fs->num_nodes)
    {
        return NULL;
    }
    node = &ramfs_nodes(fs)[index];
    return (node->type != 0U) ? node : NULL;
}

/* ---- block allocation -------------------------------------------------------------------- */

static uint32_t ramfs_alloc_block(RamFS_t *fs)
{
    uint32_t *fat = ramfs_fat(fs);
    uint32_t  i;
    uint32_t  b;

    for (i = 0; i < fs->num_blocks; ++i)
    {
        b = (fs->alloc_hint + i) % fs->num_blocks;
        if (fat[b] == RAMFS_FAT_FREE)
        {
            fat[b]         = RAMFS_FAT_END;
            fs->alloc_hint = (b + 1U) % fs->num_blocks;
            memset(ramfs_block(fs, b), 0, fs->block_size);
            return b;
        }
    }
    return RAMFS_FAT_END;
}

static void ramfs_free_chain(RamFS_t *fs, uint32_t block)
{
    uint32_t *fat = ramfs_fat(fs);
    uint32_t  next;

    while (block != RAMFS_FAT_END)
    {
        next       = fat[block];
        fat[block] = RAMFS_FAT_FREE;
        block      = next;
    }
}

/* Keep exactly ceil(size / block_size) blocks; free the rest. */
static void ramfs_trim_chain(RamFS_t *fs, RamFS_Node_t *node)
{
    uint32_t *fat  = ramfs_fat(fs);
    uint32_t  keep = (uint32_t)(((uint64_t)node->size + fs->block_size - 1U) / fs->block_size);
    uint32_t *link = &node->first;

    while (keep > 0U && *link != RAMFS_FAT_END)
    {
        link = &fat[*link];
        --keep;
    }
    ramfs_free_chain(fs, *link);
    *link = RAMFS_FAT_END;
}

/* ---- paths ------------------------------------------------------------------------------- */

/* Next path component: skips separators and "." components. len == 0 at the end of the path. */
static int32_t ramfs_next_component(const char **path, const char **start, size_t *len)
{
    const char *p = *path;

    for (;;)
    {
        while (*p == '/')
        {
            ++p;
        }
        *start = p;
        while (*p != '\0' && *p != '/')
        {
            ++p;
        }
        *len  = (size_t)(p - *start);
        *path = p;

        if (*len == 1U && (*start)[0] == '.')
        {
            continue;
        }
        if (*len == 2U && (*start)[0] == '.' && (*start)[1] == '.')
        {
            return RAMFS_ERR_NAME;
        }
        if (*len >= RAMFS_NAME_LEN)
        {
            return RAMFS_ERR_NAME;
        }
        return RAMFS_OK;
    }
}

static uint32_t ramfs_find_child(const RamFS_t *fs, uint32_t dir, const char *name)
{
    const RamFS_Node_t *nodes = ramfs_nodes(fs);
    uint32_t            i;

    for (i = 1; i < fs->num_nodes; ++i)
    {
        if (nodes[i].type != 0U && nodes[i].parent == dir && strncmp(nodes[i].name, name, RAMFS_NAME_LEN) == 0)
        {
            return i;
        }
    }
    return RAMFS_NONE;
}

/*
 * Walk a path. On RAMFS_OK, *parent is the directory holding the last component (whose name is
 * copied to leaf) and *node is that component, or RAMFS_NONE if it does not exist. The root
 * itself gives *parent = *node = 0 and leaf "". RAMFS_ERR_NOT_FOUND / RAMFS_ERR_NOT_DIR if a
 * directory before the last component is missing or is a file.
 */
static int32_t ramfs_resolve(const RamFS_t *fs, const char *path, uint32_t *parent, uint32_t *node,
                             char leaf[RAMFS_NAME_LEN])
{
    uint32_t    cur = 0;
    uint32_t    par = 0;
    const char *start;
    size_t      len;
    int32_t     rc;

    leaf[0] = '\0';
    if (path == NULL)
    {
        return RAMFS_ERR_INVALID;
    }

    for (;;)
    {
        rc = ramfs_next_component(&path, &start, &len);
        if (rc != RAMFS_OK)
        {
            return rc;
        }
        if (len == 0U)
        {
            break;
        }
        if (cur == RAMFS_NONE)
        {
            return RAMFS_ERR_NOT_FOUND;
        }
        if (ramfs_nodes(fs)[cur].type != RAMFS_TYPE_DIR)
        {
            return RAMFS_ERR_NOT_DIR;
        }
        memcpy(leaf, start, len);
        leaf[len] = '\0';
        par       = cur;
        cur       = ramfs_find_child(fs, cur, leaf);
    }

    *parent = par;
    *node   = cur;
    return RAMFS_OK;
}

/* ---- volume ------------------------------------------------------------------------------ */

int32_t RamFS_Format(void *mem, size_t bytes, uint32_t block_size, uint32_t num_nodes, uint32_t root_mode)
{
    RamFS_t *     fs;
    RamFS_Node_t *root;
    size_t        usable;
    uint64_t      fat_off;
    uint64_t      data_off;
    uint64_t      num_blocks;
    uint32_t      i;

    fs = ramfs_base(mem, bytes, &usable);
    if (fs == NULL || block_size < 16U || num_nodes < 1U)
    {
        return RAMFS_ERR_INVALID;
    }

    fat_off = RAMFS_ALIGN8(sizeof(RamFS_t)) + ((uint64_t)num_nodes * sizeof(RamFS_Node_t));
    if (fat_off >= usable)
    {
        return RAMFS_ERR_INVALID;
    }
    num_blocks = (usable - fat_off) / ((uint64_t)block_size + sizeof(uint32_t));
    for (;;)
    {
        if (num_blocks == 0U)
        {
            return RAMFS_ERR_INVALID;
        }
        data_off = RAMFS_ALIGN8(fat_off + (num_blocks * sizeof(uint32_t)));
        if (data_off + (num_blocks * block_size) <= usable)
        {
            break;
        }
        --num_blocks; /* alignment padding did not fit */
    }

    fs->magic       = 0; /* written last */
    fs->version     = RAMFS_VERSION;
    fs->total_bytes = (uint32_t)usable;
    fs->block_size  = block_size;
    fs->num_blocks  = (uint32_t)num_blocks;
    fs->num_nodes   = num_nodes;
    fs->node_off    = (uint32_t)RAMFS_ALIGN8(sizeof(RamFS_t));
    fs->fat_off     = (uint32_t)fat_off;
    fs->data_off    = (uint32_t)data_off;
    fs->alloc_hint  = 0;

    memset(ramfs_nodes(fs), 0, (size_t)num_nodes * sizeof(RamFS_Node_t));
    for (i = 0; i < fs->num_blocks; ++i)
    {
        ramfs_fat(fs)[i] = RAMFS_FAT_FREE;
    }

    root         = &ramfs_nodes(fs)[0];
    root->type   = RAMFS_TYPE_DIR;
    root->parent = 0;
    root->first  = RAMFS_FAT_END;
    root->mode   = root_mode;

    fs->magic = RAMFS_MAGIC;
    return RAMFS_OK;
}

RamFS_t *RamFS_Attach(void *mem, size_t bytes)
{
    RamFS_t *fs;
    size_t   usable;
    uint64_t fat_end;
    uint64_t data_end;

    fs = ramfs_base(mem, bytes, &usable);
    if (fs == NULL || fs->magic != RAMFS_MAGIC || fs->version != RAMFS_VERSION || fs->total_bytes > usable ||
        fs->block_size < 16U || fs->num_nodes < 1U || fs->num_blocks < 1U ||
        fs->node_off != RAMFS_ALIGN8(sizeof(RamFS_t)))
    {
        return NULL;
    }
    fat_end  = (uint64_t)fs->fat_off + ((uint64_t)fs->num_blocks * sizeof(uint32_t));
    data_end = (uint64_t)fs->data_off + ((uint64_t)fs->num_blocks * fs->block_size);
    if (fs->fat_off != fs->node_off + ((uint64_t)fs->num_nodes * sizeof(RamFS_Node_t)) || fat_end > fs->data_off ||
        (fs->data_off % 8U) != 0U || data_end > fs->total_bytes || fs->alloc_hint >= fs->num_blocks ||
        ramfs_nodes(fs)[0].type != RAMFS_TYPE_DIR)
    {
        return NULL;
    }
    return fs;
}

int32_t RamFS_Check(const RamFS_t *fs)
{
    const RamFS_Node_t *nodes = ramfs_nodes(fs);
    const uint32_t *    fat   = ramfs_fat(fs);
    uint64_t            used  = 0;
    uint64_t            free_blocks = 0;
    uint32_t            i;
    uint32_t            b;
    uint32_t            steps;
    uint32_t            up;

    for (i = 0; i < fs->num_blocks; ++i)
    {
        if (fat[i] == RAMFS_FAT_FREE)
        {
            ++free_blocks;
        }
        else if (fat[i] != RAMFS_FAT_END && fat[i] >= fs->num_blocks)
        {
            return RAMFS_ERR_CORRUPT;
        }
    }

    for (i = 0; i < fs->num_nodes; ++i)
    {
        const RamFS_Node_t *n = &nodes[i];

        if (n->type == 0U)
        {
            continue;
        }
        if ((n->type != RAMFS_TYPE_FILE && n->type != RAMFS_TYPE_DIR) || n->parent >= fs->num_nodes ||
            nodes[n->parent].type != RAMFS_TYPE_DIR || memchr(n->name, '\0', RAMFS_NAME_LEN) == NULL ||
            (i != 0U && n->name[0] == '\0'))
        {
            return RAMFS_ERR_CORRUPT;
        }

        /* The parent chain must reach the root. */
        up = i;
        for (steps = 0; up != 0U && up < fs->num_nodes && steps <= fs->num_nodes; ++steps)
        {
            up = nodes[up].parent;
        }
        if (up != 0U)
        {
            return RAMFS_ERR_CORRUPT;
        }

        if (n->type == RAMFS_TYPE_DIR)
        {
            if (n->first != RAMFS_FAT_END || n->size != 0U)
            {
                return RAMFS_ERR_CORRUPT;
            }
            continue;
        }

        /* File: chain length must match the size, every link must be in range. */
        steps = 0;
        for (b = n->first; b != RAMFS_FAT_END; b = fat[b])
        {
            if (b >= fs->num_blocks || fat[b] == RAMFS_FAT_FREE || ++steps > fs->num_blocks)
            {
                return RAMFS_ERR_CORRUPT;
            }
        }
        if (steps != (uint32_t)(((uint64_t)n->size + fs->block_size - 1U) / fs->block_size))
        {
            return RAMFS_ERR_CORRUPT;
        }
        used += steps;
    }

    /* Every block is either free or in exactly one chain (a shared or leaked block breaks the sum). */
    return (used + free_blocks == fs->num_blocks) ? RAMFS_OK : RAMFS_ERR_CORRUPT;
}

void RamFS_Usage(const RamFS_t *fs, uint32_t *block_size, uint32_t *total_blocks, uint32_t *free_blocks)
{
    const uint32_t *fat = ramfs_fat(fs);
    uint32_t        n   = 0;
    uint32_t        i;

    for (i = 0; i < fs->num_blocks; ++i)
    {
        if (fat[i] == RAMFS_FAT_FREE)
        {
            ++n;
        }
    }
    *block_size   = fs->block_size;
    *total_blocks = fs->num_blocks;
    *free_blocks  = n;
}

/* ---- namespace --------------------------------------------------------------------------- */

int32_t RamFS_Lookup(const RamFS_t *fs, const char *path, uint32_t *node)
{
    uint32_t parent;
    char     leaf[RAMFS_NAME_LEN];
    int32_t  rc = ramfs_resolve(fs, path, &parent, node, leaf);

    if (rc == RAMFS_OK && *node == RAMFS_NONE)
    {
        rc = RAMFS_ERR_NOT_FOUND;
    }
    return rc;
}

int32_t RamFS_Create(RamFS_t *fs, const char *path, uint32_t type, uint32_t mode, uint32_t *node)
{
    RamFS_Node_t *nodes = ramfs_nodes(fs);
    uint32_t      parent;
    uint32_t      existing;
    uint32_t      i;
    char          leaf[RAMFS_NAME_LEN];
    int32_t       rc;

    if (type != RAMFS_TYPE_FILE && type != RAMFS_TYPE_DIR)
    {
        return RAMFS_ERR_INVALID;
    }
    rc = ramfs_resolve(fs, path, &parent, &existing, leaf);
    if (rc != RAMFS_OK)
    {
        return rc;
    }
    if (existing != RAMFS_NONE)
    {
        return RAMFS_ERR_EXISTS; /* includes the root */
    }

    for (i = 1; i < fs->num_nodes; ++i)
    {
        if (nodes[i].type == 0U)
        {
            memset(&nodes[i], 0, sizeof(nodes[i]));
            nodes[i].type   = (uint8_t)type;
            nodes[i].parent = parent;
            nodes[i].first  = RAMFS_FAT_END;
            nodes[i].mode   = mode;
            memcpy(nodes[i].name, leaf, sizeof(nodes[i].name));
            *node = i;
            return RAMFS_OK;
        }
    }
    return RAMFS_ERR_NO_NODES;
}

int32_t RamFS_Remove(RamFS_t *fs, const char *path, uint32_t type)
{
    RamFS_Node_t *n;
    uint32_t      parent;
    uint32_t      index;
    uint32_t      cursor = 0;
    char          leaf[RAMFS_NAME_LEN];
    int32_t       rc;

    rc = ramfs_resolve(fs, path, &parent, &index, leaf);
    if (rc != RAMFS_OK)
    {
        return rc;
    }
    if (index == RAMFS_NONE)
    {
        return RAMFS_ERR_NOT_FOUND;
    }
    if (index == 0U)
    {
        return RAMFS_ERR_INVALID;
    }
    n = &ramfs_nodes(fs)[index];
    if (n->type != type)
    {
        return (n->type == RAMFS_TYPE_DIR) ? RAMFS_ERR_IS_DIR : RAMFS_ERR_NOT_DIR;
    }
    if (type == RAMFS_TYPE_DIR && RamFS_DirNext(fs, index, &cursor, leaf) == RAMFS_OK)
    {
        return RAMFS_ERR_NOT_EMPTY;
    }

    ramfs_free_chain(fs, n->first);
    memset(n, 0, sizeof(*n));
    return RAMFS_OK;
}

int32_t RamFS_Rename(RamFS_t *fs, const char *old_path, const char *new_path)
{
    RamFS_Node_t *nodes = ramfs_nodes(fs);
    uint32_t      old_parent;
    uint32_t      old_index;
    uint32_t      new_parent;
    uint32_t      new_index;
    uint32_t      up;
    char          old_leaf[RAMFS_NAME_LEN];
    char          new_leaf[RAMFS_NAME_LEN];
    int32_t       rc;

    rc = ramfs_resolve(fs, old_path, &old_parent, &old_index, old_leaf);
    if (rc != RAMFS_OK)
    {
        return rc;
    }
    if (old_index == RAMFS_NONE)
    {
        return RAMFS_ERR_NOT_FOUND;
    }
    rc = ramfs_resolve(fs, new_path, &new_parent, &new_index, new_leaf);
    if (rc != RAMFS_OK)
    {
        return rc;
    }
    if (old_index == 0U || new_leaf[0] == '\0')
    {
        return RAMFS_ERR_INVALID; /* the root cannot be moved or replaced */
    }
    if (new_index == old_index)
    {
        return RAMFS_OK;
    }
    if (new_index != RAMFS_NONE)
    {
        /* Only a file may replace a file (as POSIX rename does). */
        if (nodes[old_index].type != RAMFS_TYPE_FILE || nodes[new_index].type != RAMFS_TYPE_FILE)
        {
            return RAMFS_ERR_EXISTS;
        }
        ramfs_free_chain(fs, nodes[new_index].first);
        memset(&nodes[new_index], 0, sizeof(nodes[new_index]));
    }
    if (nodes[old_index].type == RAMFS_TYPE_DIR)
    {
        /* A directory cannot move into itself or below itself. */
        for (up = new_parent; up != 0U; up = nodes[up].parent)
        {
            if (up == old_index)
            {
                return RAMFS_ERR_INVALID;
            }
        }
    }

    nodes[old_index].parent = new_parent;
    memcpy(nodes[old_index].name, new_leaf, sizeof(nodes[old_index].name));
    return RAMFS_OK;
}

int32_t RamFS_GetInfo(const RamFS_t *fs, uint32_t node, RamFS_Info_t *info)
{
    const RamFS_Node_t *n = ramfs_node(fs, node);

    if (n == NULL)
    {
        return RAMFS_ERR_INVALID;
    }
    info->type  = n->type;
    info->size  = n->size;
    info->mode  = n->mode;
    info->mtime = n->mtime;
    return RAMFS_OK;
}

int32_t RamFS_SetMode(RamFS_t *fs, uint32_t node, uint32_t mode)
{
    RamFS_Node_t *n = ramfs_node(fs, node);

    if (n == NULL)
    {
        return RAMFS_ERR_INVALID;
    }
    n->mode = mode;
    return RAMFS_OK;
}

int32_t RamFS_SetTime(RamFS_t *fs, uint32_t node, int64_t mtime)
{
    RamFS_Node_t *n = ramfs_node(fs, node);

    if (n == NULL)
    {
        return RAMFS_ERR_INVALID;
    }
    n->mtime = mtime;
    return RAMFS_OK;
}

/* ---- file data --------------------------------------------------------------------------- */

int32_t RamFS_Read(const RamFS_t *fs, uint32_t node, uint32_t pos, void *buf, uint32_t len)
{
    const RamFS_Node_t *n   = ramfs_node(fs, node);
    const uint32_t *    fat = ramfs_fat(fs);
    uint8_t *           out = (uint8_t *)buf;
    uint32_t            block;
    uint32_t            off;
    uint32_t            chunk;
    uint32_t            done = 0;
    uint32_t            i;

    if (n == NULL || n->type != RAMFS_TYPE_FILE || (buf == NULL && len > 0U) || len > (uint32_t)INT32_MAX)
    {
        return RAMFS_ERR_INVALID;
    }
    if (pos >= n->size)
    {
        return 0;
    }
    if (len > n->size - pos)
    {
        len = n->size - pos;
    }

    block = n->first;
    for (i = pos / fs->block_size; i > 0U; --i)
    {
        block = fat[block];
    }
    off = pos % fs->block_size;

    while (done < len)
    {
        chunk = fs->block_size - off;
        if (chunk > len - done)
        {
            chunk = len - done;
        }
        memcpy(&out[done], ramfs_block(fs, block) + off, chunk);
        done += chunk;
        off = 0;
        block = fat[block];
    }
    return (int32_t)done;
}

int32_t RamFS_Write(RamFS_t *fs, uint32_t node, uint32_t pos, const void *buf, uint32_t len)
{
    RamFS_Node_t * n   = ramfs_node(fs, node);
    uint32_t *     fat = ramfs_fat(fs);
    const uint8_t *in  = (const uint8_t *)buf;
    uint32_t *     link;
    uint32_t       target;
    uint32_t       block = RAMFS_FAT_END;
    uint32_t       off;
    uint32_t       chunk;
    uint32_t       done = 0;
    uint32_t       i;

    if (n == NULL || n->type != RAMFS_TYPE_FILE || (buf == NULL && len > 0U) || len > (uint32_t)INT32_MAX ||
        (uint64_t)pos + len > (uint64_t)INT32_MAX)
    {
        return RAMFS_ERR_INVALID;
    }
    if (len == 0U)
    {
        return 0;
    }

    /* Walk to the block that holds pos, appending zeroed blocks if the file is shorter. */
    target = pos / fs->block_size;
    link   = &n->first;
    for (i = 0; ; ++i)
    {
        if (*link == RAMFS_FAT_END)
        {
            *link = ramfs_alloc_block(fs);
            if (*link == RAMFS_FAT_END)
            {
                break;
            }
        }
        block = *link;
        if (i == target)
        {
            break;
        }
        link = &fat[block];
    }

    off = pos % fs->block_size;
    while (*link != RAMFS_FAT_END && done < len)
    {
        chunk = fs->block_size - off;
        if (chunk > len - done)
        {
            chunk = len - done;
        }
        memcpy(ramfs_block(fs, block) + off, &in[done], chunk);
        done += chunk;
        off = 0;
        if (done < len)
        {
            link = &fat[block];
            if (*link == RAMFS_FAT_END)
            {
                *link = ramfs_alloc_block(fs);
            }
            block = *link;
        }
    }

    if (done > 0U && pos + done > n->size)
    {
        n->size = pos + done;
    }
    if (done < len)
    {
        ramfs_trim_chain(fs, n); /* drop gap blocks the failed write appended */
    }
    return (done > 0U) ? (int32_t)done : RAMFS_ERR_NO_SPACE;
}

int32_t RamFS_Truncate(RamFS_t *fs, uint32_t node)
{
    RamFS_Node_t *n = ramfs_node(fs, node);

    if (n == NULL || n->type != RAMFS_TYPE_FILE)
    {
        return RAMFS_ERR_INVALID;
    }
    ramfs_free_chain(fs, n->first);
    n->first = RAMFS_FAT_END;
    n->size  = 0;
    return RAMFS_OK;
}

/* ---- directories ------------------------------------------------------------------------- */

int32_t RamFS_DirNext(const RamFS_t *fs, uint32_t dir, uint32_t *cursor, char name[RAMFS_NAME_LEN])
{
    const RamFS_Node_t *nodes = ramfs_nodes(fs);
    uint32_t            i;

    for (i = (*cursor < 1U) ? 1U : *cursor; i < fs->num_nodes; ++i)
    {
        if (nodes[i].type != 0U && nodes[i].parent == dir)
        {
            memcpy(name, nodes[i].name, RAMFS_NAME_LEN);
            *cursor = i + 1U;
            return RAMFS_OK;
        }
    }
    *cursor = fs->num_nodes;
    return RAMFS_ERR_NOT_FOUND;
}
