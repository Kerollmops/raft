#include <fcntl.h>
#include <unistd.h>

#include "../../src/uv_error.h"
#include "../../src/uv_os.h"
#include "../lib/dir.h"
#include "../lib/runner.h"

#define BLOCK_SIZE_ 4096
#define BUF_SIZE 4096
#define SEGMENT_SIZE (8 * 1024 * 1024)

struct file
{
    FIXTURE_DIR;
    int fd;
};

static void *setupFile(MUNIT_UNUSED const MunitParameter params[],
                       MUNIT_UNUSED void *user_data)
{
    struct file *f = munit_malloc(sizeof *f);
    int flags = O_WRONLY | O_CREAT;
    uvErrMsg errmsg;
    int rv;
    SETUP_DIR;
    if (f->dir == NULL) {
        free(f);
        return NULL;
    }
    rv = uvOpenFile(f->dir, "x", flags, &f->fd, errmsg);
    munit_assert_int(rv, ==, 0);
    rv = posix_fallocate(f->fd, 0, SEGMENT_SIZE);
    munit_assert_int(rv, ==, 0);
    rv = fsync(f->fd);
    munit_assert_int(rv, ==, 0);
    return f;
}

static void tearDownFile(void *data)
{
    struct file *f = data;
    if (f == NULL) {
        return;
    }
    close(f->fd);
    TEAR_DOWN_DIR;
}

static char *appendN[] = {"1", "16", "256", "1024", NULL};

static char *dirFs[] = {"ext4", "btrfs", "xfs", "zfs", NULL};

static MunitParameterEnum appendParams[] = {
    {"n", appendN},
    {TEST_DIR_FS, dirFs},
    {NULL, NULL},
};

/******************************************************************************
 *
 * Synchronous writes with direct I/O.
 *
 *****************************************************************************/

#define SET_DIO                                                 \
    {                                                           \
        int flags_ = O_WRONLY | O_DIRECT | O_DSYNC;             \
        uvErrMsg errmsg_;                                       \
        int rv_;                                                \
        close(f->fd);                                           \
        rv_ = uvOpenFile(f->dir, "x", flags_, &f->fd, errmsg_); \
        munit_assert_int(rv_, ==, 0);                           \
    }

SUITE(dioWrite)

TEST(dioWrite, append, setupFile, tearDownFile, 0, appendParams)
{
    struct file *f = data;
    const char *n = munit_parameters_get(params, "n");
    void *buf;
    int rv;
    int i;

    SKIP_IF_NO_FIXTURE;
    SET_DIO;

    buf = aligned_alloc(BLOCK_SIZE_, BUF_SIZE);
    munit_assert_ptr_not_null(buf);

    for (i = 0; i < atoi(n); i++) {
        memset(buf, i, BUF_SIZE);
        rv = pwrite(f->fd, buf, BUF_SIZE, BUF_SIZE * i);
        munit_assert_int(rv, ==, BUF_SIZE);
    }

    free(buf);

    return MUNIT_OK;
}

/******************************************************************************
 *
 * Synchronous writes with buffered I/O.
 *
 *****************************************************************************/

SUITE(ioWrite)

TEST(ioWrite, append, setupFile, tearDownFile, 0, appendParams)
{
    struct file *f = data;
    const char *n = munit_parameters_get(params, "n");
    void *buf;
    int rv;
    int i;

    SKIP_IF_NO_FIXTURE;

    buf = munit_malloc(BUF_SIZE);

    for (i = 0; i < atoi(n); i++) {
        memset(buf, i, BUF_SIZE);
        rv = pwrite(f->fd, buf, BUF_SIZE, BUF_SIZE * i);
        munit_assert_int(rv, ==, BUF_SIZE);
        rv = fdatasync(f->fd);
        munit_assert_int(rv, ==, 0);
    }

    free(buf);

    return MUNIT_OK;
}
