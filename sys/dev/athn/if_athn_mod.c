
#include <sys/types.h>
#include <sys/systm.h>  /* uprintf */
#include <sys/errno.h>
#include <sys/param.h>  /* defines used in kernel.h */
#include <sys/module.h>
#include <sys/kernel.h> /* types used in module initialization */

/* Temporary to nofiy that the module was loaded */
static int
athn_usb_load(struct module *m, int what, void *arg)
{
//	const struct firmware *fp = NULL;
//	int fd;
//	void* athn_fw_bin = NULL;
//
//	size_t athn_fw_size = 0;
	int error = 0;

	switch (what) {
	case MOD_LOAD:
		uprintf("Athn KLD loaded.\n");
		break;
	case MOD_UNLOAD:
		uprintf("Athn KLD unloaded.\n");
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return(error);
}

/* Declare this module to the rest of the kernel */

static moduledata_t athn_usb_module = {
	"athn_mod",
	athn_usb_load,
	NULL
};

DECLARE_MODULE(athn_mod, athn_usb_module, SI_SUB_DRIVERS, SI_ORDER_ANY);