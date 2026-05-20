/*
 * argvsplit.c
 * argv_split() routine
 *
 * Copyright (c) 2012, 2017 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

/*
 * !doc
 *
 * libpkgconf `argvsplit` module
 * =============================
 *
 * This is a lowlevel module which provides parsing of strings into argument vectors,
 * similar to what a shell would do.
 */

/*
 * !doc
 *
 * .. c:function:: void pkgconf_argv_free(char **argv)
 *
 *    Frees an argument vector.
 *
 *    :param char** argv: The argument vector to free.
 *    :return: nothing
 */
void
pkgconf_argv_free(char **argv)
{
	free(argv[0]);
	free(argv);
}

/*
 * !doc
 *
 * .. c:function:: int pkgconf_argv_split(const char *src, int *argc, char ***argv)
 *
 *    Splits a string into an argument vector.
 *
 *    :param char*   src: The string to split.
 *    :param int*    argc: A pointer to an integer to store the argument count.
 *    :param char*** argv: A pointer to a pointer for an argument vector.
 *    :return: 0 on success, -1 on error.
 *    :rtype: int
 */
int
pkgconf_argv_split(const char *src, int *argc, char ***argv)
{
	char *buf = calloc(1, strlen(src) + 1);
	if (buf == NULL)
		return -1;

	const char *src_iter;
	char *dst_iter;
	int argc_count = 0;
	int argv_size = 5;
	char quote = 0;
	bool escaped = false;

	src_iter = src;
	dst_iter = buf;

	*argv = calloc(argv_size, sizeof (void *));
	if (*argv == NULL)
	{
		free(buf);
		return -1;
	}

	(*argv)[argc_count] = dst_iter;

	while (*src_iter)
	{
		if (escaped)
		{
			/* POSIX: only \CHAR is special inside a double quote if CHAR is {$, `, ", \, newline}. */
			if (quote == '"')
			{
				if (!(*src_iter == '$' || *src_iter == '`' || *src_iter == '"' || *src_iter == '\\'))
					*dst_iter++ = '\\';

				*dst_iter++ = *src_iter;
			}
			else
			{
				*dst_iter++ = *src_iter;
			}

			escaped = false;
		}
		else if (quote)
		{
			if (*src_iter == quote)
				quote = 0;
			else if (*src_iter == '\\' && quote != '\'')
				escaped = true;
			else
				*dst_iter++ = *src_iter;
		}
		else if (isspace((unsigned char)*src_iter))
		{
			if ((*argv)[argc_count] != NULL)
			{
				argc_count++, dst_iter++;

				if (argc_count == argv_size)
				{
					argv_size += 5;
					*argv = realloc(*argv, sizeof(void *) * argv_size);
				}

				(*argv)[argc_count] = dst_iter;
			}
		}
		else switch(*src_iter)
		{
			case '\\':
				escaped = true;
				break;

			case '\"':
			case '\'':
				quote = *src_iter;
				break;

			default:
				*dst_iter++ = *src_iter;
				break;
		}

		src_iter++;
	}

	if (escaped || quote)
	{
		free(*argv);
		free(buf);
		return -1;
	}

	if (strlen((*argv)[argc_count]))
	{
		argc_count++;
	}

	*argc = argc_count;
	return 0;
}
