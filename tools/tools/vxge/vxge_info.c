/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#include "vxge_info.h"
#include <unistd.h>

static int sockfd;
static struct ifreq ifr;

int
main(int argc, char *argv[])
{
	uid_t uid;
	
	uid = getuid();

	if (uid) {
		printf("vxge-manage: Operation not permitted.\nExiting...\n");
		goto _exit0;
	}

	if (argc >= 4) {
		if (!((strcasecmp(argv[2], "regs") == 0) ||
		    (strcasecmp(argv[2], "stats") == 0) ||
		    (strcasecmp(argv[2], "bw_pri_set") == 0) ||
		    (strcasecmp(argv[2], "port_mode_set") == 0) ||
		    (strcasecmp(argv[2], "bw_pri_get") == 0)))
			goto out;
		else {
			if (strcasecmp(argv[2], "regs") == 0) {
				if (!((strcasecmp(argv[3], "common") == 0) ||
				    (strcasecmp(argv[3], "legacy") == 0) ||
				    (strcasecmp(argv[3], "pcicfgmgmt") == 0) ||
				    (strcasecmp(argv[3], "toc") == 0) ||
				    (strcasecmp(argv[3], "vpath") == 0) ||
				    (strcasecmp(argv[3], "vpmgmt") == 0) ||
				    (strcasecmp(argv[3], "mrpcim") == 0) ||
				    (strcasecmp(argv[3], "srpcim") == 0) ||
				    (strcasecmp(argv[3], "all") == 0))) {
					goto regs;
				}
			} else if (strcasecmp(argv[2], "stats") == 0) {

				if (!((strcasecmp(argv[3], "common") == 0) ||
				    (strcasecmp(argv[3], "mrpcim") == 0) ||
				    (strcasecmp(argv[3], "all") == 0) ||
				    (strcasecmp(argv[3], "driver") == 0))) {
					goto stats;
				}
			}
		}
	} else {
		if (argc != 3)
			goto out;
		else {
			if (!((strcasecmp(argv[2], "hwinfo") == 0) ||
			    (strcasecmp(argv[2], "pciconfig") == 0) ||
			    (strcasecmp(argv[2], "port_mode_get") == 0) ||
			    (strcasecmp(argv[2], "bw_pri_get") == 0))) {
				if (strcasecmp(argv[2], "regs") == 0)
					goto regs;

				if (strcasecmp(argv[2], "stats") == 0)
					goto stats;

				if (strcasecmp(argv[2], "bw_pri_set") == 0)
					goto bw_pri_set;

				if (strcasecmp(argv[2], "port_mode_set") == 0)
					goto port_mode_set;

				goto out;
			}
		}
	}

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("Creating socket failed\n");
		goto _exit0;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	strlcpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name));

	if (strcasecmp(argv[2], "pciconfig") == 0)
		vxge_get_pci_config();

	else if (strcasecmp(argv[2], "hwinfo") == 0)
		vxge_get_hw_info();

	else if (strcasecmp(argv[2], "vpathinfo") == 0)
		vxge_get_num_vpath();

	else if (strcasecmp(argv[2], "port_mode_get") == 0)
		vxge_get_port_mode();

	else if (strcasecmp(argv[2], "regs") == 0) {

		if (strcasecmp(argv[3], "common") == 0)
			vxge_get_registers_common();

		else if (strcasecmp(argv[3], "toc") == 0)
			vxge_get_registers_toc();

		else if (strcasecmp(argv[3], "pcicfgmgmt") == 0)
			vxge_get_registers_pcicfgmgmt();

		else if (strcasecmp(argv[3], "vpath") == 0)
			vxge_get_registers_vpath();

		else if (strcasecmp(argv[3], "vpmgmt") == 0)
			vxge_get_registers_vpmgmt();

		else if (strcasecmp(argv[3], "srpcim") == 0)
			vxge_get_registers_srpcim();

		else if (strcasecmp(argv[3], "legacy") == 0)
			vxge_get_registers_legacy();

		if (strcasecmp(argv[3], "mrpcim") == 0)
			vxge_get_registers_mrpcim();

		else if (strcasecmp(argv[3], "all") == 0)
			vxge_get_registers_all();

	} else if (strcasecmp(argv[2], "stats") == 0) {

		if (strcasecmp(argv[3], "mrpcim") == 0)
			vxge_get_stats_mrpcim();

		else if (strcasecmp(argv[3], "common") == 0)
			vxge_get_stats_common();

		else if (strcasecmp(argv[3], "all") == 0)
			vxge_get_stats_all();

		else if (strcasecmp(argv[3], "driver") == 0) {
			if (argc == 4) {
				vxge_get_stats_driver(-1);
			} else if (argc == 6) {
				if ((strcasecmp(argv[4], "vpath") == 0) &&
				    (atoi(argv[5]) >= 0) &&
				    (atoi(argv[5]) < 17)) {
					vxge_get_stats_driver(atoi(argv[5]));
				} else {
					goto stats;
				}
			}
		} else {
			goto stats;
		}
	} else if (strcasecmp(argv[2], "port_mode_set") == 0) {
		if ((atoi(argv[3]) >= 2) && (atoi(argv[3]) <= 4))
			vxge_set_port_mode(atoi(argv[3]));
		else
			goto port_mode_set;
	} else if (argc == 5) {
		if (strcasecmp(argv[2], "bw_pri_set") == 0) {
			if (((atoi(argv[3]) >= 0) && (atoi(argv[3]) < 8) &&
			    (atoi(argv[4]) <= 10000)))
				vxge_set_bw_priority(atoi(argv[3]),
				    atoi(argv[4]), -1, VXGE_SET_BANDWIDTH);
			else
				goto bw_pri_set;
		}
	} else if (argc == 6) {
		if (strcasecmp(argv[2], "bw_pri_set") == 0) {
			if (((atoi(argv[3]) >= 0) && (atoi(argv[3]) < 8) &&
			    (atoi(argv[4]) <= 10000)) && (atoi(argv[5]) <= 3))
				vxge_set_bw_priority(atoi(argv[3]),
				    atoi(argv[4]), atoi(argv[5]),
				    VXGE_SET_BANDWIDTH);
			else
				goto bw_pri_set;
		}
	} else if (argc == 4) {
		if (strcasecmp(argv[2], "bw_pri_get") == 0) {
			if ((atoi(argv[3]) >= 0) && (atoi(argv[3]) < 8))
				vxge_get_bw_priority(atoi(argv[3]), VXGE_GET_BANDWIDTH);
			else
				goto bw_pri_get;
		}
	} else if (argc == 3) {
		if (strcasecmp(argv[2], "bw_pri_get") == 0)
			vxge_get_bw_priority(-1, VXGE_GET_BANDWIDTH);
		else
			goto bw_pri_get;
	}

	goto _exit0;

out:
	printf("Usage: ");
	printf("vxge-manage <INTERFACE> ");
	printf("[regs] [stats] [hwinfo] [bw_pri_get] [bw_pri_set] [port_mode_get] [port_mode_set] [pciconfig]\n");
	printf("\tINTERFACE      : Interface (vxge0, vxge1, vxge2, ..)\n");
	printf("\tregs           : Prints register values\n");
	printf("\tstats          : Prints statistics\n");
	printf("\tpciconfig      : Prints pci configuration space\n");
	printf("\thwinfo         : Displays hardware information\n");
	printf("\tbw_pri_get     : Displays bandwidth and priority information\n");
	printf("\tbw_pri_set     : Set bandwidth and priority of a function\n");
	printf("\tport_mode_get  : Displays dual port adapter's port mode\n");
	printf("\tport_mode_set  : Set dual port adapter's port mode\n\n");
	goto _exit0;

regs:
	printf("Regs\n");
	printf("[common] [legacy] [pcicfgmgmt] [toc] [vpath] [vpmgmt] [mrpcim] [srpcim] [All]\n");
	printf("\tcommon         : print common registers\n");
	printf("\tlegacy         : print legacy registers\n");
	printf("\tpcicfgmgmt     : print pcicfgmgmt registers\n");
	printf("\ttoc            : print toc registers\n");
	printf("\tvpath          : print vpath registers\n");
	printf("\tvpmgmt         : print vpmgmt registers\n");
	printf("\tmrpcim         : print mrpcim registers\n");
	printf("\tsrpcim         : print srpcim registers\n\n");
	goto _exit0;

stats:
	printf("Stats\n");
	printf("[common] [mrpcim] [driver [vpath (< 17) ]] [All]\n");
	printf("\tcommon         : print common statistics\n");
	printf("\tmrpcim         : print mrpcim statistics\n");
	printf("\tdriver         : print driver statistics\n");
	printf("\tAll            : print all statistics\n\n");
	goto _exit0;

bw_pri_set:
	printf("Bandwidth & Priority\n");
	printf("[vf-id (0-7)] [bandwidth (100-10000)] [priority (0-3)]\n\n");
	goto _exit0;

bw_pri_get:
	printf("Bandwidth & Priority\n");
	printf("[vf-id (0-7)]\n\n");
	goto _exit0;

port_mode_set:
	printf("Port mode Setting\n");
	printf("[port mode value (2-4)]\n\n");
	goto _exit0;

_exit0:
	return (0);
}

/*
 * vxge_get_registers_all
 */
void
vxge_get_registers_all(void)
{
	vxge_get_registers_legacy();
	vxge_get_registers_toc();
	vxge_get_registers_common();
	vxge_get_registers_pcicfgmgmt();
	vxge_get_registers_srpcim();
	vxge_get_registers_mrpcim();
	vxge_get_registers_vpmgmt();
	vxge_get_registers_vpath();
}

int
vxge_get_registers_common(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize =
	    reginfo_registers[VXGE_HAL_MGMT_REG_COUNT_COMMON - 1].offset + 8;

	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for register dump failed\n");
		goto _exit0;
	}

	*buffer = vxge_hal_mgmt_reg_type_common;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting register values failed\n");
		goto _exit0;
	}

	vxge_print_registers(buffer);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_registers_legacy
 */
int
vxge_get_registers_legacy(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = reginfo_legacy[VXGE_HAL_MGMT_REG_COUNT_LEGACY - 1].offset + 8;

	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for register dump failed\n");
		goto _exit0;
	}

	*buffer = vxge_hal_mgmt_reg_type_legacy;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting register values failed\n");
		goto _exit0;
	}

	vxge_print_registers_legacy(buffer);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_registers_toc
 */
int
vxge_get_registers_toc(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = reginfo_toc[VXGE_HAL_MGMT_REG_COUNT_TOC - 1].offset + 8;
	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for register dump failed\n");
		goto _exit0;
	}

	*buffer = vxge_hal_mgmt_reg_type_toc;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting register values failed\n");
		goto _exit0;
	}

	vxge_print_registers_toc(buffer);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_registers_pcicfgmgmt
 */
int
vxge_get_registers_pcicfgmgmt(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = reginfo_pcicfgmgmt[VXGE_HAL_MGMT_REG_COUNT_PCICFGMGMT - 1].offset + 8;

	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for register dump failed\n");
		goto _exit0;
	}

	*buffer = vxge_hal_mgmt_reg_type_pcicfgmgmt;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting register values failed\n");
		goto _exit0;
	}

	vxge_print_registers_pcicfgmgmt(buffer);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_registers_vpath
 */
int
vxge_get_registers_vpath(void)
{
	int bufsize, err = 0;
	u32 i, no_of_vpath;
	char *buffer = NULL;

	no_of_vpath = vxge_get_num_vpath();
	bufsize = reginfo_vpath[VXGE_HAL_MGMT_REG_COUNT_VPATH - 1].offset + 8;

	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for register dump failed\n");
		goto _exit0;
	}

	for (i = 0; i < no_of_vpath; i++) {

		bzero(buffer, bufsize);
		*buffer = vxge_hal_mgmt_reg_type_vpath;
		*((u32 *) (buffer + sizeof(u32))) = i;

		ifr.ifr_data = (caddr_t) buffer;
		err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
		if ((err < 0) || (err == EINVAL)) {
			printf("Getting register values failed\n");
			goto _exit0;
		}

		vxge_print_registers_vpath(buffer, i);
	}

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_registers_vpmgmt
 */
int
vxge_get_registers_vpmgmt(void)
{
	int bufsize, err = 0;
	u32 i, no_of_vpath;
	char *buffer = NULL;

	no_of_vpath = vxge_get_num_vpath();
	bufsize = reginfo_vpmgmt[VXGE_HAL_MGMT_REG_COUNT_VPMGMT - 1].offset + 8;
	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for register dump failed\n");
		goto _exit0;
	}

	for (i = 0; i < no_of_vpath; i++) {

		bzero(buffer, bufsize);
		*buffer = vxge_hal_mgmt_reg_type_vpmgmt;
		*((u32 *) (buffer + sizeof(u32))) = i;

		ifr.ifr_data = (caddr_t) buffer;
		err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
		if ((err < 0) || (err == EINVAL)) {
			printf("Getting register values failed\n");
			goto _exit0;
		}

		vxge_print_registers_vpmgmt(buffer);
	}

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

u32
vxge_get_num_vpath(void)
{
	int err = 0;
	u32 buffer, no_of_vpath = 0;

	buffer = VXGE_GET_VPATH_COUNT;

	ifr.ifr_data = (caddr_t) &buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
	if (err == 0)
		no_of_vpath = buffer;
	else
		printf("Getting number of vpath failed\n");

	return (no_of_vpath);
}

/*
 * vxge_get_registers_mrpcim
 */
int
vxge_get_registers_mrpcim(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = reginfo_mrpcim[VXGE_HAL_MGMT_REG_COUNT_MRPCIM - 1].offset + 8;
	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for register dump failed\n");
		goto _exit0;
	}

	*buffer = vxge_hal_mgmt_reg_type_mrpcim;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting register values failed\n");
		goto _exit0;
	}

	vxge_print_registers_mrpcim(buffer);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_registers_srpcim
 * Gets srpcim register values
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
vxge_get_registers_srpcim(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = reginfo_srpcim[VXGE_HAL_MGMT_REG_COUNT_SRPCIM - 1].offset + 8;
	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for register dump failed\n");
		goto _exit0;
	}

	*buffer = vxge_hal_mgmt_reg_type_srpcim;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_1, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting register values failed\n");
		goto _exit0;
	}

	vxge_print_registers_srpcim(buffer);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_stats_driver
 */
int
vxge_get_stats_driver(int vpath_num)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = VXGE_HAL_MGMT_STATS_COUNT_DRIVER * sizeof(u64) *
	    VXGE_HAL_MAX_VIRTUAL_PATHS;

	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for driver statistics failed\n");
		goto _exit0;
	}

	*buffer = VXGE_GET_DRIVER_STATS;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting Driver Statistics failed\n");
		goto _exit0;
	}

	vxge_print_stats_drv(buffer, vpath_num);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_stats_common
 */
int
vxge_get_stats_common(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = 1024 * 64 * sizeof(char);

	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for statistics dump failed\n");
		goto _exit0;
	}

	*buffer = VXGE_GET_DEVICE_STATS;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting statistics values failed\n");
		goto _exit0;
	}

	vxge_print_stats(buffer, VXGE_GET_DEVICE_STATS);

_exit0:
	vxge_mem_free(buffer);
	return (err);

}

/*
 * vxge_get_stats_mrpcim
 */
int
vxge_get_stats_mrpcim(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = 1024 * 64 * sizeof(char);

	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for statistics dump failed\n");
		goto _exit0;
	}

	*buffer = VXGE_GET_MRPCIM_STATS;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting statistics values failed\n");
		goto _exit0;
	}

	vxge_print_stats(buffer, VXGE_GET_MRPCIM_STATS);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

int
vxge_get_pci_config(void)
{
	int bufsize, err = 0;
	char *buffer = NULL;

	bufsize = 64 * 1024 * sizeof(char);

	buffer = (char *) vxge_mem_alloc(bufsize);
	if (!buffer) {
		printf("Allocating memory for pci config failed\n");
		goto _exit0;
	}

	*buffer = VXGE_GET_PCI_CONF;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting pci config values failed\n");
		goto _exit0;
	}

	vxge_print_pci_config(buffer);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_hw_info
 */
int
vxge_get_hw_info(void)
{
	int err = 0;
	char *buffer = NULL;

	buffer = (char *) vxge_mem_alloc(sizeof(vxge_device_hw_info_t));
	if (!buffer) {
		printf("Allocating memory for hw info failed\n");
		goto _exit0;
	}

	*buffer = VXGE_GET_DEVICE_HWINFO;

	ifr.ifr_data = (caddr_t) buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL)) {
		printf("Getting hw info failed\n");
		goto _exit0;
	}

	vxge_print_hw_info(buffer);

_exit0:
	vxge_mem_free(buffer);
	return (err);
}

/*
 * vxge_get_stats_all
 */
void
vxge_get_stats_all(void)
{
	vxge_get_stats_mrpcim();
	vxge_get_stats_common();
	vxge_get_stats_driver(0);
}

int
vxge_get_bw_priority(int func_id, vxge_query_device_info_e vxge_query_info)
{
	int err = 0;
	vxge_bw_info_t buffer;

	bzero(&buffer, sizeof(vxge_bw_info_t));

	buffer.query = (char) vxge_query_info;
	if (func_id != -1)
		buffer.func_id = func_id;

	ifr.ifr_data = (caddr_t) &buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL))
		printf("Getting bw info failed\n");
	else
		vxge_print_bw_priority(&buffer);

	return (err);
}

int
vxge_set_bw_priority(int func_id, int bandwidth, int priority,
    vxge_query_device_info_e vxge_query_info)
{
	int err = 0;
	vxge_bw_info_t buffer;

	bzero(&buffer, sizeof(vxge_bw_info_t));

	buffer.query = (char) vxge_query_info;
	buffer.func_id = func_id;
	buffer.bandwidth = bandwidth;
	buffer.priority = priority;

	ifr.ifr_data = (caddr_t) &buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL))
		printf("Setting bandwidth failed\n");

	return (err);
}

int
vxge_set_port_mode(int port_val)
{
	int err = 0;
	vxge_port_info_t buffer;

	buffer.query = VXGE_SET_PORT_MODE;
	buffer.port_mode = port_val;
	buffer.port_failure = 0;

	ifr.ifr_data = (caddr_t) &buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL))
		printf("Setting	port_mode failed\n");
	else
		printf("Port mode set. Reboot the system for changes to take effect.\n");

	return (err);
}

int
vxge_get_port_mode()
{
	int err = 0;
	vxge_port_info_t buffer;

	bzero(&buffer, sizeof(vxge_port_info_t));

	buffer.query = VXGE_GET_PORT_MODE;

	ifr.ifr_data = (caddr_t) &buffer;
	err = ioctl(sockfd, SIOCGPRIVATE_0, &ifr);
	if ((err < 0) || (err == EINVAL))
		printf("Getting port mode info failed\n");
	else
		vxge_print_port_mode(&buffer);

	return (err);
}
/*
 * Removes trailing spaces padded
 * and NULL terminates strings
 */
void
vxge_null_terminate(char *str, size_t len)
{
	len--;
	while (*str && (*str != ' ') && (len != 0))
		++str;

	--len;
	if (*str)
		*str = '\0';
}

void *
vxge_mem_alloc(u_long size)
{
	void *vaddr = NULL;
	vaddr = malloc(size);
	if (NULL != vaddr)
		bzero(vaddr, size);

	return (vaddr);
}
