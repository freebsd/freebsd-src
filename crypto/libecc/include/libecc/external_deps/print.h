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
#ifndef __PRINT_H__
#define __PRINT_H__

/* Explicitly include the inttypes here */
#ifdef WITH_STDLIB
/* We include inttypes.h for the PRI* macros */
#include <inttypes.h>
#endif

#if (__GNUC__ * 10 + __GNUC_MINOR__ >= 42)
#define LIBECC_FORMAT_FUNCTION(F,A) __attribute__((format(__printf__, F, A)))
#else
#define LIBECC_FORMAT_FUNCTION(F,A)
#endif

LIBECC_FORMAT_FUNCTION(1, 2)
void ext_printf(const char *format, ...);

#endif /* __PRINT_H__ */
