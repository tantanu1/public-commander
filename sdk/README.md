# Commander plugin SDK

Everything needed to build a Commander plugin. Guide: [docs/PLUGIN-DEV.md](../docs/PLUGIN-DEV.md).

| | |
|---|---|
| [include/commander_plugin.h](include/commander_plugin.h) | The plugin C ABI, version 1 (MIT). |
| [examples/hello-c](examples/hello-c) | Read-only `hello://` plugin in C, its `plugin.toml`, and `test_host.c`, a small host that loads a plugin and checks it. |
| [examples/hello-rust](examples/hello-rust) | Read-only `hellors://` plugin in Rust (no dependencies): `cargo build --release`. |
| [examples/hello-python](examples/hello-python) | Read-write in-memory `hello://` plugin in Python 3.9+ (standard library only) over the stdio JSON-RPC transport; needs Commander v0.0.19 or newer. Public domain (CC0). |

Build and test the C example:

```sh
cd examples/hello-c
cl /LD /O2 /I ..\..\include hello.c /Fe:hello.dll                 # Windows (MSVC)
cc -shared -fPIC -O2 -I ../../include hello.c -o libhello.so      # Linux
cc -dynamiclib -O2 -I ../../include hello.c -o libhello.dylib     # macOS

cl /I ..\..\include test_host.c && test_host hello.dll hello://   # Windows
cc -I ../../include test_host.c -ldl -o test_host && ./test_host ./libhello.so hello://   # Linux
```

Install: copy `plugin.toml` and the library into `<plugin folder>/hello/`
(Windows `%APPDATA%\commander\plugins`, Linux/macOS `~/.config/commander/plugins`), restart
Commander and type `hello://` in the path bar.

The Rust example installs the same way: `plugin.toml` plus `target/release/hello_rust.dll`
(`libhello_rust.so`, `libhello_rust.dylib`) into `<plugin folder>/hello-rust/`.
