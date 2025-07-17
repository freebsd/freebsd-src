/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/external_deps/print.h>

#ifdef WITH_STDLIB
#include <stdio.h>
#include <stdarg.h>
void ext_printf(const char *format, ...)
{
	va_list arglist;

	va_start(arglist, format);
	vprintf(format, arglist);
	va_end(arglist);
}
#else
#error "print.c: you have to implement ext_printf"
#endif
