# Major changes between releases

## Changes in version 0.7.0

Released on 2026/11/26.

This is a major cleanup release. A lot of credit goes to @mchoo7 for all of the
hard work submitted in #13 🎉.

The API should not change in a noticeable way to consumers, e.g., a number of
"transparent" changes like `const` poisoning was done with methods, etc. The
major version for the library was bumped from 3 to 4 just to be safe.

### Bugfixes

* #55: chore(build): port fix from ATF repo
* #57: fix: correct return type for`lutok::state::to_integer`
* #59: fix: Avoid implicit conversions in initializers
* #60: fix: Remove redundant `std::static_cast` call
* #61: chore: Replace `NULL` with `nullptr`
* #62: Use auto when initializing with a cast
* #63: Use default destructors instead of empty ones
* #64: Employ modern C++ idioms
* #65: General interface cleanup
* #66: fix: Swap lutok and lua CFLAGS/LDFLAGS around
* #67: Prefer `override` instead of `virtual` in C++ method declarations
* #68: Make code more explicit and safe (final)

### Developer Workflow Changes

* #51: Miscellaneous cleanup
* #53: chore(ci): sync CI fixes/enhancements from ATF
* #54: chore: run pre-commit autoupdate
* #56: feat: Port commit-check integration from the ATF project
* #69: Port over clang-format changes from ATF project

**Full Changelog**:
https://github.com/freebsd/lutok/compare/lutok-0.6.3...lutok-0.7.0

### Developer Workflow Changes

**Full Changelog**: https://github.com/freebsd/lutok/compare/lutok-0.6.3...lutok-0.7.0

## Changes in version 0.6.3

Released on 2026/09/01.

This release contains [mostly] developer-workflow related changes. Many of these changes were ported over from
the [freebsd/atf](https://github.com/freebsd/atf) repository.

### Developer Workflow Changes

* Automate uploading release assets by @ngie-eign in https://github.com/freebsd/lutok/pull/40

* Port over CI work from freebsd/atf by @ngie-eign in https://github.com/freebsd/lutok/pull/41

* chore: remove .travis.yml by @mchoo7 in https://github.com/freebsd/lutok/pull/44

* Add sanitizer functionality (ASAN, LSAN, UBSAN) support by @ngie-eign in https://github.com/freebsd/lutok/pull/46

* Add runtime code coverage support by @ngie-eign in https://github.com/freebsd/lutok/pull/47

* Add freebsd vmactions by @ngie-eign in https://github.com/freebsd/lutok/pull/48

* Add pre-commit config by @ngie-eign in https://github.com/freebsd/lutok/pull/49

**Full Changelog**: https://github.com/freebsd/lutok/compare/lutok-0.6.2...lutok-0.6.3

## Changes in version 0.6.2

Released on 2025/12/27.

* Update the minimum C++ standard to C++-20.

## Changes in version 0.6.1

Released on 2025/03/28.

* Issue 37: fix release tarball by relying on `make dist`.

## Changes in version 0.6

Released on 2025/01/04.

* Remove lingering C++-11 support. The project now requires a C++14 compliant toolchain to build.

* Fix a Doxygen warning by deleting the `stack_cleaner` copy/assignment operator.

* Vendor the ATF m4 files from freebsd/atf@atf-0.22 ,

* `m4/lua.m4`: remove legacy `lua-config` support in favor of `pkg_config`.

* Fix a benign buffer overrun complaint from gcc 13.2.

* Rename `--with-atf` to `--enable-atf`.

## Changes in version 0.5

Released on 2024/11/25.

* Use more modern autotools macros.  Bump the minimum autoconf version to 2.68 to
  support this change.

* Modify build code to improve Lua 5.4 support.

* Replace `std::auto_ptr` use with `std::unique_ptr` use.  This improves C++
  standard support and fixes the build with newer C++ standards.

## Changes in version 0.4

Released on 2013/12/07.

* Cope with the lack of AM_PROG_AR in configure.ac, which first
  appeared in Automake 1.11.2.  Fixes a problem in Ubuntu 10.04
  LTS, which appears stuck in 1.11.1.

* Stopped shipping an `Atffile`.  The only supported way to run the tests
  is via Kyua.

Interface changes:

* Issue 5: New methods added to the state class: `open_all`.

* Removed default parameter values from all state methods and all
  standalone operations.  It is often unclear what the default value is
  given that it depends on the specific Lua operation.  Being explicit
  on the caller side is clearer.

* Modified operations `do_file` and `do_string` to support passing a number
  of arguments to the loaded chunks and an error handler to the backing
  `pcall` call.


## Changes in version 0.3

Released on 2013/06/14.

* Issue 1: Added support for Lua 5.2 while maintaining support for Lua
  5.1.  Applications using Lutok can be modified to use the new
  interface in this new version and thus support both Lua releases.
  However, because of incompatible changes to the Lua API, this release
  of Lutok is incompatible with previous releases as well.

* Issue 3: Tweaked configure to look for Lua using the pkg-config names
  lua-5.2 and lua-5.1.  These are the names used by FreeBSD.

Interface changes:

* New global constants: `registry_index`.

* New methods added to the state class: `get_global_table`.

* Removed global constants: `globals_index`.


## Changes in version 0.2

Released on 2012/05/30.

* New global constants: `globals_index`.

* New methods added to the state class: `get_metafield`, `get_metatable`,
  `insert`, `push_value`, `raw_get`, and `raw_set`.

* Acknowledged that Lua 5.2 is currently not supported.


## Changes in version 0.1

Released on 2012/01/29.

* This is the first public release of the Lutok package.
