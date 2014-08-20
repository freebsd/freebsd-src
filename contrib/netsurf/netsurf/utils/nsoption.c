/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Option reading and saving (implementation).
 *
 * Options are stored in the format key:value, one per line.
 *
 * For bool options, value is "0" or "1".
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "desktop/plot_style.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/nsoption.h"

struct nsoption_s *nsoptions = NULL;
struct nsoption_s *nsoptions_default = NULL;

#define NSOPTION_BOOL(NAME, DEFAULT) \
	{ #NAME, sizeof(#NAME) - 1, OPTION_BOOL, { .b = DEFAULT } },

#define NSOPTION_STRING(NAME, DEFAULT) \
	{ #NAME, sizeof(#NAME) - 1, OPTION_STRING, { .cs = DEFAULT } },

#define NSOPTION_INTEGER(NAME, DEFAULT) \
	{ #NAME, sizeof(#NAME) - 1, OPTION_INTEGER, { .i = DEFAULT } },

#define NSOPTION_UINT(NAME, DEFAULT) \
	{ #NAME, sizeof(#NAME) - 1, OPTION_UINT, { .u = DEFAULT } },

#define NSOPTION_COLOUR(NAME, DEFAULT) \
	{ #NAME, sizeof(#NAME) - 1, OPTION_COLOUR, { .c = DEFAULT } },

/** The table of compiled in default options */
static struct nsoption_s defaults[] = {
#include "desktop/options.h"

#if defined(riscos)
#include "riscos/options.h"
#elif defined(nsgtk)
#include "gtk/options.h"
#elif defined(nsbeos)
#include "beos/options.h"
#elif defined(nsamiga)
#include "amiga/options.h"
#elif defined(nsframebuffer)
#include "framebuffer/options.h"
#elif defined(nsatari)
#include "atari/options.h"
#elif defined(nsmonkey)
#include "monkey/options.h"
#endif
	{ NULL, 0, OPTION_INTEGER, { 0 } }
};

#undef NSOPTION_BOOL
#undef NSOPTION_STRING
#undef NSOPTION_INTEGER
#undef NSOPTION_UINT
#undef NSOPTION_COLOUR

/**
 * Set an option value based on a string
 */
static bool
strtooption(const char *value, struct nsoption_s *option)
{
	bool ret = true;
	colour rgbcolour; /* RRGGBB */

	switch (option->type) {
	case OPTION_BOOL:
		option->value.b = (value[0] == '1');
		break;

	case OPTION_INTEGER:
		option->value.i = atoi(value);
		break;

	case OPTION_UINT:
		option->value.u = strtoul(value, NULL, 0);
		break;

	case OPTION_COLOUR:
		if (sscanf(value, "%x", &rgbcolour) == 1) {
			option->value.c = (((0x000000FF & rgbcolour) << 16) |
					   ((0x0000FF00 & rgbcolour) << 0) |
					   ((0x00FF0000 & rgbcolour) >> 16));
		}
		break;

	case OPTION_STRING:
		if (option->value.s != NULL) {
			free(option->value.s);
		}

		if (*value == 0) {
			/* do not allow empty strings in text options */
			option->value.s = NULL;
		} else {
			option->value.s = strdup(value);
		}
		break;

	default:
		ret = false;
		break;
	}

	return ret;
}

/* validate options to sane values */
static void nsoption_validate(struct nsoption_s *opts, struct nsoption_s *defs)
{
	int cloop;
	bool black = true;

	if (opts[NSOPTION_font_size].value.i  < 50) {
		opts[NSOPTION_font_size].value.i = 50;
	}

	if (opts[NSOPTION_font_size].value.i > 1000) {
		opts[NSOPTION_font_size].value.i = 1000;
	}

	if (opts[NSOPTION_font_min_size].value.i < 10) {
		opts[NSOPTION_font_min_size].value.i = 10;
	}

	if (opts[NSOPTION_font_min_size].value.i > 500) {
		opts[NSOPTION_font_min_size].value.i = 500;
	}

	if (opts[NSOPTION_memory_cache_size].value.i < 0) {
		opts[NSOPTION_memory_cache_size].value.i = 0;
	}

	/* to aid migration from old, broken, configuration files this
	 * checks to see if all the system colours are set to black
	 * and returns them to defaults instead
	 */

	for (cloop = NSOPTION_SYS_COLOUR_START;
	     cloop <= NSOPTION_SYS_COLOUR_END;
	     cloop++) {
		if (opts[cloop].value.c != 0) {
			black = false;
			break;
		}
	}
	if (black == true) {
		for (cloop = NSOPTION_SYS_COLOUR_START;
		     cloop <= NSOPTION_SYS_COLOUR_END;
		     cloop++) {
			opts[cloop].value.c = defs[cloop].value.c;
		}
	}
}

/**
 * Determines if an option is different between two option tables.
 *
 * @param opts The first table to compare.
 * @param defs The second table to compare.
 * @param entry The option to compare.
 * @return true if the option differs false if not.
 */
static bool
nsoption_is_set(const struct nsoption_s *opts,
		const struct nsoption_s *defs,
		const enum nsoption_e entry)
{
	bool ret = false;

	switch (opts[entry].type) {
	case OPTION_BOOL:
		if (opts[entry].value.b != defs[entry].value.b) {
			ret = true;
		}
		break;

	case OPTION_INTEGER:
		if (opts[entry].value.i != defs[entry].value.i) {
			ret = true;
		}
		break;

	case OPTION_UINT:
		if (opts[entry].value.u != defs[entry].value.u) {
			ret = true;
		}
		break;

	case OPTION_COLOUR:
		if (opts[entry].value.c != defs[entry].value.c) {
			ret = true;
		}
		break;

	case OPTION_STRING:
		/* set if:
		 *  - defs is null.
		 *  - default is null but value is not.
		 *  - default and value pointers are different
		 *    (acts as a null check because of previous check)
		 *    and the strings content differ.
		 */
		if (((defs[entry].value.s == NULL) &&
		     (opts[entry].value.s != NULL)) ||
		    ((defs[entry].value.s != opts[entry].value.s) &&
		     (strcmp(opts[entry].value.s, defs[entry].value.s) != 0))) {
			ret = true;
		}
		break;

	}
	return ret;
}

/**
 * Output choices to file stream
 *
 * @param fp The file stream to write to.
 * @param opts The options table to write.
 * @param defs The default value table to compare with.
 * @param all Output all entries not just ones changed from defaults
 */
static nserror
nsoption_output(FILE *fp,
		struct nsoption_s *opts,
		struct nsoption_s *defs,
		bool all)
{
	unsigned int entry; /* index to option being output */
	colour rgbcolour; /* RRGGBB */

	for (entry = 0; entry < NSOPTION_LISTEND; entry++) {
		if ((all == false) &&
		    (nsoption_is_set(opts, defs, entry) == false)) {
			continue;
		}

		switch (opts[entry].type) {
		case OPTION_BOOL:
			fprintf(fp, "%s:%c\n",
				opts[entry].key,
				opts[entry].value.b ? '1' : '0');
			break;

		case OPTION_INTEGER:
			fprintf(fp, "%s:%i\n",
				opts[entry].key,
				opts[entry].value.i);

			break;

		case OPTION_UINT:
			fprintf(fp, "%s:%u\n",
				opts[entry].key,
				opts[entry].value.u);
			break;

		case OPTION_COLOUR:
			rgbcolour = (((0x000000FF & opts[entry].value.c) << 16) |
				     ((0x0000FF00 & opts[entry].value.c) << 0) |
				     ((0x00FF0000 & opts[entry].value.c) >> 16));
			fprintf(fp, "%s:%06x\n",
				opts[entry].key,
				rgbcolour);

			break;

		case OPTION_STRING:
			fprintf(fp, "%s:%s\n",
				opts[entry].key,
				((opts[entry].value.s == NULL) ||
				 (*opts[entry].value.s == 0)) ? "" : opts[entry].value.s);

			break;
		}
	}

	return NSERROR_OK;
}

/**
 * Output an option value into a string, in HTML format.
 *
 * @param option The option to output the value of.
 * @param size The size of the string buffer.
 * @param pos The current position in string
 * @param string The string in which to output the value.
 * @return The number of bytes written to string or -1 on error
 */
static size_t
nsoption_output_value_html(struct nsoption_s *option,
			   size_t size,
			   size_t pos,
			   char *string)
{
	size_t slen = 0; /* length added to string */
	colour rgbcolour; /* RRGGBB */

	switch (option->type) {
	case OPTION_BOOL:
		slen = snprintf(string + pos,
				size - pos,
				"%s",
				option->value.b ? "true" : "false");
		break;

	case OPTION_INTEGER:
		slen = snprintf(string + pos,
				size - pos,
				"%i",
				option->value.i);
		break;

	case OPTION_UINT:
		slen = snprintf(string + pos,
				size - pos,
				"%u",
				option->value.u);
		break;

	case OPTION_COLOUR:
		rgbcolour = (((0x000000FF & option->value.c) << 16) |
			     ((0x0000FF00 & option->value.c) << 0) |
			     ((0x00FF0000 & option->value.c) >> 16));
		slen = snprintf(string + pos,
				size - pos,
				"<span style=\"background-color: #%06x; "
				"color: #%06x; "
				"font-family:Monospace; \">#%06X</span>",
				rgbcolour,
				colour_to_bw_furthest(rgbcolour),
				rgbcolour);
		break;

	case OPTION_STRING:
		if (option->value.s != NULL) {
			slen = snprintf(string + pos, size - pos, "%s",
					option->value.s);
		} else {
			slen = snprintf(string + pos, size - pos,
					"<span class=\"null-content\">NULL"
					"</span>");
		}
		break;
	}

	return slen;
}


/**
 * Output an option value into a string, in plain text format.
 *
 * @param option The option to output the value of.
 * @param size The size of the string buffer.
 * @param pos The current position in string
 * @param string The string in which to output the value.
 * @return The number of bytes written to string or -1 on error
 */
static size_t
nsoption_output_value_text(struct nsoption_s *option,
			   size_t size,
			   size_t pos,
			   char *string)
{
	size_t slen = 0; /* length added to string */
	colour rgbcolour; /* RRGGBB */

	switch (option->type) {
	case OPTION_BOOL:
		slen = snprintf(string + pos,
				size - pos,
				"%c",
				option->value.b ? '1' : '0');
		break;

	case OPTION_INTEGER:
		slen = snprintf(string + pos,
				size - pos,
				"%i",
				option->value.i);
		break;

	case OPTION_UINT:
		slen = snprintf(string + pos,
				size - pos,
				"%u",
				option->value.u);
		break;

	case OPTION_COLOUR:
		rgbcolour = (((0x000000FF & option->value.c) << 16) |
			     ((0x0000FF00 & option->value.c) << 0) |
			     ((0x00FF0000 & option->value.c) >> 16));
		slen = snprintf(string + pos, size - pos, "%06x", rgbcolour);
		break;

	case OPTION_STRING:
		if (option->value.s != NULL) {
			slen = snprintf(string + pos,
					size - pos,
					"%s",
					option->value.s);
		}
		break;
	}

	return slen;
}

/**
 * Duplicates an option table.
 *
 * Allocates a new option table and copies an existing one into it.
 *
 * @param src The source table to copy
 */
static nserror
nsoption_dup(struct nsoption_s *src, struct nsoption_s **pdst)
{
	struct nsoption_s *dst;
	dst = malloc(sizeof(defaults));
	if (dst == NULL) {
		return NSERROR_NOMEM;
	}
	*pdst = dst;

	/* copy the source table into the destination table */
	memcpy(dst, src, sizeof(defaults));

	while (src->key != NULL) {
		if ((src->type == OPTION_STRING) &&
		    (src->value.s != NULL)) {
			dst->value.s = strdup(src->value.s);
		}
		src++;
		dst++;
	}

	return NSERROR_OK;
}

/**
 * frees an option table.
 *
 * Iterates through an option table a freeing resources as required
 * finally freeing the option table itself.
 *
 * @param opts The option table to free.
 */
static nserror
nsoption_free(struct nsoption_s *opts)
{
	struct nsoption_s *cur; /* option being freed */

	if (opts == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	cur = opts;

	while (cur->key != NULL) {
		if ((cur->type == OPTION_STRING) && (cur->value.s != NULL)) {
			free(cur->value.s);
		}
		cur++;
	}
	free(opts);

	return NSERROR_OK;
}


/* exported interface documented in utils/nsoption.h */
nserror
nsoption_init(nsoption_set_default_t *set_defaults,
	      struct nsoption_s **popts,
	      struct nsoption_s **pdefs)
{
	nserror ret;
	struct nsoption_s *defs;
	struct nsoption_s *opts;

	ret = nsoption_dup(&defaults[0], &defs);
	if (ret != NSERROR_OK) {
		return ret;
	}

	/* update the default table */
	if (set_defaults != NULL) {
		/** @todo it would be better if the frontends actually
		 * set values in the passed in table instead of
		 * assuming the global one.
		 */
		opts = nsoptions;
		nsoptions = defs;

		ret = set_defaults(defs);

		if (ret != NSERROR_OK) {
			nsoptions = opts;
			nsoption_free(defs);
			return ret;
		}
	}

	/* copy the default values into the working set */
	ret = nsoption_dup(defs, &opts);
	if (ret != NSERROR_OK) {
		nsoption_free(defs);
		return ret;
	}

	/* return values if wanted */
	if (popts != NULL) {
		*popts = opts;
	} else {
		nsoptions = opts;
	}

	if (pdefs != NULL) {
		*pdefs = defs;
	} else {
		nsoptions_default = defs;
	}

	return NSERROR_OK;
}

/* exported interface documented in utils/nsoption.h */
nserror nsoption_finalise(struct nsoption_s *opts, struct nsoption_s *defs)
{
	/* check to see if global table selected */
	if (opts == NULL) {
		opts = nsoptions;
	}

	nsoption_free(opts);

	/* check to see if global table selected */
	if (defs == NULL) {
		defs = nsoptions_default;
	}

	nsoption_free(defs);

	return NSERROR_OK;
}

/* exported interface documented in utils/nsoption.h */
nserror
nsoption_read(const char *path, struct nsoption_s *opts)
{
	char s[100];
	FILE *fp;
	struct nsoption_s *defs;

	if (path == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* check to see if global table selected */
	if (opts == NULL) {
		opts = nsoptions;
	}

	/** @todo is this and API bug not being a parameter */
	defs = nsoptions_default;

	fp = fopen(path, "r");
	if (!fp) {
		LOG(("Failed to open file '%s'", path));
		return NSERROR_NOT_FOUND;
	}

	LOG(("Sucessfully opened '%s' for Options file", path));

	while (fgets(s, 100, fp)) {
		char *colon, *value;
		unsigned int idx;

		if ((s[0] == 0) || (s[0] == '#')) {
			continue;
		}

		colon = strchr(s, ':');
		if (colon == 0) {
			continue;
		}

		s[strlen(s) - 1] = 0;  /* remove \n at end */
		*colon = 0;  /* terminate key */
		value = colon + 1;

		for (idx = 0; opts[idx].key != NULL; idx++) {
			if (strcasecmp(s, opts[idx].key) != 0) {
				continue;
			}

			strtooption(value, &opts[idx]);
			break;
		}
	}

	fclose(fp);

	nsoption_validate(opts, defs);

	return NSERROR_OK;
}


/* exported interface documented in utils/nsoption.h */
nserror
nsoption_write(const char *path,
	       struct nsoption_s *opts,
	       struct nsoption_s *defs)
{
	FILE *fp;
	nserror ret;

	if (path == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* check to see if global table selected */
	if (opts == NULL) {
		opts = nsoptions;
	}

	/* check to see if global table selected */
	if (defs == NULL) {
		defs = nsoptions_default;
	}

	fp = fopen(path, "w");
	if (!fp) {
		LOG(("failed to open file '%s' for writing", path));
		return NSERROR_NOT_FOUND;
	}

	ret = nsoption_output(fp, opts, defs, false);

	fclose(fp);

	return ret;
}

/* exported interface documented in utils/nsoption.h */
nserror
nsoption_dump(FILE *outf, struct nsoption_s *opts)
{
	if (outf == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* check to see if global table selected */
	if (opts == NULL) {
		opts = nsoptions;
	}

	return nsoption_output(outf, opts, NULL, true);
}


/* exported interface documented in utils/nsoption.h */
nserror
nsoption_commandline(int *pargc, char **argv, struct nsoption_s *opts)
{
	char *arg;
	char *val;
	int arglen;
	int idx = 1;
	int mv_loop;
	unsigned int entry_loop;

	/* check to see if global table selected */
	if (opts == NULL) {
		opts = nsoptions;
	}

	while (idx < *pargc) {
		arg = argv[idx];
		arglen = strlen(arg);

		/* check we have an option */
		/* option must start -- and be as long as the shortest option*/
		if ((arglen < (2+5) ) || (arg[0] != '-') || (arg[1] != '-'))
			break;

		arg += 2; /* skip -- */

		val = strchr(arg, '=');
		if (val == NULL) {
			/* no equals sign - next parameter is val */
			idx++;
			if (idx >= *pargc)
				break;
			val = argv[idx];
		} else {
			/* equals sign */
			arglen = val - arg ;
			val++;
		}

		/* arg+arglen is the option to set, val is the value */

		LOG(("%.*s = %s", arglen, arg, val));

		for (entry_loop = 0;
		     entry_loop < NSOPTION_LISTEND;
		     entry_loop++) {
			if (strncmp(arg, opts[entry_loop].key, arglen) == 0) {
				strtooption(val, opts + entry_loop);
				break;
			}
		}

		idx++;
	}

	/* remove processed options from argv */
	for (mv_loop=0; mv_loop < (*pargc - idx); mv_loop++) {
		argv[mv_loop + 1] = argv[mv_loop + idx];
	}
	*pargc -= (idx - 1);

	return NSERROR_OK;
}

/* exported interface documented in options.h */
int
nsoption_snoptionf(char *string,
		   size_t size,
		   enum nsoption_e option_idx,
		   const char *fmt)
{
	size_t slen = 0; /* current output string length */
	int fmtc = 0; /* current index into format string */
	struct nsoption_s *option;

	if (option_idx >= NSOPTION_LISTEND) {
		return -1;
	}

	option = &nsoptions[option_idx]; /* assume the global table */
	if (option == NULL || option->key == NULL)
		return -1;


	while ((slen < size) && (fmt[fmtc] != 0)) {
		if (fmt[fmtc] == '%') {
			fmtc++;
			switch (fmt[fmtc]) {
			case 'k':
				slen += snprintf(string + slen,
						 size - slen,
						 "%s",
						 option->key);
				break;

			case 'p':
				if (nsoption_is_set(nsoptions,
						    nsoptions_default,
						    option_idx)) {
					slen += snprintf(string + slen,
							 size - slen,
							 "user");
				} else {
					slen += snprintf(string + slen,
							 size - slen,
							 "default");
				}
				break;

			case 't':
				switch (option->type) {
				case OPTION_BOOL:
					slen += snprintf(string + slen,
							 size - slen,
							 "boolean");
					break;

				case OPTION_INTEGER:
					slen += snprintf(string + slen,
							 size - slen,
							 "integer");
					break;

				case OPTION_UINT:
					slen += snprintf(string + slen,
							 size - slen,
							 "unsigned integer");
					break;

				case OPTION_COLOUR:
					slen += snprintf(string + slen,
							 size - slen,
							 "colour");
					break;

				case OPTION_STRING:
					slen += snprintf(string + slen,
							 size - slen,
							 "string");
					break;

				}
				break;


			case 'V':
				slen += nsoption_output_value_html(option,
								   size,
								   slen,
								   string);
				break;
			case 'v':
				slen += nsoption_output_value_text(option,
								   size,
								   slen,
								   string);
				break;
			}
			fmtc++;
		} else {
			string[slen] = fmt[fmtc];
			slen++;
			fmtc++;
		}
	}

	/* Ensure that we NUL-terminate the output */
	string[min(slen, size - 1)] = '\0';

	return slen;
}

/* exported interface documented in options.h */
nserror
nsoption_set_tbl_charp(struct nsoption_s *opts,
		       enum nsoption_e option_idx,
		       char *s)
{
	struct nsoption_s *option;

	option = &opts[option_idx];

	/* ensure it is a string option */
	if (option->type != OPTION_STRING) {
		return NSERROR_BAD_PARAMETER;
	}

	/* free any existing string */
	if (option->value.s != NULL) {
		free(option->value.s);
	}

	option->value.s = s;

	/* check for empty string */
	if ((option->value.s != NULL) && (*option->value.s == 0)) {
		free(option->value.s);
		option->value.s = NULL;
	}
	return NSERROR_OK;
}
