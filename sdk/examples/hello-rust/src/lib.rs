//! hello-rust: a read-only Commander vfs plugin in plain Rust (no dependencies).
//! Mount: type `hellors://` in the path bar; it holds one file, `hi.txt`.
//! The ABI structs below mirror sdk/include/commander_plugin.h field by field.
use std::ffi::c_void;
use std::ptr;

// ---- ABI v1 types, mirrored from sdk/include/commander_plugin.h ----
const CMDR_ABI: u32 = 1;
const OK: i32 = 0;
const NOT_FOUND: i32 = 1;
const FILE: u32 = 0;
const DIR: u32 = 1;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    ptr: *const u8,
    len: usize,
}

impl Str {
    const fn of(s: &'static str) -> Self {
        Str {
            ptr: s.as_ptr(),
            len: s.len(),
        }
    }
    unsafe fn bytes<'a>(self) -> &'a [u8] {
        if self.len == 0 {
            &[]
        } else {
            unsafe { std::slice::from_raw_parts(self.ptr, self.len) }
        }
    }
}

#[repr(C)]
pub struct Entry {
    size: u32,
    kind: u32,
    name: Str,
    file_size: u64,
    mtime_ns: i64,
    attrs: u32,
    reserved: u32,
}

#[repr(C)]
pub struct Job {
    size: u32,
    cancel: *const u8,
    ctx: *mut c_void,
    progress: Option<unsafe extern "C" fn(*mut c_void, u64, u64)>,
}

#[repr(C)]
pub struct Host {
    size: u32,
    abi: u32,
    ctx: *mut c_void,
    log: Option<unsafe extern "C" fn(*mut c_void, i32, Str)>,
}

type H = *mut c_void;
type F<T> = Option<T>;

#[repr(C)]
pub struct Vfs {
    size: u32,
    open: F<unsafe extern "C" fn(Str, *mut H) -> i32>,
    close: F<unsafe extern "C" fn(H)>,
    last_error: F<unsafe extern "C" fn() -> Str>,
    list_begin: F<unsafe extern "C" fn(H, Str, *const Job, *mut H) -> i32>,
    list_next: F<unsafe extern "C" fn(H, *mut Entry, usize, usize, *mut usize) -> i32>,
    list_end: F<unsafe extern "C" fn(H)>,
    stat: F<unsafe extern "C" fn(H, Str, *const Job, *mut Entry, usize) -> i32>,
    read_open: F<unsafe extern "C" fn(H, Str, u64, *const Job, *mut H) -> i32>,
    read: F<unsafe extern "C" fn(H, *mut u8, usize, *mut usize) -> i32>,
    read_close: F<unsafe extern "C" fn(H)>,
    write_open: F<unsafe extern "C" fn(H, Str, u32, u64, *const Job, *mut H) -> i32>,
    write: F<unsafe extern "C" fn(H, *const u8, usize) -> i32>,
    write_finish: F<unsafe extern "C" fn(H) -> i32>,
    write_abort: F<unsafe extern "C" fn(H)>,
    mkdir: F<unsafe extern "C" fn(H, Str, *const Job) -> i32>,
    remove_file: F<unsafe extern "C" fn(H, Str, *const Job) -> i32>,
    remove_dir: F<unsafe extern "C" fn(H, Str, *const Job) -> i32>,
    rename: F<unsafe extern "C" fn(H, Str, Str, *const Job) -> i32>,
}

#[repr(C)]
pub struct Plugin {
    size: u32,
    abi: u32,
    vfs: *const Vfs,
    viewer: *const c_void,
    columns: *const c_void,
    commands: *const c_void,
}
unsafe impl Sync for Plugin {}
unsafe impl Sync for Vfs {}

// ---- the plugin ----
const NAME: &str = "hi.txt";
const TEXT: &str = "Hello from Rust!\n";

/// Writes at most `entry_size` bytes (the host's struct size) into `out`.
unsafe fn put(out: *mut Entry, entry_size: usize, kind: u32, name: &'static str, len: u64) {
    let n = entry_size.min(size_of::<Entry>());
    let e = Entry {
        size: n as u32,
        kind,
        name: Str::of(name),
        file_size: len,
        mtime_ns: i64::MIN,
        attrs: 0,
        reserved: 0,
    };
    unsafe { ptr::copy_nonoverlapping(&e as *const Entry as *const u8, out as *mut u8, n) };
}

unsafe extern "C" fn open(_cfg: Str, fs: *mut H) -> i32 {
    unsafe { *fs = 1 as H };
    OK
}
unsafe extern "C" fn close(_fs: H) {}
unsafe extern "C" fn last_error() -> Str {
    Str::of("not found")
}

unsafe extern "C" fn list_begin(_fs: H, dir: Str, _j: *const Job, it: *mut H) -> i32 {
    if dir.len != 0 {
        return NOT_FOUND;
    }
    unsafe { *it = Box::into_raw(Box::new(false)) as H }; // "already listed?"
    OK
}
unsafe extern "C" fn list_next(
    it: H,
    buf: *mut Entry,
    es: usize,
    cap: usize,
    n: *mut usize,
) -> i32 {
    let done = unsafe { &mut *(it as *mut bool) };
    unsafe { *n = 0 };
    if !*done && cap > 0 {
        unsafe {
            put(buf, es, FILE, NAME, TEXT.len() as u64);
            *n = 1
        };
        *done = true;
    }
    OK
}
unsafe extern "C" fn list_end(it: H) {
    drop(unsafe { Box::from_raw(it as *mut bool) })
}

unsafe extern "C" fn stat(_fs: H, path: Str, _j: *const Job, out: *mut Entry, es: usize) -> i32 {
    match unsafe { path.bytes() } {
        b"" => unsafe { put(out, es, DIR, "", 0) },
        p if p == NAME.as_bytes() => unsafe { put(out, es, FILE, "", TEXT.len() as u64) },
        _ => return NOT_FOUND,
    }
    OK
}

unsafe extern "C" fn read_open(_fs: H, path: Str, off: u64, _j: *const Job, rd: *mut H) -> i32 {
    if unsafe { path.bytes() } != NAME.as_bytes() {
        return NOT_FOUND;
    }
    let pos = (off as usize).min(TEXT.len());
    unsafe { *rd = Box::into_raw(Box::new(pos)) as H };
    OK
}
unsafe extern "C" fn read(rd: H, buf: *mut u8, cap: usize, n: *mut usize) -> i32 {
    let pos = unsafe { &mut *(rd as *mut usize) };
    let chunk = &TEXT.as_bytes()[*pos..][..cap.min(TEXT.len() - *pos)];
    unsafe {
        ptr::copy_nonoverlapping(chunk.as_ptr(), buf, chunk.len());
        *n = chunk.len()
    };
    *pos += chunk.len();
    OK
}
unsafe extern "C" fn read_close(rd: H) {
    drop(unsafe { Box::from_raw(rd as *mut usize) })
}

static VFS: Vfs = Vfs {
    size: size_of::<Vfs>() as u32,
    open: Some(open),
    close: Some(close),
    last_error: Some(last_error),
    list_begin: Some(list_begin),
    list_next: Some(list_next),
    list_end: Some(list_end),
    stat: Some(stat),
    read_open: Some(read_open),
    read: Some(read),
    read_close: Some(read_close),
    write_open: None,
    write: None,
    write_finish: None,
    write_abort: None,
    mkdir: None,
    remove_file: None,
    remove_dir: None,
    rename: None, // read-only
};

static PLUGIN: Plugin = Plugin {
    size: size_of::<Plugin>() as u32,
    abi: CMDR_ABI,
    vfs: &VFS,
    viewer: ptr::null(),
    columns: ptr::null(),
    commands: ptr::null(),
};

#[unsafe(no_mangle)]
pub extern "C" fn cmdr_plugin_entry(host_abi: u32, _host: *const Host) -> *const Plugin {
    if host_abi == CMDR_ABI {
        &PLUGIN
    } else {
        ptr::null()
    }
}
