/* Commander plugin C ABI, version 1. Guide: docs/PLUGIN-DEV.md.
 *
 * Rules:
 * - Export one symbol: cmdr_plugin_entry. Return NULL to refuse the host ABI.
 * - Every struct starts with `uint32_t size` (= sizeof as compiled). Fields are only ever
 *   appended; the host treats fields past `size` as NULL. A NULL fn = unsupported.
 * - Strings: cmdr_str {ptr,len}, UTF-8, NOT NUL-terminated.
 * - Paths inside a vfs: '/'-separated, relative to the mount root, "" = root, no "..".
 * - Host -> plugin inputs are borrowed for the call only. Plugin -> host outputs (entry names,
 *   last_error text) stay valid until the next call on the same handle (or thread for
 *   last_error); the host copies them at once.
 * - Handles (fs, iterator, reader, writer) are opaque `void*`. A `const cmdr_job*` passed to an
 *   *_begin/*_open fn stays valid until the matching end/close/finish/abort.
 * - Threading: calls never come from the UI thread. threading = "serial" (manifest default):
 *   the host serializes every call into the plugin. threading = "concurrent": fns taking an fs
 *   handle MUST be thread-safe (the host caches one fs handle per mount and shares it across
 *   listing, copy and viewer threads); one iterator/reader/writer is never used by two threads
 *   at once, but may move between threads between calls.
 * - Versioning: CMDR_ABI bumps only on breaking changes. Appending fields/fns (covered by
 *   `size`) is not breaking and does not bump it.
 * - Never let a C++ exception / Rust panic unwind out of these fns.
 */
#ifndef COMMANDER_PLUGIN_H
#define COMMANDER_PLUGIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define CMDR_EXPORT __declspec(dllexport)
#else
#define CMDR_EXPORT __attribute__((visibility("default")))
#endif

#define CMDR_ABI 1u

/* Status codes (int32_t). Unknown values are treated as CMDR_INTERNAL. */
#define CMDR_OK 0
#define CMDR_NOT_FOUND 1
#define CMDR_DENIED 2
#define CMDR_EXISTS 3
#define CMDR_IO 4
#define CMDR_CANCELLED 5
#define CMDR_UNSUPPORTED 6
#define CMDR_INVALID 7
#define CMDR_INTERNAL 8
#define CMDR_AUTH 9      /* credentials missing/rejected */
#define CMDR_NOT_EMPTY 10 /* remove_dir on a non-empty dir */
#define CMDR_AUTH_EXPIRED 11 /* credentials expired / need a login (hosts before it: INTERNAL).
                              * last_error: line 1 = message for the user, then optional lines
                              * "login=<argv, tab-separated>" (the host runs it without a shell,
                              * detached, when the user presses "Log in"; only allow-listed
                              * programs, currently "aws") and "install=<hint>"
                              * (shown when argv[0] is not on PATH). Never a secret. */

/* cmdr_entry.kind */
#define CMDR_FILE 0u
#define CMDR_DIR 1u
#define CMDR_SYMLINK 2u

/* cmdr_entry.attrs bits */
#define CMDR_ATTR_LINK 1u /* the entry is a link (kind = what it points at): the host browses
                           * into a linked dir, but delete removes only the link and
                           * copy/move never recurse through it */

/* write_open flags */
#define CMDR_WRITE_OVERWRITE 1u /* replace an existing file; without it: CMDR_EXISTS */

typedef struct cmdr_str {
    const uint8_t* ptr;
    size_t len;
} cmdr_str;

/* Extensible: list_next/stat get the host's `entry_size` (= array stride). The plugin writes
 * min(sizeof(cmdr_entry), entry_size) bytes per entry and sets `size` to that count. The host
 * needs at least every field before `attrs`; fields past `size` read as defaults. */
typedef struct cmdr_entry {
    uint32_t size;      /* bytes of this entry filled by the plugin */
    uint32_t kind;      /* CMDR_FILE / CMDR_DIR / CMDR_SYMLINK */
    cmdr_str name;      /* plain name: no '/', '\\', ':', NUL, not "." / ".." (stat: ignored) */
    uint64_t file_size; /* bytes, 0 for dirs */
    int64_t mtime_ns;   /* unix ns; INT64_MIN = unknown */
    uint32_t attrs;     /* os-ish bits, 0 = none */
    uint32_t reserved;  /* 0 */
} cmdr_entry;

/* Long-call context (may be NULL). `cancel` points at a byte the host sets to nonzero when the
 * user cancels: poll it between network round trips and return CMDR_CANCELLED. */
typedef struct cmdr_job {
    uint32_t size;
    const volatile uint8_t* cancel; /* may be NULL */
    void* ctx;
    void (*progress)(void* ctx, uint64_t done, uint64_t total); /* may be NULL */
} cmdr_job;

#define CMDR_LOG_ERROR 0
#define CMDR_LOG_WARN 1
#define CMDR_LOG_INFO 2
#define CMDR_LOG_DEBUG 3

/* Host services, valid for the plugin's whole lifetime. */
typedef struct cmdr_host {
    uint32_t size;
    uint32_t abi;
    void* ctx;
    void (*log)(void* ctx, int32_t level, cmdr_str msg);
} cmdr_host;

typedef struct cmdr_vfs {
    uint32_t size;
    /* Required. config = mount string, e.g. "s3://bucket" (scheme://authority). */
    int32_t (*open)(cmdr_str config, void** fs_out);
    void (*close)(void* fs);
    /* Required. Detail of the last failed call on the calling thread ("" if none). */
    cmdr_str (*last_error)(void);
    /* Required. Batch listing: list_next fills up to `cap` entries, *n == 0 = done. */
    int32_t (*list_begin)(void* fs, cmdr_str dir, const cmdr_job* job, void** it_out);
    int32_t (*list_next)(void* it, cmdr_entry* buf, size_t entry_size, size_t cap, size_t* n);
    void (*list_end)(void* it);
    /* Required. CMDR_NOT_FOUND if absent. path "" = root (a dir). */
    int32_t (*stat)(void* fs, cmdr_str path, const cmdr_job* job, cmdr_entry* out,
                    size_t entry_size);
    /* Read stream from byte `offset`; read sets *n = 0 at EOF. */
    int32_t (*read_open)(void* fs, cmdr_str path, uint64_t offset, const cmdr_job* job, void** rd_out);
    int32_t (*read)(void* rd, uint8_t* buf, size_t cap, size_t* n);
    void (*read_close)(void* rd);
    /* Write stream. Parent dir must exist. Nothing is visible at `path` before write_finish
     * returns CMDR_OK; write_finish and write_abort both free `wr`. size_hint: total bytes
     * or UINT64_MAX if unknown. */
    int32_t (*write_open)(void* fs, cmdr_str path, uint32_t flags, uint64_t size_hint,
                          const cmdr_job* job, void** wr_out);
    int32_t (*write)(void* wr, const uint8_t* buf, size_t len);
    int32_t (*write_finish)(void* wr);
    void (*write_abort)(void* wr);
    int32_t (*mkdir)(void* fs, cmdr_str path, const cmdr_job* job); /* CMDR_EXISTS if present */
    int32_t (*remove_file)(void* fs, cmdr_str path, const cmdr_job* job);
    int32_t (*remove_dir)(void* fs, cmdr_str path, const cmdr_job* job); /* empty dirs only */
    /* Rename/move within this fs (file or dir). CMDR_EXISTS if `to` exists: never replaces. */
    int32_t (*rename)(void* fs, cmdr_str from, cmdr_str to, const cmdr_job* job);
    /* append-only below this line */
} cmdr_vfs;

typedef struct cmdr_plugin {
    uint32_t size;
    uint32_t abi; /* must equal CMDR_ABI */
    const cmdr_vfs* vfs; /* NULL = kind absent */
    const void* viewer;   /* reserved, NULL */
    const void* columns;  /* reserved, NULL */
    const void* commands; /* reserved, NULL */
} cmdr_plugin;

/* `host` stays valid until the library is unloaded. */
CMDR_EXPORT const cmdr_plugin* cmdr_plugin_entry(uint32_t host_abi, const cmdr_host* host);

#ifdef __cplusplus
}
#endif
#endif /* COMMANDER_PLUGIN_H */
