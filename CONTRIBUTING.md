# Contributing

Thanks for your interest in contributing.

## License

This project is licensed under the [Apache License, Version 2.0](LICENSE).
By submitting a contribution (a pull request, patch, or other proposed change),
you agree that your contribution is licensed under the same terms and that you
have the right to submit it under those terms.

## Developer Certificate of Origin (DCO)

Instead of a separate Contributor License Agreement, this project uses the
[Developer Certificate of Origin](https://developercertificate.org/) (DCO).
Every commit in a pull request must include a `Signed-off-by` trailer certifying
that you wrote the change or otherwise have the right to submit it under the
project's license.

Use your real name and a reachable email address. Anonymous or pseudonymous
sign-offs are not accepted. The trailer must identify the contributor:

```text
Signed-off-by: Your Name <your.email@example.com>
```

## Signed commits

Every contribution commit must also carry a cryptographic signature verifiable
by GitHub. Configure Git commit signing with your GPG or SSH signing key and
register the corresponding public signing key with your GitHub account.

Both `-s` and `-S` are required when committing:

```sh
git commit -s -S -m "Describe the change"
```

Lowercase `-s` adds the DCO sign-off trailer. Uppercase `-S` cryptographically
signs the commit. They serve separate purposes; every commit must have both.
If signing fails, fix the signing configuration before submitting the commit.

To fix the most recent commit before submitting it:

```sh
git commit --amend --no-edit -s -S
```

For multiple commits, use an interactive rebase and amend each affected commit
with both flags. Amendments and rebases change commit IDs; sign the rewritten
commits as well. Coordinate with collaborators before rewriting a shared branch.

## Third-party dependencies

Dependencies retain their own licenses. When adding, removing, or upgrading a
dependency, review its license and update the relevant `idf_component.yml`
manifests and generated `dependencies.lock` files in the same pull request.
Keep the root component, test application, and example application consistent.
Preserve any required third-party copyright and attribution notices.

## Per-file license headers

Source files and supported build/configuration files carry Apache-2.0 headers
with an SPDX identifier. Use `addlicense` to add a header to a new file:

```sh
addlicense -l apache -s -c "Your Name" path/to/new_file.c
```

Use the appropriate copyright holder. Preserve existing notices. For formats
that `addlicense` skips, add the equivalent header using that format's comment
syntax.

The license CI checks tracked files with:

```sh
git ls-files -z | xargs -0 addlicense -check -l apache -s -c "Maxim Pavlov"
```

Run this command in Git Bash or another shell with `xargs`. Stage new files
first so `git ls-files` includes them.

## Getting started

See the [README](README.md) for wiring, supported ESP-IDF versions, API usage,
and build instructions. Follow the project's existing C naming and formatting
conventions, applying the Google C++ Style Guide where appropriate for C.
Document header declarations with Doxygen; use implementation comments to
explain ownership, hardware sequencing, and other non-obvious decisions.

Build both applications from an activated ESP-IDF 6.1 terminal:

```sh
idf.py -C test -B ../build/test build
idf.py -C examples/basic -B ../../build/basic build
```

The Python wrappers in `scripts/` are for AI-assisted local operations. AI agents
follow [AGENTS.md](AGENTS.md) and the [scripts README](scripts/README.md).
GitHub CI uses `idf.py build` directly and must not use those wrappers.

## Pull requests

Describe the problem, resulting behavior, and validation performed. Update
documentation when changing configuration, APIs, or example behavior. Add or
update meaningful tests for behavior changes.

Both firmware builds and the license header check must pass. CI compiles the
Unity test application but does not execute its tests. Report hardware test
results separately, including the board and display used, and distinguish
successful compilation from tests actually run on an ESP32-P4.

Before submitting, ensure every contribution commit has a DCO sign-off and a
cryptographic signature. These are contribution requirements; the current
firmware and license workflows do not enforce them automatically.
