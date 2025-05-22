# Version history

## Libucl 0.9.0

* 803b588 Breaking: Try to fix streamline embedding
* 9eddef0 Fix: set p to endptr before checking
* 25d3f51 Fix broken tests
* ac644e2 Update makefile.yml
* 0a5739e Create makefile.yml
* 987389a Merge branch 'master' into vstakhov-gh-actions
* 7433904 Import lua code from Rspamd
* 3912614 Create cmake-multi-platform.yml
* 3a04c92 lua: Push string with len
* 2fefed6 Use `_WIN32` instead of `_MSC_VER`
* aecf17e Avoid build failure trying to create setup.py link if it already exists.
* 4ef9e6d Add inttypes.h for PRId64
* dcb43f0 Fix excessive escaping when using ucl_object_fromstring()

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

### Libucl 0.8.2

* .include: also validate priority to be within range
* Add -W into list of warnings
* Add ability to add file preprocessors
* Add ability to pass both the parser and userdata into a macro handler
* Add missing tests for .gitignore
* Add more safe guards when trying to insert objects
* Add some documentation/example about the .priority macro
* Add tests for single quotes
* Added CMake compile definitions
* Added CMake support to build utils
* Added a fuzzer for OSS-fuzz integration
* Added a return statement if the string is 0
* Added default CMake "build" directory to gitignore
* Added fuzzer for msgpack
* Adding another fix
* Adjust example.
* Allow to test msgpack inputs
* Another sync
* Assume gcov absense as a non-fatal error
* Avoid read when a chunk is ended
* CMake: Install headers and library.
* Check for NULL inputs in ucl_object_compare()
* Cleanup CURL handle after use
* Cleanup CURL handle after use
* Convert ucl_hash_insert() from returning int to returning bool.
* Convert ucl_hash_reserve() from returning int to bool.
* Do not try to emit single quoted strings in json mode
* Document single quotes
* Document ucl_object_iter_chk_excpn().
* Document usage of ucl_object_iter_chk_excpn().
* Don't double-escape Lua strings
* Excercise ucl_object_iter_chk_excpn().
* Fix '\v' encoding
* Fix 68d87c362b0d7fbb45f395bfae616a28439e0bbc by setting error to 0 always. Which makes it even uglier.
* Fix cmake public include install
* Fix emitting of the bad unicode escapes
* Fix format strings, add printf attribute to schema functions
* Fix levels and objects closing
* Fix load macro with try=true
* Fix mismerge.
* Fix mismerge.
* Fix old issue with parsing numbers
* Fix processing of the incomplete msgpack objects
* Fix remain calculations
* Fix remain lenght calculation that led to assertion failure
* Fix single quotes emitting
* Fix spelling and markup errors.
* Fix typos: replace missmatch with mismatch
* Fix ucl++ bug where iterators stop on a null field.
* Fix ucl_util.c not having the prototype for ucl_hash_sort()
* Fix variables expansion
* Fix vertical tab handling
* Fixed Visual Studio compilation error
* Fixed expanding variables at runtime
* Fixed linker error
* Fixed ucl_tool's command line argument parsing
* Fixing error with installing using pip from git with following command: 'pip install -e git+https://github.com/vstakhov/libucl.git/#egg=ucl
* Forgot hash sort function
* Improve ENOMEM handling: handle most of errors while consuructing parser, also extend iterator routines to allow capturing such exception and checking it in the higher level code using new ucl_object_iter_chk_excpn() API.
* Mark + as unsafe which fixes export a key with + in config mode
* Modernise the CMake build system slightly.
* Modernize CMake file with target-based includes.
* Pass correct pointer to var_handler
* Port util objdump to Windows (Visual Studio)
* Port util ucl-tool to Windows
* Provide inline free(3) wrapper, so it's easier to plug the code into out memory usage tracking framework.
* Provide inline free(3) wrapper, so it's easier to plug the code into out memory usage tracking framework.
* Provide priority validation for the .priority macro
* Put space between "exit" and ().
* Put space between name of teh function and ().
* Python build fixes
* Read data in chunks
* Remove leak in the test
* Remove one more bit of unused logic
* Remove one more stupid assertion
* Remove unnecessary (and ignored) `const` from return types.
* Remove unnecessary std::move from return statement.
* Remove unused CMake logic and ad -Wno-pointer-sign.
* Removed dependency from rspamd CMake file
* Removed null-terminator for input data
* Rename ENOMEM-safe version of kv_xxx macros from kv_xxx into kv_xxx_safe and put back old version as well (with a big warning in the header file) for a compat purposes.
* Renamed util binaries to match autotools
* Replace *neat* and *tidy* implementation of kv_xxx() macros using error handling labels with a much *uglier* implementation using "error code pointer". One man's "ugly" is other man's "pretty", I suppose.
* Replaced spaces by tabs to match coding style
* Rework hash table structure to provide pointers and order safety
* Save chunk in the parser stack
* Save filename in chunk
* Split level and flags, add obrace flag, fix msgpack flags
* Squelch incompatible pointer type warning
* Support single quoted strings
* Suppress the [-Wunused-parameter] warning.
* Sync changes from Rspamd
* Sync changes from rspamd
* Sync with Rspamd
* Understand nan and inf
* Use safe iterator - avoid leaking memory.
* docs: fix simple typo, tectual -> textual
* fix: Changed OpenSSL check inside configure.am
* fix: Incorrect pointer arithmetics in ucl_expand_single_variable
* fix: ucl_expand_single_variable doesn't call free
* lua: Return early when init fails
* make use of the undocumented flag UCL_PARSER_NO_IMPLICIT_ARRAYS, so that multiple keys are treated as arrays, and special code doesn't have to be added to the Python module to handle it.
* mypy/stubgen: add typeinterfaces for ucl python module
* o `ucl_object_iterate2()` -> `ucl_object_iterate_with_error()`;
* python: update package to 0.8.1
* `ucl_check_variable`: fix out_len on unterminated variable
* `ucl_chunk_skipc`: avoid out-of-bounds read
* `ucl_expand_single_variable`: better bounds check
* `ucl_expand_variable`: fix out-of-bounds read
* `ucl_inherit_handler`: fix format string for non-null-terminated strings
* `ucl_lc_cmp` is not used outside ucl_hash.c
* `ucl_lex_json_string`: fix out-of-bounds read
* `ucl_maybe_parse_number`: if there is trailing content, it is not a number
* `ucl_object_copy_internal`: null terminate keys
* `ucl_object_copy_internal`: use memcpy instead of strdup
* `ucl_object_free` is deprecated
* `ucl_parse_value`: fix out-of-bounds read
* `ucl_strnstr`: fix out-of-bounds read
* update JSON example to match w/ UCL example
