/*
 * $Id: funcs.h,v 1.1 1998/08/14 00:31:24 vixie Exp $
 */

/*
 * Copyright (c) 1997 by Internet Software Consortium
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

/* Notes:
 *	This file has to be included by cron.h after data structure defs.
 *	We should reorg this into sections by module.
 */

void		set_cron_uid(void),
		set_cron_cwd(void),
		load_database(cron_db *),
		open_logfile(void),
		sigpipe_func(void),
		job_add(entry *, user *),
		do_command(entry *, user *),
		link_user(cron_db *, user *),
		unlink_user(cron_db *, user *),
		free_user(user *),
		env_free(char **),
		unget_char(int, FILE *),
		free_entry(entry *),
		skip_comments(FILE *),
		log_it(const char *, int, const char *, const char *),
		log_close(void);

int		job_runqueue(void),
		set_debug_flags(char *),
		get_char(FILE *),
		get_string(char *, int, FILE *, char *),
		swap_uids(void),
		swap_uids_back(void),
		load_env(char *, FILE *),
		cron_pclose(FILE *),
		strcmp_until(const char *, const char *, int),
		allowed(char *),
		strdtb(char *);

char		*env_get(char *, char **),
		*arpadate(time_t *),
		*mkprints(unsigned char *, unsigned int),
		*first_word(char *, char *),
		**env_init(void),
		**env_copy(char **),
		**env_set(char **, char *);

user		*load_user(int, struct passwd *, const char *),
		*find_user(cron_db *, const char *);

entry		*load_entry(FILE *, void (*)(const char *),
				 struct passwd *, char **);

FILE		*cron_popen(char *, char *, entry *, PID_T *);
