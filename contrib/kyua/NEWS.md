Major changes between releases
==============================


Changes in version 0.14
-----------------------

**NOT RELEASED YET; STILL UNDER DEVELOPMENT.**

* Explicitly require C++11 language features when compiling Kyua.


Changes in version 0.13
-----------------------

**Released on August 26th, 2016.**

* Fixed execution of test cases as an unprivileged user, at least under
  NetBSD 7.0.  Kyua-level failures were probably a regression introduced
  in Kyua 0.12, but the underlying may have existed for much longer:
  test cases might have previously failed for mysterious reasons when
  running under an unprivileged user.

* Issue #134: Fixed metadata test broken on 32-bit platforms.

* Issue #139: Added per-test case start/end timestamps to all reports.

* Issue #156: Fixed crashes due to the invalid handling of cleanup
  routine data and triggered by the reuse of PIDs in long-running Kyua
  instances.

* Issue #159: Fixed TAP parser to ignore case while matching `TODO` and
  `SKIP` directives, and to also recognize `Skipped`.

* Fixed potential crash due to a race condition in the unprogramming of
  timers to control test deadlines.


Changes in version 0.12
-----------------------

**Released on November 22nd, 2015.**

This is a huge release and marks a major milestone for Kyua as it finally
implements a long-standing feature request: the ability to execute test
cases in parallel.  This is a big deal because test cases are rarely
CPU-bound: running them in parallel yields much faster execution times for
large test suites, allowing faster iteration of changes during development.

As an example: the FreeBSD test suite as of this date contains 3285 test
cases.  With sequential execution, a full test suite run takes around 12
minutes to complete, whereas on a 4-core machine with a high level of
parallelism it takes a little over 1 minute.

Implementing parallel execution required rewriting most of Kyua's core and
partly explains explains why there has not been a new release for over a
year.  The current implementation is purely subprocess-based, which works
but has some limitations and has resulted in a core that is really complex
and difficult to understand.  Future versions will investigate the use of
threads instead for a simplified programming model and additional
parallelization possibilities.

* Issue #2: Implemented support to execute test cases in parallel when
  invoking `kyua test`.  Parallel execution is *only* enabled when the new
  `parallelism` configuration variable is set to a value greater than `1`.
  The default behavior is still to run tests sequentially because some test
  suites contain test cases with side-effects that might fail when run in
  parallel.  To resolve this, the new metadata property `is_exclusive` can
  be set to `true` on a test basis to indicate that the test must be run on
  its own.

* Known regression: Running `kyua debug` on a TAP-based test program does
  not currently report the output in real time.  The output will only be
  displayed once the test program completes.  This is a shortcoming of
  the new parallel execution engine and will be resolved.

* Removed the external C-based testers code in favor of the new built-in
  implementations.  The new approach feels significantly faster than the
  previous one.

* Fixed the handling of relative paths in the `fs.*` functions available
  in `Kyuafile`s.  All paths are now resolved relative to the location of
  the caller `Kyuafile`.  `Kyuafile.top` has been updated with these
  changes and you should update custom copies of this file with the new
  version.

* Changed temporary directory creation to always grant search
  permissions on temporary directories.  This is to prevent potential
  problems when running Kyua as root and executing test cases that require
  dropping privileges (as they may later be unable to use absolute paths
  that point inside their work directory).

* The cleanup of work directories does not longer attempt to deal with
  mount points.  If a test case mounts a file system and forgets to unmount
  it, the mount point will be left behind.  It is now the responsibility of
  the test case to clean after itself.  The reasons for this change are
  simplicity and clarity: there are many more things that a test case can
  do that have side-effects on the system and Kyua cannot protect against
  them all, so it is better to just have the test undo anything it might
  have done.

* Improved `kyua report --verbose` to properly handle environment
  variables with continuation lines in them, and fixed the integration
  tests for this command to avoid false negatives.

* Changed the configuration file format to accept the definition of
  unknown variables without declaring them local.  The syntax version
  number remains at 2.  This is to allow configuration files for newer Kyua
  versions to work on older Kyua versions, as there is no reason to forbid
  this.

* Fixed stacktrace gathering with FreeBSD's ancient version of GDB.
  GDB 6.1.1 (circa 2004) does not have the `-ex` flag so we need to
  generate a temporary GDB script and feed it to GDB with `-x` instead.

* Issue #136: Fixed the XML escaping in the JUnit output so that
  non-printable characters are properly handled when they appear in the
  process's stdout or stderr.

* Issue #141: Improved reporting of errors triggered by sqlite3.  In
  particular, all error messages are now tagged with their corresponding
  database filename and, if they are API-level errors, the name of the
  sqlite3 function that caused them.

* Issue #144: Improved documentation on the support for custom properties
  in the test metadata.

* Converted the `INSTALL`, `NEWS`, and `README` distribution documents to
  Markdown for better formatting online.


Changes in version 0.11
-----------------------

**Released on October 23rd, 2014.**

* Added support to print the details of all test cases (metadata and
  their output) to `report`.  This is via a new `--verbose` flag which
  replaces the previous `--show-context`.

* Added support to specify the amount of physical disk space required
  by a test case.  This is in the form of a new `required_disk_space`
  metadata property, which can also be provided by ATF test cases as
  `require.diskspace`.

* Assimilated the contents of all the `kyua-*-tester(1)` and
  `kyua-*-interface(7)` manual pages into more relevant places.  In
  particular, added more details on test program registration and their
  metadata to `kyuafile(5)`, and added `kyua-test-isolation(7)`
  describing the isolation features of the test execution.

* Assimilated the contents of all auxiliary manual pages, including
  `kyua-build-root(7)`, `kyua-results-files(7)`, `kyua-test-filters(7)`
  and `kyua-test-isolation(7)`, into the relevant command-specific
  manual pages.  This is for easier discoverability of relevant
  information when reading how specific Kyua commands work.

* Issue #30: Plumbed through support to query configuration variables
  from ATF's test case heads.  This resolves the confusing situation
  where test cases could only do this from their body and cleanup
  routines.

* Issue #49: Extended `report` to support test case filters as
  command-line arguments.  Combined with `--verbose`, this allows
  inspecting the details of a test case failure after execution.

* Issue #55: Deprecated support for specifying `test_suite` overrides on
  a test program basis.  This idiom should not be used but support for
  it remains in place.

* Issue #72: Added caching support to the `getcwd(3)` test in configure
  so that the result can be overriden for cross-compilation purposes.

* Issue #83: Changed manual page headings to include a `kyua` prefix in
  their name.  This prevents some possible confusion when displaying,
  for example, the `kyua-test` manual page with a plain name of `test`.

* Issue #84: Started passing test-suite configuration variables to plain
  and TAP test programs via the environment.  The name of the
  environment variables set this way is prefixed by `TEST_ENV_`, so a
  configuration variable of the form
  `test_suites.some_name.allow_unsafe_ops=yes` in `kyua.conf` becomes
  `TEST_ENV_allow_unsafe_ops=YES` in the environment.

* Issues #97 and #116: Fixed the build on Illumos.

* Issue #102: Set `TMPDIR` to the test case's work directory when running
  the test case.  If the test case happens to use the `mktemp(3)` family
  of functions (due to misunderstandings on how Kyua works or due to
  the reuse of legacy test code), we don't want it to easily escape the
  automanaged work directory.

* Issue #103: Started being more liberal in the parsing of TAP test
  results by treating the number in `ok` and `not ok` lines as optional.

* Issue #105: Started using tmpfs instead of md as a temporary file
  system for tests in FreeBSD so that we do not leak `md(4)` devices.

* Issue #109: Changed the privilege dropping code to start properly
  dropping group privileges when `unprivileged_user` is set.  Also fixes
  `testers/run_test:fork_wait__unprivileged_group`.

* Issue #110: Changed `help` to display version information and clarified
  the purpose of the `about` command in its documentation.

* Issue #111: Fixed crash when defining a test program in a `Kyuafile`
  that has not yet specified the test suite name.

* Issue #114: Improved the `kyuafile(5)` manual page by clarifying the
  restrictions of the `include()` directive and by adding abundant
  examples.


Changes in version 0.10
-----------------------

**Experimental version released on August 14th, 2014.**

* Merged `kyua-cli` and `kyua-testers` into a single `kyua` package.

* Dropped the `kyua-atf-compat` package.

* Issue #100: Do not try to drop privileges to `unprivileged_user` when we
  are already running as an unprivileged user.  Doing so is not possible
  and thus causes spurious test failures when the current user is not
  root and the current user and `unprivileged_user` do not match.

* Issue #79: Mention `kyua.conf(5)` in the *See also* section of `kyua(1)`.

* Issue #75: Change the `rewrite__expected_signal__bad_arg` test in
  `testers/atf_result_test` to use a different signal value.  This is to
  prevent triggering a core dump that made the test fail in some platforms.


Changes in kyua-cli version 0.9
-------------------------------

**Experimental version released on August 8th, 2014.**

Major changes:

The internal architecture of Kyua to record the results of test suite
runs has completely changed in this release.  Kyua no longer stores all
the different test suite run results as different "actions" within the
single `store.db` database.  Instead, Kyua now generates a separate
results file inside `~/.kyua/store/` for every test suite run.

Due to the complexity involved in the migration process and the little
need for it, this is probably going to be the only release where the
`db-migrate` command is able to convert an old `store.db` file to the
new scheme.

Changes in more detail:

* Added the `report-junit` command to generate JUnit XML result files.
  The output has been verified to work within Jenkins.

* Switched to results files specific to their corresponding test suite
  run.  The unified `store.db` file is now gone: `kyua test` creates a
  new results file for every invocation under `~/.kyua/store/` and the
  `kyua report*` commands are able to locate the latest file for a
  corresponding test suite automatically.

* The `db-migrate` command takes an old `store.db` file and generates
  one results file for every previously-recorded action, later deleting
  the `store.db` file.

* The `--action` flag has been removed from all commands that accepted
  it.  This has been superseded by the tests results files.

* The `--store` flag that many commands took has been renamed to
  `--results-file` in line with the semantical changes.

* The `db-exec` command no longer creates an empty database when none
  is found.  This command is now intended to run only over existing
  files.


Changes in kyua-testers version 0.3
-----------------------------------

**Experimental version released on August 8th, 2014.**

* Made the testers set a "sanitized" value for the `HOME` environment
  variable where, for example, consecutive and trailing slashes have
  been cleared.  Mac OS X has a tendency to append a trailing slash to
  the value of `TMPDIR`, which can cause third-party tests to fail if
  they compare `${HOME}` with `$(pwd)`.

* Issues #85, #86, #90 and #92: Made the TAP parser more complete: mark
  test cases reported as `TODO` or `SKIP` as passed; handle skip plans;
  ignore lines that look like `ok` and `not ok` but aren't results; and
  handle test programs that report a pass but exit with a non-zero code.


Changes in kyua-cli version 0.8
-------------------------------

**Experimental version released on December 7th, 2013.**

* Added support for Lutok 0.4.

* Issue #24: Plug the bootstrap tests back into the test suite.  Fixes
  in `kyua-testers` 0.2 to isolate test cases into their own sessions
  should allow these to run fine.

* Issue #74: Changed the `kyuafile(5)` parser to automatically discover
  existing tester interfaces.  The various `*_test_program()` functions
  will now exist (or not) based on tester availability, which simplifies
  the addition of new testers or the selective installation of them.


Changes in kyua-testers version 0.2
-----------------------------------

**Experimental version released on December 7th, 2013.**

* Issue #74: Added the `kyua-tap-tester`, a new backend to interact with
  test programs that comply with the Test Anything Protocol.

* Issue #69: Cope with the lack of `AM_PROG_AR` in `configure.ac`, which
  first appeared in Automake 1.11.2.  Fixes a problem in Ubuntu 10.04
  LTS, which appears stuck in 1.11.1.

* Issue #24: Improve test case isolation by confining the tests to their
  own session instead of just to their own process group.


Changes in kyua-cli version 0.7
-------------------------------

**Experimental version released on October 18th, 2013.**

* Made failures from testers more resilent.  If a tester fails, the
  corresponding test case will be marked as broken instead of causing
  kyua to exit.

* Added the `--results-filter` option to the `report-html` command and
  set its default value to skip passed results from HTML reports.  This
  is to keep these reports more succint and to avoid generating tons of
  detail files that will be, in general, useless.

* Switched to use Lutok 0.3 to gain compatibility with Lua 5.2.

* Issue #69: Cope with the lack of `AM_PROG_AR` in `configure.ac`, which
  first appeared in Automake 1.11.2.  Fixes a problem in Ubuntu 10.04
  LTS, which appears stuck in 1.11.1.


Changes in kyua-cli version 0.6
-------------------------------

**Experimental version released on February 22nd, 2013.**

* Issue #36: Changed `kyua help` to not fail when the configuration file
  is bogus.  Help should always work.

* Issue #37: Simplified the `syntax()` calls in configuration and
  `Kyuafile` files to only specify the requested version instead of also
  the format name.  The format name is implied by the file being loaded, so
  there is no use in the caller having to specify it.  The version number
  of these file formats has been bumped to 2.

* Issue #39: Added per-test-case metadata values to the HTML reports.

* Issue #40: Rewrote the documentation as manual pages and removed the
  previous GNU Info document.

* Issue #47: Started using the independent testers in the `kyua-testers`
  package to run the test cases.  Kyua does not implement the logic to
  invoke test cases any more, which provides for better modularity,
  extensibility and robustness.

* Issue #57: Added support to specify arbitrary metadata properties for
  test programs right from the `Kyuafile`.  This is to make plain test
  programs more versatile, by allowing them to specify any of the
  requirements (allowed architectures, required files, etc.) supported
  by Kyua.

* Reduced automatic screen line wrapping of messages to the `help`
  command and the output of tables by `db-exec`.  Wrapping any other
  messages (specially anything going to stderr) was very annoying
  because it prevented natural copy/pasting of text.

* Increased the granularity of the error codes returned by `kyua(1)` to
  denote different error conditions.  This avoids the overload of `1` to
  indicate both "expected" errors from specific subcommands and
  unexpected errors caused by the internals of the code.  The manual now
  correctly explain how the exit codes behave on a command basis.

* Optimized the database schema to make report generation almost
  instantaneous.

* Bumped the database schema to 2.  The database now records the
  metadata of both test programs and test cases generically, without
  knowledge of their interface.

* Added the `db-migrate` command to provide a mechanism to upgrade a
  database with an old schema to the current schema.

* Removed the GDB build-time configuration variable.  This is now part
  of the `kyua-testers` package.

* Issue #31: Rewrote the `Kyuafile` parsing code in C++, which results in
  a much simpler implementation.  As a side-effect, this gets rid of the
  external Lua files required by `kyua`, which in turn make the tool
  self-contained.

* Added caching of various configure test results (particularly in those
  tests that need to execute a test program) so that cross-compilers can
  predefine the results of the tests without having to run the
  executables.


Changes in kyua-testers version 0.1
-----------------------------------

**Experimental version released on February 19th, 2013.**

This is the first public release of the `kyua-testers` package.

The goal of this first release is to adopt all the test case execution
code of `kyua-cli` 0.5 and ship it as a collection of independent tester
binaries.  The `kyua-cli` package will rely on these binaries to run the
tests, which provides better modularity and simplicity to the
architecture of Kyua.

The code in this package is all C as opposed to the current C++ codebase
of `kyua-cli`, which means that the overall build times of Kyua are now
reduced.


Changes in kyua-cli version 0.5
-------------------------------

**Experimental version released on July 10th, 2012.**

* Issue #15: Added automatic stacktrace gathering of crashing test cases.
  This relies on GDB and is a best-effort operation.

* Issue #32: Added the `--build-root` option to the debug, list and test
  commands.  This allows executing test programs from a different
  directory than where the `Kyuafile` scripts live.  See the *Build roots*
  section in the manual for more details.

* Issue #33: Removed the `kyuaify.sh` script.  This has been renamed to
  atf2kyua and moved to the `kyua-atf-compat` module, where it ships as a
  first-class utility (with a manual page and tests).

* Issue #34: Changed the HTML reports to include the stdout and stderr of
  every test case.

* Fixed the build when using a "build directory" and a clean source tree
  from the repository.


Changes in kyua-cli version 0.4
-------------------------------

**Experimental version released on June 6th, 2012.**

* Added the `report-html` command to generate HTML reports of the
  execution of any recorded action.

* Changed the `--output` flag of the `report` command to only take a
  path to the target file, not its format.  Different formats are better
  supported by implementing different subcommands, as the options they
  may receive will vary from format to format.

* Added a `--with-atf` flag to the configure script to control whether
  the ATF tests get built or not.  May be useful for packaging systems
  that do not have ATF in them yet.  Disabling ATF also cuts down the
  build time of Kyua significantly, but with the obvious drawbacks.

* Grouped `kyua` subcommands by topic both in the output of `help` and
  in the documentation.  In general, the user needs to be aware of
  commands that rely on a current project and those commands that rely
  purely on the database to generate reports.

* Made `help` print the descriptions of options and commands properly
  tabulated.

* Changed most informational messages to automatically wrap on screen
  boundaries.

* Rewrote the configuration file parsing module for extensibility.  This
  will allow future versions of Kyua to provide additional user-facing
  options in the configuration file.

  No syntax changes have been made, so existing configuration files
  (version 1) will continue to be parsed without problems.  There is one
  little exception though: all variables under the top-level
  `test_suites` tree must be declared as strings.

  Similarly, the `-v` and `--variable` flags to the command line must
  now carry a `test_suites.` prefix when referencing any variables under
  such tree.


Changes in kyua-cli version 0.3
-------------------------------

**Experimental version released on February 24th, 2012.**

* Made the `test` command record the results of the executed test
  cases into a SQLite database.  As a side effect, `test` now supports a
  `--store` option to indicate where the database lives.

* Added the `report` command to generate plain-text reports of the
  test results stored in the database.  The interface of this command is
  certainly subject to change at this point.

* Added the `db-exec` command to directly interact with the store
  database.

* Issue #28: Added support for the `require.memory` test case property
  introduced in ATF 0.15.

* Renamed the user-specific configuration file from `~/.kyuarc` to
  `~/.kyua/kyua.conf` for consistency with other files stored in the
  `~/.kyua/` subdirectory.

* Switched to use Lutok instead of our own wrappers over the Lua C
  library.  Lutok is just what used to be our own utils::lua module, but
  is now distributed separately.

* Removed the `Atffile`s from the source tree.  Kyua is stable enough
  to generate trustworthy reports, and we do not want to give the
  impression that atf-run / atf-report are still supported.

* Enabled logging to stderr for our own test programs.  This makes it
  slightly easier to debug problems in our own code when we get a
  failing test.


Changes in kyua-cli version 0.2
-------------------------------

**Experimental version released on August 24th, 2011.**

The biggest change in this release is the ability for Kyua to run test
programs implemented using different frameworks.  What this means is
that, now, a Kyua test suite can include not only ATF-based test
programs, but also "legacy" (aka plain) test programs that do not use
any framework.  I.e. if you have tests that are simple programs that
exit with 0 on success and 1 on failure, you can plug them in into a
Kyua test suite.

Other than this, there have been several user-visible changes.  The most
important are the addition of the new `config` and `debug` subcommands
to the `kyua` binary.  The former can be used to inspect the runtime
configuration of Kyua after parsing, and the latter is useful to
interact with failing tests cases in order to get more data about the
failure itself.

Without further ado, here comes the itemized list of changes:

* Generalized the run-time engine to support executing test programs
  that implement different interfaces.  Test programs that use the ATF
  libraries are just a special case of this.  (Issue #18.)

* Added support to the engine to run `plain` test programs: i.e. test
  programs that do not use any framework and report their pass/fail
  status as an exit code.  This is to simplify the integration of legacy
  test programs into a test suite, and also to demonstrate that the
  run-time engine is generic enough to support different test
  interfaces.  (Issue #18.)

* Added the `debug` subcommand.  This command allows end users to tweak
  the execution of a specific test case and to poke into the behavior of
  its execution.  At the moment, all this command allows is to view the
  stdout and stderr of the command in real time (which the `test`
  command currently completely hides).

* Added the `config` subcommand.  This command allows the end user to
  inspect the current configuration variables after evaluation, without
  having to read through configuration files.  (Issue #11.)

* Removed the `test_suites_var` function from configuration files.  This
  was used to set the value of test-suite-sepecific variables, but it
  was ugly-looking.  It is now possible to use the more natural syntax
  `test_suites.<test-suite-name>.<variable> = <value>`.  (Issue #11.)

* Added a mechanism to disable the loading of configuration files
  altogether.  Needed for testing purposes and for scriptability.
  Available by passing the `--config=none` flag.

* Enabled detection of unused parameters and variables in the code and
  fixed all warnings.  (Issue #23.)

* Changed the behavior of "developer mode".  Compiler warnings are now
  enabled unconditionally regardless of whether we are in developer mode
  or not; developer mode is now only used to perform strict warning
  checks and to enable assertions.  Additionally, developer mode is now
  only automatically enabled when building from the repository, not for
  formal releases.  (Issue #22.)

* Fixed many build and portability problems to Debian sid with GCC 4.6.3
  and Ubuntu 10.04.1 LTS.  (Issues #20, #21, #26.)


Changes in kyua-cli version 0.1
-------------------------------

**Experimental version released on June 23rd, 2011.**

This is the first public release of the `kyua-cli` package.

The scope of this release is to provide functional replacement for the
`atf-run` utility included in the atf package.  At this point, `kyua`
can reliably run the NetBSD 5.99.53 test suite delivering the same
results as `atf-run`.

The reporting facilities of this release are quite limited.  There is
no replacement for `atf-report` yet, and there is no easy way of
debugging failing test programs other than running them by hand.  These
features will mark future milestones and therefore be part of other
releases.

Be aware that this release has suffered very limited field testing.
The test suite for `kyua-cli` is quite comprehensive, but some bugs may
be left in any place.
