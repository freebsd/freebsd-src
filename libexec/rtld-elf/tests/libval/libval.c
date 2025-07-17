/*-
 *
 * Copyright (C) 2023 NetApp, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

static int val;

int get_value(void);
void set_value(int);

int
get_value(void)
{

	return (val);
}

void
set_value(int nval)
{

	val = nval;
}
