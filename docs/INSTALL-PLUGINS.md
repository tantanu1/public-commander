# Installing plugins (S3, FTP/FTPS, SFTP, 7-Zip)

Commander has these plugins: **S3** (`s3://`, Amazon S3 and S3-compatible storage),
**FTP** (`ftp://`), **FTPS** (`ftps://`, explicit TLS), **SFTP** (`sftp://`) and **7-Zip**
(`sevenzip`: RAR/ISO/CAB/... and encrypted archives; needs 7-Zip itself installed: 7-zip.org,
or `7zz` / `7z` on PATH). Developer guide (writing your own):
[PLUGIN-DEV.md](PLUGIN-DEV.md).

The installers (`.msi`, `.deb`, `.rpm`, `.AppImage`, `.dmg`) contain **only the app**. Plugins are a
separate download: `commander-plugins-<version>-<target>.zip` on the same release page. They exist
from **v0.0.6** on; the plugin ABI must match the app, so use the zip of the same release.

> **Upgrading from v0.0.6 or v0.0.7?** Those installers included the plugins; the new installer
> removes them. Drop the plugin zip on Commander (below) to get them back.

## How to use them

| Plugin | How to open it |
|---|---|
| S3 | The `s3` drive (Alt+F1 / Alt+F2): profiles, then buckets; or type `s3://` in the path bar |
| FTP / FTPS | **Ctrl+F** (Net > FTP Connect); bookmarks in `ftp.toml` |
| SFTP | **Ctrl+F** or type `sftp://host/path`; bookmarks in `sftp.toml` |
| 7-Zip | Just open a `.rar`, `.iso`, `.cab`, ... or encrypted archive (Enter); uses your installed 7-Zip |

**Configuration > Options > Plugins** shows which plugins loaded.

## Install: drop the zip on Commander

1. Download `commander-plugins-<version>-<target>.zip` for **the same version and platform** as
   your Commander from [Releases](https://github.com/tantanu1/public-commander/releases).
2. Drag the zip from Explorer / Finder / your file manager onto a Commander panel (or drag it from
   one panel to the other, or use **Configuration > Options > Plugins > Install plugin...** and
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

1. Download `commander-plugins-<version>-<target>.zip` for **the same version and platform** as
   your Commander from [Releases](https://github.com/tantanu1/public-commander/releases) (target:
   e.g. `x86_64-pc-windows-msvc`, `aarch64-apple-darwin`, `x86_64-unknown-linux-gnu`).
2. Unzip it into your user plugin folder, so you get `plugins/s3/plugin.toml`, `plugins/ftp/...`,
   `plugins/ftps/...`, `plugins/sftp/...`, `plugins/sevenzip/...`:
   - Windows: `%APPDATA%\commander\plugins` (`C:\Users\<you>\AppData\Roaming\commander\plugins`).
     PowerShell: `Expand-Archive commander-plugins-*.zip "$env:APPDATA\commander\plugins" -Force`
   - Linux / macOS: `~/.config/commander/plugins` (`$XDG_CONFIG_HOME/commander/plugins` if set).
     `mkdir -p ~/.config/commander/plugins && unzip -o commander-plugins-*.zip -d ~/.config/commander/plugins`
   - macOS may quarantine downloaded libraries: `xattr -dr com.apple.quarantine ~/.config/commander/plugins`
3. Restart Commander.

This folder survives app updates; after updating Commander, drop (or unzip) the new release's
plugin zip.

Each plugin is one folder holding `plugin.toml` + its library (`.dll` / `.so` / `.dylib`).
`ftps` holds only a `plugin.toml`: it uses the library in `../ftp`, so keep both.

Other folders searched after the user folder (the first plugin with a given id wins), e.g. for a
portable setup: `<folder of the commander program>/plugins`; Linux also
`<folder>/../lib/commander/plugins`; macOS also `Commander.app/Contents/Resources/plugins`.

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
   path). As with FTP, a password in the URL is refused.
```toml
[build]                         # open with sftp://build
host = "build.example.net"
port = 22                       # optional
user = "me"                     # optional
identity = "~/.ssh/id_ed25519"  # optional key file
password = "secret"             # optional, tried last
```
3. Sign-in order: the bookmark's `identity`, then your SSH agent (Windows: OpenSSH agent, then
   Pageant; Linux/macOS: `SSH_AUTH_SOCK`), then `~/.ssh/id_ed25519`, `id_ecdsa`, `id_rsa`, then the
   bookmark password. Passphrase-protected key files are skipped: add them to your ssh-agent.
4. Host keys are checked against `~/.ssh/known_hosts` and never trusted automatically. An unknown
   host is refused with its SHA256 fingerprint and the exact line to add; a changed key is refused
   with "HOST KEY CHANGED". Connect once with `ssh` to record a new host.

## 7-Zip (RAR, ISO, CAB, encrypted archives, ...)

The 7-Zip plugin uses **your own 7-Zip**; nothing else is bundled. Install it from 7-zip.org
(Windows), or put `7zz` / `7z` on your PATH (Linux, macOS). Env `COMMANDER_7Z` can point at the
program directly.

- Enter (or Ctrl+PgDn) on a `.rar`, `.iso`, `.cab`, `.arj`, `.lzh`, `.wim` or `.001` file opens it
  like a folder. Zip, 7z and tar files open with the built-in reader; the plugin steps in when the
  built-in reader can't (for example encrypted items).
- Password-protected archives ask for the password. It is kept in memory only, never in a path,
  log or config file.
- 7z, zip, tar and wim archives can be written to (copy in, delete, rename); RAR is extract-only.
  Alt+F5 can pack to `.7z`.
- Every file added to an archive is tested before a move deletes its source.
