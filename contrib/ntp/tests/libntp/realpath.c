#include "config.h"

#include "ntp_stdlib.h"
#include "unity.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdarg.h>

static char * resolved;

void setUp(void);
void setUp(void) {
	resolved = NULL;
}

void tearDown(void);
void tearDown(void) {
	free(resolved);
	resolved = NULL;
}

static int/*BOOL*/ isValidAbsPath(const char * path)
{
	int retv = FALSE;
	/* this needs some elaboration: */
	if (path && path[0] == '/') {
		struct stat sb;
		if (0 == lstat(path, &sb)) {
			retv = (sb.st_mode & S_IFMT) != S_IFLNK;
		}
	}
	return retv;
}

static const char * errMsg(const char *fmt, ...)
{
	static char buf[256];
	va_list va;
	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);
	return buf;
}

void test_CurrentWorkingDir(void);
void test_CurrentWorkingDir(void) {
#   ifdef SYS_WINNT
	TEST_IGNORE_MESSAGE("not applicable to windows so far");
#   else
	resolved = ntp_realpath(".");
	TEST_ASSERT_NOT_NULL_MESSAGE(resolved, "failed to resolve '.'");
	TEST_ASSERT_TRUE_MESSAGE(isValidAbsPath(resolved), "'.' not resolved to absolute path");
#   endif
}

void test_DevLinks(void);
void test_DevLinks(void) {
#   ifdef SYS_WINNT
	TEST_IGNORE_MESSAGE("not applicable to windows so far");
#   else
	char            nam[512];
	char            abs[512];
	struct dirent * ent;
	DIR           * dfs = opendir("/dev");

	TEST_ASSERT_NOT_NULL_MESSAGE(dfs, "failed to open '/dev' !?!");
	while (NULL != (ent = readdir(dfs))) {
		/* the /dev/std{in,out,err} symlinks are prone to some
		 * kind of race condition under Linux, so we better skip
		 * them here; running tests in parallel can fail mysteriously
		 * otherwise. (Dunno *how* this could happen, but it
		 * did at some point in time, quite reliably...)
		 */
		if (!strncmp(ent->d_name, "std", 3))
			continue;
		/* otherwise build the full name & try to resolve: */
		snprintf(nam, sizeof(nam), "/dev/%s", ent->d_name);
		resolved = ntp_realpath(nam);
		TEST_ASSERT_NOT_NULL_MESSAGE(resolved, errMsg("could not resolve '%s'", nam));
		strlcpy(abs, resolved, sizeof(abs));
		free(resolved);
		resolved = NULL;
		/* test/development code:
		if (strcmp(nam, abs))
			printf(" '%s' --> '%s'\n", nam, abs);
		*/
		TEST_ASSERT_TRUE_MESSAGE(isValidAbsPath(abs), errMsg("could not validate '%s'", abs));
	}
	closedir(dfs);
#   endif
}
