# Release process

## Version flow

```text
0.1.0-SNAPSHOT -> 0.1.0 -> 0.1.1-SNAPSHOT
```

Development versions use `-SNAPSHOT`. A release removes that suffix. After
publishing, bump to the next planned version and restore `-SNAPSHOT`.
Use the same version in all three manifests:

- `idf_component.yml`
- `test/components/waveshare_7inch_h_dsi_15pin/idf_component.yml`
- `examples/basic/components/waveshare_7inch_h_dsi_15pin/idf_component.yml`

All three manifests are set to the release version `0.1.0`. Merge this branch
into `main`, wait for firmware and license CI on the merged commit, then follow
steps 3 and 4 to tag and publish `v0.1.0`. After publication, follow step 5 to
start `0.1.1-SNAPSHOT` development. For later releases, follow every step.

Commands run from the repository root in Git Bash with an activated ESP-IDF
6.1 environment. GitHub CLI (`gh`) must be authenticated. Git signing must be
configured. Every commit uses both `-s` and `-S`; release tags are signed.
`main` changes only through pull requests.

## 1. Prepare the release branch

Example: release `0.1.0`.

```sh
git switch main
git pull --ff-only origin main
git status --short
git switch -c release/0.1.0
```

Start with a clean working tree. Change `version: "0.1.0-SNAPSHOT"` to
`version: "0.1.0"` in all three manifests. Update README and API documentation
if release behavior changed. Prepare the release notes: changes, fixes,
breaking changes, and known limitations.

## 2. Validate and submit the release PR

```sh
idf.py -C test -B ../build/test build
idf.py -C examples/basic -B ../../build/basic build
git ls-files -z | xargs -0 addlicense -check -l apache -s -c "Maxim Pavlov"
git diff --check
git diff
```

Review generated changes to `test/dependencies.lock` and
`examples/basic/dependencies.lock`. Run the Unity tests on an ESP32-P4 and
record the results. For hardware changes, also test the display and touch
behavior described in README. A build is not a test run.

Human maintainers and CI use `idf.py` directly. Python wrappers in `scripts/`
are for AI-assisted local operations.

Stage the release files you changed, including documentation when applicable:

```sh
git add idf_component.yml \
  test/components/waveshare_7inch_h_dsi_15pin/idf_component.yml \
  examples/basic/components/waveshare_7inch_h_dsi_15pin/idf_component.yml \
  test/dependencies.lock examples/basic/dependencies.lock
git commit -s -S -m "Release 0.1.0"
git push -u origin release/0.1.0
gh pr create --base main --head release/0.1.0 --title "Release 0.1.0" --body "Prepare version 0.1.0 for release."
```

Include validation results in the PR description. Wait for required checks and
review, then merge the PR through GitHub. If the first release requires no
file changes, skip the empty commit and PR; use the existing approved commit.

## 3. Tag the merged release commit

```sh
git switch main
git pull --ff-only origin main
git status --short
git log -1 --oneline
git rev-parse HEAD
```

Confirm the working tree is clean, all three manifests say `0.1.0`, and the
selected commit has passed firmware and license CI. Record its full SHA.
Tag that release commit before merging the next SNAPSHOT bump. If another PR
has already advanced `main`, use the recorded release commit SHA instead of
`HEAD` in the tag command.

```sh
git tag -s v0.1.0 HEAD -m "Release 0.1.0"
git tag -v v0.1.0
git push origin v0.1.0
```

Do not move or overwrite an existing release tag.

## 4. Publish the GitHub release

```sh
gh release create v0.1.0 --verify-tag --title "0.1.0" --generate-notes
```

Edit the generated notes on GitHub to include the prepared release notes,
validation results, and known limitations. Confirm the release points to the
signed tag and recorded commit SHA.

Consumers can now use:

```yaml
dependencies:
  waveshare_7inch_h_dsi_15pin:
    git: "https://github.com/max-eelabs/waveshare-7inch-dsi-lcd-h-15-pin-drv.git"
    version: "v0.1.0"
```

## 5. Resume SNAPSHOT development

```sh
git switch main
git pull --ff-only origin main
git switch -c chore/0.1.1-snapshot
```

Change all three manifests from `version: "0.1.0"` to
`version: "0.1.1-SNAPSHOT"`. If the next planned release adds features or breaks
compatibility, choose that next version instead, for example `0.2.0-SNAPSHOT`.

Build both applications again to refresh their lock files:

```sh
idf.py -C test -B ../build/test build
idf.py -C examples/basic -B ../../build/basic build
git diff --check
git add idf_component.yml \
  test/components/waveshare_7inch_h_dsi_15pin/idf_component.yml \
  examples/basic/components/waveshare_7inch_h_dsi_15pin/idf_component.yml \
  test/dependencies.lock examples/basic/dependencies.lock
git commit -s -S -m "Start 0.1.1-SNAPSHOT development"
git push -u origin chore/0.1.1-snapshot
gh pr create --base main --head chore/0.1.1-snapshot --title "Start 0.1.1-SNAPSHOT development" --body "Bump development version after the 0.1.0 release."
```

Merge through the required checks and review. Do not create a release tag or
GitHub release for SNAPSHOT versions. For the next release, repeat the process
with `0.1.1-SNAPSHOT -> 0.1.1 -> 0.1.2-SNAPSHOT`.

## ESP Component Registry publication

TBD.
