/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Rick Parrish <unitrunker@unitrunker.net>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <mqueue.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

struct Creation {
	/* true if the queue exists. */
	bool exists;
	/* true if a mode value was specified. */
	bool set_mode;
	/* access mode with rwx permission bits. */
	mode_t mode;
	/* maximum queue depth. default to an invalid depth. */
	long depth;
	/* maximum message size. default to an invalid size. */
	long size;
	/* true for blocking I/O and false for non-blocking I/O. */
	bool block;
	/* true if a group ID was specified. */
	bool set_group;
	/* group ID. */
	gid_t group;
	/* true if a user ID was specified. */
	bool set_user;
	/* user ID. */
	uid_t user;
};

struct element {
	STAILQ_ENTRY(element) links;
	const char *text;
};

static struct element *
malloc_element(const char *context)
{
	struct element *item = malloc(sizeof(struct element));

	if (item == NULL)
		/* the only non-EX_* prefixed exit code. */
		err(1, "malloc(%s)", context);
	return (item);
}

static STAILQ_HEAD(tqh, element)
	queues = STAILQ_HEAD_INITIALIZER(queues),
	contents = STAILQ_HEAD_INITIALIZER(contents);
/* send defaults to medium priority. */
static long priority = MQ_PRIO_MAX / 2;
static struct Creation creation = {
	.exists = false,
	.set_mode = false,
	.mode = 0755,
	.depth = -1,
	.size = -1,
	.block = true,
	.set_group = false,
	.group = 0,
	.set_user = false,
	.user = 0
};
static const mqd_t fail = (mqd_t)-1;
static const mode_t accepted_mode_bits =
    S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISTXT;

/* OPTIONS parsing utilitarian */

static void
parse_long(const char *text, long *capture, const char *knob, const char *name)
{
	char *cursor = NULL;
	long value = strtol(text, &cursor, 10);

	if (cursor > text && *cursor == 0) {
		*capture = value;
	} else {
		warnx("%s %s invalid format [%s].", knob, name, text);
	}
}

static void
parse_unsigned(const char *text, bool *set,
   unsigned *capture, const char *knob, const char *name)
{
	char *cursor = NULL;
	unsigned value = strtoul(text, &cursor, 8);

	if (cursor > text && *cursor == 0) {
		*set = true;
		*capture = value;
	} else {
		warnx("%s %s format [%s] ignored.", knob, name, text);
	}
}

static bool
sane_queue(const char *queue)
{
	int size = 0;

	if (queue[size] != '/') {
		warnx("queue name [%-.*s] must start with '/'.", NAME_MAX, queue);
		return (false);
	}

	for (size++; queue[size] != 0 && size < NAME_MAX; size++) {
		if (queue[size] == '/') {
			warnx("queue name [%-.*s] - only one '/' permitted.",
			    NAME_MAX, queue);
			return (false);
		}
	}

	if (size == NAME_MAX && queue[size] != 0) {
		warnx("queue name [%-.*s...] may not be longer than %d.",
		    NAME_MAX, queue, NAME_MAX);
		return (false);
	}
	return (true);
}

/* OPTIONS parsers */

static void
parse_block(const char *text)
{
	if (strcmp(text, "true") == 0 || strcmp(text, "yes") == 0) {
		creation.block = true;
	} else if (strcmp(text, "false") == 0 || strcmp(text, "no") == 0) {
		creation.block = false;
	} else {
		char *cursor = NULL;
		long value = strtol(text, &cursor, 10);
		if (cursor > text) {
			creation.block = value != 0;
		} else {
			warnx("bad -b block format [%s] ignored.", text);
		}
	}
}

static void
parse_content(const char *content)
{
	struct element *n1 = malloc_element("content");

	n1->text = content;
	STAILQ_INSERT_TAIL(&contents, n1, links);
}

static void
parse_depth(const char *text)
{
	parse_long(text, &creation.depth, "-d", "depth");
}

static void
parse_group(const char *text)
{
	struct group *entry = getgrnam(text);

	if (entry == NULL) {
		parse_unsigned(text, &creation.set_group,
		    &creation.group, "-g", "group");
	} else {
		creation.set_group = true;
		creation.group = entry->gr_gid;
	}
}

static void
parse_mode(const char *text)
{
	char *cursor = NULL;
	long value = strtol(text, &cursor, 8);

	// verify only accepted mode bits are set.
	if (cursor > text && *cursor == 0 && (value & accepted_mode_bits) == value) {
		creation.set_mode = true;
		creation.mode = (mode_t)value;
	} else {
		warnx("impossible -m mode value [%s] ignored.", text);
	}
}

static void
parse_priority(const char *text)
{
	char *cursor = NULL;
	long value = strtol(text, &cursor, 10);

	if (cursor > text && *cursor == 0) {
		if (value >= 0 && value < MQ_PRIO_MAX) {
			priority = value;
		} else {
			warnx("bad -p priority range [%s] ignored.", text);
		}
	} else {
		warnx("bad -p priority format [%s] ignored.", text);
	}
}

static void
parse_queue(const char *queue)
{
	if (sane_queue(queue)) {
		struct element *n1 = malloc_element("queue name");

		n1->text = queue;
		STAILQ_INSERT_TAIL(&queues, n1, links);
	}
}

static void
parse_single_queue(const char *queue)
{
	if (sane_queue(queue)) {
		if (STAILQ_EMPTY(&queues)) {
			struct element *n1 = malloc_element("queue name");

			n1->text = queue;
			STAILQ_INSERT_TAIL(&queues, n1, links);
		} else
			warnx("ignoring extra -q queue [%s].", queue);
	}
}

static void
parse_size(const char *text)
{
	parse_long(text, &creation.size, "-s", "size");
}

static void
parse_user(const char *text)
{
	struct passwd *entry = getpwnam(text);
	if (entry == NULL) {
		parse_unsigned(text, &creation.set_user,
		    &creation.user, "-u", "user");
	} else {
		creation.set_user = true;
		creation.user = entry->pw_uid;
	}
}

/* OPTIONS validators */

static bool
validate_always_true(void)
{
	return (true);
}

static bool
validate_content(void)
{
	bool valid = !STAILQ_EMPTY(&contents);

	if (!valid)
		warnx("no content to send.");
	return (valid);
}

static bool
validate_depth(void)
{
	bool valid = creation.exists || creation.depth > 0;

	if (!valid)
		warnx("-d maximum queue depth not provided.");
	return (valid);
}

static bool
validate_queue(void)
{
	bool valid = !STAILQ_EMPTY(&queues);

	if (!valid)
		warnx("missing -q, or no sane queue name given.");
	return (valid);
}

static bool
validate_single_queue(void)
{
	bool valid = !STAILQ_EMPTY(&queues) &&
	    STAILQ_NEXT(STAILQ_FIRST(&queues), links) == NULL;

	if (!valid)
		warnx("expected one queue.");
	return (valid);
}

static bool
validate_size(void)
{
	bool valid = creation.exists || creation.size > 0;

	if (!valid)
		warnx("-s maximum message size not provided.");
	return (valid);
}

/* OPTIONS table handling. */

struct Option {
	/* points to array of string pointers terminated by a null pointer. */
	const char **pattern;
	/* parse argument. */
	void (*parse)(const char *);
	/*
	 * displays an error and returns false if this parameter is not valid.
	 * returns true otherwise.
	 */
	bool (*validate)(void);
};

/*
 * parse options by table.
 * index - current index into argv list.
 * argc, argv - command line parameters.
 * options - null terminated list of pointers to options.
 */
static void
parse_options(int index, int argc,
    const char *argv[], const struct Option **options)
{
	while ((index + 1) < argc) {
		const struct Option **cursor = options;
		bool match = false;
		while (*cursor != NULL && !match) {
			const struct Option *option = cursor[0];
			const char **pattern = option->pattern;

			while (*pattern != NULL && !match) {
				const char *knob = *pattern;

				match = strcmp(knob, argv[index]) == 0;
				if (!match)
					pattern++;
			}

			if (match) {
				option->parse(argv[index + 1]);
				index += 2;
				break;
			}
			cursor++;
		}

		if (!match && index < argc) {
			warnx("skipping [%s].", argv[index]);
			index++;
		}
	}

	if (index < argc) {
		warnx("skipping [%s].", argv[index]);
	}
}

/* options - null terminated list of pointers to options. */
static bool
validate_options(const struct Option **options)
{
	bool valid = true;

	while (*options != NULL) {
		const struct Option *option = options[0];

		if (!option->validate())
			valid = false;
		options++;
	}
	return (valid);
}

/* SUBCOMMANDS */

/*
 * queue: name of queue to be created.
 * q_creation: creation parameters (copied by value).
 */
static int
create(const char *queue, struct Creation q_creation)
{
	int flags = O_RDWR;
	struct mq_attr stuff = {
		.mq_curmsgs = 0,
		.mq_maxmsg = q_creation.depth,
		.mq_msgsize = q_creation.size,
		.mq_flags = 0
	};

	if (!q_creation.block) {
		flags |= O_NONBLOCK;
		stuff.mq_flags |= O_NONBLOCK;
	}

	mqd_t handle = mq_open(queue, flags);
	q_creation.exists = handle != fail;
	if (!q_creation.exists) {
		/*
		 * apply size and depth checks here.
		 * if queue exists, we can default to existing depth and size.
		 * but for a new queue, we require that input.
		 */
		if (validate_size() && validate_depth()) {
			/* no need to re-apply mode. */
			q_creation.set_mode = false;
			flags |= O_CREAT;
			handle = mq_open(queue, flags, q_creation.mode, &stuff);
		}
	}

	if (handle == fail) {
		errno_t what = errno;

		warnc(what, "mq_open(create)");
		return (what);
	}

#ifdef __FreeBSD__
	/*
	 * undocumented.
	 * See https://bugs.freebsd.org/bugzilla//show_bug.cgi?id=273230
	 */
	int fd = mq_getfd_np(handle);

	if (fd < 0) {
		errno_t what = errno;

		warnc(what, "mq_getfd_np(create)");
		mq_close(handle);
		return (what);
	}
	struct stat status = {0};
	int result = fstat(fd, &status);
	if (result != 0) {
		errno_t what = errno;

		warnc(what, "fstat(create)");
		mq_close(handle);
		return (what);
	}

	/* do this only if group and / or user given. */
	if (q_creation.set_group || q_creation.set_user) {
		q_creation.user =
		    q_creation.set_user ? q_creation.user : status.st_uid;
		q_creation.group =
		    q_creation.set_group ? q_creation.group : status.st_gid;
		result = fchown(fd, q_creation.user, q_creation.group);
		if (result != 0) {
			errno_t what = errno;

			warnc(what, "fchown(create)");
			mq_close(handle);
			return (what);
		}
	}

	/* do this only if altering mode of an existing queue. */
	if (q_creation.exists && q_creation.set_mode &&
	    q_creation.mode != (status.st_mode & accepted_mode_bits)) {
		result = fchmod(fd, q_creation.mode);
		if (result != 0) {
			errno_t what = errno;

			warnc(what, "fchmod(create)");
			mq_close(handle);
			return (what);
		}
	}
#endif /* __FreeBSD__ */

	return (mq_close(handle));
}

/* queue: name of queue to be removed. */
static int
rm(const char *queue)
{
	int result = mq_unlink(queue);

	if (result != 0) {
		errno_t what = errno;

		warnc(what, "mq_unlink");
		return (what);
	}

	return (result);
}

/* Return the display character for non-zero mode. */
static char
dual(mode_t mode, char display)
{
	return (mode != 0 ? display : '-');
}

/* Select one of four display characters based on mode and modifier. */
static char
quad(mode_t mode, mode_t modifier)
{
	static const char display[] = "-xSs";
	unsigned index = 0;
	if (mode != 0)
		index += 1;
	if (modifier)
		index += 2;
	return (display[index]);
}

/* queue: name of queue to be inspected. */
static int
info(const char *queue)
{
	mqd_t handle = mq_open(queue, O_RDONLY);

	if (handle == fail) {
		errno_t what = errno;

		warnc(what, "mq_open(info)");
		return (what);
	}

	struct mq_attr actual;

	int result = mq_getattr(handle, &actual);
	if (result != 0) {
		errno_t what = errno;

		warnc(what, "mq_getattr(info)");
		return (what);
	}

	fprintf(stdout,
	    "queue: '%s'\nQSIZE: %lu\nMSGSIZE: %ld\nMAXMSG: %ld\n"
	    "CURMSG: %ld\nflags: %03ld\n",
	    queue, actual.mq_msgsize * actual.mq_curmsgs, actual.mq_msgsize,
	    actual.mq_maxmsg, actual.mq_curmsgs, actual.mq_flags);
#ifdef __FreeBSD__

	int fd = mq_getfd_np(handle);
	struct stat status;

	result = fstat(fd, &status);
	if (result != 0) {
		warn("fstat(info)");
	} else {
		mode_t mode = status.st_mode;

		fprintf(stdout, "UID: %u\nGID: %u\n", status.st_uid, status.st_gid);
		fprintf(stdout, "MODE: %c%c%c%c%c%c%c%c%c%c\n",
		    dual(mode & S_ISVTX, 's'),
		    dual(mode & S_IRUSR, 'r'),
		    dual(mode & S_IWUSR, 'w'),
		    quad(mode & S_IXUSR, mode & S_ISUID),
		    dual(mode & S_IRGRP, 'r'),
		    dual(mode & S_IWGRP, 'w'),
		    quad(mode & S_IXGRP, mode & S_ISGID),
		    dual(mode & S_IROTH, 'r'),
		    dual(mode & S_IWOTH, 'w'),
		    dual(mode & S_IXOTH, 'x'));
	}
#endif /* __FreeBSD__ */

	return (mq_close(handle));
}

/* queue: name of queue to drain one message. */
static int
recv(const char *queue)
{
	mqd_t handle = mq_open(queue, O_RDONLY);

	if (handle == fail) {
		errno_t what = errno;

		warnc(what, "mq_open(recv)");
		return (what);
	}

	struct mq_attr actual;

	int result = mq_getattr(handle, &actual);

	if (result != 0) {
		errno_t what = errno;

		warnc(what, "mq_attr(recv)");
		mq_close(handle);
		return (what);
	}

	char *text = malloc(actual.mq_msgsize + 1);
	unsigned q_priority = 0;

	memset(text, 0, actual.mq_msgsize + 1);
	result = mq_receive(handle, text, actual.mq_msgsize, &q_priority);
	if (result < 0) {
		errno_t what = errno;

		warnc(what, "mq_receive");
		mq_close(handle);
		return (what);
	}

	fprintf(stdout, "[%u]: %-*.*s\n", q_priority, result, result, text);
	return (mq_close(handle));
}

/*
 * queue: name of queue to send one message.
 * text: message text.
 * q_priority: message priority in range of 0 to 63.
 */
static int
send(const char *queue, const char *text, unsigned q_priority)
{
	mqd_t handle = mq_open(queue, O_WRONLY);

	if (handle == fail) {
		errno_t what = errno;

		warnc(what, "mq_open(send)");
		return (what);
	}

	struct mq_attr actual;

	int result = mq_getattr(handle, &actual);

	if (result != 0) {
		errno_t what = errno;

		warnc(what, "mq_attr(send)");
		mq_close(handle);
		return (what);
	}

	int size = strlen(text);

	if (size > actual.mq_msgsize) {
		warnx("truncating message to %ld characters.\n", actual.mq_msgsize);
		size = actual.mq_msgsize;
	}

	result = mq_send(handle, text, size, q_priority);

	if (result != 0) {
		errno_t what = errno;

		warnc(what, "mq_send");
		mq_close(handle);
		return (what);
	}

	return (mq_close(handle));
}

static void
usage(FILE *file)
{
	fprintf(file,
	    "usage:\n\tposixmqcontrol [rm|info|recv] -q <queue>\n"
	    "\tposixmqcontrol create -q <queue> -s <maxsize> -d <maxdepth> "
	    "[ -m <mode> ] [ -b <block> ] [-u <uid> ] [ -g <gid> ]\n"
	    "\tposixmqcontrol send -q <queue> -c <content> "
	    "[-p <priority> ]\n");
}

/* end of SUBCOMMANDS */

#define _countof(arg) ((sizeof(arg)) / (sizeof((arg)[0])))

/* convert an errno style error code to a sysexits code. */
static int
grace(int err_number)
{
	static const int xlat[][2] = {
		/* generally means the mqueuefs driver is not loaded. */
		{ENOSYS, EX_UNAVAILABLE},
		/* no such queue name. */
		{ENOENT, EX_OSFILE},
		{EIO, EX_IOERR},
		{ENODEV, EX_IOERR},
		{ENOTSUP, EX_TEMPFAIL},
		{EAGAIN, EX_IOERR},
		{EPERM, EX_NOPERM},
		{EACCES, EX_NOPERM},
		{0, EX_OK}
	};

	for (unsigned i = 0; i < _countof(xlat); i++) {
		if (xlat[i][0] == err_number)
			return (xlat[i][1]);
	}

	return (EX_OSERR);
}

/* OPTIONS tables */

/* careful: these 'names' arrays must be terminated by a null pointer. */
static const char *names_queue[] = {"-q", "--queue", "-t", "--topic", NULL};
static const struct Option option_queue = {
	.pattern = names_queue,
	.parse = parse_queue,
	.validate = validate_queue};
static const struct Option option_single_queue = {
	.pattern = names_queue,
	.parse = parse_single_queue,
	.validate = validate_single_queue};
static const char *names_depth[] = {"-d", "--depth", "--maxmsg", NULL};
static const struct Option option_depth = {
	.pattern = names_depth,
	.parse = parse_depth,
	.validate = validate_always_true};
static const char *names_size[] = {"-s", "--size", "--msgsize", NULL};
static const struct Option option_size = {
	.pattern = names_size,
	.parse = parse_size,
	.validate = validate_always_true};
static const char *names_block[] = {"-b", "--block", NULL};
static const struct Option option_block = {
	.pattern = names_block,
	.parse = parse_block,
	.validate = validate_always_true};
static const char *names_content[] = {
	"-c", "--content", "--data", "--message", NULL};
static const struct Option option_content = {
	.pattern = names_content,
	.parse = parse_content,
	.validate = validate_content};
static const char *names_priority[] = {"-p", "--priority", NULL};
static const struct Option option_priority = {
	.pattern = names_priority,
	.parse = parse_priority,
	.validate = validate_always_true};
static const char *names_mode[] = {"-m", "--mode", NULL};
static const struct Option option_mode = {
	.pattern = names_mode,
	.parse = parse_mode,
	.validate = validate_always_true};
static const char *names_group[] = {"-g", "--gid", NULL};
static const struct Option option_group = {
	.pattern = names_group,
	.parse = parse_group,
	.validate = validate_always_true};
static const char *names_user[] = {"-u", "--uid", NULL};
static const struct Option option_user = {
	.pattern = names_user,
	.parse = parse_user,
	.validate = validate_always_true};

/* careful: these arrays must be terminated by a null pointer. */
#ifdef __FreeBSD__
static const struct Option *create_options[] = {
	&option_queue, &option_depth, &option_size, &option_block,
	&option_mode, &option_group, &option_user, NULL};
#else  /* !__FreeBSD__ */
static const struct Option *create_options[] = {
	&option_queue, &option_depth, &option_size, &option_block,
	&option_mode, NULL};
#endif /* __FreeBSD__ */
static const struct Option *info_options[] = {&option_queue, NULL};
static const struct Option *unlink_options[] = {&option_queue, NULL};
static const struct Option *recv_options[] = {&option_single_queue, NULL};
static const struct Option *send_options[] = {
	&option_queue, &option_content, &option_priority, NULL};

int
main(int argc, const char *argv[])
{
	STAILQ_INIT(&queues);
	STAILQ_INIT(&contents);

	if (argc > 1) {
		const char *verb = argv[1];
		int index = 2;

		if (strcmp("create", verb) == 0 || strcmp("attr", verb) == 0) {
			parse_options(index, argc, argv, create_options);
			if (validate_options(create_options)) {
				int worst = 0;
				struct element *itq;

				STAILQ_FOREACH(itq, &queues, links) {
					const char *queue = itq->text;

					int result = create(queue, creation);
					if (result != 0)
						worst = result;
				}

				return (grace(worst));
			}

			return (EX_USAGE);
		} else if (strcmp("info", verb) == 0 || strcmp("cat", verb) == 0) {
			parse_options(index, argc, argv, info_options);
			if (validate_options(info_options)) {
				int worst = 0;
				struct element *itq;

				STAILQ_FOREACH(itq, &queues, links) {
					const char *queue = itq->text;
					int result = info(queue);

					if (result != 0)
						worst = result;
				}

				return (grace(worst));
			}

			return (EX_USAGE);
		} else if (strcmp("send", verb) == 0) {
			parse_options(index, argc, argv, send_options);
			if (validate_options(send_options)) {
				int worst = 0;
				struct element *itq;

				STAILQ_FOREACH(itq, &queues, links) {
					const char *queue = itq->text;
					struct element *itc;

					STAILQ_FOREACH(itc, &contents, links) {
						const char *content = itc->text;
						int result = send(queue, content, priority);

						if (result != 0)
							worst = result;
					}
				}

				return (grace(worst));
			}
			return (EX_USAGE);
		} else if (strcmp("recv", verb) == 0 ||
		    strcmp("receive", verb) == 0) {
			parse_options(index, argc, argv, recv_options);
			if (validate_options(recv_options)) {
				const char *queue = STAILQ_FIRST(&queues)->text;
				int worst = recv(queue);

				return (grace(worst));
			}

			return (EX_USAGE);
		} else if (strcmp("unlink", verb) == 0 ||
		    strcmp("rm", verb) == 0) {
			parse_options(index, argc, argv, unlink_options);
			if (validate_options(unlink_options)) {
				int worst = 0;
				struct element *itq;

				STAILQ_FOREACH(itq, &queues, links) {
					const char *queue = itq->text;
					int result = rm(queue);

					if (result != 0)
						worst = result;
				}

				return (grace(worst));
			}

			return (EX_USAGE);
		} else if (strcmp("help", verb) == 0) {
			usage(stdout);
			return (EX_OK);
		} else {
			warnx("Unknown verb [%s]", verb);
			return (EX_USAGE);
		}
	}

	usage(stdout);
	return (EX_OK);
}
