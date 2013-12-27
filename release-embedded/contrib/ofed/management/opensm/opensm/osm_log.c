/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2006 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Implementation of osm_log_t.
 * This object represents the log file.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <opensm/osm_log.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

static int log_exit_count = 0;

#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#include <complib/cl_timer.h>

static char *month_str[] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};
#else
void OsmReportState(IN const char *p_str);
#endif				/* ndef WIN32 */

#ifndef WIN32

static void truncate_log_file(osm_log_t * const p_log)
{
	int fd = fileno(p_log->out_port);
	if (ftruncate(fd, 0) < 0)
		fprintf(stderr, "truncate_log_file: cannot truncate: %s\n",
			strerror(errno));
	if (lseek(fd, 0, SEEK_SET) < 0)
		fprintf(stderr, "truncate_log_file: cannot rewind: %s\n",
			strerror(errno));
	p_log->count = 0;
}

#else				/* Windows */

static void truncate_log_file(osm_log_t * const p_log)
{
	fprintf(stderr,
		"truncate_log_file: cannot truncate on windows system (yet)\n");
}
#endif				/* ndef WIN32 */

void osm_log(IN osm_log_t * const p_log,
	     IN const osm_log_level_t verbosity, IN const char *p_str, ...)
{
	char buffer[LOG_ENTRY_SIZE_MAX];
	va_list args;
	int ret;
#ifdef WIN32
	SYSTEMTIME st;
	uint32_t pid = GetCurrentThreadId();
#else
	pid_t pid = 0;
	time_t tim;
	struct tm result;
	uint64_t time_usecs;
	uint32_t usecs;
#endif				/* WIN32 */

	/* If this is a call to syslog - always print it */
	if (!(verbosity & (OSM_LOG_SYS | p_log->level)))
		return;

	va_start(args, p_str);
	vsprintf(buffer, p_str, args);
	va_end(args);

	/* this is a call to the syslog */
	if (verbosity & OSM_LOG_SYS) {
		syslog(LOG_INFO, "%s\n", buffer);

		/* SYSLOG should go to stdout too */
		if (p_log->out_port != stdout) {
			printf("%s\n", buffer);
			fflush(stdout);
		}
#ifdef WIN32
		OsmReportState(buffer);
#endif				/* WIN32 */
	}

	/* regular log to default out_port */
	cl_spinlock_acquire(&p_log->lock);

	if (p_log->max_size && p_log->count > p_log->max_size) {
		/* truncate here */
		fprintf(stderr,
			"osm_log: log file exceeds the limit %lu. Truncating.\n",
			p_log->max_size);
		truncate_log_file(p_log);
	}
#ifdef WIN32
	GetLocalTime(&st);
_retry:
	ret =
	    fprintf(p_log->out_port,
		    "[%02d:%02d:%02d:%03d][%04X] 0x%02x -> %s",
		    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		    pid, verbosity, buffer);
#else
	time_usecs = cl_get_time_stamp();
	tim = time_usecs / 1000000;
	usecs = time_usecs % 1000000;
	localtime_r(&tim, &result);
	pid = pthread_self();
_retry:
	ret =
	    fprintf(p_log->out_port,
		    "%s %02d %02d:%02d:%02d %06d [%04X] 0x%02x -> %s",
		    (result.tm_mon <
		     12 ? month_str[result.tm_mon] : "???"),
		    result.tm_mday, result.tm_hour, result.tm_min,
		    result.tm_sec, usecs, pid, verbosity, buffer);
#endif

	/*  flush log */
	if (ret > 0 &&
	    (p_log->flush || (verbosity & (OSM_LOG_ERROR | OSM_LOG_SYS)))
	    && fflush(p_log->out_port) < 0)
		ret = -1;

	if (ret >= 0) {
		log_exit_count = 0;
		p_log->count += ret;
	} else if (log_exit_count < 3) {
		log_exit_count++;
		if (errno == ENOSPC && p_log->max_size) {
			fprintf(stderr,
				"osm_log: write failed: %s. Truncating log file.\n",
				strerror(errno));
			truncate_log_file(p_log);
			goto _retry;
		}
		fprintf(stderr, "osm_log: write failed: %s\n", strerror(errno));
	}

	cl_spinlock_release(&p_log->lock);
}

void osm_log_raw(IN osm_log_t * const p_log,
		 IN const osm_log_level_t verbosity, IN const char *p_buf)
{
	if (p_log->level & verbosity) {
		cl_spinlock_acquire(&p_log->lock);
		printf("%s", p_buf);
		cl_spinlock_release(&p_log->lock);

		/*
		   Flush log on errors too.
		 */
		if (p_log->flush || (verbosity & OSM_LOG_ERROR))
			fflush(stdout);
	}
}

void osm_log_msg_box(IN osm_log_t * log, osm_log_level_t level,
		     const char *func_name, const char *msg)
{
#define MSG_BOX_LENGTH 66
	char buf[MSG_BOX_LENGTH + 1];
	int i, n;

	if (!osm_log_is_active(log, level))
		return;

	n = (MSG_BOX_LENGTH - strlen(msg)) / 2 - 1;
	if (n < 0)
		n = 0;
	for (i = 0; i < n; i++)
		sprintf(buf + i, "*");
	n += snprintf(buf + n, sizeof(buf) - n, " %s ", msg);
	for (i = n; i < MSG_BOX_LENGTH; i++)
		buf[i] = '*';
	buf[i] = '\0';

	osm_log(log, level, "%s:\n\n\n"
		"*********************************************"
		"*********************\n%s\n"
		"*********************************************"
		"*********************\n\n\n", func_name, buf);
}

boolean_t osm_is_debug(void)
{
#if defined( _DEBUG_ )
	return TRUE;
#else
	return FALSE;
#endif				/* defined( _DEBUG_ ) */
}

static int open_out_port(IN osm_log_t * p_log)
{
	struct stat st;

	if (p_log->accum_log_file)
		p_log->out_port = fopen(p_log->log_file_name, "a+");
	else
		p_log->out_port = fopen(p_log->log_file_name, "w+");

	if (!p_log->out_port) {
		syslog(LOG_CRIT, "Cannot open file \'%s\' for %s: %s\n",
		       p_log->log_file_name,
		       p_log->accum_log_file ? "appending" : "writing",
		       strerror(errno));
		fprintf(stderr, "Cannot open file \'%s\': %s\n",
			p_log->log_file_name, strerror(errno));
		return -1;
	}

	if (fstat(fileno(p_log->out_port), &st) == 0)
		p_log->count = st.st_size;

	syslog(LOG_NOTICE, "%s log file opened\n", p_log->log_file_name);

	if (p_log->daemon) {
		dup2(fileno(p_log->out_port), 0);
		dup2(fileno(p_log->out_port), 1);
		dup2(fileno(p_log->out_port), 2);
	}

	return 0;
}

int osm_log_reopen_file(osm_log_t * p_log)
{
	int ret;

	if (p_log->out_port == stdout || p_log->out_port == stderr)
		return 0;
	cl_spinlock_acquire(&p_log->lock);
	fclose(p_log->out_port);
	ret = open_out_port(p_log);
	cl_spinlock_release(&p_log->lock);
	return ret;
}

ib_api_status_t osm_log_init_v2(IN osm_log_t * const p_log,
				IN const boolean_t flush,
				IN const uint8_t log_flags,
				IN const char *log_file,
				IN const unsigned long max_size,
				IN const boolean_t accum_log_file)
{
	p_log->level = log_flags;
	p_log->flush = flush;
	p_log->count = 0;
	p_log->max_size = max_size;
	p_log->accum_log_file = accum_log_file;
	p_log->log_file_name = (char *)log_file;

	openlog("OpenSM", LOG_CONS | LOG_PID, LOG_USER);

	if (log_file == NULL || !strcmp(log_file, "-") ||
	    !strcmp(log_file, "stdout"))
		p_log->out_port = stdout;
	else if (!strcmp(log_file, "stderr"))
		p_log->out_port = stderr;
	else if (open_out_port(p_log))
		return IB_ERROR;

	if (cl_spinlock_init(&p_log->lock) == CL_SUCCESS)
		return IB_SUCCESS;
	else
		return IB_ERROR;
}

ib_api_status_t osm_log_init(IN osm_log_t * const p_log,
			     IN const boolean_t flush,
			     IN const uint8_t log_flags,
			     IN const char *log_file,
			     IN const boolean_t accum_log_file)
{
	return osm_log_init_v2(p_log, flush, log_flags, log_file, 0,
			       accum_log_file);
}
