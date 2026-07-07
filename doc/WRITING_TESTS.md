---
name: libarchive-writing-tests
description: Guide for writing and maintaining libarchive unit tests.
---

# Writing Libarchive Unit Tests

Each libarchive component includes a suite of unit tests.
These are located in the `test` directory underneath the component directory
(`libarchive/test`, `tar/test`, etc).

The most common reason that we fail to accept patches to libarchive
is because the patch lacks suitable new tests.
Almost all changes should be accompanied with a new or updated test case,
and if the change is non-trivial it will likely require several
tests to verify the full range of new behavior.
A new format or filter may require dozens of new tests.

The rest of this file explains how to write a new test case and
add it to the existing test suite.
Understanding the structure of tests will also help when updating
or modifying existing tests.
The RUNNING_TESTS documentation corresponding to your platform
describes how to build and run the test suite.

## Creating a New Test

A "test" in libarchive is a C function defined with the `DEFINE_TEST(test_name)` macro.
While most test files contain only a single test, it is possible to include multiple tests in a single source file.
Many tests also involve an associated reference or sample input file, which is typically stored in the repository in UUencoded format.

### 1. Basic Template
```c
#include "test.h"

/* The test name typically matches the name of the `.c` source file. */
DEFINE_TEST(test_foo)
{
	/* The file in the repository is named "test_foo.tar.uu". */
	/* The `refname` here omits the `.uu` suffix. */
	/* The base filename should match the test name. */
	const char *refname = "test_foo.tar";

	struct archive *a;
	struct archive_entry *ae;
	int r;

	/* Each test runs in a private temporary directory. */

	/* If you have a binary archive sample, extract it first. */
	extract_reference_file(refname);

	/* Create and configure a suitable archive handle. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	/* Open the extracted reference file. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 10240));

	/* Perform whatever tests are appropriate. */

	/* For example, when fixing a crash in libarchive, it
	 * may suffice for a test to simply read all the entries and
	 * their contents, as shown below.
	 * If errors are expected, you may need to unroll the following
	 * in order to perform specific checks for each entry. */
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		const void *pv;
		size_t s;
		int64_t o;

		 while ((r == archive_read_data_block(a, &pv, &s, &o)) == ARCHIVE_OK) {
			/* Verify the data contents if appropriate. */
		}
		/* Verify that we exited the loop above on EOF, not on error. */
		assertEqualIntA(a, ARCHIVE_EOF, r);
	}

	/* We exited the loop above, verify that it's EOF and not an error. */
	assertEqualIntA(a, ARCHIVE_EOF, r);

	/* Best practice: Close and free separately to get better
	 * error reporting if close fails. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
```

### 2. Reference Files (Compatibility Tests)
For testing compatibility with existing archives:
1. Place the sample data in the `test` directory alongside the `.c` file.
2. Encode it using `uuencode`: `uuencode test_foo.tar test_foo.tar > test_foo.tar.uu`.
3. Use `extract_reference_file("test_foo.tar")` in your test to decode it into the temporary work directory.

### 3. File and Function Naming

The basic naming convention is `test_<functionality>_<format/API>_<detail>.c` where:
- **Functionality:** Indicates general functionality (e.g., `read`, `write`, `archive`, `entry`, `compat`).
- **Format/API:** The specific format or API being exercised (e.g., `format_tar`, `filter_gzip`, `next_header`, `acl`).
- **Detail:** Additional context to clarify the purpose (e.g., `empty`, `large`, `mac_metadata`).

**Example names from the current suite:**
- `test_read_format_zip_mac_metadata.c`
- `test_write_disk_perms.c`
- `test_archive_read_next_header_empty.c`

### 4. Registering the Test

The build system searches uses of `DEFINE_TEST` to locate tests
in the relevant sources.
You only need to add the new files to the build system
to have them built
and run with the rest of the suite.
To add them to the build systems,
you need to update two files:

1. `Makefile.am` in the root:
   - Add `.c` files to `libarchive_test_SOURCES`.
   - Add `.uu` reference files to `libarchive_test_EXTRA_DIST`.
   - **Note:** Please maintain alphabetical order in these lists.
2. `CMakeLists.txt` in the relevant directory (e.g., `libarchive/test/CMakeLists.txt`):
   - Add `.c` files to `libarchive_test_SOURCES`.
   - **Note:** `.uu` files do NOT need to be listed in `CMakeLists.txt`.

---

## Assertion Macros

Always use the specialized assertion macros to ensure failures are actionable.
The full list of available macros is defined in `test_utils/test_common.h`.

| Macro | Description |
|-------|-------------|
| `assert(expr)` | Standard boolean assertion. |
| `assertEqualInt(v1, v2)` | Asserts two integers are equal. Prints both values on failure. |
| `assertEqualString(s1, s2)` | Asserts two strings are equal. Prints both on failure. |
| `assertEqualMem(p1, p2, n)` | Asserts two memory blocks are equal. Provides a hexdump on failure. |
| `assertA(expr)` | Like `assert`, but also prints `archive_error_string(a)` on failure. |
| `assertEqualIntA(a, v1, v2)` | Like `assertEqualInt`, but includes archive error context. |
| `failure(fmt, ...)` | **Critical:** Use before an assertion to provide context. The message is only shown if the following assertion fails. |

**Example usage:**
```c
/* Read first entry and verify metadata. */
assertEqualIntA(a, ARCHIVE_OK, r = archive_read_next_header(a, &ae));
if (r != ARCHIVE_OK) { ... end the test ... }

assertEqualString("test_file.txt", archive_entry_pathname(ae));
assertEqualInt(1197179003, archive_entry_mtime(ae));
assertEqualInt(1000, archive_entry_uid(ae));
assertEqualString("tim", archive_entry_uname(ae));
assertEqualInt(0100644, archive_entry_mode(ae));
```

---

## Working with Files and Data

Many libarchive tests perform all operations entirely in memory.
However, when testing archive extraction, you must verify the resulting files on disk.
Similarly, when testing archive creation, you may need to create files on disk to be archived.
The harness provides a variety of file utilities expressed as assertions.

This includes assertions that create or modify files:
- `assertMakeFile(path, mode, contents)`
- `assertMakeDir(path, mode)`
- `assertChmod(pathname, mode)`

And assertions that verify particular file properties, such as:
- `assertFileExists(path)`
- `assertFileSize(path, expected_size)`
- `assertTextFileContents(expected_text, path)`

The full set of assertions is listed in `test_utils/test_common.h`.
You can search the existing tests to find examples of their usage.

---

## Platform and Environment Utilities

Some checks can only be done if the underlying system has certain
capabilities.

Utilities to check capabilities of the current system include:
- `canSymlink()`
- `canGzip()`
- `canRunCommand("cmd", NULL)`.

These can be used to circumvent specific individual checks:

```c
if (canSymlink()) {
  skipping("Symlinks not supported on this platform");
  /* ... verify that symlink entries were correctly restored */
}
```

Or to abort an entire test:
```c
if (!canSymlink()) {
	skipping("Symlinks not supported on this platform");
	return;
}
```

---

## General Best Practices

Some pointers for making more effective tests:

- **Use assertions liberally:**
  Most lines of code in a test use `assert` macros to
  verify the expected result.
- **Always Clean Up:**
  Call `archive_read_free()` or `archive_write_free()` on every exit
  path, including failures and premature exits.
- **Cross-Platform Portability:**
  Do not assume POSIX features like `symlink` or `chown`.
  Use the `can...()` helpers to verify system support before
  performing platform-specific checks.
- **Verify the Fix:**
  Run your new test against the code before and after any
  other changes to ensure the test reproduces the issue and that
  your fix addresses it.
- **Test, then Fix:**
  Pull requests should have one or more commits with
  new or updated unit tests, followed by commits that
  change functionality.
- **Verify Error Details:**
  When an API returns `ARCHIVE_WARN` or `ARCHIVE_FATAL`,
  verify the specific error code from `archive_errno()`.
  However, you should not always check the result of `archive_error_string()`,
  as the specific text can evolve over time.
  Generally, there should be one test each time a new error path is added
  that verifies the error string.
- **Match the Existing Style:**
  Changes to existing files should always match the indentation
  and style of that file.
  New files should follow the "BSD" formatting convention,
  with 8-character hard tabs for indentation.
