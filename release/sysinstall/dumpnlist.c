#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <nlist.h>
#include <stdio.h>
#include "uc_main.h"

struct nlist    nl[] = {
    {"_isa_devtab_bio"},
    {"_isa_devtab_tty"},
    {"_isa_devtab_net"},
    {"_isa_devtab_null"},
    {"_isa_biotab_wdc"},
    {"_isa_biotab_fdc"},
    {"_eisadriver_set"},
    {"_eisa_dev_list"},
    {"_pcidevice_set"},
    {"_device_list"},
    {"_scbusses"},
    {"_scsi_cinit"},
    {"_scsi_dinit"},
    {"_scsi_tinit"},
    {""},
};

int
main(int ac, char **av)
{
    int i;

    i = nlist(av[1], nl);
    if (i == -1) {
	fprintf(stderr, "nlist returns error for %s\n", av[1]);
	perror("nlist");
	return 1;
    }
    fprintf(stdout, "struct nlist nl[] = {\n");
    for (i = 0; nl[i].n_name; i++) {
	fprintf(stdout, "\t{ \"%s\", %d, %d, %d, %ld },\n",
	nl[i].n_name, nl[i].n_type, nl[i].n_other,
	nl[i].n_desc, nl[i].n_value);
    }
    fprintf(stdout, "};\n");
    return 0;
}
