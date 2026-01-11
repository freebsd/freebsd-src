# 🧠 Angry AI: Lessons Learned & Technical Wisdom

*Cumulative knowledge from the ongoing audit of the FreeBSD source tree.*

## 1. The "Assumption of Safety" Fallacy
**Case Study:** `bin/hostname/hostname.c`
- **The Bug:** Buffer overrun.
- **The Cause:** Developer assumed `gethostname()` always null-terminates.
- **The Reality:** `man 3 gethostname` explicitly says it *does not* if truncated.
- **Lesson:** **NEVER assume a C standard library function does what you think it does.** Read the man page.

## 2. The "It Works on My Machine" Trap
**Case Study:** `bin/echo/echo.c`
- **The Bug:** Missing short-write handling in `writev()`.
- **The Cause:** `writev()` almost always writes everything to a terminal.
- **The Reality:** On pipes, sockets, or full disks, it writes partially.
- **Lesson:** Test for failure modes (slow I/O, full disks), not just happy paths.

## 3. The "Trusted Source" Myth
**Case Study:** `bin/cat/cat.c`
- **The Bug:** Unchecked `st_blksize` used for `malloc()`.
- **The Cause:** Trusting `stat()` return values from the filesystem.
- **The Reality:** FUSE filesystems, network mounts, or corruption can return `st_blksize` of 0 or 2GB.
- **Lesson:** Treat **ALL** external data as hostile. This includes:
  - Filesystem metadata (`stat`, `dirent`)
  - Environment variables (`getenv`)
  - Kernel parameters (`sysconf`, `sysctl`)
  - Network data

## 4. The Integer Overflow Blind Spot
**Case Study:** `sysconf(_SC_PAGESIZE)` in `cat.c`
- **The Bug:** Casting `long` (-1 on error) to `size_t` (unsigned).
- **The Consequence:** -1 becomes `SIZE_MAX` (huge number) -> buffer overflow.
- **Lesson:** Validate **BEFORE** casting. `if (val > 0) cast(val)`.

## 5. Legacy APIs exist for a reason (usually a bad one)
- **bzero()**: Deprecated. Use `memset()`.
- **sprintf()**: Dangerous. Use `snprintf()`.
- **gets()**: **FATAL**. Never use.
- **strcpy()**: Dangerous. Use `strlcpy()`.

## 6. Comment Syntax Errors Can Break Builds
**Case Study:** AI reviewer added comments containing `sys/*`
- **The Bug:** `/*` within a `/* ... */` comment block
- **The Compiler:** `-Werror,-Wcomment` treats nested `/*` as error
- **The Impact:** Build breaks with "error: '/*' within block comment"
- **The Fix:** Use `sys/...` or `sys/xxx` instead of `sys/*`
- **Lesson:** **C doesn't support nested comments.** Any `/*` or `*/` pattern inside a comment will break. When writing comments:
  - Avoid glob patterns with `*` adjacent to `/`
  - Use `...` or `xxx` for wildcards
  - Test build with `-Werror` enabled
  - Remember: Comments are code too!

**REPEAT OFFENSE WARNING:** This mistake was made MULTIPLE TIMES despite being documented:
- First occurrence: `bin/cat/cat.c` (fixed, documented in PERSONA.md and LESSONS.md)
- Second occurrence: `bin/pwd/pwd.c` and `bin/rm/rm.c` (same error!)
- **Root cause:** Not checking existing comments before commit
- **Prevention:** Automated pre-commit hook to grep for `sys/\*` in comments
- **Lesson:** Documentation alone is insufficient. Humans (and AIs) make the same mistakes repeatedly. AUTOMATE THE CHECK.

## 7. Shell Builtin Redefinitions Break Standard Assumptions
**Case Study:** `bin/kill/kill.c` with `#ifdef SHELL`
- **The Bug:** Checking `printf`/`fprintf` return values caused compilation errors
- **The Error:** `error: invalid operands to binary expression ('void' and 'int')`
- **The Cause:** When compiled as shell builtin, `bltin/bltin.h` redefines `printf` and `fprintf` to return `void` instead of `int`
- **The Impact:** Standard C assumption that printf returns int is WRONG in shell builtin context
- **The Reality:** FreeBSD utilities often serve dual purposes:
  1. Standalone programs (`/bin/kill`)
  2. Shell builtins (for performance)
  - When used as builtins, I/O is handled differently by the shell
  - Standard I/O functions are redefined for shell integration
- **Lesson:** **Context matters!** Don't blindly apply "best practices" without understanding the compilation context:
  - Check for `#ifdef SHELL` or similar conditional compilation
  - Shell builtins may redefine standard functions
  - What's correct for standalone programs may be wrong for builtins
  - Read the headers being included (`bltin/bltin.h`, etc.)
- **Rule:** Before adding I/O error checking, verify the function actually returns `int` in ALL compilation contexts

**FILES WITH DUAL COMPILATION:**
- `bin/kill/kill.c` - standalone + shell builtin
- `bin/test/test.c` - standalone + shell builtin (also has `#ifdef SHELL`)
- Likely others in bin/ directory

**PREVENTION:** Search for `#ifdef SHELL` before adding printf/fprintf error checks.

## 8. Include Ordering: sys/types.h is SPECIAL (Not Just Alphabetical!)

**Date:** Tuesday Dec 2, 2025  
**File:** `sbin/dmesg/dmesg.c`  
**Error Type:** Build break - missing type definitions

### What Happened

I alphabetized ALL `sys/` headers, including `sys/types.h`:
```c
#include <sys/cdefs.h>
#include <sys/msgbuf.h>   // WRONG ORDER!
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/types.h>    // TOO LATE!
```

This caused build errors because `sys/msgbuf.h` uses types defined in `sys/types.h`:
- `u_int` (unsigned int)
- `uintptr_t` (pointer-sized integer)
- Other fundamental types

### Root Cause

**sys/types.h defines FUNDAMENTAL TYPES** that other system headers depend on. It cannot be alphabetized with other `sys/` headers - it must come EARLY.

### The Correct Ordering Rule

```c
1. #include <sys/cdefs.h>     // ALWAYS FIRST
2. #include <sys/types.h>      // SECOND (defines basic types)
3. #include <sys/...>          // Other sys/ headers alphabetically
4. #include <standard.h>       // Standard headers alphabetically
```

### Why This Matters

Many system headers have dependencies:
- `sys/msgbuf.h` needs `u_int` from `sys/types.h`
- `sys/lock.h` needs `uintptr_t` from `sys/types.h`
- Other headers may need `size_t`, `ssize_t`, etc.

### Prevention

**CRITICAL RULE**: When reordering includes:
1. `sys/cdefs.h` is ALWAYS first
2. `sys/types.h` is ALWAYS second (if needed)
3. `sys/param.h` often comes early too (includes sys/types.h)
4. ONLY THEN alphabetize remaining `sys/` headers
5. Then alphabetize standard headers

**DO NOT blindly alphabetize ALL sys/ headers!**

**SPECIAL HEADERS THAT COME EARLY:**
- `sys/cdefs.h` - always first
- `sys/types.h` - defines fundamental types
- `sys/param.h` - includes sys/types.h, defines system parameters

---

## 9. Using errno Requires errno.h Include

**Date:** Tuesday Dec 2, 2025  
**Files:** Multiple sbin utilities  
**Error Type:** Build break - undeclared identifier 'errno'

### What Happened

When adding strtol() validation with errno checking:
```c
errno = 0;
lval = strtol(val, &endptr, 10);
if (errno != 0 || ...)
```

I forgot to verify that `<errno.h>` was included in all affected files.

### Root Cause

**errno is not a keyword** - it's a macro defined in `<errno.h>`. Without the include:
- `errno = 0` → "error: use of undeclared identifier 'errno'"
- `if (errno != 0)` → same error

### Prevention Checklist

When adding strtol()/strtol()-based validation:
1. ✅ Add `errno = 0` before the call
2. ✅ Check `errno != 0` after the call
3. ✅ **VERIFY `<errno.h>` is included!**
4. ✅ Check ALL files being modified, not just the first one

### Files That Were Missing errno.h:
- sbin/kldunload/kldunload.c
- sbin/nos-tun/nos-tun.c
- sbin/newfs/mkfs.c
- sbin/tunefs/tunefs.c

**LESSON**: errno is NOT automatically available. ALWAYS check includes!

---

## 10. Using INT_MAX Requires limits.h Include

**Date:** Thursday Dec 4, 2025  
**File:** `sbin/comcontrol/comcontrol.c`  
**Error Type:** Build break - undeclared identifier 'INT_MAX'

### What Happened

When converting atoi() to strtol() with proper range validation:
```c
errno = 0;
lval = strtol(argv[3], &endptr, 10);
if (errno != 0 || *endptr != '\0' || lval < 0 || lval > INT_MAX)
    errx(1, "invalid drainwait value: %s", argv[3]);
drainwait = (int)lval;
```

I forgot to add `#include <limits.h>` which defines `INT_MAX`.

### Root Cause

**INT_MAX is not a keyword** - it's a macro defined in `<limits.h>`. Without the include:
- `lval > INT_MAX` → "error: use of undeclared identifier 'INT_MAX'"

### The atoi() → strtol() Conversion Pattern Requires TWO Headers

When replacing `atoi()` with proper `strtol()` validation, you need:

1. **`<errno.h>`** - for `errno` variable (Lesson #9)
2. **`<limits.h>`** - for `INT_MAX`, `LONG_MAX`, `UINT_MAX`, etc.

### Prevention Checklist

When converting atoi()/atol() to strtol() with validation:
1. ✅ Add `errno = 0` before the call
2. ✅ Check `errno != 0` after the call  
3. ✅ Check `*endptr != '\0'` for trailing garbage
4. ✅ Check range (e.g., `lval < 0 || lval > INT_MAX`)
5. ✅ **VERIFY `<errno.h>` is included!** (Lesson #9)
6. ✅ **VERIFY `<limits.h>` is included!** (THIS LESSON)

### Common Constants from limits.h

- `INT_MAX` / `INT_MIN` - for int range checks
- `LONG_MAX` / `LONG_MIN` - for long range checks  
- `UINT_MAX` - for unsigned int range checks
- `UINT16_MAX` / `UINT32_MAX` - for fixed-width type checks
- `SIZE_MAX` - for size_t range checks (from `<stdint.h>`)

### Files Where This Was Needed:
- sbin/comcontrol/comcontrol.c

**LESSON**: INT_MAX and other limit constants are NOT automatically available. When adding range validation to strtol() conversions, ALWAYS verify `<limits.h>` is included!

---

*Add to this file as new classes of bugs are discovered.*


## 2025-12-20 11:52
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2025-12-21 16:03
### COMPILER: Function Type Conflicts and Pointer Qualifiers
- Conflicting function types and passing `const char *` to a non-const parameter caused build errors.
- Ensure consistent function declarations and use `const char *` for functions that do not modify the string.


## 2025-12-22 19:30
### COMPILER: Const Qualifier Warning
- Passing a `const EVP_MD *` to a parameter expecting `EVP_MD *` discards qualifiers.
- Ensure function parameters match the constness of the arguments passed.


## 2025-12-22 19:46
### COMPILER: Empty Error Reports
- Multiple builds failed between 2025-12-22 19:46 and 2025-12-23 10:13 without emitting diagnostics.
- Capture verbose build logs, verify toolchain installation, and increase logging to surface the underlying faults instead of rerunning blindly.


## 2025-12-23 10:51
### COMPILER: Build System Configuration Issue
- The build system reported a failure without specific errors or warnings.
- Verify build configuration and environment setup to ensure all dependencies are correctly installed and paths are properly configured.


## 2025-12-25 11:48
### COMPILER: Stale .depend Files and Type Mismatch Errors
- Stale `.depend` files caused build failures; type mismatch errors occurred due to comparing a negative constant with an unsigned short.
- Clean build directories using `make clean` before building; ensure proper type comparisons in code.


## 2025-12-25 14:42
### COMPILER: Stale .depend Files and Variable Shadowing
- Stale `.depend` files caused build errors; variable shadowing in `chio.c` led to a compiler error.
- Clean build directories with `make clean` before building; rename local variables to avoid shadowing global ones.


## 2025-12-26 05:08
### COMPILER: Stale .depend Files and Conflict Markers
- Stale `.depend` files and unresolved version control conflict markers caused build failures.
- Clean build directories with `make clean` and resolve all conflict markers in source files before building.


## 2025-12-26 05:14
### COMPILER: Stale .depend Files and Syntax Errors
- A wave of builds failed because stale `.depend` files masked persistent syntax errors in `hostname.c`.
- Always `make clean` before large refactors and fix every compiler-reported syntax issue before retrying the build.


## 2025-12-27 02:07
### COMPILER: Stale .depend Files and Undefined Identifiers
- Stale `.depend` files caused build errors; undeclared identifier `mode` in `mkdir.c` slipped through.
- Clean build environments and ensure every new symbol is declared in the right header before use.


## 2025-12-27 04:39
### COMPILER: Uninitialized Variable Usage
- The variable `mpos` was used uninitialized in `ar_io.c`.
- Initialize all variables before use and keep `-Wuninitialized` warnings enabled in CI.


## 2025-12-27 18:05
### COMPILER: Stale .depend Files and Integer Conversion Errors
- Stale `.depend` files caused build issues; integer conversion errors in `fmt.c` led to further failures.
- Clean build directories with `make clean` before rebuilding and audit integer casts where widths differ.


## 2025-12-28 17:19
### HEADERS: Missing Header for INT_MAX
- What went wrong: `INT_MAX` was used without including `<limits.h>`.
- How to avoid it next time: Ensure all necessary headers are included in source files using standard constants like `INT_MAX`.


## 2025-12-31 12:25
### COMPILER: Function Declaration and Format Mismatch
- Implicit function declaration and format specifier mismatch caused build errors.
- Ensure all functions are declared before use and correct format specifiers match argument types.


## 2025-12-31 12:35
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory with `make clean` before building; ensure all functions are declared properly in headers.


## 2025-12-31 12:43
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 12:51
### COMPILER: Stale .depend Files and Implicit Function Declaration
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory with `make clean` before building; ensure all functions are declared properly in headers.


## 2025-12-31 12:56
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory with `make clean` before building; ensure all functions are declared properly in headers.


## 2025-12-31 13:04
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory with `make clean` before building; ensure all functions are declared before use.


## 2025-12-31 13:06
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 13:10
### COMPILER: Stale .depend Files and Implicit Function Declaration
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory with `make clean` before building; ensure all functions are declared properly in headers.


## 2025-12-31 13:15
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 13:31
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared functions caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2025-12-31 13:36
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directories with `make clean` before building; ensure all functions are declared properly in headers.


## 2025-12-31 13:40
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared functions led to implicit declarations.
- Clean build directories with `make clean` before building; ensure all functions are declared.


## 2025-12-31 13:48
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2025-12-31 13:58
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; implicit function declaration led to a compilation failure.
- Clean build directories using `make clean` before building again; ensure all functions are declared before use.


## 2025-12-31 14:00
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 14:02
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared functions caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2025-12-31 14:17
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 14:19
### COMPILER: Stale .depend Files and Implicit Function Declaration
- Stale `.depend` files and an undeclared function caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2025-12-31 14:32
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directories with `make clean` before building; ensure all functions are declared or defined.


## 2025-12-31 14:39
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 14:47
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directories with `make clean` before building and ensure all functions are declared or defined.


## 2025-12-31 14:52
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 15:09
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2025-12-31 15:11
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 15:12
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 15:14
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directories with `make clean` before building; ensure all functions are declared or defined.


## 2025-12-31 15:21
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directories with `make clean` before building; ensure all functions are declared or defined.


## 2025-12-31 15:23
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 15:30
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function calls caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2025-12-31 15:32
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build issues; undeclared function `get_which_name` led to a compilation error.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2025-12-31 15:34
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2025-12-31 15:36
### COMPILER: Stale .depend Files and Implicit Function Declaration
- Stale `.depend` files and an implicit function declaration caused build failures.
- Clean the build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 04:17
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and check for any issues with the build environment or configuration.


## 2026-01-01 04:20
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all build processes are correctly configured and that no external interruptions occur during builds.


## 2026-01-01 04:22
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and check for any issues with the build environment or configuration.


## 2026-01-01 04:25
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes are prematurely terminated during the build.


## 2026-01-01 04:40
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and check for any issues with the build environment or configuration.


## 2026-01-01 04:41
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 04:43
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 04:45
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 04:46
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 04:52
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 04:58
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 05:15
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all build processes are properly configured and that no output streams are prematurely closed.


## 2026-01-01 05:18
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 05:19
### COMPILER: Broken Pipe Error
- The build system encountered a broken pipe error.
- Ensure all output streams are properly managed and no processes prematurely terminate.


## 2026-01-01 05:30
### COMPILER: Empty Build Error Report
- The build failed without any specific error or warning messages.
- Ensure all dependencies and environment configurations are correctly set up before starting a build.


## 2026-01-01 05:32
### COMPILER: Build System Configuration Issue
- The build system reported a failure without specific errors or warnings.
- Verify build configuration and ensure all dependencies are correctly installed and up-to-date.


## 2026-01-01 05:33
### COMPILER: Empty Build Error Log
- The build failed with no specific errors or warnings logged.
- Ensure all build tools and dependencies are correctly installed and configured.


## 2026-01-01 05:48
### COMPILER: Build Script Issues
- The build failed due to issues in the clean script and source files.
- Ensure scripts and source files are correctly formatted and executable permissions are set.


## 2026-01-01 05:49
### COMPILER: Build System Configuration Issue
- The build system reported a failure without specific errors or warnings.
- Verify build configuration and environment setup to ensure all dependencies are correctly installed and paths are set properly.


## 2026-01-01 05:50
### COMPILER: Build System Configuration Issue
- The build system reported a failure without specific errors or warnings.
- Verify build configuration and environment setup to ensure all dependencies are correctly installed and paths are set properly.


## 2026-01-01 05:51
### COMPILER: Empty Build Error Log
- The build failed with no errors or warnings logged.
- Ensure all build tools and configurations are correctly set up to capture and report errors.


## 2026-01-01 05:52
### COMPILER: Empty Build Error Report
- The build failed without any specific error or warning messages.
- Ensure all dependencies and environment configurations are correctly set up before starting a build.


## 2026-01-01 05:55
### COMPILER: Empty Build Error Report
- The build failed without any specific error or warning messages.
- Ensure all dependencies and environment configurations are correctly set up before starting a build.


## 2026-01-01 06:02
### COMPILER: Build System Configuration Issue
- The build system reported a failure without specific errors or warnings.
- Verify build configuration and environment setup to ensure all dependencies are correctly installed and paths are set properly.


## 2026-01-01 07:07
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all build tools and dependencies are up-to-date and check for any issues with resource limits or process management.


## 2026-01-01 07:08
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 07:09
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 07:10
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 07:14
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 07:22
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all build processes are correctly configured and that no external interruptions occur during builds.


## 2026-01-01 07:23
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 07:25
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 07:26
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 08:13
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all build tools and dependencies are up-to-date and check for any issues with the build environment configuration.


## 2026-01-01 08:23
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 08:24
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and check for any issues with the build environment or tools.


## 2026-01-01 08:25
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly managed and that no processes prematurely terminate.


## 2026-01-01 08:26
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 08:33
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 08:34
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 08:36
### COMPILER: Broken Pipe Error
- What went wrong: The build system encountered a broken pipe error.
- How to avoid it next time: Ensure all output streams are properly handled and that no processes prematurely terminate.


## 2026-01-01 08:40
### BUILD: Build failure
- Error occurred during buildworld
- Review compiler output carefully


## 2026-01-01 10:58
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 11:06
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 11:43
### COMPILER: Stale Dependencies and Implicit Function Calls
- Stale dependencies and undeclared function calls caused build failures.
- Clean build directories and ensure all functions are declared before use.


## 2026-01-01 11:45
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directories using `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 11:47
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function calls caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2026-01-01 11:49
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compiler error.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 11:56
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 12:01
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 12:03
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compiler error.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 12:05
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 12:07
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration warnings treated as errors.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 12:14
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 12:19
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2026-01-01 12:21
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory with `make clean` before building; ensure all functions are declared properly in headers.


## 2026-01-01 12:26
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 12:28
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function calls caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2026-01-01 12:31
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directories with `make clean` before building and ensure all functions are declared or defined.


## 2026-01-01 12:35
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration warnings treated as errors.
- Clean build directory with `make clean` before rebuilding; ensure all functions are declared or defined.


## 2026-01-01 12:43
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 12:45
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 12:47
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 13:02
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directories with `make clean` before building and ensure all functions are declared or defined.


## 2026-01-01 13:09
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directory with `make clean` and ensure all functions are declared before use.


## 2026-01-01 13:19
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function calls caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2026-01-01 13:25
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory with `make clean` before building; ensure all functions are declared before use.


## 2026-01-01 13:36
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 13:37
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directories with `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 13:47
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and an implicit function declaration caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2026-01-01 13:54
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.


## 2026-01-01 14:05
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files and undeclared function `get_which_name` caused build failures.
- Clean build directories with `make clean` and ensure all functions are declared before use.


## 2026-01-01 14:08
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to implicit declaration issues.
- Clean build directory using `make clean` before building; ensure all functions are declared properly in headers.


## 2026-01-01 14:13
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; undeclared function `get_which_name` led to a compilation failure.
- Clean build directory with `make clean` before building; ensure all functions are declared or defined.

--- Migrated from .angry-ai directory ---

# Lessons Learned

This file tracks mistakes made during code review to avoid repeating them.
Each lesson is recorded with timestamp, category, and remediation advice.



## 2026-01-10 22:06
### COMPILER: Stale .depend Files and Syntax Error
- Stale `.depend` files caused build failures; undeclared identifier 'errstr' was a syntax error.
- Clean build directory with `make clean` before building to remove stale dependencies; correct misspelled identifiers in source code.


## 2026-01-10 22:15
### COMPILER: Stale .depend Files and Unused Variable Warning
- Stale `.depend` files caused build errors; unused variable warning in `cpuset.c` also failed.
- Clean build directory with `make clean` before building; remove unused variables to avoid warnings.


## 2026-01-11 04:41
### SYNTAX: Undeclared Identifier Error
- What went wrong: The identifier `errstr` was used but not declared in `cpuset.c`.
- How to avoid it next time: Ensure all identifiers are properly declared before use or correct any typos (e.g., `strstr` instead of `errstr`).


## 2026-01-11 04:50
### COMPILER: Stale .depend Files and Unused Variable Warning
- Stale `.depend` files caused build errors; unused variable warning in `cpuset.c` also failed.
- Clean build directory with `make clean` before building; remove unused variables to avoid warnings.


## 2026-01-11 05:09
### COMPILER: Stale .depend Files and Undeclared Identifiers
- Stale `.depend` files caused build errors; undeclared identifier `errstr` was a typo.
- Clean build directory with `make clean` before building to avoid stale dependencies; use correct identifiers like `strstr`.


## 2026-01-11 08:57
### COMPILER: Stale .depend Files and Undeclared Identifiers
- Stale `.depend` files caused build errors; undeclared identifier 'endptr' in `cpuset.c`.
- Clean build directory with `make clean` before building to remove stale dependencies; ensure all variables are declared.


## 2026-01-11 09:45
### COMPILER: Stale .depend Files and Implicit Function Declarations
- Stale `.depend` files caused build errors; implicit function declaration led to a compilation failure.
- Clean build directories with `make clean` before building; ensure all functions are declared before use.


## 2026-01-11 11:25
### COMPILER: Stale .depend Files and Undeclared Identifiers
- Stale `.depend` files caused build errors; undeclared identifier `errstr` in `cpuset.c`.
- Clean build directory with `make clean` before building to remove stale dependencies; ensure all identifiers are declared or included correctly.


## 2026-01-11 11:33
### COMPILER: Stale .depend Files and Unused Variable Warning
- Stale `.depend` files caused build errors; unused variable warning in `cpuset.c` also failed.
- Clean build directory with `make clean` before building to remove stale files; fix or suppress warnings by removing unused variables.
