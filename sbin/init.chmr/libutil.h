/*
 * missing prototypes for the functions in libutil.
 * These should be conatined in a system header file, but aren't.
 */
int logout(char *);
int logwtmp(char *, char *, char *);
int login_tty(int);
