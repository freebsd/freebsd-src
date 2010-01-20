/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: logging.c,v 1.6.18.2 2008/02/28 05:49:37 marka Exp $";
#endif /* not lint */

#include "port_before.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <isc/assertions.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/misc.h>

#include "port_after.h"

#include "logging_p.h"

static const int syslog_priority[] = { LOG_DEBUG, LOG_INFO, LOG_NOTICE,
				       LOG_WARNING, LOG_ERR, LOG_CRIT };

static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static const char *level_text[] = {
	"info: ", "notice: ", "warning: ", "error: ", "critical: "
};

static void
version_rename(log_channel chan) {
	unsigned int ver;
	char old_name[PATH_MAX+1];
	char new_name[PATH_MAX+1];
	
	ver = chan->out.file.versions;
	if (ver < 1)
		return;
	if (ver > LOG_MAX_VERSIONS)
		ver = LOG_MAX_VERSIONS;
	/*
	 * Need to have room for '.nn' (XXX assumes LOG_MAX_VERSIONS < 100)
	 */
	if (strlen(chan->out.file.name) > (size_t)(PATH_MAX-3))
		return;
	for (ver--; ver > 0; ver--) {
		sprintf(old_name, "%s.%d", chan->out.file.name, ver-1);
		sprintf(new_name, "%s.%d", chan->out.file.name, ver);
		(void)isc_movefile(old_name, new_name);
	}
	sprintf(new_name, "%s.0", chan->out.file.name);
	(void)isc_movefile(chan->out.file.name, new_name);
}

FILE *
log_open_stream(log_channel chan) {
	FILE *stream;
	int fd, flags;
	struct stat sb;
	int regular;

	if (chan == NULL || chan->type != log_file) {
		errno = EINVAL;
		return (NULL);
	}
	
	/*
	 * Don't open already open streams
	 */
	if (chan->out.file.stream != NULL)
		return (chan->out.file.stream);

	if (stat(chan->out.file.name, &sb) < 0) {
		if (errno != ENOENT) {
			syslog(LOG_ERR,
			       "log_open_stream: stat of %s failed: %s",
			       chan->out.file.name, strerror(errno));
			chan->flags |= LOG_CHANNEL_BROKEN;
			return (NULL);
		}
		regular = 1;
	} else
		regular = (sb.st_mode & S_IFREG);

	if (chan->out.file.versions) {
		if (!regular) {
			syslog(LOG_ERR,
       "log_open_stream: want versions but %s isn't a regular file",
			       chan->out.file.name);
			chan->flags |= LOG_CHANNEL_BROKEN;
			errno = EINVAL;
			return (NULL);
		}
	}

	flags = O_WRONLY|O_CREAT|O_APPEND;

	if ((chan->flags & LOG_TRUNCATE) != 0) {
		if (regular) {
			(void)unlink(chan->out.file.name);
			flags |= O_EXCL;
		} else {
			syslog(LOG_ERR,
       "log_open_stream: want truncation but %s isn't a regular file",
			       chan->out.file.name);
			chan->flags |= LOG_CHANNEL_BROKEN;
			errno = EINVAL;
			return (NULL);
		}
	}

	fd = open(chan->out.file.name, flags,
		  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if (fd < 0) {
		syslog(LOG_ERR, "log_open_stream: open(%s) failed: %s",
		       chan->out.file.name, strerror(errno));
		chan->flags |= LOG_CHANNEL_BROKEN;
		return (NULL);
	}
	stream = fdopen(fd, "a");
	if (stream == NULL) {
		syslog(LOG_ERR, "log_open_stream: fdopen() failed");
		chan->flags |= LOG_CHANNEL_BROKEN;
		return (NULL);
	}
	(void) fchown(fd, chan->out.file.owner, chan->out.file.group);

	chan->out.file.stream = stream;
	return (stream);
}

int
log_close_stream(log_channel chan) {
	FILE *stream;

	if (chan == NULL || chan->type != log_file) {
		errno = EINVAL;
		return (0);
	}
	stream = chan->out.file.stream;
	chan->out.file.stream = NULL;
	if (stream != NULL && fclose(stream) == EOF)
		return (-1);
	return (0);
}

void
log_close_debug_channels(log_context lc) {
	log_channel_list lcl;
	int i;

	for (i = 0; i < lc->num_categories; i++)
		for (lcl = lc->categories[i]; lcl != NULL; lcl = lcl->next)
			if (lcl->channel->type == log_file &&
			    lcl->channel->out.file.stream != NULL &&
			    lcl->channel->flags & LOG_REQUIRE_DEBUG)
				(void)log_close_stream(lcl->channel);
}

FILE *
log_get_stream(log_channel chan) {
	if (chan == NULL || chan->type != log_file) {
		errno = EINVAL;
		return (NULL);
	}
	return (chan->out.file.stream);
}

char *
log_get_filename(log_channel chan) {
	if (chan == NULL || chan->type != log_file) {
		errno = EINVAL;
		return (NULL);
	}
	return (chan->out.file.name);
}

int
log_check_channel(log_context lc, int level, log_channel chan) {
	int debugging, chan_level;

	REQUIRE(lc != NULL);

	debugging = ((lc->flags & LOG_OPTION_DEBUG) != 0);

	/*
	 * If not debugging, short circuit debugging messages very early.
	 */
	if (level > 0 && !debugging)
		return (0);

	if ((chan->flags & (LOG_CHANNEL_BROKEN|LOG_CHANNEL_OFF)) != 0)
		return (0);

	/* Some channels only log when debugging is on. */
	if ((chan->flags & LOG_REQUIRE_DEBUG) && !debugging)
		return (0);

	/* Some channels use the global level. */
	if ((chan->flags & LOG_USE_CONTEXT_LEVEL) != 0) {
		chan_level = lc->level;
	} else
		chan_level = chan->level;

	if (level > chan_level)
		return (0);

	return (1);
}

int 
log_check(log_context lc, int category, int level) {
	log_channel_list lcl;
	int debugging;

	REQUIRE(lc != NULL);

	debugging = ((lc->flags & LOG_OPTION_DEBUG) != 0);

	/*
	 * If not debugging, short circuit debugging messages very early.
	 */
	if (level > 0 && !debugging)
		return (0);

	if (category < 0 || category > lc->num_categories)
		category = 0;		/*%< use default */
	lcl = lc->categories[category];
	if (lcl == NULL) {
		category = 0;
		lcl = lc->categories[0];
	}

	for ( /* nothing */; lcl != NULL; lcl = lcl->next) {
		if (log_check_channel(lc, level, lcl->channel))
			return (1);
	}
	return (0);
}

void
log_vwrite(log_context lc, int category, int level, const char *format, 
	   va_list args) {
	log_channel_list lcl;
	int pri, debugging, did_vsprintf = 0;
	int original_category;
	FILE *stream;
	log_channel chan;
	struct timeval tv;
	struct tm *local_tm;
#ifdef HAVE_TIME_R
	struct tm tm_tmp;
#endif
	time_t tt;
	const char *category_name;
	const char *level_str;
	char time_buf[256];
	char level_buf[256];

	REQUIRE(lc != NULL);

	debugging = (lc->flags & LOG_OPTION_DEBUG);

	/*
	 * If not debugging, short circuit debugging messages very early.
	 */
	if (level > 0 && !debugging)
		return;

	if (category < 0 || category > lc->num_categories)
		category = 0;		/*%< use default */
	original_category = category;
	lcl = lc->categories[category];
	if (lcl == NULL) {
		category = 0;
		lcl = lc->categories[0];
	}

	/*
	 * Get the current time and format it.
	 */
	time_buf[0]='\0';
	if (gettimeofday(&tv, NULL) < 0) {
		syslog(LOG_INFO, "gettimeofday failed in log_vwrite()");
	} else {
		tt = tv.tv_sec;
#ifdef HAVE_TIME_R
		local_tm = localtime_r(&tt, &tm_tmp);
#else
		local_tm = localtime(&tt);
#endif
		if (local_tm != NULL) {
			sprintf(time_buf, "%02d-%s-%4d %02d:%02d:%02d.%03ld ",
				local_tm->tm_mday, months[local_tm->tm_mon],
				local_tm->tm_year+1900, local_tm->tm_hour,
				local_tm->tm_min, local_tm->tm_sec,
				(long)tv.tv_usec/1000);
		}
	}

	/*
	 * Make a string representation of the current category and level
	 */

	if (lc->category_names != NULL &&
	    lc->category_names[original_category] != NULL)
		category_name = lc->category_names[original_category];
	else
		category_name = "";

	if (level >= log_critical) {
		if (level >= 0) {
			sprintf(level_buf, "debug %d: ", level);
			level_str = level_buf;
		} else
			level_str = level_text[-level-1];
	} else {
		sprintf(level_buf, "level %d: ", level);
		level_str = level_buf;
	}

	/*
	 * Write the message to channels.
	 */
	for ( /* nothing */; lcl != NULL; lcl = lcl->next) {
		chan = lcl->channel;

		if (!log_check_channel(lc, level, chan))
			continue;

		if (!did_vsprintf) {
			(void)vsprintf(lc->buffer, format, args);
			if (strlen(lc->buffer) > (size_t)LOG_BUFFER_SIZE) {
				syslog(LOG_CRIT,
				       "memory overrun in log_vwrite()");
				exit(1);
			}
			did_vsprintf = 1;
		}

		switch (chan->type) {
		case log_syslog:
			if (level >= log_critical)
				pri = (level >= 0) ? 0 : -level;
			else
				pri = -log_critical;
			syslog(chan->out.facility|syslog_priority[pri],
			       "%s%s%s%s",
			       (chan->flags & LOG_TIMESTAMP) ?	time_buf : "",
			       (chan->flags & LOG_PRINT_CATEGORY) ?
			       category_name : "",
			       (chan->flags & LOG_PRINT_LEVEL) ?
			       level_str : "",
			       lc->buffer);
			break;
		case log_file:
			stream = chan->out.file.stream;
			if (stream == NULL) {
				stream = log_open_stream(chan);
				if (stream == NULL)
					break;
			}
			if (chan->out.file.max_size != ULONG_MAX) {
				long pos;
				
				pos = ftell(stream);
				if (pos >= 0 &&
				    (unsigned long)pos >
				    chan->out.file.max_size) {
					/*
					 * try to roll over the log files,
					 * ignoring all all return codes
					 * except the open (we don't want
					 * to write any more anyway)
					 */
					log_close_stream(chan);
					version_rename(chan);
					stream = log_open_stream(chan);
					if (stream == NULL)
						break;
				}
			}
			fprintf(stream, "%s%s%s%s\n", 
				(chan->flags & LOG_TIMESTAMP) ?	time_buf : "",
				(chan->flags & LOG_PRINT_CATEGORY) ?
				category_name : "",
				(chan->flags & LOG_PRINT_LEVEL) ?
				level_str : "",
				lc->buffer);
			fflush(stream);
			break;
		case log_null:
			break;
		default:
			syslog(LOG_ERR,
			       "unknown channel type in log_vwrite()");
		}
	}
}

void
log_write(log_context lc, int category, int level, const char *format, ...) {
	va_list args;

	va_start(args, format);
	log_vwrite(lc, category, level, format, args);
	va_end(args);
}

/*%
 * Functions to create, set, or destroy contexts
 */

int
log_new_context(int num_categories, char **category_names, log_context *lc) {
	log_context nlc;

	nlc = memget(sizeof (struct log_context));
	if (nlc == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	nlc->num_categories = num_categories;
	nlc->category_names = category_names;
	nlc->categories = memget(num_categories * sizeof (log_channel_list));
	if (nlc->categories == NULL) {
		memput(nlc, sizeof (struct log_context));
		errno = ENOMEM;
		return (-1);
	}
	memset(nlc->categories, '\0',
	       num_categories * sizeof (log_channel_list));
	nlc->flags = 0U;
	nlc->level = 0;
	*lc = nlc;
	return (0);
}

void
log_free_context(log_context lc) {
	log_channel_list lcl, lcl_next;
	log_channel chan;
	int i;

	REQUIRE(lc != NULL);

	for (i = 0; i < lc->num_categories; i++)
		for (lcl = lc->categories[i]; lcl != NULL; lcl = lcl_next) {
			lcl_next = lcl->next;
			chan = lcl->channel;
			(void)log_free_channel(chan);
			memput(lcl, sizeof (struct log_channel_list));
		}
	memput(lc->categories,
	       lc->num_categories * sizeof (log_channel_list));
	memput(lc, sizeof (struct log_context));
}

int
log_add_channel(log_context lc, int category, log_channel chan) {
	log_channel_list lcl;

	if (lc == NULL || category < 0 || category >= lc->num_categories) {
		errno = EINVAL;
		return (-1);
	}

	lcl = memget(sizeof (struct log_channel_list));
	if (lcl == NULL) {
		errno = ENOMEM;
		return(-1);
	}
	lcl->channel = chan;
	lcl->next = lc->categories[category];
	lc->categories[category] = lcl;
	chan->references++;
	return (0);
}

int
log_remove_channel(log_context lc, int category, log_channel chan) {
	log_channel_list lcl, prev_lcl, next_lcl;
	int found = 0;

	if (lc == NULL || category < 0 || category >= lc->num_categories) {
		errno = EINVAL;
		return (-1);
	}

	for (prev_lcl = NULL, lcl = lc->categories[category];
	     lcl != NULL;
	     lcl = next_lcl) {
		next_lcl = lcl->next;
		if (lcl->channel == chan) {
			log_free_channel(chan);
			if (prev_lcl != NULL)
				prev_lcl->next = next_lcl;
			else
				lc->categories[category] = next_lcl;
			memput(lcl, sizeof (struct log_channel_list));
			/*
			 * We just set found instead of returning because
			 * the channel might be on the list more than once.
			 */
			found = 1;
		} else
			prev_lcl = lcl;
	}
	if (!found) {
		errno = ENOENT;
		return (-1);
	}
	return (0);
}

int
log_option(log_context lc, int option, int value) {
	if (lc == NULL) {
		errno = EINVAL;
		return (-1);
	}
	switch (option) {
	case LOG_OPTION_DEBUG:
		if (value)
			lc->flags |= option;
		else
			lc->flags &= ~option;
		break;
	case LOG_OPTION_LEVEL:
		lc->level = value;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

int
log_category_is_active(log_context lc, int category) {
	if (lc == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (category >= 0 && category < lc->num_categories &&
	    lc->categories[category] != NULL)
		return (1);
	return (0);
}

log_channel
log_new_syslog_channel(unsigned int flags, int level, int facility) {
	log_channel chan;

	chan = memget(sizeof (struct log_channel));
	if (chan == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	chan->type = log_syslog;
	chan->flags = flags;
	chan->level = level;
	chan->out.facility = facility;
	chan->references = 0;
	return (chan);
}

log_channel
log_new_file_channel(unsigned int flags, int level,
		     const char *name, FILE *stream, unsigned int versions,
		     unsigned long max_size) {
	log_channel chan;

	chan = memget(sizeof (struct log_channel));
	if (chan == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	chan->type = log_file;
	chan->flags = flags;
	chan->level = level;
	if (name != NULL) {
		size_t len;
		
		len = strlen(name);
		/* 
		 * Quantize length to a multiple of 256.  There's space for the
		 * NUL, since if len is a multiple of 256, the size chosen will
		 * be the next multiple.
		 */
		chan->out.file.name_size = ((len / 256) + 1) * 256;
		chan->out.file.name = memget(chan->out.file.name_size);
		if (chan->out.file.name == NULL) {
			memput(chan, sizeof (struct log_channel));
			errno = ENOMEM;
			return (NULL);
		}
		/* This is safe. */
		strcpy(chan->out.file.name, name);
	} else {
		chan->out.file.name_size = 0;
		chan->out.file.name = NULL;
	}
	chan->out.file.stream = stream;
	chan->out.file.versions = versions;
	chan->out.file.max_size = max_size;
	chan->out.file.owner = getuid();
	chan->out.file.group = getgid();
	chan->references = 0;
	return (chan);
}

int
log_set_file_owner(log_channel chan, uid_t owner, gid_t group) {
	if (chan->type != log_file) {
		errno = EBADF;
		return (-1);
	}
	chan->out.file.owner = owner;
	chan->out.file.group = group;
	return (0);
}

log_channel
log_new_null_channel() {
	log_channel chan;

	chan = memget(sizeof (struct log_channel));
	if (chan == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	chan->type = log_null;
	chan->flags = LOG_CHANNEL_OFF;
	chan->level = log_info;
	chan->references = 0;
	return (chan);
}

int
log_inc_references(log_channel chan) {
	if (chan == NULL) {
		errno = EINVAL;
		return (-1);
	}
	chan->references++;
	return (0);
}

int
log_dec_references(log_channel chan) {
	if (chan == NULL || chan->references <= 0) {
		errno = EINVAL;
		return (-1);
	}
	chan->references--;
	return (0);
}

log_channel_type
log_get_channel_type(log_channel chan) {
	REQUIRE(chan != NULL);
	
	return (chan->type);
}

int
log_free_channel(log_channel chan) {
	if (chan == NULL || chan->references <= 0) {
		errno = EINVAL;
		return (-1);
	}
	chan->references--;
	if (chan->references == 0) {
		if (chan->type == log_file) {
			if ((chan->flags & LOG_CLOSE_STREAM) &&
			    chan->out.file.stream != NULL)
				(void)fclose(chan->out.file.stream);
			if (chan->out.file.name != NULL)
				memput(chan->out.file.name,
				       chan->out.file.name_size);
		}
		memput(chan, sizeof (struct log_channel));
	}
	return (0);
}

/*! \file */
