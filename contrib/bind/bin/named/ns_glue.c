#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ns_glue.c,v 8.17 2000/07/17 07:36:52 vixie Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996-2000 by Internet Software Consortium, Inc.
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

#include "port_before.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <resolv.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"

/*
 * IP address from unaligned octets.
 */
struct in_addr
ina_get(const u_char *data) {
	struct in_addr ret;
	u_int32_t i;

	GETLONG(i, data);
	ina_ulong(ret) = htonl(i);
	return (ret);
}

/*
 * IP address to unaligned octets.
 */
u_char *
ina_put(struct in_addr ina, u_char *data) {
	PUTLONG(ntohl(ina_ulong(ina)), data);
	return (data);
}

/*
 * IP address to presentation format.
 */
const char *
sin_ntoa(struct sockaddr_in sin) {
	static char ret[sizeof "[111.222.333.444].55555"];

	sprintf(ret, "[%s].%u", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
	return (ret);
}

/*
 * Logging Support
 */

int
ns_wouldlog(int category, int level) {
	if (log_ctx_valid)
		return (log_check(log_ctx, category, level));
	return (0);
}

void
ns_debug(int category, int level, const char *format, ...) {
	va_list args;

	if (!log_ctx_valid)
		return;
	va_start(args, format);
	log_vwrite(log_ctx, category, log_debug(level), format, args);
	va_end(args);
}

void
ns_info(int category, const char *format, ...) {
	va_list args;

	if (!log_ctx_valid)
		return;
	va_start(args, format);
	log_vwrite(log_ctx, category, log_info, format, args);
	va_end(args);
}

void
ns_notice(int category, const char *format, ...) {
	va_list args;

	if (!log_ctx_valid)
		return;
	va_start(args, format);
	log_vwrite(log_ctx, category, log_notice, format, args);
	va_end(args);
}

void
ns_warning(int category, const char *format, ...) {
	va_list args;

	if (!log_ctx_valid)
		return;
	va_start(args, format);
	log_vwrite(log_ctx, category, log_warning, format, args);
	va_end(args);
}

void
ns_error(int category, const char *format, ...) {
	va_list args;

	if (!log_ctx_valid)
		return;
	va_start(args, format);
	log_vwrite(log_ctx, category, log_error, format, args);
	va_end(args);
}

void
ns_panic(int category, int dump_core, const char *format, ...) {
	va_list args;

	if (!log_ctx_valid)
		return;
	va_start(args, format);
	log_vwrite(log_ctx, category, log_critical, format, args);
	va_end(args);
	va_start(args, format);
	log_vwrite(log_ctx, ns_log_panic, log_critical, format, args);
	va_end(args);
	if (dump_core)
		abort();
	else
		exit(1);
}

void
ns_assertion_failed(char *file, int line, assertion_type type, char *cond,
		    int print_errno)
{
	ns_panic(ns_log_insist, 1, "%s:%d: %s(%s)%s%s failed.",
		file, line, assertion_type_to_text(type), cond,
		(print_errno) ? ": " : "",
		(print_errno) ? strerror(errno) : "");
}

/*
 * XXX This is for compatibility and should eventually be removed.
 */
void
panic(const char *msg, const void *arg) {
	ns_panic(ns_log_default, 1, msg, arg);
}

/*
 * How many labels in this name?
 * Note: the root label is not included in the count.
 */
int
nlabels(const char *dname) {
	int count, i, found, escaped;
	const char *tmpdname, *end_tmpdname;
	int tmpdnamelen, c;
	
	INSIST(dname != NULL);
	
	count = 0;
	tmpdname = dname;
	tmpdnamelen = strlen(tmpdname);
	/* 
	 * Ignore a trailing label separator (i.e. an unescaped dot)
	 * in 'tmpdname'.
	 */
	if (tmpdnamelen && tmpdname[tmpdnamelen-1] == '.') {
		escaped = 0;
		/* note this loop doesn't get executed if tmpdnamelen==1 */
		for (i = tmpdnamelen - 2; i >= 0; i--)
			if (tmpdname[i] == '\\') {
				if (escaped)
					escaped = 0;
				else
					escaped = 1;
			} else
				break;
		if (!escaped)
			tmpdnamelen--;
	}
	
	end_tmpdname = tmpdname + tmpdnamelen;
	
	while(tmpdname != end_tmpdname) {
		count++;
		/*
		 * Strip off the first label if we're not already at
		 * the root label.
		 */
		for (escaped = found = 0;
		     (tmpdname != end_tmpdname) && !found;
		     tmpdname++) {
			c = *tmpdname;
			if (!escaped && (c == '.'))
				found = 1;
			
			if (escaped)
				escaped = 0;
			else if (c == '\\')
				escaped = 1;
		}
	}
	
	ns_debug(ns_log_default, 12, "nlabels of \"%s\" -> %d", dname, count);
	return (count);
}

/*
 * Get current system time and put it in a global.
 */
void
gettime(struct timeval *ttp) {
	if (gettimeofday(ttp, NULL) < 0)
		ns_error(ns_log_default, "gettimeofday: %s", strerror(errno));
	INSIST(ttp->tv_usec >= 0 && ttp->tv_usec < 1000000);
}

/*
 * This is useful for tracking down lost file descriptors.
 */
int
my_close(int fd) {
	int s;

	do {
		errno = 0;
		s = close(fd);
	} while (s < 0 && errno == EINTR);

	if (s < 0 && errno != EBADF)
		ns_info(ns_log_default, "close(%d) failed: %s", fd,
		       strerror(errno));
	else
		ns_debug(ns_log_default, 3, "close(%d) succeeded", fd);
	return (s);
}

/*
 * This is useful for tracking down lost file descriptors.
 */
int
my_fclose(FILE *fp) {
	int fd = fileno(fp),
	    s = fclose(fp);

	if (s < 0)
		ns_info(ns_log_default, "fclose(%d) failed: %s", fd,
			strerror(errno));
	else
		ns_debug(ns_log_default, 3, "fclose(%d) succeeded", fd);
	return (s);
}

/*
 * Save a counted buffer and return a pointer to it.
 */
u_char *
savebuf(const u_char *buf, size_t len, int needpanic) {
	u_char *bp = (u_char *)memget(len);

	if (bp == NULL) {
		if (needpanic)
			panic("savebuf: memget failed (%s)", strerror(errno));
		else
			return (NULL);
	}
	memcpy(bp, buf, len);
	return (bp);
}

char *
__newstr(size_t len, int needpanic) {
	return (__newstr_record(len, needpanic, __FILE__, __LINE__));
}

char *
__savestr(const char *str, int needpanic) {
	return (__savestr_record(str, needpanic, __FILE__, __LINE__));
}

void
__freestr(char *str) {
	__freestr_record(str, __FILE__, __LINE__);
}

#ifdef DEBUG_STRINGS
char *
debug_newstr(size_t len, int needpanic, const char *file, int line) {
	size_t size;

	size = len + 3;	/* 2 length bytes + NUL. */
	printf("%s:%d: newstr %d\n", file, line, size);
	return (__newstr_record(len, needpanic, file, line));
}

char *
debug_savestr(const char *str, int needpanic, const char *file, int line) {
	size_t len;

	len = strlen(str);
	len += 3;	/* 2 length bytes + NUL. */
	printf("%s:%d: savestr %d %s\n", file, line, len, str);
	return (__savestr_record(str, needpanic, file, line));
}

void
debug_freestr(char *str, const char *file, int line) {
	u_char *buf, *bp;
	size_t len;

	buf = (u_char *)str - 2/*Len*/;
	bp = buf;
	NS_GET16(len, bp);
	len += 3;	/* 2 length bytes + NUL. */
	printf("%s:%d: freestr %d %s\n", file, line, len, str);
	__freestr_record(str, file, line);
	return;
}
#endif /* DEBUG_STRINGS */

/*
 * Return a counted string buffer big enough for a string of length 'len'.
 */
char *
__newstr_record(size_t len, int needpanic, char *file, int line) {
	u_char *buf, *bp;

	REQUIRE(len <= 65536);

	buf = (u_char *)__memget_record(2/*Len*/ + len + 1/*Nul*/, file, line);
	if (buf == NULL) {
		if (needpanic)
			panic("savestr: memget failed (%s)", strerror(errno));
		else
			return (NULL);
	}
	bp = buf;
	NS_PUT16(len, bp);
	return ((char *)bp);
}

/*
 * Save a NUL terminated string and return a pointer to it.
 */
char *
__savestr_record(const char *str, int needpanic, char *file, int line) {
	char *buf;
	size_t len;

	len = strlen(str);
	if (len > 65536) {
		if (needpanic)
			ns_panic(ns_log_default, 1,
				 "savestr: string too long");
		else
			return (NULL);
	}
	buf = __newstr_record(len, needpanic, file, line);
	memcpy(buf, str, len + 1);
	return (buf);
}

void
__freestr_record(char *str, char *file, int line) {
	u_char *buf, *bp;
	size_t len;

	buf = (u_char *)str - 2/*Len*/;
	bp = buf;
	NS_GET16(len, bp);
	__memput_record(buf, 2/*Len*/ + len + 1/*Nul*/, file, line);
}

char *
checked_ctime(const time_t *t) {
	char *ctime_result;

	ctime_result = ctime(t);
	if (ctime_result == NULL) {
		ns_error(ns_log_default, "ctime() returned NULL!");
		ctime_result = "<unknown time>\n";
	}

	return (ctime_result);
}

/*
 * Since the fields in a "struct timeval" are longs, and the argument to ctime
 * is a pointer to a time_t (which might not be a long), here's a bridge.
 */
char *
ctimel(long l) {
	time_t t = (time_t)l;

	return (checked_ctime(&t));
}

/*
 * rename() is lame (can't overwrite an existing file) on some systems.
 * use movefile() instead, and let lame OS ports do what they need to.
 */
#ifndef HAVE_MOVEFILE
int
movefile(const char *oldname, const char *newname) {
	return (rename(oldname, newname));
}
#endif

#ifdef ultrix
/*
 * Some library routines in libc need to be able to see the res_send
 * and res_close symbols with out __ prefix otherwise we get multiply
 * defined symbol errors when linking named.
 */

#undef res_send
int res_send(const u_char *buf, int buflen, u_char *ans, int anssiz) {
	return __res_send(buf, buflen, ans, anssiz);
}
#undef _res_close
void _res_close(void) {
	__res_close();
}
#endif
