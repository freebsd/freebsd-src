# Version history

## Libucl 0.5

- Streamline emitter has been added, so it is now possible to output partial `ucl` objects
- Emitter now is more flexible due to emitter_context structure

### 0.5.1
- Fixed number of bugs and memory leaks

### 0.5.2

- Allow userdata objects to be emitted and destructed
- Use userdata objects to store lua function references

### Libucl 0.6

- Reworked macro interface

### Libucl 0.6.1

- Various utilities fixes

### Libucl 0.7.0

- Move to klib library from uthash to reduce memory overhead and increase performance

### Libucl 0.7.1

- Added safe iterators API

### Libucl 0.7.2

- Fixed serious bugs in schema and arrays iteration

### Libucl 0.7.3

- Fixed a bug with macros that come after an empty object
- Fixed a bug in include processing when an incorrect variable has been destroyed (use-after-free)

### Libucl 0.8.0

- Allow to save comments and macros when parsing UCL documents
- C++ API
- Python bindings (by Eitan Adler)
- Add msgpack support for parser and emitter
- Add Canonical S-expressions parser for libucl
- CLI interface for parsing and validation (by Maxim Ignatenko)
- Implement include with priority
- Add 'nested' functionality to .include macro (by Allan Jude)
- Allow searching an array of paths for includes (by Allan Jude)
- Add new .load macro (by Allan Jude)
- Implement .inherit macro (#100)
- Add merge strategies
- Add schema validation to lua API
- Add support for external references to schema validation
- Add coveralls integration to libucl
- Implement tests for 80% of libucl code lines
- Fix tonns of minor and major bugs
- Improve documentation
- Rework function names to the common conventions (old names are preserved for backwards compatibility)
- Add Coverity scan integration
- Add fuzz tests

**Incompatible changes**:

- `ucl_object_emit_full` now accepts additional argument `comments` that could be used to emit comments with UCL output

### Libucl 0.8.1

- Create ucl_parser_add_file_full() to be able to specify merge mode and parser type (by Allan Jude)
- C++ wrapper improvements (by @ftilde)
- C++ wrapper: add convenience method at() and lookup() (by Yonghee Kim)
- C++ wrapper: add assignment operator to Ucl class (by Yonghee Kim)
- C++ wrapper: support variables in parser (by Yonghee Kim)
- C++ wrapper: refactoring C++ interface (by Yonghee Kim):
    - use auto variables (if possible)
    - remove dangling expressions
    - use std::set::emplace instead of std::set::insert
    - not use std::move in return statement; considering copy elision
- C++ wrapper: fix compilation error and warnings (by Zhe Wang)
- C++ wrapper: fix iteration over objects in which the first value is `false` (by Zhe Wang)
- C++ wrapper: Macro helper functions (by Chris Meacham)
- C++ wrapper: Changing the duplicate strategy in the C++ API (by Chris Meacham)
- C++ wrapper: Added access functions for the size of a UCL_ARRAY (by Chris Meacham)
- Fix caseless comparison
- Fix include when EPERM is issued
- Fix Windows build
- Allow to reserve space in arrays and hashes
- Fix bug with including of empty files
- Move to mum_hash from xxhash
- Fix msgpack on non-x86
- python: Add support to Python 3 (by Denis Volpato Martins)
- python: Add support for Python 2.6 tests (by Denis Volpato Martins)
- python: Implement validation function and tests (by Denis Volpato Martins)
- python: Added UCL_NULL handling and tests (by Denis Volpato Martins)
- Fix schema validation for patternProperties with object data (by Denis Volpato Martins)
- Remove the dependency on NBBY, add missing <strings.h> include (by Ed Schouten)
- Allow to emit msgpack from Lua
- Performance improvements in Lua API
- Allow to pass opaque objects in Lua API for transparent C passthrough
- Various bugs fixed
- Couple of memory leaks plugged