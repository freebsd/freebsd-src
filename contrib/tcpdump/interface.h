/*
 * Copyright (c) 1988-2002
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef tcpdump_interface_h
#define tcpdump_interface_h

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#include "funcattrs.h"

#include <stdarg.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#ifndef HAVE_STRLCAT
extern size_t strlcat(char *, const char *, size_t);
#endif
#ifndef HAVE_STRLCPY
extern size_t strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRDUP
extern char *strdup(const char *);
#endif

#ifndef HAVE_STRSEP
extern char *strsep(char **, const char *);
#endif

#endif

extern char *program_name;	/* used to generate self-identifying messages */

#include <pcap.h>

#ifndef HAVE_BPF_DUMP
struct bpf_program;
#endif

/*
 * With Capsicum bpf_dump() may be not declared even if HAVE_BPF_DUMP is set.
 */
#if !defined(HAVE_BPF_DUMP) || \
    (defined(HAVE_BPF_DUMP) && HAVE_CAPSICUM && !defined(bpf_dump))
extern void bpf_dump(const struct bpf_program *, int);

#endif
