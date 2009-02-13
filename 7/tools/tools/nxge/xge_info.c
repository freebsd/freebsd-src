/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "xge_info.h"

int
main( int argc, char *argv[] )
{
	int status = EXIT_FAILURE;

	if(argc >= 4) {
	    if(!((strcmp(argv[2], "getregister")         == 0) ||
	        (strcmp(argv[2],  "setregister")          == 0) ||
	        (strcmp(argv[2],  "setbufmode")  == 0))) {
	        goto out;
	    }
	}
	else {
	    if(argc != 3) {
	        goto out;
	    }
	    else {
	        if(!((strcmp(argv[2], "hwstats")       == 0) ||
	            (strcmp(argv[2],  "pciconf")       == 0) ||
	            (strcmp(argv[2],  "devconf")       == 0) ||
	            (strcmp(argv[2],  "registers")     == 0) ||
	            (strcmp(argv[2],  "version")       == 0) ||
	            (strcmp(argv[2],  "swstats")       == 0) ||
	            (strcmp(argv[2],  "drvstats")      == 0) ||
	            (strcmp(argv[2],  "getbufmode")    == 0) ||
	            (strcmp(argv[2],  "devstats")      == 0))) {
	                goto out;
	            }
	    }
	}

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    printf("Creating socket failed\n");
	    goto _exit;
	}

	ifreqp.ifr_addr.sa_family = AF_INET;
	strcpy(ifreqp.ifr_name, argv[1]);

	if (strcmp(argv[2], "pciconf") == 0)
	    status = xge_get_pciconf();
	else if(strcmp(argv[2], "devconf") == 0)
	    status = xge_get_devconf();
	else if(strcmp(argv[2], "hwstats") == 0)
	    status = xge_get_hwstats();
	else if(strcmp(argv[2], "registers") == 0)
	    status = xge_get_registers();
	else if(strcmp(argv[2], "devstats") == 0)
	    status = xge_get_devstats();
	else if(strcmp(argv[2], "swstats") == 0)
	    status = xge_get_swstats();
	else if(strcmp(argv[2], "drvstats") == 0)
	    status = xge_get_drvstats();
	else if(strcmp(argv[2], "version") == 0)
	    status = xge_get_drv_version();
	else if(strcmp(argv[2], "getbufmode") == 0)
	    status = xge_get_buffer_mode();
	else if(strcmp(argv[2], "getregister") == 0)
	    status = xge_get_register(argv[3]);
	else if(strcmp(argv[2], "setregister") == 0)
	    status = xge_set_register(argv[3], argv[4]);
	else if(strcmp(argv[2], "setbufmode") == 0)
	    status = xge_change_buffer_mode(argv[3]);
	goto _exit;

out:
	printf("Usage: ");
	printf("getinfo <INTERFACE> [hwstats] [swstats] [devstats] ");
	printf("[drvstats] [version] [registers] [getregister offset] ");
	printf("[setregister offset value] [pciconf] [devconf] [getbufmode] ");
	printf("[setbufmode]\n");
	printf("\tINTERFACE   : Interface (nxge0, nxge1, nxge2, ..)   \n");
	printf("\thwstats     : Prints hardware statistics            \n");
	printf("\tswstats     : Prints software statistics            \n");
	printf("\tdevstats    : Prints device statistics              \n");
	printf("\tdrvstats    : Prints driver statistics              \n");
	printf("\tversion     : Prints driver version                 \n");
	printf("\tregisters   : Prints register values                \n");
	printf("\tgetregister : Read a register                       \n");
	printf("\tsetregister : Write to a register                   \n");
	printf("\tpciconf     : Prints PCI configuration space        \n");
	printf("\tdevconf     : Prints device configuration           \n");
	printf("\tgetbufmode  : Prints Buffer Mode                    \n");
	printf("\tsetbufmode  : Changes buffer mode                   \n");

_exit:
	return status;
}

/**
 * xge_get_hwstats
 * Gets hardware statistics
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_hwstats(void)
{
	char *hw_stats = NULL, *pci_cfg = NULL;
	unsigned short device_id;
	int index    = 0;
	int status = EXIT_FAILURE;

	buffer_size = GET_OFFSET_STATS(XGE_COUNT_STATS - 1) + 8;

	hw_stats = (char *)malloc(buffer_size);
	if(!hw_stats) {
	    printf("Allocating memory for hardware statistics failed\n");
	    goto _exit;
	}
	*hw_stats = XGE_QUERY_STATS;
	ifreqp.ifr_data = (caddr_t) hw_stats;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting hardware statistics failed\n");
	    goto _exit1;
	}

	buffer_size = GET_OFFSET_PCICONF(XGE_COUNT_PCICONF - 1) + 8;
	pci_cfg = (void *)malloc(buffer_size);
	if(!pci_cfg) {
	    printf("Allocating memory for PCI configuration failed\n");
	    goto _exit1;
	}

	*pci_cfg = XGE_QUERY_PCICONF;
	ifreqp.ifr_data = (caddr_t)pci_cfg;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting pci configuration space failed\n");
	    goto _exit2;
	}
	device_id = *((u16 *)((unsigned char *)pci_cfg +
	    GET_OFFSET_PCICONF(index)));

	xge_print_hwstats(hw_stats,device_id);
	status = EXIT_SUCCESS;

_exit2:
	free(pci_cfg);

_exit1:
	free(hw_stats);

_exit:
	return status;
}

/**
 * xge_get_pciconf
 * Gets PCI configuration space
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_pciconf(void)
{
	char *pci_cfg = NULL;
	int status = EXIT_FAILURE;

	buffer_size = GET_OFFSET_PCICONF(XGE_COUNT_PCICONF - 1) + 8;

	pci_cfg = (char *)malloc(buffer_size);
	if(!pci_cfg) {
	    printf("Allocating memory for PCI configuration failed\n");
	    goto _exit;
	}

	*pci_cfg = XGE_QUERY_PCICONF;
	ifreqp.ifr_data = (caddr_t)pci_cfg;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting PCI configuration space failed\n");
	    goto _exit1;
	}

	xge_print_pciconf( pci_cfg );
	status = EXIT_SUCCESS;

_exit1:
	free(pci_cfg);

_exit:
	return status;
}

/**
 * xge_get_devconf
 * Gets device configuration
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_devconf(void)
{
	char *device_cfg = NULL;
	int status = EXIT_FAILURE;

	buffer_size = XGE_COUNT_DEVCONF * sizeof(int);

	device_cfg = (char *)malloc(buffer_size);
	if(!device_cfg) {
	    printf("Allocating memory for device configuration failed\n");
	    goto _exit;
	}

	*device_cfg = XGE_QUERY_DEVCONF;
	ifreqp.ifr_data = (caddr_t)device_cfg;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting Device Configuration failed\n");
	    goto _exit1;
	}

	xge_print_devconf( device_cfg );
	status = EXIT_SUCCESS;

_exit1:
	free(device_cfg);

_exit:
	return status;
}

/**
 * xge_get_buffer_mode
 * Get current Rx buffer mode
 *
 * Return EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_buffer_mode(void)
{
	char *buf_mode = NULL;
	int status = EXIT_FAILURE;

	buf_mode = (char *)malloc(sizeof(int));
	if(!buf_mode) {
	    printf("Allocating memory for buffer mode failed\n");
	    goto _exit;
	}

	*buf_mode = XGE_QUERY_BUFFER_MODE;
	ifreqp.ifr_data = (void *)buf_mode;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting Buffer Mode failed\n");
	    goto _exit1;
	}
	printf("Rx Buffer Mode: %d\n", *ifreqp.ifr_data);
	status = EXIT_SUCCESS;

_exit1:
	free(buf_mode);

_exit:
	return status;
}

/**
 * xge_change_buffer_mode
 * Change Rx buffer mode
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_change_buffer_mode(char *bufmode)
{
	char *print_msg = NULL;
	int status = EXIT_FAILURE;

	print_msg = (char *)malloc(sizeof(char));
	if(print_msg == NULL) {
	    printf("Allocation of memory for message failed\n");
	    goto _exit;
	}

	if     (*bufmode == '1') *print_msg = XGE_SET_BUFFER_MODE_1;
	else if(*bufmode == '2') *print_msg = XGE_SET_BUFFER_MODE_2;
	else if(*bufmode == '5') *print_msg = XGE_SET_BUFFER_MODE_5;
	else {
	     printf("Invalid Buffer mode\n");
	     goto _exit1;
	}

	ifreqp.ifr_data = (char *)print_msg;
	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Changing buffer mode failed\n");
	    goto _exit1;
	}

	if(*print_msg == 'Y') {
	    printf("Requested buffer mode was already enabled\n");
	}
	else if(*print_msg == 'N') {
	    printf("Requested buffer mode is not implemented OR\n");
	    printf("Dynamic buffer changing is not supported in this driver\n");
	}
	else if(*print_msg == 'C') {
	    printf("Buffer mode changed to %c\n", *bufmode);
	}
	status = EXIT_SUCCESS;

_exit1:
	free(print_msg);

_exit:
	return status;
}


/**
 * xge_get_registers
 * Gets register values
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_registers(void)
{
	void *registers = NULL;
	int status = EXIT_FAILURE;

	buffer_size = regInfo[XGE_COUNT_REGS - 1].offset + 8;

	registers = (void *)malloc(buffer_size);
	if(!registers) {
	    printf("Allocating memory for register dump failed\n");
	    goto _exit;
	}

	ifreqp.ifr_data = (caddr_t)registers;
	if(ioctl(sockfd, SIOCGPRIVATE_1, &ifreqp) < 0) {
	    printf("Getting register values failed\n");
	    goto _exit1;
	}

	xge_print_registers(registers);
	status = EXIT_SUCCESS;

_exit1:
	free(registers);

_exit:
	return status;
}

/**
 * xge_get_register
 * Reads a register specified offset
 *
 * @offset Offset of register from base address
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_register(char *offset)
{
	xge_register_info_t *register_info = NULL;
	int status = EXIT_FAILURE;

	register_info =
	    (xge_register_info_t *)malloc(sizeof(xge_register_info_t));
	if(!register_info) {
	    printf("Allocating memory for register info failed\n");
	    goto _exit;
	}

	strcpy(register_info->option, "-r");
	sscanf(offset, "%x", &register_info->offset);
	ifreqp.ifr_data = (caddr_t)register_info;

	if(ioctl(sockfd, SIOCGPRIVATE_1, &ifreqp) < 0) {
	    printf("Reading register failed\n");
	    goto _exit1;
	}

	xge_print_register(register_info->offset, register_info->value);
	status = EXIT_SUCCESS;

_exit1:
	free(register_info);

_exit:
	return status;
}

/**
 * xge_set_register
 * Writes to a register specified offset
 *
 * @offset Offset of register from base address
 * @value Value to write to
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_set_register(char *offset, char *value)
{
	xge_register_info_t *register_info = NULL;
	int status = EXIT_FAILURE;

	register_info =
	    (xge_register_info_t *)malloc(sizeof(xge_register_info_t));
	if(!register_info) {
	    printf("Allocating memory for register info failed\n");
	    goto _exit;
	}

	strcpy(register_info->option, "-w");
	sscanf(offset, "%x", &register_info->offset);
	sscanf(value, "%llx", &register_info->value);

	ifreqp.ifr_data = (caddr_t)register_info;
	if(ioctl(sockfd, SIOCGPRIVATE_1, &ifreqp) < 0) {
	    printf("Writing register failed\n");
	    goto _exit1;
	}
	status = EXIT_SUCCESS;

_exit1:
	free(register_info);

_exit:
	return status;
}

/**
 * xge_get_devstats
 * Gets device statistics
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_devstats(void)
{
	char *dev_stats = NULL;
	int status = EXIT_FAILURE;

	buffer_size = XGE_COUNT_INTRSTAT * sizeof(u32);

	dev_stats = (char *)malloc(buffer_size);
	if(!dev_stats) {
	    printf("Allocating memory for device statistics failed\n");
	    goto _exit;
	}

	*dev_stats = XGE_QUERY_DEVSTATS;
	ifreqp.ifr_data = (caddr_t)dev_stats;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting device statistics failed\n");
	    goto _exit1;
	}

	xge_print_devstats(dev_stats);
	status = EXIT_SUCCESS;

_exit1:
	free(dev_stats);

_exit:
	return status;
}

/**
 * xge_get_swstats
 * Gets software statistics
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_swstats(void)
{
	char *sw_stats = NULL;
	int status = EXIT_FAILURE;

	buffer_size = XGE_COUNT_SWSTAT * sizeof(u32);

	sw_stats = (char *) malloc(buffer_size);
	if(!sw_stats) {
	    printf("Allocating memory for software statistics failed\n");
	    goto _exit;
	}

	*sw_stats = XGE_QUERY_SWSTATS;
	ifreqp.ifr_data = (caddr_t)sw_stats;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting software statistics failed\n");
	    goto _exit1;
	}

	xge_print_swstats(sw_stats);
	status = EXIT_SUCCESS;

_exit1:
	free(sw_stats);

_exit:
	return status;
}

/**
 * xge_get_drv_version
 * Gets driver version
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_drv_version(void)
{
	char  *version = NULL;
	int status = EXIT_FAILURE;

	buffer_size = 20;
	version = (char *)malloc(buffer_size);
	if(!version) {
	    printf("Allocating memory for driver version failed\n");
	    goto _exit;
	}

	*version = XGE_READ_VERSION;
	ifreqp.ifr_data = ( caddr_t )version;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting driver version failed\n");
	    goto _exit1;
	}
	xge_print_drv_version(version);
	status = EXIT_SUCCESS;

_exit1:
	free(version);

_exit:
	return status;
}

/**
 * xge_get_drvstats
 * Gets driver statistics
 *
 * Returns EXIT_SUCCESS or EXIT_FAILURE
 */
int
xge_get_drvstats(void)
{
	char *driver_stats = NULL;
	int status = EXIT_FAILURE;

	buffer_size = XGE_COUNT_DRIVERSTATS * sizeof(u64);

	driver_stats = (char *)malloc(buffer_size);
	if(!driver_stats) {
	    printf("Allocating memory for driver statistics failed\n");
	    goto _exit;
	}

	*driver_stats = XGE_QUERY_DRIVERSTATS;
	ifreqp.ifr_data = (caddr_t)driver_stats;

	if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0) {
	    printf("Getting Driver Statistics failed\n");
	    goto _exit1;
	}

	xge_print_drvstats(driver_stats);
	status = EXIT_SUCCESS;

_exit1:
	free(driver_stats);

_exit:
	return status;
}
