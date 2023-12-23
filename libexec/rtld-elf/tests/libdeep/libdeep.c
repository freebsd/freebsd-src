/*-
 *
 * Copyright (C) 2023 NetApp, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 */

#include <stdio.h>

int get_value(void);
int proxy_get_value(void);
void set_value(int);
void proxy_set_value(int);

int
proxy_get_value(void)
{

	return (get_value());
}

void
proxy_set_value(int val)
{

	return (set_value(val));
}
