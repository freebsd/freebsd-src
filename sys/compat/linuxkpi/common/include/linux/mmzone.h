/* Public domain. */

#ifndef _LINUX_MMZONE_H
#define _LINUX_MMZONE_H

#include <linux/mm_types.h>
#include <linux/numa.h>
#include <linux/page-flags.h>

#define MAX_ORDER	11

#define	MAX_PAGE_ORDER	10
#define	NR_PAGE_ORDERS	(MAX_PAGE_ORDER + 1)

#endif
