#!/usr/bin/env python3
"""Commander stdio plugin example: a small in-memory folder tree at hello://<name>.

Protocol: JSON-RPC 2.0, one message per line on stdin/stdout (docs/PLUGINS.md, "stdio
JSON-RPC (v1)"). stdout is the protocol only: print logs to stderr (Commander keeps them in
<plugins folder>/.logs/hello-python.log). Python 3.9+, standard library only.
Every mount starts with hello.txt and docs/readme.txt; changes live until the plugin stops.
"""
import base64
import json
import sys
import time
import traceback

# Status codes (same numbers as the C ABI).
NOT_FOUND, EXISTS, UNSUPPORTED, INVALID, NOT_EMPTY, INTERNAL = 1, 3, 6, 7, 10, 8
FILE, DIR = 0, 1
BATCH = 500  # entries per $/entries notification


class Error(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def log(*args):
    print(*args, file=sys.stderr, flush=True)


class Tree:
    """One mount: path -> bytes (file) or None (folder); "" is the root."""

    def __init__(self):
        self.nodes = {"": None, "docs": None}
        self.mtime = {}
        self.save("hello.txt", b"Hello from a Python plugin!\n")
        self.save("docs/readme.txt", b"Edit me with F4, copy me with F5.\n")

    def save(self, path, data):
        self.nodes[path] = bytes(data)
        self.mtime[path] = time.time_ns()

    def get(self, path):
        if path not in self.nodes:
            raise Error(NOT_FOUND, path + ": not found")
        return self.nodes[path]

    def children(self, folder):
        prefix = folder + "/" if folder else ""
        for path in sorted(self.nodes):
            if path and path.startswith(prefix) and "/" not in path[len(prefix):]:
                yield path[len(prefix):], path

    def entry(self, name, path):
        data = self.nodes[path]
        if data is None:
            return [name, DIR, 0, None]
        return [name, FILE, len(data), self.mtime.get(path)]

    def need_parent(self, path):
        parent = path.rpartition("/")[0]
        if self.nodes.get(parent, b"") is not None:
            raise Error(NOT_FOUND, path + ": parent folder missing")


def check(path):
    """A relative '/'-separated path ("" = root) without '.', '..' or empty parts."""
    if not isinstance(path, str):
        raise Error(INVALID, "path missing")
    if path and any(p in ("", ".", "..") for p in path.split("/")):
        raise Error(INVALID, "bad path " + repr(path))
    return path


class Plugin:
    def __init__(self, send):
        self.send = send
        self.mounts = {}  # config -> Tree
        self.handles = {}  # number -> Tree (fs) / [data, pos] (reader) / dict (writer)
        self.next = 0

    def new_handle(self, value):
        self.next += 1
        self.handles[self.next] = value
        return self.next

    def tree(self, p):
        t = self.handles.get(p.get("fs"))
        if not isinstance(t, Tree):
            raise Error(INVALID, "bad fs handle")
        return t

    # --- requests (answered) ---
    def initialize(self, p, _id):
        caps = ["read", "write", "mkdir", "remove_file", "remove_dir", "rename"]
        return {"abi": 1, "kinds": ["vfs"], "caps": {"vfs": caps}}

    def vfs_open(self, p, _id):
        config = p.get("config", "")  # "hello://<name>": one tree per name
        return self.new_handle(self.mounts.setdefault(config, Tree()))

    def vfs_list(self, p, call_id):
        t, folder = self.tree(p), check(p.get("dir"))
        if t.get(folder) is not None:
            raise Error(INVALID, folder + ": not a folder")
        batch = []
        for name, path in t.children(folder):
            batch.append(t.entry(name, path))
            if len(batch) == BATCH:
                self.entries(call_id, batch)
                batch = []
        if batch:
            self.entries(call_id, batch)
        return None

    def entries(self, call_id, batch):
        params = {"id": call_id, "e": batch}
        self.send({"jsonrpc": "2.0", "method": "$/entries", "params": params})

    def vfs_stat(self, p, _id):
        t, path = self.tree(p), check(p.get("path"))
        t.get(path)
        return t.entry("", path)

    def vfs_read_open(self, p, _id):
        t, path = self.tree(p), check(p.get("path"))
        data = t.get(path)
        if data is None:
            raise Error(INVALID, path + ": is a folder")
        return self.new_handle([data, min(p.get("offset", 0), len(data))])

    def vfs_read(self, p, _id):
        r = self.handles.get(p.get("h"))
        if not isinstance(r, list):
            raise Error(INVALID, "bad read handle")
        data, pos = r
        chunk = data[pos:pos + p.get("len", 0)]
        r[1] = pos + len(chunk)
        return base64.b64encode(chunk).decode("ascii")  # "" = end of file

    def vfs_write_open(self, p, _id):
        t, path = self.tree(p), check(p.get("path"))
        if not path:
            raise Error(INVALID, "cannot write the root")
        t.need_parent(path)
        old = t.nodes.get(path, "absent")
        if old is None or (old != "absent" and not p.get("overwrite")):
            raise Error(EXISTS, path + ": already exists")
        return self.new_handle({"tree": t, "path": path, "data": bytearray()})

    def writer(self, p):
        w = self.handles.get(p.get("h"))
        if not isinstance(w, dict):
            raise Error(INVALID, "bad write handle")
        return w

    def vfs_write(self, p, _id):
        self.writer(p)["data"] += base64.b64decode(p.get("data", ""))
        return None

    def vfs_write_finish(self, p, _id):
        w = self.writer(p)
        del self.handles[p["h"]]
        w["tree"].save(w["path"], w["data"])
        return None

    def vfs_mkdir(self, p, _id):
        t, path = self.tree(p), check(p.get("path"))
        if path in t.nodes:
            raise Error(EXISTS, path + ": already exists")
        t.need_parent(path)
        t.nodes[path] = None
        return None

    def vfs_remove_file(self, p, _id):
        t, path = self.tree(p), check(p.get("path"))
        if t.get(path) is None:
            raise Error(INVALID, path + ": is a folder")
        del t.nodes[path]
        return None

    def vfs_remove_dir(self, p, _id):
        t, path = self.tree(p), check(p.get("path"))
        if not path or t.get(path) is not None:
            raise Error(INVALID, path + ": not a folder")
        if any(t.children(path)):
            raise Error(NOT_EMPTY, path + ": not empty")
        del t.nodes[path]
        return None

    def vfs_rename(self, p, _id):
        t, old, new = self.tree(p), check(p.get("from")), check(p.get("to"))
        t.get(old)
        if not old or not new or new.startswith(old + "/"):
            raise Error(INVALID, "cannot move " + repr(old) + " there")
        if new in t.nodes:
            raise Error(EXISTS, new + ": already exists")  # never replace
        t.need_parent(new)
        for path in [x for x in t.nodes if x == old or x.startswith(old + "/")]:
            moved = new + path[len(old):]
            t.nodes[moved] = t.nodes.pop(path)
            if path in t.mtime:
                t.mtime[moved] = t.mtime.pop(path)
        return None

    # --- notifications (no answer) ---
    def notify(self, method, p):
        if method == "vfs.close":
            self.handles.pop(p.get("fs"), None)
        elif method in ("vfs.read_close", "vfs.write_abort"):
            self.handles.pop(p.get("h"), None)
        # "$/cancel": every call here is quick, nothing to stop.


def main():
    out = sys.stdout.buffer

    def send(msg):
        out.write(json.dumps(msg, ensure_ascii=False).encode("utf-8") + b"\n")
        out.flush()

    plugin = Plugin(send)
    log("hello-python started")
    for line in sys.stdin.buffer:  # ends when Commander closes stdin
        try:
            msg = json.loads(line)
        except ValueError:
            msg = None
        if not isinstance(msg, dict):
            log("ignored a line that is not a JSON object")
            continue
        method, params, call_id = msg.get("method", ""), msg.get("params") or {}, msg.get("id")
        if method == "shutdown":  # a notification: just exit
            break
        if call_id is None:
            plugin.notify(method, params)
            continue
        known = method == "initialize" or method.startswith("vfs.")
        handler = getattr(plugin, method.replace(".", "_"), None) if known else None
        try:
            if handler is None:
                raise Error(UNSUPPORTED, method + ": not supported")
            reply = {"result": handler(params, call_id)}
        except Error as e:
            reply = {"error": {"code": e.code, "message": str(e)}}
        except Exception as e:  # a bug here must not kill the plugin
            log(traceback.format_exc())
            reply = {"error": {"code": INTERNAL, "message": repr(e)}}
        send({"jsonrpc": "2.0", "id": call_id, **reply})


if __name__ == "__main__":
    main()
