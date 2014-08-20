#include <inttypes.h>
#include <stdio.h>

#include <libcss/libcss.h>
#include "stylesheet.h"

#define ITERATIONS (1)
#define DUMP_CSS (1)

#if DUMP_CSS
#include "dump.h"
#endif

#include "testutils.h"

static css_error resolve_url(void *pw,
		const char *base, lwc_string *rel, lwc_string **abs)
{
	UNUSED(pw);
	UNUSED(base);

	/* About as useless as possible */
	*abs = lwc_string_ref(rel);

	return CSS_OK;
}

int main(int argc, char **argv)
{
	css_stylesheet_params params;
	css_stylesheet *sheet;
	FILE *fp;
	size_t len, origlen;
#define CHUNK_SIZE (4096)
	uint8_t buf[CHUNK_SIZE];
	css_error error;
	int count;

	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_21;
	params.charset = "UTF-8";
	params.url = argv[1];
	params.title = NULL;
	params.allow_quirks = false;
	params.inline_style = false;
	params.resolve = resolve_url;
	params.resolve_pw = NULL;
	params.import = NULL;
	params.import_pw = NULL;
	params.color = NULL;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	for (count = 0; count < ITERATIONS; count++) {

		assert(css_stylesheet_create(&params, &sheet) == CSS_OK);

		fp = fopen(argv[1], "rb");
		if (fp == NULL) {
			printf("Failed opening %s\n", argv[1]);
			return 1;
		}

		fseek(fp, 0, SEEK_END);
		origlen = len = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		while (len >= CHUNK_SIZE) {
			size_t read = fread(buf, 1, CHUNK_SIZE, fp);
			assert(read == CHUNK_SIZE);

			error = css_stylesheet_append_data(sheet, buf, 
					CHUNK_SIZE);
			assert(error == CSS_OK || error == CSS_NEEDDATA);

			len -= CHUNK_SIZE;
		}

		if (len > 0) {
			size_t read = fread(buf, 1, len, fp);
			assert(read == len);

			error = css_stylesheet_append_data(sheet, buf, len);
			assert(error == CSS_OK || error == CSS_NEEDDATA);

			len = 0;
		}

		fclose(fp);

		error = css_stylesheet_data_done(sheet);
		assert(error == CSS_OK || error == CSS_IMPORTS_PENDING);

		while (error == CSS_IMPORTS_PENDING) {
			lwc_string *url;
			uint64_t media;

			error = css_stylesheet_next_pending_import(sheet,
					&url, &media);
			assert(error == CSS_OK || error == CSS_INVALID);

			if (error == CSS_OK) {
				css_stylesheet *import;
				char *buf = malloc(lwc_string_length(url) + 1);

				memcpy(buf, lwc_string_data(url), 
						lwc_string_length(url));
				buf[lwc_string_length(url)] = '\0';

				params.url = buf;

				assert(css_stylesheet_create(&params,
					&import) == CSS_OK);

				assert(css_stylesheet_data_done(import) == 
					CSS_OK);

				assert(css_stylesheet_register_import(sheet,
					import) == CSS_OK);

				css_stylesheet_destroy(import);

				error = CSS_IMPORTS_PENDING;

				free(buf);
			}
		}

#if DUMP_CSS
		{
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
			char *out;
			size_t outsize = max(16384, origlen * 8);
			size_t outlen = outsize;
			size_t written;
			out = malloc(outsize);
			assert(out != NULL);
			dump_sheet(sheet, out, &outlen);
			written = fwrite(out, 1, outsize - outlen, stdout);
			assert(written == outsize - outlen);
			free(out);
		}
#endif

		css_stylesheet_destroy(sheet);
	}

	printf("PASS\n");
        
	return 0;
}

