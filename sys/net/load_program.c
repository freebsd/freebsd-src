#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <gbpf.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/ebpf.h>
#include <sys/ebpf_param.h>

#include <sys/xdp.h>
#include <sys/ebpf_probe.h>
#include <gbpf_driver.h>

struct my_data {
	int mymap;
	int myprog;
};

/*
 * Callback when the found from the ELF file. 
 * At this moment, the relocation for the maps are already 
 * done. So, what we need is just load the program and 
 * pick the file descriptor.
 */

void
on_prog(GBPFElfWalker *walker, const char *name,
		struct ebpf_inst *prog, uint32_t prog_len)
{
	struct my_data *data = (struct my_data *)walker->data;

	printf("Found program: %s\n", name);
	
	data->myprog = gbpf_load_prog(walker->driver, EBPF_PROG_TYPE_XDP,
					prog, prog_len);
	assert(data->myprog != -1);
}

/*
 * Callback when the map found from the ELF file.
 * At this moment, the maps is already "created".
 * So, what we need to do is pick the file descriptor.
 */
void
on_map(GBPFElfWalker *walker, const char *name, int desc, 
		struct ebpf_map_def *map)
{
	struct my_data *data = (struct my_data *)walker->data;

	printf("Found map: %s\n", name);
	data->mymap = desc;
}

int
main(int argc, char **argv)
{
	int error;
	struct my_data data = {0};
	EBPFDevDriver *devDriver = ebpf_dev_driver_create();
	assert(devDriver != NULL);

	GBPFDriver *driver = &devDriver->base;

	GBPFElfWalker walker = {
		.driver = driver,
		.on_prog = on_prog,
		.on_map = on_map,
		.data = &data
	};

	error = gbpf_walk_elf(&walker, driver, argv[1]);
	assert(error == 0);

	gbpf_attach_probe(driver, data.myprog, "ebpf", "xdp", "", "xdp_rx", "vtnet0", 0);

	ebpf_dev_driver_destroy(devDriver);

	return EXIT_SUCCESS;
}
