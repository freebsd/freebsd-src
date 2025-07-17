# News

## 7.0.2

This is a production release that fixes `Ctrl+d` on FreeBSD and Linux when using
`editline`.

This bug was caused by the macOS fix in `7.0.0`. Unfortunately, this means that
macOS does not respond properly to `Ctrl+d`.

## 7.0.1

This is a production release that fixes a warning using GCC on FreeBSD.

Other users do ***NOT*** need to upgrade.

## 7.0.0

This is a production release to fix three bugs.

The first bug is that `bc`/`dc` will exit on macOS when the terminal is resized.

The second bug is that an array, which should only be a function parameter, was
accepted as part of larger expressions.

The third bug is that value stack for `dc` was cleared on any error. However,
this is not how other `dc` behave. To bring `dc` more in line with other
implementations, this behavior was changed. This change is why this version is a
new major version.

## 6.7.6

This is a production release to fix one bug.

The bug was that `bc` attempted to jump out when flushing `stdout` on exit, but
there is no jump buf at that point.

## 6.7.5

This is a production release to fix one small bug.

The bug is that sometimes numbers are printed to incorrect line lengths. The
number is always correct; the line is just longer than the limit.

Users who do not care do not need to update.

## 6.7.4

This is a production release to fix problems in the `bc` manual.

Users only need to update if desired.

## 6.7.3

This is a production release to fix the library build on Mac OSX.

Users on other platforms do *not* need to update.

## 6.7.2

This is a production release to remove some debugging code that I accidentally
committed.

## 6.7.1

This is a production release with a bug fix for `SIGINT` only being handled
once.

## 6.7.0

This is a production release with three new functions in the [extended math
library][16]: `min()`, `max()`, and `i2rand()`.

## 6.6.1

This is a production release with an improved `p()` function in the [extended
math library][16].

Users who don't care do not need to upgrade.

## 6.6.0

This is a production release with two bug fixes and one change.

The first bug fix is to fix the build on Mac OSX.

The second bug was to remove printing a leading zero in scientific or
engineering output modes.

The change was that the implementation of `irand()` was improved to call the
PRNG less.

## 6.5.0

This is a production release that fixes an infinite loop bug in `root()` and
`cbrt()`, fixes a bug with `BC_LINE_LENGTH=0`, and adds the `fib()` function to
the extended math library to calculate Fibonacci numbers.

## 6.4.0

This is a production release that fixes a `read()`/`?` bug and adds features to
`bcl`.

The bug was that multiple read calls could repeat old data.

The new features in `bcl` are functions to preserve `BclNumber` arguments and
not free them.

***WARNING for `bcl` Users***: The `bcl_rand_seedWithNum()` function used to not
consume its arguments. Now it does. This change could have made this version
`7.0.0`, but I'm 99.9% confident that there are no `bcl` users, or if there are,
they probably don't use the PRNG. So I took a risk and didn't update the major
version.

`bcl` now includes more capacity to check for invalid numbers when built to run
under Valgrind.

## 6.3.1

This is a production release that fixes a `bc` dependency loop for minimal
environments and Linux from Scratch.

## 6.3.0

This is a production release with a couple of fixes for manuals and a new
feature for `dc`: there is now a command to query whether extended registers are
enabled or not.

Users who don't care do not need to upgrade.

## 6.2.6

This is a production release that fixes an install bug that affected locale
installation of all locales when using `mksh`. Users do ***NOT*** need to
upgrade if they don't use `mksh` and/or don't need to install all locales.

## 6.2.5

This is a production release that fixes a test bug that affected Android and
`mksh`. Users do ***NOT*** need to upgrade unless they use `mksh` or another
affected shell and need to run the test suite.

## 6.2.4

This is a production release that fixes a test failure that happens when
`tests/bc/scripts/timeconst.bc` doesn't exist. This should only affect
packagers.

This bug happened because I forgot something I added in the previous release:
better error checking in tests to help packagers. Unfortunately, I was too
zealous with the error checking.

## 6.2.3

This is a production release that moves `bc` to <https://git.gavinhoward.com>.

That's all it does: update links. Users do ***NOT*** need to upgrade; there are
redirects that will stay in place indefinitely. This release is only for new
users.

## 6.2.2

This is a production release that fixes a bug.

The bug was that if an array element was used as a parameter, and then a later
parameter had the same name as the array whose element was used, `bc` would grab
the element from the new array parameter, not the actual element from before the
function call.

## 6.2.1

This is a production release with one bug fix for a memory bug in history.

## 6.2.0

This is a production release with a new feature and a few bug fixes.

The bug fixes include:

* A crash when `bc` and `dc` are built using editline, but history is not
  activated.
* A missing local in the `uint*()` family of functions in the extended math
  library.
* A failure to clear the tail call list in `dc` on error.
* A crash when attempting to swap characters in command-line history when no
  characters exist.
* `SIGWINCH` was activated even when history was not.

The new feature is that stack traces are now given for runtime errors. In debug
mode, the C source file and line of errors are given as well.

## 6.1.1

This is a production release that fixes a build issue with predefined builds and
generated tests.

## 6.1.0

This is a production release that fixes a discrepancy from the `bc` standard,
a couple of memory bugs, and adds new features.

The discrepancy from the `bc` standard was with regards to the behavior of the
`quit` command. This `bc` used to quit whenever it encountered `quit` during
parsing, even if it was parsing a full file. Now, `bc` only quits when
encountering `quit` *after* it has executed all executable statements up to that
point.

This behavior is slightly different from GNU `bc`, but users will only notice
the difference if they put `quit` on the same line as other statements.

The first memory bug could be reproduced by assigning a string to a non-local
variable in a function, then redefining the function with use of the same
non-local variable, which would still refer to a string in the previous version
of the function.

The second memory bug was caused by passing an array argument to the `asciify()`
built-in function. In certain cases, that was wrongly allowed, and the
interpreter just assumed everything was correct and accessed memory. Now that
arrays are allowed as arguments (see below), this is not an issue.

The first feature was the addition of the `is_number()` built-in function (`u`
in `dc`) that returns 1 if the runtime argument is a number and 0 otherwise.

The second feature was the addition of the `is_string()` built-in function (`t`
in `dc`) that returns 1 if the runtime argument is a string and 0 otherwise.

These features were added because I realized that type-checking is necessary now
that strings can be assigned to variables in `bc` and because they've always
been assignable to variables in `dc`.

The last added feature is the ability of the `asciify()` built-in function in
`bc` to convert a full array of numbers into a string. This means that
character-by-character printing will not be necessary, and more strings than
just single-character ones will be able to be created.

## 6.0.4

This is a production release that most users will not need to upgrade to.

This fixes a build bug for `bcl` only on OpenBSD. Users that do not need `bcl`
or have not run into build errors with `bcl` do ***NOT*** need to upgrade.

## 6.0.3

This is a production release that fixes a build bug for cross-compilation.

Users that do not need cross-compilation do ***NOT*** need to upgrade.

## 6.0.2

This is a production release that fixes two bugs:

* The `-l` option overrode the `-S` option.
* A double-free and crash when sending a `SIGINT` while executing expressions
  given on the command-line.

## 6.0.1

This is a production release that fixes memory bugs and memory leaks in `bcl`.

Users that do not use `bcl` (use only `bc` and/or `dc`) do ***NOT*** need to
upgrade.

These happened because I was unaware that the `bcl` test was not hooked into the
Valgrind test infrastructure. Then, when I ran the release script, which tests
everything under Valgrind (or so I thought), it caught nothing, and I thought it
was safe.

But it was not.

Nevertheless, I have now run it under Valgrind and fixed all of the memory bugs
(caused by not using `memset()` where I should have but previously didn't have
to) and memory leaks.

## 6.0.0

This is a production release that fixes an oversight in the `bc` parser (that
sometimes caused the wrong error message) and adds a feature for compatibility
with the BSD `bc` and `dc`: turning off digit clamping when parsing numbers.

The default for clamping can be set during the build (see the [build
manual][13]), it can be set with the `BC_DIGIT_CLAMP` and `DC_DIGIT_CLAMP`
environment variables, and it can be set with the `-c` and `-C` command-line
options.

Turning off clamping was also added to the `bcl` library.

In addition, signal handling was removed from the `bcl` library in order to add
the capability for multi-threading. This required a major version bump. I
apologize to all library users (I don't know of any), but signals and threads do
not play well together.

To help with building, a convenience option (`-p`) to `configure.sh` was added
to build a `bc` and `dc` that is by default compatible with either the BSD `bc`
and `dc` or the GNU `bc` and `dc`.

## 5.3.3

This is a production release that fixes a build problem in the FreeBSD base
system.

All other users do **NOT** need to upgrade.

## 5.3.2

This is a production release that fixes prompt bugs with editline and readline
where the `BC_PROMPT` environment variable was not being respected.

This also fixes editline and readline output on `EOF`.

## 5.3.1

This is a production release that fixes a build problem in the FreeBSD base
system, as well as a problem in the `en_US` locale. If you don't have problems
with either, you do not need to upgrade.

## 5.3.0

This is a production release that adds features and has a few bug fixes.

First, support for editline and readline history has been added. To use
editline, pass `-e` to `configure.sh`, and to use readline, pass `-r`.

Second, history support for Windows has been fixed and re-enabled.

Third, command-line options to set `scale`, `ibase`, `obase`, and `seed` were
added. This was requested long ago, and I originally disagreed with the idea.

Fourth, the manuals had typos and were missing information. That has been fixed.

Fifth, the manuals received different formatting to be more readable as
manpages.

## 5.2.5

This is a production release that fixes this `bc`'s behavior on `^D` to match
GNU `bc`.

## 5.2.4

This is a production release that fixes two bugs in history:

* Without prompt, the cursor could not be placed on the first character in a
  line.
* Home and End key handling in `tmux` was fixed.

Any users that do not care about these improvements do not need to upgrade.

## 5.2.3

This is a production release that fixes one bug, a parse error when passing a
file to `bc` using `-f` if that file had a multiline comment or string in it.

## 5.2.2

This is a production release that fixes one bug, a segmentation fault if
`argv[0]` equals `NULL`.

This is not a critical bug; there will be no vulnerability as far as I can tell.
There is no need to update if you do not wish to.

## 5.2.1

This is a production release that fixes two parse bugs when in POSIX standard
mode. One of these bugs was due to a quirk of the POSIX grammar, and the other
was because `bc` was too strict.

## 5.2.0

This is a production release that adds a new feature, fixes some bugs, and adds
out-of-source builds and a `pkg-config` file for `bcl`.

The new feature is the ability to turn off exiting on expressions. It is also
possible to set the default using `configure.sh`. This behavior used to exist
with the `BC_EXPR_EXIT` environment variable, which is now used again.

Bugs fixed include:

* Some possible race conditions with error handling.
* Install and uninstall targets for `bcl` did not work.

## 5.1.1

This is a production release that completes a bug fix from `5.1.0`. The bug
exists in all versions of `bc`.

The bug was that `if` statements without `else` statements would not be handled
correctly at the end of files or right before a function definition.

## 5.1.0

This is a production release with some fixes and new features.

* Fixed a bug where an `if` statement without an `else` before defining a
  function caused an error.
* Fixed a bug with the `bc` banner and `-q`.
* Fixed a bug on Windows where files were not read correctly.
* Added a command-line flag (`-z`) to make `bc` and `dc` print leading zeroes on
  numbers `-1 < x < 1`.
* Added four functions to `lib2.bc` (`plz()`, `plznl()`, `pnlz()`, and
  `pnlznl()`) to allow printing numbers with or without leading zeros, despite
  the use of `-z` or not.
* Added builtin functions to query global state like line length, global stacks,
  and leading zeroes.
* Added a command-line flag (`-L`) to disable wrapping when printing numbers.
* Improved builds on Windows.

## 5.0.2

This is a production release with one fix for a flaky test. If you have not
experienced problems with the test suite, you do ***NOT*** need to upgrade.

The test was one that tested whether `bc` fails gracefully when it can't
allocate memory. Unfortunately, there are cases when Linux and FreeBSD lie and
pretend to allocate the memory.

The reason they do this is because a lot of programs don't use all of the memory
they allocate, so those OS's usually get away with it.

However, this `bc` uses all of the memory it allocates (at least at page
granularity), so when it tries to use the memory, FreeBSD and Linux kill it.

This only happens sometimes, however. Other times (on my machine), they do, in
fact, refuse the request.

So I changed the test to not test for that because I think the graceful failure
code won't really change much.

## 5.0.1

This is a production release with two fixes:

* Fix for the build on Mac OSX.
* Fix for the build on Android.

Users that do not use those platforms do ***NOT*** need to update.

## 5.0.0

This is a major production release with several changes:

* Added support for OpenBSD's `pledge()` and `unveil()`.
* Fixed print bug where a backslash newline combo was printed even if only one
  digit was left, something I blindly copied from GNU `bc`, like a fool.
* Fixed bugs in the manuals.
* Fixed a possible multiplication overflow in power.
* Temporary numbers are garbage collected if allocation fails, and the
  allocation is retried. This is to make `bc` and `dc` more resilient to running
  out of memory.
* Limited the number of temporary numbers and made the space for them static so
  that allocating more space for them cannot fail.
* Allowed integers with non-zero `scale` to be used with power, places, and
  shift operators.
* Added greatest common divisor and least common multiple to `lib2.bc`.
* Added `SIGQUIT` handling to history.
* Added a command to `dc` (`y`) to get the length of register stacks.
* Fixed multi-digit bugs in `lib2.bc`.
* Removed the no prompt build option.
* Created settings that builders can set defaults for and users can set their
  preferences for. This includes the `bc` banner, resetting on `SIGINT`, TTY
  mode, and prompt.
* Added history support to Windows.
* Fixed bugs with the handling of register names in `dc`.
* Fixed bugs with multi-line comments and strings in both calculators.
* Added a new error type and message for `dc` when register stacks don't have
  enough items.
* Optimized string allocation.
* Made `bc` and `dc` UTF-8 capable.
* Fixed a bug with `void` functions.
* Fixed a misspelled symbol in `bcl`. This is technically a breaking change,
  which requires this to be `5.0.0`.
* Added the ability for users to get the copyright banner back.
* Added the ability for users to have `bc` and `dc` quit on `SIGINT`.
* Added the ability for users to disable prompt and TTY mode by environment
  variables.
* Added the ability for users to redefine keywords. This is another reason this
  is `5.0.0`.
* Added `dc`'s modular exponentiation and divmod to `bc`.
* Added the ability to assign strings to variables and array elements and pass
  them to functions in `bc`.
* Added `dc`'s asciify command and stream printing to `bc`.
* Added a command to `dc` (`Y`) to get the length of an array.
* Added a command to `dc` (`,`) to get the depth of the execution stack.
* Added bitwise and, or, xor, left shift, right shift, reverse, left rotate,
  right rotate, and mod functions to `lib2.bc`.
* Added the functions `s2u(x)` and `s2un(x,n)`, to `lib2.bc`.

## 4.0.2

This is a production release that fixes two bugs:

1.	If no files are used and the first statement on `stdin` is invalid, `scale`
	would not be set to `20` even if `-l` was used.
2.	When using history, `bc` failed to respond properly to `SIGSTOP` and
	`SIGTSTP`.

## 4.0.1

This is a production release that only adds one thing: flushing output when it
is printed with a print statement.

## 4.0.0

This is a production release with many fixes, a new command-line option, and a
big surprise:

* A bug was fixed in `dc`'s `P` command where the item on the stack was *not*
  popped.
* Various bugs in the manuals have been fixed.
* A known bug was fixed where history did not interact well with prompts printed
  by user code without newlines.
* A new command-line option, `-R` and `--no-read-prompt` was added to disable
  just the prompt when using `read()` (`bc`) or `?` (`dc`).
* And finally, **official support for Windows was added**.

The last item is why this is a major version bump.

Currently, only one set of build options (extra math and prompt enabled, history
and NLS/locale support disabled, both calculators enabled) is supported on
Windows. However, both debug and release builds are supported.

In addition, Windows builds are supported for the the library (`bcl`).

For more details about how to build on Windows, see the [README][5] or the
[build manual][13].

## 3.3.4

This is a production release that fixes a small bug.

The bug was that output was not flushed before a `read()` call, so prompts
without a newline on the end were not flushed before the `read()` call.

This is such a tiny bug that users only need to upgrade if they are affected.

## 3.3.3

This is a production release with one tweak and fixes for manuals.

The tweak is that `length(0)` returns `1` instead of `0`. In `3.3.1`, I changed
it so `length(0.x)`, where `x` could be any number of digits, returned the
`scale`, but `length(0)` still returned `0` because I believe that `0` has `0`
significant digits.

After request of FreeBSD and considering the arguments of a mathematician,
compatibility with other `bc`'s, and the expectations of users, I decided to
make the change.

The fixes for manuals fixed a bug where `--` was rendered as `-`.

## 3.3.2

This is a production release that fixes a divide-by-zero bug in `root()` in the
[extended math library][16]. All previous versions with `root()` have the bug.

## 3.3.1

This is a production release that fixes a bug.

The bug was in the reporting of number length when the value was 0.

## 3.3.0

This is a production release that changes one behavior and fixes documentation
bugs.

The changed behavior is the treatment of `-e` and `-f` when given through
`BC_ENV_ARGS` or `DC_ENV_ARGS`. Now `bc` and `dc` do not exit when those options
(or their equivalents) are given through those environment variables. However,
`bc` and `dc` still exit when they or their equivalents are given on the
command-line.

## 3.2.7

This is a production release that removes a small non-portable shell operation
in `configure.sh`. This problem was only noticed on OpenBSD, not FreeBSD or
Linux.

Non-OpenBSD users do ***NOT*** need to upgrade, although NetBSD users may also
need to upgrade.

## 3.2.6

This is a production release that fixes the build on FreeBSD.

There was a syntax error in `configure.sh` that the Linux shell did not catch,
and FreeBSD depends on the existence of `tests/all.sh`.

All users that already upgraded to `3.2.5` should update to this release, with
my apologies for the poor release of `3.2.5`. Other users should skip `3.2.5` in
favor of this version.

## 3.2.5

This is a production release that fixes several bugs and adds a couple small
things.

The two most important bugs were bugs that causes `dc` to access memory
out-of-bounds (crash in debug builds). This was found by upgrading to `afl++`
from `afl`. Both were caused by a failure to distinguish between the same two
cases.

Another bug was the failure to put all of the licenses in the `LICENSE.md` file.

Third, some warnings by `scan-build` were found and eliminated. This needed one
big change: `bc` and `dc` now bail out as fast as possible on fatal errors
instead of unwinding the stack.

Fourth, the pseudo-random number now attempts to seed itself with `/dev/random`
if `/dev/urandom` fails.

Finally, this release has a few quality-of-life changes to the build system. The
usage should not change at all; the only thing that changed was making sure the
`Makefile.in` was written to rebuild properly when headers changed and to not
rebuild when not necessary.

## 3.2.4

This is a production release that fixes a warning on `gcc` 6 or older, which
does not have an attribute that is used.

Users do ***NOT*** need to upgrade if they don't use `gcc` 6 or older.

## 3.2.3

This is a production release that fixes a bug in `gen/strgen.sh`. I recently
changed `gen/strgen.c`, but I did not change `gen/strgen.sh`.

Users that do not use `gen/strgen.sh` do not need to upgrade.

## 3.2.2

This is a production release that fixes a portability bug in `configure.sh`. The
bug was using the GNU `find` extension `-wholename`.

## 3.2.1

This is a production release that has one fix for `bcl(3)`. It is technically
not a bug fix since the behavior is undefined, but the `BclNumber`s that
`bcl_divmod()` returns will be set to `BCL_ERROR_INVALID_NUM` if there is an
error. Previously, they were not set.

## 3.2.0

This is a production release that has one bug fix and a major addition.

The bug fix was a missing `auto` variable in the bessel `j()` function in the
math library.

The major addition is a way to build a version of `bc`'s math code as a library.
This is done with the `-a` option to `configure.sh`. The API for the library can
be read in `./manuals/bcl.3.md` or `man bcl` once the library is installed with
`make install`.

This library was requested by developers before I even finished version 1.0, but
I could not figure out how to do it until now.

If the library has API breaking changes, the major version of `bc` will be
incremented.

## 3.1.6

This is a production release that fixes a new warning from Clang 12 for FreeBSD
and also removes some possible undefined behavior found by UBSan that compilers
did not seem to take advantage of.

Users do ***NOT*** need to upgrade, if they do not want to.

## 3.1.5

This is a production release that fixes the Chinese locales (which caused `bc`
to crash) and a crash caused by `bc` executing code when it should not have been
able to.

***ALL USERS SHOULD UPGRADE.***

## 3.1.4

This is a production release that fixes one bug, changes two behaviors, and
removes one environment variable.

The bug is like the one in the last release except it applies if files are being
executed. I also made the fix more general.

The behavior that was changed is that `bc` now exits when given `-e`, `-f`,
`--expression` or `--file`. However, if the last one of those is `-f-` (using
`stdin` as the file), `bc` does not exit. If `-f-` exists and is not the last of
the `-e` and `-f` options (and equivalents), `bc` gives a fatal error and exits.

Next, I removed the `BC_EXPR_EXIT` and `DC_EXPR_EXIT` environment variables
since their use is not needed with the behavior change.

Finally, I made it so `bc` does not print the header, though the `-q` and
`--quiet` options were kept for compatibility with GNU `bc`.

## 3.1.3

This is a production release that fixes one minor bug: if `bc` was invoked like
the following, it would error:

```
echo "if (1 < 3) 1" | bc
```

Unless users run into this bug, they do not need to upgrade, but it is suggested
that they do.

## 3.1.2

This is a production release that adds a way to install *all* locales. Users do
***NOT*** need to upgrade.

For package maintainers wishing to make use of the change, just pass `-l` to
`configure.sh`.

## 3.1.1

This is a production release that adds two Spanish locales. Users do ***NOT***
need to upgrade, unless they want those locales.

## 3.1.0

This is a production release that adjusts one behavior, fixes eight bugs, and
improves manpages for FreeBSD. Because this release fixes bugs, **users and
package maintainers should update to this version as soon as possible**.

The behavior that was adjusted was how code from the `-e` and `-f` arguments
(and equivalents) were executed. They used to be executed as one big chunk, but
in this release, they are now executed line-by-line.

The first bug fix in how output to `stdout` was handled in `SIGINT`. If a
`SIGINT` came in, the `stdout` buffer was not correctly flushed. In fact, a
clean-up function was not getting called. This release fixes that bug.

The second bug is in how `dc` handled input from `stdin`. This affected `bc` as
well since it was a mishandling of the `stdin` buffer.

The third fixed bug was that `bc` and `dc` could `abort()` (in debug mode) when
receiving a `SIGTERM`. This one was a race condition with pushing and popping
items onto and out of vectors.

The fourth bug fixed was that `bc` could leave extra items on the stack and
thus, not properly clean up some memory. (The memory would still get
`free()`'ed, but it would not be `free()`'ed when it could have been.)

The next two bugs were bugs in `bc`'s parser that caused crashes when executing
the resulting code.

The last two bugs were crashes in `dc` that resulted from mishandling of
strings.

The manpage improvement was done by switching from [ronn][20] to [Pandoc][21] to
generate manpages. Pandoc generates much cleaner manpages and doesn't leave
blank lines where they shouldn't be.

## 3.0.3

This is a production release that adds one new feature: specific manpages.

Before this release, `bc` and `dc` only used one manpage each that referred to
various build options. This release changes it so there is one manpage set per
relevant build type. Each manual only has information about its particular
build, and `configure.sh` selects the correct set for install.

## 3.0.2

This is a production release that adds `utf8` locale symlinks and removes an
unused `auto` variable from the `ceil()` function in the [extended math
library][16].

Users do ***NOT*** need to update unless they want the locales.

## 3.0.1

This is a production release with two small changes. Users do ***NOT*** need to
upgrade to this release; however, if they haven't upgraded to `3.0.0` yet, it
may be worthwhile to upgrade to this release.

The first change is fixing a compiler warning on FreeBSD with strict warnings
on.

The second change is to make the new implementation of `ceil()` in `lib2.bc`
much more efficient.

## 3.0.0

*Notes for package maintainers:*

*First, the `2.7.0` release series saw a change in the option parsing. This made
me change one error message and add a few others. The error message that was
changed removed one format specifier. This means that `printf()` will seqfault
on old locale files. Unfortunately, `bc` cannot use any locale files except the
global ones that are already installed, so it will use the previous ones while
running tests during install. **If `bc` segfaults while running arg tests when
updating, it is because the global locale files have not been replaced. Make
sure to either prevent the test suite from running on update or remove the old
locale files before updating.** (Removing the locale files can be done with
`make uninstall` or by running the [`locale_uninstall.sh`][22] script.) Once
this is done, `bc` should install without problems.*

*Second, **the option to build without signal support has been removed**. See
below for the reasons why.*

This is a production release with some small bug fixes, a few improvements,
three major bug fixes, and a complete redesign of `bc`'s error and signal
handling. **Users and package maintainers should update to this version as soon
as possible.**

The first major bug fix was in how `bc` executed files. Previously, a whole file
was parsed before it was executed, but if a function is defined *after* code,
especially if the function definition was actually a redefinition, and the code
before the definition referred to the previous function, this `bc` would replace
the function before executing any code. The fix was to make sure that all code
that existed before a function definition was executed.

The second major bug fix was in `bc`'s `lib2.bc`. The `ceil()` function had a
bug where a `0` in the decimal place after the truncation position, caused it to
output the wrong numbers if there was any non-zero digit after.

The third major bug is that when passing parameters to functions, if an
expression included an array (not an array element) as a parameter, it was
accepted, when it should have been rejected. It is now correctly rejected.

Beyond that, this `bc` got several improvements that both sped it up, improved
the handling of signals, and improved the error handling.

First, the requirements for `bc` were pushed back to POSIX 2008. `bc` uses one
function, `strdup()`, which is not in POSIX 2001, and it is in the X/Open System
Interfaces group 2001. It is, however, in POSIX 2008, and since POSIX 2008 is
old enough to be supported anywhere that I care, that should be the requirement.

Second, the `BcVm` global variable was put into `bss`. This actually slightly
reduces the size of the executable from a massive code shrink, and it will stop
`bc` from allocating a large set of memory when `bc` starts.

Third, the default Karatsuba length was updated from 64 to 32 after making the
optimization changes below, since 32 is going to be better than 64 after the
changes.

Fourth, Spanish translations were added.

Fifth, the interpreter received a speedup to make performance on non-math-heavy
scripts more competitive with GNU `bc`. While improvements did, in fact, get it
much closer (see the [benchmarks][19]), it isn't quite there.

There were several things done to speed up the interpreter:

First, several small inefficiencies were removed. These inefficiencies included
calling the function `bc_vec_pop(v)` twice instead of calling
`bc_vec_npop(v, 2)`. They also included an extra function call for checking the
size of the stack and checking the size of the stack more than once on several
operations.

Second, since the current `bc` function is the one that stores constants and
strings, the program caches pointers to the current function's vectors of
constants and strings to prevent needing to grab the current function in order
to grab a constant or a string.

Third, `bc` tries to reuse `BcNum`'s (the internal representation of
arbitary-precision numbers). If a `BcNum` has the default capacity of
`BC_NUM_DEF_SIZE` (32 on 64-bit and 16 on 32-bit) when it is freed, it is added
to a list of available `BcNum`'s. And then, when a `BcNum` is allocated with a
capacity of `BC_NUM_DEF_SIZE` and any `BcNum`'s exist on the list of reusable
ones, one of those ones is grabbed instead.

In order to support these changes, the `BC_NUM_DEF_SIZE` was changed. It used to
be 16 bytes on all systems, but it was changed to more closely align with the
minimum allocation size on Linux, which is either 32 bytes (64-bit musl), 24
bytes (64-bit glibc), 16 bytes (32-bit musl), or 12 bytes (32-bit glibc). Since
these are the minimum allocation sizes, these are the sizes that would be
allocated anyway, making it worth it to just use the whole space, so the value
of `BC_NUM_DEF_SIZE` on 64-bit systems was changed to 32 bytes.

On top of that, at least on 64-bit, `BC_NUM_DEF_SIZE` supports numbers with
either 72 integer digits or 45 integer digits and 27 fractional digits. This
should be more than enough for most cases since `bc`'s default `scale` values
are 0 or 20, meaning that, by default, it has at most 20 fractional digits. And
45 integer digits are *a lot*; it's enough to calculate the amount of mass in
the Milky Way galaxy in kilograms. Also, 72 digits is enough to calculate the
diameter of the universe in Planck lengths.

(For 32-bit, these numbers are either 32 integer digits or 12 integer digits and
20 fractional digits. These are also quite big, and going much bigger on a
32-bit system seems a little pointless since 12 digits is just under a trillion
and 20 fractional digits is still enough for about any use since `10^-20` light
years is just under a millimeter.)

All of this together means that for ordinary uses, and even uses in scientific
work, the default number size will be all that is needed, which means that
nearly all, if not all, numbers will be reused, relieving pressure on the system
allocator.

I did several experiments to find the changes that had the most impact,
especially with regard to reusing `BcNum`'s. One was putting `BcNum`'s into
buckets according to their capacity in powers of 2 up to 512. That performed
worse than `bc` did in `2.7.2`. Another was putting any `BcNum` on the reuse
list that had a capacity of `BC_NUM_DEF_SIZE * 2` and reusing them for `BcNum`'s
that requested `BC_NUM_DEF_SIZE`. This did reduce the amount of time spent, but
it also spent a lot of time in the system allocator for an unknown reason. (When
using `strace`, a bunch more `brk` calls showed up.) Just reusing `BcNum`'s that
had exactly `BC_NUM_DEF_SIZE` capacity spent the smallest amount of time in both
user and system time. This makes sense, especially with the changes to make
`BC_NUM_DEF_SIZE` bigger on 64-bit systems, since the vast majority of numbers
will only ever use numbers with a size less than or equal to `BC_NUM_DEF_SIZE`.

Last of all, `bc`'s signal handling underwent a complete redesign. (This is the
reason that this version is `3.0.0` and not `2.8.0`.) The change was to move
from a polling approach to signal handling to an interrupt-based approach.

Previously, every single loop condition had a check for signals. I suspect that
this could be expensive when in tight loops.

Now, the signal handler just uses `longjmp()` (actually `siglongjmp()`) to start
an unwinding of the stack until it is stopped or the stack is unwound to
`main()`, which just returns. If `bc` is currently executing code that cannot be
safely interrupted (according to POSIX), then signals are "locked." The signal
handler checks if the lock is taken, and if it is, it just sets the status to
indicate that a signal arrived. Later, when the signal lock is released, the
status is checked to see if a signal came in. If so, the stack unwinding starts.

This design eliminates polling in favor of maintaining a stack of `jmp_buf`'s.
This has its own performance implications, but it gives better interaction. And
the cost of pushing and popping a `jmp_buf` in a function is paid at most twice.
Most functions do not pay that price, and most of the rest only pay it once.
(There are only some 3 functions in `bc` that push and pop a `jmp_buf` twice.)

As a side effect of this change, I had to eliminate the use of `stdio.h` in `bc`
because `stdio` does not play nice with signals and `longjmp()`. I implemented
custom I/O buffer code that takes a fraction of the size. This means that static
builds will be smaller, but non-static builds will be bigger, though they will
have less linking time.

This change is also good because my history implementation was already bypassing
`stdio` for good reasons, and unifying the architecture was a win.

Another reason for this change is that my `bc` should *always* behave correctly
in the presence of signals like `SIGINT`, `SIGTERM`, and `SIGQUIT`. With the
addition of my own I/O buffering, I needed to also make sure that the buffers
were correctly flushed even when such signals happened.

For this reason, I **removed the option to build without signal support**.

As a nice side effect of this change, the error handling code could be changed
to take advantage of the stack unwinding that signals used. This means that
signals and error handling use the same code paths, which means that the stack
unwinding is well-tested. (Errors are tested heavily in the test suite.)

It also means that functions do not need to return a status code that
***every*** caller needs to check. This eliminated over 100 branches that simply
checked return codes and then passed that return code up the stack if necessary.
The code bloat savings from this is at least 1700 bytes on `x86_64`, *before*
taking into account the extra code from removing `stdio.h`.

## 2.7.2

This is a production release with one major bug fix.

The `length()` built-in function can take either a number or an array. If it
takes an array, it returns the length of the array. Arrays can be passed by
reference. The bug is that the `length()` function would not properly
dereference arrays that were references. This is a bug that affects all users.

**ALL USERS SHOULD UPDATE `bc`**.

## 2.7.1

This is a production release with fixes for new locales and fixes for compiler
warnings on FreeBSD.

## 2.7.0

This is a production release with a bug fix for Linux, new translations, and new
features.

Bug fixes:

* Option parsing in `BC_ENV_ARGS` was broken on Linux in 2.6.1 because `glibc`'s
  `getopt_long()` is broken. To get around that, and to support long options on
  every platform, an adapted version of [`optparse`][17] was added. Now, `bc`
  does not even use `getopt()`.
* Parsing `BC_ENV_ARGS` with quotes now works. It isn't the smartest, but it
  does the job if there are spaces in file names.

The following new languages are supported:

* Dutch
* Polish
* Russian
* Japanes
* Simplified Chinese

All of these translations were generated using [DeepL][18], so improvements are
welcome.

There is only one new feature: **`bc` now has a built-in pseudo-random number
generator** (PRNG).

The PRNG is seeded, making it useful for applications where
`/dev/urandom` does not work because output needs to be reproducible. However,
it also uses `/dev/urandom` to seed itself by default, so it will start with a
good seed by default.

It also outputs 32 bits on 32-bit platforms and 64 bits on 64-bit platforms, far
better than the 15 bits of C's `rand()` and `bash`'s `$RANDOM`.

In addition, the PRNG can take a bound, and when it gets a bound, it
automatically adjusts to remove bias. It can also generate numbers of arbitrary
size. (As of the time of release, the largest pseudo-random number generated by
this `bc` was generated with a bound of `2^(2^20)`.)

***IMPORTANT: read the [`bc` manual][9] and the [`dc` manual][10] to find out
exactly what guarantees the PRNG provides. The underlying implementation is not
guaranteed to stay the same, but the guarantees that it provides are guaranteed
to stay the same regardless of the implementation.***

On top of that, four functions were added to `bc`'s [extended math library][16]
to make using the PRNG easier:

* `frand(p)`: Generates a number between `[0,1)` to `p` decimal places.
* `ifrand(i, p)`: Generates an integer with bound `i` and adds it to `frand(p)`.
* `srand(x)`: Randomizes the sign of `x`. In other words, it flips the sign of
  `x` with probability `0.5`.
* `brand()`: Returns a random boolean value (either `0` or `1`).

## 2.6.1

This is a production release with a bug fix for FreeBSD.

The bug was that when `bc` was built without long options, it would give a fatal
error on every run. This was caused by a mishandling of `optind`.

## 2.6.0

This release is a production release ***with no bugfixes***. If you do not want
to upgrade, you don't have to.

No source code changed; the only thing that changed was `lib2.bc`.

This release adds one function to the [extended math library][16]: `p(x, y)`,
which calculates `x` to the power of `y`, whether or not `y` is an integer. (The
`^` operator can only accept integer powers.)

This release also includes a couple of small tweaks to the [extended math
library][16], mostly to fix returning numbers with too high of `scale`.

## 2.5.3

This release is a production release which addresses inconsistencies in the
Portuguese locales. No `bc` code was changed.

The issues were that the ISO files used different naming, and also that the
files that should have been symlinks were not. I did not catch that because
GitHub rendered them the exact same way.

## 2.5.2

This release is a production release.

No code was changed, but the build system was changed to allow `CFLAGS` to be
given to `CC`, like this:

```
CC="gcc -O3 -march=native" ./configure.sh
```

If this happens, the flags are automatically put into `CFLAGS`, and the compiler
is set appropriately. In the example above this means that `CC` will be "gcc"
and `CFLAGS` will be "-O3 -march=native".

This behavior was added to conform to GNU autotools practices.

## 2.5.1

This is a production release which addresses portability concerns discovered
in the `bc` build system. No `bc` code was changed.

* Support for Solaris SPARC and AIX were added.
* Minor documentations edits were performed.
* An option for `configure.sh` was added to disable long options if
  `getopt_long()` is missing.

## 2.5.0

This is a production release with new translations. No code changed.

The translations were contributed by [bugcrazy][15], and they are for
Portuguese, both Portugal and Brazil locales.

## 2.4.0

This is a production release primarily aimed at improving `dc`.

* A couple of copy and paste errors in the [`dc` manual][10] were fixed.
* `dc` startup was optimized by making sure it didn't have to set up `bc`-only
  things.
* The `bc` `&&` and `||` operators were made available to `dc` through the `M`
  and `m` commands, respectively.
* `dc` macros were changed to be tail call-optimized.

The last item, tail call optimization, means that if the last thing in a macro
is a call to another macro, then the old macro is popped before executing the
new macro. This change was made to stop `dc` from consuming more and more memory
as macros are executed in a loop.

The `q` and `Q` commands still respect the "hidden" macros by way of recording
how many macros were removed by tail call optimization.

## 2.3.2

This is a production release meant to fix warnings in the Gentoo `ebuild` by
making it possible to disable binary stripping. Other users do *not* need to
upgrade.

## 2.3.1

This is a production release. It fixes a bug that caused `-1000000000 < -1` to
return `0`. This only happened with negative numbers and only if the value on
the left was more negative by a certain amount. That said, this bug *is* a bad
bug, and needs to be fixed.

**ALL USERS SHOULD UPDATE `bc`**.

## 2.3.0

This is a production release with changes to the build system.

## 2.2.0

This release is a production release. It only has new features and performance
improvements.

1.	The performance of `sqrt(x)` was improved.
2.	The new function `root(x, n)` was added to the extended math library to
	calculate `n`th roots.
3.	The new function `cbrt(x)` was added to the extended math library to
	calculate cube roots.

## 2.1.3

This is a non-critical release; it just changes the build system, and in
non-breaking ways:

1.	Linked locale files were changed to link to their sources with a relative
	link.
2.	A bug in `configure.sh` that caused long option parsing to fail under `bash`
	was fixed.

## 2.1.2

This release is not a critical release.

1.	A few codes were added to history.
2.	Multiplication was optimized a bit more.
3.	Addition and subtraction were both optimized a bit more.

## 2.1.1

This release contains a fix for the test suite made for Linux from Scratch: now
the test suite prints `pass` when a test is passed.

Other than that, there is no change in this release, so distros and other users
do not need to upgrade.

## 2.1.0

This release is a production release.

The following bugs were fixed:

1.	A `dc` bug that caused stack mishandling was fixed.
2.	A warning on OpenBSD was fixed.
3.	Bugs in `ctrl+arrow` operations in history were fixed.
4.	The ability to paste multiple lines in history was added.
5.	A `bc` bug, mishandling of array arguments to functions, was fixed.
6.	A crash caused by freeing the wrong pointer was fixed.
7.	A `dc` bug where strings, in a rare case, were mishandled in parsing was
	fixed.

In addition, the following changes were made:

1.	Division was slightly optimized.
2.	An option was added to the build to disable printing of prompts.
3.	The special case of empty arguments is now handled. This is to prevent
	errors in scripts that end up passing empty arguments.
4.	A harmless bug was fixed. This bug was that, with the pop instructions
	(mostly) removed (see below), `bc` would leave extra values on its stack for
	`void` functions and in a few other cases. These extra items would not
	affect anything put on the stack and would not cause any sort of crash or
	even buggy behavior, but they would cause `bc` to take more memory than it
	needed.

On top of the above changes, the following optimizations were added:

1.	The need for pop instructions in `bc` was removed.
2.	Extra tests on every iteration of the interpreter loop were removed.
3.	Updating function and code pointers on every iteration of the interpreter
	loop was changed to only updating them when necessary.
4.	Extra assignments to pointers were removed.

Altogether, these changes sped up the interpreter by around 2x.

***NOTE***: This is the last release with new features because this `bc` is now
considered complete. From now on, only bug fixes and new translations will be
added to this `bc`.

## 2.0.3

This is a production, bug-fix release.

Two bugs were fixed in this release:

1.	A rare and subtle signal handling bug was fixed.
2.	A misbehavior on `0` to a negative power was fixed.

The last bug bears some mentioning.

When I originally wrote power, I did not thoroughly check its error cases;
instead, I had it check if the first number was `0` and then if so, just return
`0`. However, `0` to a negative power means that `1` will be divided by `0`,
which is an error.

I caught this, but only after I stopped being cocky. You see, sometime later, I
had noticed that GNU `bc` returned an error, correctly, but I thought it was
wrong simply because that's not what my `bc` did. I saw it again later and had a
double take. I checked for real, finally, and found out that my `bc` was wrong
all along.

That was bad on me. But the bug was easy to fix, so it is fixed now.

There are two other things in this release:

1.	Subtraction was optimized by [Stefan Eßer][14].
2.	Division was also optimized, also by Stefan Eßer.

## 2.0.2

This release contains a fix for a possible overflow in the signal handling. I
would be surprised if any users ran into it because it would only happen after 2
billion (`2^31-1`) `SIGINT`'s, but I saw it and had to fix it.

## 2.0.1

This release contains very few things that will apply to any users.

1.	A slight bug in `dc`'s interactive mode was fixed.
2.	A bug in the test suite that was only triggered on NetBSD was fixed.
3.	**The `-P`/`--no-prompt` option** was added for users that do not want a
	prompt.
4.	A `make check` target was added as an alias for `make test`.
5.	`dc` got its own read prompt: `?> `.

## 2.0.0

This release is a production release.

This release is also a little different from previous releases. From here on
out, I do not plan on adding any more features to this `bc`; I believe that it
is complete. However, there may be bug fix releases in the future, if I or any
others manage to find bugs.

This release has only a few new features:

1.	`atan2(y, x)` was added to the extended math library as both `a2(y, x)` and
	`atan2(y, x)`.
2.	Locales were fixed.
3.	A **POSIX shell-compatible script was added as an alternative to compiling
	`gen/strgen.c`** on a host machine. More details about making the choice
	between the two can be found by running `./configure.sh --help` or reading
	the [build manual][13].
4.	Multiplication was optimized by using **diagonal multiplication**, rather
	than straight brute force.
5.	The `locale_install.sh` script was fixed.
6.	`dc` was given the ability to **use the environment variable
	`DC_ENV_ARGS`**.
7.	`dc` was also given the ability to **use the `-i` or `--interactive`**
	options.
8.	Printing the prompt was fixed so that it did not print when it shouldn't.
9.	Signal handling was fixed.
10.	**Handling of `SIGTERM` and `SIGQUIT`** was fixed.
11.	The **built-in functions `maxibase()`, `maxobase()`, and `maxscale()`** (the
	commands `T`, `U`, `V` in `dc`, respectively) were added to allow scripts to
	query for the max allowable values of those globals.
12.	Some incompatibilities with POSIX were fixed.

In addition, this release is `2.0.0` for a big reason: the internal format for
numbers changed. They used to be a `char` array. Now, they are an array of
larger integers, packing more decimal digits into each integer. This has
delivered ***HUGE*** performance improvements, especially for multiplication,
division, and power.

This `bc` should now be the fastest `bc` available, but I may be wrong.

## 1.2.8

This release contains a fix for a harmless bug (it is harmless in that it still
works, but it just copies extra data) in the [`locale_install.sh`][12] script.

## 1.2.7

This version contains fixes for the build on Arch Linux.

## 1.2.6

This release removes the use of `local` in shell scripts because it's not POSIX
shell-compatible, and also updates a man page that should have been updated a
long time ago but was missed.

## 1.2.5

This release contains some missing locale `*.msg` files.

## 1.2.4

This release contains a few bug fixes and new French translations.

## 1.2.3

This release contains a fix for a bug: use of uninitialized data. Such data was
only used when outputting an error message, but I am striving for perfection. As
Michelangelo said, "Trifles make perfection, and perfection is no trifle."

## 1.2.2

This release contains fixes for OpenBSD.

## 1.2.1

This release contains bug fixes for some rare bugs.

## 1.2.0

This is a production release.

There have been several changes since `1.1.0`:

1.	The build system had some changes.
2.	Locale support has been added. (Patches welcome for translations.)
3.	**The ability to turn `ibase`, `obase`, and `scale` into stacks** was added
	with the `-g` command-line option. (See the [`bc` manual][9] for more
	details.)
4.	Support for compiling on Mac OSX out of the box was added.
5.	The extended math library got `t(x)`, `ceil(x)`, and some aliases.
6.	The extended math library also got `r2d(x)` (for converting from radians to
	degrees) and `d2r(x)` (for converting from degrees to radians). This is to
	allow using degrees with the standard library.
7.	Both calculators now accept numbers in **scientific notation**. See the
	[`bc` manual][9] and the [`dc` manual][10] for details.
8.	Both calculators can **output in either scientific or engineering
	notation**. See the [`bc` manual][9] and the [`dc` manual][10] for details.
9.	Some inefficiencies were removed.
10.	Some bugs were fixed.
11.	Some bugs in the extended library were fixed.
12.	Some defects from [Coverity Scan][11] were fixed.

## 1.1.4

This release contains a fix to the build system that allows it to build on older
versions of `glibc`.

## 1.1.3

This release contains a fix for a bug in the test suite where `bc` tests and
`dc` tests could not be run in parallel.

## 1.1.2

This release has a fix for a history bug; the down arrow did not work.

## 1.1.1

This release fixes a bug in the `1.1.0` build system. The source is exactly the
same.

The bug that was fixed was a failure to install if no `EXECSUFFIX` was used.

## 1.1.0

This is a production release. However, many new features were added since `1.0`.

1.	**The build system has been changed** to use a custom, POSIX
	shell-compatible configure script ([`configure.sh`][6]) to generate a POSIX
	make-compatible `Makefile`, which means that `bc` and `dc` now build out of
	the box on any POSIX-compatible system.
2.	Out-of-memory and output errors now cause the `bc` to report the error,
	clean up, and die, rather than just reporting and trying to continue.
3.	**Strings and constants are now garbage collected** when possible.
4.	Signal handling and checking has been made more simple and more thorough.
5.	`BcGlobals` was refactored into `BcVm` and `BcVm` was made global. Some
	procedure names were changed to reflect its difference to everything else.
6.	Addition got a speed improvement.
7.	Some common code for addition and multiplication was refactored into its own
	procedure.
8.	A bug was removed where `dc` could have been selected, but the internal
	`#define` that returned `true` for a query about `dc` would not have
	returned `true`.
9.	Useless calls to `bc_num_zero()` were removed.
10.	**History support was added.** The history support is based off of a
	[UTF-8 aware fork][7] of [`linenoise`][8], which has been customized with
	`bc`'s own data structures and signal handling.
11.	Generating C source from the math library now removes tabs from the library,
	shrinking the size of the executable.
12.	The math library was shrunk.
13.	Error handling and reporting was improved.
14.	Reallocations were reduced by giving access to the request size for each
	operation.
15.	**`abs()` (`b` command for `dc`) was added as a builtin.**
16.	Both calculators were tested on FreeBSD.
17.	Many obscure parse bugs were fixed.
18.	Markdown and man page manuals were added, and the man pages are installed by
	`make install`.
19.	Executable size was reduced, though the added features probably made the
	executable end up bigger.
20.	**GNU-style array references were added as a supported feature.**
21.	Allocations were reduced.
22.	**New operators were added**: `$` (`$` for `dc`), `@` (`@` for `dc`), `@=`,
	`<<` (`H` for `dc`), `<<=`, `>>` (`h` for `dc`), and `>>=`. See the
	[`bc` manual][9] and the [`dc` manual][10] for more details.
23.	**An extended math library was added.** This library contains code that
	makes it so I can replace my desktop calculator with this `bc`. See the
	[`bc` manual][3] for more details.
24.	Support for all capital letters as numbers was added.
25.	**Support for GNU-style void functions was added.**
26.	A bug fix for improper handling of function parameters was added.
27.	Precedence for the or (`||`) operator was changed to match GNU `bc`.
28.	`dc` was given an explicit negation command.
29.	`dc` was changed to be able to handle strings in arrays.

## 1.1 Release Candidate 3

This release is the eighth release candidate for 1.1, though it is the third
release candidate meant as a general release candidate. The new code has not
been tested as thoroughly as it should for release.

## 1.1 Release Candidate 2

This release is the seventh release candidate for 1.1, though it is the second
release candidate meant as a general release candidate. The new code has not
been tested as thoroughly as it should for release.

## 1.1 FreeBSD Beta 5

This release is the sixth release candidate for 1.1, though it is the fifth
release candidate meant specifically to test if `bc` works on FreeBSD. The new
code has not been tested as thoroughly as it should for release.

## 1.1 FreeBSD Beta 4

This release is the fifth release candidate for 1.1, though it is the fourth
release candidate meant specifically to test if `bc` works on FreeBSD. The new
code has not been tested as thoroughly as it should for release.

## 1.1 FreeBSD Beta 3

This release is the fourth release candidate for 1.1, though it is the third
release candidate meant specifically to test if `bc` works on FreeBSD. The new
code has not been tested as thoroughly as it should for release.

## 1.1 FreeBSD Beta 2

This release is the third release candidate for 1.1, though it is the second
release candidate meant specifically to test if `bc` works on FreeBSD. The new
code has not been tested as thoroughly as it should for release.

## 1.1 FreeBSD Beta 1

This release is the second release candidate for 1.1, though it is meant
specifically to test if `bc` works on FreeBSD. The new code has not been tested as
thoroughly as it should for release.

## 1.1 Release Candidate 1

This is the first release candidate for 1.1. The new code has not been tested as
thoroughly as it should for release.

## 1.0

This is the first non-beta release. `bc` is ready for production use.

As such, a lot has changed since 0.5.

1.	`dc` has been added. It has been tested even more thoroughly than `bc` was
	for `0.5`. It does not have the `!` command, and for security reasons, it
	never will, so it is complete.
2.	`bc` has been more thoroughly tested. An entire section of the test suite
	(for both programs) has been added to test for errors.
3.	A prompt (`>>> `) has been added for interactive mode, making it easier to
	see inputs and outputs.
4.	Interrupt handling has been improved, including elimination of race
	conditions (as much as possible).
5.	MinGW and [Windows Subsystem for Linux][1] support has been added (see
	[xstatic][2] for binaries).
6.	Memory leaks and errors have been eliminated (as far as ASan and Valgrind
	can tell).
7.	Crashes have been eliminated (as far as [afl][3] can tell).
8.	Karatsuba multiplication was added (and thoroughly) tested, speeding up
	multiplication and power by orders of magnitude.
9.	Performance was further enhanced by using a "divmod" function to reduce
	redundant divisions and by removing superfluous `memset()` calls.
10.	To switch between Karatsuba and `O(n^2)` multiplication, the config variable
	`BC_NUM_KARATSUBA_LEN` was added. It is set to a sane default, but the
	optimal number can be found with [`karatsuba.py`][4] (requires Python 3)
	and then configured through `make`.
11.	The random math test generator script was changed to Python 3 and improved.
	`bc` and `dc` have together been run through 30+ million random tests.
12.	All known math bugs have been fixed, including out of control memory
	allocations in `sine` and `cosine` (that was actually a parse bug), certain
	cases of infinite loop on square root, and slight inaccuracies (as much as
	possible; see the [README][5]) in transcendental functions.
13.	Parsing has been fixed as much as possible.
14.	Test coverage was improved to 94.8%. The only paths not covered are ones
	that happen when `malloc()` or `realloc()` fails.
15.	An extension to get the length of an array was added.
16.	The boolean not (`!`) had its precedence change to match negation.
17.	Data input was hardened.
18.	`bc` was made fully compliant with POSIX when the `-s` flag is used or
	`POSIXLY_CORRECT` is defined.
19.	Error handling was improved.
20.	`bc` now checks that files it is given are not directories.

## 1.0 Release Candidate 7

This is the seventh release candidate for 1.0. It fixes a few bugs in 1.0
Release Candidate 6.

## 1.0 Release Candidate 6

This is the sixth release candidate for 1.0. It fixes a few bugs in 1.0 Release
Candidate 5.

## 1.0 Release Candidate 5

This is the fifth release candidate for 1.0. It fixes a few bugs in 1.0 Release
Candidate 4.

## 1.0 Release Candidate 4

This is the fourth release candidate for 1.0. It fixes a few bugs in 1.0 Release
Candidate 3.

## 1.0 Release Candidate 3

This is the third release candidate for 1.0. It fixes a few bugs in 1.0 Release
Candidate 2.

## 1.0 Release Candidate 2

This is the second release candidate for 1.0. It fixes a few bugs in 1.0 Release
Candidate 1.

## 1.0 Release Candidate 1

This is the first Release Candidate for 1.0. `bc` is complete, with `dc`, but it
is not tested.

## 0.5

This beta release completes more features, but it is still not complete nor
tested as thoroughly as necessary.

## 0.4.1

This beta release fixes a few bugs in 0.4.

## 0.4

This is a beta release. It does not have the complete set of features, and it is
not thoroughly tested.

[1]: https://docs.microsoft.com/en-us/windows/wsl/install-win10
[2]: https://pkg.musl.cc/bc/
[3]: http://lcamtuf.coredump.cx/afl/
[4]: ./scripts/karatsuba.py
[5]: ./README.md
[6]: ./configure.sh
[7]: https://github.com/rain-1/linenoise-mob
[8]: https://github.com/antirez/linenoise
[9]: ./manuals/bc/A.1.md
[10]: ./manuals/dc/A.1.md
[11]: https://scan.coverity.com/projects/gavinhoward-bc
[12]: ./scripts/locale_install.sh
[13]: ./manuals/build.md
[14]: https://github.com/stesser
[15]: https://github.com/bugcrazy
[16]: ./manuals/bc/A.1.md#extended-library
[17]: https://github.com/skeeto/optparse
[18]: https://www.deepl.com/translator
[19]: ./manuals/benchmarks.md
[20]: https://github.com/apjanke/ronn-ng
[21]: https://pandoc.org/
[22]: ./scripts/locale_uninstall.sh
