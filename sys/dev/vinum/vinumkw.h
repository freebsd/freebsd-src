/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumkw.h,v 1.16 2000/02/19 06:52:46 grog Exp grog $
 * $FreeBSD$
 */

/*
 * Command keywords that vinum knows.  These include both user-level
 * and kernel-level stuff
 */

/*
 * Our complete vocabulary.  The names of the commands are
 * the same as the identifier without the kw_ at the beginning
 * (i.e. kw_create defines the "create" keyword).  Preprocessor
 * magic in parser.c does the rest.
 *
 * To add a new word: put it in the table below and one of the
 * lists in vinumparser.c (probably keywords).
 */
enum keyword {
    kw_create,
    kw_modify,
    kw_list,
    kw_l = kw_list,
    kw_ld,						    /* list drive */
    kw_ls,						    /* list subdisk */
    kw_lp,						    /* list plex */
    kw_lv,						    /* list volume */
    kw_set,
    kw_rm,
    kw_mv,						    /* move object */
    kw_move,						    /* synonym for mv */
    kw_start,
    kw_stop,
    kw_makedev,						    /* make /dev/vinum devices */
    kw_setdaemon,					    /* set daemon flags */
    kw_getdaemon,					    /* set daemon flags */
    kw_help,
    kw_drive,
    kw_partition,
    kw_sd,
    kw_subdisk = kw_sd,
    kw_plex,
    kw_volume,
    kw_vol = kw_volume,
    kw_read,
    kw_readpol,
    kw_org,
    kw_name,
    kw_concat,
    kw_striped,
    kw_raid4,
    kw_raid5,
    kw_driveoffset,
    kw_plexoffset,
    kw_len,
    kw_length = kw_len,
    kw_size = kw_len,
    kw_state,
    kw_setupstate,
    kw_d,						    /* flag names */
    kw_f,
    kw_r,
    kw_s,
    kw_v,
    kw_w,
    kw_round,						    /* round robin */
    kw_prefer,						    /* prefer plex */
    kw_device,
    kw_init,
    kw_label,
    kw_resetconfig,
    kw_writethrough,
    kw_writeback,
    kw_raw,
    kw_replace,
    kw_resetstats,
    kw_attach,
    kw_detach,
    kw_rename,
    kw_printconfig,
    kw_saveconfig,
    kw_hotspare,
    kw_detached,
#ifdef VINUMDEBUG
    kw_debug,						    /* go into debugger */
#endif
    kw_stripe,
    kw_mirror,
    kw_info,
    kw_quit,
    kw_max,
    kw_setstate,
    kw_checkparity,
    kw_rebuildparity,
    kw_dumpconfig,
    kw_retryerrors,
    kw_invalid_keyword = -1
};

struct _keywords {
    char *name;
    enum keyword keyword;
};

struct keywordset {
    int size;
    struct _keywords *k;
};

extern struct _keywords keywords[];
extern struct _keywords flag_keywords[];

extern struct keywordset keyword_set;
extern struct keywordset flag_set;
