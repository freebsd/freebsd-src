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
 * User configuration and display program
 * --------------------------------------
 *
 * Control blocks
 *
 */

#define	MAX_NIFS	256		/* Max network interfaces */
#define	MIN_VCI		32		/* Smallest non-reserved VCI */

#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif

 
/*
 * User commands
 */
struct cmd {
	char	*name;			/* Command name */
	int	minp;			/* Minimum number of parameters */
	int	maxp;			/* Maximum number of parameters */
	void	(*func)			/* Processing function */
			__P((int, char **, struct cmd *));
	char	*help;			/* User help string */
};


/*
 * Supported signalling protocols
 */
struct proto {
	char	*p_name;		/* Protocol name */
	u_char	p_id;			/* Protocol id */ 
};


/*
 * Table of state names
 */
struct state {
	char	*s_name;		/* State name */
	u_char	s_id;			/* State id */ 
};


/*
 * Supported signalling protocol states
 */
struct proto_state {
	char 		*p_name;	/* Signalling manager name */
	struct state	*p_state;	/* Protocol state table */
	struct state	*v_state;	/* Protocol VCC state table */
	u_char		p_id;		/* Protocol ID */ 
};


/*
 * Supported VCC owners
 */
struct owner {
	char	*o_name;		/* Owner name */
	u_int	o_sap;			/* Owner's SAP */
	void	(*o_pvcadd)		/* PVC ADD processing function */
			__P((int, char **, struct cmd *, struct atmaddreq *,
			     struct air_int_rsp *));
};


/*
 * Supported AALs
 */
struct aal {
	char	*a_name;		/* AAL name */
	u_char	a_id;			/* AAL code */ 
};


/*
 * Supported encapsulations
 */
struct encaps {
	char	*e_name;		/* Encapsulation name */
	u_char	e_id;			/* Encapsulation code */ 
};


/*
 * External variables
 */
extern char	*prog;			/* Program invocation */
extern char	prefix[];		/* Current command prefix */

/*
 * Global function declarations
 */
	/* atm.c */
int		do_cmd __P((struct cmd *, int, char **));
void		usage __P((struct cmd *, char *));
void		attach __P((int, char **, struct cmd *));
void		detach __P((int, char **, struct cmd *));
void		pvc_add __P((int, char **, struct cmd *));
void		arp_add __P((int, char **, struct cmd *));
void		pvc_dlt __P((int, char **, struct cmd *));
void		svc_dlt __P((int, char **, struct cmd *));
void		vcc_dlt __P((int, char **, struct cmd *, struct atmdelreq *));
void		arp_dlt __P((int, char **, struct cmd *));
void		help __P((int, char **, struct cmd *));

	/* atm_eni.c */
void		show_eni_stats __P((char *, int, char **));
void		print_eni_oc3 __P((struct air_vinfo_rsp *));
void		print_eni_atm __P((struct air_vinfo_rsp *));
void		print_eni_aal0 __P((struct air_vinfo_rsp *));
void		print_eni_aal5 __P((struct air_vinfo_rsp *));
void		print_eni_driver __P((struct air_vinfo_rsp *));

	/* atm_fore200.c */
void		show_fore200_stats __P((char *, int, char **));
void		print_fore200_taxi __P((struct air_vinfo_rsp *));
void		print_fore200_oc3 __P((struct air_vinfo_rsp *));
void		print_fore200_dev __P((struct air_vinfo_rsp *));
void		print_fore200_atm __P((struct air_vinfo_rsp *));
void		print_fore200_aal0 __P((struct air_vinfo_rsp *));
void		print_fore200_aal4 __P((struct air_vinfo_rsp *));
void		print_fore200_aal5 __P((struct air_vinfo_rsp *));
void		print_fore200_driver __P((struct air_vinfo_rsp *));

	/* atm_inet.c */
void		ip_pvcadd __P((int, char **, struct cmd *, struct atmaddreq *,
			struct air_int_rsp *));

	/* atm_print.c */
void		print_arp_info __P((struct air_arp_rsp *));
void		print_asrv_info __P((struct air_asrv_rsp *));
void		print_cfg_info __P((struct air_cfg_rsp *));
void		print_intf_info __P((struct air_int_rsp *));
void		print_ip_vcc_info __P((struct air_ip_vcc_rsp *));
void		print_netif_info __P((struct air_netif_rsp *));
void		print_intf_stats __P((struct air_phy_stat_rsp *));
void		print_vcc_stats __P((struct air_vcc_rsp *));
void		print_vcc_info __P((struct air_vcc_rsp *));
void		print_version_info __P((struct air_version_rsp *));

	/* atm_set.c */
void		set_arpserver __P((int, char **, struct cmd *));
void		set_macaddr __P((int, char **, struct cmd *));
void		set_netif __P((int, char **, struct cmd *));
void		set_prefix __P((int, char **, struct cmd *));

	/* atm_show.c */
void		show_arp __P((int, char **, struct cmd *));
void		show_arpserv __P((int, char **, struct cmd *));
void		show_config __P((int, char **, struct cmd *));
void		show_intf __P((int, char **, struct cmd *));
void		show_ip_vcc __P((int, char **, struct cmd *));
void		show_netif __P((int, char **, struct cmd *));
void		show_intf_stats __P((int, char **, struct cmd *));
void		show_vcc_stats __P((int, char **, struct cmd *));
void		show_vcc __P((int, char **, struct cmd *));
void		show_version __P((int, char **, struct cmd *));

	/* atm_subr.c */
char *		get_vendor __P((int));
char *		get_adapter __P((int));
char *		get_media_type __P((int));
char *		get_bus_type __P((int));
char *		get_bus_slot_info __P((int, u_long));
char *		get_adapter_name __P((char *));
int		do_info_ioctl __P((struct atminfreq *, int));
int		get_vcc_info __P((char *, struct air_vcc_rsp **));
int		verify_nif_name __P((char *));
struct sockaddr_in *
		get_ip_addr __P((char *));
int		get_hex_addr __P((char *, u_char *, int));
char *		format_mac_addr __P((Mac_addr *));
int		parse_ip_prefix __P((char *, struct in_addr *));
int		compress_prefix_list __P((struct in_addr *, int));
void		check_netif_name __P((char *));
void		sock_error __P((int));
