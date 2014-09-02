#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <vm/vm.h>
#include <machine/devmap.h>
#include <machine/machdep.h>
#include <arm/at91/at91var.h>
#include <arm/at91/at91soc.h>
#include <arm/at91/at91_aicreg.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <machine/fdt.h>

extern const struct arm_devmap_entry at91_devmap[];
extern struct bus_space at91_bs_tag;
bus_space_tag_t fdtbus_bs_tag = &at91_bs_tag;

struct fdt_fixup_entry fdt_fixup_table[] = {
	{ NULL, NULL }
};

static int
fdt_aic_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{
	int offset;

	if (fdt_is_compatible(node, "atmel,at91rm9200-aic"))
		offset = 0;
	else
		return (ENXIO);

	*interrupt = fdt32_to_cpu(intr[0]) + offset;
	*trig = INTR_TRIGGER_CONFORM;
	*pol = INTR_POLARITY_CONFORM;

	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_aic_decode_ic,
	NULL
};

static void
at91_eoi(void *unused)
{
	uint32_t *eoicr = (uint32_t *)(0xdffff000 + IC_EOICR);

	*eoicr = 0;
}


vm_offset_t
initarm_lastaddr(void)
{

	return (arm_devmap_lastaddr());
}

void
initarm_early_init(void)
{

	arm_post_filter = at91_eoi;
	at91_soc_id();
	arm_devmap_register_table(at91_devmap);
}

int
initarm_devmap_init(void)
{

//	arm_devmap_add_entry(0xfff00000, 0x00100000); /* 1MB - uart, aic and timers*/

	return (0);
}

void
initarm_gpio_init(void)
{
}

void
initarm_late_init(void)
{
}
