# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

libucl is a C library for parsing UCL (Universal Configuration Language) — an nginx-inspired config format that is a superset of JSON. It also reads/writes MessagePack, emits YAML, and reads S-expressions. Version 0.9.4.

## Build Commands

```bash
# Standard CMake build
cmake -B build && cmake --build build

# Build with sanitizers
cmake -B build -DENABLE_SANITIZERS=ON -DSANITIZE_TYPE=address && cmake --build build

# Build with utilities (ucl-tool, objdump, chargen)
cmake -B build -DENABLE_UTILS=ON && cmake --build build

# Build with Lua bindings
cmake -B build -DENABLE_LUA=ON && cmake --build build
```

## Running Tests

```bash
# Run all tests via CTest
cd build && ctest

# Run a specific test (test names: test_basic, test_schema, test_msgpack, test_generate, test_speed)
cd build && ctest -R test_basic

# Run test binary directly (needs env vars)
TEST_DIR=tests TEST_OUT_DIR=build/tests TEST_BINARY_DIR=build/tests ./tests/basic.test
```

Tests use shell wrapper scripts (`tests/*.test`) that invoke compiled test binaries. Basic tests parse `.in` files and compare output against `.res` files in `tests/basic/`. Schema tests run JSON Schema v4 test suites from `tests/schema/*.json`.

## Architecture

**Core source files** (all in `src/`):

- `ucl_parser.c` — State-machine parser handling UCL, JSON, and msgpack input. The main parsing loop, macro processing (`.include`, `.priority`), and variable expansion live here. This is the largest and most complex file.
- `ucl_emitter.c` — Tree-walking emitter dispatching to format-specific handlers (JSON, YAML, config, msgpack). Uses a context/ops pattern where `ucl_emitter_context` holds state and `ucl_emitter_operations` is a vtable of format callbacks.
- `ucl_emitter_streamline.c` — Streaming emitter API that builds output incrementally without requiring a complete object tree.
- `ucl_emitter_utils.c` — Shared emitter helpers: number formatting, string escaping, indentation.
- `ucl_schema.c` — JSON Schema draft v4 validator. Recursive descent over schema keywords (type, properties, allOf/anyOf/oneOf, etc.).
- `ucl_hash.c` — Hash table wrapping uthash with optional case-insensitive keys. Each `ucl_object_t` of type `UCL_OBJECT` stores children in one of these.
- `ucl_util.c` — Object manipulation API (creation, lookup, iteration, type conversion), file I/O, and the `.include` macro implementation.
- `ucl_msgpack.c` — MessagePack serialization/deserialization.
- `ucl_sexp.c` — S-expression (canonical form) parser.

**Key headers**:

- `include/ucl.h` — Public C API. All external types and functions.
- `include/ucl++.h` — C++ wrapper with `Ucl` class.
- `src/ucl_internal.h` — Internal parser state, chunk management, macro structures.

**Bundled dependencies** (header-only, in-tree):

- `uthash/` — Hash table macros (uthash, utlist, utstring)
- `klib/` — kvec (dynamic arrays)

**Object model**: The central type is `ucl_object_t` (defined in `ucl.h`). Objects form a tree; each node has a type (int, float, string, boolean, object, array, null, userdata), a key, and a next pointer for implicit arrays (duplicate keys). Objects use reference counting (`ucl_object_ref`/`ucl_object_unref`).

**Parser lifecycle**: Create with `ucl_parser_new()`, feed data with `ucl_parser_add_string()`/`ucl_parser_add_file()`/`ucl_parser_add_fd()`, retrieve result with `ucl_parser_get_object()`, free with `ucl_parser_free()`. The parser processes input in chunks and maintains a state stack.

## Test Data Convention

Basic parse tests: `tests/basic/N.in` (input) + `tests/basic/N.res` (expected output). To add a parse test, create a new `.in`/`.res` pair. The test harness also exercises FD-based parsing, JSON/YAML output modes, and comment/macro preservation modes on every input file.

## Notable CMake Options

| Option | Default | Purpose |
|--------|---------|---------|
| `ENABLE_SANITIZERS` | OFF | AddressSanitizer/UBSan/etc. |
| `SANITIZE_TYPE` | address | Which sanitizer (address, thread, undefined, memory, leak) |
| `ENABLE_URL_INCLUDE` | OFF | `.include` from URLs (needs libcurl or libfetch) |
| `ENABLE_URL_SIGN` | OFF | Signature verification for includes (needs OpenSSL) |
| `ENABLE_LUA` | OFF | Lua bindings (`lua/lua_ucl.c`) |
| `BUILD_SHARED_LIBS` | OFF | Build shared instead of static library |
