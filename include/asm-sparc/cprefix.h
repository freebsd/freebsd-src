/* cprefix.h:  This file is included by assembly source which needs
 *             to know what the c-label prefixes are. The newer versions
 *	       of cpp that come with gcc predefine such things to help
 *	       us out. The reason this stuff is needed is to make
 *	       solaris compiles of the kernel work.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */
#ifndef __SPARC_CPREFIX_H
#define __SPARC_CPREFIX_H

#if defined(__svr4__) || defined(__ELF__)
#define C_LABEL_PREFIX
#define C_LABEL_STR(name) #name
#else
#define C_LABEL_PREFIX _
#define C_LABEL_STR(name) "_" #name
#endif

#define CONCAT(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a##b

#define C_LABEL(name) CONCAT(C_LABEL_PREFIX, name)

#endif /* !(__SPARC_CPREFIX_H) */
