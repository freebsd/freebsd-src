
/* $Id: sunkbdmap.c,v 1.1 1997/09/07 15:40:27 ecd Exp $
 * sunkbdmap.c: Wrapper around sunkeymap.c to change table names.
 *
 * Copyright (C) 1997  Eddie C. Dost  (ecd@skynet.be)
 */

#include <linux/config.h>

#ifdef CONFIG_PCI

#define func_buf			sun_func_buf
#define func_table			sun_func_table
#define funcbufsize			sun_funcbufsize
#define funcbufleft			sun_funcbufleft
#define funcbufptr			sun_funcbufptr
#define accent_table			sun_accent_table
#define accent_table_size		sun_accent_table_size

#define key_maps			sun_key_maps
#define keymap_count			sun_keymap_count

#define plain_map			sun_plain_map
#define shift_map			sun_shift_map
#define ctrl_map			sun_ctrl_map
#define alt_map				sun_alt_map
#define altgr_map			sun_altgr_map
#define shift_ctrl_map			sun_shift_ctrl_map
#define ctrl_alt_map			sun_ctrl_alt_map

#endif /* CONFIG_PCI */

#include "sunkeymap.c"
