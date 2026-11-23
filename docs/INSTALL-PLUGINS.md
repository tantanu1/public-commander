# Installing plugins (S3, FTP/FTPS, SFTP, 7-Zip)

Commander ships with these plugins: **S3** (`s3://`, Amazon S3 and S3-compatible storage),
**FTP** (`ftp://`), **FTPS** (`ftps://`, explicit TLS), **SFTP** (`sftp://`) and **7-Zip**
(`sevenzip`: RAR/ISO/CAB/... and encrypted archives; needs 7-Zip itself installed: 7-zip.org,
or `7zz` / `7z` on PATH). Developer guide (writing your own):
[PLUGIN-DEV.md](PLUGIN-DEV.md).

Plugins ship with releases **starting with v0.0.6** (v0.0.5 and older have none). On an
older version, install the newest release first (the plugin ABI must match the app, so use
plugins from the same release).

## Installers: nothing to do

The installers already contain the plugins. Install Commander and they work:

| Installer | Bundled plugins live in |
|---|---|
| Windows `.msi` | `%LOCALAPPDATA%\Programs\Commander\plugins` |
| Linux `.deb` / `.rpm` | `/usr/lib/commander/plugins` |
| Linux `.AppImage` | inside the image |
| macOS `.dmg` | `Commander.app/Contents/Resources/plugins` |

## Plain binary / portable: install the plugin zip

Download `commander-plugins-<version>-<target>.zip` for **the same version and platform** as your
Commander from [Releases](https://github.com/tantanu1/public-commander/releases) (target: e.g.
`x86_64-pc-windows-msvc`, `aarch64-apple-darwin`, `x86_64-unknown-linux-gnu`). Extract it into
one of the plugin folders, so you get `plugins/s3/plugin.toml`, `plugins/ftp/...`,
`plugins/ftps/...`, `plugins/sftp/...`, `plugins/sevenzip/...`, then restart Commander.

Plugin folders, searched in this order (the first plugin with a given id wins):

| | Your plugin folder (survives updates) | Next to the program |
|---|---|---|
| Windows | `%APPDATA%\commander\plugins` (`C:\Users\<you>\AppData\Roaming\commander\plugins`) | `<folder of commander.exe>\plugins` |
| Linux | `~/.config/commander/plugins` (`$XDG_CONFIG_HOME/commander/plugins` if set) | `<folder of commander>/plugins`, then `<folder>/../lib/commander/plugins` |
| macOS | `~/.config/commander/plugins` (`$XDG_CONFIG_HOME/commander/plugins` if set) | `<folder of commander>/plugins`, then `Commander.app/Contents/Resources/plugins` |

Windows, PowerShell: `Expand-Archive commander-plugins-*.zip "$env:APPDATA\commander\plugins" -Force`.
Linux/macOS: `mkdir -p ~/.config/commander/plugins && unzip -o commander-plugins-*.zip -d ~/.config/commander/plugins`.
macOS may quarantine downloaded libraries: `xattr -dr com.apple.quarantine ~/.config/commander/plugins`.

Each plugin is one folder holding `plugin.toml` + its library (`.dll` / `.so` / `.dylib`).
`ftps` holds only a `plugin.toml`: it uses the library in `../ftp`, so keep both.

## Check that a plugin loaded

**Configuration > Options > Plugins** lists the folders searched, every plugin found (name,
version, `scheme://`, folder) and any problems (bad `plugin.toml`, wrong ABI). Plugins are found
at startup: restart after installing. Quick test: type `s3://` in the path bar; without the
plugin it is just an unknown path. Ctrl+F says "FTP plugin not installed" when ftp is missing.

## S3 (Amazon S3, MinIO, Cloudflare R2, SeaweedFS, ...)

1. Credentials, like the AWS CLI (first match wins): env `AWS_ACCESS_KEY_ID` +
   `AWS_SECRET_ACCESS_KEY` (+ `AWS_SESSION_TOKEN`); else the profile `AWS_PROFILE` (default
   `default`) in `~/.aws/credentials`, then `~/.aws/config`. Region: `AWS_REGION`,
   `AWS_DEFAULT_REGION`, profile `region`, else `us-east-1`; buckets in other regions are
   followed automatically. Not supported: SSO, assume-role, `credential_process`, instance roles.
2. S3-compatible servers: set the endpoint with env `AWS_ENDPOINT_URL` (or `AWS_ENDPOINT_URL_S3`,
   or `endpoint_url = http://localhost:9000` in the profile). Path-style addressing is then the
   default; override with env `COMMANDER_S3_PATH_STYLE=0|1`.
3. Type `s3://` in the path bar (bucket list) or `s3://bucket/folder`, Enter. F3 view,
   F5 copy, F6 move/rename, F7 new folder, F8 delete (always permanent) work as on disk.

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
