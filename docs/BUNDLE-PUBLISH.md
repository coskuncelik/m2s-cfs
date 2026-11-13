# Publishing the bundle `cfs-m2s-freertos`

How to push a new version of the shareable bundle to https://github.com/coskuncelik/cfs-m2s-freertos (public).
How the bundle is made and checked: `docs/ARCHITECTURE.md` ("Shareable bundle"), D30, D35. Publishing needs the
user's approval each time (`CLAUDE.md`: commit and push only with approval).

## Before pushing

First set the new version's tag and commit message (team-facing: no decision numbers, step names or paths of
this repository; the export checks that). Export them in the shell, because `07-check-bundle.sh` runs the export
again itself and would otherwise use the defaults:

```
export BUNDLE_TAG=v3-... BUNDLE_TITLE="..." BUNDLE_BODY="..."
./scripts/07-export-bundle.sh      # commits the new version on top of the published main, with tags; does not push
./scripts/07-check-bundle.sh       # must end with "=== 07-check-bundle OK ==="
```

The result is the git repository `build/export/cfs-m2s-freertos`: its `main` is the published `main` plus one new
commit. Look at it before pushing:

```
cd build/export/cfs-m2s-freertos
git log --oneline --decorate -3
git diff --stat origin/main HEAD
```

## Why a plain `git push` fails here

Inside the Codespace, git authenticates with the Codespace's own token (`GITHUB_TOKEN`). That token can only write
to the repository the Codespace was created for (`m2s-cfs`). Pushing to `cfs-m2s-freertos` therefore fails with:

```
remote: Permission to coskuncelik/cfs-m2s-freertos.git denied to coskuncelik.
fatal: unable to access 'https://github.com/coskuncelik/cfs-m2s-freertos.git/': The requested URL returned error: 403
```

## The token

Use a **fine-grained personal access token**, limited to this one repository:

- GitHub → Settings → Developer settings → Personal access tokens → Fine-grained tokens → Generate new token;
- Repository access: "Only select repositories" → `coskuncelik/cfs-m2s-freertos`;
- Permissions → Repository permissions → **Contents: Read and write** (nothing else is needed);
- a short expiry is fine; a new token can be made for each publication.

Never write the token into a file, a script, a commit or a document.

## Pushing

From the bundle repository:

```
cd build/export/cfs-m2s-freertos
read -rs T          # paste the token, press Enter (nothing is shown; it stays out of the shell history)
git push "https://x-access-token:${T}@github.com/coskuncelik/cfs-m2s-freertos.git" main v1-sim v2-sim-qemu <new tag>
unset T
```

- `x-access-token` is the user name GitHub expects when a token is given in the URL; the token is the password.
- Name the tags explicitly: a plain `git push` sends only the branch, no tags.
- **Always a normal push, never `--force`.** The new commit sits on top of the published `main`, so the push is a
  fast-forward and the team can `git pull`. If GitHub answers "non-fast-forward" or "rejected", someone pushed to
  the bundle in the meantime: stop, do not force, and re-run the export (it starts from the published `main`).

## Checking afterwards

```
git ls-remote https://github.com/coskuncelik/cfs-m2s-freertos.git
git -C build/export/cfs-m2s-freertos rev-parse HEAD
```

`refs/heads/main` and the tags must show the same commits as the local repository. Then record the publication in
`docs/STATUS.md` (commit, tags, date).

## History of publications

| Date | `main` | Tag | Content |
|---|---|---|---|
| 2026-11-05 | `d48850f` | `v1-sim` | Linux simulator (`sample_defs`), counter_app |
| 2026-11-09 | `ed41597` | `v2-sim-qemu` | configurations `freertos_sim` and `freertos_qemu` (Cortex-M3 on QEMU) |
| 2026-11-13 | `0bf16e7` | `v3-tctm-sim` | commands and telemetry over UDP on the simulator (`sch_lab`, `ci_lab`, `to_lab`, networking), ground station, counter_app features, build-time `/cf` image with tables |

## For the team

After a new version: `git pull` in their clone, delete old build folders if the release notes say so, and build
with the commands in the bundle's `README.md`.
