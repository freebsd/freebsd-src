/*
 * This file, written by Garrett A. Wollman, is in the public domain.
 */
/*
 * Declarations for lsdev(8).
 */

extern const char *const devtypes[];	/* device type array */
extern int vflag;

int findtype(const char *);		/* get device type by name */
void hprint_config(void);		/* machine-specific header printer */
struct devconf;
void print_config(struct devconf *);	/* machine-specific print routine */
