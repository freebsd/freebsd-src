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
 *	@(#) $FreeBSD: src/sys/dev/hea/eni_var.h,v 1.2 1999/08/28 00:41:46 peter Exp $
 *
 */

/*
 * Efficient ENI Adapter Support
 * -----------------------------
 *
 * Local driver include files and global declarations
 *
 */

#ifndef	_ENI_ENI_VAR_H
#define	_ENI_ENI_VAR_H

/*
 * Global function declarations
 */
	/* eni_buffer.c */
int	eni_init_memory __P((Eni_unit *));
caddr_t	eni_allocate_buffer __P((Eni_unit *, u_long *));
void	eni_free_buffer __P((Eni_unit *, caddr_t));

	/* eni_if.c */
int	eni_atm_ioctl __P((int, caddr_t, caddr_t));
void	eni_zero_stats __P((Eni_unit *));

	/* eni_init.c */
int	eni_init __P((Eni_unit *));

	/* eni_intr.c */
#if defined(BSD) && BSD < 199506
int	eni_intr __P((void *));
#else
void	eni_intr __P((void *));
#endif

	/* eni_receive.c */
void	eni_do_service __P((Eni_unit *));
void	eni_recv_drain __P((Eni_unit *));

	/* eni_transmit.c */
int	eni_set_dma __P((Eni_unit *, int, u_long *, int, long *, int, u_long, int ));
void	eni_output __P((Cmn_unit *, Cmn_vcc *, KBuffer *));
void	eni_xmit_drain __P((Eni_unit *));

	/* eni_vcm.c */
int	eni_instvcc __P((Cmn_unit *, Cmn_vcc *));
int	eni_openvcc __P((Cmn_unit *, Cmn_vcc *));
int	eni_closevcc __P((Cmn_unit *, Cmn_vcc *));

/*
 * Global variable declarations
 */
extern Eni_unit		*eni_units[];
extern struct stack_defn	*eni_services;
extern struct sp_info	eni_nif_pool;
extern struct sp_info	eni_vcc_pool;

#endif	/* _ENI_ENI_VAR_H */
