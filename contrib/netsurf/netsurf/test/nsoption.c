#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"

bool verbose_log = true;

nserror gui_options_init_defaults(struct nsoption_s *defaults)
{
#if defined(riscos)
	/* Set defaults for absent option strings */
	nsoption_setnull_charp(ca_bundle, strdup("NetSurf:Resources.ca-bundle"));
	nsoption_setnull_charp(cookie_file, strdup("NetSurf:Cookies"));
	nsoption_setnull_charp(cookie_jar, strdup(CHOICES_PREFIX "Cookies"));

	if (nsoption_charp(ca_bundle) == NULL ||
	    nsoption_charp(cookie_file) == NULL ||
	    nsoption_charp(cookie_jar) == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
#elif defined(nsgtk)
	char *hdir = getenv("HOME");
	char buf[PATH_MAX];

	/* Set defaults for absent option strings */
	snprintf(buf, PATH_MAX, "%s/.netsurf/Cookies", hdir);
	nsoption_setnull_charp(cookie_file, strdup(buf));
	nsoption_setnull_charp(cookie_jar, strdup(buf));
	if (nsoption_charp(cookie_file) == NULL ||
	    nsoption_charp(cookie_jar) == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	if (nsoption_charp(downloads_directory) == NULL) {
		snprintf(buf, PATH_MAX, "%s/", hdir);
		nsoption_set_charp(downloads_directory, strdup(buf));
	}

	if (nsoption_charp(url_file) == NULL) {
		snprintf(buf, PATH_MAX, "%s/.netsurf/URLs", hdir);
		nsoption_set_charp(url_file, strdup(buf));
	}

	if (nsoption_charp(hotlist_path) == NULL) {
		snprintf(buf, PATH_MAX, "%s/.netsurf/Hotlist", hdir);
		nsoption_set_charp(hotlist_path, strdup(buf));
	}

	nsoption_setnull_charp(ca_path, strdup("/etc/ssl/certs"));

	if (nsoption_charp(url_file) == NULL ||
	    nsoption_charp(ca_path) == NULL ||
	    nsoption_charp(downloads_directory) == NULL ||
	    nsoption_charp(hotlist_path) == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

#endif
	return NSERROR_OK;
}


int main(int argc, char**argv)
{
	FILE *fp;

	nsoption_init(gui_options_init_defaults, NULL, NULL);

	nsoption_read("data/Choices", NULL);

	nsoption_write("Choices-short", NULL, NULL);

	fp = fopen("Choices-all", "w");

	nsoption_dump(fp, NULL);

	fclose(fp);

	return 0;
}
