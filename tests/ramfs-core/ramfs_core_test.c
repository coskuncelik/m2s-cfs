/*
 * ramfs_core_test.c — m2s-cfs host test of the static RAM file system core (step 04b).
 * Plain C on the host, no OSAL and no FreeRTOS. Built and run by scripts/04-build-osal-freertos-sim.sh.
 * Exit status 0 only if every check passes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os-ramfs.h"

static unsigned checks;
static unsigned failures;

#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        ++checks;                                                                \
        if (!(cond))                                                             \
        {                                                                        \
            ++failures;                                                          \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
        }                                                                        \
    } while (0)

#define BS 64U

static uint64_t area_words[8192]; /* 64 KiB, 8-byte aligned */
static uint8_t *area = (uint8_t *)area_words;

static uint32_t rng_state = 12345U;
static uint32_t rng(void)
{
    rng_state = rng_state * 1103515245U + 12345U;
    return rng_state >> 8;
}

static uint32_t free_blocks(const RamFS_t *fs)
{
    uint32_t bs, total, nfree;
    RamFS_Usage(fs, &bs, &total, &nfree);
    return nfree;
}

static void test_format_attach(void)
{
    RamFS_t *fs;
    uint32_t bs, total, nfree;

    /* Misaligned start and odd size still give a valid volume. */
    CHECK(RamFS_Format(area + 3, 10001, BS, 16, 7) == RAMFS_OK);
    fs = RamFS_Attach(area + 3, 10001);
    CHECK(fs != NULL);
    if (fs == NULL)
    {
        return;
    }
    RamFS_Usage(fs, &bs, &total, &nfree);
    CHECK(bs == BS);
    CHECK(total > 100 && total == nfree);
    CHECK(RamFS_Check(fs) == RAMFS_OK);

    /* Bad arguments and areas too small. */
    CHECK(RamFS_Format(NULL, 4096, BS, 8, 0) == RAMFS_ERR_INVALID);
    CHECK(RamFS_Format(area, 4096, 8, 8, 0) == RAMFS_ERR_INVALID);
    CHECK(RamFS_Format(area, 4096, BS, 0, 0) == RAMFS_ERR_INVALID);
    CHECK(RamFS_Format(area, 100, BS, 8, 0) == RAMFS_ERR_INVALID);

    /* Unformatted memory does not attach. */
    memset(area, 0xA5, 4096);
    CHECK(RamFS_Attach(area, 4096) == NULL);
}

static void test_namespace(void)
{
    RamFS_t *    fs;
    uint32_t     a, b, f, n, cursor;
    char         name[RAMFS_NAME_LEN];
    RamFS_Info_t info;
    int          seen_b = 0, seen_f = 0, others = 0;

    CHECK(RamFS_Format(area, 32768, BS, 32, 7) == RAMFS_OK);
    fs = RamFS_Attach(area, 32768);
    CHECK(fs != NULL);
    if (fs == NULL)
    {
        return;
    }

    CHECK(RamFS_Create(fs, "a", RAMFS_TYPE_DIR, 7, &a) == RAMFS_OK);
    CHECK(RamFS_Create(fs, "/a//b/", RAMFS_TYPE_DIR, 7, &b) == RAMFS_OK);
    CHECK(RamFS_Create(fs, "./a/./f", RAMFS_TYPE_FILE, 6, &f) == RAMFS_OK);
    CHECK(RamFS_Lookup(fs, "a/b", &n) == RAMFS_OK && n == b);
    CHECK(RamFS_Lookup(fs, "//a/f//", &n) == RAMFS_OK && n == f);
    CHECK(RamFS_Lookup(fs, "", &n) == RAMFS_OK && n == 0);
    CHECK(RamFS_Lookup(fs, "/", &n) == RAMFS_OK && n == 0);

    CHECK(RamFS_Create(fs, "a", RAMFS_TYPE_FILE, 6, &n) == RAMFS_ERR_EXISTS);
    CHECK(RamFS_Create(fs, "", RAMFS_TYPE_DIR, 7, &n) == RAMFS_ERR_EXISTS);
    CHECK(RamFS_Create(fs, "x/y", RAMFS_TYPE_FILE, 6, &n) == RAMFS_ERR_NOT_FOUND);
    CHECK(RamFS_Create(fs, "a/f/y", RAMFS_TYPE_FILE, 6, &n) == RAMFS_ERR_NOT_DIR);
    CHECK(RamFS_Create(fs, "a/../z", RAMFS_TYPE_FILE, 6, &n) == RAMFS_ERR_NAME);
    CHECK(RamFS_Create(fs, "a/0123456789012345678", RAMFS_TYPE_FILE, 6, &n) == RAMFS_OK); /* 19 chars */
    CHECK(RamFS_Create(fs, "a/01234567890123456789", RAMFS_TYPE_FILE, 6, &n) == RAMFS_ERR_NAME); /* 20 */
    CHECK(RamFS_Lookup(fs, "a/missing", &n) == RAMFS_ERR_NOT_FOUND);
    CHECK(RamFS_Lookup(fs, "missing/x", &n) == RAMFS_ERR_NOT_FOUND);

    /* Listing "a": b, f and the 19-character file, nothing else. */
    cursor = 0;
    while (RamFS_DirNext(fs, a, &cursor, name) == RAMFS_OK)
    {
        if (strcmp(name, "b") == 0)
            ++seen_b;
        else if (strcmp(name, "f") == 0)
            ++seen_f;
        else
            ++others;
    }
    CHECK(seen_b == 1 && seen_f == 1 && others == 1);
    CHECK(RamFS_DirNext(fs, a, &cursor, name) == RAMFS_ERR_NOT_FOUND);

    /* Mode and time are stored as given. */
    CHECK(RamFS_SetMode(fs, f, 2) == RAMFS_OK);
    CHECK(RamFS_SetTime(fs, f, -1234567890123LL) == RAMFS_OK);
    CHECK(RamFS_GetInfo(fs, f, &info) == RAMFS_OK && info.type == RAMFS_TYPE_FILE && info.mode == 2 &&
          info.mtime == -1234567890123LL && info.size == 0);

    /* Remove: type must match, directories must be empty, the root cannot go. */
    CHECK(RamFS_Remove(fs, "a", RAMFS_TYPE_FILE) == RAMFS_ERR_IS_DIR);
    CHECK(RamFS_Remove(fs, "a/f", RAMFS_TYPE_DIR) == RAMFS_ERR_NOT_DIR);
    CHECK(RamFS_Remove(fs, "a", RAMFS_TYPE_DIR) == RAMFS_ERR_NOT_EMPTY);
    CHECK(RamFS_Remove(fs, "/", RAMFS_TYPE_DIR) == RAMFS_ERR_INVALID);
    CHECK(RamFS_Remove(fs, "a/nothere", RAMFS_TYPE_FILE) == RAMFS_ERR_NOT_FOUND);

    /* Rename: move a file, rename a non-empty directory, replace a file, refuse loops. */
    CHECK(RamFS_Write(fs, f, 0, "hello", 5) == 5);
    CHECK(RamFS_Rename(fs, "a/f", "a/b/g") == RAMFS_OK);
    CHECK(RamFS_Lookup(fs, "a/f", &n) == RAMFS_ERR_NOT_FOUND);
    CHECK(RamFS_Rename(fs, "a", "c") == RAMFS_OK);
    CHECK(RamFS_Lookup(fs, "c/b/g", &n) == RAMFS_OK && n == f);
    CHECK(RamFS_Rename(fs, "c", "c/b/inside") == RAMFS_ERR_INVALID);
    CHECK(RamFS_Rename(fs, "c/b", "c/b") == RAMFS_OK);
    CHECK(RamFS_Create(fs, "c/h", RAMFS_TYPE_FILE, 6, &n) == RAMFS_OK);
    CHECK(RamFS_Write(fs, n, 0, "0123456789012345678901234567890123456789012345678901234567890123456789", 70) == 70);
    {
        uint32_t before = free_blocks(fs);
        CHECK(RamFS_Rename(fs, "c/b/g", "c/h") == RAMFS_OK); /* replaces h, frees its 2 blocks */
        CHECK(free_blocks(fs) == before + 2);
    }
    {
        char buf[8] = {0};
        CHECK(RamFS_Lookup(fs, "c/h", &n) == RAMFS_OK && n == f);
        CHECK(RamFS_Read(fs, n, 0, buf, sizeof(buf)) == 5 && memcmp(buf, "hello", 5) == 0);
    }
    CHECK(RamFS_Rename(fs, "c/h", "c/b") == RAMFS_ERR_EXISTS);   /* file onto directory */
    CHECK(RamFS_Rename(fs, "c/b", "c/h") == RAMFS_ERR_EXISTS);   /* directory onto file */
    CHECK(RamFS_Rename(fs, "c/none", "c/x") == RAMFS_ERR_NOT_FOUND);
    CHECK(RamFS_Rename(fs, "c/h", "nodir/x") == RAMFS_ERR_NOT_FOUND);
    CHECK(RamFS_Rename(fs, "/", "r") == RAMFS_ERR_INVALID);
    CHECK(RamFS_Check(fs) == RAMFS_OK);

    /* Attach again (as after a processor reset): contents are kept. */
    fs = RamFS_Attach(area, 32768);
    CHECK(fs != NULL && RamFS_Lookup(fs, "c/b", &n) == RAMFS_OK);
}

/* Random writes, seeks and reads checked byte for byte against a reference copy. */
static void test_data_random(void)
{
    static uint8_t ref[6000];
    static uint8_t buf[6000];
    RamFS_t *      fs;
    uint32_t       f, size = 0, i, pos, len;
    int32_t        rc;
    int            mismatches = 0;

    CHECK(RamFS_Format(area, 65536, BS, 16, 7) == RAMFS_OK);
    fs = RamFS_Attach(area, 65536);
    CHECK(fs != NULL);
    if (fs == NULL)
    {
        return;
    }
    CHECK(RamFS_Create(fs, "data", RAMFS_TYPE_FILE, 6, &f) == RAMFS_OK);
    memset(ref, 0, sizeof(ref));

    for (i = 0; i < 2000; ++i)
    {
        pos = rng() % 5000U;
        len = rng() % 900U;
        if (rng() % 2U)
        {
            uint32_t k;
            for (k = 0; k < len; ++k)
            {
                buf[k] = (uint8_t)rng();
            }
            if (pos + len > sizeof(ref))
            {
                len = (uint32_t)sizeof(ref) - pos;
            }
            rc = RamFS_Write(fs, f, pos, buf, len);
            if (rc != (int32_t)len)
            {
                ++mismatches;
            }
            memcpy(&ref[pos], buf, len);
            if (len > 0U && pos + len > size)
            {
                size = pos + len; /* the gap [old size, pos) stays zero in ref */
            }
        }
        else
        {
            uint32_t expect = (pos >= size) ? 0U : ((len < size - pos) ? len : size - pos);
            rc              = RamFS_Read(fs, f, pos, buf, len);
            if (rc != (int32_t)expect || memcmp(buf, &ref[pos], expect) != 0)
            {
                ++mismatches;
            }
        }
    }
    CHECK(mismatches == 0);
    {
        RamFS_Info_t info;
        CHECK(RamFS_GetInfo(fs, f, &info) == RAMFS_OK && info.size == size);
    }
    CHECK(RamFS_Read(fs, f, 0, buf, size) == (int32_t)size && memcmp(buf, ref, size) == 0);
    CHECK(RamFS_Check(fs) == RAMFS_OK);

    /* Truncate frees every block. */
    {
        uint32_t bs, total, nfree;
        CHECK(RamFS_Truncate(fs, f) == RAMFS_OK);
        RamFS_Usage(fs, &bs, &total, &nfree);
        CHECK(nfree == total);
        CHECK(RamFS_Read(fs, f, 0, buf, 10) == 0);
    }
    /* Writes past the end leave zeros; zero-length writes do nothing. */
    {
        uint8_t z[200];
        CHECK(RamFS_Write(fs, f, 150, "X", 1) == 1);
        CHECK(RamFS_Read(fs, f, 0, z, sizeof(z)) == 151);
        for (i = 0; i < 150; ++i)
        {
            if (z[i] != 0)
                break;
        }
        CHECK(i == 150 && z[150] == 'X');
        CHECK(RamFS_Write(fs, f, 1000, "", 0) == 0);
    }
    CHECK(RamFS_Check(fs) == RAMFS_OK);
}

static void test_limits(void)
{
    RamFS_t *    fs;
    uint32_t     f, g, n, bs, total, nfree, i;
    int32_t      rc;
    uint8_t      big[4096];
    RamFS_Info_t info;
    char         name[8];

    memset(big, 'B', sizeof(big));
    CHECK(RamFS_Format(area, 4096, BS, 6, 7) == RAMFS_OK);
    fs = RamFS_Attach(area, 4096);
    CHECK(fs != NULL);
    if (fs == NULL)
    {
        return;
    }
    RamFS_Usage(fs, &bs, &total, &nfree);

    /* Disk full: a write returns what fit, then RAMFS_ERR_NO_SPACE. */
    CHECK(RamFS_Create(fs, "f", RAMFS_TYPE_FILE, 6, &f) == RAMFS_OK);
    rc = RamFS_Write(fs, f, 0, big, sizeof(big));
    CHECK(rc == (int32_t)(total * BS));
    CHECK(free_blocks(fs) == 0);
    CHECK(RamFS_Write(fs, f, (uint32_t)rc, "x", 1) == RAMFS_ERR_NO_SPACE);
    CHECK(RamFS_Create(fs, "g", RAMFS_TYPE_FILE, 6, &g) == RAMFS_OK);
    CHECK(RamFS_Write(fs, g, 0, "x", 1) == RAMFS_ERR_NO_SPACE);
    CHECK(RamFS_GetInfo(fs, g, &info) == RAMFS_OK && info.size == 0);
    CHECK(RamFS_Check(fs) == RAMFS_OK);

    /* A failed write far past the end must not leave gap blocks behind. */
    CHECK(RamFS_Remove(fs, "f", RAMFS_TYPE_FILE) == RAMFS_OK);
    CHECK(free_blocks(fs) == total);

    /* Reused blocks come back zeroed: every block held 'B' before the remove. */
    {
        uint8_t z[3 * BS + 1];
        CHECK(RamFS_Write(fs, g, 3 * BS, "y", 1) == 1);
        CHECK(RamFS_Read(fs, g, 0, z, sizeof(z)) == (int32_t)sizeof(z));
        for (i = 0; i < 3 * BS && z[i] == 0; ++i)
        {
        }
        CHECK(i == 3 * BS && z[3 * BS] == 'y');
        CHECK(RamFS_Truncate(fs, g) == RAMFS_OK);
    }

    CHECK(RamFS_Write(fs, g, (total + 5U) * BS, "x", 1) == RAMFS_ERR_NO_SPACE);
    CHECK(free_blocks(fs) == total);
    CHECK(RamFS_Check(fs) == RAMFS_OK);

    /* Node table full: 6 nodes = root + 5. */
    CHECK(RamFS_Remove(fs, "g", RAMFS_TYPE_FILE) == RAMFS_OK);
    for (i = 0; i < 5; ++i)
    {
        snprintf(name, sizeof(name), "n%u", (unsigned)i);
        CHECK(RamFS_Create(fs, name, RAMFS_TYPE_FILE, 6, &n) == RAMFS_OK);
    }
    CHECK(RamFS_Create(fs, "n5", RAMFS_TYPE_FILE, 6, &n) == RAMFS_ERR_NO_NODES);
    CHECK(RamFS_Check(fs) == RAMFS_OK);
}

static void test_corruption(void)
{
    RamFS_t *fs;
    uint32_t f;

    CHECK(RamFS_Format(area, 8192, BS, 8, 7) == RAMFS_OK);
    fs = RamFS_Attach(area, 8192);
    CHECK(fs != NULL);
    if (fs == NULL)
    {
        return;
    }
    CHECK(RamFS_Create(fs, "f", RAMFS_TYPE_FILE, 6, &f) == RAMFS_OK);
    CHECK(RamFS_Write(fs, f, 0, "abcdefghijklmnopqrstuvwxyz", 26) == 26);
    CHECK(RamFS_Check(fs) == RAMFS_OK);

    /* A wrong file size (chain length no longer matches) is detected. */
    {
        uint8_t *nodes = (uint8_t *)fs + 40; /* node_off = ALIGN8(sizeof header) = 40 */
        uint32_t saved;
        memcpy(&saved, nodes + 48 + 16, 4); /* node 1, field size */
        {
            uint32_t bad = 1000;
            memcpy(nodes + 48 + 16, &bad, 4);
        }
        CHECK(RamFS_Check(fs) == RAMFS_ERR_CORRUPT);
        memcpy(nodes + 48 + 16, &saved, 4);
        CHECK(RamFS_Check(fs) == RAMFS_OK);
    }
    /* A damaged magic number stops the volume from attaching. */
    area[0] ^= 0xFF;
    CHECK(RamFS_Attach(area, 8192) == NULL);
    area[0] ^= 0xFF;
    CHECK(RamFS_Attach(area, 8192) != NULL);
}

int main(void)
{
    test_format_attach();
    test_namespace();
    test_data_random();
    test_limits();
    test_corruption();

    printf("ramfs-core: %u checks, %u failed\n", checks, failures);
    printf("RESULT: %s\n", (failures == 0U) ? "PASS" : "FAIL");
    return (failures == 0U) ? EXIT_SUCCESS : EXIT_FAILURE;
}
