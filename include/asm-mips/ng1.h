/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SGI/Newport video card ioctl definitions
 */
#ifndef _ASM_NG1_H
#define _ASM_NG1_H

typedef struct {
        int flags;
        __u16 w, h;
        __u16 fields_sec;
} ng1_vof_info_t;

struct ng1_info {
	struct gfx_info gfx_info;
	__u8 boardrev;
        __u8 rex3rev;
        __u8 vc2rev;
        __u8 monitortype;
        __u8 videoinstalled;
        __u8 mcrev;
        __u8 bitplanes;
        __u8 xmap9rev;
        __u8 cmaprev;
        ng1_vof_info_t ng1_vof_info;
        __u8 bt445rev;
        __u8 paneltype;
};

#define GFX_NAME_NEWPORT "NG1"

/* ioctls */
#define NG1_SET_CURSOR_HOTSPOT 21001
struct ng1_set_cursor_hotspot {
	unsigned short xhot;
        unsigned short yhot;
};

#define NG1_SETDISPLAYMODE     21006
struct ng1_setdisplaymode_args {
        int wid;
        unsigned int mode;
};

#define NG1_SETGAMMARAMP0      21007
struct ng1_setgammaramp_args {
        unsigned char red   [256];
        unsigned char green [256];
        unsigned char blue  [256];
};

#endif /* _ASM_NG1_H */
