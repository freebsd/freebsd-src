#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H

/*
 * Allocate a 8 Mb temporary mapping area for copy_user_page/clear_user_page.
 * This area needs to be aligned on a 8 Mb boundary.
 */

#define TMPALIAS_MAP_START (__PAGE_OFFSET - 0x01000000)
#define FIXADDR_START   ((unsigned long)TMPALIAS_MAP_START)

#endif
