# public-commander

Release builds for **Commander** (a fast, modern Total Commander clone in Rust).
This repo holds no source: its only job is CI. The workflow checks out a tag of the private
source repo, builds Windows / Linux / macOS binaries and installers, and publishes them under
[Releases](../../releases).

## How releases are triggered

- **Automatic:** every 30 min a scheduled run lists the source repo's tags and builds the newest
  `v*` tag that has no release here yet. If there is none, it exits quietly.
- **Manual:** Actions > `release` > Run workflow, enter a tag (e.g. `v0.0.4`). Re-running an
  existing tag re-uploads its assets.

No `pull_request` triggers exist, so fork PRs never see the secret.

## One-time setup: the `SOURCE_TOKEN` secret

1. GitHub > Settings > Developer settings > Fine-grained tokens > Generate new token.
   Resource owner: `tantanu1`. Repository access: *Only select repositories* > `tantanu1/commander`.
   Permissions: **Contents: Read-only** (Metadata: Read-only is added automatically). Set an expiry.
2. `gh secret set SOURCE_TOKEN -R tantanu1/public-commander` and paste the token when prompted.

Repo secrets are write-only: nobody (collaborators included) can read them back, they are masked
in logs, and they are not passed to workflows triggered from forks. When the token expires, runs
fail with a clear error; repeat the steps above.

Notes: build logs are public (they show source file paths and compiler output). Uploaded artifacts
hold only the built binaries/installers and expire after 1 day; no Actions cache is used, since
fork-PR workflows could restore it. Manual runs can set `dry_run` to build a branch (e.g. `main`)
without publishing. Tags without `packaging/` (v0.0.3 and older) ship binaries only. Scheduled
workflows are auto-disabled by GitHub after 60 days without commits to this repo; re-enable in
the Actions tab or run it manually.
