/***************************************************************************
 * mkconfigs.c
 * (C) 2002 Randy Dunlap <rddunlap@osdl.org>

#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Rules for scripts/mkconfigs.c input.config output.c
# to generate configs.c from linux/.config:
# - drop lines that begin with '#'
# - drop blank lines
# - lines that use double-quotes must \\-escape-quote them

################## skeleton configs.c file: ####################

#include <linux/init.h>
#include <linux/module.h>

static char *configs[] __initdata =

  <insert lines selected lines of .config, quoted, with added '\n'>,

;

################### end configs.c file ######################

 * Changelog for ver. 0.2, 2002-02-15, rddunlap@osdl.org:
 - strip leading "CONFIG_" from config option strings;
 - use "static" and "__attribute__((unused))";
 - don't use EXPORT_SYMBOL();
 - separate each config line with \newline instead of space;

 * Changelog for ver. 0.3, 2002-02-18, rddunlap@osdl.org:
 - keep all "not set" comment lines from .config so that 'make *config'
   will be happy, but don't keep other comments;
 - keep leading "CONFIG_" on each line;

****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define VERSION		"0.2"
#define LINE_SIZE	1000

int include_all_lines = 1;	// whether to include "=n" lines in the output

void usage (const char *progname)
{
	fprintf (stderr, "%s ver. %s\n", progname, VERSION);
	fprintf (stderr, "usage:  %s input.config.name path/configs.c\n",
			progname);
	exit (1);
}

void make_intro (FILE *sourcefile)
{
	fprintf (sourcefile, "#include <linux/init.h>\n");
/////	fprintf (sourcefile, "#include <linux/module.h>\n");
	fprintf (sourcefile, "\n");
/////	fprintf (sourcefile, "char *configs[] __initdata = {\n");
	fprintf (sourcefile, "static char __attribute__ ((unused)) *configs[] __initdata = {\n");
	fprintf (sourcefile, "  \"CONFIG_BEGIN=n\\n\" ,\n");
}

void make_ending (FILE *sourcefile)
{
	fprintf (sourcefile, "  \"CONFIG_END=n\\n\"\n");
	fprintf (sourcefile, "};\n");
/////	fprintf (sourcefile, "EXPORT_SYMBOL (configs);\n");
}

void make_lines (FILE *configfile, FILE *sourcefile)
{
	char cfgline[LINE_SIZE];
	char *ch;

	while (fgets (cfgline, LINE_SIZE, configfile)) {
		/* kill the trailing newline in cfgline */
		cfgline[strlen (cfgline) - 1] = '\0';

		/* don't keep #-only line or an empty/blank line */
		if ((cfgline[0] == '#' && cfgline[1] == '\0') ||
		    cfgline[0] == '\0')
			continue;

		if (!include_all_lines &&
		    cfgline[0] == '#') // strip out all comment lines
			continue;

		/* really only want to keep lines that begin with
		 * "CONFIG_" or "# CONFIG_" */
		if (strncmp (cfgline, "CONFIG_", 7) &&
		    strncmp (cfgline, "# CONFIG_", 9))
		    	continue;

		/*
		 * use strchr() to check for "-quote in cfgline;
		 * if not found, output the line, quoted;
		 * if found, output a char at a time, with \\-quote
		 * preceding double-quote chars
		 */
		if (!strchr (cfgline, '"')) {
			fprintf (sourcefile, "  \"%s\\n\" ,\n", cfgline);
			continue;
		}

		/* go to char-at-a-time mode for this config and
		 * precede any double-quote with a backslash */
		fprintf (sourcefile, "  \"");	/* lead-in */
		for (ch = cfgline; *ch; ch++) {
			if (*ch == '"')
				fputc ('\\', sourcefile);
			fputc (*ch, sourcefile);
		}
		fprintf (sourcefile, "\\n\" ,\n");
	}
}

void make_configs (FILE *configfile, FILE *sourcefile)
{
	make_intro (sourcefile);
	make_lines (configfile, sourcefile);
	make_ending (sourcefile);
}

int main (int argc, char *argv[])
{
	char *progname = argv[0];
	char *configname, *sourcename;
	FILE *configfile, *sourcefile;

	if (argc != 3)
		usage (progname);

	configname = argv[1];
	sourcename = argv[2];

	configfile = fopen (configname, "r");
	if (!configfile) {
		fprintf (stderr, "%s: cannot open '%s'\n",
				progname, configname);
		exit (2);
	}
	sourcefile = fopen (sourcename, "w");
	if (!sourcefile) {
		fprintf (stderr, "%s: cannot open '%s'\n",
				progname, sourcename);
		exit (2);
	}

	make_configs (configfile, sourcefile);

	if (fclose (sourcefile)) {
		fprintf (stderr, "%s: error %d closing '%s'\n",
				progname, errno, sourcename);
		exit (3);
	}
	if (fclose (configfile)) {
		fprintf (stderr, "%s: error %d closing '%s'\n",
				progname, errno, configname);
		exit (3);
	}

	exit (0);
}

/* end mkconfigs.c */
