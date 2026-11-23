# Writing a Commander plugin

A Commander plugin is a shared library (`.dll`, `.so`, `.dylib`) plus a small `plugin.toml`
manifest. The library exports **one C function** that returns a table of function pointers, so you
can write a plugin in any language that can export a C function: C, C++, Rust, Zig, Go (cgo),
C# (NativeAOT), Pascal, Nim, ...

Today plugins provide a **virtual file system** (`vfs`): a tree of folders and files that the user
browses in a panel and works with using the normal keys (F3 view, F5 copy, F6 move/rename, F7 new
folder, F8 delete). The bundled S3, FTP/FTPS, SFTP and 7-Zip plugins are built exactly this way.

What you need, all in this repository:

| | |
|---|---|
| [sdk/include/commander_plugin.h](../sdk/include/commander_plugin.h) | The C ABI, version 1. The source of truth: read its comments. |
| [sdk/examples/hello-c](../sdk/examples/hello-c) | A complete read-only plugin in C (about 200 lines) and a small test host. |
| [sdk/examples/hello-rust](../sdk/examples/hello-rust) | The same idea in Rust, with no dependencies. |

Contents: [Quick start](#quick-start) · [How the host uses a plugin](#how-the-host-uses-a-plugin) ·
[The manifest](#the-manifest-plugintoml) · [The C ABI](#the-c-abi-version-1) ·
[Other languages](#other-languages) · [Testing](#testing) · [Packaging and shipping](#packaging-and-shipping) ·
[Limits](#current-limits)

## Quick start

Build the C example and try it:

```sh
cd sdk/examples/hello-c
# Windows (Developer Command Prompt):
cl /LD /O2 /I ..\..\include hello.c /Fe:hello.dll
# Linux:
cc -shared -fPIC -O2 -I ../../include hello.c -o libhello.so
# macOS:
cc -dynamiclib -O2 -I ../../include hello.c -o libhello.dylib
```

Check it outside the app with the test host (it loads the library, lists the whole tree, stats and
reads every file):

```sh
cl /I ..\..\include test_host.c && test_host hello.dll hello://           # Windows
cc -I ../../include test_host.c -ldl -o test_host && ./test_host ./libhello.so hello://   # Linux
```

Install it: copy the library and `plugin.toml` into a folder named after the plugin id inside your
plugin folder, then restart Commander.

| OS | Your plugin folder |
|---|---|
| Windows | `%APPDATA%\commander\plugins\hello\` |
| Linux, macOS | `~/.config/commander/plugins/hello/` (or `$XDG_CONFIG_HOME/commander/plugins/hello/`) |

**Configuration > Options > Plugins** now lists "Hello (C example)" with `hello://`. Type
`hello://` in the path bar (Ctrl+Shift+P edits it) and press Enter: the panel shows `readme.txt`
and a `greetings` folder. F3 views a file, F5 copies it to the other panel.

## How the host uses a plugin

1. **Discovery.** At startup Commander reads every `plugins/*/plugin.toml` in the plugin folders
   (your folder first, then the one next to the program, then the installer's bundled folder; the
   first plugin with a given id wins). Only the manifest is read: no plugin code runs until the user
   needs it.
2. **Loading.** The first time a path with your scheme is opened, the host loads the library,
   calls `cmdr_plugin_entry(CMDR_ABI, host)` and keeps the returned table. Return `NULL` to refuse a
   host ABI you don't support. On Windows the library's own folder is searched for its DLL
   dependencies, so you can ship them next to it.
3. **Mounting.** For `s3://bucket/photos/2025` the host calls `open("s3://bucket")` (the mount
   string is `scheme://authority`) and then works with paths relative to the mount root:
   `photos/2025`. One opened handle is cached per mount and reused. Going up from the mount root
   leads to `scheme://` (an empty authority, for example a bucket or bookmark list), then stops.
4. **Calls.** Listing, viewing, copying and deleting all run on worker threads, never on the UI
   thread, so a slow network call never freezes the window. Long calls get a `cmdr_job` with a
   cancel flag.
5. **Errors.** Every function returns a status code; the host shows `last_error()` text to the
   user. `CMDR_AUTH` also drops the cached mount, so the next attempt opens a fresh one.

The host protects the user from a misbehaving plugin where it can: entry names containing `/`, `\`,
`:` or NUL, and `.` / `..`, are hidden; the mount root can't be deleted or moved; copy and move
never recurse through entries flagged as links.

**Archive plugins.** A plugin can also open files by extension (like the 7-Zip plugin). Set
`[vfs] extensions` in the manifest. When the user enters such a file, the mount string is
`<scheme>://<encoded path>`, where the archive's file path is encoded as one segment: the native
path separator becomes `|`, and `%`, `|`, `@`, `/`, `\` and control characters become `%XX`
(`C:\x\a.rar` -> `sevenzip://C:|x|a.rar`). Built-in readers keep zip, 7z and tar; your plugin is
asked when they can't read a file. If listing an archive returns `CMDR_AUTH`, the host asks the
user for a password and calls `open` again with `mount + "\n" + password`. The password never
appears in a path.

## The manifest (`plugin.toml`)

```toml
id = "hello"                 # required, [a-z0-9-]{1,32}, unique
name = "Hello (C example)"   # required, shown in Options > Plugins
version = "0.1.0"            # required, your plugin's version
abi = 1                      # required, must match the host's CMDR_ABI
transport = "dylib"          # required, the only transport today
kinds = ["vfs"]              # required
threading = "serial"         # optional: "serial" (default) or "concurrent"

[entry]                      # library file per OS, relative to this folder
windows = "hello.dll"
linux = "libhello.so"
macos = "libhello.dylib"

[vfs]
scheme = "hello"             # mounts hello://...   ([a-z][a-z0-9+.-]+, at least 2 chars)
# extensions = ["rar", "iso"]  # optional: open files with these extensions (archive plugins)
```

From **v0.0.9** unknown manifest keys (top level or in any section) are ignored and listed as a
warning in **Options > Plugins**, so a typo is still visible. Older hosts (v0.0.8 and before) are
strict and reject the whole manifest.

Optional `[drive]` section: puts a drive chip in the drive bar and a row in Alt+F1 / Alt+F2.
```toml
[drive]
label = "s3"       # chip text, 1-16 chars
root = "s3://"     # where a click goes; must start with "<vfs.scheme>://"
```
A plugin that uses `[drive]` needs Commander v0.0.9 or newer.

Keep your own settings in your own config file, not in the manifest. An `[entry]` path may point
into a sibling folder: the bundled FTPS plugin is a manifest-only folder whose entry is
`../ftp/commander_ftp.dll`, so one library serves two schemes.

## The C ABI (version 1)

The header is short and fully commented; this section explains the rules you must follow.

### Entry point and tables

```c
CMDR_EXPORT const cmdr_plugin* cmdr_plugin_entry(uint32_t host_abi, const cmdr_host* host);
```

Return a pointer to a `cmdr_plugin` that lives as long as the library (a `static const` is ideal)
with `abi = CMDR_ABI` and `vfs` pointing to your `cmdr_vfs` table. `viewer`, `columns` and
`commands` are reserved: set them to `NULL`. `host` stays valid until the library is unloaded;
`host->log(host->ctx, CMDR_LOG_INFO, msg)` writes to the host log.

**Every struct starts with `uint32_t size`.** Set it to `sizeof` the struct as you compiled it.
Fields are only ever appended to the end, and the host treats anything past your `size` as absent.
That is how a plugin built against an older header keeps working with a newer host, and why
`CMDR_ABI` changes only on breaking changes.

**A `NULL` function pointer means "not supported".** `open`, `close`, `last_error`, the three
`list_*` functions and `stat` are required. Leave the `read_*` functions `NULL` and files can't be
viewed or copied out. Leave the write functions `NULL` and the plugin is read-only: the host
refuses F5 into it, F7 and F8 there.

### Strings, paths and memory

- Strings are `cmdr_str { ptr, len }`: UTF-8, **not** NUL-terminated.
- Paths inside a mount are `/`-separated and relative to the mount root; `""` is the root. They
  never contain `..`.
- **Host to plugin** inputs are borrowed for the duration of the call only. Copy anything you keep
  (the example copies the folder path into its iterator).
- **Plugin to host** outputs (entry names, `last_error()` text) must stay valid until your next call
  on the same handle (or, for `last_error`, the same thread). The host copies them right away.
- Never free memory across the boundary. Each side frees what it allocated.

### Handles

`open` returns an opaque fs handle, `list_begin` an iterator, `read_open` a reader, `write_open` a
writer. They are your own `void*` (usually a pointer to a struct you allocated). The host always
pairs them: `close`, `list_end`, `read_close`, and `write_finish` or `write_abort` (both free the
writer).

### Errors

Return `CMDR_OK` (0) or a status: `CMDR_NOT_FOUND`, `CMDR_DENIED`, `CMDR_EXISTS`, `CMDR_IO`,
`CMDR_CANCELLED`, `CMDR_UNSUPPORTED`, `CMDR_INVALID`, `CMDR_INTERNAL`, `CMDR_AUTH` (credentials
missing or rejected), `CMDR_NOT_EMPTY`, `CMDR_AUTH_EXPIRED` (11, v0.0.9). Unknown values are
treated as `CMDR_INTERNAL`, so hosts older than v0.0.9 show `CMDR_AUTH_EXPIRED` as a plain error. Put a
human-readable reason where `last_error()` will find it, **per calling thread** (a thread-local
variable). Never put secrets in error text.

`CMDR_AUTH_EXPIRED` means credentials expired or need a login. `last_error()` holds line 1 = the
message for the user, then optional lines `login=<argv, tab-separated>` and `install=<hint>`. The
host then offers **Log in** and **Retry**: Log in runs the argv without a shell, detached, and
shows the exact command first. The host only runs allow-listed programs, currently `aws`; any
other program is refused with a message. `install=` is shown when `argv[0]` is not on PATH.

### Listing and stat

- `list_begin(fs, dir, job, &it)`: `CMDR_NOT_FOUND` if `dir` doesn't exist.
- `list_next(it, buf, entry_size, cap, &n)`: fill up to `cap` entries and set `n`; `n == 0` means
  the listing is complete. `buf` is an array whose stride is the **host's** `entry_size`: write
  `min(sizeof(cmdr_entry), entry_size)` bytes per entry and set each entry's `size` to that
  number. Names must stay valid until the next call on `it`.
- `stat(fs, path, job, &out, entry_size)`: same entry rules (the name is ignored). `""` is the root
  and is a folder. Return `CMDR_NOT_FOUND` for a missing path.
- `cmdr_entry`: `kind` (`CMDR_FILE`, `CMDR_DIR`, `CMDR_SYMLINK`), `name` (a plain name, no `/`),
  `file_size` in bytes, `mtime_ns` in Unix nanoseconds (`INT64_MIN` if unknown) and `attrs`. Set
  `CMDR_ATTR_LINK` for a link and give it the `kind` of its target: the panel opens a linked
  folder, F8 removes only the link, and copy and move never recurse through it.

### Reading and writing

- `read_open(fs, path, offset, job, &rd)` then `read(rd, buf, cap, &n)` until `n == 0` (end of
  file). Support `offset`: Lister uses it.
- `write_open(fs, path, flags, size_hint, job, &wr)`, `write(wr, buf, len)` repeatedly, then
  `write_finish(wr)` or `write_abort(wr)`. The parent folder exists. **Nothing may be visible at
  `path` until `write_finish` returns `CMDR_OK`** (write to a temporary name and rename it at the
  end, for example), so a cancelled or failed copy never leaves a half-written file behind.
  Without `CMDR_WRITE_OVERWRITE` in `flags`, an existing target is `CMDR_EXISTS`. `size_hint` is
  the total size, or `UINT64_MAX` if unknown.
- `mkdir` returns `CMDR_EXISTS` if the folder is there. `remove_dir` removes empty folders only
  (`CMDR_NOT_EMPTY` otherwise): the host deletes the contents first. `rename(fs, from, to)` moves a
  file or folder inside the mount and **never replaces**: `CMDR_EXISTS` if `to` exists.

### Cancel and progress

Long calls receive a `const cmdr_job*` (it may be `NULL`). `job->cancel` points to a byte the host
sets to nonzero when the user presses Cancel: check it between network round trips and return
`CMDR_CANCELLED`. `job->progress` may be `NULL`. A job passed to a `*_begin` / `*_open` function
stays valid until the matching end, close, finish or abort.

### Threading

Calls never come from the UI thread. With `threading = "serial"` (the default) the host makes one
call into your plugin at a time, so you need no locking. With `threading = "concurrent"` every
function that takes the fs handle **must be thread-safe**: the host shares one handle per mount
between the listing, copy and viewer threads. An iterator, reader or writer is never used by two
threads at once, but may move between threads between calls. Start with `serial`; switch to
`concurrent` when your plugin is ready and parallel transfers matter.

If your plugin starts its own threads (an async runtime, a connection pool), pin the library so it
can never be unloaded under them: `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, ...)`
on Windows, `dlopen(your_path, RTLD_NOW | RTLD_NODELETE)` on Linux and macOS.

### Never unwind across the boundary

A C++ exception or Rust panic must not escape an exported function: catch it and return
`CMDR_INTERNAL`. (In Rust, a panic inside an `extern "C"` function aborts the whole program, so
the user loses Commander.)

## Other languages

- **C++:** include the header as-is (it has `extern "C"`), catch all exceptions at the boundary.
- **Rust:** see [sdk/examples/hello-rust](../sdk/examples/hello-rust). Build a `cdylib`, mirror the
  structs with `#[repr(C)]` (or generate them with `bindgen` from the header), export
  `#[unsafe(no_mangle)] pub extern "C" fn cmdr_plugin_entry`, and use `Option<unsafe extern "C" fn ...>`
  for nullable function pointers. Setting `panic = "abort"` makes the "no unwinding" rule explicit.
- **Zig:** `@cImport` the header and `export fn cmdr_plugin_entry`.
- **Go:** build with `-buildmode=c-shared`, include the header via cgo and `//export` the entry
  point. Keep Go pointers out of what you hand to the host (allocate returned names with `C.malloc`
  or keep them in C memory).
- **C#:** NativeAOT with `[UnmanagedCallersOnly(EntryPoint = "cmdr_plugin_entry")]`.

Whatever the language: `size` fields first, `NULL` for unsupported functions, UTF-8 `{ptr,len}`
strings, and the library file names must match the manifest's `[entry]`.

## Testing

**1. Outside the app.** [test_host.c](../sdk/examples/hello-c/test_host.c) loads your library like
the host does: it checks that a future ABI is refused, opens the mount you pass, stats the root and
a missing path, then lists the whole tree and reads the first bytes of every file. Extend it with
the checks that matter for your plugin. Commander's own plugins are tested this way: one shared
conformance suite runs the same scenarios against every plugin. Cover at least:

- list the root, a subfolder, an empty folder and a missing folder (`CMDR_NOT_FOUND`);
- `stat` of `""` (a folder), of files and folders, and of a missing path;
- reads from offset 0 and from the middle of a file, reading exactly to end of file;
- writes: the file is invisible before `write_finish`, `write_abort` leaves nothing,
  `CMDR_EXISTS` without the overwrite flag, replacement with it;
- `mkdir` on an existing folder, `remove_dir` on a non-empty folder, `rename` onto an existing name;
- names with spaces, non-ASCII (UTF-8) characters and long paths;
- an `entry_size` smaller and larger than your `sizeof(cmdr_entry)` (older and newer hosts);
- cancel: set the job's cancel byte during a long call and expect `CMDR_CANCELLED`;
- with `threading = "concurrent"`: many threads listing and reading through one fs handle.

**2. In the app.** Install the plugin (see [Quick start](#quick-start)) and check
**Options > Plugins**: it shows every plugin found and any problem with a manifest (bad key, wrong
ABI, invalid id or scheme). Then type `yourscheme://` in the path bar and try F3, F5 both ways, F6,
F7 and F8. Plugins are found at startup: restart Commander after changing the manifest or library.

Remember that a plugin runs inside the Commander process with the user's rights: a crash in the
plugin is a crash of the app. Run your plugin under AddressSanitizer or Valgrind in the test host
before shipping.

## Packaging and shipping

A plugin is shipped as its folder: `plugin.toml` plus the library for one OS and CPU.

```
hello/
  plugin.toml
  hello.dll          (Windows)  or  libhello.so (Linux)  or  libhello.dylib (macOS)
```

- Build one library per target you support: Windows x64 and ARM64, Linux x64, macOS Apple
  Silicon and Intel. Name your downloads after the target, as the official releases do
  (`hello-0.1.0-x86_64-pc-windows-msvc.zip`).
- Users unzip the folder into their plugin folder
  ([INSTALL-PLUGINS.md](INSTALL-PLUGINS.md) explains where) and restart Commander.
- The manifest's `abi` must match the user's Commander. Bump `version` in `plugin.toml` with every
  release so users can see which one they have in Options > Plugins.
- Linux: build on an old distribution (or with the oldest glibc you support) so the library loads
  on newer ones. macOS: libraries downloaded with a browser get quarantined; tell users to run
  `xattr -dr com.apple.quarantine <plugin folder>`, or sign and notarize the library.
- Keep secrets out of the manifest and out of URLs. Store credentials the way the bundled plugins
  do: in a separate config file next to Commander's (`ftp.toml`, `sftp.toml`) or reuse an existing
  standard (`~/.aws/credentials`, ssh-agent).

## Current limits

- Only the `vfs` kind and the `dylib` transport exist today. Viewer, custom column and command
  plugins are reserved in the ABI (`viewer`, `columns`, `commands`) for later versions.
- There is no way yet for a plugin to ask the user for a secret, except the archive password
  prompt described above.
- Plugins are not sandboxed: they run with the user's rights, like Total Commander plugins. Only
  install plugins you trust.
