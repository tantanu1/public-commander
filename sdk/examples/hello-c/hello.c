/* hello: a tiny read-only Commander vfs plugin in plain C (ABI v1).
 *
 * Mount it by typing  hello://  in the path bar. It shows a fixed tree:
 *
 *   hello://
 *     readme.txt
 *     greetings/
 *       english.txt
 *       french.txt
 *
 * F3 views a file, F5 copies it out. Write calls are left NULL, so the host
 * treats the plugin as read-only (F7/F8/F6 into it are refused).
 *
 * Build (guide: docs/PLUGIN-DEV.md):
 *   Windows (MSVC):  cl /LD /O2 /I ..\..\include hello.c /Fe:hello.dll
 *   Linux:           cc -shared -fPIC -O2 -I ../../include hello.c -o libhello.so
 *   macOS:           cc -dynamiclib -O2 -I ../../include hello.c -o libhello.dylib
 */
#include <stdlib.h>
#include <string.h>

#include "commander_plugin.h"

/* ---- the fixed tree ------------------------------------------------------ */

typedef struct node {
    const char* path; /* '/'-separated, relative to the mount root */
    uint32_t kind;    /* CMDR_FILE or CMDR_DIR */
    const char* text; /* file contents (NULL for dirs) */
} node;

static const node TREE[] = {
    {"readme.txt", CMDR_FILE, "Hello from a C plugin!\nPress F5 to copy me out.\n"},
    {"greetings", CMDR_DIR, NULL},
    {"greetings/english.txt", CMDR_FILE, "Hello, world!\n"},
    {"greetings/french.txt", CMDR_FILE, "Bonjour, le monde !\n"},
};
#define TREE_LEN (sizeof TREE / sizeof TREE[0])

/* 2026-01-01 00:00:00 UTC in unix nanoseconds: a fixed date for every entry. */
#define MTIME_NS 1767225600000000000LL

/* ---- helpers ------------------------------------------------------------- */

/* last_error() text is per calling thread. */
#if defined(_MSC_VER)
static __declspec(thread) const char* g_last_error = "";
#else
static _Thread_local const char* g_last_error = "";
#endif

static int32_t fail(int32_t status, const char* why) {
    g_last_error = why;
    return status;
}

static cmdr_str str_of(const char* s) {
    cmdr_str r = {(const uint8_t*)s, strlen(s)};
    return r;
}

/* Exact match of a NUL-terminated C string against a {ptr,len} string. */
static int str_eq(cmdr_str a, const char* b) {
    return a.len == strlen(b) && memcmp(a.ptr, b, a.len) == 0;
}

static const node* find(cmdr_str path) {
    for (size_t i = 0; i < TREE_LEN; i++)
        if (str_eq(path, TREE[i].path)) return &TREE[i];
    return NULL;
}

/* Is `n` a direct child of `dir` ("" = root)? Sets *name to its last path part. */
static int child_of(const node* n, cmdr_str dir, const char** name) {
    const char* slash = strrchr(n->path, '/');
    size_t parent_len = slash ? (size_t)(slash - n->path) : 0;
    if (parent_len != dir.len || memcmp(n->path, dir.ptr, dir.len) != 0) return 0;
    *name = slash ? slash + 1 : n->path;
    return 1;
}

/* Fill one entry. The host passes its own entry_size: write at most that many bytes. */
static void fill(cmdr_entry* out, size_t entry_size, const char* name, const node* n) {
    cmdr_entry e;
    memset(&e, 0, sizeof e);
    e.size = (uint32_t)(entry_size < sizeof e ? entry_size : sizeof e);
    e.kind = n ? n->kind : CMDR_DIR; /* n == NULL: the root */
    e.name = str_of(name);
    e.file_size = (n && n->text) ? strlen(n->text) : 0;
    e.mtime_ns = MTIME_NS;
    memcpy(out, &e, e.size);
}

/* ---- vfs functions ------------------------------------------------------- */

/* config is the mount string, e.g. "hello://". No per-mount state: return a dummy handle. */
static int32_t hello_open(cmdr_str config, void** fs_out) {
    (void)config;
    *fs_out = (void*)1;
    return CMDR_OK;
}

static void hello_close(void* fs) { (void)fs; }

static cmdr_str hello_last_error(void) { return str_of(g_last_error); }

typedef struct iter {
    char dir[256]; /* our own copy: host inputs are only borrowed for the call */
    size_t dir_len;
    size_t next;   /* index into TREE */
} iter;

static int32_t hello_list_begin(void* fs, cmdr_str dir, const cmdr_job* job, void** it_out) {
    (void)fs, (void)job;
    if (dir.len != 0) {
        const node* n = find(dir);
        if (!n) return fail(CMDR_NOT_FOUND, "no such folder");
        if (n->kind != CMDR_DIR) return fail(CMDR_INVALID, "not a folder");
    }
    if (dir.len >= sizeof(((iter*)0)->dir)) return fail(CMDR_INVALID, "path too long");
    iter* it = calloc(1, sizeof *it);
    if (!it) return fail(CMDR_INTERNAL, "out of memory");
    memcpy(it->dir, dir.ptr, dir.len);
    it->dir_len = dir.len;
    *it_out = it;
    return CMDR_OK;
}

/* Fill up to `cap` entries; *n == 0 means the listing is done. */
static int32_t hello_list_next(void* p, cmdr_entry* buf, size_t entry_size, size_t cap, size_t* n) {
    iter* it = p;
    cmdr_str dir = {(const uint8_t*)it->dir, it->dir_len};
    *n = 0;
    while (it->next < TREE_LEN && *n < cap) {
        const node* nd = &TREE[it->next++];
        const char* name;
        if (child_of(nd, dir, &name)) {
            /* buf is an array with stride entry_size (the host's sizeof). */
            fill((cmdr_entry*)((uint8_t*)buf + *n * entry_size), entry_size, name, nd);
            ++*n;
        }
    }
    return CMDR_OK;
}

static void hello_list_end(void* it) { free(it); }

static int32_t hello_stat(void* fs, cmdr_str path, const cmdr_job* job, cmdr_entry* out,
                          size_t entry_size) {
    (void)fs, (void)job;
    if (path.len == 0) { /* the root is always a dir */
        fill(out, entry_size, "", NULL);
        return CMDR_OK;
    }
    const node* n = find(path);
    if (!n) return fail(CMDR_NOT_FOUND, "no such file");
    fill(out, entry_size, "", n); /* stat ignores the name */
    return CMDR_OK;
}

typedef struct reader {
    const char* data;
    size_t len, pos;
} reader;

static int32_t hello_read_open(void* fs, cmdr_str path, uint64_t offset, const cmdr_job* job,
                               void** rd_out) {
    (void)fs, (void)job;
    const node* n = find(path);
    if (!n) return fail(CMDR_NOT_FOUND, "no such file");
    if (n->kind != CMDR_FILE) return fail(CMDR_INVALID, "not a file");
    reader* rd = calloc(1, sizeof *rd);
    if (!rd) return fail(CMDR_INTERNAL, "out of memory");
    rd->data = n->text;
    rd->len = strlen(n->text);
    rd->pos = offset < rd->len ? (size_t)offset : rd->len;
    *rd_out = rd;
    return CMDR_OK;
}

static int32_t hello_read(void* p, uint8_t* buf, size_t cap, size_t* n) {
    reader* rd = p;
    size_t left = rd->len - rd->pos;
    *n = left < cap ? left : cap; /* *n == 0 = end of file */
    memcpy(buf, rd->data + rd->pos, *n);
    rd->pos += *n;
    return CMDR_OK;
}

static void hello_read_close(void* rd) { free(rd); }

/* ---- the exported entry point ---------------------------------------------- */

static const cmdr_vfs VFS = {
    sizeof(cmdr_vfs),
    hello_open,
    hello_close,
    hello_last_error,
    hello_list_begin,
    hello_list_next,
    hello_list_end,
    hello_stat,
    hello_read_open,
    hello_read,
    hello_read_close,
    /* write_open, write, write_finish, write_abort, mkdir, remove_file, remove_dir,
     * rename: NULL = not supported (read-only plugin). */
};

static const cmdr_plugin PLUGIN = {
    sizeof(cmdr_plugin),
    CMDR_ABI,
    &VFS,
    NULL, /* viewer: reserved */
    NULL, /* columns: reserved */
    NULL, /* commands: reserved */
};

CMDR_EXPORT const cmdr_plugin* cmdr_plugin_entry(uint32_t host_abi, const cmdr_host* host) {
    (void)host; /* host->log(host->ctx, CMDR_LOG_INFO, msg) is available if you need it */
    return host_abi == CMDR_ABI ? &PLUGIN : NULL;
}
