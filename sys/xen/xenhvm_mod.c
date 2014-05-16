#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>

static int
xenhvm_modevent(module_t mod, int type, void *arg)
{

	switch (type) {
	case MOD_LOAD:
		if (inw(0x10) == 0x49d2) {
			if (bootverbose)
				printf("Xen detected: disabling emulated block and network devices\n");
			outw(0x10, 3);
		}
		return (0);
	}

	return (EOPNOTSUPP);
}

static moduledata_t xenhvm_mod = {
	"xenhvm",
	xenhvm_modevent,
	0
};

DECLARE_MODULE(xenhvm, xenhvm_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
