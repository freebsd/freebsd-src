#ifdef __STDC__
# define	P(s) s
#else
# define P(s) ()
#endif

/* add_ticket.c */
int add_ticket P((KTEXT , int , char *, int , char *, char *, char *, int , KTEXT ));

/* cr_err_reply.c */
void cr_err_reply P((KTEXT , char *, char *, char *, u_long , u_long , char *));

/* create_auth_reply.c */
KTEXT create_auth_reply P((char *, char *, char *, long , int , unsigned long , int , KTEXT ));

/* create_ciph.c */
int create_ciph P((KTEXT , C_Block , char *, char *, char *, unsigned long , int , KTEXT , unsigned long , C_Block ));

/* create_death_packet.c */
KTEXT krb_create_death_packet P((char *));

/* create_ticket.c */
int krb_create_ticket P((KTEXT , unsigned int , char *, char *, char *, long , char *, int , long , char *, char *, C_Block ));

/* debug_decl.c */

/* decomp_ticket.c */
int decomp_ticket P((KTEXT , unsigned char *, char *, char *, char *, unsigned long *, C_Block , int *, unsigned long *, char *, char *, C_Block , Key_schedule ));

/* dest_tkt.c */
int dest_tkt P((void ));

/* extract_ticket.c */
int extract_ticket P((KTEXT , int , char *, int *, int *, char *, KTEXT ));

/* fgetst.c */
int fgetst P((FILE *, char *, int ));

/* get_ad_tkt.c */
int get_ad_tkt P((char *, char *, char *, int ));

/* get_admhst.c */
int krb_get_admhst P((char *, char *, int ));

/* get_cred.c */
int krb_get_cred P((char *, char *, char *, CREDENTIALS *));

/* get_in_tkt.c */
int krb_get_pw_in_tkt P((char *, char *, char *, char *, char *, int , char *));
int placebo_read_password P((des_cblock *, char *, int ));
int placebo_read_pw_string P((char *, int , char *, int ));

/* get_krbhst.c */
int krb_get_krbhst P((char *, char *, int ));

/* get_krbrlm.c */
int krb_get_lrealm P((char *, int ));

/* get_phost.c */
char *krb_get_phost P((char *));

/* get_pw_tkt.c */
int get_pw_tkt P((char *, char *, char *, char *));

/* get_request.c */
int get_request P((KTEXT , int , char **, char **));

/* get_svc_in_tkt.c */
int krb_get_svc_in_tkt P((char *, char *, char *, char *, char *, int , char *));

/* get_tf_fullname.c */
int krb_get_tf_fullname P((char *, char *, char *, char *));

/* get_tf_realm.c */
int krb_get_tf_realm P((char *, char *));

/* getopt.c */
int getopt P((int , char **, char *));

/* getrealm.c */
char *krb_realmofhost P((char *));

/* getst.c */
int getst P((int , char *, int ));

/* in_tkt.c */
int in_tkt P((char *, char *));

/* k_gethostname.c */
int k_gethostname P((char *, int ));

/* klog.c */
char *klog P((int , char *, int , int , int , int , int , int , int , int , int , int ));
int kset_logfile P((char *));

/* kname_parse.c */
int kname_parse P((char *, char *, char *, char *));
int k_isname P((char *));
int k_isinst P((char *));
int k_isrealm P((char *));

/* kntoln.c */
int krb_kntoln P((AUTH_DAT *, char *));

/* krb_err_txt.c */

/* krb_get_in_tkt.c */
int krb_get_in_tkt P((char *, char *, char *, char *, char *, int , int (*key_proc )(), int (*decrypt_proc )(), char *));

/* kuserok.c */
int kuserok P((AUTH_DAT *, char *));

/* log.c */
void log P((char *, int , int , int , int , int , int , int , int , int , int ));
int set_logfile P((char *));
int new_log P((long , char *));

/* mk_err.c */
long krb_mk_err P((u_char *, long , char *));

/* mk_priv.c */
long krb_mk_priv P((u_char *, u_char *, u_long , Key_schedule , C_Block , struct sockaddr_in *, struct sockaddr_in *));

/* mk_req.c */
int krb_mk_req P((KTEXT , char *, char *, char *, long ));
int krb_set_lifetime P((int ));

/* mk_safe.c */
long krb_mk_safe P((u_char *, u_char *, u_long , C_Block *, struct sockaddr_in *, struct sockaddr_in *));

/* month_sname.c */
char *month_sname P((int ));

/* netread.c */
int krb_net_read P((int , char *, int ));

/* netwrite.c */
int krb_net_write P((int , char *, int ));

/* one.c */

/* pkt_cipher.c */
KTEXT pkt_cipher P((KTEXT ));

/* pkt_clen.c */
int pkt_clen P((KTEXT ));

/* rd_err.c */
int krb_rd_err P((u_char *, u_long , long *, MSG_DAT *));

/* rd_priv.c */
long krb_rd_priv P((u_char *, u_long , Key_schedule , C_Block , struct sockaddr_in *, struct sockaddr_in *, MSG_DAT *));

/* rd_req.c */
int krb_set_key P((char *, int ));
int krb_rd_req P((KTEXT , char *, char *, long , AUTH_DAT *, char *));

/* rd_safe.c */
long krb_rd_safe P((u_char *, u_long , C_Block *, struct sockaddr_in *, struct sockaddr_in *, MSG_DAT *));

/* read_service_key.c */
int read_service_key P((char *, char *, char *, int , char *, char *));

/* recvauth.c */
int krb_recvauth P((long , int , KTEXT , char *, char *, struct sockaddr_in *, struct sockaddr_in *, AUTH_DAT *, char *, Key_schedule , char *));

/* save_credentials.c */
int save_credentials P((char *, char *, char *, C_Block , int , int , KTEXT , long ));

/* send_to_kdc.c */
int send_to_kdc P((KTEXT , KTEXT , char *));

/* sendauth.c */
int krb_sendauth P((long , int , KTEXT , char *, char *, char *, u_long , MSG_DAT *, CREDENTIALS *, Key_schedule , struct sockaddr_in *, struct sockaddr_in *, char *));
int krb_sendsvc P((int , char *));

/* setenv.c */
int setenv P((char *, char *, int ));
void unsetenv P((char *));
char *getenv P((char *));
char *_findenv P((char *, int *));

/* stime.c */
char *stime P((long *));

/* tf_shm.c */
int krb_shm_create P((char *));
int krb_is_diskless P((void ));
int krb_shm_dest P((char *));

/* tf_util.c */
int tf_init P((char *, int ));
int tf_get_pname P((char *));
int tf_get_pinst P((char *));
int tf_get_cred P((CREDENTIALS *));
int tf_close P((void ));
int tf_save_cred P((char *, char *, char *, C_Block , int , int , KTEXT , long ));

/* tkt_string.c */
char *tkt_string P((void ));
void krb_set_tkt_string P((char *));

/* util.c */
int ad_print P((AUTH_DAT *));
int placebo_cblock_print P((des_cblock ));

#undef P
