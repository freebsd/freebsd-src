/*
 * $Id$
 */

extern void ID0init(void);
extern uid_t ID0realuid(void);
extern int ID0ioctl(int, unsigned long, void *);
extern int ID0unlink(const char *);
extern int ID0socket(int, int, int);
extern FILE *ID0fopen(const char *, const char *);
extern int ID0open(const char *, int);
extern int ID0uu_lock(const char *);
extern int ID0uu_unlock(const char *);
