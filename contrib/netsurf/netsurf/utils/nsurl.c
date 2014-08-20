/*
 * Copyright 2011 Michael Drake <tlsa@netsurf-browser.org>
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
 * NetSurf URL handling (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <libwapcaplet/libwapcaplet.h>
#include <stdlib.h>
#include <string.h>

#include "utils/corestrings.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/utils.h"


/* Define to enable NSURL debugging */
#undef NSURL_DEBUG

static bool nsurl__is_unreserved(unsigned char c)
{
	/* From RFC3986 section 2.3 (unreserved characters) 
	 *
	 *      unreserved  = ALPHA / DIGIT / "-" / "." / "_" / "~"
	 *
	 */
	static const bool unreserved[256] = {
		false, false, false, false, false, false, false, false, /* 00 */
		false, false, false, false, false, false, false, false, /* 08 */
		false, false, false, false, false, false, false, false, /* 10 */
		false, false, false, false, false, false, false, false, /* 18 */
		false, false, false, false, false, false, false, false, /* 20 */
		false, false, false, false, false, true,  true,  false, /* 28 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 30 */
		true,  true,  false, false, false, false, false, false, /* 38 */
		false, true,  true,  true,  true,  true,  true,  true,  /* 40 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 48 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 50 */
		true,  true,  true,  false, false, false, false, true,  /* 58 */
		false, true,  true,  true,  true,  true,  true,  true,  /* 60 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 68 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 70 */
		true,  true,  true,  false, false, false, true,  false, /* 78 */
		false, false, false, false, false, false, false, false, /* 80 */
		false, false, false, false, false, false, false, false, /* 88 */
		false, false, false, false, false, false, false, false, /* 90 */
		false, false, false, false, false, false, false, false, /* 98 */
		false, false, false, false, false, false, false, false, /* A0 */
		false, false, false, false, false, false, false, false, /* A8 */
		false, false, false, false, false, false, false, false, /* B0 */
		false, false, false, false, false, false, false, false, /* B8 */
		false, false, false, false, false, false, false, false, /* C0 */
		false, false, false, false, false, false, false, false, /* C8 */
		false, false, false, false, false, false, false, false, /* D0 */
		false, false, false, false, false, false, false, false, /* D8 */
		false, false, false, false, false, false, false, false, /* E0 */
		false, false, false, false, false, false, false, false, /* E8 */
		false, false, false, false, false, false, false, false, /* F0 */
		false, false, false, false, false, false, false, false  /* F8 */
	};
	return unreserved[c];
}

/* The ASCII codes which should not be percent escaped */
static bool nsurl__is_no_escape(unsigned char c)
{
	static const bool no_escape[256] = {
		false, false, false, false, false, false, false, false, /* 00 */
		false, false, false, false, false, false, false, false, /* 08 */
		false, false, false, false, false, false, false, false, /* 10 */
		false, false, false, false, false, false, false, false, /* 18 */
		false, true,  false, true,  true,  false, true,  true,  /* 20 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 28 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 30 */
		true,  true,  true,  true,  false, true,  false, true,  /* 38 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 40 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 48 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 50 */
		true,  true,  true,  true,  false, true,  false, true,  /* 58 */
		false, true,  true,  true,  true,  true,  true,  true,  /* 60 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 68 */
		true,  true,  true,  true,  true,  true,  true,  true,  /* 70 */
		true,  true,  true,  false, true,  false, true,  false, /* 78 */
		false, false, false, false, false, false, false, false, /* 80 */
		false, false, false, false, false, false, false, false, /* 88 */
		false, false, false, false, false, false, false, false, /* 90 */
		false, false, false, false, false, false, false, false, /* 98 */
		false, false, false, false, false, false, false, false, /* A0 */
		false, false, false, false, false, false, false, false, /* A8 */
		false, false, false, false, false, false, false, false, /* B0 */
		false, false, false, false, false, false, false, false, /* B8 */
		false, false, false, false, false, false, false, false, /* C0 */
		false, false, false, false, false, false, false, false, /* C8 */
		false, false, false, false, false, false, false, false, /* D0 */
		false, false, false, false, false, false, false, false, /* D8 */
		false, false, false, false, false, false, false, false, /* E0 */
		false, false, false, false, false, false, false, false, /* E8 */
		false, false, false, false, false, false, false, false, /* F0 */
		false, false, false, false, false, false, false, false, /* F8 */
	};
	return no_escape[c];
}


/** nsurl scheme type */
enum scheme_type {
	NSURL_SCHEME_OTHER,
	NSURL_SCHEME_HTTP,
	NSURL_SCHEME_HTTPS,
	NSURL_SCHEME_FTP,
	NSURL_SCHEME_MAILTO
};


/** nsurl components */
struct nsurl_components {
	lwc_string *scheme;
	lwc_string *username;
	lwc_string *password;
	lwc_string *host;
	lwc_string *port;
	lwc_string *path;
	lwc_string *query;
	lwc_string *fragment;

	enum scheme_type scheme_type;
};


/**
 * NetSurf URL object
 *
 * [scheme]://[username][:password]@[host]:[port][/path][?query][#fragment]
 */
struct nsurl {
	struct nsurl_components components;

	int count;	/* Number of references to NetSurf URL object */
	uint32_t hash;	/* Hash value for nsurl identification */

	size_t length;	/* Length of string */
	char string[FLEX_ARRAY_LEN_DECL];	/* Full URL as a string */
};


/** Marker set, indicating positions of sections within a URL string */
struct url_markers {
	size_t start; /** start of URL */
	size_t scheme_end;
	size_t authority;

	size_t colon_first;
	size_t at;
	size_t colon_last;

	size_t path;
	size_t query;
	size_t fragment;

	size_t end; /** end of URL */

	enum scheme_type scheme_type;
};


/** Marker set, indicating positions of sections within a URL string */
struct nsurl_component_lengths {
	size_t scheme;
	size_t username;
	size_t password;
	size_t host;
	size_t port;
	size_t path;
	size_t query;
	size_t fragment;
};


/** Flags indicating which parts of a URL string are required for a nsurl */
enum nsurl_string_flags {
	NSURL_F_SCHEME			= (1 << 0),
	NSURL_F_SCHEME_PUNCTUATION	= (1 << 1),
	NSURL_F_AUTHORITY_PUNCTUATION	= (1 << 2),
	NSURL_F_USERNAME		= (1 << 3),
	NSURL_F_PASSWORD		= (1 << 4),
	NSURL_F_CREDENTIALS_PUNCTUATION	= (1 << 5),
	NSURL_F_HOST			= (1 << 6),
	NSURL_F_PORT			= (1 << 7),
	NSURL_F_AUTHORITY		= (NSURL_F_USERNAME |
						NSURL_F_PASSWORD |
						NSURL_F_HOST |
						NSURL_F_PORT),
	NSURL_F_PATH			= (1 << 8),
	NSURL_F_QUERY			= (1 << 9),
	NSURL_F_FRAGMENT_PUNCTUATION	= (1 << 10),
	NSURL_F_FRAGMENT		= (1 << 11)
};


/** Sections of a URL */
enum url_sections {
	URL_SCHEME,
	URL_CREDENTIALS,
	URL_HOST,
	URL_PATH,
	URL_QUERY,
	URL_FRAGMENT
};


#define nsurl__component_copy(c) (c == NULL) ? NULL : lwc_string_ref(c)

#define nsurl__component_compare(c1, c2, match)	      		\
	if (c1 && c2 && lwc_error_ok ==				\
			lwc_string_isequal(c1, c2, match)) {	\
		/* do nothing */                                \
	} else if (c1 || c2) {					\
		*match = false;					\
	}


/**
 * Obtains a set of markers delimiting sections in a URL string
 *
 * \param url_s		URL string
 * \param markers	Updated to mark sections in the URL string
 * \param joining	True iff URL string is a relative URL for joining
 */
static void nsurl__get_string_markers(const char * const url_s,
		struct url_markers *markers, bool joining)
{
	const char *pos = url_s; /** current position in url_s */
	bool is_http = false;
	bool trailing_whitespace = false;

	/* Initialise marker set */
	struct url_markers marker = { 0, 0, 0,   0, 0, 0,
				      0, 0, 0,   0, NSURL_SCHEME_OTHER };

	/* Skip any leading whitespace in url_s */
	while (isspace(*pos))
		pos++;

	/* Record start point */
	marker.start = pos - url_s;

	marker.scheme_end = marker.authority = marker.colon_first = marker.at =
			marker.colon_last = marker.path = marker.start;

	if (*pos == '\0') {
		/* Nothing but whitespace, early exit */
		marker.query = marker.fragment = marker.end = marker.path;
		*markers = marker;
		return;
	}

	/* Get scheme */
	if (isalpha(*pos)) {
		pos++;

		while (*pos != ':' && *pos != '\0') {
			if (!isalnum(*pos) && *pos != '+' &&
					*pos != '-' && *pos != '.') {
				/* This character is not valid in the
				 * scheme */
				break;
			}
			pos++;
		}

		if (*pos == ':') {
			/* This delimits the end of the scheme */
			size_t off;

			marker.scheme_end = pos - url_s;

			off = marker.scheme_end - marker.start;

			/* Detect http(s) and mailto for scheme specifc
			 * normalisation */
			if (off == SLEN("http") &&
					(((*(pos - off + 0) == 'h') ||
					  (*(pos - off + 0) == 'H')) &&
					 ((*(pos - off + 1) == 't') ||
					  (*(pos - off + 1) == 'T')) &&
					 ((*(pos - off + 2) == 't') ||
					  (*(pos - off + 2) == 'T')) &&
					 ((*(pos - off + 3) == 'p') ||
					  (*(pos - off + 3) == 'P')))) {
				marker.scheme_type = NSURL_SCHEME_HTTP;
				is_http = true;
			} else if (off == SLEN("https") &&
					(((*(pos - off + 0) == 'h') ||
					  (*(pos - off + 0) == 'H')) &&
					 ((*(pos - off + 1) == 't') ||
					  (*(pos - off + 1) == 'T')) &&
					 ((*(pos - off + 2) == 't') ||
					  (*(pos - off + 2) == 'T')) &&
					 ((*(pos - off + 3) == 'p') ||
					  (*(pos - off + 3) == 'P')) &&
					 ((*(pos - off + 4) == 's') ||
					  (*(pos - off + 4) == 'S')))) {
				marker.scheme_type = NSURL_SCHEME_HTTPS;
				is_http = true;
			} else if (off == SLEN("ftp") &&
					(((*(pos - off + 0) == 'f') ||
					  (*(pos - off + 0) == 'F')) &&
					 ((*(pos - off + 1) == 't') ||
					  (*(pos - off + 1) == 'T')) &&
					 ((*(pos - off + 2) == 'p') ||
					  (*(pos - off + 2) == 'P')))) {
				marker.scheme_type = NSURL_SCHEME_FTP;
			} else if (off == SLEN("mailto") &&
					(((*(pos - off + 0) == 'm') ||
					  (*(pos - off + 0) == 'M')) &&
					 ((*(pos - off + 1) == 'a') ||
					  (*(pos - off + 1) == 'A')) &&
					 ((*(pos - off + 2) == 'i') ||
					  (*(pos - off + 2) == 'I')) &&
					 ((*(pos - off + 3) == 'l') ||
					  (*(pos - off + 3) == 'L')) &&
					 ((*(pos - off + 4) == 't') ||
					  (*(pos - off + 4) == 'T')) &&
					 ((*(pos - off + 5) == 'o') ||
					  (*(pos - off + 5) == 'O')))) {
				marker.scheme_type = NSURL_SCHEME_MAILTO;
			}

			/* Skip over colon */
			pos++;

			/* Mark place as start of authority */
			marker.authority = marker.colon_first = marker.at =
					marker.colon_last = marker.path =
					pos - url_s;

		} else {
			/* Not found a scheme  */
			if (joining == false) {
				/* Assuming no scheme == http */
				marker.scheme_type = NSURL_SCHEME_HTTP;
				is_http = true;
			}
		}
	}

	/* Get authority
	 *
	 * Two slashes always indicates the start of an authority.
	 *
	 * We are more relaxed in the case of http:
	 *   a. when joining, one or more slashes indicates start of authority
	 *   b. when not joining, we assume authority if no scheme was present
	 * and in the case of mailto: when we assume there is an authority.
	 */
	if ((*pos == '/' && *(pos + 1) == '/') ||
			(is_http && ((joining && *pos == '/') || 
					(joining == false &&
					marker.scheme_end != marker.start))) ||
			marker.scheme_type == NSURL_SCHEME_MAILTO) {

		/* Skip over leading slashes */
		if (*pos == '/') {
			if (is_http == false) {
				if (*pos == '/') pos++;
				if (*pos == '/') pos++;
			} else {
				while (*pos == '/')
					pos++;
			}

			marker.authority = marker.colon_first = marker.at =
					marker.colon_last = marker.path =
					pos - url_s;
		}

		/* Need to get (or complete) the authority */
		while (*pos != '\0') {
			if (*pos == '/' || *pos == '?' || *pos == '#') {
				/* End of the authority */
				break;

			} else if (*pos == ':' && marker.colon_first ==
					marker.authority) {
				/* could be username:password or host:port
				 * separator */
				marker.colon_first = pos - url_s;

			} else if (*pos == ':' && marker.colon_first !=
					marker.authority) {
				/* could be host:port separator */
				marker.colon_last = pos - url_s;

			} else if (*pos == '@' && marker.at ==
					marker.authority) {
				/* Credentials @ host separator */
				marker.at = pos - url_s;
			}

			pos++;
		}

		marker.path = pos - url_s;

	} else if ((*pos == '\0' || *pos == '/') &&
			joining == false && is_http == true) {
		marker.path = pos - url_s;
	}

	/* Get path
	 *
	 * Needs to start with '/' if there's no authority
	 */
	if (*pos == '/' || ((marker.path == marker.authority) &&
			(*pos != '?') && (*pos != '#') && (*pos != '\0'))) {
		while (*(++pos) != '\0') {
			if (*pos == '?' || *pos == '#') {
				/* End of the path */
				break;
			}
		}
	}

	marker.query = pos - url_s;

	/* Get query */
	if (*pos == '?') {
		while (*(++pos) != '\0') {
			if (*pos == '#') {
				/* End of the query */
				break;
			}
		}
	}

	marker.fragment = pos - url_s;

	/* Get fragment */
	if (*pos == '#') {
		while (*(++pos) != '\0')
			;
	}

	/* We got to the end of url_s.
	 * Need to skip back over trailing whitespace to find end of URL */
	pos--;
	if (pos >= url_s && isspace(*pos)) {
		trailing_whitespace = true;
		while (pos >= url_s && isspace(*pos))
			pos--;
	}

	marker.end = pos + 1 - url_s;

	if (trailing_whitespace == true) {
		/* Ensure last url section doesn't pass end */
		if (marker.fragment > marker.end)
			marker.fragment = marker.end;
		if (marker.query > marker.end)
			marker.query = marker.end;
		if (marker.path > marker.end)
			marker.path = marker.end;
		if (marker.colon_last > marker.end)
			marker.colon_last = marker.end;
		if (marker.at > marker.end)
			marker.at = marker.end;
		if (marker.colon_last > marker.end)
			marker.colon_last = marker.end;
		if (marker.fragment > marker.end)
			marker.fragment = marker.end;
	}

	/* Got all the URL components pegged out now */
	*markers = marker;
}


/**
 * Remove dot segments from a path, as per rfc 3986, 5.2.4
 *
 * \param path		path to remove dot segments from ('\0' terminated)
 * \param output	path with dot segments removed
 * \return size of output
 */
static size_t nsurl__remove_dot_segments(char *path, char *output)
{
	char *path_pos = path;
	char *output_pos = output;

	while (*path_pos != '\0') {
#ifdef NSURL_DEBUG
		LOG((" in:%s", path_pos));
		LOG(("out:%.*s", output_pos - output, output));
#endif
		if (*path_pos == '.') {
			if (*(path_pos + 1) == '.' &&
					*(path_pos + 2) == '/') {
				/* Found prefix of "../" */
				path_pos += SLEN("../");
				continue;

			} else if (*(path_pos + 1) == '/') {
				/* Found prefix of "./" */
				path_pos += SLEN("./");
				continue;
			}
		} else if (*path_pos == '/' && *(path_pos + 1) == '.') {
			if (*(path_pos + 2) == '/') {
				/* Found prefix of "/./" */
				path_pos += SLEN("/.");
				continue;

			} else if (*(path_pos + 2) == '\0') {
				/* Found "/." at end of path */
				*(output_pos++) = '/';

				/* End of input path */
				break;

			} else if (*(path_pos + 2) == '.') {
				if (*(path_pos + 3) == '/') {
					/* Found prefix of "/../" */
					path_pos += SLEN("/..");

					if (output_pos > output)
						output_pos--;
					while (output_pos > output &&
							*output_pos != '/')
						output_pos--;

					continue;

				} else if (*(path_pos + 3) == '\0') {
					/* Found "/.." at end of path */

					while (output_pos > output &&
							*(output_pos -1 ) !='/')
						output_pos--;

					/* End of input path */
					break;
				}
			}
		} else if (*path_pos == '.') {
			if (*(path_pos + 1) == '\0') {
				/* Found "." at end of path */

				/* End of input path */
				break;

			} else if (*(path_pos + 1) == '.' &&
					*(path_pos + 2) == '\0') {
				/* Found ".." at end of path */

				/* End of input path */
				break;
			}
		}
		/* Copy first character into output path */
		*output_pos++ = *path_pos++;

		/* Copy up to but not including next '/' */
		  while ((*path_pos != '/') && (*path_pos != '\0'))
		  	*output_pos++ = *path_pos++;
	}

	return output_pos - output;
}


/**
 * Get the length of the longest section
 *
 * \param m	markers delimiting url sections in a string
 * \return the length of the longest section
 */
static size_t nsurl__get_longest_section(struct url_markers *m)
{
	size_t length = m->scheme_end - m->start;	/* scheme */

	if (length < m->at - m->authority)		/* credentials */
		length = m->at - m->authority;

	if (length < m->path - m->at)			/* host */
		length = m->path - m->at;

	if (length < m->query - m->path)		/* path */
		length = m->query - m->path;

	if (length < m->fragment - m->query)		/* query */
		length = m->fragment - m->query;

	if (length < m->end - m->fragment)		/* fragment */
		length = m->end - m->fragment;

	return length;
}


/**
 * Converts two hexadecimal digits to a single number
 *
 * \param c1	most significant hex digit
 * \param c2	least significant hex digit
 * \return the total value of the two digit hex number, or -ve if input not hex
 *
 * For unescaping url encoded characters.
 */
static inline int nsurl__get_ascii_offset(char c1, char c2)
{
	int offset;

	/* Use 1st char as most significant hex digit */
	if (isdigit(c1))
		offset = 16 * (c1 - '0');
	else if (c1 >= 'a' && c1 <= 'f')
		offset = 16 * (c1 - 'a' + 10);
	else if (c1 >= 'A' && c1 <= 'F')
		offset = 16 * (c1 - 'A' + 10);
	else
		/* Not valid hex */
		return -1;

	/* Use 2nd char as least significant hex digit and sum */
	if (isdigit(c2))
		offset += c2 - '0';
	else if (c2 >= 'a' && c2 <= 'f')
		offset += c2 - 'a' + 10;
	else if (c2 >= 'A' && c2 <= 'F')
		offset += c2 - 'A' + 10;
	else
		/* Not valid hex */
		return -1;

	return offset;
}


/**
 * Create the components of a NetSurf URL object for a section of a URL string
 *
 * \param url_s		URL string
 * \param section	Sets which section of URL string is to be normalised
 * \param pegs		Set of markers delimiting the URL string's sections
 * \param pos_norm	A buffer large enough for the normalised string (*3 + 1)
 * \param url		A NetSurf URL object, to which components may be added
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * The section of url_s is normalised appropriately.
 */
static nserror nsurl__create_from_section(const char * const url_s,
		const enum url_sections section,
		const struct url_markers *pegs,
		char *pos_norm,
		struct nsurl_components *url)
{
	int ascii_offset;
	int start = 0;
	int end = 0;
	const char *pos;
	const char *pos_url_s;
	char *norm_start = pos_norm;
	size_t copy_len;
	size_t length;
	enum {
		NSURL_F_NO_PORT		= (1 << 0)
	} flags = 0;

	switch (section) {
	case URL_SCHEME:
		start = pegs->start;
		end = pegs->scheme_end;
		break;

	case URL_CREDENTIALS:
		start = pegs->authority;
		end = pegs->at;
		break;

	case URL_HOST:
		start = (pegs->at == pegs->authority &&
				*(url_s + pegs->at) != '@') ?
				pegs->at :
				pegs->at + 1;
		end = pegs->path;
		break;

	case URL_PATH:
		start = pegs->path;
		end = pegs->query;
		break;

	case URL_QUERY:
		start = pegs->query;
		end = pegs->fragment;
		break;

	case URL_FRAGMENT:
		start = (*(url_s + pegs->fragment) != '#') ?
				pegs->fragment :
				pegs->fragment + 1;
		end = pegs->end;
		break;
	}

	if (end < start)
		end = start;

	length = end - start;

	/* Stage 1: Normalise the required section */

	pos = pos_url_s = url_s + start;
	copy_len = 0;
	for (; pos < url_s + end; pos++) {
		if (*pos == '%' && (pos + 2 < url_s + end)) {
			/* Might be an escaped character needing unescaped */

			/* Find which character which was escaped */
			ascii_offset = nsurl__get_ascii_offset(*(pos + 1),
					*(pos + 2));

			if (ascii_offset < 0) {
				/* % with invalid hex digits. */
				copy_len++;
				continue;
			}

			if (nsurl__is_unreserved(ascii_offset) == false) {
				/* This character should be escaped after all,
				 * just let it get copied */
				copy_len += 3;
				pos += 2;
				continue;
			}

			if (copy_len > 0) {
				/* Copy up to here */
				memcpy(pos_norm, pos_url_s, copy_len);
				pos_norm += copy_len;
				copy_len = 0;
			}

			/* Put the unescaped character in the normalised URL */
			*(pos_norm++) = (char)ascii_offset;
			pos += 2;
			pos_url_s = pos + 1;

			length -= 2;

		} else if (nsurl__is_no_escape(*pos) == false) {

			/* This needs to be escaped */
			if (copy_len > 0) {
				/* Copy up to here */
				memcpy(pos_norm, pos_url_s, copy_len);
				pos_norm += copy_len;
				copy_len = 0;
			}

			/* escape */
			*(pos_norm++) = '%';
			*(pos_norm++) = digit2uppercase_hex(
					((unsigned char)*pos) >> 4);
			*(pos_norm++) = digit2uppercase_hex(
					((unsigned char)*pos) & 0xf);
			pos_url_s = pos + 1;

			length += 2;

		} else if ((section == URL_SCHEME || section == URL_HOST) &&
				isupper(*pos)) {
			/* Lower case this letter */

			if (copy_len > 0) {
				/* Copy up to here */
				memcpy(pos_norm, pos_url_s, copy_len);
				pos_norm += copy_len;
				copy_len = 0;
			}
			/* Copy lower cased letter into normalised URL */
			*(pos_norm++) = tolower(*pos);
			pos_url_s = pos + 1;

		} else {
			/* This character is safe in normalised URL */
			copy_len++;
		}
	}

	if (copy_len > 0) {
		/* Copy up to here */
		memcpy(pos_norm, pos_url_s, copy_len);
		pos_norm += copy_len;
	}

	/* Mark end of section */
	(*pos_norm) = '\0';

	/* Stage 2: Create the URL components for the required section */
	switch (section) {
	case URL_SCHEME:
		if (length == 0) {
			/* No scheme, assuming http */
			url->scheme = lwc_string_ref(corestring_lwc_http);
		} else {
			/* Add scheme to URL */
			if (lwc_intern_string(norm_start, length,
					&url->scheme) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;

	case URL_CREDENTIALS:
		url->username = NULL;
		url->password = NULL;

		if (length != 0 && *norm_start != ':') {
			char *sec_start = norm_start;
			if (pegs->colon_first != pegs->authority &&
					pegs->at > pegs->colon_first + 1) {
				/* there's a password */
				sec_start += pegs->colon_first -
						pegs->authority + 1;
				if (lwc_intern_string(sec_start,
						pegs->at - pegs->colon_first -1,
						&url->password) !=
						lwc_error_ok) {
					return NSERROR_NOMEM;
				}

				/* update start pos and length for username */
				sec_start = norm_start;
				length -= pegs->at - pegs->colon_first;
			} else if (pegs->colon_first != pegs->authority &&
					pegs->at == pegs->colon_first + 1) {
				/* strip username colon */
				length--;
			}

			/* Username */
			if (lwc_intern_string(sec_start, length,
					&url->username) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;

	case URL_HOST:
		url->host = NULL;
		url->port = NULL;

		if (length != 0) {
			size_t colon = 0;
			char *sec_start = norm_start;
			if (pegs->at < pegs->colon_first &&
					pegs->colon_last == pegs->authority) {
				/* There's one colon and it's after @ marker */
				colon = pegs->colon_first;
			} else if (pegs->colon_last != pegs->authority) {
				/* There's more than one colon */
				colon = pegs->colon_last;
			} else {
				/* There's no colon that could be a port
				 * separator */
				flags |= NSURL_F_NO_PORT;
			}

			if (!(flags & NSURL_F_NO_PORT)) {
				/* Determine whether colon is a port separator
				 */
				sec_start += colon - pegs->at;
				while (++sec_start < norm_start + length) {
					if (!isdigit(*sec_start)) {
						/* Character after port isn't a
						 * digit; not a port separator
						 */
						flags |= NSURL_F_NO_PORT;
						break;
					}
				}
			}

			if (!(flags & NSURL_F_NO_PORT)) {
				/* There's a port */
				size_t skip = (pegs->at == pegs->authority) ?
						1 : 0;
				sec_start = norm_start + colon - pegs->at +
						skip;
				if (url->scheme != NULL &&
						url->scheme_type ==
						NSURL_SCHEME_HTTP &&
						length -
						(colon - pegs->at + skip) == 2 &&
						*sec_start == '8' &&
						*(sec_start + 1) == '0') {
					/* Scheme is http, and port is default
					 * (80) */
					flags |= NSURL_F_NO_PORT;
				}

				if (length - (colon - pegs->at + skip) <= 0) {
					/* No space for a port after the colon
					 */
					flags |= NSURL_F_NO_PORT;
				}

				/* Add non-redundant ports to NetSurf URL */
				sec_start = norm_start + colon - pegs->at +
						skip;
				if (!(flags & NSURL_F_NO_PORT) &&
						lwc_intern_string(sec_start,
						length -
						(colon - pegs->at + skip),
						&url->port) != lwc_error_ok) {
					return NSERROR_NOMEM;
				}

				/* update length for host */
				skip = (pegs->at == pegs->authority) ? 0 : 1;
				length = colon - pegs->at - skip;
			}

			/* host */
			if (lwc_intern_string(norm_start, length,
					&url->host) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		}

		break;

	case URL_PATH:
		if (length != 0) {
			if (lwc_intern_string(norm_start, length,
					&url->path) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		} else if (url->host != NULL &&
				url->scheme_type != NSURL_SCHEME_MAILTO) {
			/* Set empty path to "/", if there's a host */
			if (lwc_intern_string("/", SLEN("/"),
					&url->path) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		} else {
			url->path = NULL;
		}

		break;

	case URL_QUERY:
		if (length != 0) {
			if (lwc_intern_string(norm_start, length,
					&url->query) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		} else {
			url->query = NULL;
		}

		break;

	case URL_FRAGMENT:
		if (length != 0) {
			if (lwc_intern_string(norm_start, length,
					&url->fragment) != lwc_error_ok) {
				return NSERROR_NOMEM;
			}
		} else {
			url->fragment = NULL;
		}

		break;
	}

	return NSERROR_OK;
}


/**
 * Get nsurl string info; total length, component lengths, & components present
 *
 * \param url		NetSurf URL components
 * \param parts		Which parts of the URL are required in the string
 * \param url_l		Updated to total string length
 * \param lengths	Updated with individual component lengths
 * \param pflags	Updated to contain relevant string flags
 */
static void nsurl__get_string_data(const struct nsurl_components *url,
		nsurl_component parts, size_t *url_l,
		struct nsurl_component_lengths *lengths,
		enum nsurl_string_flags *pflags)
{
	enum nsurl_string_flags flags = *pflags;
	*url_l = 0;

	/* Intersection of required parts and available parts gives
	 * the output parts */
	if (url->scheme && parts & NSURL_SCHEME) {
		flags |= NSURL_F_SCHEME;

		lengths->scheme = lwc_string_length(url->scheme);
		*url_l += lengths->scheme;
	}

	if (url->username && parts & NSURL_USERNAME) {
		flags |= NSURL_F_USERNAME;

		lengths->username = lwc_string_length(url->username);
		*url_l += lengths->username;
	}

	if (url->password && parts & NSURL_PASSWORD) {
		flags |= NSURL_F_PASSWORD;

		lengths->password = lwc_string_length(url->password);
		*url_l += SLEN(":") + lengths->password;
	}

	if (url->host && parts & NSURL_HOST) {
		flags |= NSURL_F_HOST;

		lengths->host = lwc_string_length(url->host);
		*url_l += lengths->host;
	}

	if (url->port && parts & NSURL_PORT) {
		flags |= NSURL_F_PORT;

		lengths->port = lwc_string_length(url->port);
		*url_l += SLEN(":") + lengths->port;
	}

	if (url->path && parts & NSURL_PATH) {
		flags |= NSURL_F_PATH;

		lengths->path = lwc_string_length(url->path);
		*url_l += lengths->path;
	}

	if (url->query && parts & NSURL_QUERY) {
		flags |= NSURL_F_QUERY;

		lengths->query = lwc_string_length(url->query);
		*url_l += lengths->query;
	}

	if (url->fragment && parts & NSURL_FRAGMENT) {
		flags |= NSURL_F_FRAGMENT;

		lengths->fragment = lwc_string_length(url->fragment);
		*url_l += lengths->fragment;
	}

	/* Turn on any spanned punctuation */
	if ((flags & NSURL_F_SCHEME) && (parts > NSURL_SCHEME)) {
		flags |= NSURL_F_SCHEME_PUNCTUATION;

		*url_l += SLEN(":");
	}

	if ((flags & NSURL_F_SCHEME) && (flags > NSURL_F_SCHEME) &&
			url->path && lwc_string_data(url->path)[0] == '/') {
		flags |= NSURL_F_AUTHORITY_PUNCTUATION;

		*url_l += SLEN("//");
	}

	if ((flags & (NSURL_F_USERNAME | NSURL_F_PASSWORD)) &&
				flags & NSURL_F_HOST) {
		flags |= NSURL_F_CREDENTIALS_PUNCTUATION;

		*url_l += SLEN("@");
	}

	if ((flags & ~NSURL_F_FRAGMENT) && (flags & NSURL_F_FRAGMENT)) {
		flags |= NSURL_F_FRAGMENT_PUNCTUATION;

		*url_l += SLEN("#");
	}

	*pflags = flags;
}


/**
 * Get nsurl string info; total length, component lengths, & components present
 *
 * \param url		NetSurf URL components
 * \param url_s		Updated to contain the string
 * \param l		Individual component lengths
 * \param flags		String flags
 */
static void nsurl_get_string(const struct nsurl_components *url, char *url_s,
		struct nsurl_component_lengths *l,
		enum nsurl_string_flags flags)
{
	char *pos;

	/* Copy the required parts into the url string */
	pos = url_s;

	if (flags & NSURL_F_SCHEME) {
		memcpy(pos, lwc_string_data(url->scheme), l->scheme);
		pos += l->scheme;
	}

	if (flags & NSURL_F_SCHEME_PUNCTUATION) {
		*(pos++) = ':';
	}

	if (flags & NSURL_F_AUTHORITY_PUNCTUATION) {
		*(pos++) = '/';
		*(pos++) = '/';
	}

	if (flags & NSURL_F_USERNAME) {
		memcpy(pos, lwc_string_data(url->username), l->username);
		pos += l->username;
	}

	if (flags & NSURL_F_PASSWORD) {
		*(pos++) = ':';
		memcpy(pos, lwc_string_data(url->password), l->password);
		pos += l->password;
	}

	if (flags & NSURL_F_CREDENTIALS_PUNCTUATION) {
		*(pos++) = '@';
	}

	if (flags & NSURL_F_HOST) {
		memcpy(pos, lwc_string_data(url->host), l->host);
		pos += l->host;
	}

	if (flags & NSURL_F_PORT) {
		*(pos++) = ':';
		memcpy(pos, lwc_string_data(url->port), l->port);
		pos += l->port;
	}

	if (flags & NSURL_F_PATH) {
		memcpy(pos, lwc_string_data(url->path), l->path);
		pos += l->path;
	}

	if (flags & NSURL_F_QUERY) {
		memcpy(pos, lwc_string_data(url->query), l->query);
		pos += l->query;
	}

	if (flags & NSURL_F_FRAGMENT) {
		if (flags & NSURL_F_FRAGMENT_PUNCTUATION)
			*(pos++) = '#';
		memcpy(pos, lwc_string_data(url->fragment), l->fragment);
		pos += l->fragment;
	}

	*pos = '\0';
}


/**
 * Calculate hash value
 *
 * \param url		NetSurf URL object to set hash value for
 */
static void nsurl_calc_hash(nsurl *url)
{
	uint32_t hash = 0;

	if (url->components.scheme)
		hash ^= lwc_string_hash_value(url->components.scheme);

	if (url->components.username)
		hash ^= lwc_string_hash_value(url->components.username);

	if (url->components.password)
		hash ^= lwc_string_hash_value(url->components.password);

	if (url->components.host)
		hash ^= lwc_string_hash_value(url->components.host);

	if (url->components.port)
		hash ^= lwc_string_hash_value(url->components.port);

	if (url->components.path)
		hash ^= lwc_string_hash_value(url->components.path);

	if (url->components.query)
		hash ^= lwc_string_hash_value(url->components.query);

	if (url->components.fragment)
		hash ^= lwc_string_hash_value(url->components.fragment);

	url->hash = hash;
}


/**
 * Destroy components
 *
 * \param c	url components
 */
static void nsurl_destroy_components(struct nsurl_components *c)
{
	if (c->scheme)
		lwc_string_unref(c->scheme);

	if (c->username)
		lwc_string_unref(c->username);

	if (c->password)
		lwc_string_unref(c->password);

	if (c->host)
		lwc_string_unref(c->host);

	if (c->port)
		lwc_string_unref(c->port);

	if (c->path)
		lwc_string_unref(c->path);

	if (c->query)
		lwc_string_unref(c->query);

	if (c->fragment)
		lwc_string_unref(c->fragment);
}


#ifdef NSURL_DEBUG
/**
 * Dump a NetSurf URL's internal components
 *
 * \param url	The NetSurf URL to dump components of
 */
static void nsurl__dump(const nsurl *url)
{
	if (url->components.scheme)
		LOG(("  Scheme: %s", lwc_string_data(url->components.scheme)));

	if (url->components.username)
		LOG(("Username: %s",
				lwc_string_data(url->components.username)));

	if (url->components.password)
		LOG(("Password: %s",
				lwc_string_data(url->components.password)));

	if (url->components.host)
		LOG(("    Host: %s", lwc_string_data(url->components.host)));

	if (url->components.port)
		LOG(("    Port: %s", lwc_string_data(url->components.port)));

	if (url->components.path)
		LOG(("    Path: %s", lwc_string_data(url->components.path)));

	if (url->components.query)
		LOG(("   Query: %s", lwc_string_data(url->components.query)));

	if (url->components.fragment)
		LOG(("Fragment: %s",
				lwc_string_data(url->components.fragment)));
}
#endif

/******************************************************************************
 * NetSurf URL Public API                                                     *
 ******************************************************************************/

/* exported interface, documented in nsurl.h */
nserror nsurl_create(const char * const url_s, nsurl **url)
{
	struct url_markers m;
	struct nsurl_components c;
	size_t length;
	char *buff;
	struct nsurl_component_lengths str_len = { 0, 0, 0, 0,  0, 0, 0, 0 };
	enum nsurl_string_flags str_flags = 0;
	nserror e = NSERROR_OK;
	bool match;

	assert(url_s != NULL);

	/* Peg out the URL sections */
	nsurl__get_string_markers(url_s, &m, false);

	/* Get the length of the longest section */
	length = nsurl__get_longest_section(&m);

	/* Allocate enough memory to url escape the longest section */
	buff = malloc(length * 3 + 1);
	if (buff == NULL)
		return NSERROR_NOMEM;

	/* Set scheme type */
	c.scheme_type = m.scheme_type;

	/* Build NetSurf URL object from sections */
	e |= nsurl__create_from_section(url_s, URL_SCHEME, &m, buff, &c);
	e |= nsurl__create_from_section(url_s, URL_CREDENTIALS, &m, buff, &c);
	e |= nsurl__create_from_section(url_s, URL_HOST, &m, buff, &c);
	e |= nsurl__create_from_section(url_s, URL_PATH, &m, buff, &c);
	e |= nsurl__create_from_section(url_s, URL_QUERY, &m, buff, &c);
	e |= nsurl__create_from_section(url_s, URL_FRAGMENT, &m, buff, &c);

	/* Finished with buffer */
	free(buff);

	if (e != NSERROR_OK) {
		nsurl_destroy_components(&c);
		return NSERROR_NOMEM;
	}

	/* Validate URL */
	if ((lwc_string_isequal(c.scheme, corestring_lwc_http,
			&match) == lwc_error_ok && match == true) ||
			(lwc_string_isequal(c.scheme, corestring_lwc_https,
			&match) == lwc_error_ok && match == true)) {
		/* http, https must have host */
		if (c.host == NULL) {
			nsurl_destroy_components(&c);
			return NSERROR_BAD_URL;
		}
	}

	/* Get the string length and find which parts of url are present */
	nsurl__get_string_data(&c, NSURL_WITH_FRAGMENT, &length,
			&str_len, &str_flags);

	/* Create NetSurf URL object */
	*url = malloc(sizeof(nsurl) + length + 1); /* Add 1 for \0 */
	if (*url == NULL) {
		nsurl_destroy_components(&c);
		return NSERROR_NOMEM;
	}

	(*url)->components = c;
	(*url)->length = length;

	/* Fill out the url string */
	nsurl_get_string(&c, (*url)->string, &str_len, str_flags);

	/* Get the nsurl's hash */
	nsurl_calc_hash(*url);

	/* Give the URL a reference */
	(*url)->count = 1;

	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
nsurl *nsurl_ref(nsurl *url)
{
	assert(url != NULL);

	url->count++;

	return url;
}


/* exported interface, documented in nsurl.h */
void nsurl_unref(nsurl *url)
{
	assert(url != NULL);
	assert(url->count > 0);

	if (--url->count > 0)
		return;

#ifdef NSURL_DEBUG
	nsurl__dump(url);
#endif

	/* Release lwc strings */
	nsurl_destroy_components(&url->components);

	/* Free the NetSurf URL */
	free(url);
}


/* exported interface, documented in nsurl.h */
bool nsurl_compare(const nsurl *url1, const nsurl *url2, nsurl_component parts)
{
	bool match = true;

	assert(url1 != NULL);
	assert(url2 != NULL);

	/* Compare URL components */

	/* Path, host and query first, since they're most likely to differ */

	if (parts & NSURL_PATH) {
		nsurl__component_compare(url1->components.path,
				url2->components.path, &match);

		if (match == false)
			return false;
	}

	if (parts & NSURL_HOST) {
		nsurl__component_compare(url1->components.host,
				url2->components.host, &match);

		if (match == false)
			return false;
	}

	if (parts & NSURL_QUERY) {
		nsurl__component_compare(url1->components.query,
				url2->components.query, &match);

		if (match == false)
			return false;
	}

	if (parts & NSURL_SCHEME) {
		nsurl__component_compare(url1->components.scheme,
				url2->components.scheme, &match);

		if (match == false)
			return false;
	}

	if (parts & NSURL_USERNAME) {
		nsurl__component_compare(url1->components.username,
				url2->components.username, &match);

		if (match == false)
			return false;
	}

	if (parts & NSURL_PASSWORD) {
		nsurl__component_compare(url1->components.password,
				url2->components.password, &match);

		if (match == false)
			return false;
	}

	if (parts & NSURL_PORT) {
		nsurl__component_compare(url1->components.port,
				url2->components.port, &match);

		if (match == false)
			return false;
	}

	if (parts & NSURL_FRAGMENT) {
		nsurl__component_compare(url1->components.fragment,
				url2->components.fragment, &match);

		if (match == false)
			return false;
	}

	return true;
}


/* exported interface, documented in nsurl.h */
nserror nsurl_get(const nsurl *url, nsurl_component parts,
		char **url_s, size_t *url_l)
{
	struct nsurl_component_lengths str_len = { 0, 0, 0, 0,  0, 0, 0, 0 };
	enum nsurl_string_flags str_flags = 0;

	/* Get the string length and find which parts of url need copied */
	nsurl__get_string_data(&(url->components), parts, url_l,
			&str_len, &str_flags);

	if (*url_l == 0) {
		return NSERROR_BAD_URL;
	}

	/* Allocate memory for url string */
	*url_s = malloc(*url_l + 1); /* adding 1 for '\0' */
	if (*url_s == NULL) {
		return NSERROR_NOMEM;
	}

	/* Copy the required parts into the url string */
	nsurl_get_string(&(url->components), *url_s, &str_len, str_flags);

	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
lwc_string *nsurl_get_component(const nsurl *url, nsurl_component part)
{
	assert(url != NULL);

	switch (part) {
	case NSURL_SCHEME:
		return (url->components.scheme != NULL) ?
				lwc_string_ref(url->components.scheme) : NULL;

	case NSURL_USERNAME:
		return (url->components.username != NULL) ?
				lwc_string_ref(url->components.username) : NULL;

	case NSURL_PASSWORD:
		return (url->components.password != NULL) ?
				lwc_string_ref(url->components.password) : NULL;

	case NSURL_HOST:
		return (url->components.host != NULL) ?
				lwc_string_ref(url->components.host) : NULL;

	case NSURL_PORT:
		return (url->components.port != NULL) ?
				lwc_string_ref(url->components.port) : NULL;

	case NSURL_PATH:
		return (url->components.path != NULL) ?
				lwc_string_ref(url->components.path) : NULL;

	case NSURL_QUERY:
		return (url->components.query != NULL) ?
				lwc_string_ref(url->components.query) : NULL;

	case NSURL_FRAGMENT:
		return (url->components.fragment != NULL) ?
				lwc_string_ref(url->components.fragment) : NULL;

	default:
		LOG(("Unsupported value passed to part param."));
		assert(0);
	}

	return NULL;
}


/* exported interface, documented in nsurl.h */
bool nsurl_has_component(const nsurl *url, nsurl_component part)
{
	assert(url != NULL);

	switch (part) {
	case NSURL_SCHEME:
		if (url->components.scheme != NULL)
			return true;
		else
			return false;

	case NSURL_CREDENTIALS:
		/* Only username required for credentials section */
		/* Fall through */
	case NSURL_USERNAME:
		if (url->components.username != NULL)
			return true;
		else
			return false;

	case NSURL_PASSWORD:
		if (url->components.password != NULL)
			return true;
		else
			return false;

	case NSURL_HOST:
		if (url->components.host != NULL)
			return true;
		else
			return false;

	case NSURL_PORT:
		if (url->components.port != NULL)
			return true;
		else
			return false;

	case NSURL_PATH:
		if (url->components.path != NULL)
			return true;
		else
			return false;

	case NSURL_QUERY:
		if (url->components.query != NULL)
			return true;
		else
			return false;

	case NSURL_FRAGMENT:
		if (url->components.fragment != NULL)
			return true;
		else
			return false;

	default:
		LOG(("Unsupported value passed to part param."));
		assert(0);
	}

	return false;
}


/* exported interface, documented in nsurl.h */
const char *nsurl_access(const nsurl *url)
{
	assert(url != NULL);

	return url->string;
}


/* exported interface, documented in nsurl.h */
const char *nsurl_access_leaf(const nsurl *url)
{
	size_t path_len;
	const char *path;
	const char *leaf;

	if (url->components.path == NULL)
		return "";

	path = lwc_string_data(url->components.path);
	path_len = lwc_string_length(url->components.path);

	if (path_len == 0)
		return "";

	if (path_len == 1 && *path == '/')
		return "/";

	leaf = path + path_len;

	do {
		leaf--;
	} while ((leaf != path) && (*leaf != '/'));

	if (*leaf == '/')
		leaf++;

	return leaf;
}


/* exported interface, documented in nsurl.h */
size_t nsurl_length(const nsurl *url)
{
	assert(url != NULL);

	return url->length;
}


/* exported interface, documented in nsurl.h */
uint32_t nsurl_hash(const nsurl *url)
{
	assert(url != NULL);

	return url->hash;
}


/* exported interface, documented in nsurl.h */
nserror nsurl_join(const nsurl *base, const char *rel, nsurl **joined)
{
	struct url_markers m;
	struct nsurl_components c;
	size_t length;
	char *buff;
	char *buff_pos;
	char *buff_start;
	struct nsurl_component_lengths str_len = { 0, 0, 0, 0,  0, 0, 0, 0 };
	enum nsurl_string_flags str_flags = 0;
	nserror error = 0;
	enum {
		NSURL_F_REL		=  0,
		NSURL_F_BASE_SCHEME	= (1 << 0),
		NSURL_F_BASE_AUTHORITY	= (1 << 1),
		NSURL_F_BASE_PATH	= (1 << 2),
		NSURL_F_MERGED_PATH	= (1 << 3),
		NSURL_F_BASE_QUERY	= (1 << 4)
	} joined_parts;

	assert(base != NULL);
	assert(rel != NULL);

	/* Peg out the URL sections */
	nsurl__get_string_markers(rel, &m, true);

	/* Get the length of the longest section */
	length = nsurl__get_longest_section(&m);

	/* Initially assume that the joined URL can be formed entierly from
	 * the relative URL. */
	joined_parts = NSURL_F_REL;

	/* Update joined_compnents to indicate any required parts from the
	 * base URL. */
	if (m.scheme_end - m.start <= 0) {
		/* The relative url has no scheme.
		 * Use base URL's scheme. */
		joined_parts |= NSURL_F_BASE_SCHEME;

		if (m.path - m.authority <= 0) {
			/* The relative URL has no authority.
			 * Use base URL's authority. */
			joined_parts |= NSURL_F_BASE_AUTHORITY;

			if (m.query - m.path <= 0) {
				/* The relative URL has no path.
				 * Use base URL's path. */
				joined_parts |= NSURL_F_BASE_PATH;

				if (m.fragment - m.query <= 0) {
					/* The relative URL has no query.
					 * Use base URL's query. */
					joined_parts |= NSURL_F_BASE_QUERY;
				}

			} else if (*(rel + m.path) != '/') {
				/* Relative URL has relative path */
				joined_parts |= NSURL_F_MERGED_PATH;
			}
		}
	}

	/* Allocate enough memory to url escape the longest section, plus
	 * space for path merging (if required). */
	if (joined_parts & NSURL_F_MERGED_PATH) {
		/* Need to merge paths */
		length += (base->components.path != NULL) ?
				lwc_string_length(base->components.path) : 0;
	}
	length *= 4;
	/* Plus space for removing dots from path */
	length += (m.query - m.path) + ((base->components.path != NULL) ?
			lwc_string_length(base->components.path) : 0);

	buff = malloc(length + 5);
	if (buff == NULL)
		return NSERROR_NOMEM;

	buff_pos = buff;

	/* Form joined URL from base or rel components, as appropriate */

	if (joined_parts & NSURL_F_BASE_SCHEME) {
		c.scheme_type = base->components.scheme_type;

		c.scheme = nsurl__component_copy(base->components.scheme);
	} else {
		c.scheme_type = m.scheme_type;

		error |= nsurl__create_from_section(rel, URL_SCHEME, &m,
				buff, &c);
	}

	if (joined_parts & NSURL_F_BASE_AUTHORITY) {
		c.username = nsurl__component_copy(base->components.username);
		c.password = nsurl__component_copy(base->components.password);
		c.host = nsurl__component_copy(base->components.host);
		c.port = nsurl__component_copy(base->components.port);
	} else {
		error |= nsurl__create_from_section(rel, URL_CREDENTIALS, &m,
				buff, &c);
		error |= nsurl__create_from_section(rel, URL_HOST, &m,
				buff, &c);
	}

	if (joined_parts & NSURL_F_BASE_PATH) {
		c.path = nsurl__component_copy(base->components.path);

	} else if (joined_parts & NSURL_F_MERGED_PATH) {
		struct url_markers m_path;
		size_t new_length;

		if (base->components.host != NULL &&
					base->components.path == NULL) {
			/* Append relative path to "/". */
			*(buff_pos++) = '/';
			memcpy(buff_pos, rel + m.path, m.query - m.path);
			buff_pos += m.query - m.path;

		} else {
			/* Append relative path to all but last segment of
			 * base path. */
			size_t path_end = lwc_string_length(
					base->components.path);
			const char *path = lwc_string_data(
					base->components.path);

			while (*(path + path_end) != '/' &&
					path_end != 0) {
				path_end--;
			}
			if (*(path + path_end) == '/')
				path_end++;

			/* Copy the base part */
			memcpy(buff_pos, path, path_end);
			buff_pos += path_end;

			/* Copy the relative part */
			memcpy(buff_pos, rel + m.path, m.query - m.path);
			buff_pos += m.query - m.path;
		}

		/* add termination to string */
		*buff_pos++ = '\0';

		new_length = nsurl__remove_dot_segments(buff, buff_pos);

		m_path.path = 0;
		m_path.query = new_length;

		buff_start = buff_pos + new_length;
		error |= nsurl__create_from_section(buff_pos, URL_PATH, &m_path,
				buff_start, &c);

	} else {
		struct url_markers m_path;
		size_t new_length;

		memcpy(buff_pos, rel + m.path, m.query - m.path);
		buff_pos += m.query - m.path;
		*(buff_pos++) = '\0';

		new_length = nsurl__remove_dot_segments(buff, buff_pos);

		m_path.path = 0;
		m_path.query = new_length;

		buff_start = buff_pos + new_length;
		error |= nsurl__create_from_section(buff_pos, URL_PATH, &m_path,
				buff_start, &c);
	}

	if (joined_parts & NSURL_F_BASE_QUERY)
		c.query = nsurl__component_copy(base->components.query);
	else
		error |= nsurl__create_from_section(rel, URL_QUERY, &m,
				buff, &c);

	error |= nsurl__create_from_section(rel, URL_FRAGMENT, &m,
			buff, &c);

	/* Free temporary buffer */
	free(buff);

	if (error != NSERROR_OK)
		return NSERROR_NOMEM;

	/* Get the string length and find which parts of url are present */
	nsurl__get_string_data(&c, NSURL_WITH_FRAGMENT, &length,
			&str_len, &str_flags);

	/* Create NetSurf URL object */
	*joined = malloc(sizeof(nsurl) + length + 1); /* Add 1 for \0 */
	if (*joined == NULL)
		return NSERROR_NOMEM;

	(*joined)->components = c;
	(*joined)->length = length;

	/* Fill out the url string */
	nsurl_get_string(&c, (*joined)->string, &str_len, str_flags);

	/* Get the nsurl's hash */
	nsurl_calc_hash(*joined);

	/* Give the URL a reference */
	(*joined)->count = 1;

	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
nserror nsurl_defragment(const nsurl *url, nsurl **no_frag)
{
	size_t length;
	char *pos;

	/* check for source url having no fragment already */
	if (url->components.fragment == NULL) {
		*no_frag = (nsurl *)url;

		(*no_frag)->count++;

		return NSERROR_OK;
	}

	/* Find the change in length from url to new_url */
	length = url->length;
	if (url->components.fragment != NULL) {
		length -= 1 + lwc_string_length(url->components.fragment);
	}

	/* Create NetSurf URL object */
	*no_frag = malloc(sizeof(nsurl) + length + 1); /* Add 1 for \0 */
	if (*no_frag == NULL) {
		return NSERROR_NOMEM;
	}

	/* Copy components */
	(*no_frag)->components.scheme =
			nsurl__component_copy(url->components.scheme);
	(*no_frag)->components.username =
			nsurl__component_copy(url->components.username);
	(*no_frag)->components.password =
			nsurl__component_copy(url->components.password);
	(*no_frag)->components.host =
			nsurl__component_copy(url->components.host);
	(*no_frag)->components.port =
			nsurl__component_copy(url->components.port);
	(*no_frag)->components.path =
			nsurl__component_copy(url->components.path);
	(*no_frag)->components.query =
			nsurl__component_copy(url->components.query);
	(*no_frag)->components.fragment = NULL;

	(*no_frag)->components.scheme_type = url->components.scheme_type;

	(*no_frag)->length = length;

	/* Fill out the url string */
	pos = (*no_frag)->string;
	memcpy(pos, url->string, length);
	pos += length;
	*pos = '\0';

	/* Get the nsurl's hash */
	nsurl_calc_hash(*no_frag);

	/* Give the URL a reference */
	(*no_frag)->count = 1;

	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
nserror nsurl_refragment(const nsurl *url, lwc_string *frag, nsurl **new_url)
{
	int frag_len;
	int base_len;
	char *pos;
	size_t len;

	assert(url != NULL);
	assert(frag != NULL);

	/* Find the change in length from url to new_url */
	base_len = url->length;
	if (url->components.fragment != NULL) {
		base_len -= 1 + lwc_string_length(url->components.fragment);
	}
	frag_len = lwc_string_length(frag);

	/* Set new_url's length */
	len = base_len + 1 /* # */ + frag_len;

	/* Create NetSurf URL object */
	*new_url = malloc(sizeof(nsurl) + len + 1); /* Add 1 for \0 */
	if (*new_url == NULL) {
		return NSERROR_NOMEM;
	}

	(*new_url)->length = len;

	/* Set string */
	pos = (*new_url)->string;
	memcpy(pos, url->string, base_len);
	pos += base_len;
	*pos = '#';
	memcpy(++pos, lwc_string_data(frag), frag_len);
	pos += frag_len;
	*pos = '\0';

	/* Copy components */
	(*new_url)->components.scheme =
			nsurl__component_copy(url->components.scheme);
	(*new_url)->components.username =
			nsurl__component_copy(url->components.username);
	(*new_url)->components.password =
			nsurl__component_copy(url->components.password);
	(*new_url)->components.host =
			nsurl__component_copy(url->components.host);
	(*new_url)->components.port =
			nsurl__component_copy(url->components.port);
	(*new_url)->components.path =
			nsurl__component_copy(url->components.path);
	(*new_url)->components.query =
			nsurl__component_copy(url->components.query);
	(*new_url)->components.fragment =
			lwc_string_ref(frag);

	(*new_url)->components.scheme_type = url->components.scheme_type;

	/* Get the nsurl's hash */
	nsurl_calc_hash(*new_url);

	/* Give the URL a reference */
	(*new_url)->count = 1;

	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
nserror nsurl_replace_query(const nsurl *url, const char *query,
		nsurl **new_url)
{
	int query_len;		/* Length of new query string, including '?' */
	int frag_len = 0;	/* Length of fragment, including '#' */
	int base_len;		/* Length of URL up to start of query */
	char *pos;
	size_t len;
	lwc_string *lwc_query;

	assert(url != NULL);
	assert(query != NULL);
	assert(query[0] == '?');

	/* Get the length of the new query */
	query_len = strlen(query);

	/* Find the change in length from url to new_url */
	base_len = url->length;
	if (url->components.query != NULL) {
		base_len -= lwc_string_length(url->components.query);
	}
	if (url->components.fragment != NULL) {
		frag_len = 1 + lwc_string_length(url->components.fragment);
		base_len -= frag_len;
	}

	/* Set new_url's length */
	len = base_len + query_len + frag_len;

	/* Create NetSurf URL object */
	*new_url = malloc(sizeof(nsurl) + len + 1); /* Add 1 for \0 */
	if (*new_url == NULL) {
		return NSERROR_NOMEM;
	}

	if (lwc_intern_string(query, query_len, &lwc_query) != lwc_error_ok) {
		free(*new_url);
		return NSERROR_NOMEM;
	}

	(*new_url)->length = len;

	/* Set string */
	pos = (*new_url)->string;
	memcpy(pos, url->string, base_len);
	pos += base_len;
	memcpy(pos, query, query_len);
	pos += query_len;
	if (url->components.fragment != NULL) {
		const char *frag = lwc_string_data(url->components.fragment);
		*pos = '#';
		memcpy(++pos, frag, frag_len - 1);
		pos += frag_len - 1;
	}
	*pos = '\0';

	/* Copy components */
	(*new_url)->components.scheme =
			nsurl__component_copy(url->components.scheme);
	(*new_url)->components.username =
			nsurl__component_copy(url->components.username);
	(*new_url)->components.password =
			nsurl__component_copy(url->components.password);
	(*new_url)->components.host =
			nsurl__component_copy(url->components.host);
	(*new_url)->components.port =
			nsurl__component_copy(url->components.port);
	(*new_url)->components.path =
			nsurl__component_copy(url->components.path);
	(*new_url)->components.query = lwc_query;
	(*new_url)->components.fragment =
			nsurl__component_copy(url->components.fragment);

	(*new_url)->components.scheme_type = url->components.scheme_type;

	/* Get the nsurl's hash */
	nsurl_calc_hash(*new_url);

	/* Give the URL a reference */
	(*new_url)->count = 1;

	return NSERROR_OK;
}


/* exported interface, documented in nsurl.h */
nserror nsurl_parent(const nsurl *url, nsurl **new_url)
{
	lwc_string *lwc_path;
	size_t old_path_len, new_path_len;
	size_t len;
	const char* path = NULL;
	char *pos;

	assert(url != NULL);

	old_path_len = (url->components.path == NULL) ? 0 :
			lwc_string_length(url->components.path);

	/* Find new path length */
	if (old_path_len == 0) {
		new_path_len = old_path_len;
	} else {
		path = lwc_string_data(url->components.path);

		new_path_len = old_path_len;
		if (old_path_len > 1) {
			/* Skip over any trailing / */
			if (path[new_path_len - 1] == '/')
				new_path_len--;

			/* Work back to next / */
			while (new_path_len > 0 &&
					path[new_path_len - 1] != '/')
				new_path_len--;
		}
	}

	/* Find the length of new_url */
	len = url->length;
	if (url->components.query != NULL) {
		len -= lwc_string_length(url->components.query);
	}
	if (url->components.fragment != NULL) {
		len -= 1; /* # */
		len -= lwc_string_length(url->components.fragment);
	}
	len -= old_path_len - new_path_len;

	/* Create NetSurf URL object */
	*new_url = malloc(sizeof(nsurl) + len + 1); /* Add 1 for \0 */
	if (*new_url == NULL) {
		return NSERROR_NOMEM;
	}

	/* Make new path */
	if (old_path_len == 0) {
		lwc_path = NULL;
	} else if (old_path_len == new_path_len) {
		lwc_path = lwc_string_ref(url->components.path);
	} else {
		if (lwc_intern_string(path, old_path_len - new_path_len,
				&lwc_path) != lwc_error_ok) {
			free(*new_url);
			return NSERROR_NOMEM;
		}
	}

	(*new_url)->length = len;

	/* Set string */
	pos = (*new_url)->string;
	memcpy(pos, url->string, len);
	pos += len;
	*pos = '\0';

	/* Copy components */
	(*new_url)->components.scheme =
			nsurl__component_copy(url->components.scheme);
	(*new_url)->components.username =
			nsurl__component_copy(url->components.username);
	(*new_url)->components.password =
			nsurl__component_copy(url->components.password);
	(*new_url)->components.host =
			nsurl__component_copy(url->components.host);
	(*new_url)->components.port =
			nsurl__component_copy(url->components.port);
	(*new_url)->components.path = lwc_path;
	(*new_url)->components.query = NULL;
	(*new_url)->components.fragment = NULL;

	(*new_url)->components.scheme_type = url->components.scheme_type;

	/* Get the nsurl's hash */
	nsurl_calc_hash(*new_url);

	/* Give the URL a reference */
	(*new_url)->count = 1;

	return NSERROR_OK;
}

