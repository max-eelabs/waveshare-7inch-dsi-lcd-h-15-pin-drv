# Build and flash output

The Python wrappers are for AI-assisted local firmware operations. GitHub CI
must use standard ESP-IDF commands directly, not these wrappers.

Use silent wrappers for routine local firmware operations:

- Configure: `python scripts/configure.py` (optional; build configures a fresh directory)
- Build: `python scripts/build.py`
- Flash only when requested: `python scripts/flash.py`
- Add `--example` to select the display example instead of tests.

Defaults are `test/` and `build/test/`; see `scripts/README.md` for overrides.

Do not invoke Ninja, idf.py, or esptool directly for routine local operations
unless diagnosing a failure. CI workflow commands use idf.py directly.

# Contributions and commits

Follow [CONTRIBUTING.md](CONTRIBUTING.md). When a commit is requested, use both
`-s` (DCO sign-off) and `-S` (cryptographic signature). Do not omit either flag
if signing fails; resolve the signing configuration first.
