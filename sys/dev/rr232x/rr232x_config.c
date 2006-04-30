#include <dev/rr232x/rr232x_config.h>
/****************************************************************************
 * config.c - auto-generated file
 * $FreeBSD$
 ****************************************************************************/
#include <dev/rr232x/os_bsd.h>

extern int init_module_him_sx508x(void);
extern int init_module_vdev_raw(void);
extern int init_module_partition(void);
extern int init_module_raid0(void);
extern int init_module_raid1(void);
extern int init_module_raid5(void);
extern int init_module_jbod(void);

int init_config(void)
{
	init_module_him_sx508x();
	init_module_vdev_raw();
	init_module_partition();
	init_module_raid0();
	init_module_raid1();
	init_module_raid5();
	init_module_jbod();
	return 0;
}

char driver_name[] = "rr232x";
char driver_name_long[] = "RocketRAID 232x controller driver";
char driver_ver[] = "v1.02 (" __DATE__ " " __TIME__ ")";
int  osm_max_targets = 0xff;
