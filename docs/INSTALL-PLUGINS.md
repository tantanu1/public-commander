# Installing plugins (S3, FTP/FTPS, SFTP, SMB, 7-Zip, VMware VMs)

Commander has these plugins: **S3** (`s3://`, Amazon S3 and S3-compatible storage),
**FTP** (`ftp://`), **FTPS** (`ftps://`, explicit TLS), **SFTP** (`sftp://`), **SMB** (`smb://`,
Windows shares), **VMware VMs** (`vmware://`, files inside running VMware VMs) and **7-Zip**
(`sevenzip`: RAR/ISO/CAB/... and encrypted archives; needs 7-Zip itself installed: 7-zip.org,
or `7zz` / `7z` on PATH). Developer guide (writing your own):
[PLUGIN-DEV.md](PLUGIN-DEV.md).

The installers (`.msi`, `.deb`, `.rpm`, `.AppImage`, `.dmg`) contain **only the app**. Plugins are a
separate download from the open-source plugins repo:
[github.com/tantanu1/commander-plugins/releases](https://github.com/tantanu1/commander-plugins/releases).
Each release has, per platform (target, e.g. `x86_64-pc-windows-msvc`, `aarch64-apple-darwin`,
`x86_64-unknown-linux-gnu`):

- one zip per plugin: `<id>-<plugin version>-<target>.zip`, e.g. `sftp-0.2.0-x86_64-pc-windows-msvc.zip`
  (`ftps` also holds `ftp`, whose library it uses);
- `commander-plugins-<version>-<target>.zip`: all plugins.

A plugins release `vX.Y.Z` is built with Commander vX.Y.Z: use the one with your Commander's
version (or an older one). Up to Commander v0.0.32 the plugin zip was on the Commander release
page instead.

> **Upgrading from v0.0.6 or v0.0.7?** Those installers included the plugins; the new installer
> removes them. Drop the plugin zip on Commander (below) to get them back.

## How to use them

| Plugin | How to open it |
|---|---|
| S3 | The `s3` drive (Alt+F1 / Alt+F2): profiles, then buckets; or type `s3://` in the path bar |
| FTP / FTPS | **Ctrl+F** (Net > FTP Connect); bookmarks in `ftp.toml` |
| SFTP | **Ctrl+F** or type `sftp://host/path`; bookmarks in `sftp.toml` |
| SMB | **Ctrl+F** or type `smb://host/share/path`; bookmarks in `smb.toml` |
| VMware VMs | Type `vmware://` (running VMs); the guest login is asked when you open one |
| 7-Zip | Just open a `.rar`, `.iso`, `.cab`, ... or encrypted archive (Enter); uses your installed 7-Zip |

**Configuration > Options > Plugins** shows which plugins loaded.

## Install: drop the zip on Commander

1. Download a plugin zip (one plugin, or `commander-plugins-<version>-<target>.zip` for all) for
   **your Commander's version and platform** from the
   [plugin releases](https://github.com/tantanu1/commander-plugins/releases).
2. Drag the zip from Explorer / Finder / your file manager onto a Commander panel (or drag it from
   one panel to the other, or use **Configuration > Options > Plugins > Install from file...** (a zip or a plugin folder) and
   type its path). Commander lists the plugins inside, all checked: untick what you don't want,
   press **Install** (Enter). Esc cancels; **Copy instead** copies the zip like any file. A zip
   without plugins, or one dropped into an archive / FTP / S3 panel, is just copied.
3. Done: the plugins work at once, no restart.

Commander checks the zip first and refuses it with a message if a plugin is for another ABI
version or platform, its library is missing, or the zip holds unsafe names (`..`, absolute
paths), symbolic links or more than 512 MiB. Plugins go into your user plugin folder (below);
plugins you install this way survive app updates.

**Upgrades**: dropping a newer plugin zip asks "Upgrade <id> <old> -> <new>". A plugin that is in
use (a panel showed `sftp://...`, an archive was opened with it, ...) can't be replaced while
Commander runs: the new version waits in `plugins/.staged/` and is put in place when Commander
restarts (it tells you so).

**Uninstall**: **Configuration > Options > Plugins**, **Uninstall** on its row. It deletes the
plugin's folder in your user plugin folder (a plugin in use: at the next start). Plugins in the
program's folder are shown as built in and can't be uninstalled there.

## Install by hand: unzip into your plugin folder

1. Download a plugin zip for **your Commander's version and platform** from the
   [plugin releases](https://github.com/tantanu1/commander-plugins/releases) (above).
2. Unzip it into your user plugin folder, so you get `plugins/<id>/plugin.toml` (all plugins:
   `plugins/s3/...`, `plugins/ftp/...`, `plugins/ftps/...`, `plugins/sftp/...`, `plugins/smb/...`,
   `plugins/sevenzip/...`, `plugins/vmware-vms-disk/...`):
   - Windows: `%APPDATA%\commander\plugins` (`C:\Users\<you>\AppData\Roaming\commander\plugins`).
     PowerShell: `Expand-Archive <zip> "$env:APPDATA\commander\plugins" -Force`
   - Linux / macOS: `~/.config/commander/plugins` (`$XDG_CONFIG_HOME/commander/plugins` if set).
     `mkdir -p ~/.config/commander/plugins && unzip -o <zip> -d ~/.config/commander/plugins`
   - macOS may quarantine downloaded libraries: `xattr -dr com.apple.quarantine ~/.config/commander/plugins`
3. Restart Commander.

This folder survives app updates; after updating Commander, drop (or unzip) the plugin zips of
the matching plugins release.

Each plugin is one folder holding `plugin.toml` + its library (`.dll` / `.so` / `.dylib`).
`ftps` holds only a `plugin.toml`: it uses the library in `../ftp`, so keep both (the ftps zip
contains both). The zips also hold this guide (`INSTALL-PLUGINS.md`), `LICENSE.txt` (MIT) and
`THIRD-PARTY-NOTICES.txt`, and each plugin folder its `README.md`.

Other folders searched after the user folder (the first plugin with a given id wins), e.g. for a
portable setup: `<folder of the commander program>/plugins`; Linux also
`<folder>/../lib/commander/plugins`; macOS also `Commander.app/Contents/Resources/plugins`.

## Script plugins (Python and other languages)

Some plugins are programs (for example a `plugin.py`) instead of a library: their `plugin.toml`
says `transport = "stdio"` and which command runs them (`[entry] command`, with per-OS
`windows_command` / `linux_command` / `macos_command`). Install them the same way (drop the zip,
or **Install from file...** with the zip or the plugin folder). The program they need (e.g.
Python: `py` on Windows, `python3` elsewhere) must be installed; Commander never installs it.
Options > Plugins shows them with `(stdio)` after the version, and "cannot start ..." if the
program is missing. A crash or hang of such a plugin only fails that operation (after 30 s
without an answer for a hang); the next use starts it again. Its error output is kept in
`<plugins folder>/.logs/<id>.log`. Example: `sdk/examples/hello-python`.

## Check that a plugin loaded

**Configuration > Options > Plugins** lists every plugin found (name, version, folder, status:
found / loaded / error, pending upgrade or removal), the folders searched and any problems (bad
`plugin.toml`, wrong ABI). Plugins unzipped by hand are found at startup: restart after that
(dropped zips need no restart). Quick test: type `s3://` in the path bar; without the
plugin it is just an unknown path. Ctrl+F says "FTP plugin not installed" when ftp is missing.

## S3 (Amazon S3, MinIO, Cloudflare R2, SeaweedFS, ...)

1. Open it from the **`s3` drive** (drive bar, or Alt+F1 / Alt+F2): it lists your AWS profiles
   (`~/.aws/config` and `~/.aws/credentials`), open a profile to see its buckets. You can also
   type `s3://<profile>/`, `s3://<profile>/<bucket>/folder` or the older `s3://bucket/folder`
   in the path bar. F3 view, F5 copy, F6 move/rename, F7 new folder, F8 delete (always
   permanent) work as on disk. F6 between two profiles is not supported: copy instead.
2. Credentials per profile: env `AWS_ACCESS_KEY_ID` + `AWS_SECRET_ACCESS_KEY` (+
   `AWS_SESSION_TOKEN`) for the default profile; else keys in the profile; else SSO / `aws login`
   / `credential_process` / role profiles, which need the **AWS CLI v2** on PATH. Region:
   `AWS_REGION`, `AWS_DEFAULT_REGION`, profile `region`, else `us-east-1`; buckets in other
   regions are followed automatically.
3. **Expired credentials**: Commander shows "Credentials expired" and the exact command it will
   run (e.g. `aws sso login --profile work`). **Log in** (Enter) runs it for you (the browser
   opens), then **Retry** (R). Only `aws` is ever run this way. Profiles with static keys have no
   login button: renew the keys.
4. S3-compatible servers: set the endpoint with env `AWS_ENDPOINT_URL` (or `AWS_ENDPOINT_URL_S3`,
   or `endpoint_url = http://localhost:9000` in the profile). Path-style addressing is then the
   default; override with env `COMMANDER_S3_PATH_STYLE=0|1`.
5. Downloads (F5 out of S3, F3 view, quick view): objects bigger than one chunk are fetched in
   parallel ranged GETs and delivered in order. Tune in `s3.toml` next to `config.toml`
   (`%APPDATA%\commander\s3.toml` on Windows, `~/.config/commander/s3.toml` elsewhere; env
   `COMMANDER_S3_CONFIG` overrides the path). Bad or missing values keep the defaults.
```toml
download_parallel = 8    # ranged GETs at once per file, 1..32 (1 = one plain GET)
download_chunk_mib = 8   # chunk size in MiB, 1..256; memory per file ~ (parallel + 1) x chunk
                         # (parallel is lowered so that stays <= 1 GiB)
```

Credentials are read once per connection: restart Commander after changing them.
Example `~/.aws/credentials` (Windows: `%USERPROFILE%\.aws\credentials`):
```ini
[default]
aws_access_key_id = AKIA...
aws_secret_access_key = ...
```

## FTP / FTPS

1. Press **Ctrl+F** (Net > FTP Connect): pick a bookmark or type `ftp://[user@]host[:port]/path`
   or `ftps://...`. Without a user you log in anonymously.
2. Passwords go **only** in `ftp.toml`, never in the URL (a `user:pass@` URL is refused, so a
   password never ends up in history, tabs or config). `ftp.toml` sits next to `config.toml`:
   `%APPDATA%\commander\ftp.toml` (Windows), `~/.config/commander/ftp.toml` (Linux, macOS);
   env `COMMANDER_FTP_CONFIG` overrides the path.
```toml
[work]              # open with ftp://work (or pick "work" in Ctrl+F)
host = "ftp.example.com"
port = 21           # optional
user = "me"         # optional, none = anonymous
password = "secret" # optional
tls = true          # optional: explicit FTPS (AUTH TLS)
```
`ftp://` lists the bookmarks. Passive mode only; FTPS certificates must be valid (self-signed
servers fail); no implicit FTPS (port 990).

## SFTP

1. Press **Ctrl+F** (the same dialog lists FTP and SFTP bookmarks) or type
   `sftp://[user@]host[:port]/path` or `sftp://<bookmark>`. Without a user, your local user name
   is used. `sftp://` lists the bookmarks.
2. Bookmarks live in `sftp.toml` next to `config.toml` (env `COMMANDER_SFTP_CONFIG` overrides the
   path). As with FTP, a password in the URL is refused. The easy way: in the Ctrl+F dialog press
   **F7** (new SFTP bookmark) or select one and press **F4** (edit): name, host, port, user,
   private key file and an optional password, which is written only when **Save password** is
   ticked. The other bookmarks in the file stay as they are. A saved password is stored in
   **plain text** in `sftp.toml` (owner-only 0600 on Linux/macOS when Commander creates the file;
   on Windows only your user profile's permissions protect it): prefer a key file. Or by hand:
```toml
[build]                         # open with sftp://build
host = "build.example.net"
port = 22                       # optional
user = "me"                     # optional
identity = "~/.ssh/id_ed25519"  # optional key file
password = "secret"             # optional, tried last
```
3. Your `~/.ssh/config` works too: `sftp://myalias/path` uses that `Host` block's `HostName`,
   `User`, `Port`, `IdentityFile` and `IdentitiesOnly` (wildcards like `Host *.example.com`
   included). A user or port in the URL, and the fields of a bookmark, win. Nothing in the file
   is ever run: `Match`, `Include`, `ProxyCommand` and `ProxyJump` are ignored.
4. Sign-in order: the key files (the bookmark's `identity`, then `IdentityFile`s), then your SSH
   agent (Windows: OpenSSH agent, then Pageant; Linux/macOS: `SSH_AUTH_SOCK`; skipped with
   `IdentitiesOnly yes`); without a key file: the agent, then `~/.ssh/id_ed25519`, `id_ecdsa`,
   `id_rsa`; then the bookmark password.
5. Passphrase-protected key: when nothing else signed you in, Commander asks for the key's
   passphrase (masked; Esc cancels) and connects again. A wrong one asks again. The passphrase
   stays in memory only until you quit; it is never written to disk, logs or settings.
6. Host keys are checked against `~/.ssh/known_hosts` and never trusted automatically. An unknown
   host is refused with its SHA256 fingerprint and the exact line to add; a changed key is refused
   with "HOST KEY CHANGED". Connect once with `ssh` to record a new host.

## SMB (Windows shares, Samba, NAS)

On Windows, `\\server\share` paths already work in any panel (Windows' own client, your
Windows sign-in). The SMB plugin is mainly for **Linux and macOS**, and on Windows for shares you
want to open with other credentials from a bookmark.

1. Press **Ctrl+F** (the same dialog lists SMB bookmarks) or type `smb://[user@]host[:port]/share/path`.
   `smb://host` lists the server's shares (no hidden `$` admin shares: type `smb://host/C$`);
   `smb://` lists the bookmarks. A domain account: `smb://CORP%5Cme@host` (`DOMAIN\user`) or
   `smb://me%40corp.example@host` (`user@domain`), or the bookmark's `domain` field.
2. Without a user, Commander signs in as guest (if the server allows it). With a user and no
   saved password, Commander asks for the password (masked; Esc cancels); a wrong one asks again.
   The typed password stays in memory only until you quit; it is never written to disk, logs or
   settings. A password in the URL is refused.
3. Bookmarks live in `smb.toml` next to `config.toml` (env `COMMANDER_SMB_CONFIG` overrides the
   path). In the Ctrl+F dialog press **F7** (new bookmark, type SMB) or select one and press
   **F4** (edit): name, host, port, share, domain, user, **Require encryption** and an optional password, written only
   when **Save password** is ticked. A saved password is stored in **plain text** in `smb.toml`
   (owner-only 0600 on Linux/macOS when Commander creates the file; on Windows only your user
   profile's permissions protect it): prefer being asked. Or by hand:
```toml
[nas]                           # open with smb://nas
host = "nas.local"
port = 445                      # optional
share = "media"                 # optional: smb://nas is this share (else the share list)
user = "me"                     # optional (empty = guest)
domain = "CORP"                 # optional
password = "secret"             # optional, plain text
encrypt = true                  # optional: refuse unencrypted sessions (SMB 3)
```
Without `encrypt = true` (**Require encryption** in the form), a server that only *prefers*
encryption can be talked down to plain text by an attacker on the network path; signing still
protects the integrity of signed-in sessions, but guest sessions are neither signed nor encrypted.
4. SMB 2.0.2 to 3.1.1 with NTLMv2 sign-in, message signing and encryption when the server asks
   for them. No Kerberos / single sign-on and no SMB1. F3 / F4 / F5 / F6 / F7 / F8 work as on
   disk, except a move (F6) between two shares of one server, which is refused: copy (F5), then
   delete.

## VMware VMs (files inside running VMs)

Browse and change the files of **running** VMware Workstation / Player / Fusion VMs through
VMware Tools in the guest (no network or share needed in the guest). Not for ESXi / vCenter.

1. Type `vmware://`: one folder per running VM, plus `<vm>.connect.txt` (F3: the guest IP and the
   `sftp://<user>@<ip>/` / `smb://<user>@<ip>/` address to open the same guest with the SFTP /
   SMB plugin, which is faster for big files; type it in the path bar).
2. Enter a VM: Commander asks for the **guest** login. Type `user:password` (a domain account:
   `CORP\me:secret`), or only the password when the user is known (`vmware://user@<vm>`, or
   `user` / `[users]` in `vmware.toml`). Memory only, never written anywhere; a wrong one asks again.
3. Windows guests show drive letters (`C` = `C:\`), Linux guests `/`. F3 / F4 / F5 / F6 / F7 / F8
   work as on disk; each file is copied whole through a private temp folder (VIX: roughly
   2-25 MB/s).
   **Encrypted VMs** (every VM with a vTPM, e.g. Windows 11): Commander first asks for the
   **VM encryption password** (the one Workstation asks at power-on), then the guest login.
   Memory only; a wrong one asks again with VMware's error text. The **vm** drive button opens
   `vmware://`.
4. Needs VMware Tools in the guest and, best, the **VMware VIX** library (Windows: VIX SDK in
   `C:\Program Files (x86)\VMware\VMware VIX`). Without VIX the plugin uses `vmrun`: slower, no
   sizes or dates in listings (copying a whole folder out fails), and the **guest password is on
   vmrun's command line** while it runs (other local users / processes may see it; the login
   prompt says so). A replace writes a new file: the old file's permissions / owner / ACL are
   not kept (an executable loses `+x`).
5. Optional `vmware.toml` next to `config.toml` (env `COMMANDER_VMWARE_CONFIG`):
```toml
product = "ws"          # ws | player | fusion
backend = "auto"        # auto (VIX, else vmrun) | vix | vmrun
vix = 'C:\Program Files (x86)\VMware\VMware VIX\Workstation-17.0.0\64bit\vix.dll'
vmrun = 'C:\Program Files (x86)\VMware\VMware Workstation\vmrun.exe'
user = "me"             # guest user for every VM
drives = "CD"           # Windows guests: drives to show (default: probe C..Z)
[users]
"Windows 11" = "Administrator"
```

## 7-Zip (RAR, ISO, CAB, encrypted archives, ...)

The 7-Zip plugin uses **your own 7-Zip**; nothing else is bundled. Install it from 7-zip.org
(Windows), or put `7zz` / `7z` on your PATH (Linux, macOS). Env `COMMANDER_7Z` can point at the
program directly.

- Enter (or Ctrl+PgDn) on a `.rar`, `.iso`, `.cab`, `.arj`, `.lzh`, `.wim` or `.001` file opens it
  like a folder. Zip, 7z and tar files open with the built-in reader; the plugin steps in when the
  built-in reader can't (for example encrypted items). Multi-part zips (`name.z01`.. + `name.zip`,
  `name.zip.001`.., or `name.001`.. holding a zip) open with the built-in reader too (read-only);
  other `.001` sets (7z, RAR, ...) still go to the plugin.
- Password-protected archives ask for the password. It is kept in memory only, never in a path,
  log or config file.
- 7z, zip, tar and wim archives can be written to (copy in, delete, rename); RAR is extract-only.
  Alt+F5 can pack to `.7z`.
- Every file added to an archive is tested before a move deletes its source.
