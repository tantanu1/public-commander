/* test_host: a tiny stand-in for Commander that loads a vfs plugin and exercises it.
 * Use it as a starting point for your own checks before trying the plugin in the app.
 *
 *   Windows (MSVC):  cl /I ..\..\include test_host.c && test_host hello.dll hello://
 *   Linux:           cc -I ../../include test_host.c -ldl -o test_host && ./test_host ./libhello.so hello://
 *   macOS:           cc -I ../../include test_host.c -o test_host && ./test_host ./libhello.dylib hello://
 *
 * It prints the whole tree (list + stat) and the first bytes of every file (read).
 */
#include <stdio.h>
#include <string.h>

#include "commander_plugin.h"

#if defined(_WIN32)
#include <windows.h>
static void* load(const char* p) { return (void*)LoadLibraryA(p); }
static void* sym(void* lib, const char* n) { return (void*)GetProcAddress((HMODULE)lib, n); }
#else
#include <dlfcn.h>
static void* load(const char* p) { return dlopen(p, RTLD_NOW); }
static void* sym(void* lib, const char* n) { return dlsym(lib, n); }
#endif

typedef const cmdr_plugin* (*entry_fn)(uint32_t, const cmdr_host*);

static const cmdr_vfs* V;
static int failures;

static cmdr_str S(const char* s) {
    cmdr_str r = {(const uint8_t*)s, strlen(s)};
    return r;
}

static void host_log(void* ctx, int32_t level, cmdr_str msg) {
    (void)ctx;
    fprintf(stderr, "[plugin log %d] %.*s\n", level, (int)msg.len, (const char*)msg.ptr);
}

static int check(int32_t st, const char* what) {
    if (st != CMDR_OK) {
        cmdr_str e = V->last_error();
        printf("FAIL %s: status %d (%.*s)\n", what, st, (int)e.len, (const char*)e.ptr);
        failures++;
    }
    return st == CMDR_OK;
}

static void dump_file(void* fs, const char* path) {
    void* rd;
    if (!V->read_open || !check(V->read_open(fs, S(path), 0, NULL, &rd), "read_open")) return;
    uint8_t buf[64];
    size_t n;
    if (check(V->read(rd, buf, sizeof buf, &n), "read")) {
        const uint8_t* nl = memchr(buf, '\n', n); /* show the first line only */
        size_t len = nl ? (size_t)(nl - buf) : n;
        printf("      first line: \"%.*s\"\n", (int)len, (const char*)buf);
    }
    V->read_close(rd);
}

static void walk(void* fs, const char* dir, int depth) {
    void* it;
    if (!check(V->list_begin(fs, S(dir), NULL, &it), "list_begin")) return;
    cmdr_entry batch[8];
    size_t n;
    do {
        if (!check(V->list_next(it, batch, sizeof batch[0], 8, &n), "list_next")) break;
        for (size_t i = 0; i < n; i++) {
            /* copy the name now: it is only valid until the next call on the iterator */
            char name[512], path[1024];
            snprintf(name, sizeof name, "%.*s", (int)batch[i].name.len, (const char*)batch[i].name.ptr);
            snprintf(path, sizeof path, "%s%s%s", dir, *dir ? "/" : "", name);
            printf("%*s%s%s  (%llu bytes)\n", depth * 2, "", name,
                   batch[i].kind == CMDR_DIR ? "/" : "", (unsigned long long)batch[i].file_size);
            cmdr_entry st;
            check(V->stat(fs, S(path), NULL, &st, sizeof st), "stat");
            if (batch[i].kind == CMDR_DIR) walk(fs, path, depth + 1);
            else dump_file(fs, path);
        }
    } while (n > 0);
    V->list_end(it);
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: test_host <plugin library> <mount, e.g. hello://>\n");
        return 2;
    }
    void* lib = load(argv[1]);
    entry_fn entry = lib ? (entry_fn)sym(lib, "cmdr_plugin_entry") : NULL;
    if (!entry) {
        fprintf(stderr, "cannot load %s or find cmdr_plugin_entry\n", argv[1]);
        return 1;
    }
    cmdr_host host = {sizeof host, CMDR_ABI, NULL, host_log};
    if (entry(CMDR_ABI + 1, &host) != NULL) printf("WARN plugin accepted a future ABI\n");
    const cmdr_plugin* p = entry(CMDR_ABI, &host);
    if (!p || p->abi != CMDR_ABI || !p->vfs) {
        fprintf(stderr, "plugin refused ABI %u or has no vfs\n", CMDR_ABI);
        return 1;
    }
    V = p->vfs;
    void* fs;
    if (!check(V->open(S(argv[2]), &fs), "open")) return 1;

    cmdr_entry root;
    if (check(V->stat(fs, S(""), NULL, &root, sizeof root), "stat root") && root.kind != CMDR_DIR) {
        printf("FAIL root is not a dir\n");
        failures++;
    }
    cmdr_entry missing;
    if (V->stat(fs, S("no/such/file"), NULL, &missing, sizeof missing) != CMDR_NOT_FOUND) {
        printf("FAIL stat of a missing path must return CMDR_NOT_FOUND\n");
        failures++;
    }
    printf("%s\n", argv[2]);
    walk(fs, "", 1);
    V->close(fs);
    printf(failures ? "%d failure(s)\n" : "all checks passed\n", failures);
    return failures != 0;
}
