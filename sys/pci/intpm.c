/*-
 * Copyright (c) 1998, 1999 Takanori Watanabe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.    IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: intpm.c,v 1.4 1999/01/28 00:57:53 dillon Exp $
 */

#include "pci.h"
#include "intpm.h"

#if NPCI > 0
#if NINTPM >0
/* I don't think the chip is used in other architecture. :-)*/
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>

#include <machine/clock.h>
#include <sys/uio.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/buf.h>

#include <dev/smbus/smbconf.h>

#include "smbus_if.h"

/*This should be removed if force_pci_map_int supported*/
#include <sys/interrupt.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <pci/intpmreg.h>

#include "opt_intpm.h"

static struct _pcsid
{
        pcidi_t type;
	char	*desc;
} pci_ids[] =
{
	{ 0x71138086,"Intel 82371AB Power management controller"},
	
	{ 0x00000000,	NULL					}
};
static int intsmb_probe(device_t);
static int intsmb_attach(device_t);
static void intsmb_print_child(device_t, device_t);

static int intsmb_intr(device_t dev);
static int intsmb_slvintr(device_t dev);
static void  intsmb_alrintr(device_t dev);
static int intsmb_callback(device_t dev, int index, caddr_t data);
static int intsmb_quick(device_t dev, u_char slave, int how);
static int intsmb_sendb(device_t dev, u_char slave, char byte);
static int intsmb_recvb(device_t dev, u_char slave, char *byte);
static int intsmb_writeb(device_t dev, u_char slave, char cmd, char byte);
static int intsmb_writew(device_t dev, u_char slave, char cmd, short word);
static int intsmb_readb(device_t dev, u_char slave, char cmd, char *byte);
static int intsmb_readw(device_t dev, u_char slave, char cmd, short *word);
static int intsmb_pcall(device_t dev, u_char slave, char cmd, short sdata, short *rdata);
static int intsmb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf);
static int intsmb_bread(device_t dev, u_char slave, char cmd, u_char count, char *buf);
static void intsmb_start(device_t dev,u_char cmd,int nointr);
static int intsmb_stop(device_t dev);
static int intsmb_stop_poll(device_t dev);
static int intsmb_free(device_t dev);
static struct intpm_pci_softc *intpm_alloc(int unit);
static const char* intpm_probe __P((pcici_t tag, pcidi_t type));
static void intpm_attach __P((pcici_t config_id, int unit));
static devclass_t intsmb_devclass;

static device_method_t intpm_methods[]={
        DEVMETHOD(device_probe,intsmb_probe),
        DEVMETHOD(device_attach,intsmb_attach),

        DEVMETHOD(bus_print_child, intsmb_print_child),
        
        DEVMETHOD(smbus_callback,intsmb_callback),
        DEVMETHOD(smbus_quick,intsmb_quick),
        DEVMETHOD(smbus_sendb,intsmb_sendb),
        DEVMETHOD(smbus_recvb,intsmb_recvb),
        DEVMETHOD(smbus_writeb,intsmb_writeb),
        DEVMETHOD(smbus_writew,intsmb_writew),
        DEVMETHOD(smbus_readb,intsmb_readb),
        DEVMETHOD(smbus_readw,intsmb_readw),
        DEVMETHOD(smbus_pcall,intsmb_pcall),
        DEVMETHOD(smbus_bwrite,intsmb_bwrite),
        DEVMETHOD(smbus_bread,intsmb_bread),
        {0,0}
};

static struct intpm_pci_softc{
        bus_space_tag_t smbst;
        bus_space_handle_t smbsh;
	bus_space_tag_t pmst;
	bus_space_handle_t pmsh;
        pcici_t cfg;
	device_t  smbus;
}intpm_pci[NINTPM];


struct intsmb_softc{
        struct intpm_pci_softc *pci_sc;
        bus_space_tag_t st;
        bus_space_handle_t sh;
        device_t smbus;
        int isbusy;
};
static driver_t intpm_driver = {
        "intsmb",
        intpm_methods,
        DRIVER_TYPE_MISC,
        sizeof(struct intsmb_softc),
};
static u_long intpm_count ;

static struct	pci_device intpm_device = {
	"intpm",
 	intpm_probe,
	intpm_attach,
	&intpm_count
};

DATA_SET (pcidevice_set, intpm_device);

static int 
intsmb_probe(device_t dev)
{
        struct intsmb_softc *sc =(struct intsmb_softc *) device_get_softc(dev);
        sc->smbus=smbus_alloc_bus(dev);
        if (!sc->smbus)
                return (EINVAL);    /* XXX don't know what to return else */
        device_set_desc(dev,"Intel PIIX4 SMBUS Interface");
        
        return (0);          /* XXX don't know what to return else */
}
static int
intsmb_attach(device_t dev)
{
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        sc->pci_sc=&intpm_pci[device_get_unit(dev)];
        sc->isbusy=0;
	sc->sh=sc->pci_sc->smbsh;
	sc->st=sc->pci_sc->smbst;
	sc->pci_sc->smbus=dev;
        device_probe_and_attach(sc->smbus);
#ifdef ENABLE_ALART
	/*Enable Arart*/
	bus_space_write_1(sc->st,sc->sh,PIIX4_SMBSLVCNT,
			  PIIX4_SMBSLVCNT_ALTEN);
#endif 
        return (0);
}

static void
intsmb_print_child(device_t bus, device_t dev)
{
	printf(" on %s%d", device_get_name(bus), device_get_unit(bus));
	return;
}
static int 
intsmb_callback(device_t dev, int index, caddr_t data)
{
	int error = 0;
	intrmask_t s;
	s=splnet();
	switch (index) {
	case SMB_REQUEST_BUS:
		break;
	case SMB_RELEASE_BUS:
		break;
	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}
/*counterpart of smbtx_smb_free*/
static        int
intsmb_free(device_t dev){
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        if((bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTSTS)&
	    PIIX4_SMBHSTSTAT_BUSY)
#ifdef ENABLE_ALART
	   ||(bus_space_read_1(sc->st,sc->sh,PIIX4_SMBSLVSTS)&
	      PIIX4_SMBSLVSTS_BUSY)
#endif
	   || sc->isbusy)
                return EBUSY;
        sc->isbusy=1;
	/*Disable Intrrupt in slave part*/
#ifndef ENABLE_ALART
	bus_space_write_1(sc->st,sc->sh,PIIX4_SMBSLVCNT,0);
#endif
        /*Reset INTR Flag to prepare INTR*/
	bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTSTS,
			  (PIIX4_SMBHSTSTAT_INTR|
			   PIIX4_SMBHSTSTAT_ERR|
			   PIIX4_SMBHSTSTAT_BUSC|
			   PIIX4_SMBHSTSTAT_FAIL)
		);
        return 0;
}

static int
intsmb_intr(device_t dev)
{
	struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
	int status;
	intrmask_t s;
	status=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTSTS);
	if(status&PIIX4_SMBHSTSTAT_BUSY){
		return 1;
		
	}
	s=splhigh();
	if(sc->isbusy&&(status&(PIIX4_SMBHSTSTAT_INTR|
				PIIX4_SMBHSTSTAT_ERR|
				PIIX4_SMBHSTSTAT_BUSC|
				PIIX4_SMBHSTSTAT_FAIL))){
		int tmp;
		sc->isbusy=0;
		tmp=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTCNT);
		bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCNT,
				  tmp&~PIIX4_SMBHSTCNT_INTREN);
		splx(s);
		wakeup(sc);
		return 0;
	}
	splx(s);
	return 1;/* Not Completed*/
}
static int
intsmb_slvintr(device_t dev)
{
	struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        int status,retval;
	retval=1;
        status=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBSLVSTS);
	if(status&PIIX4_SMBSLVSTS_BUSY)
		return retval;
	if(status&PIIX4_SMBSLVSTS_ALART){
		intsmb_alrintr(dev);
		retval=0;
	}else if(status&~(PIIX4_SMBSLVSTS_ALART|PIIX4_SMBSLVSTS_SDW2
			  |PIIX4_SMBSLVSTS_SDW1)){
		retval=0;
	}
	/*Reset Status Register*/
	bus_space_write_1(sc->st,sc->sh,PIIX4_SMBSLVSTS,PIIX4_SMBSLVSTS_ALART|
			  PIIX4_SMBSLVSTS_SDW2|PIIX4_SMBSLVSTS_SDW1|
			  PIIX4_SMBSLVSTS_SLV);
	return retval;
}

static void intsmb_alrintr(device_t dev)
{
	struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
	int slvcnt;
#ifdef ENABLE_ALART
	int error;
#endif

	/*stop generating INTR from ALART*/
	slvcnt=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBSLVCNT);
#ifdef ENABLE_ALART
	bus_space_write_1(sc->st,sc->sh,PIIX4_SMBSLVCNT,
			  slvcnt&~PIIX4_SMBSLVCNT_ALTEN) ;
#endif
	DELAY(5);
	/*ask bus who assert it and then ask it what's the matter. */	
#ifdef ENABLE_ALART
	error=intsmb_free(dev);
	if(!error){
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,SMBALTRESP
                                  |LSB);
		intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_BYTE,1);
		if(!(error=intsmb_stop_poll(dev))){
			volatile u_int8_t *addr;
			addr=bus_space_read_1(sc->st,sc->sh,
					      PIIX4_SMBHSTDAT0);
			printf("ALART_RESPONSE: %p\n", addr);
		}
	}else{
	        printf("ERROR\n");
	}

	/*Re-enable INTR from ALART*/
	bus_space_write_1(sc->st,sc->sh,PIIX4_SMBSLVCNT,
			  slvcnt|PIIX4_SMBSLVCNT_ALTEN) ;
	DELAY(5);
#endif

	return;
}
static void
intsmb_start(device_t dev,unsigned char cmd,int nointr)
{
	struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
	unsigned char tmp;
	tmp=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTCNT);
	tmp&= 0xe0;
	tmp |= cmd;
	tmp |=PIIX4_SMBHSTCNT_START;
	/*While not in autoconfiguration Intrrupt Enabled*/
	if(!cold||!nointr)
		tmp |=PIIX4_SMBHSTCNT_INTREN;
	bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCNT,tmp);
}

/*Polling Code. Polling is not encouraged 
 * because It is required to wait for the device get busy.
 *(29063505.pdf from Intel)
 * But during boot,intrrupt cannot be used.
 * so use polling code while in autoconfiguration.
 */

static        int
intsmb_stop_poll(device_t dev){
        int error,i;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
	/*
	 *  In smbtx driver ,Simply waiting.
	 *  This loops 100-200 times.
	 */
	for(i=0;i<0x7fff;i++){
                if((bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTSTS)
		    &PIIX4_SMBHSTSTAT_BUSY)){
                        break;
                }
	}
	for(i=0;i<0x7fff;i++){
		int status;
		status=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTSTS);
		if(!(status&PIIX4_SMBHSTSTAT_BUSY)){
			sc->isbusy=0;
			error=(status&PIIX4_SMBHSTSTAT_ERR)?EIO :
				(status&PIIX4_SMBHSTSTAT_BUSC)?EBUSY:
				(status&PIIX4_SMBHSTSTAT_FAIL)?EIO:0;
			if(error==0&&!(status&PIIX4_SMBHSTSTAT_INTR)){
				printf("unknown cause why?");
			}
			return error;
		}
	}
	sc->isbusy=0;
	return EIO;
}
/*
 *wait for completion and return result.
 */
static        int
intsmb_stop(device_t dev){
        int error;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
	if(cold){
		/*So that it can use device during probing device on SMBus.*/
		error=intsmb_stop_poll(dev);
		return error;
	}else{
		if(!tsleep(sc,(PWAIT)|PCATCH,"SMBWAI",hz/8)){
			int status;
			status=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTSTS);
			if(!(status&PIIX4_SMBHSTSTAT_BUSY)){
				error=(status&PIIX4_SMBHSTSTAT_ERR)?EIO :
					(status&PIIX4_SMBHSTSTAT_BUSC)?EBUSY:
					(status&PIIX4_SMBHSTSTAT_FAIL)?EIO:0;
				if(error==0&&!(status&PIIX4_SMBHSTSTAT_INTR)){
					printf("intsmb%d:unknown cause why?\n",
					       device_get_unit(dev));
				}
#ifdef ENABLE_ALART
				bus_space_write_1(sc->st,sc->sh,
						  PIIX4_SMBSLVCNT,PIIX4_SMBSLVCNT_ALTEN);
#endif
				return error;
			}
		}
	}
	/*Timeout Procedure*/
	sc->isbusy=0;
	/*Re-enable supressed intrrupt from slave part*/
	bus_space_write_1(sc->st,sc->sh,
			  PIIX4_SMBSLVCNT,PIIX4_SMBSLVCNT_ALTEN);
        return EIO;
}

static int
intsmb_quick(device_t dev, u_char slave, int how)
{
        int error=0;
        u_char data;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        data=slave;
	/*Quick command is part of Address, I think*/
        switch(how){
        case SMB_QWRITE:
                data&=~LSB;
		break;
        case SMB_QREAD:
                data|=LSB;
                break;
        default:
                error=EINVAL;
        }
        if(!error){
	        error=intsmb_free(dev);
                if(!error){
                        bus_space_write_1(sc->st,sc->sh,
					  PIIX4_SMBHSTADD,data);
			intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_QUICK,0);
                        error=intsmb_stop(dev);
                }
        }

        return (error);
}

static int
intsmb_sendb(device_t dev, u_char slave, char byte)
{
        int error;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(!error){
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave&~LSB);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCMD,byte);
		intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_BYTE,0);
                error=intsmb_stop(dev);
        }
        return (error);
}
static int
intsmb_recvb(device_t dev, u_char slave, char *byte)
{
        int error;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(!error){
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave
				  |LSB);
                intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_BYTE,0);
                if(!(error=intsmb_stop(dev))){
#ifdef RECV_IS_IN_CMD
		        /*Linux SMBus stuff also troubles
			  Because Intel's datasheet will not make clear.
			 */
                        *byte=bus_space_read_1(sc->st,sc->sh,
					       PIIX4_SMBHSTCMD);
#else
                        *byte=bus_space_read_1(sc->st,sc->sh,
					       PIIX4_SMBHSTDAT0);
#endif
                }
        }
        return (error);
}
static int
intsmb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
        int error;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(!error){
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave&~LSB);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCMD,cmd);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTDAT0,byte);
		intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_BDATA,0);
                error=intsmb_stop(dev);
        }
        return (error);
}
static int
intsmb_writew(device_t dev, u_char slave, char cmd, short word)
{
        int error;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(!error){
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave&~LSB);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCMD,cmd);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTDAT0,
				  word&0xff);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTDAT1,
				  (word>>8)&0xff);
		intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_WDATA,0);
                error=intsmb_stop(dev);
        }
        return (error);
}

static int
intsmb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
        int error;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(!error){
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave|LSB);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCMD,cmd);
		intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_BDATA,0);
                if(!(error=intsmb_stop(dev))){
		        *byte=bus_space_read_1(sc->st,sc->sh,
					       PIIX4_SMBHSTDAT0);
                }
        }
        return (error);
}
static int
intsmb_readw(device_t dev, u_char slave, char cmd, short *word)
{
        int error;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(!error){
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave|LSB);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCMD,cmd);
		intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_WDATA,0);
                if(!(error=intsmb_stop(dev))){
                        *word=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTDAT0)&0xff;
                        *word|=(bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTDAT1)&0xff)<<8;
                }
        }
        return (error);
}
/*
 * Data sheet claims that it implements all function, but also claims
 * that it implements 7 function and not mention PCALL. So I don't know
 * whether it will work.
 */
static int
intsmb_pcall(device_t dev, u_char slave, char cmd, short sdata, short *rdata)
{
#ifdef PROCCALL_TEST
        int error;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(!error){
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave&~LSB);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCMD,cmd);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTDAT0,sdata&0xff);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTDAT1,(sdata&0xff)>>8);
                intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_WDATA,0);
        }
        if(!(error=intsmb_stop(dev))){
                *rdata=bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTDAT0)&0xff;
                *rdata|=(bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTDAT1)&0xff)<<8;
        }
        return error;
#else
	return 0;
#endif
}
static int
intsmb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
        int error,i;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(count>SMBBLOCKTRANS_MAX||count==0)
                error=EINVAL;
        if(!error){
                /*Reset internal array index*/
                bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTCNT);
		
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave&~LSB);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCMD,cmd);
                for(i=0;i<count;i++){
                        bus_space_write_1(sc->st,sc->sh,PIIX4_SMBBLKDAT,buf[i]);
                }
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTDAT0,count);
                intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_BLOCK,0);
                error=intsmb_stop(dev);
        }
        return (error);
}

static int
intsmb_bread(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
        int error,i;
        struct intsmb_softc *sc = (struct intsmb_softc *)device_get_softc(dev);
        error=intsmb_free(dev);
        if(count>SMBBLOCKTRANS_MAX||count==0)
                error=EINVAL;
        if(!error){
                /*Reset internal array index*/
                bus_space_read_1(sc->st,sc->sh,PIIX4_SMBHSTCNT);
		
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTADD,slave|LSB);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTCMD,cmd);
                bus_space_write_1(sc->st,sc->sh,PIIX4_SMBHSTDAT0,count);
                intsmb_start(dev,PIIX4_SMBHSTCNT_PROT_BLOCK,0);
                error=intsmb_stop(dev);
                if(!error){
                        bzero(buf,count);/*Is it needed?*/
                        count= bus_space_read_1(sc->st,sc->sh,
						PIIX4_SMBHSTDAT0);
                        if(count!=0&&count<=SMBBLOCKTRANS_MAX){
			        for(i=0;i<count;i++){
				        buf[i]=bus_space_read_1(sc->st,
								sc->sh,
								PIIX4_SMBBLKDAT);
				}
			}
                        else{
				error=EIO;
                        }
		}
	}
        return (error);
}

DRIVER_MODULE(intsmb, root , intpm_driver, intsmb_devclass, 0, 0);


static void intpm_intr __P((void *arg));

static const char*
intpm_probe (pcici_t tag, pcidi_t type)
{
        struct _pcsid	*ep =pci_ids;
        while (ep->type && ep->type != type)
                ++ep;
        return (ep->desc);
}

static struct intpm_pci_softc *intpm_alloc(int unit){
        if(unit<NINTPM)
                return &intpm_pci[unit];
        else
                return NULL;
}

/*Same as pci_map_int but this ignores INTPIN*/
static int force_pci_map_int(pcici_t cfg, pci_inthand_t *func, void *arg, unsigned *maskptr)
{
        int error;
#ifdef APIC_IO
        int nextpin, muxcnt;
#endif
	/* Spec sheet claims that it use IRQ 9*/
        int irq = 9;
        void *idesc;
        
        idesc = inthand_add(NULL, irq, func, arg, maskptr, 0);
        if (idesc == 0)
                return 0;
#ifdef APIC_IO
        nextpin = next_apic_irq(irq);
        
        if (nextpin < 0)
                return 1;
        
        /* 
         * Attempt handling of some broken mp tables.
         *
         * It's OK to yell (since the mp tables are broken).
         * 
         * Hanging in the boot is not OK
         */
        
        muxcnt = 2;
        nextpin = next_apic_irq(nextpin);
        while (muxcnt < 5 && nextpin >= 0) {
                muxcnt++;
                nextpin = next_apic_irq(nextpin);
        }
        if (muxcnt >= 5) {
                printf("bogus MP table, more than 4 IO APIC pins connected to the same PCI device or ISA/EISA interrupt\n");
                return 0;
        }
        
        printf("bogus MP table, %d IO APIC pins connected to the same PCI device or ISA/EISA interrupt\n", muxcnt);
        
        nextpin = next_apic_irq(irq);
        while (nextpin >= 0) {
                idesc = inthand_add(NULL, nextpin, func, arg, maskptr, 0);
                if (error != 0)
                        return 0;
                printf("Registered extra interrupt handler for int %d (in addition to int %d)\n", nextpin, irq);
                nextpin = next_apic_irq(nextpin);
        }
#endif
        return 1;
}
static void
intpm_attach(config_id, unit)
     pcici_t config_id;
     int	unit;
{
        int value;
        
        char * str;
        {
                struct intpm_pci_softc *sciic;
                device_t smbinterface;
                value=pci_cfgread(config_id,PCI_BASE_ADDR_SMB,4);
                sciic=intpm_alloc(unit);
                if(sciic==NULL){
                        return;
                }

		sciic->smbst=(value&1)?I386_BUS_SPACE_IO:I386_BUS_SPACE_MEM;

		/*Calling pci_map_port is better.But bus_space_handle_t != 
		 * pci_port_t, so I don't call support routine while 
		 * bus_space_??? support routine will be appear.
		 */
                sciic->smbsh=value&(~1);
		if(sciic->smbsh==I386_BUS_SPACE_MEM){
		       /*According to the spec, this will not occur*/
                       int dummy;
		       pci_map_mem(config_id,PCI_BASE_ADDR_SMB,&sciic->smbsh,&dummy);
		}
                printf("intpm%d: %s %x ",unit,
		       (sciic->smbst==I386_BUS_SPACE_IO)?"I/O mapped":"Memory",
		       sciic->smbsh);
#ifndef NO_CHANGE_PCICONF
		pci_cfgwrite(config_id,PCIR_INTLINE,0x09,1);
                pci_cfgwrite(config_id,PCI_HST_CFG_SMB, 
			     PCI_INTR_SMB_IRQ9|PCI_INTR_SMB_ENABLE,1);
#endif
		config_id->intline=pci_cfgread(config_id,PCIR_INTLINE,1);
		printf("ALLOCED IRQ %d ",config_id->intline);
                value=pci_cfgread(config_id,PCI_HST_CFG_SMB,1);
                switch(value&0xe){
                case PCI_INTR_SMB_SMI:
                        str="SMI";
                        break;
                case PCI_INTR_SMB_IRQ9:
                        str="IRQ 9";
                        break;
                default:
                        str="BOGUS";
                }
                printf("intr %s %s ",str,((value&1)? "enabled":"disabled"));
                value=pci_cfgread(config_id,PCI_REVID_SMB,1);
                printf("revision %d\n",value);                
                /*
                 * Install intr HANDLER here
                 */
                if(force_pci_map_int(config_id,intpm_intr,sciic,&net_imask)==0){
                        printf("intpm%d: Failed to map intr\n",unit);
                }
                smbinterface=device_add_child(root_bus,"intsmb",unit,NULL);
                device_probe_and_attach(smbinterface);
        }
        value=pci_cfgread(config_id,PCI_BASE_ADDR_PM,4);
        printf("intpm%d: PM %s %x \n",unit,(value&1)?"I/O mapped":"Memory",value&0xfffe);
        return;
}
static void intpm_intr(void *arg)
{
        struct intpm_pci_softc *sc;
        sc=(struct intpm_pci_softc *)arg;
	intsmb_intr(sc->smbus);
	intsmb_slvintr(sc->smbus);
}
#endif /* NPCI > 0 */
#endif
