
#ifdef _NETINET_IF_ETHER_H_
extern timeout_t	aarpprobe;
extern int	aarpresolve	__P((struct arpcom *,
					struct mbuf *,
					struct sockaddr_at *,
					u_char *));
extern void	aarpinput	__P(( struct arpcom *, struct mbuf *));
extern int	at_broadcast	__P((struct sockaddr_at  *));
#endif

#ifdef _NETATALK_AARP_H_
extern void	aarptfree	__P((struct aarptab *));
#endif

extern void	aarp_clean	__P((void));
extern int	at_control	__P(( struct socket *so,
					int cmd,
					caddr_t data,
					struct ifnet *ifp,
					struct proc *p ));
extern u_short	at_cksum	__P(( struct mbuf *m, int skip));
extern void	ddp_init	__P((void ));
extern struct ifaddr *at_ifawithnet	__P((struct sockaddr_at *,
						struct ifaddrhead *));
#ifdef	_NETATALK_DDP_VAR_H_
extern int	ddp_output	__P((struct mbuf *m, struct socket *so)); 

#endif
#if	defined (_NETATALK_DDP_VAR_H_) && defined(_NETATALK_AT_VAR_H_)
extern struct ddpcb  *ddp_search __P((struct sockaddr_at *,
                                		struct sockaddr_at *,
						struct at_ifaddr *));
#endif
#ifdef _NET_ROUTE_H_
int     ddp_route( struct mbuf *m, struct route *ro);
#endif


