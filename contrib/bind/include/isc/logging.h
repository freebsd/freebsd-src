/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#define log_critical		(-5)
#define log_error   		(-4)
#define log_warning 		(-3)
#define log_notice  		(-2)
#define log_info  		(-1)
#define log_debug(level)	(level)

typedef enum { log_syslog, log_file, log_null } log_channel_type;

#define LOG_MAX_VERSIONS 99

#define LOG_CLOSE_STREAM		0x0001
#define LOG_TIMESTAMP			0x0002
#define LOG_TRUNCATE			0x0004
#define LOG_USE_CONTEXT_LEVEL		0x0008
#define LOG_PRINT_LEVEL			0x0010
#define LOG_REQUIRE_DEBUG		0x0020
#define LOG_CHANNEL_BROKEN		0x0040
#define LOG_PRINT_CATEGORY		0x0080
#define LOG_CHANNEL_OFF			0x0100

typedef struct log_context *log_context;
typedef struct log_channel *log_channel;

#define LOG_OPTION_DEBUG		0x01
#define LOG_OPTION_LEVEL		0x02

#define log_open_stream		__log_open_stream
#define log_close_stream	__log_close_stream
#define log_get_stream		__log_get_stream
#define log_get_filename	__log_get_filename
#define log_check_channel	__log_check_channel
#define log_check		__log_check
#define log_vwrite		__log_vwrite
#define log_write		__log_write
#define log_new_context		__log_new_context
#define log_free_context	__log_free_context
#define log_add_channel		__log_add_channel
#define log_remove_channel	__log_remove_channel
#define log_option		__log_option
#define log_category_is_active	__log_category_is_active
#define log_new_syslog_channel	__log_new_syslog_channel
#define log_new_file_channel	__log_new_file_channel
#define log_set_file_owner	__log_set_file_owner
#define log_new_null_channel	__log_new_null_channel
#define log_inc_references	__log_inc_references
#define log_dec_references	__log_dec_references
#define log_get_channel_type	__log_get_channel_type
#define log_free_channel	__log_free_channel
#define log_close_debug_channels	__log_close_debug_channels

FILE *			log_open_stream(log_channel);
int			log_close_stream(log_channel);
FILE *			log_get_stream(log_channel);
char *			log_get_filename(log_channel);
int			log_check_channel(log_context, int, log_channel);
int			log_check(log_context, int, int);
void			log_vwrite(log_context, int, int, const char *, 
				   va_list args);
#ifdef __GNUC__
void			log_write(log_context, int, int, const char *, ...)
				__attribute__((__format__(__printf__, 4, 5)));
#else
void			log_write(log_context, int, int, const char *, ...);
#endif
int			log_new_context(int, char **, log_context *);
void			log_free_context(log_context);
int			log_add_channel(log_context, int, log_channel);
int			log_remove_channel(log_context, int, log_channel);
int			log_option(log_context, int, int);
int			log_category_is_active(log_context, int);
log_channel		log_new_syslog_channel(unsigned int, int, int);
log_channel		log_new_file_channel(unsigned int, int, const char *,
					     FILE *, unsigned int,
					     unsigned long);
int			log_set_file_owner(log_channel, uid_t, gid_t);
log_channel		log_new_null_channel(void);
int			log_inc_references(log_channel);
int			log_dec_references(log_channel);
log_channel_type	log_get_channel_type(log_channel);
int			log_free_channel(log_channel);
void			log_close_debug_channels(log_context);

#endif /* !LOGGING_H */
