#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>
#include <libpkgconf/path.h>
#include <assert.h>

static void
test_path_find_basename(void)
{
	assert(!strcmp(pkgconf_path_find_basename("/usr/lib/pkgconfig/foo.pc"), "foo.pc"));
	assert(!strcmp(pkgconf_path_find_basename("/usr/lib/pkgconfig"), "pkgconfig"));
	assert(!strcmp(pkgconf_path_find_basename("foo.pc"), "foo.pc"));
	assert(!strcmp(pkgconf_path_find_basename("/foo.pc"), "foo.pc"));
	assert(!strcmp(pkgconf_path_find_basename("/"), ""));
	assert(!strcmp(pkgconf_path_find_basename(""), ""));
	assert(!strcmp(pkgconf_path_find_basename("/usr/"), ""));
	assert(!strcmp(pkgconf_path_find_basename("usr/"), ""));
	assert(!strcmp(pkgconf_path_find_basename("///usr/lib///pkgconfig///foo.pc"), "foo.pc"));

#ifdef _WIN32
	assert(!strcmp(pkgconf_path_find_basename("C:\\lib\\pkgconfig\\foo.pc"), "foo.pc"));
	assert(!strcmp(pkgconf_path_find_basename("C:/lib/pkgconfig/foo.pc"), "foo.pc"));
	assert(!strcmp(pkgconf_path_find_basename("C:/lib\\pkgconfig/foo.pc"), "foo.pc"));
	assert(!strcmp(pkgconf_path_find_basename("C:\\lib/pkgconfig\\foo.pc"), "foo.pc"));
#endif
}

static void
test_path_trim_basename(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	pkgconf_buffer_append(&buf, "/usr/lib/pkgconfig/foo.pc");
	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), "/usr/lib/pkgconfig"));

	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), "/usr/lib"));

	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), "/usr"));

	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), ""));

	assert(!pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), ""));

	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_append(&buf, "foo.pc");
	assert(!pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), "foo.pc"));

	pkgconf_buffer_finalize(&buf);
}

static void
test_determine_prefix_logic(void)
{
	pkgconf_buffer_t buf = PKGCONF_BUFFER_INITIALIZER;

	/* Normal case */
	pkgconf_buffer_append(&buf, "/opt/foo/lib/pkgconfig/bar.pc");
	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_path_find_basename(pkgconf_buffer_str(&buf)), "pkgconfig"));
	assert(pkgconf_path_trim_basename(&buf));
	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), "/opt/foo"));

	/* Short path: /pkgconfig/foo.pc */
	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_append(&buf, "/pkgconfig/foo.pc");
	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_path_find_basename(pkgconf_buffer_str(&buf)), "pkgconfig"));
	assert(pkgconf_path_trim_basename(&buf)); /* trims pkgconfig, returns true because of / */
	assert(!pkgconf_path_trim_basename(&buf)); /* fails to trim further */

	/* Another short path: lib/pkgconfig/foo.pc */
	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_append(&buf, "lib/pkgconfig/foo.pc");
	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_path_find_basename(pkgconf_buffer_str(&buf)), "pkgconfig"));
	assert(pkgconf_path_trim_basename(&buf));
	assert(!pkgconf_path_trim_basename(&buf));

	/* Trailing slash */
	pkgconf_buffer_reset(&buf);
	pkgconf_buffer_append(&buf, "/usr/lib/pkgconfig/");
	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), "/usr/lib/pkgconfig"));
	assert(pkgconf_path_trim_basename(&buf));
	assert(!strcmp(pkgconf_buffer_str(&buf), "/usr/lib"));

	pkgconf_buffer_finalize(&buf);
}


int
main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	test_path_find_basename();
	test_path_trim_basename();
	test_determine_prefix_logic();

	return 0;
}
