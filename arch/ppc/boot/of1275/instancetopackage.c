/*
 * Copyright (C) Leigh Brown 2002.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include "of1275.h"

phandle
instancetopackage(const ihandle instance)
{
    struct prom_args {
	char *service;
	int nargs;
	int nret;
	ihandle instance;
	phandle package;
    } args;

    args.service = "instance-to-package";
    args.nargs = 1;
    args.nret = 1;
    args.instance = instance;
    (*of_prom_entry)(&args);
    return args.package;
}
