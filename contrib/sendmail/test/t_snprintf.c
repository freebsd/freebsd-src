#include <stdio.h>
#include <sysexits.h>

#define TEST_STRING	"1234567890"

int
main(argc, argv)
	int argc;
	char **argv;
{
	int r;
	char buf[5];

	r = snprintf(buf, sizeof buf, "%s", TEST_STRING);

	if (buf[sizeof buf - 1] != '\0')
	{
		fprintf(stderr, "Add the following to devtools/Site/site.config.m4:\n\n");
		fprintf(stderr, "APPENDDEF(`confENVDEF', `-DSNPRINTF_IS_BROKEN=1')\n\n");
		exit(EX_OSERR);
	}
	fprintf(stderr, "snprintf() appears to work properly\n");
	exit(EX_OK);
}
