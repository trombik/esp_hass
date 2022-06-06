# Documentation

For building HTML documentation, `sphinx` with `sphinx-c-autodoc` is used.

`sphinx-c-autodoc` depends on `clang` python module. `clang` python module
depends on `clang`.  The version of `clang` is 13. See
[.requirements.txt](../requirements.txt).

## Preparing

Set `IDF_PATH` environment variable to the path to `esp-idf`.

If `make` is not `gmake` on your machine &mdash; on Linux or macos &mdash;
define `MAKE` as `make`.

```console
export MAKE=make
```

## Generating documentation

```console
tox -e build_docs
```
