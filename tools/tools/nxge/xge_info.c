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
/******************************************
 *  xge_info.c
 *
 *  To get the Tx, Rx, PCI, Interrupt statistics,
 *  PCI configuration space and bar0 register
 *  values
 ******************************************/
#include "xge_info.h"

int
main( int argc, char *argv[] )
{
    if(argc >= 4) {
	if(!((strcmp(argv[2], "-r")          == 0) ||
	     (strcmp(argv[2], "-w")          == 0) ||
             (strcmp(argv[2], "chgbufmode")  == 0)))
	      { goto use; }
      }
    else {
   
	if(argc != 3) { goto out; }
	
	else
	  {
	    if(!((strcmp(argv[2], "stats")         == 0) ||
		 (strcmp(argv[2], "pciconf")       == 0) ||
		 (strcmp(argv[2], "devconf")       == 0) ||
		 (strcmp(argv[2], "reginfo")       == 0) ||
		 (strcmp(argv[2], "driverversion") == 0) ||
		 (strcmp(argv[2], "swstats")	   == 0) ||
                 (strcmp(argv[2], "getbufmode")    == 0) ||
		 (strcmp(argv[2], "intr")          == 0)))
		  { goto out; }
	  }
      }

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      {
	printf("Creating socket failed\n");
	return EXIT_FAILURE;
      }

    ifreqp.ifr_addr.sa_family = AF_INET;
    strcpy(ifreqp.ifr_name, argv[1]);

    if     (strcmp(argv[2], "pciconf")       == 0) return getPciConf();
    else if(strcmp(argv[2], "devconf")       == 0) return getDevConf();
    else if(strcmp(argv[2], "stats")         == 0) return getStats();
    else if(strcmp(argv[2], "reginfo")       == 0) return getRegInfo();
    else if(strcmp(argv[2], "intr")          == 0) return getIntrStats();
    else if(strcmp(argv[2], "swstats")       == 0) return getTcodeStats();
    else if(strcmp(argv[2], "driverversion") == 0) return getDriverVer();
    else if(strcmp(argv[2], "-r")            == 0) return getReadReg(argv[2],
    argv[3]);
    else if(strcmp(argv[2], "-w")            == 0) return getWriteReg(argv[2],
    argv[3],argv[5]); 
    else if(strcmp(argv[2], "chgbufmode") == 0) return changeBufMode(argv[3]);
    else if(strcmp(argv[2], "getbufmode") == 0) return getBufMode();
    else return EXIT_FAILURE;

use:
    printf("Usage:");
    printf("%s <INTERFACE> [-r] [-w] [chgbufmode]\n", argv[0]);
    printf("\t -r <offset>               : Read register  \n");
    printf("\t -w <offset> -v <value>    : Write register \n");
    printf("\t chgbufmode <Buffer mode>  : Changes buffer mode \n");
    return EXIT_FAILURE;

out:
    printf("Usage:");
    printf("%s <INTERFACE> <[stats] [reginfo] [pciconf] [devconf] ", argv[0]);
    printf("[intr] [swstats] [driverversion] ");
    printf("[getbufmode] [chgbufmode] [-r] [-w] >\n");
    printf("\tINTERFACE               : Interface (xge0, xge1, xge2, ..)\n");
    printf("\tstats                   : Prints statistics               \n");
    printf("\treginfo                 : Prints register values          \n");
    printf("\tpciconf                 : Prints PCI configuration space  \n");
    printf("\tdevconf                 : Prints device configuration     \n");
    printf("\tintr                    : Prints interrupt statistics     \n");
    printf("\tswstats                 : Prints sw statistics            \n");
    printf("\tdriverversion           : Prints driver version           \n");
    printf("\tgetbufmode              : Prints Buffer Mode              \n");
    printf("\tchgbufmode              : Changes buffer mode             \n");
    printf("\t -r <offset>            : Read register                   \n");
    printf("\t -w <offset> -v <value> : Write register                  \n");
    return EXIT_FAILURE;
}

int
getStats()
{
    void *hw_stats;
    void *pci_cfg;
    unsigned short device_id;
    int index    = 0;
    bufferSize = GET_OFFSET_STATS(XGE_COUNT_STATS - 1) + 8;

    hw_stats   = (void *) malloc(bufferSize);
    if(!hw_stats)
    {
        printf("Allocating memory for hw_stats failed\n");
        return EXIT_FAILURE;
    }
    pAccess         = (char *)hw_stats;
    *pAccess        = XGE_QUERY_STATS;
    ifreqp.ifr_data = (caddr_t) hw_stats;

    if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0)
    {
        printf("Getting hardware statistics failed\n");
        free(hw_stats);
        return EXIT_FAILURE;
    }
    bufferSize = GET_OFFSET_PCICONF(XGE_COUNT_PCICONF -1) + 8;

    pci_cfg = (void *) malloc(bufferSize);
    if(!pci_cfg)
    {
        printf("Allocating memory for pci_cfg  failed\n");
        return EXIT_FAILURE;
    }

    pAccess         = (char *)pci_cfg;
    *pAccess        = XGE_QUERY_PCICONF;
    ifreqp.ifr_data = (caddr_t)pci_cfg;

    if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0)
    {
        printf("Getting pci configuration space failed\n");
        free(pci_cfg);
        return EXIT_FAILURE;
    }
    device_id = *( ( u16 * )( ( unsigned char * )pci_cfg +
                GET_OFFSET_PCICONF(index) ) );
     
    logStats( hw_stats,device_id );
    free(hw_stats);
    free(pci_cfg);
    return EXIT_SUCCESS;
}

int
getPciConf()
{
    void *pci_cfg;

    indexer = 0;
    bufferSize = GET_OFFSET_PCICONF(XGE_COUNT_PCICONF -1) + 8;

    pci_cfg = (void *) malloc(bufferSize);
    if(!pci_cfg)
    {
        printf("Allocating memory for pci_cfg  failed\n");
        return EXIT_FAILURE;
    }

    pAccess         = (char *)pci_cfg;
    *pAccess        = XGE_QUERY_PCICONF;
    ifreqp.ifr_data = (caddr_t)pci_cfg;

    if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0)
    {
        printf("Getting pci configuration space failed\n");
        free(pci_cfg);
        return EXIT_FAILURE;
    }

    logPciConf( pci_cfg );
    free(pci_cfg);
    return EXIT_SUCCESS;
}

int
getDevConf()
{
    void *device_cfg;

    indexer    = 0;
    bufferSize = XGE_COUNT_DEVCONF * sizeof(int);

    device_cfg = (void *) malloc(bufferSize);
    if(!device_cfg)
    {
        printf("Allocating memory for device_cfg  failed\n");
        return EXIT_FAILURE;
    }
    pAccess         = (char *)device_cfg;
    *pAccess        = XGE_QUERY_DEVCONF;
    ifreqp.ifr_data = (caddr_t)device_cfg;

    if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0)
    {
        printf("Getting Device Configuration failed\n");
        free(device_cfg);
        return EXIT_FAILURE;
    }

    logDevConf( device_cfg );
    free(device_cfg);
    return EXIT_SUCCESS;
}

int
getBufMode()
{
    void *buf_mode = 0;

    buf_mode = (void *) malloc(sizeof(int));
    if(!buf_mode)
    {
        printf("Allocating memory for Buffer mode parameter  failed\n");
        return EXIT_FAILURE;
    }

    pAccess         = (char *)buf_mode;
    *pAccess        = XGE_QUERY_BUFFER_MODE;
    ifreqp.ifr_data = (void *)buf_mode;

    if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0)
    {
        printf("Getting Buffer Mode failed\n");
        free(buf_mode);
        return EXIT_FAILURE;
    }
    printf("Buffer Mode is %d\n", *ifreqp.ifr_data);
    free(buf_mode);
    return EXIT_SUCCESS;
}


int
changeBufMode(char *bufmode)
{

    char *print_msg;
    pAccess = (char *)malloc(sizeof(char));

    if(*bufmode == '1'){
         *pAccess         =  XGE_SET_BUFFER_MODE_1;
    }else if (*bufmode == '2'){
         *pAccess         =  XGE_SET_BUFFER_MODE_2;
    }else if (*bufmode == '3'){
         *pAccess         =  XGE_SET_BUFFER_MODE_3;
    }else if (*bufmode == '5'){
         *pAccess         =  XGE_SET_BUFFER_MODE_5;
    }else{
         printf("Invalid Buffer mode\n");
         return EXIT_FAILURE;
     }

    ifreqp.ifr_data = (char *)pAccess;
    if( ioctl( sockfd, SIOCGPRIVATE_0, &ifreqp ) < 0 )
    {
        printf( "Changing Buffer Mode Failed\n" );
        return EXIT_FAILURE;
    }
    print_msg = (char *)ifreqp.ifr_data;
    if(*print_msg == 'Y')
        printf("Requested buffer mode was already enabled\n");
    else if(*print_msg == 'N')
        printf("Requested buffer mode is not implemented OR\nDynamic buffer changing is not supported in this driver\n");
    else if(*print_msg == 'C')
        printf("Buffer mode changed to %c\n", *bufmode);

    return EXIT_SUCCESS;
}


int
getRegInfo()
{
    void *regBuffer;

    indexer     = 0;
    bufferSize = regInfo[XGE_COUNT_REGS - 1].offset + 8;

    regBuffer = ( void * ) malloc ( bufferSize );
    if( !regBuffer )
    {
        printf( "Allocating memory for register dump failed\n" );
        return EXIT_FAILURE;
    }
    
    ifreqp.ifr_data = ( caddr_t )regBuffer;
    if( ioctl( sockfd, SIOCGPRIVATE_1, &ifreqp ) < 0 )
    {
        printf( "Getting register dump failed\n" );
    	free( regBuffer );
        return EXIT_FAILURE;
    }

    logRegInfo( regBuffer );
    free( regBuffer );
    return EXIT_SUCCESS;
}

int 
getReadReg(char *opt,char *offst)
{
    bar0reg_t *reg;
    
    reg = ( bar0reg_t * ) malloc (sizeof(bar0reg_t));
    if( !reg )
    {
        printf( "Allocating memory for reading register  failed\n" );
        return EXIT_FAILURE;
    }
    strcpy(reg->option, opt); 
    sscanf(offst,"%x",&reg->offset);
    ifreqp.ifr_data = ( caddr_t )reg;
    if( ioctl( sockfd, SIOCGPRIVATE_1, &ifreqp ) < 0 )
    {
        printf( "Reading register failed\n" );
    	free(reg);
        return EXIT_FAILURE;
    }
    logReadReg ( reg->offset,reg->value );
    free(reg);
    return EXIT_SUCCESS;
}


int
getWriteReg(char *opt,char *offst,char *val)
{
    bar0reg_t *reg;
    
    reg = ( bar0reg_t * ) malloc (sizeof(bar0reg_t));
    if( !reg )
    {
        printf( "Allocating memory for writing  register  failed\n" );
        return EXIT_FAILURE;
    }
    strcpy(reg->option, opt);
    sscanf(offst,"%x",&reg->offset);
    sscanf(val,"%llx",&reg->value);
    ifreqp.ifr_data = ( caddr_t )reg;
    if( ioctl( sockfd, SIOCGPRIVATE_1, &ifreqp ) < 0 )
    {
        printf( "Writing register failed\n" );
    	free(reg);
        return EXIT_FAILURE;
    }
    free(reg);
    return EXIT_SUCCESS;
}


int
getIntrStats()
{
    void *intr_stat;

    bufferSize = XGE_COUNT_INTRSTAT * sizeof(u32);

    intr_stat = (void *) malloc(bufferSize);
    if(!intr_stat)
    {
        printf("Allocating memory for intr_stat failed\n");
        return EXIT_FAILURE;
    }
    pAccess = (char *)intr_stat;
    *pAccess = XGE_QUERY_INTRSTATS ;
    ifreqp.ifr_data = (caddr_t)intr_stat;

    if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0)
    {
        printf("Getting interrupt statistics failed\n");
    	free(intr_stat);
        return EXIT_FAILURE;
    }
    intr_stat = (char *)ifreqp.ifr_data;

    logIntrStats( intr_stat );
    free(intr_stat);
    return EXIT_SUCCESS;
}

int
getTcodeStats()
{
    void *tcode_stat;

    bufferSize = XGE_COUNT_TCODESTAT * sizeof(u32);

    tcode_stat = (void *) malloc(bufferSize);
    if(!tcode_stat)
    {
        printf("Allocating memory for tcode_stat failed\n");
        return EXIT_FAILURE;
    }
    pAccess = (char *)tcode_stat;
    *pAccess = XGE_QUERY_TCODE ;
    ifreqp.ifr_data = (caddr_t)tcode_stat;
    if(ioctl(sockfd, SIOCGPRIVATE_0, &ifreqp) < 0)
    {
        printf("Getting tcode statistics failed\n");
        free(tcode_stat);
        return EXIT_FAILURE;
    }
    tcode_stat = (char *)ifreqp.ifr_data;

    logTcodeStats( tcode_stat );
    free(tcode_stat);
    return EXIT_SUCCESS;
}

int
getDriverVer()
{
    char  *version;
    bufferSize = 20;
    version = ( char * ) malloc ( bufferSize );
    if( !version )
    {
        printf( "Allocating memory for getting driver version failed\n" );
        return EXIT_FAILURE;
    }
     pAccess         = version;
    *pAccess         = XGE_READ_VERSION;

    ifreqp.ifr_data = ( caddr_t )version;
    if( ioctl( sockfd, SIOCGPRIVATE_0, &ifreqp ) < 0 )
    {
        printf( "Getting driver version failed\n" );
        free( version );
        return EXIT_FAILURE;
    }
    logDriverInfo(version);
    free( version );
    return EXIT_SUCCESS;

}

