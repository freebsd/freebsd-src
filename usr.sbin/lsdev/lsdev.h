/*
 * Declarations for lsdev(8).
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <vm/vm.h>
#include <sys/sysctl.h>
#include <sys/devconf.h>

extern const char *const devtypes[]; /* device type array */
extern void print_config(struct devconf *); /* machine-specific print routine */
extern void hprint_config(void); /* machine-specific header printer */
extern int vflag;

extern int findtype(const char *); /* get device type by name */
