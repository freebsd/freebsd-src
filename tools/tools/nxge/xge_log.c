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
#include "xge_log.h"

void
logStats( void *hwStats, unsigned short device_id )
{
    int index = 0;
    int count = 0;
    count = XGE_COUNT_STATS - ((device_id == DEVICE_ID_XFRAME_II) ? 0 : XGE_COUNT_EXTENDED_STATS);
    fdAll = fopen( "stats.log", "w+" );
    if( fdAll )
    {
        XGE_PRINT_HEADER_STATS(fdAll);

        for( index = 0; index < count ; index++ )
        {
            switch( statsInfo[index].type )
            {
                case 2:
                {
                    statsInfo[index].value = 
                        *( ( u16 * )( ( unsigned char * ) hwStats +
                        GET_OFFSET_STATS( index ) ) );
                    break;
                }
                case 4:
                {
                    statsInfo[index].value =
                        *( ( u32 * )( ( unsigned char * ) hwStats +
                        GET_OFFSET_STATS( index ) ) );
                    break;
                }
                case 8:
                {
                    statsInfo[index].value =
                        *( ( u64 * )( ( unsigned char * ) hwStats +
                        GET_OFFSET_STATS( index ) ) );
                    break;
                }
            }

            XGE_PRINT_STATS(fdAll,(const char *) statsInfo[index].name,
                statsInfo[index].value);
        }
        XGE_PRINT_LINE(fdAll);
        fclose(fdAll);
    }
}

void 
logPciConf( void * pciConf )
{
    int index = 0;

    fdAll = fopen( "pciconf.log", "w+" );
    if( fdAll )
    {
        XGE_PRINT_HEADER_PCICONF(fdAll);

        for( index = 0; index < XGE_COUNT_PCICONF; index++ )
        {
            pciconfInfo[index].value =
                *( ( u16 * )( ( unsigned char * )pciConf +
                GET_OFFSET_PCICONF(index) ) );
            XGE_PRINT_PCICONF(fdAll,(const char *) pciconfInfo[index].name,
                GET_OFFSET_PCICONF(index), pciconfInfo[index].value);
        }

        XGE_PRINT_LINE(fdAll);
        fclose(fdAll);
    }
}

void
logDevConf( void * devConf )
{
    int index = 0;

    fdAll = fopen( "devconf.log", "w+" );
    if( fdAll )
    {
        XGE_PRINT_HEADER_DEVCONF(fdAll);

        for( index = 0; index < XGE_COUNT_DEVCONF; index++ )
        {
            devconfInfo[index].value =
                *( ( u32 * )( ( unsigned char * )devConf +
                ( index * ( sizeof( int ) ) ) ) );
            XGE_PRINT_DEVCONF(fdAll,(const char *) devconfInfo[index].name,
                devconfInfo[index].value);
        }

        XGE_PRINT_LINE(fdAll);
        fclose( fdAll );
    }
}

void
logRegInfo( void * regBuffer )
{
    int index = 0;

    fdAll = fopen( "reginfo.log", "w+" );
    if( fdAll )
    {
        XGE_PRINT_HEADER_REGS(fdAll);

        for( index = 0; index < XGE_COUNT_REGS; index++ )
        {
            regInfo[index].value =
                *( ( u64 * )( ( unsigned char * )regBuffer +
                regInfo[index].offset ) );
            XGE_PRINT_REGS(fdAll,(const char *) regInfo[index].name,
                regInfo[index].offset, regInfo[index].value);
        }

        XGE_PRINT_LINE(fdAll);
        fclose(fdAll);
    }
}
void
logReadReg(u64 offset,u64 temp)
{
    int index=0;
    
    fdAll = fopen( "readreg.log", "w+");
    if( fdAll )
    {
	XGE_PRINT_READ_HEADER_REGS(fdAll);
		
	regInfo[index].offset = offset ; 

	regInfo[index].value = temp ;
		
	printf("0x%.8X\t0x%.16llX\n",regInfo[index].offset, regInfo[index].value);
		
	XGE_PRINT_LINE(fdAll);
        fclose(fdAll);
    }
}
void
logIntrStats( void * intrStats )
{
    int index = 0;

    fdAll = fopen( "intrstats.log", "w+" );
    if(fdAll)
    {
        XGE_PRINT_HEADER_STATS(fdAll);

        for( index = 0; index < XGE_COUNT_INTRSTAT; index++ )
        {
            intrInfo[index].value =
                *( ( u32 * )( ( unsigned char * )intrStats +
                ( index * ( sizeof( u32 ) ) ) ) );
            XGE_PRINT_STATS(fdAll,(const char *) intrInfo[index].name,
                intrInfo[index].value);
        }

        XGE_PRINT_LINE(fdAll);
        fclose(fdAll);
    }
}

void
logTcodeStats( void * tcodeStats )
{
    int index = 0;

    fdAll = fopen( "tcodestats.log", "w+" );
    if(fdAll)
    {
        XGE_PRINT_HEADER_STATS(fdAll);

        for( index = 0; index < XGE_COUNT_TCODESTAT; index++ )
        {   
	    if(!(tcodeInfo[index].flag)) 
            {
                switch( tcodeInfo[index].type )
                {
                  case 2:
                  {
                      tcodeInfo[index].value =
                          *( ( u16 * )( ( unsigned char * )tcodeStats +
                          ( index * ( sizeof( u16 ) ) ) ) );
                      break;
                  }
                  case 4:
                  {
                      tcodeInfo[index].value =
                          *( ( u32 * )( ( unsigned char * )tcodeStats +
                          ( index * ( sizeof( u32 ) ) ) ) );
                      break;
                  }
                }
               		    
            XGE_PRINT_STATS(fdAll,(const char *) tcodeInfo[index].name,
                tcodeInfo[index].value);
            }
	}  

        XGE_PRINT_LINE(fdAll);
        fclose(fdAll);
    }
}

void
logDriverInfo( char *version )
{
  fdAll = fopen( "driverinfo.log", "w+");
  if (fdAll)
  {
       XGE_PRINT_LINE(fdAll);
       printf("DRIVER VERSION : %s\n",version);
       XGE_PRINT_LINE(fdAll); 
       fclose(fdAll);
  }
  
}

