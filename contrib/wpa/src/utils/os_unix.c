/*
 * OS specific functions for UNIX/POSIX systems
 * Copyright (c) 2005-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include <time.h>
#include <sys/wait.h>

#ifdef ANDROID
#include <sys/capability.h>
#include <sys/prctl.h>
#include <private/android_filesystem_config.h>
#endif /* ANDROID */

#ifdef __MACH__
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif /* __MACH__ */

#include "os.h"
#include "common.h"

#ifdef WPA_TRACE

#include "wpa_debug.h"
#include "trace.h"
#include "list.h"

static struct dl_list alloc_list = DL_LIST_HEAD_INIT(alloc_list);

#define ALLOC_MAGIC 0xa84ef1b2
#define FREED_MAGIC 0x67fd487a

struct os_alloc_trace {
	unsigned int magic;
	struct dl_list list __attribute__((aligned(16)));
	size_t len;
	WPA_TRACE_INFO
} __attribute__((aligned(16)));

#endif /* WPA_TRACE */


void os_sleep(os_time_t sec, os_time_t usec)
{
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200809L)
	const struct timespec req = { sec, usec * 1000 };

	nanosleep(&req, NULL);
#else
	if (sec)
		sleep(sec);
	if (usec)
		usleep(usec);
#endif
}


int os_get_time(struct os_time *t)
{
	int res;
	struct timeval tv;
	res = gettimeofday(&tv, NULL);
	t->sec = tv.tv_sec;
	t->usec = tv.tv_usec;
	return res;
}

int os_get_reltime(struct os_reltime *t)
{
#ifndef __MACH__
#if defined(CLOCK_BOOTTIME)
	static clockid_t clock_id = CLOCK_BOOTTIME;
#elif defined(CLOCK_MONOTONIC)
	static clockid_t clock_id = CLOCK_MONOTONIC;
#else
	static clockid_t clock_id = CLOCK_REALTIME;
#endif
	struct timespec ts;
	int res;

	if (TEST_FAIL())
		return -1;

	while (1) {
		res = clock_gettime(clock_id, &ts);
		if (res == 0) {
			t->sec = ts.tv_sec;
			t->usec = ts.tv_nsec / 1000;
			return 0;
		}
		switch (clock_id) {
#ifdef CLOCK_BOOTTIME
		case CLOCK_BOOTTIME:
			clock_id = CLOCK_MONOTONIC;
			break;
#endif
#ifdef CLOCK_MONOTONIC
/*
 * FreeBSD has both BOOTTIME and MONOTONIC defined to the same value, since they
 * mean the same thing. FreeBSD 14.1 and ealier don't, so need this case.
 */
#if !(defined(CLOCK_BOOTTIME) && CLOCK_BOOTTIME == CLOCK_MONOTONIC)
		case CLOCK_MONOTONIC:
			clock_id = CLOCK_REALTIME;
			break;
#endif
#endif
		case CLOCK_REALTIME:
			return -1;
		}
	}
#else /* __MACH__ */
	uint64_t abstime, nano;
	static mach_timebase_info_data_t info = { 0, 0 };

	if (!info.denom) {
		if (mach_timebase_info(&info) != KERN_SUCCESS)
			return -1;
	}

	abstime = mach_absolute_time();
	nano = (abstime * info.numer) / info.denom;

	t->sec = nano / NSEC_PER_SEC;
	t->usec = (nano - (((uint64_t) t->sec) * NSEC_PER_SEC)) / NSEC_PER_USEC;

	return 0;
#endif /* __MACH__ */
}


int os_mktime(int year, int month, int day, int hour, int min, int sec,
	      os_time_t *t)
{
	struct tm tm, *tm1;
	time_t t_local, t1, t2;
	os_time_t tz_offset;

	if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
	    hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 ||
	    sec > 60)
		return -1;

	memset(&tm, 0, sizeof(tm));
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;

	t_local = mktime(&tm);

	/* figure out offset to UTC */
	tm1 = localtime(&t_local);
	if (tm1) {
		t1 = mktime(tm1);
		tm1 = gmtime(&t_local);
		if (tm1) {
			t2 = mktime(tm1);
			tz_offset = t2 - t1;
		} else
			tz_offset = 0;
	} else
		tz_offset = 0;

	*t = (os_time_t) t_local - tz_offset;
	return 0;
}


int os_gmtime(os_time_t t, struct os_tm *tm)
{
	struct tm *tm2;
	time_t t2 = t;

	tm2 = gmtime(&t2);
	if (tm2 == NULL)
		return -1;
	tm->sec = tm2->tm_sec;
	tm->min = tm2->tm_min;
	tm->hour = tm2->tm_hour;
	tm->day = tm2->tm_mday;
	tm->month = tm2->tm_mon + 1;
	tm->year = tm2->tm_year + 1900;
	return 0;
}


#ifdef __APPLE__
#include <fcntl.h>
static int os_daemon(int nochdir, int noclose)
{
	int devnull;

	if (chdir("/") < 0)
		return -1;

	devnull = open("/dev/null", O_RDWR);
	if (devnull < 0)
		return -1;

	if (dup2(devnull, STDIN_FILENO) < 0) {
		close(devnull);
		return -1;
	}

	if (dup2(devnull, STDOUT_FILENO) < 0) {
		close(devnull);
		return -1;
	}

	if (dup2(devnull, STDERR_FILENO) < 0) {
		close(devnull);
		return -1;
	}

	return 0;
}
#else /* __APPLE__ */
#define os_daemon daemon
#endif /* __APPLE__ */


int os_daemonize(const char *pid_file)
{
#if defined(__uClinux__) || defined(__sun__)
	return -1;
#else /* defined(__uClinux__) || defined(__sun__) */
	if (os_daemon(0, 0)) {
		perror("daemon");
		return -1;
	}

	if (pid_file) {
		FILE *f = fopen(pid_file, "w");
		if (f) {
			fprintf(f, "%u\n", getpid());
			fclose(f);
		}
	}

	return -0;
#endif /* defined(__uClinux__) || defined(__sun__) */
}


void os_daemonize_terminate(const char *pid_file)
{
	if (pid_file)
		unlink(pid_file);
}


int os_get_random(unsigned char *buf, size_t len)
{
#ifdef TEST_FUZZ
	size_t i;

	for (i = 0; i < len; i++)
		buf[i] = i & 0xff;
	return 0;
#else /* TEST_FUZZ */
	FILE *f;
	size_t rc;

	if (TEST_FAIL())
		return -1;

	f = fopen("/dev/urandom", "rb");
	if (f == NULL) {
		printf("Could not open /dev/urandom.\n");
		return -1;
	}

	rc = fread(buf, 1, len, f);
	fclose(f);

	return rc != len ? -1 : 0;
#endif /* TEST_FUZZ */
}


unsigned long os_random(void)
{
	return random();
}


char * os_rel2abs_path(const char *rel_path)
{
	char *buf = NULL, *cwd, *ret;
	size_t len = 128, cwd_len, rel_len, ret_len;
	int last_errno;

	if (!rel_path)
		return NULL;

	if (rel_path[0] == '/')
		return os_strdup(rel_path);

	for (;;) {
		buf = os_malloc(len);
		if (buf == NULL)
			return NULL;
		cwd = getcwd(buf, len);
		if (cwd == NULL) {
			last_errno = errno;
			os_free(buf);
			if (last_errno != ERANGE)
				return NULL;
			len *= 2;
			if (len > 2000)
				return NULL;
		} else {
			buf[len - 1] = '\0';
			break;
		}
	}

	cwd_len = os_strlen(cwd);
	rel_len = os_strlen(rel_path);
	ret_len = cwd_len + 1 + rel_len + 1;
	ret = os_malloc(ret_len);
	if (ret) {
		os_memcpy(ret, cwd, cwd_len);
		ret[cwd_len] = '/';
		os_memcpy(ret + cwd_len + 1, rel_path, rel_len);
		ret[ret_len - 1] = '\0';
	}
	os_free(buf);
	return ret;
}


int os_program_init(void)
{
	unsigned int seed;

#ifdef ANDROID
	/*
	 * We ignore errors here since errors are normal if we
	 * are already running as non-root.
	 */
#ifdef ANDROID_SETGROUPS_OVERRIDE
	gid_t groups[] = { ANDROID_SETGROUPS_OVERRIDE };
#else /* ANDROID_SETGROUPS_OVERRIDE */
	gid_t groups[] = { AID_INET, AID_WIFI, AID_KEYSTORE };
#endif /* ANDROID_SETGROUPS_OVERRIDE */
	struct __user_cap_header_struct header;
	struct __user_cap_data_struct cap;

	setgroups(ARRAY_SIZE(groups), groups);

	prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);

	setgid(AID_WIFI);
	setuid(AID_WIFI);

	header.version = _LINUX_CAPABILITY_VERSION;
	header.pid = 0;
	cap.effective = cap.permitted =
		(1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW);
	cap.inheritable = 0;
	capset(&header, &cap);
#endif /* ANDROID */

	if (os_get_random((unsigned char *) &seed, sizeof(seed)) == 0)
		srandom(seed);

	return 0;
}


void os_program_deinit(void)
{
#ifdef WPA_TRACE
	struct os_alloc_trace *a;
	unsigned long total = 0;
	dl_list_for_each(a, &alloc_list, struct os_alloc_trace, list) {
		total += a->len;
		if (a->magic != ALLOC_MAGIC) {
			wpa_printf(MSG_INFO, "MEMLEAK[%p]: invalid magic 0x%x "
				   "len %lu",
				   a, a->magic, (unsigned long) a->len);
			continue;
		}
		wpa_printf(MSG_INFO, "MEMLEAK[%p]: len %lu",
			   a, (unsigned long) a->len);
		wpa_trace_dump("memleak", a);
	}
	if (total)
		wpa_printf(MSG_INFO, "MEMLEAK: total %lu bytes",
			   (unsigned long) total);
	wpa_trace_deinit();
#endif /* WPA_TRACE */
}


int os_setenv(const char *name, const char *value, int overwrite)
{
	return setenv(name, value, overwrite);
}


int os_unsetenv(const char *name)
{
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__) || \
    defined(__OpenBSD__)
	unsetenv(name);
	return 0;
#else
	return unsetenv(name);
#endif
}


char * os_readfile(const char *name, size_t *len)
{
	FILE *f;
	char *buf;
	long pos;

	f = fopen(name, "rb");
	if (f == NULL)
		return NULL;

	if (fseek(f, 0, SEEK_END) < 0 || (pos = ftell(f)) < 0) {
		fclose(f);
		return NULL;
	}
	*len = pos;
	if (fseek(f, 0, SEEK_SET) < 0) {
		fclose(f);
		return NULL;
	}

	buf = os_malloc(*len);
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}

	if (fread(buf, 1, *len, f) != *len) {
		fclose(f);
		os_free(buf);
		return NULL;
	}

	fclose(f);

	return buf;
}


int os_file_exists(const char *fname)
{
	return access(fname, F_OK) == 0;
}


#if !defined __DragonFly__
int os_fdatasync(FILE *stream)
{
	if (!fflush(stream)) {
#if defined __FreeBSD__ || defined __linux__
		return fdatasync(fileno(stream));
#else /* !__linux__ && !__FreeBSD__ */
#ifdef F_FULLFSYNC
		/* OS X does not implement fdatasync(). */
		return fcntl(fileno(stream), F_FULLFSYNC);
#else /* F_FULLFSYNC */
		return fsync(fileno(stream));
#endif /* F_FULLFSYNC */
#endif /* __linux__ */
	}

	return -1;
}
#endif


#ifndef WPA_TRACE
void * os_zalloc(size_t size)
{
	return calloc(1, size);
}
#endif /* WPA_TRACE */


size_t os_strlcpy(char *dest, const char *src, size_t siz)
{
	const char *s = src;
	size_t left = siz;

	if (left) {
		/* Copy string up to the maximum size of the dest buffer */
		while (--left != 0) {
			if ((*dest++ = *s++) == '\0')
				break;
		}
	}

	if (left == 0) {
		/* Not enough room for the string; force NUL-termination */
		if (siz != 0)
			*dest = '\0';
		while (*s++)
			; /* determine total src string length */
	}

	return s - src - 1;
}


int os_memcmp_const(const void *a, const void *b, size_t len)
{
	const u8 *aa = a;
	const u8 *bb = b;
	size_t i;
	u8 res;

	for (res = 0, i = 0; i < len; i++)
		res |= aa[i] ^ bb[i];

	return res;
}


void * os_memdup(const void *src, size_t len)
{
	void *r = os_malloc(len);

	if (r && src)
		os_memcpy(r, src, len);
	return r;
}


#ifdef WPA_TRACE

#if defined(WPA_TRACE_BFD) && defined(CONFIG_TESTING_OPTIONS)
struct wpa_trace_test_fail {
	unsigned int fail_after;
	char pattern[256];
} wpa_trace_test_fail[5][2];

int testing_test_fail(const char *tag, bool is_alloc)
{
	const char *ignore_list[] = {
		"os_malloc", "os_zalloc", "os_calloc", "os_realloc",
		"os_realloc_array", "os_strdup", "os_memdup"
	};
	const char *func[WPA_TRACE_LEN];
	size_t i, j, res, len, idx;
	char *pos, *next;
	int match;

	is_alloc = !!is_alloc;

	for (idx = 0; idx < ARRAY_SIZE(wpa_trace_test_fail[is_alloc]); idx++) {
		if (wpa_trace_test_fail[is_alloc][idx].fail_after != 0)
			break;
	}
	if (idx >= ARRAY_SIZE(wpa_trace_test_fail[is_alloc]))
		return 0;

	res = wpa_trace_calling_func(func, WPA_TRACE_LEN);
	i = 0;

	if (is_alloc) {
		/* Skip our own stack frame */
		i++;

		/* Skip allocation helpers */
		for (j = 0; j < ARRAY_SIZE(ignore_list) && i < res; j++) {
			if (os_strcmp(func[i], ignore_list[j]) == 0)
				i++;
		}
	} else {
		/* Not allocation, we might have a tag, if so, replace our
		 * own stack frame with the tag, otherwise skip it.
		 */
		if (tag)
			func[0] = tag;
		else
			i++;
	}

	pos = wpa_trace_test_fail[is_alloc][idx].pattern;

	/* The prefixes mean:
	 * - '=': The function needs to be next in the backtrace
	 * - '?': The function is optionally present in the backtrace
	 */

	match = 0;
	while (i < res) {
		int allow_skip = 1;
		int maybe = 0;
		bool prefix = false;

		if (*pos == '=') {
			allow_skip = 0;
			pos++;
		} else if (*pos == '?') {
			maybe = 1;
			pos++;
		}
		next = os_strchr(pos, ';');
		if (next)
			len = next - pos;
		else
			len = os_strlen(pos);
		if (len >= 1 && pos[len - 1] == '*') {
			prefix = true;
			len -= 1;
		}
		if (os_strncmp(pos, func[i], len) != 0 ||
		    (!prefix && func[i][len] != '\0')) {
			if (maybe && next) {
				pos = next + 1;
				continue;
			}
			if (allow_skip) {
				i++;
				continue;
			}
			return 0;
		}
		if (!next) {
			match = 1;
			break;
		}
		pos = next + 1;
		i++;
	}
	if (!match)
		return 0;

	wpa_trace_test_fail[is_alloc][idx].fail_after--;
	if (wpa_trace_test_fail[is_alloc][idx].fail_after == 0) {
		wpa_printf(MSG_INFO, "TESTING: fail at %s",
			   wpa_trace_test_fail[is_alloc][idx].pattern);
		for (i = 0; i < res; i++)
			wpa_printf(MSG_INFO, "backtrace[%d] = %s",
				   (int) i, func[i]);
		return 1;
	}

	return 0;
}


int testing_set_fail_pattern(bool is_alloc, char *patterns)
{
#ifdef WPA_TRACE_BFD
	char *token, *context = NULL;
	size_t idx;

	is_alloc = !!is_alloc;

	os_memset(wpa_trace_test_fail[is_alloc], 0,
		  sizeof(wpa_trace_test_fail[is_alloc]));

	idx = 0;
	while ((token = str_token(patterns, " \n\r\t", &context)) &&
	       idx < ARRAY_SIZE(wpa_trace_test_fail[is_alloc])) {
		wpa_trace_test_fail[is_alloc][idx].fail_after = atoi(token);
		token = os_strchr(token, ':');
		if (!token) {
			os_memset(wpa_trace_test_fail[is_alloc], 0,
				  sizeof(wpa_trace_test_fail[is_alloc]));
			return -1;
		}

		os_strlcpy(wpa_trace_test_fail[is_alloc][idx].pattern,
			   token + 1,
			   sizeof(wpa_trace_test_fail[is_alloc][0].pattern));
		idx++;
	}

	return 0;
#else /* WPA_TRACE_BFD */
	return -1;
#endif /* WPA_TRACE_BFD */
}


int testing_get_fail_pattern(bool is_alloc, char *buf, size_t buflen)
{
#ifdef WPA_TRACE_BFD
	size_t idx, ret;
	char *pos = buf;
	char *end = buf + buflen;

	is_alloc = !!is_alloc;

	for (idx = 0; idx < ARRAY_SIZE(wpa_trace_test_fail[is_alloc]); idx++) {
		if (wpa_trace_test_fail[is_alloc][idx].pattern[0] == '\0')
			break;

		ret = os_snprintf(pos, end - pos, "%s%u:%s",
				  pos == buf ? "" : " ",
				  wpa_trace_test_fail[is_alloc][idx].fail_after,
				  wpa_trace_test_fail[is_alloc][idx].pattern);
		if (os_snprintf_error(end - pos, ret))
			break;
		pos += ret;
	}

	return pos - buf;
#else /* WPA_TRACE_BFD */
	return -1;
#endif /* WPA_TRACE_BFD */
}

#else /* defined(WPA_TRACE_BFD) && defined(CONFIG_TESTING_OPTIONS) */

static inline int testing_test_fail(const char *tag, bool is_alloc)
{
	return 0;
}

#endif

void * os_malloc(size_t size)
{
	struct os_alloc_trace *a;

	if (testing_test_fail(NULL, true))
		return NULL;

	a = malloc(sizeof(*a) + size);
	if (a == NULL)
		return NULL;
	a->magic = ALLOC_MAGIC;
	dl_list_add(&alloc_list, &a->list);
	a->len = size;
	wpa_trace_record(a);
	return a + 1;
}


void * os_realloc(void *ptr, size_t size)
{
	struct os_alloc_trace *a;
	size_t copy_len;
	void *n;

	if (ptr == NULL)
		return os_malloc(size);

	a = (struct os_alloc_trace *) ptr - 1;
	if (a->magic != ALLOC_MAGIC) {
		wpa_printf(MSG_INFO, "REALLOC[%p]: invalid magic 0x%x%s",
			   a, a->magic,
			   a->magic == FREED_MAGIC ? " (already freed)" : "");
		wpa_trace_show("Invalid os_realloc() call");
		abort();
	}
	n = os_malloc(size);
	if (n == NULL)
		return NULL;
	copy_len = a->len;
	if (copy_len > size)
		copy_len = size;
	os_memcpy(n, a + 1, copy_len);
	os_free(ptr);
	return n;
}


void os_free(void *ptr)
{
	struct os_alloc_trace *a;

	if (ptr == NULL)
		return;
	a = (struct os_alloc_trace *) ptr - 1;
	if (a->magic != ALLOC_MAGIC) {
		wpa_printf(MSG_INFO, "FREE[%p]: invalid magic 0x%x%s",
			   a, a->magic,
			   a->magic == FREED_MAGIC ? " (already freed)" : "");
		wpa_trace_show("Invalid os_free() call");
		abort();
	}
	dl_list_del(&a->list);
	a->magic = FREED_MAGIC;

	wpa_trace_check_ref(ptr);
	free(a);
}


void * os_zalloc(size_t size)
{
	void *ptr = os_malloc(size);
	if (ptr)
		os_memset(ptr, 0, size);
	return ptr;
}


char * os_strdup(const char *s)
{
	size_t len;
	char *d;
	len = os_strlen(s);
	d = os_malloc(len + 1);
	if (d == NULL)
		return NULL;
	os_memcpy(d, s, len);
	d[len] = '\0';
	return d;
}

#endif /* WPA_TRACE */


int os_exec(const char *program, const char *arg, int wait_completion)
{
	pid_t pid;
	int pid_status;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return -1;
	}

	if (pid == 0) {
		/* run the external command in the child process */
		const int MAX_ARG = 30;
		char *_program, *_arg, *pos;
		char *argv[MAX_ARG + 1];
		int i;

		_program = os_strdup(program);
		_arg = os_strdup(arg);

		argv[0] = _program;

		i = 1;
		pos = _arg;
		while (i < MAX_ARG && pos && *pos) {
			while (*pos == ' ')
				pos++;
			if (*pos == '\0')
				break;
			argv[i++] = pos;
			pos = os_strchr(pos, ' ');
			if (pos)
				*pos++ = '\0';
		}
		argv[i] = NULL;

		execv(program, argv);
		perror("execv");
		os_free(_program);
		os_free(_arg);
		exit(0);
		return -1;
	}

	if (wait_completion) {
		/* wait for the child process to complete in the parent */
		waitpid(pid, &pid_status, 0);
	}

	return 0;
}
