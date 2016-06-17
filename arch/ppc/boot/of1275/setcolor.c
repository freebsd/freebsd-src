/*
 * arch/ppc/boot/of1275/setcolor.c
 *
 * Copyright (C) Leigh Brown 2002.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include "of1275.h"

int
setcolor(ihandle instance, int color, int red, int green, int blue)
{
    struct prom_args {
	char *service;
	int nargs;
	int nret;
	char *method;
	ihandle instance;
	int color;
	int blue;
	int green;
	int red;
	int result;
    } args;

    args.service = "call-method";
    args.nargs = 6;
    args.nret = 1;
    args.method = "color!";
    args.instance = instance;
    args.color = color;
    args.blue = blue;
    args.green = green;
    args.red = red;
    args.result = 0;
    (*of_prom_entry)(&args);
    return args.result;
}
