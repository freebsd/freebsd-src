#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <nlist.h>
#include <stdio.h>

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
#ifdef DO_SCSI
    {"_scbusses"},
    {"_scsi_cinit"},
    {"_scsi_dinit"},
    {"_scsi_tinit"},
#endif
    {""},
};

int
main(int ac, char **av)
{
    int i, len;

    i = nlist(av[1], nl);
    if (i == -1) {
	fprintf(stderr, "nlist returns error for %s\n", av[1]);
	perror("nlist");
	return 1;
    }
    len = (sizeof(nl) / sizeof(struct nlist)) - 1;
    printf("%d\n", len);
    for (i = 0; i < len; i++) {
	printf("%s\n", nl[i].n_name);
	printf("%d %d %d %ld\n",
	       nl[i].n_type, nl[i].n_other, nl[i].n_desc, nl[i].n_value);
    }
    return 0;
}
