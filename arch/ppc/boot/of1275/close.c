/*
 * Copyright (C) Leigh Brown 2002.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include "of1275.h"

void
close(ihandle instance)
{
    struct prom_args {
	char *service;
	int nargs;
	int nret;
	ihandle instance;
    } args;

    args.service = "close";
    args.nargs = 1;
    args.nret = 0;
    args.instance = instance;
    (*of_prom_entry)(&args);
}
