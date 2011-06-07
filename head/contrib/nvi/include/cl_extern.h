#ifndef HAVE_CURSES_ADDNSTR
int addnstr __P((char *, int));
#endif
#ifndef HAVE_CURSES_BEEP
void beep __P((void));
#endif
#ifndef HAVE_CURSES_FLASH
void flash __P((void));
#endif
#ifndef HAVE_CURSES_IDLOK
void idlok __P((WINDOW *, int));
#endif
#ifndef HAVE_CURSES_KEYPAD
int keypad __P((void *, int));
#endif
#ifndef HAVE_CURSES_NEWTERM
void *newterm __P((const char *, FILE *, FILE *));
#endif
#ifndef HAVE_CURSES_SETUPTERM
void setupterm __P((char *, int, int *));
#endif
#ifdef HAVE_CURSES_TIGETSTR
char *tigetstr();
#else
char *tigetstr __P((char *));
#endif
#ifndef HAVE_CURSES_TIGETSTR
int tigetnum __P((char *));
#endif
int cl_addstr __P((SCR *, const char *, size_t));
int cl_attr __P((SCR *, scr_attr_t, int));
int cl_baud __P((SCR *, u_long *));
int cl_bell __P((SCR *));
int cl_clrtoeol __P((SCR *));
int cl_cursor __P((SCR *, size_t *, size_t *));
int cl_deleteln __P((SCR *));
int cl_ex_adjust __P((SCR *, exadj_t));
int cl_insertln __P((SCR *));
int cl_keyval __P((SCR *, scr_keyval_t, CHAR_T *, int *));
int cl_move __P((SCR *, size_t, size_t));
int cl_refresh __P((SCR *, int));
int cl_rename __P((SCR *, char *, int));
int cl_suspend __P((SCR *, int *));
void cl_usage __P((void));
int sig_init __P((GS *, SCR *));
int cl_event __P((SCR *, EVENT *, u_int32_t, int));
int cl_screen __P((SCR *, u_int32_t));
int cl_quit __P((GS *));
int cl_getcap __P((SCR *, char *, char **));
int cl_term_init __P((SCR *));
int cl_term_end __P((GS *));
int cl_fmap __P((SCR *, seq_t, CHAR_T *, size_t, CHAR_T *, size_t));
int cl_optchange __P((SCR *, int, char *, u_long *));
int cl_omesg __P((SCR *, CL_PRIVATE *, int));
int cl_ssize __P((SCR *, int, size_t *, size_t *, int *));
int cl_putchar __P((int));
