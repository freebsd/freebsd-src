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
 *	@(#) $FreeBSD: src/sys/netatm/atm_var.h,v 1.2 1999/08/28 00:48:39 peter Exp $
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM system variables
 *
 */

#ifndef _NETATM_ATM_VAR_H
#define _NETATM_ATM_VAR_H


#ifdef ATM_KERNEL
/*
 * Global variable declarations
 */
	/* atm_aal5.c */
#if (defined(__FreeBSD__) && (BSD >= 199506))
extern struct pr_usrreqs	atm_aal5_usrreqs;
#endif

	/* atm_proto.c */
extern struct domain	atmdomain;

	/* atm_subr.c */
extern struct atm_pif	*atm_interface_head;
extern struct atm_ncm	*atm_netconv_head;
extern Atm_endpoint	*atm_endpoints[];
extern struct sp_info	*atm_pool_head;
extern struct stackq_entry	*atm_stackq_head;
extern struct stackq_entry	*atm_stackq_tail;
extern struct ifqueue	atm_intrq;
#ifdef sgi
extern  int		atm_intr_index;
#endif
extern struct atm_sock_stat	atm_sock_stat;
extern int		atm_init;
extern int		atm_version;
extern int		atm_debug;
extern struct timeval	atm_debugtime;
extern int		atm_dev_print;
extern int		atm_print_data;
extern struct sp_info	atm_attributes_pool;

	/* atm_usrreq.c */
#if (defined(__FreeBSD__) && (BSD >= 199506))
extern struct pr_usrreqs	atm_dgram_usrreqs;
#endif


/*
 * Global function declarations
 */
	/* atm_aal5.c */
int		atm_aal5_ctloutput __P((struct socket *, struct sockopt *));
void		atm_aal5_init __P((void));

	/* atm_cm.c */
int		atm_cm_connect __P((Atm_endpoint *, void *, Atm_attributes *,
			Atm_connection **));
int		atm_cm_listen __P((Atm_endpoint *, void *, Atm_attributes *,
			Atm_connection **));
int		atm_cm_addllc __P((Atm_endpoint *, void *, struct attr_llc *,
			Atm_connection *, Atm_connection **));
int		atm_cm_addparty __P((Atm_connection *, int,
			struct t_atm_sap *));
int		atm_cm_dropparty __P((Atm_connection *, int,
			struct t_atm_cause *));
int		atm_cm_release __P((Atm_connection *, struct t_atm_cause *));
int		atm_cm_abort __P((Atm_connvc *, struct t_atm_cause *));
int		atm_cm_incoming __P((struct vccb *, Atm_attributes *));
void		atm_cm_connected __P((Atm_connvc *));
void		atm_cm_cleared __P((Atm_connvc *));
Atm_connection *atm_cm_match __P((Atm_attributes *, Atm_connection *));
int		atm_cm_cpcs_ctl __P((int, Atm_connection *, void *));
int		atm_cm_cpcs_data __P((Atm_connection *, KBuffer *));
int		atm_cm_saal_ctl __P((int, Atm_connection *, void *));
int		atm_cm_saal_data __P((Atm_connection *, KBuffer *));
int		atm_cm_sscop_ctl __P((int, Atm_connection *, void *, void *));
int		atm_cm_sscop_data __P((Atm_connection *, KBuffer *));
int		atm_endpoint_register __P((Atm_endpoint *));
int		atm_endpoint_deregister __P((Atm_endpoint *));

	/* atm_device.c */
int		atm_dev_inst __P((struct stack_defn **, Atm_connvc *));
void		atm_dev_lower __P((int, void *, int, int));
void *		atm_dev_alloc __P((u_int, u_int, u_int));
void		atm_dev_free __P((void *));
#if defined(sun4m)
void *		atm_dma_map __P((caddr_t, int, int));
void		atm_dma_free __P((caddr_t, int));
#endif
KBuffer *	atm_dev_compress __P((KBuffer *));
Cmn_vcc *	atm_dev_vcc_find __P((Cmn_unit *, u_int, u_int, u_int));
void		atm_dev_pdu_print __P((Cmn_unit *, Cmn_vcc *, KBuffer *,
			char *));

	/* atm_if.c */
int		atm_physif_register __P((Cmn_unit *, char *,
			struct stack_defn *));
int		atm_physif_deregister __P((Cmn_unit *));
void		atm_physif_freenifs __P((struct atm_pif *));
int		atm_netconv_register __P((struct atm_ncm *));
int		atm_netconv_deregister __P((struct atm_ncm *));
int		atm_nif_attach __P((struct atm_nif *));
void		atm_nif_detach __P((struct atm_nif *));
int		atm_nif_setaddr __P((struct atm_nif *, struct ifaddr *));
#if (defined(BSD) && (BSD >= 199103))
int		atm_ifoutput __P((struct ifnet *, KBuffer *,
			struct sockaddr *, struct rtentry *));
#else
int		atm_ifoutput __P((struct ifnet *, KBuffer *,
			struct sockaddr *));
#endif
struct atm_pif *
		atm_pifname __P((char *));
struct atm_nif *
		atm_nifname __P((char *));

	/* atm_proto.c */
#if (defined(__FreeBSD__) && (BSD >= 199506))
int		atm_proto_notsupp1 __P((struct socket *));
int		atm_proto_notsupp2 __P((struct socket *, struct sockaddr *,
			struct proc *));
int		atm_proto_notsupp3 __P((struct socket *, struct sockaddr **));
int		atm_proto_notsupp4 __P((struct socket *, int, KBuffer *, 
			struct sockaddr *, KBuffer *, struct proc *));
#endif

	/* atm_signal.c */
int		atm_sigmgr_register __P((struct sigmgr *));
int		atm_sigmgr_deregister __P((struct sigmgr *));
int		atm_sigmgr_attach __P((struct atm_pif *, u_char));
int		atm_sigmgr_detach __P((struct atm_pif *));
int		atm_stack_register __P((struct stack_defn *));
int		atm_stack_deregister __P((struct stack_defn *));
int		atm_create_stack __P((Atm_connvc *, struct stack_list *,
			void (*)__P((int, void *, int, int)) ));

	/* atm_socket.c */
int		atm_sock_attach __P((struct socket *, u_long, u_long));
int		atm_sock_detach __P((struct socket *));
int		atm_sock_bind __P((struct socket *, struct sockaddr *));
int		atm_sock_listen __P((struct socket *, Atm_endpoint *));
int		atm_sock_connect __P((struct socket *, struct sockaddr *,
			Atm_endpoint *));
int		atm_sock_disconnect __P((struct socket *));
int		atm_sock_sockaddr __P((struct socket *, struct sockaddr **));
int		atm_sock_peeraddr __P((struct socket *, struct sockaddr **));
int		atm_sock_setopt __P((struct socket *, struct sockopt *,
			Atm_pcb *));
int		atm_sock_getopt __P((struct socket *, struct sockopt *,
			Atm_pcb *));
void		atm_sock_connected __P((void *));
void		atm_sock_cleared __P((void *, struct t_atm_cause *));

	/* atm_subr.c */
void		atm_initialize __P((void));
void *		atm_allocate __P((struct sp_info *));
void		atm_free __P((void *));
void		atm_release_pool __P((struct sp_info *));
void		atm_timeout __P((struct atm_time *, int, 
			void (*) __P((struct atm_time *)) ));
int		atm_untimeout __P((struct atm_time *));
int		atm_stack_enq __P((int, void (*) __P((int, void *, int, int)), 
			void *, Atm_connvc *, int, int));
void		atm_stack_drain __P((void));
void		atm_intr __P((void));
void		atm_pdu_print __P((KBuffer *, char *));

	/* atm_usrreq.c */
#if (!(defined(__FreeBSD__) && (BSD >= 199506)))
int		atm_dgram_usrreq __P((struct socket *, int, KBuffer *,
			KBuffer *, KBuffer *));
#endif

#endif	/* ATM_KERNEL */

#endif	/* _NETATM_ATM_VAR_H */
