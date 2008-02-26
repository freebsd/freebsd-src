/*
 * from: FreeBSD: src/sys/tools/fw_stub.awk,v 1.6 2007/03/02 11:42:53 flz
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/systm.h>
#include <dev/cxgb/cxgb_t3fw.h>

static int
cxgb_t3fw_modevent(module_t mod, int type, void *unused)
{
	const struct firmware *fp, *parent;
	int error;
	switch (type) {
	case MOD_LOAD:

		fp = firmware_register("cxgb_t3fw", t3fw, 
				       (size_t)t3fw_length,
				       0, NULL);
		if (fp == NULL)
			goto fail_0;
		parent = fp;
		return (0);
	fail_0:
		return (ENXIO);
	case MOD_UNLOAD:
		error = firmware_unregister("cxgb_t3fw");
		return (error);
	}
	return (EINVAL);
}

static moduledata_t cxgb_t3fw_mod = {
        "cxgb_t3fw",
        cxgb_t3fw_modevent,
        0
};
DECLARE_MODULE(cxgb_t3fw, cxgb_t3fw_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(cxgb_t3fw, 1);
MODULE_DEPEND(cxgb_t3fw, firmware, 1, 1, 1);

