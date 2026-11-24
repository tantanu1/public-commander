# hello-python: a Commander plugin in Python

Public domain (CC0 1.0): copy freely, use it as the template for your own plugin.

A complete file-system plugin in one Python file (standard library only, Python 3.9+). It shows
an in-memory folder tree at `hello://<name>`: every name gets its own tree with `hello.txt` and
`docs/readme.txt`. You can browse it, view (F3), edit (F4), copy in and out (F5), move and
rename (F6), make folders (F7) and delete (F8). Changes live until the plugin stops (Commander
exits, or the **Unload idle plugins** command, `cm_UnloadPlugins`).

It talks to Commander over **stdio**: JSON-RPC 2.0, one message per line on stdin/stdout. The
full message list is in Commander's plugin guide (`docs/PLUGINS.md`, "stdio JSON-RPC (v1)").
A crash or hang in a stdio plugin never takes Commander down: the call fails with an error and
the plugin is started again on the next use.

## Files

| File | What |
|---|---|
| `plugin.toml` | Manifest: id, `transport = "stdio"`, the command to run, the `hello://` scheme, a drive button |
| `plugin.py` | The plugin |
| `LICENSE` | CC0 1.0: public domain, copy freely (no attribution needed) |

## Install

1. Python 3.9 or newer must be installed. Windows: the python.org installer (it brings the `py`
   launcher). Linux / macOS: `python3` on your PATH.
2. In Commander: **Configuration > Options > Plugins > Install from file...**, type the path of
   this folder (or of a zip holding it), **Install**.
3. Type `hello://demo` in the path bar, or click the **hello** drive button.

How the command is picked: `[entry] command` is the argv for every OS; `windows_command`,
`linux_command` or `macos_command` replaces it on that OS. Here Windows runs `py -3 plugin.py`,
everything else `python3 plugin.py`. No shell is involved; the working folder is the plugin
folder. Without the `py` launcher on Windows, change it to `["python", "plugin.py"]`.

## Debug

- Anything the plugin writes to **stderr** (`log(...)` in `plugin.py`, `print(..., file=sys.stderr)`,
  tracebacks) goes to `<plugins folder>/.logs/hello-python.log`, e.g.
  `%APPDATA%\commander\plugins\.logs\hello-python.log` (Windows) or
  `~/.config/commander/plugins/.logs/hello-python.log`. The last stderr line also shows in the
  error message when the plugin dies.
- **stdout is the protocol only.** A stray `print()` to stdout is not JSON: Commander stops the
  plugin ("wrote a line that is not JSON").
- Run it by hand and type requests:
  ```
  python3 plugin.py
  {"jsonrpc":"2.0","id":1,"method":"initialize","params":{"abi":1}}
  {"jsonrpc":"2.0","id":2,"method":"vfs.open","params":{"config":"hello://demo"}}
  {"jsonrpc":"2.0","id":3,"method":"vfs.list","params":{"fs":1,"dir":""}}
  ```
- Configuration > Options > Plugins shows whether it loaded, or the error (for example
  "cannot start py: program not found").
- After editing `plugin.py`: run **Unload idle plugins** (`cm_UnloadPlugins`), then use
  `hello://` again: a new process starts with your code.

## Make it yours

Change `id`, `name` and `scheme` in `plugin.toml`, rename the folder to the new id, and replace
the `Tree` class with your data source. Leave out capabilities you don't support from
`initialize` (`caps.vfs`): Commander then greys out those commands.
