/*
 * ntp_libopts.c
 *
 * Common code interfacing with Autogen's libopts command-line option
 * processing.
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stddef.h>
#include "ntp_libopts.h"
#include "ntp_stdlib.h"

extern const char *Version;	/* version.c for each program */


/*
 * ntpOptionProcess() is a clone of libopts' optionProcess which
 * overrides the --version output, appending detail from version.c
 * which was not available at Autogen time.
 */
int
ntpOptionProcess(
	tOptions *	pOpts,
	int		argc,
	char **		argv
	)
{
	char *		pchOpts;
	char **		ppzFullVersion;
	char *		pzNewFV;
	char *		pzAutogenFV;
	size_t		octets;
	int		rc;

	pchOpts = (void *)pOpts;
	ppzFullVersion = (char **)(pchOpts + offsetof(tOptions,
						      pzFullVersion));
	pzAutogenFV = *ppzFullVersion;
	octets = strlen(pzAutogenFV) +
		 1 +	/* '\n' */
		 strlen(Version) +
		 1;	/* '\0' */
	pzNewFV = emalloc(octets);
	snprintf(pzNewFV, octets, "%s\n%s", pzAutogenFV, Version);
	*ppzFullVersion = pzNewFV;
	rc = optionProcess(pOpts, argc, argv);
	*ppzFullVersion = pzAutogenFV;
	free(pzNewFV);

	return rc;
}
