/*
 * $FreeBSD$
 */
extern void setmodulus __P((char *modx));

extern keystatus pk_setkey __P(( uid_t, keybuf ));
extern keystatus pk_encrypt __P(( uid_t, char *, netobj *, des_block * ));
extern keystatus pk_decrypt __P(( uid_t, char *, netobj *, des_block * ));
extern keystatus pk_netput __P(( uid_t, key_netstarg * ));
extern keystatus pk_netget __P(( uid_t, key_netstarg * ));
extern keystatus pk_get_conv_key __P(( uid_t, keybuf, cryptkeyres * ));
extern void pk_nodefaultkeys __P(( void ));

extern void crypt_prog_1 __P(( struct svc_req *, register SVCXPRT * ));
extern void load_des __P(( int, char * ));

extern int (*_my_crypt)__P(( char *, int, struct desparams * ));
