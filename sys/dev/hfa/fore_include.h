/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * FORE Systems 200-Series Adapter Support
 * ---------------------------------------
 *
 * Local driver include files and global declarations
 *
 */

#ifndef _FORE_INCLUDE_H
#define _FORE_INCLUDE_H

#include <netatm/kern_include.h>
#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

/*
 * If not specified elsewhere, guess which type of bus support we want
 */
#if !(defined(FORE_PCI) || defined(FORE_SBUS))
#if defined(sparc)
#define	FORE_SBUS
#elif defined(__i386__)
#define	FORE_PCI
#endif
#endif

#ifdef FORE_PCI
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#endif

#include <dev/hfa/fore.h>
#include <dev/hfa/fore_aali.h>
#include <dev/hfa/fore_slave.h>
#include <dev/hfa/fore_stats.h>
#include <dev/hfa/fore_var.h>

/*
 * Global function declarations
 */
	/* fore_buffer.c */
int		fore_buf_allocate __P((Fore_unit *));
void		fore_buf_initialize __P((Fore_unit *));
void		fore_buf_supply __P((Fore_unit *));
void		fore_buf_free __P((Fore_unit *));

	/* fore_command.c */
int		fore_cmd_allocate __P((Fore_unit *));
void		fore_cmd_initialize __P((Fore_unit *));
void		fore_cmd_drain __P((Fore_unit *));
void		fore_cmd_free __P((Fore_unit *));

	/* fore_if.c */
int		fore_atm_ioctl __P((int, caddr_t, caddr_t));
void		fore_interface_free __P((Fore_unit *));

	/* fore_init.c */
void		fore_initialize __P((Fore_unit *));
void		fore_initialize_complete __P((Fore_unit *));

	/* fore_intr.c */
#if defined(sun)
int		fore_poll __P((void));
#endif
#if (defined(BSD) && (BSD <= 199306))
int		fore_intr __P((void *));
#else
void		fore_intr __P((void *));
#endif
void		fore_watchdog __P((Fore_unit *));

	/* fore_load.c */

	/* fore_output.c */
void		fore_output __P((Cmn_unit *, Cmn_vcc *, KBuffer *));

	/* fore_receive.c */
int		fore_recv_allocate __P((Fore_unit *));
void		fore_recv_initialize __P((Fore_unit *));
void		fore_recv_drain __P((Fore_unit *));
void		fore_recv_free __P((Fore_unit *));

	/* fore_stats.c */
int		fore_get_stats __P((Fore_unit *));

	/* fore_timer.c */
void		fore_timeout __P((struct atm_time *));

	/* fore_transmit.c */
int		fore_xmit_allocate __P((Fore_unit *));
void		fore_xmit_initialize __P((Fore_unit *));
void		fore_xmit_drain __P((Fore_unit *));
void		fore_xmit_free __P((Fore_unit *));

	/* fore_vcm.c */
int		fore_instvcc __P((Cmn_unit *, Cmn_vcc *));
int		fore_openvcc __P((Cmn_unit *, Cmn_vcc *));
int		fore_closevcc __P((Cmn_unit *, Cmn_vcc *));


/*
 * Global variable declarations
 */
extern Fore_device	fore_devices[];
extern Fore_unit	*fore_units[];
extern int		fore_nunits;
extern struct stack_defn	*fore_services;
extern struct sp_info	fore_nif_pool;
extern struct sp_info	fore_vcc_pool;
extern struct atm_time	fore_timer;

#endif	/* _FORE_INCLUDE_H */
