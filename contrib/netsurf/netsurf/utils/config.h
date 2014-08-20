/*
 * Copyright 2003-7 John M Bell <jmb202@ecs.soton.ac.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NETSURF_UTILS_CONFIG_H_
#define _NETSURF_UTILS_CONFIG_H_

#include <stddef.h>
#include <dirent.h>

/* Try to detect which features the target OS supports */

#if (defined(_GNU_SOURCE) && !defined(__APPLE__) || defined(__HAIKU__))
#define HAVE_STRNDUP
#else
#undef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

#if (defined(_GNU_SOURCE) || defined(__APPLE__) || defined(__HAIKU__))
#define HAVE_STRCASESTR
#else
#undef HAVE_STRCASESTR
char *strcasestr(const char *haystack, const char *needle);
#endif

/* For some reason, UnixLib defines this unconditionally. 
 * Assume we're using UnixLib if building for RISC OS. */
#if ((defined(_GNU_SOURCE) && !defined(__APPLE__)) || defined(riscos))
#define HAVE_STRCHRNUL
#else
#undef HAVE_STRCHRNUL
char *strchrnul(const char *s, int c);
#endif

#define HAVE_INETATON
#if (defined(_WIN32))
#undef HAVE_INETATON
#include <winsock2.h>
#define EAFNOSUPPORT WSAEAFNOSUPPORT
int inet_aton(const char *cp, struct in_addr *inp);
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define HAVE_INETPTON
#if (defined(_WIN32))
#undef HAVE_INETPTON
int inet_pton(int af, const char *src, void *dst);
#endif

#define HAVE_UTSNAME
#if (defined(_WIN32))
#undef HAVE_UTSNAME
#endif

#define HAVE_REALPATH
#if (defined(_WIN32))
#undef HAVE_REALPATH
char *realpath(const char *path, char *resolved_path);
#endif

#define HAVE_MKDIR
#if (defined(_WIN32))
#undef HAVE_MKDIR
#endif

#define HAVE_SIGPIPE
#if (defined(_WIN32))
#undef HAVE_SIGPIPE
#endif

#define HAVE_STDOUT
#if (defined(_WIN32))
#undef HAVE_STDOUT
#endif

#define HAVE_MMAP
#if (defined(_WIN32) || defined(riscos) || defined(__HAIKU__) || defined(__BEOS__) || defined(__amigaos4__) || defined(__AMIGA__) || defined(__MINT__))
#undef HAVE_MMAP
#endif

#define HAVE_SCANDIR
#if (defined(_WIN32))
#undef HAVE_SCANDIR
int alphasort(const struct dirent **d1, const struct dirent **d2);
int scandir(const char *dir, struct dirent ***namelist,
		int (*sel)(const struct dirent *),
		int (*compar)(const struct dirent **, const struct dirent **));
#endif

/* This section toggles build options on and off.
 * Simply undefine a symbol to turn the relevant feature off.
 *
 * IF ADDING A FEATURE HERE, ADD IT TO Docs/Doxyfile's "PREDEFINED" DEFINITION AS WELL.
 */

/* Platform specific features */
#if defined(riscos)
    /* Theme auto-install */
    #define WITH_THEME_INSTALL
#elif defined(__HAIKU__) || defined(__BEOS__)
    /* for intptr_t */
    #include <inttypes.h>
    #if defined(__HAIKU__)
        /*not yet: #define WITH_MMAP*/
    #endif
    #if defined(__BEOS__)
    	/* Not even BONE has it. */
    	#define NO_IPV6 1
    #endif
#else
    /* We're likely to have a working mmap() */
    #define WITH_MMAP
#endif

/* gtk */
#if defined(gtk)
	#define WITH_THEME_INSTALL
#endif

/* amiga */
#if defined(__amigaos4__) || defined(__AMIGA__) || \
		defined(nsatari)
	#define NO_IPV6
#endif

#endif
