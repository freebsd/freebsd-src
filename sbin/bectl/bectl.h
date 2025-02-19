/*
 * Copyright (c) 2018 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

int usage(bool explicit);

int bectl_cmd_jail(int argc, char *argv[]);
int bectl_cmd_unjail(int argc, char *argv[]);

int bectl_cmd_list(int argc, char *argv[]);

extern libbe_handle_t *be;
