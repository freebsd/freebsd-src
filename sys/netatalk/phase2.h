/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 */

# if defined( ultrix ) || defined( BSD4_4 )
#include <net/if_llc.h>
#  if defined( __FreeBSD__ ) 
#define llc_org_code llc_un.type_snap.org_code
#define llc_ether_type llc_un.type_snap.ether_type
#  endif __FreeBSD__
# else ultrix BSD4_4

#ifdef sun
#include <net/if_ieee802.h>
#endif sun

/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 *      @(#)if_llc.h	7.2 (Berkeley) 6/28/90
 */

/*
 * IEEE 802.2 Link Level Control headers, for use in conjunction with
 * 802.{3,4,5} media access control methods.
 *
 * Headers here do not use bit fields due to shortcommings in many
 * compilers.
 */

struct llc {
	u_char	llc_dsap;
	u_char	llc_ssap;
	union {
	    struct {
		u_char control;
		u_char format_id;
		u_char class;
		u_char window_x2;
	    } type_u;
	    struct {
		u_char num_snd_x2;
		u_char num_rcv_x2;
	    } type_i;
	    struct {
		u_char control;
		u_char num_rcv_x2;
	    } type_s;
	    struct {
		u_char control;
		u_char org_code[3];
		u_short ether_type;
	    } type_snap;
	} llc_un;
};
#define llc_control llc_un.type_u.control
#define llc_fid llc_un.type_u.format_id
#define llc_class llc_un.type_u.class
#define llc_window llc_un.type_u.window_x2
#define llc_org_code llc_un.type_snap.org_code
#define llc_ether_type llc_un.type_snap.ether_type

#define LLC_UI		0x3
#define LLC_UI_P	0x13
#define LLC_XID		0xaf
#define LLC_XID_P	0xbf
#define LLC_TEST	0xe3
#define LLC_TEST_P	0xf3

#define LLC_ISO_LSAP	0xfe
#define LLC_SNAP_LSAP	0xaa

# endif ultrix BSD4_4

#if defined( sun ) || defined( ibm032 )
#define SIOCPHASE1	_IOW(i, 100, struct ifreq)	/* AppleTalk phase 1 */
#define SIOCPHASE2	_IOW(i, 101, struct ifreq)	/* AppleTalk phase 2 */
#endif sun ibm032

#if defined( ultrix ) || defined( BSD4_4 ) || defined( _IBMR2 )
#define SIOCPHASE1	_IOW('i', 100, struct ifreq)	/* AppleTalk phase 1 */
#define SIOCPHASE2	_IOW('i', 101, struct ifreq)	/* AppleTalk phase 2 */
#endif ultrix BSD4_4 _IBMR2
