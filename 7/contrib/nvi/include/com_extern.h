#ifndef HAVE_BSEARCH
void	*bsearch __P((const void *, const void *, size_t,
   size_t, int (*)(const void *, const void *)));
#endif
#ifndef HAVE_SETENV
int setenv __P((const char *, const char *, int));
#endif
#ifndef HAVE_UNSETENV
void unsetenv __P((const char *));
#endif
#ifndef HAVE_GETHOSTNAME
int gethostname __P((char *, int));
#endif
#ifndef HAVE_GETOPT
int getopt __P((int, char * const *, const char *)); 
#endif
#ifndef HAVE_MEMCHR
void *memchr __P((const void *, int, size_t));
#endif
#ifndef HAVE_MEMCPY
void *memcpy __P((void *, const void *, size_t));
#endif
#ifndef HAVE_MEMMOVE
void *memmove __P((void *, const void *, size_t));
#endif
#ifndef HAVE_MEMSET
void *memset __P((void *, int, size_t));
#endif
#ifndef HAVE_MKSTEMP
int mkstemp __P((char *));
#endif
#ifndef HAVE_MMAP
char *mmap __P((char *, size_t, int, int, int, off_t));
#endif
#ifndef HAVE_MMAP
int munmap __P((char *, size_t));
#endif
#ifndef HAVE_SNPRINTF
int snprintf __P((char *, size_t, const char *, ...));
#endif
#ifndef HAVE_STRDUP
char *strdup __P((const char *));
#endif
#ifndef HAVE_STRERROR
char *strerror __P((int));
#endif
#ifndef HAVE_STRPBRK
char *strpbrk __P((const char *, const char *));
#endif
#ifndef HAVE_STRSEP
char *strsep __P((char **, const char *));
#endif
#ifndef HAVE_STRTOL
long strtol __P((const char *, char **, int));
#endif
#ifndef HAVE_STRTOUL
unsigned long strtoul __P((const char *, char **, int));
#endif
#ifndef HAVE_VSNPRINTF
int vsnprintf __P((char *, size_t, const char *, ...));
#endif
SCR *api_fscreen __P((int, char *));
int api_aline __P((SCR *, recno_t, char *, size_t));
int api_dline __P((SCR *, recno_t));
int api_gline __P((SCR *, recno_t, char **, size_t *));
int api_iline __P((SCR *, recno_t, char *, size_t));
int api_lline __P((SCR *, recno_t *));
int api_sline __P((SCR *, recno_t, char *, size_t));
int api_getmark __P((SCR *, int, MARK *));
int api_setmark __P((SCR *, int, MARK *));
int api_nextmark __P((SCR *, int, char *));
int api_getcursor __P((SCR *, MARK *));
int api_setcursor __P((SCR *, MARK *));
void api_emessage __P((SCR *, char *));
void api_imessage __P((SCR *, char *));
int api_edit __P((SCR *, char *, SCR **, int));
int api_escreen __P((SCR *));
int api_swscreen __P((SCR *, SCR *));
int api_map __P((SCR *, char *, char *, size_t));
int api_unmap __P((SCR *, char *));
int api_opts_get __P((SCR *, char *, char **, int *));
int api_opts_set __P((SCR *, char *, char *, u_long, int));
int api_run_str __P((SCR *, char *));
int cut __P((SCR *, CHAR_T *, MARK *, MARK *, int));
int cut_line __P((SCR *, recno_t, size_t, size_t, CB *));
void cut_close __P((GS *));
TEXT *text_init __P((SCR *, const char *, size_t, size_t));
void text_lfree __P((TEXTH *));
void text_free __P((TEXT *));
int del __P((SCR *, MARK *, MARK *, int));
FREF *file_add __P((SCR *, CHAR_T *));
int file_init __P((SCR *, FREF *, char *, int));
int file_end __P((SCR *, EXF *, int));
int file_write __P((SCR *, MARK *, MARK *, char *, int));
int file_m1 __P((SCR *, int, int));
int file_m2 __P((SCR *, int));
int file_m3 __P((SCR *, int));
int file_aw __P((SCR *, int));
void set_alt_name __P((SCR *, char *));
lockr_t file_lock __P((SCR *, char *, int *, int, int));
int v_key_init __P((SCR *));
void v_key_ilookup __P((SCR *));
size_t v_key_len __P((SCR *, ARG_CHAR_T));
CHAR_T *v_key_name __P((SCR *, ARG_CHAR_T));
int v_key_val __P((SCR *, ARG_CHAR_T));
int v_event_push __P((SCR *, EVENT *, CHAR_T *, size_t, u_int));
int v_event_get __P((SCR *, EVENT *, int, u_int32_t));
void v_event_err __P((SCR *, EVENT *));
int v_event_flush __P((SCR *, u_int));
int db_eget __P((SCR *, recno_t, char **, size_t *, int *));
int db_get __P((SCR *, recno_t, u_int32_t, char **, size_t *));
int db_delete __P((SCR *, recno_t));
int db_append __P((SCR *, int, recno_t, char *, size_t));
int db_insert __P((SCR *, recno_t, char *, size_t));
int db_set __P((SCR *, recno_t, char *, size_t));
int db_exist __P((SCR *, recno_t));
int db_last __P((SCR *, recno_t *));
void db_err __P((SCR *, recno_t));
int log_init __P((SCR *, EXF *));
int log_end __P((SCR *, EXF *));
int log_cursor __P((SCR *));
int log_line __P((SCR *, recno_t, u_int));
int log_mark __P((SCR *, LMARK *));
int log_backward __P((SCR *, MARK *));
int log_setline __P((SCR *));
int log_forward __P((SCR *, MARK *));
int editor __P((GS *, int, char *[]));
void v_end __P((GS *));
int mark_init __P((SCR *, EXF *));
int mark_end __P((SCR *, EXF *));
int mark_get __P((SCR *, ARG_CHAR_T, MARK *, mtype_t));
int mark_set __P((SCR *, ARG_CHAR_T, MARK *, int));
int mark_insdel __P((SCR *, lnop_t, recno_t));
void msgq __P((SCR *, mtype_t, const char *, ...));
void msgq_str __P((SCR *, mtype_t, char *, char *));
void mod_rpt __P((SCR *));
void msgq_status __P((SCR *, recno_t, u_int));
int msg_open __P((SCR *, char *));
void msg_close __P((GS *));
const char *msg_cmsg __P((SCR *, cmsg_t, size_t *));
const char *msg_cat __P((SCR *, const char *, size_t *));
char *msg_print __P((SCR *, const char *, int *));
int opts_init __P((SCR *, int *));
int opts_set __P((SCR *, ARGS *[], char *));
int o_set __P((SCR *, int, u_int, char *, u_long));
int opts_empty __P((SCR *, int, int));
void opts_dump __P((SCR *, enum optdisp));
int opts_save __P((SCR *, FILE *));
OPTLIST const *opts_search __P((char *));
void opts_nomatch __P((SCR *, char *));
int opts_copy __P((SCR *, SCR *));
void opts_free __P((SCR *));
int f_altwerase __P((SCR *, OPTION *, char *, u_long *));
int f_columns __P((SCR *, OPTION *, char *, u_long *));
int f_lines __P((SCR *, OPTION *, char *, u_long *));
int f_lisp __P((SCR *, OPTION *, char *, u_long *));
int f_msgcat __P((SCR *, OPTION *, char *, u_long *));
int f_paragraph __P((SCR *, OPTION *, char *, u_long *));
int f_print __P((SCR *, OPTION *, char *, u_long *));
int f_readonly __P((SCR *, OPTION *, char *, u_long *));
int f_recompile __P((SCR *, OPTION *, char *, u_long *));
int f_reformat __P((SCR *, OPTION *, char *, u_long *));
int f_section __P((SCR *, OPTION *, char *, u_long *));
int f_ttywerase __P((SCR *, OPTION *, char *, u_long *));
int f_w300 __P((SCR *, OPTION *, char *, u_long *));
int f_w1200 __P((SCR *, OPTION *, char *, u_long *));
int f_w9600 __P((SCR *, OPTION *, char *, u_long *));
int f_window __P((SCR *, OPTION *, char *, u_long *));
int put __P((SCR *, CB *, CHAR_T *, MARK *, MARK *, int));
int rcv_tmp __P((SCR *, EXF *, char *));
int rcv_init __P((SCR *));
int rcv_sync __P((SCR *, u_int));
int rcv_list __P((SCR *));
int rcv_read __P((SCR *, FREF *));
int screen_init __P((GS *, SCR *, SCR **));
int screen_end __P((SCR *));
SCR *screen_next __P((SCR *));
int f_search __P((SCR *,
   MARK *, MARK *, char *, size_t, char **, u_int));
int b_search __P((SCR *,
   MARK *, MARK *, char *, size_t, char **, u_int));
void search_busy __P((SCR *, busy_t));
int seq_set __P((SCR *, CHAR_T *,
   size_t, CHAR_T *, size_t, CHAR_T *, size_t, seq_t, int));
int seq_delete __P((SCR *, CHAR_T *, size_t, seq_t));
int seq_mdel __P((SEQ *));
SEQ *seq_find
   __P((SCR *, SEQ **, EVENT *, CHAR_T *, size_t, seq_t, int *));
void seq_close __P((GS *));
int seq_dump __P((SCR *, seq_t, int));
int seq_save __P((SCR *, FILE *, char *, seq_t));
int e_memcmp __P((CHAR_T *, EVENT *, size_t));
void *binc __P((SCR *, void *, size_t *, size_t));
int nonblank __P((SCR *, recno_t, size_t *));
char *tail __P((char *));
CHAR_T *v_strdup __P((SCR *, const CHAR_T *, size_t));
enum nresult nget_uslong __P((u_long *, const char *, char **, int));
enum nresult nget_slong __P((long *, const char *, char **, int));
void TRACE __P((SCR *, const char *, ...));
