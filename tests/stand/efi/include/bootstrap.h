/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

struct file_metadata {
	size_t md_size;
	uint16_t md_type;
	vm_offset_t md_addr;
	struct file_metadata *md_next;
	char md_data[1];
};

struct preloaded_file {
	char *f_name, *f_type, *f_args;
	vm_offset_t f_addr;
	struct file_metadata *f_metadata;
	struct preloaded_file *f_next;
};

struct env_var {
	const char *ev_name, *ev_value;
	int ev_flags;
	struct env_var *ev_next;
};

#define EV_NOKENV 1
#define environ test_environment
extern struct env_var *environ;

struct preloaded_file *file_findfile(const char *, const char *);
struct file_metadata *file_findmetadata(struct preloaded_file *, int);
