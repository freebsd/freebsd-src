/*
 * $FreeBSD$
 *
 * Copyright (c) 2002-2004 David Boggs. <boggs@boggs.palo-alto.ca.us>
 * All rights reserved.
 *
 * BSD License:
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
 * GNU General Public License:
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Description:
 *
 * This is an open-source Unix device driver for PCI-bus WAN interface cards.
 * It sends and receives packets in HDLC frames over synchronous links.
 * A generic PC plus Unix plus some SBE/LMC cards makes an OPEN router.
 * This driver works with FreeBSD, NetBSD, OpenBSD, BSD/OS and Linux.
 * It has been tested on i386 (32-bit little-end), Sparc (64-bit big-end),
 * and Alpha (64-bit little-end) architectures.
 *
 * History and Authors:
 *
 * Ron Crane had the neat idea to use a Fast Ethernet chip as a PCI
 * interface and add an Ethernet-to-HDLC gate array to make a WAN card.
 * David Boggs designed the Ethernet-to-HDLC gate arrays and PC cards.
 * We did this at our company, LAN Media Corporation (LMC).
 * SBE Corp aquired LMC and continues to make the cards.
 *
 * Since the cards use Tulip Ethernet chips, we started with Matt Thomas'
 * ubiquitous "de" driver.  Michael Graff stripped out the Ethernet stuff
 * and added HSSI stuff.  Basil Gunn ported it to Solaris (lost) and
 * Rob Braun ported it to Linux.  Andrew Stanley-Jones added support
 * for three more cards and wrote the first version of lmcconfig.
 * During 2002-5 David Boggs rewrote it and now feels responsible for it.
 *
 * Responsible Individual:
 *
 * Send bug reports and improvements to <boggs@boggs.palo-alto.ca.us>.
 */
#ifdef __FreeBSD__
# include <sys/param.h>	/* OS version */
# define  IFNET 1
# include "opt_inet.h"	/* INET */
# include "opt_inet6.h"	/* INET6 */
# include "opt_netgraph.h" /* NETGRAPH */
# ifdef HAVE_KERNEL_OPTION_HEADERS
# include "opt_device_polling.h" /* DEVICE_POLLING */
# endif
# ifndef NETGRAPH
#  define NETGRAPH 0
# endif
# define  P2P 0		/* not in FreeBSD */
# if (__FreeBSD_version >= 500000)
#  define NSPPP 1	/* No count devices in FreeBSD 5 */
#  include "opt_bpf.h"	/* DEV_BPF */
#  define NBPFILTER DEV_BPF
# else  /* FreeBSD-4 */
# include "sppp.h"	/* NSPPP */
#  include "bpf.h"	/* NBPF */
#  define NBPFILTER NBPF
# endif
# define  GEN_HDLC 0	/* not in FreeBSD */
#
# include <sys/systm.h>
# include <sys/kernel.h>
# include <sys/malloc.h>
# include <sys/mbuf.h>
# include <sys/socket.h>
# include <sys/sockio.h>
# include <sys/module.h>
# include <sys/bus.h>
# include <sys/lock.h>
# include <net/if.h>
# include <net/if_types.h>
# include <net/if_media.h>
# include <net/netisr.h>
# include <machine/bus.h>
# include <machine/resource.h>
# include <machine/clock.h>
# include <sys/rman.h>
# include <vm/vm.h>
# include <vm/pmap.h>
# if (__FreeBSD_version >= 500000)
#  include <sys/mutex.h>
#  include <dev/pci/pcivar.h>
# else /* FreeBSD-4 */
#  include <sys/proc.h>
#  include <pci/pcivar.h>
# endif
# if NETGRAPH
#  include <netgraph/ng_message.h>
#  include <netgraph/netgraph.h>
# endif
# if (INET || INET6)
#  include <netinet/in.h>
#  include <netinet/in_var.h>
# endif
# if NSPPP
#  include <net/if_sppp.h>
# endif
# if NBPFILTER
#  include <net/bpf.h>
# endif
/* and finally... */
# include <dev/lmc/if_lmc.h>
#endif /*__FreeBSD__*/

#ifdef __NetBSD__
# include <sys/param.h>	/* OS version */
# define  IFNET 1
# include "opt_inet.h"	/* INET6, INET */
# define  NETGRAPH 0	/* not in NetBSD */
# include "sppp.h"	/* NSPPP */
# define  P2P 0		/* not in NetBSD */
# include "opt_altq_enabled.h" /* ALTQ */
# include "bpfilter.h"	/* NBPFILTER */
# define  GEN_HDLC 0	/* not in NetBSD */
#
# include <sys/systm.h>
# include <sys/kernel.h>
# include <sys/lkm.h>
# include <sys/mbuf.h>
# include <sys/socket.h>
# include <sys/sockio.h>
# include <sys/device.h>
# include <sys/lock.h>
# include <net/if.h>
# include <net/if_types.h>
# include <net/if_media.h>
# include <net/netisr.h>
# include <machine/bus.h>
# include <machine/intr.h>
# include <dev/pci/pcivar.h>
# if (__NetBSD_Version__ >= 106000000)
#  include <uvm/uvm_extern.h>
# else
#  include <vm/vm.h>
# endif
# if (INET || INET6)
#  include <netinet/in.h>
#  include <netinet/in_var.h>
# endif
# if NSPPP
#  if (__NetBSD_Version__ >= 106000000)
#   include <net/if_spppvar.h>
#  else
#   include <net/if_sppp.h>
#  endif
# endif
# if NBPFILTER
#  include <net/bpf.h>
# endif
/* and finally... */
# include "if_lmc.h"
#endif /*__NetBSD__*/

#ifdef __OpenBSD__
# include <sys/param.h>	/* OS version */
# define  IFNET 1
/* -DINET  is passed on the compiler command line */
/* -DINET6 is passed on the compiler command line */
# define  NETGRAPH 0	/* not in OpenBSD */
# include "sppp.h"	/* NSPPP */
# define  P2P 0		/* not in OpenBSD */
/* -DALTQ  is passed on the compiler command line */
# include "bpfilter.h"	/* NBPFILTER */
# define  GEN_HDLC 0	/* not in OpenBSD */
#
# include <sys/systm.h>
# include <sys/kernel.h>
# include <sys/conf.h>
# include <sys/exec.h>
# include <sys/lkm.h>
# include <sys/mbuf.h>
# include <sys/socket.h>
# include <sys/sockio.h>
# include <sys/device.h>
# include <sys/lock.h>
# include <net/if.h>
# include <net/if_types.h>
# include <net/if_media.h>
# include <net/netisr.h>
# include <machine/bus.h>
# include <machine/intr.h>
# include <dev/pci/pcivar.h>
# if (OpenBSD >= 200206)
#  include <uvm/uvm_extern.h>
# else
#  include <vm/vm.h>
# endif
# if (INET || INET6)
#  include <netinet/in.h>
#  include <netinet/in_var.h>
# endif
# if NSPPP
#  include <net/if_sppp.h>
# endif
# if NBPFILTER
#  include <net/bpf.h>
# endif
/* and finally... */
# include "if_lmc.h"
#endif /*__OpenBSD__*/

#ifdef __bsdi__
# include <sys/param.h>	/* OS version */
# define  IFNET 1
/* -DINET  is passed on the compiler command line */
/* -DINET6 is passed on the compiler command line */
# define  NETGRAPH 0	/* not in BSD/OS */
# define  NSPPP 0	/* not in BSD/OS */
/* -DPPP   is passed on the compiler command line */
/* -DCISCO_HDLC is passed on the compiler command line */
/* -DFR    is passed on the compiler command line */
# if (PPP || CISCO_HDLC || FR)
#  define P2P 1
# else
#  define P2P 0
# endif
# define  ALTQ 0	/* not in BSD/OS */
# include "bpfilter.h"	/* NBPFILTER */
# define  GEN_HDLC 0	/* not in BSD/OS */
#
# include <sys/kernel.h>
# include <sys/malloc.h>
# include <sys/mbuf.h>
# include <sys/socket.h>
# include <sys/sockio.h>
# include <sys/device.h>
# include <sys/lock.h>
# include <net/if.h>
# include <net/if_types.h>
# include <net/if_media.h>
# include <net/netisr.h>
# include <vm/vm.h>
# include <i386/isa/dma.h>
# include <i386/isa/isavar.h>
# include <i386/include/cpu.h>
# include <i386/pci/pci.h>
# if (INET || INET6)
#  include <netinet/in.h>
#  include <netinet/in_var.h>
# endif
# if P2P
#  include <net/if_p2p.h>
#  include <sys/ttycom.h>
# endif
# if NBPFILTER
#  include <net/bpf.h>
# endif
/* and finally... */
# include "if_lmc.h"
#endif /*__bsdi__*/

#ifdef __linux__
# include <linux/config.h>
# if (CONFIG_HDLC || CONFIG_HDLC_MODULE)
#  define GEN_HDLC 1
# else
#  define GEN_HDLC 0
# endif
# define IFNET 0	/* different in Linux */
# define NETGRAPH 0	/* not in Linux */
# define NSPPP 0	/* different in Linux */
# define P2P 0		/* not in Linux */
# define ALTQ 0		/* different in Linux */
# define NBPFILTER 0	/* different in Linux */
#
# include <linux/pci.h>
# include <linux/delay.h>
# include <linux/netdevice.h>
# include <linux/if_arp.h>
# if GEN_HDLC
#  include <linux/hdlc.h>
# endif
/* and finally... */
# include "if_lmc.h"
#endif /* __linux__ */

/* The SROM is a generic 93C46 serial EEPROM (64 words by 16 bits). */
/* Data is set up before the RISING edge of CLK; CLK is parked low. */
static void
shift_srom_bits(softc_t *sc, u_int32_t data, u_int32_t len)
  {
  u_int32_t csr = READ_CSR(TLP_SROM_MII);
  for (; len>0; len--)
    {  /* MSB first */
    if (data & (1<<(len-1)))
      csr |=  TLP_SROM_DIN;	/* DIN setup */
    else
      csr &= ~TLP_SROM_DIN;	/* DIN setup */
    WRITE_CSR(TLP_SROM_MII, csr);
    csr |=  TLP_SROM_CLK;	/* CLK rising edge */
    WRITE_CSR(TLP_SROM_MII, csr);
    csr &= ~TLP_SROM_CLK;	/* CLK falling edge */
    WRITE_CSR(TLP_SROM_MII, csr);
    }
  }

/* Data is sampled on the RISING edge of CLK; CLK is parked low. */
static u_int16_t
read_srom(softc_t *sc, u_int8_t addr)
  {
  int i;
  u_int32_t csr;
  u_int16_t data;

  /* Enable SROM access. */
  csr = (TLP_SROM_SEL | TLP_SROM_RD | TLP_MII_MDOE);
  WRITE_CSR(TLP_SROM_MII, csr);
  /* CS rising edge prepares SROM for a new cycle. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* assert CS */
  shift_srom_bits(sc,  6,   4);		/* issue read cmd */
  shift_srom_bits(sc, addr, 6);		/* issue address */
  for (data=0, i=16; i>=0; i--)		/* read ->17<- bits of data */
    {  /* MSB first */
    csr = READ_CSR(TLP_SROM_MII);	/* DOUT sampled */
    data = (data<<1) | ((csr & TLP_SROM_DOUT) ? 1:0);
    csr |=  TLP_SROM_CLK;		/* CLK rising edge */
    WRITE_CSR(TLP_SROM_MII, csr);
    csr &= ~TLP_SROM_CLK;		/* CLK falling edge */
    WRITE_CSR(TLP_SROM_MII, csr);
    }
  /* Disable SROM access. */
  WRITE_CSR(TLP_SROM_MII, TLP_MII_MDOE);

  return data;
  }

/* The SROM is formatted by the mfgr and should NOT be written! */
/* But lmcconfig can rewrite it in case it gets overwritten somehow. */
/* IOCTL SYSCALL: can sleep. */
static void
write_srom(softc_t *sc, u_int8_t addr, u_int16_t data)
  {
  u_int32_t csr;
  int i;

  /* Enable SROM access. */
  csr = (TLP_SROM_SEL | TLP_SROM_RD | TLP_MII_MDOE);
  WRITE_CSR(TLP_SROM_MII, csr);

  /* Issue write-enable command. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* assert CS */
  shift_srom_bits(sc,  4, 4);		/* issue write enable cmd */
  shift_srom_bits(sc, 63, 6);		/* issue address */
  csr &= ~TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* deassert CS */

  /* Issue erase command. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* assert CS */
  shift_srom_bits(sc, 7, 4);		/* issue erase cmd */
  shift_srom_bits(sc, addr, 6);		/* issue address */
  csr &= ~TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* deassert CS */

  /* Issue write command. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* assert CS */
  for (i=0; i<10; i++)  /* 100 ms max wait */
    if ((READ_CSR(TLP_SROM_MII) & TLP_SROM_DOUT)==0) SLEEP(10000);
  shift_srom_bits(sc, 5, 4);		/* issue write cmd */
  shift_srom_bits(sc, addr, 6);		/* issue address */
  shift_srom_bits(sc, data, 16);	/* issue data */
  csr &= ~TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* deassert CS */

  /* Issue write-disable command. */
  csr |= TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* assert CS */
  for (i=0; i<10; i++)  /* 100 ms max wait */
    if ((READ_CSR(TLP_SROM_MII) & TLP_SROM_DOUT)==0) SLEEP(10000);
  shift_srom_bits(sc, 4, 4);		/* issue write disable cmd */
  shift_srom_bits(sc, 0, 6);		/* issue address */
  csr &= ~TLP_SROM_CS;
  WRITE_CSR(TLP_SROM_MII, csr);	/* deassert CS */

  /* Disable SROM access. */
  WRITE_CSR(TLP_SROM_MII, TLP_MII_MDOE);
  }

/* Not all boards have BIOS roms. */
/* The BIOS ROM is an AMD 29F010 1Mbit (128K by 8) EEPROM. */
static u_int8_t
read_bios(softc_t *sc, u_int32_t addr)
  {
  u_int32_t srom_mii;

  /* Load the BIOS rom address register. */
  WRITE_CSR(TLP_BIOS_ROM, addr);

  /* Enable the BIOS rom. */
  srom_mii = TLP_BIOS_SEL | TLP_BIOS_RD | TLP_MII_MDOE;
  WRITE_CSR(TLP_SROM_MII, srom_mii);

  /* Wait at least 20 PCI cycles. */
  DELAY(20);

  /* Read the BIOS rom data. */
  srom_mii = READ_CSR(TLP_SROM_MII);

  /* Disable the BIOS rom. */
  WRITE_CSR(TLP_SROM_MII, TLP_MII_MDOE);

  return (u_int8_t)srom_mii & 0xFF;
  }

static void
write_bios_phys(softc_t *sc, u_int32_t addr, u_int8_t data)
  {
  u_int32_t srom_mii;

  /* Load the BIOS rom address register. */
  WRITE_CSR(TLP_BIOS_ROM, addr);

  /* Enable the BIOS rom. */
  srom_mii = TLP_BIOS_SEL | TLP_BIOS_WR | TLP_MII_MDOE;

  /* Load the data into the data register. */
  srom_mii = (srom_mii & 0xFFFFFF00) | (data & 0xFF);
  WRITE_CSR(TLP_SROM_MII, srom_mii);

  /* Wait at least 20 PCI cycles. */
  DELAY(20);

  /* Disable the BIOS rom. */
  WRITE_CSR(TLP_SROM_MII, TLP_MII_MDOE);
  }

/* IOCTL SYSCALL: can sleep. */
static void
write_bios(softc_t *sc, u_int32_t addr, u_int8_t data)
  {
  u_int8_t read_data;

  /* this sequence enables writing */
  write_bios_phys(sc, 0x5555, 0xAA);
  write_bios_phys(sc, 0x2AAA, 0x55);
  write_bios_phys(sc, 0x5555, 0xA0);
  write_bios_phys(sc, addr,   data);

  /* Wait for the write operation to complete. */
  for (;;)  /* interruptable syscall */
    {
    for (;;)
      {
      read_data = read_bios(sc, addr);
      if ((read_data & 0x80) == (data & 0x80)) break;
      if  (read_data & 0x20)
        {  /* Data sheet says read it again. */
        read_data = read_bios(sc, addr);
        if ((read_data & 0x80) == (data & 0x80)) break;
        if (DRIVER_DEBUG)
          printf("%s: write_bios() failed; rom addr=0x%x\n",
           NAME_UNIT, addr);
        return;
        }
      }
    read_data = read_bios(sc, addr);
    if (read_data == data) break;
    }
  }

/* IOCTL SYSCALL: can sleep. */
static void
erase_bios(softc_t *sc)
  {
  unsigned char read_data;

  /* This sequence enables erasing: */
  write_bios_phys(sc, 0x5555, 0xAA);
  write_bios_phys(sc, 0x2AAA, 0x55);
  write_bios_phys(sc, 0x5555, 0x80);
  write_bios_phys(sc, 0x5555, 0xAA);
  write_bios_phys(sc, 0x2AAA, 0x55);
  write_bios_phys(sc, 0x5555, 0x10);

  /* Wait for the erase operation to complete. */
  for (;;) /* interruptable syscall */
    {
    for (;;)
      {
      read_data = read_bios(sc, 0);
      if (read_data & 0x80) break;
      if (read_data & 0x20)
        {  /* Data sheet says read it again. */
        read_data = read_bios(sc, 0);
        if (read_data & 0x80) break;
        if (DRIVER_DEBUG)
          printf("%s: erase_bios() failed\n", NAME_UNIT);
        return;
        }
      }
    read_data = read_bios(sc, 0);
    if (read_data == 0xFF) break;
    }
  }

/* MDIO is 3-stated between tranactions. */
/* MDIO is set up before the RISING edge of MDC; MDC is parked low. */
static void
shift_mii_bits(softc_t *sc, u_int32_t data, u_int32_t len)
  {
  u_int32_t csr = READ_CSR(TLP_SROM_MII);
  for (; len>0; len--)
    {  /* MSB first */
    if (data & (1<<(len-1)))
      csr |=  TLP_MII_MDOUT; /* MDOUT setup */
    else
      csr &= ~TLP_MII_MDOUT; /* MDOUT setup */
    WRITE_CSR(TLP_SROM_MII, csr);
    csr |=  TLP_MII_MDC;     /* MDC rising edge */
    WRITE_CSR(TLP_SROM_MII, csr);
    csr &= ~TLP_MII_MDC;     /* MDC falling edge */
    WRITE_CSR(TLP_SROM_MII, csr);
    }
  }

/* The specification for the MII is IEEE Std 802.3 clause 22. */
/* MDIO is sampled on the RISING edge of MDC; MDC is parked low. */
static u_int16_t
read_mii(softc_t *sc, u_int8_t regad)
  {
  int i;
  u_int32_t csr;
  u_int16_t data = 0;
 
  WRITE_CSR(TLP_SROM_MII, TLP_MII_MDOUT);

  shift_mii_bits(sc, 0xFFFFF, 20);	/* preamble */
  shift_mii_bits(sc, 0xFFFFF, 20);	/* preamble */
  shift_mii_bits(sc, 1, 2);		/* start symbol */
  shift_mii_bits(sc, 2, 2);		/* read op */
  shift_mii_bits(sc, 0, 5);		/* phyad=0 */
  shift_mii_bits(sc, regad, 5);		/* regad */
  csr = READ_CSR(TLP_SROM_MII);
  csr |= TLP_MII_MDOE;
  WRITE_CSR(TLP_SROM_MII, csr);
  shift_mii_bits(sc, 0, 2);		/* turn-around */
  for (i=15; i>=0; i--)			/* data */
    {  /* MSB first */
    csr = READ_CSR(TLP_SROM_MII);	/* MDIN sampled */
    data = (data<<1) | ((csr & TLP_MII_MDIN) ? 1:0);
    csr |=  TLP_MII_MDC;		/* MDC rising edge */
    WRITE_CSR(TLP_SROM_MII, csr);
    csr &= ~TLP_MII_MDC;		/* MDC falling edge */
    WRITE_CSR(TLP_SROM_MII, csr);
    }
  return data;
  }

static void
write_mii(softc_t *sc, u_int8_t regad, u_int16_t data)
  {
  WRITE_CSR(TLP_SROM_MII, TLP_MII_MDOUT);
  shift_mii_bits(sc, 0xFFFFF, 20);	/* preamble */
  shift_mii_bits(sc, 0xFFFFF, 20);	/* preamble */
  shift_mii_bits(sc, 1, 2);		/* start symbol */
  shift_mii_bits(sc, 1, 2);		/* write op */
  shift_mii_bits(sc, 0, 5);		/* phyad=0 */
  shift_mii_bits(sc, regad, 5);		/* regad */
  shift_mii_bits(sc, 2, 2);		/* turn-around */
  shift_mii_bits(sc, data, 16);		/* data */
  WRITE_CSR(TLP_SROM_MII, TLP_MII_MDOE);
  if (regad == 16) sc->led_state = data; /* a small optimization */
  }

static void
set_mii16_bits(softc_t *sc, u_int16_t bits)
  {
  u_int16_t mii16 = read_mii(sc, 16);
  mii16 |= bits;
  write_mii(sc, 16, mii16);
  }

static void
clr_mii16_bits(softc_t *sc, u_int16_t bits)
  {
  u_int16_t mii16 = read_mii(sc, 16);
  mii16 &= ~bits;
  write_mii(sc, 16, mii16);
  }

static void
set_mii17_bits(softc_t *sc, u_int16_t bits)
  {
  u_int16_t mii17 = read_mii(sc, 17);
  mii17 |= bits;
  write_mii(sc, 17, mii17);
  }

static void
clr_mii17_bits(softc_t *sc, u_int16_t bits)
  {
  u_int16_t mii17 = read_mii(sc, 17);
  mii17 &= ~bits;
  write_mii(sc, 17, mii17);
  }

/*
 * Watchdog code is more readable if it refreshes LEDs
 *  once a second whether they need it or not.
 * But MII refs take 150 uSecs each, so remember the last value
 *  written to MII16 and avoid LED writes that do nothing.
 */

static void
led_off(softc_t *sc, u_int16_t led)
  {
  if ((led & sc->led_state) == led) return;
  set_mii16_bits(sc, led);
  }

static void
led_on(softc_t *sc, u_int16_t led)
  {
  if ((led & sc->led_state) == 0) return;
  clr_mii16_bits(sc, led);
  }

static void
led_inv(softc_t *sc, u_int16_t led)
  {
  u_int16_t mii16 = read_mii(sc, 16);
  mii16 ^= led;
  write_mii(sc, 16, mii16);
  }

/*
 * T1 & T3 framer registers are accessed through MII regs 17 & 18.
 * Write the address to MII reg 17 then R/W data through MII reg 18.
 * The hardware interface is an Intel-style 8-bit muxed A/D bus.
 */
static void
write_framer(softc_t *sc, u_int16_t addr, u_int8_t data)
  {
  write_mii(sc, 17, addr);
  write_mii(sc, 18, data);
  }

static u_int8_t
read_framer(softc_t *sc, u_int16_t addr)
  {
  write_mii(sc, 17, addr);
  return (u_int8_t)read_mii(sc, 18);
  }

/* Tulip's hardware implementation of General Purpose IO
 *   (GPIO) pins makes life difficult for software.
 * Bits 7-0 in the Tulip GPIO CSR are used for two purposes
 *   depending on the state of bit 8.
 * If bit 8 is 0 then bits 7-0 are "data" bits.
 * If bit 8 is 1 then bits 7-0 are "direction" bits.
 * If a direction bit is one, the data bit is an output.
 * The problem is that the direction bits are WRITE-ONLY.
 * Software must remember the direction bits in a shadow copy.
 * (sc->gpio_dir) in order to change some but not all of the bits.
 * All accesses to the Tulip GPIO register use these five procedures.
 */

static void
make_gpio_input(softc_t *sc, u_int32_t bits)
  {
  sc->gpio_dir &= ~bits;
  WRITE_CSR(TLP_GPIO, TLP_GPIO_DIR | (sc->gpio_dir));
  }

static void
make_gpio_output(softc_t *sc, u_int32_t bits)
  {
  sc->gpio_dir |= bits;
  WRITE_CSR(TLP_GPIO, TLP_GPIO_DIR | (sc->gpio_dir));
  }

static u_int32_t
read_gpio(softc_t *sc)
  {
  return READ_CSR(TLP_GPIO);
  }

static void
set_gpio_bits(softc_t *sc, u_int32_t bits)
  {
  WRITE_CSR(TLP_GPIO, (read_gpio(sc) |  bits) & 0xFF);
  }

static void
clr_gpio_bits(softc_t *sc, u_int32_t bits)
  {
  WRITE_CSR(TLP_GPIO, (read_gpio(sc) & ~bits) & 0xFF);
  }

/* Reset ALL of the flip-flops in the gate array to zero. */
/* This does NOT change the gate array programming. */
/* Called during initialization so it must not sleep. */
static void
reset_xilinx(softc_t *sc)
  {
  /* Drive RESET low to force initialization. */
  clr_gpio_bits(sc, GPIO_RESET);
  make_gpio_output(sc, GPIO_RESET);

  /* Hold RESET low for more than 10 uSec. */
  DELAY(50);

  /* Done with RESET; make it an input. */
  make_gpio_input(sc,  GPIO_RESET);
  }

/* Load Xilinx gate array program from on-board rom. */
/* This changes the gate array programming. */
/* IOCTL SYSCALL: can sleep. */
static void
load_xilinx_from_rom(softc_t *sc)
  {
  int i;

  /* Drive MODE low to load from ROM rather than GPIO. */
  clr_gpio_bits(sc, GPIO_MODE);
  make_gpio_output(sc, GPIO_MODE);

  /* Drive DP & RESET low to force configuration. */
  clr_gpio_bits(sc, GPIO_RESET | GPIO_DP);
  make_gpio_output(sc, GPIO_RESET | GPIO_DP);

  /* Hold RESET & DP low for more than 10 uSec. */
  DELAY(50);

  /* Done with RESET & DP; make them inputs. */
  make_gpio_input(sc, GPIO_DP | GPIO_RESET);

  /* BUSY-WAIT for Xilinx chip to configure itself from ROM bits. */
  for (i=0; i<100; i++) /* 1 sec max delay */
    if ((read_gpio(sc) & GPIO_DP) == 0) SLEEP(10000);

  /* Done with MODE; make it an input. */
  make_gpio_input(sc, GPIO_MODE);
  }

/* Load the Xilinx gate array program from userland bits. */
/* This changes the gate array programming. */
/* IOCTL SYSCALL: can sleep. */
static int
load_xilinx_from_file(softc_t *sc, char *addr, u_int32_t len)
  {
  char *data;
  int i, j, error;

  /* Get some pages to hold the Xilinx bits; biggest file is < 6 KB. */
  if (len > 8192) return EFBIG;  /* too big */
  data = malloc(len, M_DEVBUF, M_WAITOK);
  if (data == NULL) return ENOMEM;

  /* Copy the Xilinx bits from userland. */
  if ((error = copyin(addr, data, len)))
    {
    free(data, M_DEVBUF);
    return error;
    }

  /* Drive MODE high to load from GPIO rather than ROM. */
  set_gpio_bits(sc, GPIO_MODE);
  make_gpio_output(sc, GPIO_MODE);

  /* Drive DP & RESET low to force configuration. */
  clr_gpio_bits(sc, GPIO_RESET | GPIO_DP);
  make_gpio_output(sc, GPIO_RESET | GPIO_DP);

  /* Hold RESET & DP low for more than 10 uSec. */
  DELAY(50);
  
  /* Done with RESET & DP; make them inputs. */
  make_gpio_input(sc, GPIO_RESET | GPIO_DP);

  /* BUSY-WAIT for Xilinx chip to clear its config memory. */
  make_gpio_input(sc, GPIO_INIT);
  for (i=0; i<10000; i++) /* 1 sec max delay */
    if ((read_gpio(sc) & GPIO_INIT)==0) SLEEP(10000);

  /* Configure CLK and DATA as outputs. */
  set_gpio_bits(sc, GPIO_CLK);  /* park CLK high */
  make_gpio_output(sc, GPIO_CLK | GPIO_DATA);

  /* Write bits to Xilinx; CLK is parked HIGH. */
  /* DATA is set up before the RISING edge of CLK. */
  for (i=0; i<len; i++)
    for (j=0; j<8; j++)
      {  /* LSB first */
      if ((data[i] & (1<<j)) != 0)
        set_gpio_bits(sc, GPIO_DATA); /* DATA setup */
      else
        clr_gpio_bits(sc, GPIO_DATA); /* DATA setup */
      clr_gpio_bits(sc, GPIO_CLK); /* CLK falling edge */
      set_gpio_bits(sc, GPIO_CLK); /* CLK rising edge */
      }

  /* Stop driving all Xilinx-related signals. */
  /* Pullup and pulldown resistors take over. */
  make_gpio_input(sc, GPIO_CLK | GPIO_DATA | GPIO_MODE);

  free(data, M_DEVBUF);
  return 0;
  }

/* Write fragments of a command into the synthesized oscillator. */
/* DATA is set up before the RISING edge of CLK.  CLK is parked low. */
static void
shift_synth_bits(softc_t *sc, u_int32_t data, u_int32_t len)
  {
  int i;

  for (i=0; i<len; i++)
    { /* LSB first */
    if ((data & (1<<i)) != 0)
      set_gpio_bits(sc, GPIO_DATA); /* DATA setup */
    else
      clr_gpio_bits(sc, GPIO_DATA); /* DATA setup */
    set_gpio_bits(sc, GPIO_CLK);    /* CLK rising edge */
    clr_gpio_bits(sc, GPIO_CLK);    /* CLK falling edge */
    }
  }

/* Write a command to the synthesized oscillator on SSI and HSSIc. */
static void
write_synth(softc_t *sc, struct synth *synth)
  {
  /* SSI cards have a programmable prescaler */
  if (sc->status.card_type == TLP_CSID_SSI)
    {
    if (synth->prescale == 9) /* divide by 512 */
      set_mii17_bits(sc, MII17_SSI_PRESCALE);
    else                      /* divide by  32 */
      clr_mii17_bits(sc, MII17_SSI_PRESCALE);
    }

  clr_gpio_bits(sc,    GPIO_DATA | GPIO_CLK);
  make_gpio_output(sc, GPIO_DATA | GPIO_CLK);

  /* SYNTH is a low-true chip enable for the AV9110 chip. */
  set_gpio_bits(sc,    GPIO_SSI_SYNTH);
  make_gpio_output(sc, GPIO_SSI_SYNTH);
  clr_gpio_bits(sc,    GPIO_SSI_SYNTH);

  /* Serially shift the command into the AV9110 chip. */
  shift_synth_bits(sc, synth->n, 7);
  shift_synth_bits(sc, synth->m, 7);
  shift_synth_bits(sc, synth->v, 1);
  shift_synth_bits(sc, synth->x, 2);
  shift_synth_bits(sc, synth->r, 2);
  shift_synth_bits(sc, 0x16, 5); /* enable clk/x output */

  /* SYNTH (chip enable) going high ends the command. */
  set_gpio_bits(sc,   GPIO_SSI_SYNTH);
  make_gpio_input(sc, GPIO_SSI_SYNTH);

  /* Stop driving serial-related signals; pullups/pulldowns take over. */
  make_gpio_input(sc, GPIO_DATA | GPIO_CLK);

  /* remember the new synthesizer parameters */
  if (&sc->config.synth != synth) sc->config.synth = *synth;
  }

/* Write a command to the DAC controlling the VCXO on some T3 adapters. */
/* The DAC is a TI-TLV5636: 12-bit resolution and a serial interface. */
/* DATA is set up before the FALLING edge of CLK.  CLK is parked HIGH. */
static void
write_dac(softc_t *sc, u_int16_t data)
  {
  int i;

  /* Prepare to use DATA and CLK. */
  set_gpio_bits(sc,    GPIO_DATA | GPIO_CLK);
  make_gpio_output(sc, GPIO_DATA | GPIO_CLK);

  /* High-to-low transition prepares DAC for new value. */
  set_gpio_bits(sc,    GPIO_T3_DAC);
  make_gpio_output(sc, GPIO_T3_DAC);
  clr_gpio_bits(sc,    GPIO_T3_DAC);

  /* Serially shift command bits into DAC. */
  for (i=0; i<16; i++)
    { /* MSB first */
    if ((data & (1<<(15-i))) != 0)
      set_gpio_bits(sc, GPIO_DATA); /* DATA setup */
    else
      clr_gpio_bits(sc, GPIO_DATA); /* DATA setup */
    clr_gpio_bits(sc, GPIO_CLK);    /* CLK falling edge */
    set_gpio_bits(sc, GPIO_CLK);    /* CLK rising edge */
    }

  /* Done with DAC; make it an input; loads new value into DAC. */
  set_gpio_bits(sc,   GPIO_T3_DAC);
  make_gpio_input(sc, GPIO_T3_DAC);

  /* Stop driving serial-related signals; pullups/pulldowns take over. */
  make_gpio_input(sc, GPIO_DATA | GPIO_CLK);
  }

/* begin HSSI card code */

/* Must not sleep. */
static void
hssi_config(softc_t *sc)
  {
  if (sc->status.card_type == 0)
    { /* defaults */
    sc->status.card_type  = READ_PCI_CFG(sc, TLP_CSID);
    sc->config.crc_len    = CFG_CRC_16;
    sc->config.loop_back  = CFG_LOOP_NONE;
    sc->config.tx_clk_src = CFG_CLKMUX_ST;
    sc->config.dte_dce    = CFG_DTE;
    sc->config.synth.n    = 52; /* 52.000 Mbs */
    sc->config.synth.m    = 5;
    sc->config.synth.v    = 0;
    sc->config.synth.x    = 0;
    sc->config.synth.r    = 0;
    sc->config.synth.prescale = 2;
    }

  /* set CRC length */
  if (sc->config.crc_len == CFG_CRC_32)
    set_mii16_bits(sc, MII16_HSSI_CRC32);
  else
    clr_mii16_bits(sc, MII16_HSSI_CRC32);

  /* Assert pin LA in HSSI conn: ask modem for local loop. */
  if (sc->config.loop_back == CFG_LOOP_LL)
    set_mii16_bits(sc, MII16_HSSI_LA);
  else
    clr_mii16_bits(sc, MII16_HSSI_LA);

  /* Assert pin LB in HSSI conn: ask modem for remote loop. */
  if (sc->config.loop_back == CFG_LOOP_RL)
    set_mii16_bits(sc, MII16_HSSI_LB);
  else
    clr_mii16_bits(sc, MII16_HSSI_LB);

  if (sc->status.card_type == TLP_CSID_HSSI)
    {
    /* set TXCLK src */
    if (sc->config.tx_clk_src == CFG_CLKMUX_ST)
      set_gpio_bits(sc, GPIO_HSSI_TXCLK);
    else
      clr_gpio_bits(sc, GPIO_HSSI_TXCLK);
    make_gpio_output(sc, GPIO_HSSI_TXCLK);
    }
  else if (sc->status.card_type == TLP_CSID_HSSIc)
    {  /* cPCI HSSI rev C has extra features */
    /* Set TXCLK source. */
    u_int16_t mii16 = read_mii(sc, 16);
    mii16 &= ~MII16_HSSI_CLKMUX;
    mii16 |= (sc->config.tx_clk_src&3)<<13;
    write_mii(sc, 16, mii16);

    /* cPCI HSSI implements loopback towards the net. */
    if (sc->config.loop_back == CFG_LOOP_LINE)
      set_mii16_bits(sc, MII16_HSSI_LOOP);
    else
      clr_mii16_bits(sc, MII16_HSSI_LOOP);

    /* Set DTE/DCE mode. */
    if (sc->config.dte_dce == CFG_DCE)
      set_gpio_bits(sc, GPIO_HSSI_DCE);
    else
      clr_gpio_bits(sc, GPIO_HSSI_DCE);
    make_gpio_output(sc, GPIO_HSSI_DCE);

    /* Program the synthesized oscillator. */
    write_synth(sc, &sc->config.synth);
    }
  }

static void
hssi_ident(softc_t *sc)
  {
  }

/* Called once a second; must not sleep. */
static int
hssi_watchdog(softc_t *sc)
  {
  u_int16_t mii16 = read_mii(sc, 16) & MII16_HSSI_MODEM;
  int link_status = STATUS_UP;

  led_inv(sc, MII16_HSSI_LED_UL);  /* Software is alive. */
  led_on(sc, MII16_HSSI_LED_LL);  /* always on (SSI cable) */

  /* Check the transmit clock. */
  if (sc->status.tx_speed == 0)
    {
    led_on(sc, MII16_HSSI_LED_UR);
    link_status = STATUS_DOWN;
    }
  else
    led_off(sc, MII16_HSSI_LED_UR);

  /* Is the modem ready? */
  if ((mii16 & MII16_HSSI_CA) == 0)
    {
    led_off(sc, MII16_HSSI_LED_LR);
    link_status = STATUS_DOWN;
    }
  else
    led_on(sc, MII16_HSSI_LED_LR);

  /* Print the modem control signals if they changed. */
  if ((DRIVER_DEBUG) && (mii16 != sc->last_mii16))
    {
    char *on = "ON ", *off = "OFF";
    printf("%s: TA=%s CA=%s LA=%s LB=%s LC=%s TM=%s\n", NAME_UNIT,
     (mii16 & MII16_HSSI_TA) ? on : off,
     (mii16 & MII16_HSSI_CA) ? on : off,
     (mii16 & MII16_HSSI_LA) ? on : off,
     (mii16 & MII16_HSSI_LB) ? on : off,
     (mii16 & MII16_HSSI_LC) ? on : off,
     (mii16 & MII16_HSSI_TM) ? on : off);
    }

  /* SNMP one-second-report */
  sc->status.snmp.hssi.sigs = mii16 & MII16_HSSI_MODEM;

  /* Remember this state until next time. */
  sc->last_mii16 = mii16;

  /* If a loop back is in effect, link status is UP */
  if (sc->config.loop_back != CFG_LOOP_NONE)
    link_status = STATUS_UP;

  return link_status;
  }

/* IOCTL SYSCALL: can sleep (but doesn't). */
static int
hssi_ioctl(softc_t *sc, struct ioctl *ioctl)
  {
  int error = 0;

  if (ioctl->cmd == IOCTL_SNMP_SIGS)
    {
    u_int16_t mii16 = read_mii(sc, 16);
    mii16 &= ~MII16_HSSI_MODEM;
    mii16 |= (MII16_HSSI_MODEM & ioctl->data);
    write_mii(sc, 16, mii16);
    }
  else if (ioctl->cmd == IOCTL_SET_STATUS)
    {
    if (ioctl->data != 0)
      set_mii16_bits(sc, MII16_HSSI_TA);
    else
      clr_mii16_bits(sc, MII16_HSSI_TA);
    }
  else
    error = EINVAL;

  return error;
  }

/* begin DS3 card code */

/* Must not sleep. */
static void
t3_config(softc_t *sc)
  {
  int i;
  u_int8_t ctl1;

  if (sc->status.card_type == 0)
    { /* defaults */
    sc->status.card_type  = TLP_CSID_T3;
    sc->config.crc_len    = CFG_CRC_16;
    sc->config.loop_back  = CFG_LOOP_NONE;
    sc->config.format     = CFG_FORMAT_T3CPAR;
    sc->config.cable_len  = 10; /* meters */
    sc->config.scrambler  = CFG_SCRAM_DL_KEN;
    sc->config.tx_clk_src = CFG_CLKMUX_INT;

    /* Center the VCXO -- get within 20 PPM of 44736000. */
    write_dac(sc, 0x9002); /* set Vref = 2.048 volts */
    write_dac(sc, 2048); /* range is 0..4095 */
    }

  /* Set cable length. */
  if (sc->config.cable_len > 30)
    clr_mii16_bits(sc, MII16_DS3_ZERO);
  else
    set_mii16_bits(sc, MII16_DS3_ZERO);

  /* Set payload scrambler polynomial. */
  if (sc->config.scrambler == CFG_SCRAM_LARS)
    set_mii16_bits(sc, MII16_DS3_POLY);
  else
    clr_mii16_bits(sc, MII16_DS3_POLY);

  /* Set payload scrambler on/off. */
  if (sc->config.scrambler == CFG_SCRAM_OFF)
    clr_mii16_bits(sc, MII16_DS3_SCRAM);
  else
    set_mii16_bits(sc, MII16_DS3_SCRAM);

  /* Set CRC length. */
  if (sc->config.crc_len == CFG_CRC_32)
    set_mii16_bits(sc, MII16_DS3_CRC32);
  else
    clr_mii16_bits(sc, MII16_DS3_CRC32);

  /* Loopback towards host thru the line interface. */
  if (sc->config.loop_back == CFG_LOOP_OTHER)
    set_mii16_bits(sc, MII16_DS3_TRLBK);
  else
    clr_mii16_bits(sc, MII16_DS3_TRLBK);

  /* Loopback towards network thru the line interface. */
  if (sc->config.loop_back == CFG_LOOP_LINE)
    set_mii16_bits(sc, MII16_DS3_LNLBK);
  else if (sc->config.loop_back == CFG_LOOP_DUAL)
    set_mii16_bits(sc, MII16_DS3_LNLBK);
  else
    clr_mii16_bits(sc, MII16_DS3_LNLBK);

  /* Configure T3 framer chip; write EVERY writeable register. */
  ctl1 = CTL1_SER | CTL1_XTX;
  if (sc->config.loop_back == CFG_LOOP_INWARD) ctl1 |= CTL1_3LOOP;
  if (sc->config.loop_back == CFG_LOOP_DUAL)   ctl1 |= CTL1_3LOOP;
  if (sc->config.format == CFG_FORMAT_T3M13)   ctl1 |= CTL1_M13MODE;
  write_framer(sc, T3CSR_CTL1,     ctl1);
  write_framer(sc, T3CSR_TX_FEAC,  CTL5_EMODE);
  write_framer(sc, T3CSR_CTL8,     CTL8_FBEC);
  write_framer(sc, T3CSR_CTL12,    CTL12_DLCB1 | CTL12_C21 | CTL12_MCB1);
  write_framer(sc, T3CSR_DBL_FEAC, 0);
  write_framer(sc, T3CSR_CTL14,    CTL14_RGCEN | CTL14_TGCEN);
  write_framer(sc, T3CSR_INTEN,    0);
  write_framer(sc, T3CSR_CTL20,    CTL20_CVEN);

  /* Clear error counters and latched error bits */
  /*  that may have happened while initializing. */
  for (i=0; i<21; i++) read_framer(sc, i);
  }

static void
t3_ident(softc_t *sc)
  {
  printf(", TXC03401 rev B");
  }

/* Called once a second; must not sleep. */
static int
t3_watchdog(softc_t *sc)
  {
  u_int16_t CV;
  u_int8_t CERR, PERR, MERR, FERR, FEBE;
  u_int8_t ctl1, stat16, feac;
  int link_status = STATUS_UP;
  u_int16_t mii16;

  /* Read the alarm registers. */
  ctl1   = read_framer(sc, T3CSR_CTL1);
  stat16 = read_framer(sc, T3CSR_STAT16);
  mii16  = read_mii(sc, 16);

  /* Always ignore the RTLOC alarm bit. */
  stat16 &= ~STAT16_RTLOC;

  /* Software is alive. */
  led_inv(sc, MII16_DS3_LED_GRN);

  /* Receiving Alarm Indication Signal (AIS). */
  if ((stat16 & STAT16_RAIS) != 0) /* receiving ais */
    led_on(sc, MII16_DS3_LED_BLU);
  else if (ctl1 & CTL1_TXAIS) /* sending ais */
    led_inv(sc, MII16_DS3_LED_BLU);
  else
    led_off(sc, MII16_DS3_LED_BLU);

  /* Receiving Remote Alarm Indication (RAI). */
  if ((stat16 & STAT16_XERR) != 0) /* receiving rai */
    led_on(sc, MII16_DS3_LED_YEL);
  else if ((ctl1 & CTL1_XTX) == 0) /* sending rai */
    led_inv(sc, MII16_DS3_LED_YEL);
  else
    led_off(sc, MII16_DS3_LED_YEL);

  /* If certain status bits are set then the link is 'down'. */
  /* The bad bits are: rxlos rxoof rxais rxidl xerr. */
  if ((stat16 & ~(STAT16_FEAC | STAT16_SEF)) != 0)
    link_status = STATUS_DOWN;

  /* Declare local Red Alarm if the link is down. */
  if (link_status == STATUS_DOWN)
    led_on(sc, MII16_DS3_LED_RED);
  else if (sc->loop_timer != 0) /* loopback is active */
    led_inv(sc, MII16_DS3_LED_RED);
  else
    led_off(sc, MII16_DS3_LED_RED);

  /* Print latched error bits if they changed. */
  if ((DRIVER_DEBUG) && ((stat16 & ~STAT16_FEAC) != sc->last_stat16))
    {
    char *on = "ON ", *off = "OFF";
    printf("%s: RLOS=%s ROOF=%s RAIS=%s RIDL=%s SEF=%s XERR=%s\n",
     NAME_UNIT,
     (stat16 & STAT16_RLOS) ? on : off,
     (stat16 & STAT16_ROOF) ? on : off,
     (stat16 & STAT16_RAIS) ? on : off,
     (stat16 & STAT16_RIDL) ? on : off,
     (stat16 & STAT16_SEF)  ? on : off,
     (stat16 & STAT16_XERR) ? on : off);
    }

  /* Check and print error counters if non-zero. */
  CV   = read_framer(sc, T3CSR_CVHI)<<8;
  CV  += read_framer(sc, T3CSR_CVLO);
  PERR = read_framer(sc, T3CSR_PERR);
  CERR = read_framer(sc, T3CSR_CERR);
  FERR = read_framer(sc, T3CSR_FERR);
  MERR = read_framer(sc, T3CSR_MERR);
  FEBE = read_framer(sc, T3CSR_FEBE);

  /* CV is invalid during LOS. */
  if ((stat16 & STAT16_RLOS)!=0) CV = 0;
  /* CERR & FEBE are invalid in M13 mode */
  if (sc->config.format == CFG_FORMAT_T3M13) CERR = FEBE = 0;
  /* FEBE is invalid during AIS. */
  if ((stat16 & STAT16_RAIS)!=0) FEBE = 0;
  if (DRIVER_DEBUG && (CV || PERR || CERR || FERR || MERR || FEBE))
    printf("%s: CV=%u PERR=%u CERR=%u FERR=%u MERR=%u FEBE=%u\n",
     NAME_UNIT, CV,   PERR,   CERR,   FERR,   MERR,   FEBE);

  /* Driver keeps crude link-level error counters (SNMP is better). */
  sc->status.cntrs.lcv_errs  += CV;
  sc->status.cntrs.par_errs  += PERR;
  sc->status.cntrs.cpar_errs += CERR;
  sc->status.cntrs.frm_errs  += FERR;
  sc->status.cntrs.mfrm_errs += MERR;
  sc->status.cntrs.febe_errs += FEBE;

  /* Check for FEAC messages (FEAC not defined in M13 mode). */
  if (FORMAT_T3CPAR && (stat16 & STAT16_FEAC)) do
    {
    feac = read_framer(sc, T3CSR_FEAC_STK);
    if ((feac & FEAC_STK_VALID)==0) break;
    /* Ignore RxFEACs while a far end loopback has been requested. */
    if ((sc->status.snmp.t3.line & TLOOP_FAR_LINE)!=0) continue;
    switch (feac & FEAC_STK_FEAC)
      {
      case T3BOP_LINE_UP:   break;
      case T3BOP_LINE_DOWN: break;
      case T3BOP_LOOP_DS3:
        {
        if (sc->last_FEAC == T3BOP_LINE_DOWN)
          {
          if (DRIVER_DEBUG)
            printf("%s: Received a 'line loopback deactivate' FEAC msg\n", NAME_UNIT);
          clr_mii16_bits(sc, MII16_DS3_LNLBK);
          sc->loop_timer = 0;
	  }
        if (sc->last_FEAC == T3BOP_LINE_UP)
          {
          if (DRIVER_DEBUG)
            printf("%s: Received a 'line loopback activate' FEAC msg\n", NAME_UNIT);
          set_mii16_bits(sc, MII16_DS3_LNLBK);
          sc->loop_timer = 300;
	  }
        break;
        }
      case T3BOP_OOF:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'far end LOF' FEAC msg\n", NAME_UNIT);
        break;
	}
      case T3BOP_IDLE:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'far end IDL' FEAC msg\n", NAME_UNIT);
        break;
	}
      case T3BOP_AIS:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'far end AIS' FEAC msg\n", NAME_UNIT);
        break;
	}
      case T3BOP_LOS:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'far end LOS' FEAC msg\n", NAME_UNIT);
        break;
	}
      default:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'type 0x%02X' FEAC msg\n", NAME_UNIT, feac & FEAC_STK_FEAC);
        break;
	}
      }
    sc->last_FEAC = feac & FEAC_STK_FEAC;
    } while ((feac & FEAC_STK_MORE) != 0);
  stat16 &= ~STAT16_FEAC;

  /* Send Service-Affecting priority FEAC messages */
  if (((sc->last_stat16 ^ stat16) & 0xF0) && (FORMAT_T3CPAR))
    {
    /* Transmit continuous FEACs */
    write_framer(sc, T3CSR_CTL14,
     read_framer(sc, T3CSR_CTL14) & ~CTL14_FEAC10);
    if      ((stat16 & STAT16_RLOS)!=0)
      write_framer(sc, T3CSR_TX_FEAC, 0xC0 + T3BOP_LOS);
    else if ((stat16 & STAT16_ROOF)!=0)
      write_framer(sc, T3CSR_TX_FEAC, 0xC0 + T3BOP_OOF);
    else if ((stat16 & STAT16_RAIS)!=0)
      write_framer(sc, T3CSR_TX_FEAC, 0xC0 + T3BOP_AIS);
    else if ((stat16 & STAT16_RIDL)!=0)
      write_framer(sc, T3CSR_TX_FEAC, 0xC0 + T3BOP_IDLE);
    else
      write_framer(sc, T3CSR_TX_FEAC, CTL5_EMODE);
    }

  /* Start sending RAI, Remote Alarm Indication. */
  if (((stat16 & STAT16_ROOF)!=0) && ((stat16 & STAT16_RLOS)==0) &&
   ((sc->last_stat16 & STAT16_ROOF)==0))
    write_framer(sc, T3CSR_CTL1, ctl1 &= ~CTL1_XTX);
  /* Stop sending RAI, Remote Alarm Indication. */
  else if (((stat16 & STAT16_ROOF)==0) && ((sc->last_stat16 & STAT16_ROOF)!=0))
    write_framer(sc, T3CSR_CTL1, ctl1 |=  CTL1_XTX);

  /* Start sending AIS, Alarm Indication Signal */
  if (((stat16 & STAT16_RLOS)!=0) && ((sc->last_stat16 & STAT16_RLOS)==0))
    {
    set_mii16_bits(sc, MII16_DS3_FRAME);
    write_framer(sc, T3CSR_CTL1, ctl1 |  CTL1_TXAIS);
    }
  /* Stop sending AIS, Alarm Indication Signal */
  else if (((stat16 & STAT16_RLOS)==0) && ((sc->last_stat16 & STAT16_RLOS)!=0))
    {
    clr_mii16_bits(sc, MII16_DS3_FRAME);
    write_framer(sc, T3CSR_CTL1, ctl1 & ~CTL1_TXAIS);
    }

  /* Time out loopback requests. */
  if (sc->loop_timer != 0)
    if (--sc->loop_timer == 0)
      if ((mii16 & MII16_DS3_LNLBK)!=0)
        {
        if (DRIVER_DEBUG)
          printf("%s: Timeout: Loop Down after 300 seconds\n", NAME_UNIT);
        clr_mii16_bits(sc, MII16_DS3_LNLBK); /* line loopback off */
        }

  /* SNMP error counters */
  sc->status.snmp.t3.lcv  = CV;
  sc->status.snmp.t3.pcv  = PERR;
  sc->status.snmp.t3.ccv  = CERR;
  sc->status.snmp.t3.febe = FEBE;

  /* SNMP Line Status */
  sc->status.snmp.t3.line = 0;
  if ((ctl1  & CTL1_XTX)==0)   sc->status.snmp.t3.line |= TLINE_TX_RAI;
  if (stat16 & STAT16_XERR)    sc->status.snmp.t3.line |= TLINE_RX_RAI;
  if (ctl1   & CTL1_TXAIS)     sc->status.snmp.t3.line |= TLINE_TX_AIS;
  if (stat16 & STAT16_RAIS)    sc->status.snmp.t3.line |= TLINE_RX_AIS;
  if (stat16 & STAT16_ROOF)    sc->status.snmp.t3.line |= TLINE_LOF;
  if (stat16 & STAT16_RLOS)    sc->status.snmp.t3.line |= TLINE_LOS;
  if (stat16 & STAT16_SEF)     sc->status.snmp.t3.line |= T3LINE_SEF;

  /* SNMP Loopback Status */
  sc->status.snmp.t3.loop &= ~TLOOP_FAR_LINE;
  if (sc->config.loop_back == CFG_LOOP_TULIP)
                               sc->status.snmp.t3.loop |= TLOOP_NEAR_OTHER;
  if (ctl1  & CTL1_3LOOP)      sc->status.snmp.t3.loop |= TLOOP_NEAR_INWARD;
  if (mii16 & MII16_DS3_TRLBK) sc->status.snmp.t3.loop |= TLOOP_NEAR_OTHER;
  if (mii16 & MII16_DS3_LNLBK) sc->status.snmp.t3.loop |= TLOOP_NEAR_LINE;
/*if (ctl12 & CTL12_RTPLOOP)   sc->status.snmp.t3.loop |= TLOOP_NEAR_PAYLOAD; */

  /* Remember this state until next time. */
  sc->last_stat16 = stat16;

  /* If an INWARD loopback is in effect, link status is UP */
  if (sc->config.loop_back != CFG_LOOP_NONE) /* XXX INWARD ONLY */
    link_status = STATUS_UP;

  return link_status;
  }

/* IOCTL SYSCALL: can sleep. */
static void
t3_send_dbl_feac(softc_t *sc, int feac1, int feac2)
  {
  u_int8_t tx_feac;
  int i;

  /* The FEAC transmitter could be sending a continuous */
  /*  FEAC msg when told to send a double FEAC message. */
  /* So save the current state of the FEAC transmitter. */
  tx_feac = read_framer(sc, T3CSR_TX_FEAC);
  /* Load second FEAC code and stop FEAC transmitter. */
  write_framer(sc, T3CSR_TX_FEAC,  CTL5_EMODE + feac2);
  /* FEAC transmitter sends 10 more FEACs and then stops. */
  SLEEP(20000); /* sending one FEAC takes 1700 uSecs */
  /* Load first FEAC code and start FEAC transmitter. */
  write_framer(sc, T3CSR_DBL_FEAC, CTL13_DFEXEC + feac1);
  /* Wait for double FEAC sequence to complete -- about 70 ms. */
  for (i=0; i<10; i++) /* max delay 100 ms */
    if (read_framer(sc, T3CSR_DBL_FEAC) & CTL13_DFEXEC) SLEEP(10000);
  /* Flush received FEACS; don't respond to our own loop cmd! */
  while (read_framer(sc, T3CSR_FEAC_STK) & FEAC_STK_VALID) DELAY(1); /* XXX HANG */
  /* Restore previous state of the FEAC transmitter. */
  /* If it was sending a continous FEAC, it will resume. */
  write_framer(sc, T3CSR_TX_FEAC, tx_feac);
  }

/* IOCTL SYSCALL: can sleep. */
static int
t3_ioctl(softc_t *sc, struct ioctl *ioctl)
  {
  int error = 0;

  switch (ioctl->cmd)
    {
    case IOCTL_SNMP_SEND:  /* set opstatus? */
      {
      if (sc->config.format != CFG_FORMAT_T3CPAR)
        error = EINVAL;
      else if (ioctl->data == TSEND_LINE)
        {
        sc->status.snmp.t3.loop |= TLOOP_FAR_LINE;
        t3_send_dbl_feac(sc, T3BOP_LINE_UP, T3BOP_LOOP_DS3);
        }
      else if (ioctl->data == TSEND_RESET)
        {
        t3_send_dbl_feac(sc, T3BOP_LINE_DOWN, T3BOP_LOOP_DS3);
        sc->status.snmp.t3.loop &= ~TLOOP_FAR_LINE;
        }
      else
        error = EINVAL;
      break;
      }
    case IOCTL_SNMP_LOOP:  /* set opstatus = test? */
      {
      if (ioctl->data == CFG_LOOP_NONE)
        {
        clr_mii16_bits(sc, MII16_DS3_FRAME);
        clr_mii16_bits(sc, MII16_DS3_TRLBK);
        clr_mii16_bits(sc, MII16_DS3_LNLBK);
        write_framer(sc, T3CSR_CTL1,
         read_framer(sc, T3CSR_CTL1) & ~CTL1_3LOOP);
        write_framer(sc, T3CSR_CTL12,
         read_framer(sc, T3CSR_CTL12) & ~(CTL12_RTPLOOP | CTL12_RTPLLEN));
	}
      else if (ioctl->data == CFG_LOOP_LINE)
        set_mii16_bits(sc, MII16_DS3_LNLBK);
      else if (ioctl->data == CFG_LOOP_OTHER)
        set_mii16_bits(sc, MII16_DS3_TRLBK);
      else if (ioctl->data == CFG_LOOP_INWARD)
        write_framer(sc, T3CSR_CTL1,
         read_framer(sc, T3CSR_CTL1) | CTL1_3LOOP);
      else if (ioctl->data == CFG_LOOP_DUAL)
        {
        set_mii16_bits(sc, MII16_DS3_LNLBK);
        write_framer(sc, T3CSR_CTL1,
         read_framer(sc, T3CSR_CTL1) | CTL1_3LOOP);
	}
      else if (ioctl->data == CFG_LOOP_PAYLOAD)
        {
        set_mii16_bits(sc, MII16_DS3_FRAME);
        write_framer(sc, T3CSR_CTL12,
         read_framer(sc, T3CSR_CTL12) |  CTL12_RTPLOOP);
        write_framer(sc, T3CSR_CTL12,
         read_framer(sc, T3CSR_CTL12) |  CTL12_RTPLLEN);
        DELAY(25); /* at least two frames (22 uS) */
        write_framer(sc, T3CSR_CTL12,
         read_framer(sc, T3CSR_CTL12) & ~CTL12_RTPLLEN);
	}
      else
        error = EINVAL;
      break;
      }
    default:
      error = EINVAL;
      break;
    }

  return error;
  }

/* begin SSI card code */

/* Must not sleep. */
static void
ssi_config(softc_t *sc)
  {
  if (sc->status.card_type == 0)
    { /* defaults */
    sc->status.card_type  = TLP_CSID_SSI;
    sc->config.crc_len    = CFG_CRC_16;
    sc->config.loop_back  = CFG_LOOP_NONE;
    sc->config.tx_clk_src = CFG_CLKMUX_ST;
    sc->config.dte_dce    = CFG_DTE;
    sc->config.synth.n    = 51; /* 1.536 MHz */
    sc->config.synth.m    = 83;
    sc->config.synth.v    =  1;
    sc->config.synth.x    =  1;
    sc->config.synth.r    =  1;
    sc->config.synth.prescale = 4;
    }

  /* Disable the TX clock driver while programming the oscillator. */
  clr_gpio_bits(sc, GPIO_SSI_DCE);
  make_gpio_output(sc, GPIO_SSI_DCE);

  /* Program the synthesized oscillator. */
  write_synth(sc, &sc->config.synth);

  /* Set DTE/DCE mode. */
  /* If DTE mode then DCD & TXC are received. */
  /* If DCE mode then DCD & TXC are driven. */
  /* Boards with MII rev=4.0 don't drive DCD. */
  if (sc->config.dte_dce == CFG_DCE)
    set_gpio_bits(sc, GPIO_SSI_DCE);
  else
    clr_gpio_bits(sc, GPIO_SSI_DCE);
  make_gpio_output(sc, GPIO_SSI_DCE);

  /* Set CRC length. */
  if (sc->config.crc_len == CFG_CRC_32)
    set_mii16_bits(sc, MII16_SSI_CRC32);
  else
    clr_mii16_bits(sc, MII16_SSI_CRC32);

  /* Loop towards host thru cable drivers and receivers. */
  /* Asserts DCD at the far end of a null modem cable. */
  if (sc->config.loop_back == CFG_LOOP_PINS)
    set_mii16_bits(sc, MII16_SSI_LOOP);
  else
    clr_mii16_bits(sc, MII16_SSI_LOOP);

  /* Assert pin LL in modem conn: ask modem for local loop. */
  /* Asserts TM at the far end of a null modem cable. */
  if (sc->config.loop_back == CFG_LOOP_LL)
    set_mii16_bits(sc, MII16_SSI_LL);
  else
    clr_mii16_bits(sc, MII16_SSI_LL);

  /* Assert pin RL in modem conn: ask modem for remote loop. */
  if (sc->config.loop_back == CFG_LOOP_RL)
    set_mii16_bits(sc, MII16_SSI_RL);
  else
    clr_mii16_bits(sc, MII16_SSI_RL);
  }

static void
ssi_ident(softc_t *sc)
  {
  printf(", LTC1343/44");
  }

/* Called once a second; must not sleep. */
static int
ssi_watchdog(softc_t *sc)
  {
  u_int16_t cable;
  u_int16_t mii16 = read_mii(sc, 16) & MII16_SSI_MODEM;
  int link_status = STATUS_UP;

  /* Software is alive. */
  led_inv(sc, MII16_SSI_LED_UL);

  /* Check the transmit clock. */
  if (sc->status.tx_speed == 0)
    {
    led_on(sc, MII16_SSI_LED_UR);
    link_status = STATUS_DOWN;
    }
  else
    led_off(sc, MII16_SSI_LED_UR);

  /* Check the external cable. */
  cable = read_mii(sc, 17);
  cable = cable &  MII17_SSI_CABLE_MASK;
  cable = cable >> MII17_SSI_CABLE_SHIFT;
  if (cable == 7)
    {
    led_off(sc, MII16_SSI_LED_LL); /* no cable */
    link_status = STATUS_DOWN;
    }
  else
    led_on(sc, MII16_SSI_LED_LL);

  /* The unit at the other end of the cable is ready if: */
  /*  DTE mode and DCD pin is asserted */
  /*  DCE mode and DSR pin is asserted */
  if (((sc->config.dte_dce == CFG_DTE) && ((mii16 & MII16_SSI_DCD)==0)) ||
      ((sc->config.dte_dce == CFG_DCE) && ((mii16 & MII16_SSI_DSR)==0)))
    {
    led_off(sc, MII16_SSI_LED_LR);
    link_status = STATUS_DOWN;
    }
  else
    led_on(sc, MII16_SSI_LED_LR);

  if (DRIVER_DEBUG && (cable != sc->status.cable_type))
    printf("%s: SSI cable type changed to '%s'\n",
     NAME_UNIT, ssi_cables[cable]);
  sc->status.cable_type = cable;

  /* Print the modem control signals if they changed. */
  if ((DRIVER_DEBUG) && (mii16 != sc->last_mii16))
    {
    char *on = "ON ", *off = "OFF";
    printf("%s: DTR=%s DSR=%s RTS=%s CTS=%s DCD=%s RI=%s LL=%s RL=%s TM=%s\n",
     NAME_UNIT,
     (mii16 & MII16_SSI_DTR) ? on : off,
     (mii16 & MII16_SSI_DSR) ? on : off,
     (mii16 & MII16_SSI_RTS) ? on : off,
     (mii16 & MII16_SSI_CTS) ? on : off,
     (mii16 & MII16_SSI_DCD) ? on : off,
     (mii16 & MII16_SSI_RI)  ? on : off,
     (mii16 & MII16_SSI_LL)  ? on : off,
     (mii16 & MII16_SSI_RL)  ? on : off,
     (mii16 & MII16_SSI_TM)  ? on : off);
    }

  /* SNMP one-second report */
  sc->status.snmp.ssi.sigs = mii16 & MII16_SSI_MODEM;

  /* Remember this state until next time. */
  sc->last_mii16 = mii16;

  /* If a loop back is in effect, link status is UP */
  if (sc->config.loop_back != CFG_LOOP_NONE)
    link_status = STATUS_UP;

  return link_status;
  }

/* IOCTL SYSCALL: can sleep (but doesn't). */
static int
ssi_ioctl(softc_t *sc, struct ioctl *ioctl)
  {
  int error = 0;

  if (ioctl->cmd == IOCTL_SNMP_SIGS)
    {
    u_int16_t mii16 = read_mii(sc, 16);
    mii16 &= ~MII16_SSI_MODEM;
    mii16 |= (MII16_SSI_MODEM & ioctl->data);
    write_mii(sc, 16, mii16);
    }
  else if (ioctl->cmd == IOCTL_SET_STATUS)
    {
    if (ioctl->data != 0)
      set_mii16_bits(sc, (MII16_SSI_DTR | MII16_SSI_RTS | MII16_SSI_DCD));
    else
      clr_mii16_bits(sc, (MII16_SSI_DTR | MII16_SSI_RTS | MII16_SSI_DCD));
    }
  else
    error = EINVAL;

  return error;
  }

/* begin T1E1 card code */

/* Must not sleep. */
static void
t1_config(softc_t *sc)
  {
  int i;
  u_int8_t pulse, lbo, gain;

  if (sc->status.card_type == 0)
    {  /* defaults */
    sc->status.card_type   = TLP_CSID_T1E1;
    sc->config.crc_len     = CFG_CRC_16;
    sc->config.loop_back   = CFG_LOOP_NONE;
    sc->config.tx_clk_src  = CFG_CLKMUX_INT;
    sc->config.format      = CFG_FORMAT_T1ESF;
    sc->config.cable_len   = 10;
    sc->config.time_slots  = 0x01FFFFFE;
    sc->config.tx_pulse    = CFG_PULSE_AUTO;
    sc->config.rx_gain     = CFG_GAIN_AUTO;
    sc->config.tx_lbo      = CFG_LBO_AUTO;

    /* Bt8370 occasionally powers up in a loopback mode. */
    /* Data sheet says zero LOOP reg and do a s/w reset. */
    write_framer(sc, Bt8370_LOOP, 0x00); /* no loopback */
    write_framer(sc, Bt8370_CR0,  0x80); /* s/w reset */
    for (i=0; i<10; i++) /* max delay 10 ms */
      if (read_framer(sc, Bt8370_CR0) & 0x80) DELAY(1000);
    }

  /* Set CRC length. */
  if (sc->config.crc_len == CFG_CRC_32)
    set_mii16_bits(sc, MII16_T1_CRC32);
  else
    clr_mii16_bits(sc, MII16_T1_CRC32);

  /* Invert HDLC payload data in SF/AMI mode. */
  /* HDLC stuff bits satisfy T1 pulse density. */
  if (FORMAT_T1SF)
    set_mii16_bits(sc, MII16_T1_INVERT);
  else
    clr_mii16_bits(sc, MII16_T1_INVERT);

  /* Set the transmitter output impedance. */
  if (FORMAT_E1ANY) set_mii16_bits(sc, MII16_T1_Z);

  /* 001:CR0 -- Control Register 0 - T1/E1 and frame format */
  write_framer(sc, Bt8370_CR0, sc->config.format);

  /* 002:JAT_CR -- Jitter Attenuator Control Register */
  if (sc->config.tx_clk_src == CFG_CLKMUX_RT) /* loop timing */
    write_framer(sc, Bt8370_JAT_CR, 0xA3); /* JAT in RX path */
  else
    { /* 64-bit elastic store; free-running JCLK and CLADO */
    write_framer(sc, Bt8370_JAT_CR, 0x4B); /* assert jcenter */
    write_framer(sc, Bt8370_JAT_CR, 0x43); /* release jcenter */
    }

  /* 00C-013:IERn -- Interrupt Enable Registers */
  for (i=Bt8370_IER7; i<=Bt8370_IER0; i++)
    write_framer(sc, i, 0); /* no interrupts; polled */

  /* 014:LOOP -- loopbacks */
  if      (sc->config.loop_back == CFG_LOOP_PAYLOAD)
    write_framer(sc, Bt8370_LOOP, LOOP_PAYLOAD);
  else if (sc->config.loop_back == CFG_LOOP_LINE)
    write_framer(sc, Bt8370_LOOP, LOOP_LINE);
  else if (sc->config.loop_back == CFG_LOOP_OTHER)
    write_framer(sc, Bt8370_LOOP, LOOP_ANALOG);
  else if (sc->config.loop_back == CFG_LOOP_INWARD)
    write_framer(sc, Bt8370_LOOP, LOOP_FRAMER);
  else if (sc->config.loop_back == CFG_LOOP_DUAL)
    write_framer(sc, Bt8370_LOOP, LOOP_DUAL);
  else
    write_framer(sc, Bt8370_LOOP, 0x00); /* no loopback */

  /* 015:DL3_TS -- Data Link 3 */
  write_framer(sc, Bt8370_DL3_TS, 0x00); /* disabled */

  /* 018:PIO -- Programmable I/O */
  write_framer(sc, Bt8370_PIO, 0xFF); /* all pins are outputs */

  /* 019:POE -- Programmable Output Enable */
  write_framer(sc, Bt8370_POE, 0x00); /* all outputs are enabled */

  /* 01A;CMUX -- Clock Input Mux */
  if (sc->config.tx_clk_src == CFG_CLKMUX_EXT)
    write_framer(sc, Bt8370_CMUX, 0x0C); /* external timing */
  else
    write_framer(sc, Bt8370_CMUX, 0x0F); /* internal timing */

  /* 020:LIU_CR -- Line Interface Unit Config Register */
  write_framer(sc, Bt8370_LIU_CR, 0xC1); /* reset LIU, squelch */

  /* 022:RLIU_CR -- RX Line Interface Unit Config Reg */
  /* Errata sheet says don't use freeze-short, but we do anyway! */
  write_framer(sc, Bt8370_RLIU_CR, 0xB1); /* AGC=2048, Long Eye */

  /* Select Rx sensitivity based on cable length. */
  if ((gain = sc->config.rx_gain) == CFG_GAIN_AUTO)
    {
    if      (sc->config.cable_len > 2000)
      gain = CFG_GAIN_EXTEND;
    else if (sc->config.cable_len > 1000)
      gain = CFG_GAIN_LONG;
    else if (sc->config.cable_len > 100)
      gain = CFG_GAIN_MEDIUM;
    else
      gain = CFG_GAIN_SHORT;
    }

  /* 024:VGA_MAX -- Variable Gain Amplifier Max gain */
  write_framer(sc, Bt8370_VGA_MAX, gain);

  /* 028:PRE_EQ -- Pre Equalizer */
  if (gain == CFG_GAIN_EXTEND)
    write_framer(sc, Bt8370_PRE_EQ, 0xE6);  /* ON; thresh 6 */
  else
    write_framer(sc, Bt8370_PRE_EQ, 0xA6);  /* OFF; thresh 6 */

  /* 038-03C:GAINn -- RX Equalizer gain thresholds */
  write_framer(sc, Bt8370_GAIN0, 0x24);
  write_framer(sc, Bt8370_GAIN1, 0x28);
  write_framer(sc, Bt8370_GAIN2, 0x2C);
  write_framer(sc, Bt8370_GAIN3, 0x30);
  write_framer(sc, Bt8370_GAIN4, 0x34);

  /* 040:RCR0 -- Receiver Control Register 0 */
  if      (FORMAT_T1ESF)
    write_framer(sc, Bt8370_RCR0, 0x05); /* B8ZS, 2/5 FErrs */
  else if (FORMAT_T1SF)
    write_framer(sc, Bt8370_RCR0, 0x84); /* AMI,  2/5 FErrs */
  else if (FORMAT_E1NONE)
    write_framer(sc, Bt8370_RCR0, 0x41); /* HDB3, rabort */
  else if (FORMAT_E1CRC)
    write_framer(sc, Bt8370_RCR0, 0x09); /* HDB3, 3 FErrs or 915 CErrs */
  else  /* E1 no CRC */
    write_framer(sc, Bt8370_RCR0, 0x19); /* HDB3, 3 FErrs */

  /* 041:RPATT -- Receive Test Pattern configuration */
  write_framer(sc, Bt8370_RPATT, 0x3E); /* looking for framed QRSS */

  /* 042:RLB -- Receive Loop Back code detector config */
  write_framer(sc, Bt8370_RLB, 0x09); /* 6 bits down; 5 bits up */

  /* 043:LBA -- Loop Back Activate code */
  write_framer(sc, Bt8370_LBA, 0x08); /* 10000 10000 10000 ... */

  /* 044:LBD -- Loop Back Deactivate code */
  write_framer(sc, Bt8370_LBD, 0x24); /* 100100 100100 100100 ... */

  /* 045:RALM -- Receive Alarm signal configuration */
  write_framer(sc, Bt8370_RALM, 0x0C); /* yel_intg rlof_intg */

  /* 046:LATCH -- Alarm/Error/Counter Latch register */
  write_framer(sc, Bt8370_LATCH, 0x1F); /* stop_cnt latch_{cnt,err,alm} */

  /* Select Pulse Shape based on cable length (T1 only). */
  if ((pulse = sc->config.tx_pulse) == CFG_PULSE_AUTO)
    {
    if (FORMAT_T1ANY)
      {
      if      (sc->config.cable_len > 200)
        pulse = CFG_PULSE_T1CSU;
      else if (sc->config.cable_len > 160)
        pulse = CFG_PULSE_T1DSX4;
      else if (sc->config.cable_len > 120)
        pulse = CFG_PULSE_T1DSX3;
      else if (sc->config.cable_len > 80)
        pulse = CFG_PULSE_T1DSX2;
      else if (sc->config.cable_len > 40)
        pulse = CFG_PULSE_T1DSX1;
      else
        pulse = CFG_PULSE_T1DSX0;
      }
    else
      pulse = CFG_PULSE_E1TWIST;
    }

  /* Select Line Build Out based on cable length (T1CSU only). */
  if ((lbo = sc->config.tx_lbo) == CFG_LBO_AUTO)
    {
    if (pulse == CFG_PULSE_T1CSU)
      {
      if      (sc->config.cable_len > 1500)
        lbo = CFG_LBO_0DB;
      else if (sc->config.cable_len > 1000)
        lbo = CFG_LBO_7DB;
      else if (sc->config.cable_len >  500)
        lbo = CFG_LBO_15DB;
      else
        lbo = CFG_LBO_22DB;
      }
    else
      lbo = 0;
    }

  /* 068:TLIU_CR -- Transmit LIU Control Register */
  write_framer(sc, Bt8370_TLIU_CR, (0x40 | (lbo & 0x30) | (pulse & 0x0E)));

  /* 070:TCR0 -- Transmit Framer Configuration */
  write_framer(sc, Bt8370_TCR0, sc->config.format>>1);

  /* 071:TCR1 -- Transmitter Configuration */
  if (FORMAT_T1SF)
    write_framer(sc, Bt8370_TCR1, 0x43); /* tabort, AMI PDV enforced */
  else
    write_framer(sc, Bt8370_TCR1, 0x41); /* tabort, B8ZS or HDB3 */

  /* 072:TFRM -- Transmit Frame format       MYEL YEL MF FE CRC FBIT */
  if      (sc->config.format == CFG_FORMAT_T1ESF)
    write_framer(sc, Bt8370_TFRM, 0x0B); /*  -   YEL MF -  CRC FBIT */
  else if (sc->config.format == CFG_FORMAT_T1SF)
    write_framer(sc, Bt8370_TFRM, 0x19); /*  -   YEL MF -   -  FBIT */
  else if (sc->config.format == CFG_FORMAT_E1FAS)
    write_framer(sc, Bt8370_TFRM, 0x11); /*  -   YEL -  -   -  FBIT */
  else if (sc->config.format == CFG_FORMAT_E1FASCRC)
    write_framer(sc, Bt8370_TFRM, 0x1F); /*  -   YEL MF FE CRC FBIT */
  else if (sc->config.format == CFG_FORMAT_E1FASCAS)
    write_framer(sc, Bt8370_TFRM, 0x31); /* MYEL YEL -  -   -  FBIT */
  else if (sc->config.format == CFG_FORMAT_E1FASCRCCAS)
    write_framer(sc, Bt8370_TFRM, 0x3F); /* MYEL YEL MF FE CRC FBIT */
  else if (sc->config.format == CFG_FORMAT_E1NONE)
    write_framer(sc, Bt8370_TFRM, 0x00); /* NO FRAMING BITS AT ALL! */

  /* 073:TERROR -- Transmit Error Insert */
  write_framer(sc, Bt8370_TERROR, 0x00); /* no errors, please! */

  /* 074:TMAN -- Transmit Manual Sa-byte/FEBE configuration */
  write_framer(sc, Bt8370_TMAN, 0x00); /* none */

  /* 075:TALM -- Transmit Alarm Signal Configuration */
  if (FORMAT_E1ANY)
    write_framer(sc, Bt8370_TALM, 0x38); /* auto_myel auto_yel auto_ais */
  else if (FORMAT_T1ANY)
    write_framer(sc, Bt8370_TALM, 0x18); /* auto_yel auto_ais */

  /* 076:TPATT -- Transmit Test Pattern Configuration */
  write_framer(sc, Bt8370_TPATT, 0x00); /* disabled */

  /* 077:TLB -- Transmit Inband Loopback Code Configuration */
  write_framer(sc, Bt8370_TLB, 0x00); /* disabled */

  /* 090:CLAD_CR -- Clack Rate Adapter Configuration */
  if (FORMAT_T1ANY)
    write_framer(sc, Bt8370_CLAD_CR, 0x06); /* loop filter gain 1/2^6 */
  else
    write_framer(sc, Bt8370_CLAD_CR, 0x08); /* loop filter gain 1/2^8 */

  /* 091:CSEL -- CLAD frequency Select */
  if (FORMAT_T1ANY)
    write_framer(sc, Bt8370_CSEL, 0x55); /* 1544 kHz */
  else
    write_framer(sc, Bt8370_CSEL, 0x11); /* 2048 kHz */

  /* 092:CPHASE -- CLAD Phase detector */
  if (FORMAT_T1ANY)
    write_framer(sc, Bt8370_CPHASE, 0x22); /* phase compare @  386 kHz */
  else
    write_framer(sc, Bt8370_CPHASE, 0x00); /* phase compare @ 2048 kHz */

  if (FORMAT_T1ESF) /* BOP & PRM are enabled in T1ESF mode only. */
    {
    /* 0A0:BOP -- Bit Oriented Protocol messages */
    write_framer(sc, Bt8370_BOP, RBOP_25 | TBOP_OFF);
    /* 0A4:DL1_TS -- Data Link 1 Time Slot Enable */
    write_framer(sc, Bt8370_DL1_TS, 0x40); /* FDL bits in odd frames */
    /* 0A6:DL1_CTL -- Data Link 1 Control */
    write_framer(sc, Bt8370_DL1_CTL, 0x03); /* FCS mode, TX on, RX on */
    /* 0A7:RDL1_FFC -- Rx Data Link 1 Fifo Fill Control */
    write_framer(sc, Bt8370_RDL1_FFC, 0x30); /* assert "near full" at 48 */
    /* 0AA:PRM -- Performance Report Messages */
    write_framer(sc, Bt8370_PRM, 0x80);
    }

  /* 0D0:SBI_CR -- System Bus Interface Configuration Register */
  if (FORMAT_T1ANY)
    write_framer(sc, Bt8370_SBI_CR, 0x47); /* 1.544 with 24 TS +Fbits */
  else
    write_framer(sc, Bt8370_SBI_CR, 0x46); /* 2.048 with 32 TS */

  /* 0D1:RSB_CR -- Receive System Bus Configuration Register */
  /* Change RINDO & RFSYNC on falling edge of RSBCLKI. */
  write_framer(sc, Bt8370_RSB_CR, 0x70);

  /* 0D2,0D3:RSYNC_{TS,BIT} -- Receive frame Sync offset */
  write_framer(sc, Bt8370_RSYNC_BIT, 0x00);
  write_framer(sc, Bt8370_RSYNC_TS,  0x00);

  /* 0D4:TSB_CR -- Transmit System Bus Configuration Register */
  /* Change TINDO & TFSYNC on falling edge of TSBCLKI. */
  write_framer(sc, Bt8370_TSB_CR, 0x30);

  /* 0D5,0D6:TSYNC_{TS,BIT} -- Transmit frame Sync offset */
  write_framer(sc, Bt8370_TSYNC_BIT, 0x00);
  write_framer(sc, Bt8370_TSYNC_TS,  0x00);

  /* 0D7:RSIG_CR -- Receive SIGnalling Configuratin Register */
  write_framer(sc, Bt8370_RSIG_CR, 0x00);

  /* Assign and configure 64Kb TIME SLOTS. */
  /* TS24..TS1 must be assigned for T1, TS31..TS0 for E1. */
  /* Timeslots with no user data have RINDO and TINDO off. */
  for (i=0; i<32; i++)
    {
    /* 0E0-0FF:SBCn -- System Bus Per-Channel Control */
    if      (FORMAT_T1ANY && (i==0 || i>24))
      write_framer(sc, Bt8370_SBCn +i, 0x00); /* not assigned in T1 mode */
    else if (FORMAT_E1ANY && (i==0)  && !FORMAT_E1NONE)
      write_framer(sc, Bt8370_SBCn +i, 0x01); /* assigned, TS0  o/h bits */
    else if (FORMAT_E1CAS && (i==16) && !FORMAT_E1NONE)
      write_framer(sc, Bt8370_SBCn +i, 0x01); /* assigned, TS16 o/h bits */
    else if ((sc->config.time_slots & (1<<i)) != 0)
      write_framer(sc, Bt8370_SBCn +i, 0x0D); /* assigned, RINDO, TINDO */
    else
      write_framer(sc, Bt8370_SBCn +i, 0x01); /* assigned, idle */

    /* 100-11F:TPCn -- Transmit Per-Channel Control */
    if      (FORMAT_E1CAS && (i==0))
      write_framer(sc, Bt8370_TPCn +i, 0x30); /* tidle, sig=0000 (MAS) */
    else if (FORMAT_E1CAS && (i==16))
      write_framer(sc, Bt8370_TPCn +i, 0x3B); /* tidle, sig=1011 (XYXX) */
    else if ((sc->config.time_slots & (1<<i)) == 0)
      write_framer(sc, Bt8370_TPCn +i, 0x20); /* tidle: use TSLIP_LOn */
    else
      write_framer(sc, Bt8370_TPCn +i, 0x00); /* nothing special */

    /* 140-15F:TSLIP_LOn -- Transmit PCM Slip Buffer */
    write_framer(sc, Bt8370_TSLIP_LOn +i, 0x7F); /* idle chan data */
    /* 180-19F:RPCn -- Receive Per-Channel Control */
    write_framer(sc, Bt8370_RPCn +i, 0x00);   /* nothing special */
    }

  /* Enable transmitter output drivers. */
  set_mii16_bits(sc, MII16_T1_XOE);
  }

static void
t1_ident(softc_t *sc)
  {
  printf(", Bt837%x rev %x",
   read_framer(sc, Bt8370_DID)>>4,
   read_framer(sc, Bt8370_DID)&0x0F);
  }

/* Called once a second; must not sleep. */
static int
t1_watchdog(softc_t *sc)
  {
  u_int16_t LCV = 0, FERR = 0, CRC = 0, FEBE = 0;
  u_int8_t alm1, alm3, loop, isr0;
  int link_status = STATUS_UP;
  int i;

  /* Read the alarm registers */
  alm1 = read_framer(sc, Bt8370_ALM1);
  alm3 = read_framer(sc, Bt8370_ALM3);
  loop = read_framer(sc, Bt8370_LOOP);
  isr0 = read_framer(sc, Bt8370_ISR0);

  /* Always ignore the SIGFRZ alarm bit, */
  alm1 &= ~ALM1_SIGFRZ;
  if (FORMAT_T1ANY)  /* ignore RYEL in T1 modes */
    alm1 &= ~ALM1_RYEL;
  else if (FORMAT_E1NONE) /* ignore all alarms except LOS */
    alm1 &= ALM1_RLOS;

  /* Software is alive. */
  led_inv(sc, MII16_T1_LED_GRN);

  /* Receiving Alarm Indication Signal (AIS). */
  if ((alm1 & ALM1_RAIS)!=0) /* receiving ais */
    led_on(sc, MII16_T1_LED_BLU);
  else if ((alm1 & ALM1_RLOS)!=0) /* sending ais */
    led_inv(sc, MII16_T1_LED_BLU);
  else
    led_off(sc, MII16_T1_LED_BLU);

  /* Receiving Remote Alarm Indication (RAI). */
  if ((alm1 & (ALM1_RMYEL | ALM1_RYEL))!=0) /* receiving rai */
    led_on(sc, MII16_T1_LED_YEL);
  else if ((alm1 & ALM1_RLOF)!=0) /* sending rai */
    led_inv(sc, MII16_T1_LED_YEL);
  else
    led_off(sc, MII16_T1_LED_YEL);

  /* If any alarm bits are set then the link is 'down'. */
  /* The bad bits are: rmyel ryel rais ralos rlos rlof. */
  /* Some alarm bits have been masked by this point. */
  if (alm1 != 0) link_status = STATUS_DOWN;

  /* Declare local Red Alarm if the link is down. */
  if (link_status == STATUS_DOWN)
    led_on(sc, MII16_T1_LED_RED);
  else if (sc->loop_timer != 0) /* loopback is active */
    led_inv(sc, MII16_T1_LED_RED);
  else
    led_off(sc, MII16_T1_LED_RED);

  /* Print latched error bits if they changed. */
  if ((DRIVER_DEBUG) && (alm1 != sc->last_alm1))
    {
    char *on = "ON ", *off = "OFF";
    printf("%s: RLOF=%s RLOS=%s RALOS=%s RAIS=%s RYEL=%s RMYEL=%s\n",
     NAME_UNIT,
     (alm1 & ALM1_RLOF)  ? on : off,
     (alm1 & ALM1_RLOS)  ? on : off,
     (alm1 & ALM1_RALOS) ? on : off,
     (alm1 & ALM1_RAIS)  ? on : off,
     (alm1 & ALM1_RYEL)  ? on : off,
     (alm1 & ALM1_RMYEL) ? on : off);
    }

  /* Check and print error counters if non-zero. */
  LCV = read_framer(sc, Bt8370_LCV_LO)  +
        (read_framer(sc, Bt8370_LCV_HI)<<8);
  if (!FORMAT_E1NONE)
    FERR = read_framer(sc, Bt8370_FERR_LO) +
          (read_framer(sc, Bt8370_FERR_HI)<<8);
  if (FORMAT_E1CRC || FORMAT_T1ESF)
    CRC  = read_framer(sc, Bt8370_CRC_LO)  +
          (read_framer(sc, Bt8370_CRC_HI)<<8);
  if (FORMAT_E1CRC)
    FEBE = read_framer(sc, Bt8370_FEBE_LO) +
          (read_framer(sc, Bt8370_FEBE_HI)<<8);
  /* Only LCV is valid if Out-Of-Frame */
  if (FORMAT_E1NONE) FERR = CRC = FEBE = 0;
  if ((DRIVER_DEBUG) && (LCV || FERR || CRC || FEBE))
    printf("%s: LCV=%u FERR=%u CRC=%u FEBE=%u\n",
     NAME_UNIT, LCV,   FERR,   CRC,   FEBE);

  /* Driver keeps crude link-level error counters (SNMP is better). */
  sc->status.cntrs.lcv_errs  += LCV;
  sc->status.cntrs.frm_errs  += FERR;
  sc->status.cntrs.crc_errs  += CRC;
  sc->status.cntrs.febe_errs += FEBE;

  /* Check for BOP messages in the ESF Facility Data Link. */
  if ((FORMAT_T1ESF) && (read_framer(sc, Bt8370_ISR1) & 0x80))
    {
    u_int8_t bop_code = read_framer(sc, Bt8370_RBOP) & 0x3F;

    switch (bop_code)
      {
      case T1BOP_OOF:
        {
        if ((DRIVER_DEBUG) && ((sc->last_alm1 & ALM1_RMYEL)==0))
          printf("%s: Receiving a 'yellow alarm' BOP msg\n", NAME_UNIT);
        break;
        }
      case T1BOP_LINE_UP:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'line loopback activate' BOP msg\n", NAME_UNIT);
        write_framer(sc, Bt8370_LOOP, LOOP_LINE);
        sc->loop_timer = 305;
        break;
        }
      case T1BOP_LINE_DOWN:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'line loopback deactivate' BOP msg\n", NAME_UNIT);
        write_framer(sc, Bt8370_LOOP,
         read_framer(sc, Bt8370_LOOP) & ~LOOP_LINE);
        sc->loop_timer = 0;
        break;
        }
      case T1BOP_PAY_UP:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'payload loopback activate' BOP msg\n", NAME_UNIT);
        write_framer(sc, Bt8370_LOOP, LOOP_PAYLOAD);
        sc->loop_timer = 305;
        break;
        }
      case T1BOP_PAY_DOWN:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a 'payload loopback deactivate' BOP msg\n", NAME_UNIT);
        write_framer(sc, Bt8370_LOOP,
         read_framer(sc, Bt8370_LOOP) & ~LOOP_PAYLOAD);
        sc->loop_timer = 0;
        break;
        }
      default:
        {
        if (DRIVER_DEBUG)
          printf("%s: Received a type 0x%02X BOP msg\n", NAME_UNIT, bop_code);
        break;
        }
      }
    }

  /* Check for HDLC pkts in the ESF Facility Data Link. */
  if ((FORMAT_T1ESF) && (read_framer(sc, Bt8370_ISR2) & 0x70))
    {
    /* while (not fifo-empty && not start-of-msg) flush fifo */
    while ((read_framer(sc, Bt8370_RDL1_STAT) & 0x0C) == 0)
      read_framer(sc, Bt8370_RDL1);
    /* If (not fifo-empty), then begin processing fifo contents. */
    if ((read_framer(sc, Bt8370_RDL1_STAT) & 0x0C) == 0x08)
      {
      u_int8_t msg[64];
      u_int8_t stat = read_framer(sc, Bt8370_RDL1);
      sc->status.cntrs.fdl_pkts++;
      for (i=0; i<(stat & 0x3F); i++)
        msg[i] = read_framer(sc, Bt8370_RDL1);
      /* Is this FDL message a T1.403 performance report? */
      if (((stat & 0x3F)==11) &&
          ((msg[0]==0x38) || (msg[0]==0x3A)) &&
           (msg[1]==1)   &&  (msg[2]==3))
        /* Copy 4 PRs from FDL pkt to SNMP struct. */
        memcpy(sc->status.snmp.t1.prm, msg+3, 8);
      }
    }

  /* Check for inband loop up/down commands. */
  if (FORMAT_T1ANY)
    {
    u_int8_t isr6   = read_framer(sc, Bt8370_ISR6);
    u_int8_t alarm2 = read_framer(sc, Bt8370_ALM2);
    u_int8_t tlb    = read_framer(sc, Bt8370_TLB);

    /* Inband Code == Loop Up && On Transition && Inband Tx Inactive */
    if ((isr6 & 0x40) && (alarm2 & 0x40) && ((tlb & 1)==0))
      { /* CSU loop up is 10000 10000 ... */
      if (DRIVER_DEBUG)
        printf("%s: Received a 'CSU Loop Up' inband msg\n", NAME_UNIT);
      write_framer(sc, Bt8370_LOOP, LOOP_LINE); /* Loop up */
      sc->loop_timer = 305;
      }
    /* Inband Code == Loop Down && On Transition && Inband Tx Inactive */
    if ((isr6 & 0x80) && (alarm2 & 0x80) && ((tlb & 1)==0))
      { /* CSU loop down is 100 100 100 ... */
      if (DRIVER_DEBUG)
        printf("%s: Received a 'CSU Loop Down' inband msg\n", NAME_UNIT);
      write_framer(sc, Bt8370_LOOP,
       read_framer(sc, Bt8370_LOOP) & ~LOOP_LINE); /* loop down */
      sc->loop_timer = 0;
      }
    }

  /* Manually send Yellow Alarm BOP msgs. */
  if (FORMAT_T1ESF)
    {
    u_int8_t isr7 = read_framer(sc, Bt8370_ISR7);

    if ((isr7 & 0x02) && (alm1 & 0x02)) /* RLOF on-transition */
      { /* Start sending continuous Yellow Alarm BOP messages. */
      write_framer(sc, Bt8370_BOP,  RBOP_25 | TBOP_CONT);
      write_framer(sc, Bt8370_TBOP, 0x00); /* send BOP; order matters */
      }
    else if ((isr7 & 0x02) && ((alm1 & 0x02)==0)) /* RLOF off-transition */
      { /* Stop sending continuous Yellow Alarm BOP messages. */
      write_framer(sc, Bt8370_BOP,  RBOP_25 | TBOP_OFF);
      }
    }

  /* Time out loopback requests. */
  if (sc->loop_timer != 0)
    if (--sc->loop_timer == 0)
      if (loop != 0)
        {
        if (DRIVER_DEBUG)
          printf("%s: Timeout: Loop Down after 300 seconds\n", NAME_UNIT);
        write_framer(sc, Bt8370_LOOP, loop & ~(LOOP_PAYLOAD | LOOP_LINE));
        }

  /* RX Test Pattern status */
  if ((DRIVER_DEBUG) && (isr0 & 0x10))
    printf("%s: RX Test Pattern Sync\n", NAME_UNIT);

  /* SNMP Error Counters */
  sc->status.snmp.t1.lcv  = LCV;
  sc->status.snmp.t1.fe   = FERR;
  sc->status.snmp.t1.crc  = CRC;
  sc->status.snmp.t1.febe = FEBE;

  /* SNMP Line Status */
  sc->status.snmp.t1.line = 0;
  if  (alm1 & ALM1_RMYEL)  sc->status.snmp.t1.line |= TLINE_RX_RAI;
  if  (alm1 & ALM1_RYEL)   sc->status.snmp.t1.line |= TLINE_RX_RAI;
  if  (alm1 & ALM1_RLOF)   sc->status.snmp.t1.line |= TLINE_TX_RAI;
  if  (alm1 & ALM1_RAIS)   sc->status.snmp.t1.line |= TLINE_RX_AIS;
  if  (alm1 & ALM1_RLOS)   sc->status.snmp.t1.line |= TLINE_TX_AIS;
  if  (alm1 & ALM1_RLOF)   sc->status.snmp.t1.line |= TLINE_LOF;
  if  (alm1 & ALM1_RLOS)   sc->status.snmp.t1.line |= TLINE_LOS;
  if  (alm3 & ALM3_RMAIS)  sc->status.snmp.t1.line |= T1LINE_RX_TS16_AIS;
  if  (alm3 & ALM3_SRED)   sc->status.snmp.t1.line |= T1LINE_TX_TS16_LOMF;
  if  (alm3 & ALM3_SEF)    sc->status.snmp.t1.line |= T1LINE_SEF;
  if  (isr0 & 0x10)        sc->status.snmp.t1.line |= T1LINE_RX_TEST;
  if ((alm1 & ALM1_RMYEL) && (FORMAT_E1CAS))
                           sc->status.snmp.t1.line |= T1LINE_RX_TS16_LOMF;

  /* SNMP Loopback Status */
  sc->status.snmp.t1.loop &= ~(TLOOP_FAR_LINE | TLOOP_FAR_PAYLOAD);
  if (sc->config.loop_back == CFG_LOOP_TULIP)
                           sc->status.snmp.t1.loop |= TLOOP_NEAR_OTHER;
  if (loop & LOOP_PAYLOAD) sc->status.snmp.t1.loop |= TLOOP_NEAR_PAYLOAD;
  if (loop & LOOP_LINE)    sc->status.snmp.t1.loop |= TLOOP_NEAR_LINE;
  if (loop & LOOP_ANALOG)  sc->status.snmp.t1.loop |= TLOOP_NEAR_OTHER;
  if (loop & LOOP_FRAMER)  sc->status.snmp.t1.loop |= TLOOP_NEAR_INWARD;

  /* Remember this state until next time. */
  sc->last_alm1 = alm1;

  /* If an INWARD loopback is in effect, link status is UP */
  if (sc->config.loop_back != CFG_LOOP_NONE) /* XXX INWARD ONLY */
    link_status = STATUS_UP;

  return link_status;
  }

/* IOCTL SYSCALL: can sleep. */
static void
t1_send_bop(softc_t *sc, int bop_code)
  {
  u_int8_t bop;
  int i;

  /* The BOP transmitter could be sending a continuous */
  /*  BOP msg when told to send this BOP_25 message. */
  /* So save and restore the state of the BOP machine. */
  bop = read_framer(sc, Bt8370_BOP);
  write_framer(sc, Bt8370_BOP, RBOP_OFF | TBOP_OFF);
  for (i=0; i<40; i++) /* max delay 400 ms. */
    if (read_framer(sc, Bt8370_BOP_STAT) & 0x80) SLEEP(10000);
  /* send 25 repetitions of bop_code */
  write_framer(sc, Bt8370_BOP, RBOP_OFF | TBOP_25);
  write_framer(sc, Bt8370_TBOP, bop_code); /* order matters */
  /* wait for tx to stop */
  for (i=0; i<40; i++) /* max delay 400 ms. */
    if (read_framer(sc, Bt8370_BOP_STAT) & 0x80) SLEEP(10000);
  /* Restore previous state of the BOP machine. */
  write_framer(sc, Bt8370_BOP, bop);
  }

/* IOCTL SYSCALL: can sleep. */
static int
t1_ioctl(softc_t *sc, struct ioctl *ioctl)
  {
  int error = 0;

  switch (ioctl->cmd)
    {
    case IOCTL_SNMP_SEND:  /* set opstatus? */
      {
      switch (ioctl->data)
        {
        case TSEND_NORMAL:
          {
          write_framer(sc, Bt8370_TPATT, 0x00); /* tx pattern generator off */
          write_framer(sc, Bt8370_RPATT, 0x00); /* rx pattern detector off */
          write_framer(sc, Bt8370_TLB,   0x00); /* tx inband generator off */
          break;
	  }
        case TSEND_LINE:
          {
          if (FORMAT_T1ESF)
            t1_send_bop(sc, T1BOP_LINE_UP);
          else if (FORMAT_T1SF)
            {
            write_framer(sc, Bt8370_LBP, 0x08); /* 10000 10000 ... */
            write_framer(sc, Bt8370_TLB, 0x05); /* 5 bits, framed, start */
	    }
          sc->status.snmp.t1.loop |= TLOOP_FAR_LINE;
          break;
	  }
        case TSEND_PAYLOAD:
          {
          t1_send_bop(sc, T1BOP_PAY_UP);
          sc->status.snmp.t1.loop |= TLOOP_FAR_PAYLOAD;
          break;
	  }
        case TSEND_RESET:
          {
          if (sc->status.snmp.t1.loop == TLOOP_FAR_LINE)
            {
            if (FORMAT_T1ESF)
              t1_send_bop(sc, T1BOP_LINE_DOWN);
            else if (FORMAT_T1SF)
              {
              write_framer(sc, Bt8370_LBP, 0x24); /* 100100 100100 ... */
              write_framer(sc, Bt8370_TLB, 0x09); /* 6 bits, framed, start */
	      }
            sc->status.snmp.t1.loop &= ~TLOOP_FAR_LINE;
	    }
          if (sc->status.snmp.t1.loop == TLOOP_FAR_PAYLOAD)
            {
            t1_send_bop(sc, T1BOP_PAY_DOWN);
            sc->status.snmp.t1.loop &= ~TLOOP_FAR_PAYLOAD;
	    }
          break;
	  }
        case TSEND_QRS:
          {
          write_framer(sc, Bt8370_TPATT, 0x1E); /* framed QRSS */
          break;
	  }
        default:
          {
          error = EINVAL;
          break;
	  }
	}
      break;
      }
    case IOCTL_SNMP_LOOP:  /* set opstatus = test? */
      {
      u_int8_t new_loop = 0;

      if (ioctl->data == CFG_LOOP_NONE)
        new_loop = 0;
      else if (ioctl->data == CFG_LOOP_PAYLOAD)
        new_loop = LOOP_PAYLOAD;
      else if (ioctl->data == CFG_LOOP_LINE)
        new_loop = LOOP_LINE;
      else if (ioctl->data == CFG_LOOP_OTHER)
        new_loop = LOOP_ANALOG;
      else if (ioctl->data == CFG_LOOP_INWARD)
        new_loop = LOOP_FRAMER;
      else if (ioctl->data == CFG_LOOP_DUAL)
        new_loop = LOOP_DUAL;
      else
        error = EINVAL;
      if (error == 0)
        {
        write_framer(sc, Bt8370_LOOP, new_loop);
        sc->config.loop_back = ioctl->data;
	}
      break;
      }
    default:
      error = EINVAL;
      break;
    }

  return error;
  }

static
struct card hssi_card =
  {
  .config   = hssi_config,
  .ident    = hssi_ident,
  .watchdog = hssi_watchdog,
  .ioctl    = hssi_ioctl,
  };

static
struct card t3_card =
  {
  .config   = t3_config,
  .ident    = t3_ident,
  .watchdog = t3_watchdog,
  .ioctl    = t3_ioctl,
  };

static
struct card ssi_card =
  {
  .config   = ssi_config,
  .ident    = ssi_ident,
  .watchdog = ssi_watchdog,
  .ioctl    = ssi_ioctl,
  };

static
struct card t1_card =
  {
  .config   = t1_config,
  .ident    = t1_ident,
  .watchdog = t1_watchdog,
  .ioctl    = t1_ioctl,
  };

/* RAWIP is raw IP packets (v4 or v6) in HDLC frames with NO HEADERS. */
/* No HDLC Address/Control fields!  No line control protocol at all!  */
/* This code is BSD/ifnet-specific; Linux and Netgraph also do RAWIP. */

#if IFNET

# if ((defined(__FreeBSD__) && (__FreeBSD_version < 500000)) ||\
        defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__))
static void
netisr_dispatch(int isr, struct mbuf *mbuf)
  {
  struct ifqueue *intrq = NULL;
  int qfull = 0;

#if INET
  if (isr == NETISR_IP)   intrq = &ipintrq;
#endif
#if INET6
  if (isr == NETISR_IPV6) intrq = &ip6intrq;
#endif

  if ((intrq != NULL) && ((qfull = IF_QFULL(intrq)) == 0))
    {
    /* rxintr_cleanup() ENQUEUES in a hard interrupt. */
    /* networking code DEQUEUES in a soft interrupt. */
    /* Some BSD QUEUE routines are not interrupt-safe. */
    DISABLE_INTR; /* noop in FreeBSD */
    IF_ENQUEUE(intrq, mbuf);
    ENABLE_INTR;
    schednetisr(isr); /* schedule a soft interrupt */
    }
  else
    {
    m_freem(mbuf);
    if ((intrq != NULL) && (qfull != 0))
      IF_DROP(intrq);
    }
  }
# endif /* ((__FreeBSD__ && (__FreeBSD_version < 500000)) || */
           /* __NetBSD__ || __OpenBSD__ || __bsdi__) */

/* rxintr_cleanup calls this to give a newly arrived pkt to higher levels. */
static void
raw_input(struct ifnet *ifp, struct mbuf *mbuf)
  {
  softc_t *sc = IFP2SC(ifp);

# if INET
  if (mbuf->m_data[0]>>4 == 4)
    netisr_dispatch(NETISR_IP,   mbuf);
  else
# endif
# if INET6
  if (mbuf->m_data[0]>>4 == 6)
    netisr_dispatch(NETISR_IPV6, mbuf);
  else
# endif
    {
    m_freem(mbuf);
    sc->status.cntrs.idiscards++;
    if (DRIVER_DEBUG)
      printf("%s: raw_input: rx pkt discarded: not IPv4 or IPv6\n", NAME_UNIT);
    }
  }

#endif /* IFNET */

/* There are TWO VERSIONS of interrupt/DMA code: Linux & BSD.
 * Handling Linux and the BSDs with CPP directives would
 *  make the code unreadable, so there are two versions.
 * Conceptually, the two versions do the same thing and
 *  core_interrupt() doesn't know they are different.
 *
 * We are "standing on the head of a pin" in these routines.
 * Tulip CSRs can be accessed, but nothing else is interrupt-safe!
 * Do NOT access: MII, GPIO, SROM, BIOSROM, XILINX, SYNTH, or DAC.
 */

#if BSD /* BSD version of interrupt/DMA code */

/* Singly-linked tail-queues hold mbufs with active DMA.
 * For RX, single mbuf clusters; for TX, mbuf chains are queued.
 * NB: mbufs are linked through their m_nextpkt field.
 * Callers must hold sc->bottom_lock; not otherwise locked.
 */

/* Put an mbuf (chain) on the tail of the descriptor ring queue. */
static void  /* BSD version */
mbuf_enqueue(struct desc_ring *ring, struct mbuf *m)
  {
  m->m_nextpkt = NULL;
  if (ring->tail == NULL)
    ring->head = m;
  else
    ring->tail->m_nextpkt = m;
  ring->tail = m;
  }

/* Get an mbuf (chain) from the head of the descriptor ring queue. */
static struct mbuf*  /* BSD version */
mbuf_dequeue(struct desc_ring *ring)
  {
  struct mbuf *m = ring->head;
  if (m != NULL)
    if ((ring->head = m->m_nextpkt) == NULL)
      ring->tail = NULL;
  return m;
  }

# ifdef __FreeBSD__
static void /* *** FreeBSD ONLY *** Callout from bus_dmamap_load() */
fbsd_dmamap_load(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
  {
  struct desc_ring *ring = arg;
  ring->nsegs = error ? 0 : nsegs;
  ring->segs[0] = segs[0];
  ring->segs[1] = segs[1];
  }
# endif

/* Initialize a DMA descriptor ring. */
static int  /* BSD version */
create_ring(softc_t *sc, struct desc_ring *ring, int num_descs)
  {
  struct dma_desc *descs;
  int size_descs = sizeof(struct dma_desc)*num_descs;
  int i, error = 0;

  /* The DMA descriptor array must not cross a page boundary. */
  if (size_descs > PAGE_SIZE)
    {
    printf("%s: DMA descriptor array > PAGE_SIZE (%d)\n", NAME_UNIT, 
     (u_int)PAGE_SIZE);
    return EINVAL;
    }

#ifdef __FreeBSD__

  /* Create a DMA tag for descriptors and buffers. */
  if ((error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
   BUS_SPACE_MAXADDR, NULL, NULL, PAGE_SIZE, 2, PAGE_SIZE, BUS_DMA_ALLOCNOW,
# if (__FreeBSD_version >= 502000)
   NULL, NULL,
# endif
   &ring->tag)))
    {
    printf("%s: bus_dma_tag_create() failed: error %d\n", NAME_UNIT, error);
    return error;
    }

  /* Allocate wired physical memory for DMA descriptor array */
  /*  and map physical address to kernel virtual address. */
  if ((error = bus_dmamem_alloc(ring->tag, (void**)&ring->first,
   BUS_DMA_NOWAIT | BUS_DMA_COHERENT | BUS_DMA_ZERO, &ring->map)))
    {
    printf("%s: bus_dmamem_alloc() failed; error %d\n", NAME_UNIT, error);
    return error;
    }
  descs = ring->first;

  /* Map kernel virtual address to PCI address for DMA descriptor array. */
  if ((error = bus_dmamap_load(ring->tag, ring->map, descs, size_descs,
   fbsd_dmamap_load, ring, 0)))
    {
    printf("%s: bus_dmamap_load() failed; error %d\n", NAME_UNIT, error);
    return error;
    }
  ring->dma_addr = ring->segs[0].ds_addr;

  /* Allocate dmamaps for each DMA descriptor. */
  for (i=0; i<num_descs; i++)
    if ((error = bus_dmamap_create(ring->tag, 0, &descs[i].map)))
      {
      printf("%s: bus_dmamap_create() failed; error %d\n", NAME_UNIT, error);
      return error;
      }

#elif (defined(__NetBSD__) || defined(__OpenBSD__))

  /* Use the DMA tag passed to attach() for descriptors and buffers. */
  ring->tag = sc->pa_dmat;

  /* Allocate wired physical memory for DMA descriptor array. */
  if ((error = bus_dmamem_alloc(ring->tag, size_descs, PAGE_SIZE, 0,
   ring->segs, 1, &ring->nsegs, BUS_DMA_NOWAIT)))
    {
    printf("%s: bus_dmamem_alloc() failed; error %d\n", NAME_UNIT, error);
    return error;
    }

  /* Map physical address to kernel virtual address. */
  if ((error = bus_dmamem_map(ring->tag, ring->segs, ring->nsegs,
   size_descs, (caddr_t *)&ring->first, BUS_DMA_NOWAIT | BUS_DMA_COHERENT)))
    {
    printf("%s: bus_dmamem_map() failed; error %d\n", NAME_UNIT, error);
    return error;
    }
  descs = ring->first; /* suppress compiler warning about aliasing */
  memset(descs, 0, size_descs);

  /* Allocate dmamap for PCI access to DMA descriptor array. */
  if ((error = bus_dmamap_create(ring->tag, size_descs, 1,
   size_descs, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ring->map)))
    {
    printf("%s: bus_dmamap_create() failed; error %d\n", NAME_UNIT, error);
    return error;
    }

  /* Map kernel virtual address to PCI address for DMA descriptor array. */
  if ((error = bus_dmamap_load(ring->tag, ring->map, descs, size_descs,
   0, BUS_DMA_NOWAIT)))
    {
    printf("%s: bus_dmamap_load() failed; error %d\n", NAME_UNIT, error);
    return error;
    }
  ring->dma_addr = ring->map->dm_segs[0].ds_addr;

  /* Allocate dmamaps for each DMA descriptor. */
  for (i=0; i<num_descs; i++)
    if ((error = bus_dmamap_create(ring->tag, MAX_DESC_LEN, 2,
     MAX_CHUNK_LEN, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &descs[i].map)))
      {
      printf("%s: bus_dmamap_create() failed; error %d\n", NAME_UNIT, error);
      return error;
      }

#elif defined(__bsdi__)

  /* Allocate wired physical memory for DMA descriptor array. */
  if ((ring->first = malloc(size_descs, M_DEVBUF, M_NOWAIT)) == NULL)
    {
    printf("%s: malloc() failed for DMA descriptor array\n", NAME_UNIT);
    return ENOMEM;
    }
  descs = ring->first;
  memset(descs, 0, size_descs);

  /* Map kernel virtual address to PCI address for DMA descriptor array. */
  ring->dma_addr = vtophys(descs); /* Relax! BSD/OS only. */

#endif

  ring->read  = descs;
  ring->write = descs;
  ring->first = descs;
  ring->last  = descs + num_descs -1;
  ring->last->control = TLP_DCTL_END_RING;
  ring->num_descs = num_descs;
  ring->size_descs = size_descs;
  ring->head = NULL;
  ring->tail = NULL;

  return 0;
  }

/* Destroy a DMA descriptor ring */
static void  /* BSD version */
destroy_ring(softc_t *sc, struct desc_ring *ring)
  {
  struct dma_desc *desc;
  struct mbuf *m;

  /* Free queued mbufs. */
  while ((m = mbuf_dequeue(ring)) != NULL)
    m_freem(m);

  /* TX may have one pkt that is not on any queue. */
  if (sc->tx_mbuf != NULL)
    {
    m_freem(sc->tx_mbuf);
    sc->tx_mbuf = NULL;
    }

  /* Unmap active DMA descriptors. */
  while (ring->read != ring->write)
    {
    bus_dmamap_unload(ring->tag, ring->read->map);
    if (ring->read++ == ring->last) ring->read = ring->first;
    }

#ifdef __FreeBSD__

  /* Free the dmamaps of all DMA descriptors. */
  for (desc=ring->first; desc!=ring->last+1; desc++)
    if (desc->map != NULL)
      bus_dmamap_destroy(ring->tag, desc->map);

  /* Unmap PCI address for DMA descriptor array. */
  if (ring->dma_addr != 0)
    bus_dmamap_unload(ring->tag, ring->map);
  /* Free kernel memory for DMA descriptor array. */
  if (ring->first != NULL)
    bus_dmamem_free(ring->tag, ring->first, ring->map);
  /* Free the DMA tag created for this ring. */
  if (ring->tag != NULL)
    bus_dma_tag_destroy(ring->tag);

#elif (defined(__NetBSD__) || defined(__OpenBSD__))

  /* Free the dmamaps of all DMA descriptors. */
  for (desc=ring->first; desc!=ring->last+1; desc++)
    if (desc->map != NULL)
      bus_dmamap_destroy(ring->tag, desc->map);

  /* Unmap PCI address for DMA descriptor array. */
  if (ring->dma_addr != 0)
    bus_dmamap_unload(ring->tag, ring->map);
  /* Free dmamap for DMA descriptor array. */
  if (ring->map != NULL)
    bus_dmamap_destroy(ring->tag, ring->map);
  /* Unmap kernel address for DMA descriptor array. */
  if (ring->first != NULL)
    bus_dmamem_unmap(ring->tag, (caddr_t)ring->first, ring->size_descs);
  /* Free kernel memory for DMA descriptor array. */
  if (ring->segs[0].ds_addr != 0)
    bus_dmamem_free(ring->tag, ring->segs, ring->nsegs);

#elif defined(__bsdi__)

  /* Free kernel memory for DMA descriptor array. */
  if (ring->first != NULL)
    free(ring->first, M_DEVBUF);

#endif
  }

/* Clean up after a packet has been received. */
static int  /* BSD version */
rxintr_cleanup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->rxring;
  struct dma_desc *first_desc, *last_desc;
  struct mbuf *first_mbuf=NULL, *last_mbuf=NULL;
  struct mbuf *new_mbuf;
  int pkt_len, desc_len;

#if (defined(__FreeBSD__) && DEVICE_POLLING)
  /* Input packet flow control (livelock prevention): */
  /* Give pkts to higher levels only if quota is > 0. */
  if (sc->quota <= 0) return 0;
#endif

  /* This looks complicated, but remember: typically packets up */
  /*  to 2048 bytes long fit in one mbuf and use one descriptor. */

  first_desc = last_desc = ring->read;

  /* ASSERTION: If there is a descriptor in the ring and the hardware has */
  /*  finished with it, then that descriptor will have RX_FIRST_DESC set. */
  if ((ring->read != ring->write) && /* descriptor ring not empty */
     ((ring->read->status & TLP_DSTS_OWNER) == 0) && /* hardware done */
     ((ring->read->status & TLP_DSTS_RX_FIRST_DESC) == 0)) /* should be set */
    panic("%s: rxintr_cleanup: rx-first-descriptor not set.\n", NAME_UNIT);

  /* First decide if a complete packet has arrived. */
  /* Run down DMA descriptors looking for one marked "last". */
  /* Bail out if an active descriptor is encountered. */
  /* Accumulate most significant bits of packet length. */
  pkt_len = 0;
  for (;;)
    {
    if (last_desc == ring->write) return 0;  /* no more descs */
    if (last_desc->status & TLP_DSTS_OWNER) return 0; /* still active */
    if (last_desc->status & TLP_DSTS_RX_LAST_DESC) break; /* end of packet */
    pkt_len += last_desc->length1 + last_desc->length2; /* entire desc filled */
    if (last_desc++->control & TLP_DCTL_END_RING) last_desc = ring->first; /* ring wrap */
    }

  /* A complete packet has arrived; how long is it? */
  /* H/w ref man shows RX pkt length as a 14-bit field. */
  /* An experiment found that only the 12 LSBs work. */
  if (((last_desc->status>>16)&0xFFF) == 0) pkt_len += 4096; /* carry-bit */
  pkt_len = (pkt_len & 0xF000) + ((last_desc->status>>16) & 0x0FFF);
  /* Subtract the CRC length unless doing so would underflow. */
  if (pkt_len >= sc->config.crc_len) pkt_len -= sc->config.crc_len;

  /* Run down DMA descriptors again doing the following:
   *  1) put pkt info in pkthdr of first mbuf,
   *  2) link mbufs,
   *  3) set mbuf lengths.
   */
  first_desc = ring->read;
  do
    {
    /* Read a DMA descriptor from the ring. */
    last_desc = ring->read;
    /* Advance the ring read pointer. */
    if (ring->read++ == ring->last) ring->read = ring->first;

    /* Dequeue the corresponding cluster mbuf. */
    new_mbuf = mbuf_dequeue(ring);
    if (new_mbuf == NULL)
      panic("%s: rxintr_cleanup: expected an mbuf\n", NAME_UNIT);

    desc_len = last_desc->length1 + last_desc->length2;
    /* If bouncing, copy bounce buf to mbuf. */
    DMA_SYNC(last_desc->map, desc_len, BUS_DMASYNC_POSTREAD);
    /* Unmap kernel virtual address to PCI address. */
    bus_dmamap_unload(ring->tag, last_desc->map);

    /* 1) Put pkt info in pkthdr of first mbuf. */
    if (last_desc == first_desc)
      {
      first_mbuf = new_mbuf;
      first_mbuf->m_pkthdr.len   = pkt_len; /* total pkt length */
#if IFNET
      first_mbuf->m_pkthdr.rcvif = sc->ifp; /* how it got here */
#else
      first_mbuf->m_pkthdr.rcvif = NULL;
#endif
      }
    else /* 2) link mbufs. */
      {
      last_mbuf->m_next = new_mbuf;
      /* M_PKTHDR should be set in the first mbuf only. */
      new_mbuf->m_flags &= ~M_PKTHDR;
      }
    last_mbuf = new_mbuf;

    /* 3) Set mbuf lengths. */
    new_mbuf->m_len = (pkt_len >= desc_len) ? desc_len : pkt_len;
    pkt_len -= new_mbuf->m_len;
    } while ((last_desc->status & TLP_DSTS_RX_LAST_DESC) == 0);

  /* Decide whether to accept or to discard this packet. */
  /* RxHDLC sets MIIERR for bad CRC, abort and partial byte at pkt end. */
  if (((last_desc->status & TLP_DSTS_RX_BAD) == 0) &&
   (sc->status.oper_status == STATUS_UP) &&
   (first_mbuf->m_pkthdr.len > 0))
    {
    /* Optimization: copy a small pkt into a small mbuf. */
    if (first_mbuf->m_pkthdr.len <= COPY_BREAK)
      {
      MGETHDR(new_mbuf, M_DONTWAIT, MT_DATA);
      if (new_mbuf != NULL)
        {
        new_mbuf->m_pkthdr.rcvif = first_mbuf->m_pkthdr.rcvif;
        new_mbuf->m_pkthdr.len   = first_mbuf->m_pkthdr.len;
        new_mbuf->m_len          = first_mbuf->m_len;
        memcpy(new_mbuf->m_data,   first_mbuf->m_data,
         first_mbuf->m_pkthdr.len);
        m_freem(first_mbuf);
        first_mbuf = new_mbuf;
        }
      }
    /* Include CRC and one flag byte in input byte count. */
    sc->status.cntrs.ibytes += first_mbuf->m_pkthdr.len + sc->config.crc_len +1;
    sc->status.cntrs.ipackets++;
#if IFNET
    sc->ifp->if_ipackets++;
    LMC_BPF_MTAP(first_mbuf);
#endif
#if (defined(__FreeBSD__) && DEVICE_POLLING)
    sc->quota--;
#endif

    /* Give this good packet to the network stacks. */
#if NETGRAPH
    if (sc->ng_hook != NULL) /* is hook connected? */
      {
# if (__FreeBSD_version >= 500000)
      int error;  /* ignore error */
      NG_SEND_DATA_ONLY(error, sc->ng_hook, first_mbuf);
# else /* FreeBSD-4 */
      ng_queue_data(sc->ng_hook, first_mbuf, NULL);
# endif
      return 1;  /* did something */
      }
#endif /* NETGRAPH */
    if (sc->config.line_pkg == PKG_RAWIP)
      raw_input(sc->ifp, first_mbuf);
    else
      {
#if NSPPP
      sppp_input(sc->ifp, first_mbuf);
#elif P2P
      new_mbuf = first_mbuf;
      while (new_mbuf != NULL)
        {
        sc->p2p->p2p_hdrinput(sc->p2p, new_mbuf->m_data, new_mbuf->m_len);
        new_mbuf = new_mbuf->m_next;
        }
      sc->p2p->p2p_input(sc->p2p, NULL);
      m_freem(first_mbuf);
#else
      m_freem(first_mbuf);
      sc->status.cntrs.idiscards++;
#endif
      }
    }
  else if (sc->status.oper_status != STATUS_UP)
    {
    /* If the link is down, this packet is probably noise. */
    m_freem(first_mbuf);
    sc->status.cntrs.idiscards++;
    if (DRIVER_DEBUG)
      printf("%s: rxintr_cleanup: rx pkt discarded: link down\n", NAME_UNIT);
    }
  else /* Log and discard this bad packet. */
    {
    if (DRIVER_DEBUG)
      printf("%s: RX bad pkt; len=%d %s%s%s%s\n",
       NAME_UNIT, first_mbuf->m_pkthdr.len,
       (last_desc->status & TLP_DSTS_RX_MII_ERR)  ? " miierr"  : "",
       (last_desc->status & TLP_DSTS_RX_DRIBBLE)  ? " dribble" : "",
       (last_desc->status & TLP_DSTS_RX_DESC_ERR) ? " descerr" : "",
       (last_desc->status & TLP_DSTS_RX_OVERRUN)  ? " overrun" : "");
    if (last_desc->status & TLP_DSTS_RX_OVERRUN)
      sc->status.cntrs.fifo_over++;
    else
      sc->status.cntrs.ierrors++;
    m_freem(first_mbuf);
    }

  return 1; /* did something */
  }

/* Setup (prepare) to receive a packet. */
/* Try to keep the RX descriptor ring full of empty buffers. */
static int  /* BSD version */
rxintr_setup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->rxring;
  struct dma_desc *desc;
  struct mbuf *m;
  int desc_len;
  int error;

  /* Ring is full if (wrap(write+1)==read) */
  if (((ring->write == ring->last) ? ring->first : ring->write+1) == ring->read)
    return 0;  /* ring is full; nothing to do */

  /* Allocate a small mbuf and attach an mbuf cluster. */
  MGETHDR(m, M_DONTWAIT, MT_DATA);
  if (m == NULL)
    {
    sc->status.cntrs.rxdma++;
    if (DRIVER_DEBUG)
      printf("%s: rxintr_setup: MGETHDR() failed\n", NAME_UNIT);
    return 0;
    }
  MCLGET(m, M_DONTWAIT);
  if ((m->m_flags & M_EXT) == 0)
    {
    m_freem(m);
    sc->status.cntrs.rxdma++;
    if (DRIVER_DEBUG)
      printf("%s: rxintr_setup: MCLGET() failed\n", NAME_UNIT);
    return 0;
    }

  /* Queue the mbuf for later processing by rxintr_cleanup. */
  mbuf_enqueue(ring, m);

  /* Write a DMA descriptor into the ring. */
  /* Hardware won't see it until the OWNER bit is set. */
  desc = ring->write;
  /* Advance the ring write pointer. */
  if (ring->write++ == ring->last) ring->write = ring->first;

  desc_len = (MCLBYTES < MAX_DESC_LEN) ? MCLBYTES : MAX_DESC_LEN;
  /* Map kernel virtual address to PCI address. */
  if ((error = DMA_LOAD(desc->map, m->m_data, desc_len)))
    printf("%s: bus_dmamap_load(rx) failed; error %d\n", NAME_UNIT, error);
  /* Invalidate the cache for this mbuf. */
  DMA_SYNC(desc->map, desc_len, BUS_DMASYNC_PREREAD);

  /* Set up the DMA descriptor. */
#ifdef __FreeBSD__
  desc->address1 = ring->segs[0].ds_addr;
#elif (defined(__NetBSD__) || defined(__OpenBSD__))
  desc->address1 = desc->map->dm_segs[0].ds_addr;
#elif defined(__bsdi__)
  desc->address1 = vtophys(m->m_data); /* Relax! BSD/OS only. */
#endif
  desc->length1  = desc_len>>1;
  desc->address2 = desc->address1 + desc->length1;
  desc->length2  = desc_len>>1;

  /* Before setting the OWNER bit, flush the cache (memory barrier). */
  DMA_SYNC(ring->map, ring->size_descs, BUS_DMASYNC_PREWRITE);

  /* Commit the DMA descriptor to the hardware. */
  desc->status = TLP_DSTS_OWNER;

  /* Notify the receiver that there is another buffer available. */
  WRITE_CSR(TLP_RX_POLL, 1);

  return 1; /* did something */
  }

/* Clean up after a packet has been transmitted. */
/* Free the mbuf chain and update the DMA descriptor ring. */
static int  /* BSD version */
txintr_cleanup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *desc;

  while ((ring->read != ring->write) && /* while ring is not empty */
        ((ring->read->status & TLP_DSTS_OWNER) == 0))
    {
    /* Read a DMA descriptor from the ring. */
    desc = ring->read;
    /* Advance the ring read pointer. */
    if (ring->read++ == ring->last) ring->read = ring->first;

    /* This is a no-op on most architectures. */
    DMA_SYNC(desc->map, desc->length1 + desc->length2, BUS_DMASYNC_POSTWRITE);
    /* Unmap kernel virtual address to PCI address. */
    bus_dmamap_unload(ring->tag, desc->map);

    /* If this descriptor is the last segment of a packet, */
    /*  then dequeue and free the corresponding mbuf chain. */
    if ((desc->control & TLP_DCTL_TX_LAST_SEG) != 0)
      {
      struct mbuf *m;
      if ((m = mbuf_dequeue(ring)) == NULL)
        panic("%s: txintr_cleanup: expected an mbuf\n", NAME_UNIT);

      /* Include CRC and one flag byte in output byte count. */
      sc->status.cntrs.obytes += m->m_pkthdr.len + sc->config.crc_len +1;
      sc->status.cntrs.opackets++;
#if IFNET
      sc->ifp->if_opackets++;
      LMC_BPF_MTAP(m);
#endif
      /* The only bad TX status is fifo underrun. */
      if ((desc->status & TLP_DSTS_TX_UNDERRUN) != 0)
        sc->status.cntrs.fifo_under++;

      m_freem(m);
      return 1;  /* did something */
      }
    }

  return 0;
  }

/* Build DMA descriptors for a transmit packet mbuf chain. */
static int /* 0=success; 1=error */ /* BSD version */
txintr_setup_mbuf(softc_t *sc, struct mbuf *m)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *desc;
  unsigned int desc_len;

  /* build DMA descriptors for a chain of mbufs. */
  while (m != NULL)
    {
    char *data = m->m_data;
    int length = m->m_len; /* zero length mbufs happen! */

    /* Build DMA descriptors for one mbuf. */
    while (length > 0)
      {
      int error;

      /* Ring is full if (wrap(write+1)==read) */
      if (((ring->temp==ring->last) ? ring->first : ring->temp+1) == ring->read)
        { /* Not enough DMA descriptors; try later. */
        for (; ring->temp!=ring->write;
         ring->temp = (ring->temp==ring->first)? ring->last : ring->temp-1)
          bus_dmamap_unload(ring->tag, ring->temp->map);
        sc->status.cntrs.txdma++;
        return 1;
	}

      /* Provisionally, write a descriptor into the ring. */
      /* But don't change the REAL ring write pointer. */
      /* Hardware won't see it until the OWNER bit is set. */
      desc = ring->temp;
      /* Advance the temporary ring write pointer. */
      if (ring->temp++ == ring->last) ring->temp = ring->first;

      /* Clear all control bits except the END_RING bit. */
      desc->control &= TLP_DCTL_END_RING;
      /* Don't pad short packets up to 64 bytes. */
      desc->control |= TLP_DCTL_TX_NO_PAD;
      /* Use Tulip's CRC-32 generator, if appropriate. */
      if (sc->config.crc_len != CFG_CRC_32)
        desc->control |= TLP_DCTL_TX_NO_CRC;
      /* Set the OWNER bit, except in the first descriptor. */
      if (desc != ring->write)
        desc->status = TLP_DSTS_OWNER;

      desc_len = (length > MAX_CHUNK_LEN) ? MAX_CHUNK_LEN : length;
      /* Map kernel virtual address to PCI address. */
      if ((error = DMA_LOAD(desc->map, data, desc_len)))
        printf("%s: bus_dmamap_load(tx) failed; error %d\n", NAME_UNIT, error);
      /* Flush the cache and if bouncing, copy mbuf to bounce buf. */
      DMA_SYNC(desc->map, desc_len, BUS_DMASYNC_PREWRITE);

      /* Prevent wild fetches if mapping fails (nsegs==0). */
      desc->length1  = desc->length2  = 0;
      desc->address1 = desc->address2 = 0;
#if (defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
        {
# ifdef __FreeBSD__
        bus_dma_segment_t *segs = ring->segs;
        int nsegs = ring->nsegs;
# elif (defined(__NetBSD__) || defined(__OpenBSD__))
        bus_dma_segment_t *segs = desc->map->dm_segs;
        int nsegs = desc->map->dm_nsegs;
# endif
        if (nsegs >= 1)
          {
          desc->address1 = segs[0].ds_addr;
          desc->length1  = segs[0].ds_len;
          }
        if (nsegs == 2)
          {
          desc->address2 = segs[1].ds_addr;
          desc->length2  = segs[1].ds_len;
          }
        }
#elif defined(__bsdi__)
      desc->address1 = vtophys(data); /* Relax! BSD/OS only. */
      desc->length1  = desc_len;
#endif

      data   += desc_len;
      length -= desc_len;
      } /* while (length > 0) */

    m = m->m_next;
    } /* while (m != NULL) */

  return 0; /* success */
  }

/* Setup (prepare) to transmit a packet. */
/* Select a packet, build DMA descriptors and give packet to hardware. */
/* If DMA descriptors run out, abandon the attempt and return 0. */
static int  /* BSD version */
txintr_setup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *first_desc, *last_desc;

  /* Protect against half-up links: Don't transmit */
  /*  if the receiver can't hear the far end. */
  if (sc->status.oper_status != STATUS_UP) return 0;

  /* Pick a packet to transmit. */
#if NETGRAPH
  if ((sc->ng_hook != NULL) && (sc->tx_mbuf == NULL))
    {
    if (!IFQ_IS_EMPTY(&sc->ng_fastq))
      IFQ_DEQUEUE(&sc->ng_fastq, sc->tx_mbuf);
    else
      IFQ_DEQUEUE(&sc->ng_sndq,  sc->tx_mbuf);
    }
  else
#endif
  if (sc->tx_mbuf == NULL)
    {
    if (sc->config.line_pkg == PKG_RAWIP)
      IFQ_DEQUEUE(&sc->ifp->if_snd, sc->tx_mbuf);
    else
      {
#if NSPPP
      sc->tx_mbuf = sppp_dequeue(sc->ifp);
#elif P2P
      if (!IFQ_IS_EMPTY(&sc->p2p->p2p_isnd))
        IFQ_DEQUEUE(&sc->p2p->p2p_isnd, sc->tx_mbuf);
      else
        IFQ_DEQUEUE(&sc->ifp->if_snd, sc->tx_mbuf);
#endif
      }
    }
  if (sc->tx_mbuf == NULL) return 0;  /* no pkt to transmit */

  /* Build DMA descriptors for an outgoing mbuf chain. */
  ring->temp = ring->write; /* temporary ring write pointer */
  if (txintr_setup_mbuf(sc, sc->tx_mbuf) != 0) return 0;

  /* Enqueue the mbuf; txintr_cleanup will free it. */
  mbuf_enqueue(ring, sc->tx_mbuf);

  /* The transmitter has room for another packet. */
  sc->tx_mbuf = NULL;

  /* Set first & last segment bits. */
  /* last_desc is the desc BEFORE the one pointed to by ring->temp. */
  first_desc = ring->write;
  first_desc->control |= TLP_DCTL_TX_FIRST_SEG;
  last_desc = (ring->temp==ring->first)? ring->last : ring->temp-1;
   last_desc->control |= TLP_DCTL_TX_LAST_SEG;
  /* Interrupt at end-of-transmission?  Why bother the poor computer! */
/* last_desc->control |= TLP_DCTL_TX_INTERRUPT; */

  /* Make sure the OWNER bit is not set in the next descriptor. */
  /* The OWNER bit may have been set if a previous call aborted. */
  ring->temp->status = 0;

  /* Commit the DMA descriptors to the software. */
  ring->write = ring->temp;

  /* Before setting the OWNER bit, flush the cache (memory barrier). */
  DMA_SYNC(ring->map, ring->size_descs, BUS_DMASYNC_PREWRITE);

  /* Commit the DMA descriptors to the hardware. */
  first_desc->status = TLP_DSTS_OWNER;

  /* Notify the transmitter that there is another packet to send. */
  WRITE_CSR(TLP_TX_POLL, 1);

  return 1; /* did something */
  }

#endif /* BSD */

#ifdef __linux__
/* NOTE: this is the LINUX version of the interrupt/DMA code, */

/* Singly-linked tail-queues hold sk_buffs with active DMA.
 * skbuffs are linked through their sk_buff.next field.
 * Callers must hold sc->bottom_lock; not otherwise locked.
 */

/* Put an skbuff on the tail of the descriptor ring queue. */
static void  /* Linux version */
skbuff_enqueue(struct desc_ring *ring, struct sk_buff *skb)
  {
  skb->next = NULL;
  if (ring->tail == NULL)
    ring->head = skb;
  else
    ring->tail->next = skb;
  ring->tail = skb;
  }

/* Get an skbuff from the head of the descriptor ring queue. */
static struct sk_buff*  /* Linux version */
skbuff_dequeue(struct desc_ring *ring)
  {
  struct sk_buff *skb = ring->head;
  if (skb != NULL)
    if ((ring->head = skb->next) == NULL)
      ring->tail = NULL;
  return skb;
  }

/* Initialize a DMA descriptor ring. */
static int  /* Linux version */
create_ring(softc_t *sc, struct desc_ring *ring, int num_descs)
  {
  struct dma_desc *descs;
  int size_descs = sizeof(struct dma_desc)*num_descs;

  /* Allocate and map memory for DMA descriptor array. */
  if ((descs = pci_alloc_consistent(sc->pci_dev, size_descs,
   &ring->dma_addr)) == NULL)
    {
    printk("%s: pci_alloc_consistent() failed\n", NAME_UNIT);
    return ENOMEM;
    }
  memset(descs, 0, size_descs);

  ring->read  = descs;
  ring->write = descs;
  ring->first = descs;
  ring->last  = descs + num_descs -1;
  ring->last->control = TLP_DCTL_END_RING;
  ring->num_descs = num_descs;
  ring->size_descs = size_descs;
  ring->head = NULL;
  ring->tail = NULL;

  return 0;
  }

/* Destroy a DMA descriptor ring */
static void  /* Linux version */
destroy_ring(softc_t *sc, struct desc_ring *ring)
  {
  struct sk_buff *skb;

  /* Free queued skbuffs. */
  while ((skb = skbuff_dequeue(ring)) != NULL)
    dev_kfree_skb(skb);

  /* TX may have one pkt that is not on any queue. */
  if (sc->tx_skb != NULL)
    {
    dev_kfree_skb(sc->tx_skb);
    sc->tx_skb = NULL;
    }

  if (ring->first != NULL)
    {
    /* Unmap active DMA descriptors. */
    while (ring->read != ring->write)
      {
      pci_unmap_single(sc->pci_dev, ring->read->address1,
       ring->read->length1 + ring->read->length2, PCI_DMA_BIDIRECTIONAL);
      if (ring->read++ == ring->last) ring->read = ring->first;
      }

    /* Unmap and free memory for DMA descriptor array. */
    pci_free_consistent(sc->pci_dev, ring->size_descs, ring->first,
     ring->dma_addr);
    }
  }

static int  /* Linux version */
rxintr_cleanup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->rxring;
  struct dma_desc *first_desc, *last_desc;
  struct sk_buff *first_skb=NULL, *last_skb=NULL;
  struct sk_buff *new_skb;
  int pkt_len, desc_len;

  /* Input packet flow control (livelock prevention): */
  /* Give pkts to higher levels only if quota is > 0. */
  if (sc->quota <= 0) return 0;

  /* This looks complicated, but remember: packets up to 4032 */
  /*  bytes long fit in one skbuff and use one DMA descriptor. */

  first_desc = last_desc = ring->read;

  /* ASSERTION: If there is a descriptor in the ring and the hardware has */
  /*  finished with it, then that descriptor will have RX_FIRST_DESC set. */
  if ((ring->read != ring->write) && /* descriptor ring not empty */
     ((ring->read->status & TLP_DSTS_OWNER) == 0) && /* hardware done */
     ((ring->read->status & TLP_DSTS_RX_FIRST_DESC) == 0)) /* should be set */
    panic("%s: rxintr_cleanup: rx-first-descriptor not set.\n", NAME_UNIT);

  /* First decide if a complete packet has arrived. */
  /* Run down DMA descriptors looking for one marked "last". */
  /* Bail out if an active descriptor is encountered. */
  /* Accumulate most significant bits of packet length. */
  pkt_len = 0;
  for (;;)
    {
    if (last_desc == ring->write) return 0;  /* no more descs */
    if (last_desc->status & TLP_DSTS_OWNER) return 0; /* still active */
    if (last_desc->status & TLP_DSTS_RX_LAST_DESC) break; /* end of packet */
    pkt_len += last_desc->length1 + last_desc->length2; /* entire desc filled */
    if (last_desc++->control & TLP_DCTL_END_RING) last_desc = ring->first; /* ring wrap */
    }

  /* A complete packet has arrived; how long is it? */
  /* H/w ref man shows RX pkt length as a 14-bit field. */
  /* An experiment found that only the 12 LSBs work. */
  if (((last_desc->status>>16)&0xFFF) == 0) pkt_len += 4096; /* carry-bit */
  pkt_len = (pkt_len & 0xF000) + ((last_desc->status>>16) & 0x0FFF);
  /* Subtract the CRC length unless doing so would underflow. */
  if (pkt_len >= sc->config.crc_len) pkt_len -= sc->config.crc_len;

  /* Run down DMA descriptors again doing the following:
   *  1) put pkt info in hdr of first skbuff.
   *  2) put additional skbuffs on frag_list.
   *  3) set skbuff lengths.
   */
  first_desc = ring->read;
  do
    {
    /* Read a DMA descriptor from the ring. */
    last_desc = ring->read;
    /* Advance the ring read pointer. */
    if (ring->read++ == ring->last) ring->read = ring->first;

    /* Dequeue the corresponding skbuff. */
    new_skb = skbuff_dequeue(ring);
    if (new_skb == NULL)
      panic("%s: rxintr_cleanup: expected an skbuff\n", NAME_UNIT);

    desc_len = last_desc->length1 + last_desc->length2;
    /* Unmap kernel virtual addresss to PCI address. */
    pci_unmap_single(sc->pci_dev, last_desc->address1,
     desc_len, PCI_DMA_FROMDEVICE);

    /* Set skbuff length. */
    skb_put(new_skb, (pkt_len >= desc_len) ? desc_len : pkt_len);
    pkt_len -= new_skb->len;

    /* 1) Put pkt info in hdr of first skbuff. */
    if (last_desc == first_desc)
      {
      first_skb = new_skb;
      if (sc->config.line_pkg == PKG_RAWIP)
        {
        if      (first_skb->data[0]>>4 == 4)
          first_skb->protocol = htons(ETH_P_IP);
        else if (first_skb->data[0]>>4 == 6)
          first_skb->protocol = htons(ETH_P_IPV6);
	}
      else
#if GEN_HDLC
        first_skb->protocol = hdlc_type_trans(first_skb, sc->net_dev);
#else
        first_skb->protocol = htons(ETH_P_HDLC);
#endif
      first_skb->mac.raw = first_skb->data;
      first_skb->dev = sc->net_dev;
      do_gettimeofday(&first_skb->stamp);
      sc->net_dev->last_rx = jiffies;
      }
    else /* 2) link skbuffs. */
      {
      /* Put this skbuff on the frag_list of the first skbuff. */
      new_skb->next = NULL;
      if (skb_shinfo(first_skb)->frag_list == NULL)
        skb_shinfo(first_skb)->frag_list = new_skb;
      else
        last_skb->next = new_skb;
      /* 3) set skbuff lengths. */
      first_skb->len      += new_skb->len;
      first_skb->data_len += new_skb->len;
      }
    last_skb = new_skb;
    } while ((last_desc->status & TLP_DSTS_RX_LAST_DESC) == 0);

  /* Decide whether to accept or to discard this packet. */
  /* RxHDLC sets MIIERR for bad CRC, abort and partial byte at pkt end. */
  if (((last_desc->status & TLP_DSTS_RX_BAD) == 0) &&
   (sc->status.oper_status == STATUS_UP) &&
   (first_skb->len > 0))
    {
    /* Optimization: copy a small pkt into a small skbuff. */
    if (first_skb->len <= COPY_BREAK)
      if ((new_skb = skb_copy(first_skb, GFP_ATOMIC)) != NULL)
        {
        dev_kfree_skb_any(first_skb);
        first_skb = new_skb;
	}

    /* Include CRC and one flag byte in input byte count. */
    sc->status.cntrs.ibytes += first_skb->len + sc->config.crc_len +1;
    sc->status.cntrs.ipackets++;

    /* Give this good packet to the network stacks. */
    netif_receive_skb(first_skb);  /* NAPI */
    sc->quota--;
    }
  else if (sc->status.oper_status != STATUS_UP)
    {
    /* If the link is down, this packet is probably noise. */
    sc->status.cntrs.idiscards++;
    dev_kfree_skb_any(first_skb);
    if (DRIVER_DEBUG)
      printk("%s: rxintr_cleanup: rx pkt discarded: link down\n", NAME_UNIT);
    }
  else /* Log and discard this bad packet. */
    {
    if (DRIVER_DEBUG)
      printk("%s: RX bad pkt; len=%d %s%s%s%s\n",
       NAME_UNIT, first_skb->len,
       (last_desc->status & TLP_DSTS_RX_MII_ERR)  ? " miierr"  : "",
       (last_desc->status & TLP_DSTS_RX_DRIBBLE)  ? " dribble" : "",
       (last_desc->status & TLP_DSTS_RX_DESC_ERR) ? " descerr" : "",
       (last_desc->status & TLP_DSTS_RX_OVERRUN)  ? " overrun" : "");
    if (last_desc->status & TLP_DSTS_RX_OVERRUN)
      sc->status.cntrs.fifo_over++;
    else
      sc->status.cntrs.ierrors++;
    dev_kfree_skb_any(first_skb);
    }

  return 1; /* did something */
  }

/* Setup (prepare) to receive a packet. */
/* Try to keep the RX descriptor ring full of empty buffers. */
static int  /* Linux version */
rxintr_setup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->rxring;
  struct dma_desc *desc;
  struct sk_buff *skb;
  u_int32_t dma_addr;

  /* Ring is full if (wrap(write+1)==read) */
  if (((ring->write == ring->last) ? ring->first : ring->write+1) == ring->read)
    return 0;  /* ring is full; nothing to do */

  /* Allocate an skbuff. */
  if ((skb = dev_alloc_skb(MAX_DESC_LEN)) == NULL)
    {
    sc->status.cntrs.rxdma++;
    if (DRIVER_DEBUG)
      printk("%s: rxintr_setup: dev_alloc_skb() failed\n", NAME_UNIT);
    return 0;
    }
  skb->dev = sc->net_dev;

  /* Queue the skbuff for later processing by rxintr_cleanup. */
  skbuff_enqueue(ring, skb);

  /* Write a DMA descriptor into the ring. */
  /* Hardware won't see it until the OWNER bit is set. */
  desc = ring->write;
  /* Advance the ring write pointer. */
  if (ring->write++ == ring->last) ring->write = ring->first;

  /* Map kernel virtual addresses to PCI addresses. */
  dma_addr = pci_map_single(sc->pci_dev, skb->data,
   MAX_DESC_LEN, PCI_DMA_FROMDEVICE);
  /* Set up the DMA descriptor. */
  desc->address1 = dma_addr;
  desc->length1  = MAX_CHUNK_LEN;
  desc->address2 = desc->address1 + desc->length1;
  desc->length2  = MAX_CHUNK_LEN;

  /* Before setting the OWNER bit, flush the cache (memory barrier). */
  wmb(); /* write memory barrier */

  /* Commit the DMA descriptor to the hardware. */
  desc->status = TLP_DSTS_OWNER;

  /* Notify the receiver that there is another buffer available. */
  WRITE_CSR(TLP_RX_POLL, 1);

  return 1; /* did something */
  }

/* Clean up after a packet has been transmitted. */
/* Free the sk_buff and update the DMA descriptor ring. */
static int  /* Linux version */
txintr_cleanup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *desc;

  while ((ring->read != ring->write) && /* ring is not empty */
        ((ring->read->status & TLP_DSTS_OWNER) == 0))
    {
    /* Read a DMA descriptor from the ring. */
    desc = ring->read;
    /* Advance the ring read pointer. */
    if (ring->read++ == ring->last) ring->read = ring->first;
    /* Unmap kernel virtual address to PCI address. */
    pci_unmap_single(sc->pci_dev, desc->address1,
     desc->length1 + desc->length2, PCI_DMA_TODEVICE);

    /* If this descriptor is the last segment of a packet, */
    /*  then dequeue and free the corresponding skbuff. */
    if ((desc->control & TLP_DCTL_TX_LAST_SEG) != 0)
      {
      struct sk_buff *skb;
      if ((skb = skbuff_dequeue(ring)) == NULL)
        panic("%s: txintr_cleanup: expected an sk_buff\n", NAME_UNIT);

      /* Include CRC and one flag byte in output byte count. */
      sc->status.cntrs.obytes += skb->len + sc->config.crc_len +1;
      sc->status.cntrs.opackets++;

      /* The only bad TX status is fifo underrun. */
      if ((desc->status & TLP_DSTS_TX_UNDERRUN) != 0)
        {
        sc->status.cntrs.fifo_under++; /* also increment oerrors? */
        if (DRIVER_DEBUG)
          printk("%s: txintr_cleanup: tx fifo underrun\n", NAME_UNIT);
	}

      dev_kfree_skb_any(skb);
      return 1;  /* did something */
      }
    }

  return 0;
  }

/* Build DMA descriptors for a tranmit packet fragment, */
/* Assertion: fragment is contiguous in physical memory. */
static int /* 0=success; 1=error */ /* linux version */
txintr_setup_frag(softc_t *sc, char *data, int length)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *desc;
  unsigned int desc_len;
  u_int32_t dma_addr;

  while (length > 0)
    {
    /* Ring is full if (wrap(write+1)==read) */
    if (((ring->temp==ring->last) ? ring->first : ring->temp+1) == ring->read)
      { /* Not enough DMA descriptors; try later. */
      for (; ring->temp!=ring->write;
       ring->temp = (ring->temp==ring->first)? ring->last : ring->temp-1)
        pci_unmap_single(sc->pci_dev, ring->temp->address1,
         ring->temp->length1 + ring->temp->length2, PCI_DMA_FROMDEVICE);
      sc->status.cntrs.txdma++;
      return 1;
      }

    /* Provisionally, write a DMA descriptor into the ring. */
    /* But don't change the REAL ring write pointer. */
    /* Hardware won't see it until the OWNER bit is set. */
    desc = ring->temp;
    /* Advance the temporary ring write pointer. */
    if (ring->temp++ == ring->last) ring->temp = ring->first;

    /* Clear all control bits except the END_RING bit. */
    desc->control &= TLP_DCTL_END_RING;
    /* Don't pad short packets up to 64 bytes */
    desc->control |= TLP_DCTL_TX_NO_PAD;
    /* Use Tulip's CRC-32 generator, if appropriate. */
    if (sc->config.crc_len != CFG_CRC_32)
      desc->control |= TLP_DCTL_TX_NO_CRC;
    /* Set the OWNER bit, except in the first descriptor. */
    if (desc != ring->write)
      desc->status = TLP_DSTS_OWNER;

    desc_len = (length >= MAX_DESC_LEN) ? MAX_DESC_LEN : length;
    /* Map kernel virtual address to PCI address. */
    dma_addr = pci_map_single(sc->pci_dev, data, desc_len, PCI_DMA_TODEVICE);
    /* If it will fit in one chunk, do so, otherwise split it. */
    if (desc_len <= MAX_CHUNK_LEN)
      {
      desc->address1 = dma_addr;
      desc->length1  = desc_len;
      desc->address2 = 0;
      desc->length2  = 0;
      }
    else
      {
      desc->address1 = dma_addr;
      desc->length1  = desc_len>>1;
      desc->address2 = desc->address1 + desc->length1;
      desc->length2  = desc_len>>1;
      if (desc_len & 1) desc->length2++;
      }

    data   += desc_len;
    length -= desc_len;
    } /* while (length > 0) */

  return 0; /* success */
  }

/* NB: this procedure is recursive! */
static int /* 0=success; 1=error */
txintr_setup_skb(softc_t *sc, struct sk_buff *skb)
  {
  struct sk_buff *list;
  int i;

  /* First, handle the data in the skbuff itself. */
  if (txintr_setup_frag(sc, skb->data, skb_headlen(skb)))
    return 1;

  /* Next, handle the VM pages in the Scatter/Gather list. */
  if (skb_shinfo(skb)->nr_frags != 0)
    for (i=0; i<skb_shinfo(skb)->nr_frags; i++)
      {
      skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
      if (txintr_setup_frag(sc, page_address(frag->page) +
       frag->page_offset, frag->size))
        return 1;
      }

  /* Finally, handle the skbuffs in the frag_list. */
  if ((list = skb_shinfo(skb)->frag_list) != NULL)
    for (; list; list=list->next)
      if (txintr_setup_skb(sc, list)) /* recursive! */
        return 1;

  return 0;
  }

/* Setup (prepare) to transmit a packet. */
/* Select a packet, build DMA descriptors and give packet to hardware. */
/* If DMA descriptors run out, abandon the attempt and return 0. */
static int  /* Linux version */
txintr_setup(softc_t *sc)
  {
  struct desc_ring *ring = &sc->txring;
  struct dma_desc *first_desc, *last_desc;

  /* Protect against half-up links: Don't transmit */
  /*  if the receiver can't hear the far end. */
  if (sc->status.oper_status != STATUS_UP) return 0;

  /* Pick a packet to transmit. */
  /* linux_start() puts packets in sc->tx_skb. */
  if (sc->tx_skb == NULL)
    {
    if (netif_queue_stopped(sc->net_dev) != 0)
      netif_wake_queue(sc->net_dev);
    return 0; /* no pkt to transmit */
    }

  /* Build DMA descriptors for an outgoing skbuff. */
  ring->temp = ring->write; /* temporary ring write pointer */
  if (txintr_setup_skb(sc, sc->tx_skb) != 0) return 0;

  /* Enqueue the skbuff; txintr_cleanup will free it. */
  skbuff_enqueue(ring, sc->tx_skb);

  /* The transmitter has room for another packet. */
  sc->tx_skb = NULL;

  /* Set first & last segment bits. */
  /* last_desc is the desc BEFORE the one pointed to by ring->temp. */
  first_desc = ring->write;
  first_desc->control |= TLP_DCTL_TX_FIRST_SEG;
  last_desc = (ring->temp==ring->first)? ring->last : ring->temp-1;
   last_desc->control |= TLP_DCTL_TX_LAST_SEG;
  /* Interrupt at end-of-transmission?  Why bother the poor computer! */
/* last_desc->control |= TLP_DCTL_TX_INTERRUPT; */

  /* Make sure the OWNER bit is not set in the next descriptor. */
  /* The OWNER bit may have been set if a previous call aborted. */
  ring->temp->status = 0;

  /* Commit the DMA descriptors to the software. */
  ring->write = ring->temp;

  /* Before setting the OWNER bit, flush the cache (memory barrier). */
  wmb(); /* write memory barrier */

  /* Commit the DMA descriptors to the hardware. */
  first_desc->status = TLP_DSTS_OWNER;

  /* Notify the transmitter that there is another packet to send. */
  WRITE_CSR(TLP_TX_POLL, 1);

  sc->net_dev->trans_start = jiffies;

  return 1; /* did something */
  }

#endif /* __linux__ */

static void
check_intr_status(softc_t *sc)
  {
  u_int32_t status, cfcs, op_mode;
  u_int32_t missed, overruns;

  /* Check for four unusual events:
   *  1) fatal PCI bus errors       - some are recoverable
   *  2) transmitter FIFO underruns - increase fifo threshold
   *  3) receiver FIFO overruns     - clear potential hangup
   *  4) no receive descs or bufs   - count missed packets
   */

  /* 1) A fatal bus error causes a Tulip to stop initiating bus cycles. */
  /* Module unload/load or boot are the only fixes for Parity Errors. */
  /* Master and Target Aborts can be cleared and life may continue. */
  status = READ_CSR(TLP_STATUS);
  if ((status & TLP_STAT_FATAL_ERROR) != 0)
    {
    u_int32_t fatal = (status & TLP_STAT_FATAL_BITS)>>TLP_STAT_FATAL_SHIFT;
    printf("%s: FATAL PCI BUS ERROR: %s%s%s%s\n", NAME_UNIT,
     (fatal == 0) ? "PARITY ERROR" : "",
     (fatal == 1) ? "MASTER ABORT" : "",
     (fatal == 2) ? "TARGET ABORT" : "",
     (fatal >= 3) ? "RESERVED (?)" : "");
    cfcs = READ_PCI_CFG(sc, TLP_CFCS);  /* try to clear it */
    cfcs &= ~(TLP_CFCS_MSTR_ABORT | TLP_CFCS_TARG_ABORT);
    WRITE_PCI_CFG(sc, TLP_CFCS, cfcs);
    }

  /* 2) If the transmitter fifo underruns, increase the transmit fifo */
  /*  threshold: the number of bytes required to be in the fifo */
  /*  before starting the transmitter (cost: increased tx delay). */
  /* The TX_FSM must be stopped to change this parameter. */
  if ((status & TLP_STAT_TX_UNDERRUN) != 0)
    {
    op_mode = READ_CSR(TLP_OP_MODE);
    /* enable store-and-forward mode if tx_threshold tops out? */
    if ((op_mode & TLP_OP_TX_THRESH) < TLP_OP_TX_THRESH)
      {
      op_mode += 0x4000;  /* increment TX_THRESH field; can't overflow */
      WRITE_CSR(TLP_OP_MODE, op_mode & ~TLP_OP_TX_RUN);
      /* Wait for the TX FSM to stop; it might be processing a pkt. */
      while (READ_CSR(TLP_STATUS) & TLP_STAT_TX_FSM); /* XXX HANG */
      WRITE_CSR(TLP_OP_MODE, op_mode); /* restart tx */
      if (DRIVER_DEBUG)
        printf("%s: tx underrun; tx fifo threshold now %d bytes\n",
         NAME_UNIT, 128<<((op_mode>>TLP_OP_TR_SHIFT)&3));
      }
    }

  /* 3) Errata memo from Digital Equipment Corp warns that 21140A */
  /* receivers through rev 2.2 can hang if the fifo overruns. */
  /* Recommended fix: stop and start the RX FSM after an overrun. */
  missed = READ_CSR(TLP_MISSED);
  if ((overruns = ((missed & TLP_MISS_OVERRUN)>>TLP_OVERRUN_SHIFT)) != 0)
    {
    if (DRIVER_DEBUG)
      printf("%s: rx overrun cntr=%d\n", NAME_UNIT, overruns);
    sc->status.cntrs.overruns += overruns;
    if ((READ_PCI_CFG(sc, TLP_CFRV) & 0xFF) <= 0x22)
      {
      op_mode = READ_CSR(TLP_OP_MODE);
      WRITE_CSR(TLP_OP_MODE, op_mode & ~TLP_OP_RX_RUN);
      /* Wait for the RX FSM to stop; it might be processing a pkt. */
      while (READ_CSR(TLP_STATUS) & TLP_STAT_RX_FSM); /* XXX HANG */
      WRITE_CSR(TLP_OP_MODE, op_mode);  /* restart rx */
      }
    }

  /* 4) When the receiver is enabled and a packet arrives, but no DMA */
  /*  descriptor is available, the packet is counted as 'missed'. */
  /* The receiver should never miss packets; warn if it happens. */
  if ((missed = (missed & TLP_MISS_MISSED)) != 0)
    {
    if (DRIVER_DEBUG)
      printf("%s: rx missed %d pkts\n", NAME_UNIT, missed);
    sc->status.cntrs.missed += missed;
    }
  }

static void /* This is where the work gets done. */
core_interrupt(void *arg, int check_status)
  {
  softc_t *sc = arg;
  int activity;

  /* If any CPU is inside this critical section, then */
  /* other CPUs should go away without doing anything. */
  if (BOTTOM_TRYLOCK == 0)
    {
    sc->status.cntrs.lck_intr++;
    return;
    }

  /* Clear pending card interrupts. */
  WRITE_CSR(TLP_STATUS, READ_CSR(TLP_STATUS));

  /* In Linux, pci_alloc_consistent() means DMA descriptors */
  /*  don't need explicit syncing. */
#if BSD
  {
  struct desc_ring *ring = &sc->txring;
  DMA_SYNC(sc->txring.map, sc->txring.size_descs,
   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
  ring = &sc->rxring;
  DMA_SYNC(sc->rxring.map, sc->rxring.size_descs,
   BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
  }
#endif

  do  /* This is the main loop for interrupt processing. */
    {
    activity  = txintr_cleanup(sc);
    activity += txintr_setup(sc);
    activity += rxintr_cleanup(sc);
    activity += rxintr_setup(sc);
    } while (activity);

#if BSD
  {
  struct desc_ring *ring = &sc->txring;
  DMA_SYNC(sc->txring.map, sc->txring.size_descs,
   BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
  ring = &sc->rxring;
  DMA_SYNC(sc->rxring.map, sc->rxring.size_descs,
   BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
  }
#endif

  /* As the interrupt is dismissed, check for four unusual events. */
  if (check_status) check_intr_status(sc);

  BOTTOM_UNLOCK;
  }

/* user_interrupt() may be called from a syscall or a softirq */
static void
user_interrupt(softc_t *sc, int check_status)
  {
  DISABLE_INTR; /* noop on FreeBSD-5 and Linux */
  core_interrupt(sc, check_status);
  ENABLE_INTR;  /* noop on FreeBSD-5 and Linux */
  }

#if BSD

# if (defined(__FreeBSD__) && DEVICE_POLLING)

/* Service the card from the kernel idle loop without interrupts. */
static void
fbsd_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
  {
  softc_t *sc = IFP2SC(ifp);

#if (__FreeBSD_version < 700000)
  if ((ifp->if_capenable & IFCAP_POLLING) == 0)
    {
    ether_poll_deregister(ifp);
    cmd = POLL_DEREGISTER;
    }

  if (cmd == POLL_DEREGISTER)
    {
    /* Last call -- reenable card interrupts. */
    WRITE_CSR(TLP_INT_ENBL, TLP_INT_TXRX);
    return;
    }
#endif

  sc->quota = count;
  core_interrupt(sc, (cmd==POLL_AND_CHECK_STATUS));
  }

# endif  /* (__FreeBSD__ && DEVICE_POLLING) */

/* BSD kernels call this procedure when an interrupt happens. */
static intr_return_t
bsd_interrupt(void *arg)
  {
  softc_t *sc = arg;

  /* Cut losses early if this is not our interrupt. */
  if ((READ_CSR(TLP_STATUS) & TLP_INT_TXRX) == 0)
    return IRQ_NONE;

# if (defined(__FreeBSD__) && DEVICE_POLLING)
  if (sc->ifp->if_capenable & IFCAP_POLLING)
    return IRQ_NONE;

  if ((sc->ifp->if_capabilities & IFCAP_POLLING) &&
   (ether_poll_register(fbsd_poll, sc->ifp)))
    {
    WRITE_CSR(TLP_INT_ENBL, TLP_INT_DISABLE);
    return IRQ_NONE;
    }
  else
    sc->quota = sc->rxring.num_descs; /* input flow control */
# endif  /* (__FreeBSD__ && DEVICE_POLLING) */

  /* Disable card interrupts. */
  WRITE_CSR(TLP_INT_ENBL, TLP_INT_DISABLE);

  core_interrupt(sc, 0);

  /* Enable card interrupts. */
  WRITE_CSR(TLP_INT_ENBL, TLP_INT_TXRX);

  return IRQ_HANDLED;
  }

#endif /* BSD */

/* Administrative status of the driver (UP or DOWN) has changed. */
/* A card-specific action may be required: T1 and T3 cards: no-op. */
/* HSSI and SSI cards change the state of modem ready signals. */
static void
set_status(softc_t *sc, int status)
  {
  struct ioctl ioctl;

  ioctl.cmd = IOCTL_SET_STATUS;
  ioctl.data = status;

  sc->card->ioctl(sc, &ioctl);
  }

#if P2P

/* Callout from P2P: */
/* Get the state of DCD (Data Carrier Detect). */
static int
p2p_getmdm(struct p2pcom *p2p, caddr_t result)
  {
  softc_t *sc = IFP2SC(&p2p->p2p_if);

  /* Non-zero isn't good enough; TIOCM_CAR is 0x40. */
  *(int *)result = (sc->status.oper_status==STATUS_UP) ? TIOCM_CAR : 0;

  return 0;
  }

/* Callout from P2P: */
/* Set the state of DTR (Data Terminal Ready). */
static int
p2p_mdmctl(struct p2pcom *p2p, int flag)
  {
  softc_t *sc = IFP2SC(&p2p->p2p_if);

  set_status(sc, flag);

  return 0;
  }

#endif /* P2P */

#if NSPPP

# ifndef PP_FR
#  define PP_FR 0
# endif

/* Callout from SPPP: */
static void
sppp_tls(struct sppp *sppp)
  {
# ifdef __FreeBSD__
  if (!(sppp->pp_mode  & IFF_LINK2) &&
      !(sppp->pp_flags & PP_FR))
# elif defined(__NetBSD__) || defined(__OpenBSD__)
  if (!(sppp->pp_flags & PP_CISCO))
# endif
    sppp->pp_up(sppp);
  }

/* Callout from SPPP: */
static void
sppp_tlf(struct sppp *sppp)
  {
# ifdef __FreeBSD__
  if (!(sppp->pp_mode  & IFF_LINK2) &&
      !(sppp->pp_flags & PP_FR))
# elif defined(__NetBSD__) || defined(__OpenBSD__)
  if (!(sppp->pp_flags & PP_CISCO))
# endif
    sppp->pp_down(sppp);
  }

#endif /* NSPPP */

/* Configure line protocol stuff.
 * Called by attach_card() during module init.
 * Called by core_ioctl()  when lmcconfig writes sc->config.
 * Called by detach_card() during module shutdown.
 */
static void
config_proto(softc_t *sc, struct config *config)
  {
  /* Use line protocol stack instead of RAWIP mode. */
  if ((sc->config.line_pkg == PKG_RAWIP) &&
         (config->line_pkg != PKG_RAWIP))
    {
#if NSPPP
    LMC_BPF_DETACH;
    sppp_attach(sc->ifp);
    LMC_BPF_ATTACH(DLT_PPP, 4);
    sc->sppp->pp_tls = sppp_tls;
    sc->sppp->pp_tlf = sppp_tlf;
    /* Force reconfiguration of SPPP params. */
    sc->config.line_prot = 0;
    sc->config.keep_alive = config->keep_alive ? 0:1;
#elif P2P
    int error = 0;
    sc->p2p->p2p_proto = 0; /* force p2p_attach */
    if ((error = p2p_attach(sc->p2p))) /* calls bpfattach() */
      {
      printf("%s: p2p_attach() failed; error %d\n", NAME_UNIT, error);
      config->line_pkg = PKG_RAWIP;  /* still in RAWIP mode */
      }
    else
      {
      sc->p2p->p2p_mdmctl = p2p_mdmctl; /* set DTR */
      sc->p2p->p2p_getmdm = p2p_getmdm; /* get DCD */
      }
#elif GEN_HDLC
    int error = 0;
    sc->net_dev->mtu = HDLC_MAX_MTU;
    if ((error = hdlc_open(sc->net_dev)))
      {
      printf("%s: hdlc_open() failed; error %d\n", NAME_UNIT, error);
      printf("%s: Try 'sethdlc %s ppp'\n", NAME_UNIT, NAME_UNIT);
      config->line_pkg = PKG_RAWIP;  /* still in RAWIP mode */
      }
#else /* no line protocol stack was configured */
    config->line_pkg = PKG_RAWIP;  /* still in RAWIP mode */
#endif
    }

  /* Bypass line protocol stack and return to RAWIP mode. */
  if ((sc->config.line_pkg != PKG_RAWIP) &&
         (config->line_pkg == PKG_RAWIP))
    {
#if NSPPP
    LMC_BPF_DETACH;
    sppp_flush(sc->ifp);
    sppp_detach(sc->ifp);
    setup_ifnet(sc->ifp);
    LMC_BPF_ATTACH(DLT_RAW, 0);
#elif P2P
    int error = 0;
    if_qflush(&sc->p2p->p2p_isnd);
    if ((error = p2p_detach(sc->p2p)))
      {
      printf("%s: p2p_detach() failed; error %d\n",  NAME_UNIT, error);
      printf("%s: Try 'ifconfig %s down -remove'\n", NAME_UNIT, NAME_UNIT);
      config->line_pkg = PKG_P2P; /* not in RAWIP mode; still attached to P2P */
      }
    else
      {
      setup_ifnet(sc->ifp);
      LMC_BPF_ATTACH(DLT_RAW, 0);
      }
#elif GEN_HDLC
    hdlc_proto_detach(sc->hdlc_dev);
    hdlc_close(sc->net_dev);
    setup_netdev(sc->net_dev);
#endif
    }

#if NSPPP

  if (config->line_pkg != PKG_RAWIP)
    {
    /* Check for change to PPP protocol. */
    if ((sc->config.line_prot != PROT_PPP) &&
           (config->line_prot == PROT_PPP))
      {
      LMC_BPF_DETACH;
# if (defined(__NetBSD__) || defined(__OpenBSD__))
      sc->sppp->pp_flags &= ~PP_CISCO;
# elif defined(__FreeBSD__)
      sc->ifp->if_flags  &= ~IFF_LINK2;
      sc->sppp->pp_flags &= ~PP_FR;
# endif
      LMC_BPF_ATTACH(DLT_PPP, 4);
      sppp_ioctl(sc->ifp, SIOCSIFFLAGS, NULL);
      }

# ifndef DLT_C_HDLC
#  define DLT_C_HDLC DLT_PPP
# endif

    /* Check for change to C_HDLC protocol. */
    if ((sc->config.line_prot != PROT_C_HDLC) &&
           (config->line_prot == PROT_C_HDLC))
      {
      LMC_BPF_DETACH;
# if (defined(__NetBSD__) || defined(__OpenBSD__))
      sc->sppp->pp_flags |=  PP_CISCO;
# elif defined(__FreeBSD__)
      sc->ifp->if_flags  |=  IFF_LINK2;
      sc->sppp->pp_flags &= ~PP_FR;
# endif
      LMC_BPF_ATTACH(DLT_C_HDLC, 4);
      sppp_ioctl(sc->ifp, SIOCSIFFLAGS, NULL);
      }

    /* Check for change to Frame Relay protocol. */
    if ((sc->config.line_prot != PROT_FRM_RLY) &&
           (config->line_prot == PROT_FRM_RLY))
      {
      LMC_BPF_DETACH;
# if (defined(__NetBSD__) || defined(__OpenBSD__))
      sc->sppp->pp_flags &= ~PP_CISCO;
# elif defined(__FreeBSD__)
      sc->ifp->if_flags  &= ~IFF_LINK2;
      sc->sppp->pp_flags |= PP_FR;
# endif
      LMC_BPF_ATTACH(DLT_FRELAY, 4);
      sppp_ioctl(sc->ifp, SIOCSIFFLAGS, NULL);
      }

    /* Check for disabling keep-alives. */
    if ((sc->config.keep_alive != 0) &&
           (config->keep_alive == 0))
      sc->sppp->pp_flags &= ~PP_KEEPALIVE;

    /* Check for enabling keep-alives. */
    if ((sc->config.keep_alive == 0) &&
           (config->keep_alive != 0))
      sc->sppp->pp_flags |=  PP_KEEPALIVE;	
    }

#endif /* NSPPP */

  /* Loop back through the TULIP Ethernet chip; (no CRC). */
  /* Data sheet says stop DMA before changing OPMODE register. */
  /* But that's not as simple as it sounds; works anyway. */
  /* Check for enabling loopback thru Tulip chip. */
  if ((sc->config.loop_back != CFG_LOOP_TULIP) &&
         (config->loop_back == CFG_LOOP_TULIP))
    {
    u_int32_t op_mode = READ_CSR(TLP_OP_MODE);
    op_mode |= TLP_OP_INT_LOOP;
    WRITE_CSR(TLP_OP_MODE, op_mode);
    config->crc_len = CFG_CRC_0;
    }

  /* Check for disabling loopback thru Tulip chip. */
  if ((sc->config.loop_back == CFG_LOOP_TULIP) &&
         (config->loop_back != CFG_LOOP_TULIP))
    {
    u_int32_t op_mode = READ_CSR(TLP_OP_MODE);
    op_mode &= ~TLP_OP_LOOP_MODE;
    WRITE_CSR(TLP_OP_MODE, op_mode);
    config->crc_len = CFG_CRC_16;
    }
  }

/* This is the core ioctl procedure. */
/* It handles IOCTLs from lmcconfig(8). */
/* It must not run when card watchdogs run. */
/* Called from a syscall (user context; no spinlocks). */
/* This procedure can SLEEP. */
static int
core_ioctl(softc_t *sc, u_long cmd, caddr_t data)
  {
  struct iohdr  *iohdr  = (struct iohdr  *) data;
  struct ioctl  *ioctl  = (struct ioctl  *) data;
  struct status *status = (struct status *) data;
  struct config *config = (struct config *) data;
  int error = 0;

  /* All structs start with a string and a cookie. */
  if (((struct iohdr *)data)->cookie != NGM_LMC_COOKIE)
    return EINVAL;

  while (TOP_TRYLOCK == 0)
    {
    sc->status.cntrs.lck_ioctl++;
    SLEEP(10000); /* yield? */
    }
  switch (cmd)
    {
    case LMCIOCGSTAT:
      {
      *status = sc->status;
      iohdr->cookie = NGM_LMC_COOKIE;
      break;
      }
    case LMCIOCGCFG:
      {
      *config = sc->config;
      iohdr->cookie = NGM_LMC_COOKIE;
      break;
      }
    case LMCIOCSCFG:
      {
      if ((error = CHECK_CAP)) break;
      config_proto(sc, config);
      sc->config = *config;
      sc->card->config(sc);
      break;
      }
    case LMCIOCREAD:
      {
      if (ioctl->cmd == IOCTL_RW_PCI)
        {
        if (ioctl->address > 252) { error = EFAULT; break; }
        ioctl->data = READ_PCI_CFG(sc, ioctl->address);
	}
      else if (ioctl->cmd == IOCTL_RW_CSR)
        {
        if (ioctl->address > 15) { error = EFAULT; break; }
        ioctl->data = READ_CSR(ioctl->address*TLP_CSR_STRIDE);
	}
      else if (ioctl->cmd == IOCTL_RW_SROM)
        {
        if (ioctl->address > 63)  { error = EFAULT; break; }
        ioctl->data = read_srom(sc, ioctl->address);
	}
      else if (ioctl->cmd == IOCTL_RW_BIOS)
        ioctl->data = read_bios(sc, ioctl->address);
      else if (ioctl->cmd == IOCTL_RW_MII)
        ioctl->data = read_mii(sc, ioctl->address);
      else if (ioctl->cmd == IOCTL_RW_FRAME)
        ioctl->data = read_framer(sc, ioctl->address);
      else
        error = EINVAL;
      break;
      }
    case LMCIOCWRITE:
      {
      if ((error = CHECK_CAP)) break;
      if (ioctl->cmd == IOCTL_RW_PCI)
        {
        if (ioctl->address > 252) { error = EFAULT; break; }
        WRITE_PCI_CFG(sc, ioctl->address, ioctl->data);
	}
      else if (ioctl->cmd == IOCTL_RW_CSR)
        {
        if (ioctl->address > 15) { error = EFAULT; break; }
        WRITE_CSR(ioctl->address*TLP_CSR_STRIDE, ioctl->data);
	}
      else if (ioctl->cmd == IOCTL_RW_SROM)
        {
        if (ioctl->address > 63)  { error = EFAULT; break; }
        write_srom(sc, ioctl->address, ioctl->data); /* can sleep */
	}
      else if (ioctl->cmd == IOCTL_RW_BIOS)
        {
        if (ioctl->address == 0) erase_bios(sc);
        write_bios(sc, ioctl->address, ioctl->data); /* can sleep */
	}
      else if (ioctl->cmd == IOCTL_RW_MII)
        write_mii(sc, ioctl->address, ioctl->data);
      else if (ioctl->cmd == IOCTL_RW_FRAME)
        write_framer(sc, ioctl->address, ioctl->data);
      else if (ioctl->cmd == IOCTL_WO_SYNTH)
        write_synth(sc, (struct synth *)&ioctl->data);
      else if (ioctl->cmd == IOCTL_WO_DAC)
        {
        write_dac(sc, 0x9002); /* set Vref = 2.048 volts */
        write_dac(sc, ioctl->data & 0xFFF);
	}
      else
        error = EINVAL;
      break;
      }
    case LMCIOCTL:
      {
      if ((error = CHECK_CAP)) break;
      if (ioctl->cmd == IOCTL_XILINX_RESET)
        {
        reset_xilinx(sc);
        sc->card->config(sc);
	}
      else if (ioctl->cmd == IOCTL_XILINX_ROM)
        {
        load_xilinx_from_rom(sc); /* can sleep */
        sc->card->config(sc);
	}
      else if (ioctl->cmd == IOCTL_XILINX_FILE)
        {
        /* load_xilinx_from_file() can sleep. */
        error = load_xilinx_from_file(sc, ioctl->ucode, ioctl->data);
        if (error != 0) load_xilinx_from_rom(sc); /* try the rom */
        sc->card->config(sc);
        set_status(sc, (error==0));  /* XXX */
	}
      else if (ioctl->cmd == IOCTL_RESET_CNTRS)
        {
        memset(&sc->status.cntrs, 0, sizeof(struct event_cntrs));
        microtime(&sc->status.cntrs.reset_time);
        }
      else
        error = sc->card->ioctl(sc, ioctl); /* can sleep */
      break;
      }
    default:
      error = EINVAL;
      break;
    }
  TOP_UNLOCK;

  return error;
  }

/* This is the core watchdog procedure. */
/* It calculates link speed, and calls the card-specific watchdog code. */
/* Calls interrupt() in case one got lost; also kick-starts the device. */
/* ioctl syscalls and card watchdog routines must be interlocked.       */
/* This procedure must not sleep. */
static void
core_watchdog(softc_t *sc)
  {
  /* Read and restart the Tulip timer. */
  u_int32_t tx_speed = READ_CSR(TLP_TIMER);
  WRITE_CSR(TLP_TIMER, 0xFFFF);

  /* Measure MII clock using a timer in the Tulip chip.
   * This timer counts transmitter bits divided by 4096.
   * Since this is called once a second the math is easy.
   * This is only correct when the link is NOT sending pkts.
   * On a fully-loaded link, answer will be HALF actual rate.
   * Clock rate during pkt is HALF clk rate between pkts.
   * Measuring clock rate really measures link utilization!
   */
  sc->status.tx_speed = (0xFFFF - (tx_speed & 0xFFFF)) << 12;

  /* The first status reset time is when the calendar clock is set. */
  if (sc->status.cntrs.reset_time.tv_sec < 1000)
    microtime(&sc->status.cntrs.reset_time);

  /* Update hardware (operational) status. */
  /* Call the card-specific watchdog routines. */
  if (TOP_TRYLOCK != 0)
    {
    sc->status.oper_status = sc->card->watchdog(sc);

    /* Increment a counter which tells user-land */
    /*  observers that SNMP state has been updated. */
    sc->status.ticks++;

    TOP_UNLOCK;
    }
  else
    sc->status.cntrs.lck_watch++;

  /* In case an interrupt gets lost... */
  user_interrupt(sc, 1);
  }

#if IFNET

/* Called from a syscall (user context; no spinlocks). */
static int
raw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
  {
  struct ifreq *ifr = (struct ifreq *) data;
  int error = 0;

  switch (cmd)
    {
# if (defined(__FreeBSD__) && DEVICE_POLLING)  /* XXX necessary? */
    case SIOCSIFCAP:
# endif
    case SIOCSIFDSTADDR:
    case SIOCAIFADDR:
    case SIOCSIFFLAGS:
#if 0
    case SIOCADDMULTI:
    case SIOCDELMULTI:
      break;
#endif
    case SIOCSIFADDR:
      ifp->if_flags |= IFF_UP;	/* a Unix tradition */
      break;
    case SIOCSIFMTU:
      ifp->if_mtu = ifr->ifr_mtu;
      break;
    default:
      error = EINVAL;
      break;
    }
  return error;
  }

/* Called from a syscall (user context; no spinlocks). */
static int
ifnet_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
  {
  softc_t *sc = IFP2SC(ifp);
# ifdef __OpenBSD__
  struct ifreq *ifr = (struct ifreq *) data;
# endif
  int error = 0;

  switch (cmd)
    {
    /* Catch the IOCTLs used by lmcconfig. */
    case LMCIOCGSTAT:
    case LMCIOCGCFG:
    case LMCIOCSCFG:
    case LMCIOCREAD:
    case LMCIOCWRITE:
    case LMCIOCTL:
      error = core_ioctl(sc, cmd, data);
      break;
# ifdef __OpenBSD__
    /* Catch the IOCTLs used by ifconfig. */
    case SIOCSIFMEDIA:
      if ((error = CHECK_CAP)) break;
    case SIOCGIFMEDIA:
      error = ifmedia_ioctl(ifp, ifr, &sc->ifm, cmd);
      break;
    case SIOCSIFTIMESLOT:
      if ((error = CHECK_CAP)) break;
      if (sc->status.card_type == TLP_CSID_T1E1)
        {
        struct config config = sc->config;
        if ((error = copyin(ifr->ifr_data, &config.time_slots,
         sizeof config.time_slots))) break;
        config.iohdr.cookie = NGM_LMC_COOKIE;
        error = core_ioctl(sc, LMCIOCSCFG, (caddr_t)&config);
	}
      else
        error = EINVAL;
      break;
    case SIOCGIFTIMESLOT:
      if (sc->status.card_type == TLP_CSID_T1E1)
        error = copyout(&sc->config.time_slots, ifr->ifr_data,
         sizeof sc->config.time_slots);
      else
        error = EINVAL;
      break;
# endif
    /* Pass the rest to the line protocol. */
    default:
      if (sc->config.line_pkg == PKG_RAWIP)
        error =  raw_ioctl(ifp, cmd, data);
      else
# if NSPPP
        error = sppp_ioctl(ifp, cmd, data);
# elif P2P
        error =  p2p_ioctl(ifp, cmd, data);
# else
        error = EINVAL;
# endif
      break;
    }

  if (DRIVER_DEBUG && (error!=0))
    printf("%s: ifnet_ioctl; cmd=0x%08lx error=%d\n",
     NAME_UNIT, cmd, error);

  return error;
  }

/* Called from a syscall (user context; no spinlocks). */
static void
ifnet_start(struct ifnet *ifp)
  {
  softc_t *sc = IFP2SC(ifp);

  /* Start the transmitter; incoming pkts are NOT processed. */
  user_interrupt(sc, 0);
  }

/* sppp and p2p replace this with their own proc. */
/* RAWIP mode is the only time this is used. */
/* Called from a syscall (user context; no spinlocks). */
static int
raw_output(struct ifnet *ifp, struct mbuf *m,
 struct sockaddr *dst, struct rtentry *rt)
  {
  softc_t *sc = IFP2SC(ifp);
  int error = 0;

  /* Fail if the link is down. */
  if (sc->status.oper_status != STATUS_UP)
    {
    m_freem(m);
    sc->status.cntrs.odiscards++;
    if (DRIVER_DEBUG)
      printf("%s: raw_output: tx pkt discarded: link down\n", NAME_UNIT);
    return ENETDOWN;
    }

# if NETGRAPH
  /* Netgraph has priority over the ifnet kernel interface. */
  if (sc->ng_hook != NULL)
    {
    m_freem(m);
    sc->status.cntrs.odiscards++;
    if (DRIVER_DEBUG)
      printf("%s: raw_output: tx pkt discarded: netgraph active\n", NAME_UNIT);
    return EBUSY;
    }
# endif

  /* raw_output() ENQUEUEs in a syscall or softirq. */
  /* txintr_setup() DEQUEUEs in a hard interrupt. */
  /* Some BSD QUEUE routines are not interrupt-safe. */
  {
  DISABLE_INTR;
# if (__FreeBSD_version >= 503000)
  IFQ_ENQUEUE(&ifp->if_snd, m, error);
# else
  IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
# endif
  ENABLE_INTR;
  }

  if (error==0)
    user_interrupt(sc, 0); /* start the transmitter */
  else
    {
    m_freem(m);
    sc->status.cntrs.odiscards++;
    if (DRIVER_DEBUG)
      printf("%s: raw_output: IFQ_ENQUEUE() failed; error %d\n",
       NAME_UNIT, error);
    }

  return error;
  }

/* Called from a softirq once a second. */
static void
ifnet_watchdog(struct ifnet *ifp)
  {
  softc_t *sc = IFP2SC(ifp);
  u_int8_t old_oper_status = sc->status.oper_status;
  struct event_cntrs *cntrs = &sc->status.cntrs;

  core_watchdog(sc); /* updates oper_status */

#if NETGRAPH
  if (sc->ng_hook != NULL)
    {
    sc->status.line_pkg  = PKG_NG;
    sc->status.line_prot = 0;
    }
  else
#endif
  if (sc->config.line_pkg == PKG_RAWIP)
    {
    sc->status.line_pkg  = PKG_RAWIP;
    sc->status.line_prot = PROT_IP_HDLC;
    }
  else
    {
# if P2P
    /* Notice change in link status. */
    if ((old_oper_status != sc->status.oper_status) && (sc->p2p->p2p_modem))
      (*sc->p2p->p2p_modem)(sc->p2p, sc->status.oper_status==STATUS_UP);

    /* Notice change in line protocol. */
    sc->status.line_pkg = PKG_P2P;
    switch (sc->ifp->if_type)
      {
      case IFT_PPP:
        sc->status.line_prot = PROT_PPP;
        break;
      case IFT_PTPSERIAL:
        sc->status.line_prot = PROT_C_HDLC;
        break;
      case IFT_FRELAY:
        sc->status.line_prot = PROT_FRM_RLY;
        break;
      default:
        sc->status.line_prot = 0;
        break;
      }

# elif NSPPP
    /* Notice change in link status. */
    if     ((old_oper_status != STATUS_UP) &&
     (sc->status.oper_status == STATUS_UP))  /* link came up */
      sppp_tls(sc->sppp);
    if     ((old_oper_status == STATUS_UP) &&
     (sc->status.oper_status != STATUS_UP))  /* link went down */
      sppp_tlf(sc->sppp);

    /* Notice change in line protocol. */
    sc->status.line_pkg = PKG_SPPP;
#  ifdef __FreeBSD__
    if (sc->sppp->pp_flags & PP_FR)
      sc->status.line_prot = PROT_FRM_RLY;
    else if (sc->ifp->if_flags  & IFF_LINK2)
#  elif (defined(__NetBSD__) || defined(__OpenBSD__))
    if (sc->sppp->pp_flags & PP_CISCO)
#  endif
      sc->status.line_prot = PROT_C_HDLC;
    else
      sc->status.line_prot = PROT_PPP;

# else
    /* Suppress compiler warning. */
    if (old_oper_status == STATUS_UP);
# endif
    }

  /* Copy statistics from sc to ifp. */
  ifp->if_baudrate = sc->status.tx_speed;
  ifp->if_ipackets = cntrs->ipackets;
  ifp->if_opackets = cntrs->opackets;
  ifp->if_ibytes   = cntrs->ibytes;
  ifp->if_obytes   = cntrs->obytes;
  ifp->if_ierrors  = cntrs->ierrors;
  ifp->if_oerrors  = cntrs->oerrors;
  ifp->if_iqdrops  = cntrs->idiscards;

# if ((__FreeBSD_version >= 500000) || defined(__OpenBSD__) || defined(__NetBSD__))
  if (sc->status.oper_status == STATUS_UP)
    ifp->if_link_state = LINK_STATE_UP;
  else
    ifp->if_link_state = LINK_STATE_DOWN;
# endif

  /* Call this procedure again after one second. */
  ifp->if_timer = 1;
  }

# ifdef __OpenBSD__

/* Callback from ifmedia. */
static int
ifmedia_change(struct ifnet *ifp)
  {
  softc_t *sc = IFP2SC(ifp);
  struct config config = sc->config;
  int media = sc->ifm.ifm_media;
  int error;

  /* ifconfig lmc0 media t1 */
  if      (sc->status.card_type == TLP_CSID_T3)
    {
    if      ((media & IFM_TMASK) == IFM_TDM_T3)
      config.format = CFG_FORMAT_T3CPAR;
    else if ((media & IFM_TMASK) == IFM_TDM_T3_M13)
      config.format = CFG_FORMAT_T3M13;
    }
  else if (sc->status.card_type == TLP_CSID_T1E1)
    {
    if      ((media & IFM_TMASK) == IFM_TDM_T1)
      config.format = CFG_FORMAT_T1ESF;
    else if ((media & IFM_TMASK) == IFM_TDM_T1_AMI)
      config.format = CFG_FORMAT_T1SF;
    else if ((media & IFM_TMASK) == IFM_TDM_E1)
      config.format = CFG_FORMAT_E1NONE;
    else if ((media & IFM_TMASK) == IFM_TDM_E1_G704)
      config.format = CFG_FORMAT_E1FASCRC;
    }

  /* ifconfig lmc0 mediaopt loopback */
  if (media & IFM_LOOP)
    config.loop_back = CFG_LOOP_TULIP;
  else
    config.loop_back = CFG_LOOP_NONE;

  /* ifconfig lmc0 mediaopt crc16 */
  if (media & IFM_TDM_HDLC_CRC16)
    config.crc_len = CFG_CRC_16;
  else
    config.crc_len = CFG_CRC_32;

  /* Set ConFiGuration. */
  config.iohdr.cookie = NGM_LMC_COOKIE;
  error = core_ioctl(sc, LMCIOCSCFG, (caddr_t)&config);

  return error;
  }

/* Callback from ifmedia. */
static void
ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
  {
  softc_t *sc = IFP2SC(ifp);

  /* ifconfig wants to know if the hardware link is up. */
  ifmr->ifm_status = IFM_AVALID;
  if (sc->status.oper_status == STATUS_UP)
    ifmr->ifm_status |= IFM_ACTIVE;

  ifmr->ifm_active = sc->ifm.ifm_cur->ifm_media;

  if (sc->config.loop_back != CFG_LOOP_NONE)
    ifmr->ifm_active |= IFM_LOOP;

  if (sc->config.crc_len == CFG_CRC_16)
    ifmr->ifm_active |= IFM_TDM_HDLC_CRC16;
  }

# endif  /* __OpenBSD__ */

static void
setup_ifnet(struct ifnet *ifp)
  {
  softc_t *sc = ifp->if_softc;

  /* Initialize the generic network interface. */
  /* Note similarity to linux's setup_netdev(). */
  ifp->if_flags    = IFF_POINTOPOINT;
  ifp->if_flags   |= IFF_RUNNING;
  ifp->if_ioctl    = ifnet_ioctl;
  ifp->if_start    = ifnet_start;	/* sppp changes this */
  ifp->if_output   = raw_output;	/* sppp & p2p change this */
  ifp->if_input    = raw_input;
  ifp->if_watchdog = ifnet_watchdog;
  ifp->if_timer    = 1;
  ifp->if_mtu      = MAX_DESC_LEN;	/* sppp & p2p change this */
  ifp->if_type     = IFT_PTPSERIAL;	/* p2p changes this */

# if (defined(__FreeBSD__) && DEVICE_POLLING)
  ifp->if_capabilities |= IFCAP_POLLING;
# if (__FreeBSD_version < 500000)
  ifp->if_capenable    |= IFCAP_POLLING;
# endif
# endif

  /* Every OS does it differently! */
# if (defined(__FreeBSD__) && (__FreeBSD_version < 502000))
  (const char *)ifp->if_name = device_get_name(sc->dev);
  ifp->if_unit  = device_get_unit(sc->dev);
# elif (__FreeBSD_version >= 502000)
  ifp->if_dname = device_get_name(sc->dev);
  ifp->if_dunit = device_get_unit(sc->dev);
  strlcpy(ifp->if_xname, device_get_nameunit(sc->dev), IFNAMSIZ);
# elif defined(__NetBSD__)
  strcpy(ifp->if_xname, sc->dev.dv_xname);
# elif __OpenBSD__
  bcopy(sc->dev.dv_xname, ifp->if_xname, IFNAMSIZ);
# elif defined(__bsdi__)
  ifp->if_name  = sc->dev.dv_cfdata->cf_driver->cd_name;
  ifp->if_unit  = sc->dev.dv_unit;
# endif
  }

static int
ifnet_attach(softc_t *sc)
  {
# if (__FreeBSD_version >= 600000)
  sc->ifp  = if_alloc(NSPPP ? IFT_PPP : IFT_OTHER);
  if (sc->ifp == NULL) return ENOMEM;
# endif
# if NSPPP
#  if (__FreeBSD_version >= 600000)
  sc->sppp = sc->ifp->if_l2com;
#  else
  sc->ifp  = &sc->spppcom.pp_if;
  sc->sppp = &sc->spppcom;
#  endif
# elif P2P
  sc->ifp  = &sc->p2pcom.p2p_if;
  sc->p2p  = &sc->p2pcom;
# elif (__FreeBSD_version < 600000)
  sc->ifp  = &sc->ifnet;
# endif

  /* Initialize the network interface struct. */
  sc->ifp->if_softc = sc;
  setup_ifnet(sc->ifp);

  /* ALTQ output queue initialization. */
  IFQ_SET_MAXLEN(&sc->ifp->if_snd, SNDQ_MAXLEN);
  IFQ_SET_READY(&sc->ifp->if_snd);

  /* Attach to the ifnet kernel interface. */
  if_attach(sc->ifp);

# if ((defined(__NetBSD__) && __NetBSD_Version__ >= 106000000) || \
     (defined(__OpenBSD__) && OpenBSD >= 200211))
  if_alloc_sadl(sc->ifp);
# endif

  /* Attach Berkeley Packet Filter. */
  LMC_BPF_ATTACH(DLT_RAW, 0);

# ifdef __OpenBSD__
  /* Initialize ifmedia mechanism. */
  ifmedia_init(&sc->ifm, IFM_OMASK | IFM_GMASK | IFM_IMASK,
   ifmedia_change, ifmedia_status);
  if       (sc->status.card_type == TLP_CSID_T3)
    {
    ifmedia_add(&sc->ifm, IFM_TDM | IFM_TDM_T3, 0, NULL);
    ifmedia_add(&sc->ifm, IFM_TDM | IFM_TDM_T3_M13, 0, NULL);
    ifmedia_set(&sc->ifm, IFM_TDM | IFM_TDM_T3);
    }
  else if  (sc->status.card_type == TLP_CSID_T1E1)
    {
    ifmedia_add(&sc->ifm, IFM_TDM | IFM_TDM_T1, 0, NULL);
    ifmedia_add(&sc->ifm, IFM_TDM | IFM_TDM_T1_AMI, 0, NULL);
    ifmedia_add(&sc->ifm, IFM_TDM | IFM_TDM_E1, 0, NULL);
    ifmedia_add(&sc->ifm, IFM_TDM | IFM_TDM_E1_G704, 0, NULL);
    ifmedia_set(&sc->ifm, IFM_TDM | IFM_TDM_T1);
    }
  else if ((sc->status.card_type == TLP_CSID_HSSI) ||
           (sc->status.card_type == TLP_CSID_SSI))
    {
    ifmedia_add(&sc->ifm, IFM_TDM | IFM_NONE, 0, NULL);
    ifmedia_set(&sc->ifm, IFM_TDM | IFM_NONE);
    }
# endif  /* __OpenBSD__ */

  return 0;
  }

static void
ifnet_detach(softc_t *sc)
  {
# ifdef __OpenBSD__
  ifmedia_delete_instance(&sc->ifm, IFM_INST_ANY);
# endif

# if (defined(__FreeBSD__) && DEVICE_POLLING)
  if (sc->ifp->if_capenable & IFCAP_POLLING)
    ether_poll_deregister(sc->ifp);
# endif

  /* Detach Berkeley Packet Filter. */
  LMC_BPF_DETACH;

# if ((defined(__NetBSD__) && __NetBSD_Version__ >= 106000000) || \
     (defined(__OpenBSD__) && OpenBSD >= 200211))
  if_free_sadl(sc->ifp);
# endif

  /* Detach from the ifnet kernel interface. */
  if_detach(sc->ifp);

# if (__FreeBSD_version >= 600000)
  if_free_type(sc->ifp, NSPPP ? IFT_PPP : IFT_OTHER);
# endif
  }

#endif  /* IFNET */

#if NETGRAPH

/* Netgraph changed significantly between FreeBSD-4 and -5. */
/* These are backward compatibility hacks for FreeBSD-4. */
# if (__FreeBSD_version >= 500000)
/* These next two macros should be added to netgraph */
#  define NG_TYPE_REF(type) atomic_add_int(&(type)->refs, 1)
#  define NG_TYPE_UNREF(type)	\
do {				\
  if ((type)->refs == 1)	\
    ng_rmtype(type);		\
  else				\
    atomic_subtract_int(&(type)->refs, 1); \
   } while (0)
# else /* FreeBSD-4 */
#  define NGI_GET_MSG(item, msg)	/* nothing */
#  define NG_HOOK_FORCE_QUEUE(hook)	/* nothing */
#  define NG_TYPE_REF(type) atomic_add_int(&(type)->refs, 1)
#  define NG_TYPE_UNREF(type)	\
do {				\
  if ((type)->refs == 1)	\
    LIST_REMOVE(type, types);	\
  else				\
    atomic_subtract_int(&(type)->refs, 1); \
   } while (0)
# endif

/* It is an error to construct new copies of this Netgraph node. */
/* All instances are constructed by ng_attach and are persistent. */
# if (__FreeBSD_version >= 500000)
static int ng_constructor(node_p  node) { return EINVAL; }
# else /* FreeBSD-4 */
static int ng_constructor(node_p *node) { return EINVAL; }
# endif

/* Incoming Netgraph control message. */
# if (__FreeBSD_version >= 500000)
static int
ng_rcvmsg(node_p node, item_p item, hook_p lasthook)
  {
  struct ng_mesg *msg;
# else /* FreeBSD-4 */
static int
ng_rcvmsg(node_p node, struct ng_mesg *msg,
 const char *retaddr,  struct ng_mesg **rptr)
  {
# endif
  struct ng_mesg *resp = NULL;
  softc_t *sc = NG_NODE_PRIVATE(node);
  int error = 0;

  NGI_GET_MSG(item, msg);
  if (msg->header.typecookie == NGM_LMC_COOKIE)
    {
    switch (msg->header.cmd)
      {
      case LMCIOCGSTAT:
      case LMCIOCGCFG:
      case LMCIOCSCFG:
      case LMCIOCREAD:
      case LMCIOCWRITE:
      case LMCIOCTL:
        {
        /* Call the core ioctl procedure. */
        error = core_ioctl(sc, msg->header.cmd, msg->data);
        if ((msg->header.cmd & IOC_OUT) != 0)
          { /* synchronous response */
          NG_MKRESPONSE(resp, msg, sizeof(struct ng_mesg) +
           IOCPARM_LEN(msg->header.cmd), M_NOWAIT);
          if (resp == NULL)
            error = ENOMEM;
          else
            memcpy(resp->data, msg->data, IOCPARM_LEN(msg->header.cmd));
          }
        break;
        }
      default:
        error = EINVAL;
        break;
      }
    }
  else if ((msg->header.typecookie == NGM_GENERIC_COOKIE) &&
           (msg->header.cmd == NGM_TEXT_STATUS))
    {  /* synchronous response */
    NG_MKRESPONSE(resp, msg, sizeof(struct ng_mesg) +
     NG_TEXTRESPONSE, M_NOWAIT);
    if (resp == NULL)
      error = ENOMEM;
    else
      {
      char *s = resp->data;
      sprintf(s, "Card type = <%s>\n"
       "This driver considers the link to be %s.\n"
       "Use lmcconfig to configure this interface.\n",
       sc->dev_desc, (sc->status.oper_status==STATUS_UP) ? "UP" : "DOWN");
      resp->header.arglen = strlen(s) +1;
      }
    }
  else
/* Netgraph should be able to read and write these
 *  parameters with text-format control messages:
 *  SSI	     HSSI     T1E1     T3
 *  crc	     crc      crc      crc      
 *  loop     loop     loop     loop
 *           clksrc   clksrc
 *  dte	     dte      format   format
 *  synth    synth    cablen   cablen
 *  cable             timeslot scram
 *                    gain
 *                    pulse
 *                    lbo
 * Someday I'll implement this...
 */
    error = EINVAL;

  /* Handle synchronous response. */
# if (__FreeBSD_version >= 500000)
  NG_RESPOND_MSG(error, node, item, resp);
  NG_FREE_MSG(msg);
# else /* FreeBSD-4 */
  if (rptr != NULL)
    *rptr = resp;
  else if (resp != NULL)
    FREE(resp, M_NETGRAPH);
  FREE(msg, M_NETGRAPH);
# endif

  return error;
  }

/* This is a persistent netgraph node. */
static int
ng_shutdown(node_p node)
  {
# if (__FreeBSD_version >= 500000)
  /* unless told to really die, bounce back to life */
  if ((node->nd_flags & NG_REALLY_DIE)==0)
    node->nd_flags &= ~NG_INVALID; /* bounce back to life */
# else /* FreeBSD-4 */
  ng_cutlinks(node);
  node->flags &= ~NG_INVALID;  /* bounce back to life */
# endif

  return 0;
  }

/* ng_disconnect is the opposite of this procedure. */
static int
ng_newhook(node_p node, hook_p hook, const char *name)
  {
  softc_t *sc = NG_NODE_PRIVATE(node);

  /* Hook name must be 'rawdata'. */
  if (strncmp(name, "rawdata", 7) != 0)	return EINVAL;

  /* Is our hook connected? */
  if (sc->ng_hook != NULL) return EBUSY;

  /* Accept the hook. */
  sc->ng_hook = hook;

  return 0;
  }

/* Both ends have accepted their hooks and the links have been made. */
/* This is the last chance to reject the connection request. */
static int
ng_connect(hook_p hook)
  {
  /* Probably not at splnet, force outward queueing. (huh?) */
  NG_HOOK_FORCE_QUEUE(NG_HOOK_PEER(hook));
  return 0; /* always accept */
  }

/* Receive data in mbufs from another Netgraph node. */
/* Transmit an mbuf-chain on the communication link. */
/* This procedure is very similar to raw_output(). */
/* Called from a syscall (user context; no spinlocks). */
# if (__FreeBSD_version >= 500000)
static int
ng_rcvdata(hook_p hook, item_p item)
  {
  softc_t *sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
  int error = 0;
  struct mbuf *m;
  meta_p meta = NULL;

  NGI_GET_M(item, m);
  NGI_GET_META(item, meta);
  NG_FREE_ITEM(item);
# else /* FreeBSD-4 */
static int
ng_rcvdata(hook_p hook, struct mbuf *m, meta_p meta)
  {
  softc_t *sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
  int error = 0;
# endif

  /* This macro must not store into meta! */
  NG_FREE_META(meta);

  /* Fail if the link is down. */
  if (sc->status.oper_status  != STATUS_UP)
    {
    m_freem(m);
    sc->status.cntrs.odiscards++;
    if (DRIVER_DEBUG)
      printf("%s: ng_rcvdata: tx pkt discarded: link down\n", NAME_UNIT);
    return ENETDOWN;
    }

  /* ng_rcvdata() ENQUEUEs in a syscall or softirq. */
  /* txintr_setup() DEQUEUEs in a hard interrupt. */
  /* Some BSD QUEUE routines are not interrupt-safe. */
  {
  DISABLE_INTR;
# if (__FreeBSD_version >= 503000)
  if (meta==NULL)
    IFQ_ENQUEUE(&sc->ng_sndq, m, error);
  else
    IFQ_ENQUEUE(&sc->ng_fastq, m, error);
# else
  if (meta==NULL)
    IFQ_ENQUEUE(&sc->ng_sndq, m, NULL, error);
  else
    IFQ_ENQUEUE(&sc->ng_fastq, m, NULL, error);
# endif
  ENABLE_INTR;
  }

  if (error==0)
    user_interrupt(sc, 0); /* start the transmitter */
  else
    {
    m_freem(m);
    sc->status.cntrs.odiscards++;
    if (DRIVER_DEBUG)
      printf("%s: ng_rcvdata: IFQ_ENQUEUE() failed; error %d\n",
       NAME_UNIT, error);
    }

  return error;
  }

/* ng_newhook is the opposite of this procedure, not */
/*  ng_connect, as you might expect from the names. */
static int
ng_disconnect(hook_p hook)
  {
  softc_t *sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));

  /* Disconnect the hook. */
  sc->ng_hook = NULL;

  return 0;
  }

static
struct ng_type ng_type =
  {
  .version	= NG_ABI_VERSION,
  .name		= NG_LMC_NODE_TYPE,
  .mod_event	= NULL,
  .constructor	= ng_constructor,
  .rcvmsg	= ng_rcvmsg,
# if (__FreeBSD_version >=503000)
  .close	= NULL,
# endif
  .shutdown	= ng_shutdown,
  .newhook	= ng_newhook,
  .findhook	= NULL,
  .connect	= ng_connect,
  .rcvdata	= ng_rcvdata,
# if (defined(__FreeBSD__) && (__FreeBSD_version < 500000))
  .rcvdataq	= ng_rcvdata,
# endif
  .disconnect	= ng_disconnect,
  };

# if (IFNET == 0)
/* Called from a softirq once a second. */
static void
ng_watchdog(void *arg)
  {
  softc_t *sc = arg;

  /* Call the core watchdog procedure. */
  core_watchdog(sc);

  /* Set line protocol and package status. */
  sc->status.line_pkg  = PKG_NG;
  sc->status.line_prot = 0;

  /* Call this procedure again after one second. */
  callout_reset(&sc->ng_callout, hz, ng_watchdog, sc);
  }
# endif

/* Attach to the Netgraph kernel interface (/sys/netgraph).
 * It is called once for each physical card during device attach.
 * This is effectively ng_constructor.
 */
static int
ng_attach(softc_t *sc)
  {
  int error;

  /* If this node type is not known to Netgraph then register it. */
  if (ng_type.refs == 0) /* or: if (ng_findtype(&ng_type) == NULL) */
    {
    if ((error = ng_newtype(&ng_type)))
      {
      printf("%s: ng_newtype() failed; error %d\n", NAME_UNIT, error);
      return error;
      }
    }
  else
    NG_TYPE_REF(&ng_type);

  /* Call the superclass node constructor. */
  if ((error = ng_make_node_common(&ng_type, &sc->ng_node)))
    {
    NG_TYPE_UNREF(&ng_type);
    printf("%s: ng_make_node_common() failed; error %d\n", NAME_UNIT, error);
    return error;
    }

  /* Associate a name with this netgraph node. */
  if ((error = ng_name_node(sc->ng_node, NAME_UNIT)))
    {
    NG_NODE_UNREF(sc->ng_node);
    NG_TYPE_UNREF(&ng_type);
    printf("%s: ng_name_node() failed; error %d\n", NAME_UNIT, error);
    return error;
    }

# if (__FreeBSD_version >= 500000)
  /* Initialize the send queue mutexes. */
  mtx_init(&sc->ng_sndq.ifq_mtx,  NAME_UNIT, "sndq",  MTX_DEF);
  mtx_init(&sc->ng_fastq.ifq_mtx, NAME_UNIT, "fastq", MTX_DEF);
# endif

  /* Put a backpointer to the softc in the netgraph node. */
  NG_NODE_SET_PRIVATE(sc->ng_node, sc);

  /* ALTQ output queue initialization. */
  IFQ_SET_MAXLEN(&sc->ng_fastq, SNDQ_MAXLEN);
  IFQ_SET_READY(&sc->ng_fastq);
  IFQ_SET_MAXLEN(&sc->ng_sndq,  SNDQ_MAXLEN);
  IFQ_SET_READY(&sc->ng_sndq);

  /* If ifnet is present, it will call watchdog. */
  /* Otherwise, arrange to call watchdog here. */
# if (IFNET == 0)
  /* Arrange to call ng_watchdog() once a second. */
#  if (__FreeBSD_version >= 500000)
  callout_init(&sc->ng_callout, 0);
#  else  /* FreeBSD-4 */
  callout_init(&sc->ng_callout);
#  endif
  callout_reset(&sc->ng_callout, hz, ng_watchdog, sc);
# endif

  return 0;
  }

static void
ng_detach(softc_t *sc)
  {
# if (IFNET == 0)
  callout_stop(&sc->ng_callout);
# endif
# if (__FreeBSD_version >= 500000)
  mtx_destroy(&sc->ng_sndq.ifq_mtx);
  mtx_destroy(&sc->ng_fastq.ifq_mtx);
  ng_rmnode_self(sc->ng_node); /* free hook */
  NG_NODE_UNREF(sc->ng_node);  /* free node */
  NG_TYPE_UNREF(&ng_type);
# else /* FreeBSD-4 */
  ng_unname(sc->ng_node);      /* free name */
  ng_cutlinks(sc->ng_node);    /* free hook */
  NG_NODE_UNREF(sc->ng_node);  /* free node */
  NG_TYPE_UNREF(&ng_type);
# endif
  }

#endif /* NETGRAPH */

/* The next few procedures initialize the card. */

/* Returns 0 on success; error code on failure. */
static int
startup_card(softc_t *sc)
  {
  int num_rx_descs, error = 0;
  u_int32_t tlp_bus_pbl, tlp_bus_cal, tlp_op_tr;
  u_int32_t tlp_cfdd, tlp_cfcs;
  u_int32_t tlp_cflt, tlp_csid, tlp_cfit;

  /* Make sure the COMMAND bits are reasonable. */
  tlp_cfcs = READ_PCI_CFG(sc, TLP_CFCS);
  tlp_cfcs &= ~TLP_CFCS_MWI_ENABLE;
  tlp_cfcs |=  TLP_CFCS_BUS_MASTER;
  tlp_cfcs |=  TLP_CFCS_MEM_ENABLE;
  tlp_cfcs |=  TLP_CFCS_IO_ENABLE;
  tlp_cfcs |=  TLP_CFCS_PAR_ERROR;
  tlp_cfcs |=  TLP_CFCS_SYS_ERROR;
  WRITE_PCI_CFG(sc, TLP_CFCS, tlp_cfcs);

  /* Set the LATENCY TIMER to the recommended value, */
  /*  and make sure the CACHE LINE SIZE is reasonable. */
  tlp_cfit = READ_PCI_CFG(sc, TLP_CFIT);
  tlp_cflt = READ_PCI_CFG(sc, TLP_CFLT);
  tlp_cflt &= ~TLP_CFLT_LATENCY;
  tlp_cflt |= (tlp_cfit & TLP_CFIT_MAX_LAT)>>16;
  /* "prgmbl burst length" and "cache alignment" used below. */
  switch(tlp_cflt & TLP_CFLT_CACHE)
    {
    case 8: /* 8 bytes per cache line */
      { tlp_bus_pbl = 32; tlp_bus_cal = 1; break; }
    case 16:
      { tlp_bus_pbl = 32; tlp_bus_cal = 2; break; }
    case 32:
      { tlp_bus_pbl = 32; tlp_bus_cal = 3; break; }
    default:
      {
      tlp_bus_pbl = 32; tlp_bus_cal = 1;
      tlp_cflt &= ~TLP_CFLT_CACHE;
      tlp_cflt |= 8;
      break;
      }
    }
  WRITE_PCI_CFG(sc, TLP_CFLT, tlp_cflt);

  /* Make sure SNOOZE and SLEEP modes are disabled. */
  tlp_cfdd = READ_PCI_CFG(sc, TLP_CFDD);
  tlp_cfdd &= ~TLP_CFDD_SLEEP;
  tlp_cfdd &= ~TLP_CFDD_SNOOZE;
  WRITE_PCI_CFG(sc, TLP_CFDD, tlp_cfdd);
  DELAY(11*1000); /* Tulip wakes up in 10 ms max */

  /* Software Reset the Tulip chip; stops DMA and Interrupts. */
  /* This does not change the PCI config regs just set above. */
  WRITE_CSR(TLP_BUS_MODE, TLP_BUS_RESET); /* self-clearing */
  DELAY(5);  /* Tulip is dead for 50 PCI cycles after reset. */

  /* Reset the Xilinx Field Programmable Gate Array. */
  reset_xilinx(sc); /* side effect: turns on all four LEDs */

  /* Configure card-specific stuff (framers, line interfaces, etc.). */
  sc->card->config(sc);

  /* Initializing cards can glitch clocks and upset fifos. */
  /* Reset the FIFOs between the Tulip and Xilinx chips. */
  set_mii16_bits(sc, MII16_FIFO);
  clr_mii16_bits(sc, MII16_FIFO);

  /* Initialize the PCI busmode register. */
  /* The PCI bus cycle type "Memory Write and Invalidate" does NOT */
  /*  work cleanly in any version of the 21140A, so don't enable it! */
  WRITE_CSR(TLP_BUS_MODE,
        (tlp_bus_cal ? TLP_BUS_READ_LINE : 0) |
        (tlp_bus_cal ? TLP_BUS_READ_MULT : 0) |
        (tlp_bus_pbl<<TLP_BUS_PBL_SHIFT) |
        (tlp_bus_cal<<TLP_BUS_CAL_SHIFT) |
   ((BYTE_ORDER == BIG_ENDIAN) ? TLP_BUS_DESC_BIGEND : 0) |
   ((BYTE_ORDER == BIG_ENDIAN) ? TLP_BUS_DATA_BIGEND : 0) |
                TLP_BUS_DSL_VAL |
                TLP_BUS_ARB);

  /* Pick number of RX descriptors and TX fifo threshold. */
  /* tx_threshold in bytes: 0=128, 1=256, 2=512, 3=1024 */
  tlp_csid = READ_PCI_CFG(sc, TLP_CSID);
  switch(tlp_csid)
    {
    case TLP_CSID_HSSI:		/* 52 Mb/s */
    case TLP_CSID_HSSIc:	/* 52 Mb/s */
    case TLP_CSID_T3:		/* 45 Mb/s */
      { num_rx_descs = 48; tlp_op_tr = 2; break; }
    case TLP_CSID_SSI:		/* 10 Mb/s */
      { num_rx_descs = 32; tlp_op_tr = 1; break; }
    case TLP_CSID_T1E1:		/*  2 Mb/s */
      { num_rx_descs = 16; tlp_op_tr = 0; break; }
    default:
      { num_rx_descs = 16; tlp_op_tr = 0; break; }
    }

  /* Create DMA descriptors and initialize list head registers. */
  if ((error = create_ring(sc, &sc->txring, NUM_TX_DESCS))) return error;
  WRITE_CSR(TLP_TX_LIST, sc->txring.dma_addr);
  if ((error = create_ring(sc, &sc->rxring, num_rx_descs))) return error;
  WRITE_CSR(TLP_RX_LIST, sc->rxring.dma_addr);

  /* Initialize the operating mode register. */
  WRITE_CSR(TLP_OP_MODE, TLP_OP_INIT | (tlp_op_tr<<TLP_OP_TR_SHIFT));

  /* Read the missed frame register (result ignored) to zero it. */
  error = READ_CSR( TLP_MISSED); /* error is used as a bit-dump */

  /* Disable rx watchdog and tx jabber features. */
  WRITE_CSR(TLP_WDOG, TLP_WDOG_INIT);

  /* Enable card interrupts. */
  WRITE_CSR(TLP_INT_ENBL, TLP_INT_TXRX);

  return 0;
  }

/* Stop DMA and Interrupts; free descriptors and buffers. */
static void
shutdown_card(void *arg)
  {
  softc_t *sc = arg;

  /* Leave the LEDs in the state they were in after power-on. */
  led_on(sc, MII16_LED_ALL);

  /* Software reset the Tulip chip; stops DMA and Interrupts */
  WRITE_CSR(TLP_BUS_MODE, TLP_BUS_RESET); /* self-clearing */
  DELAY(5);  /* Tulip is dead for 50 PCI cycles after reset. */

  /* Disconnect from the PCI bus except for config cycles. */
  /* Hmmm; Linux syslogs a warning that IO and MEM are disabled. */
  WRITE_PCI_CFG(sc, TLP_CFCS, TLP_CFCS_MEM_ENABLE | TLP_CFCS_IO_ENABLE);

  /* Free the DMA descriptor rings. */
  destroy_ring(sc, &sc->txring);
  destroy_ring(sc, &sc->rxring);
  }

/* Start the card and attach a kernel interface and line protocol. */
static int
attach_card(softc_t *sc, const char *intrstr)
  {
  struct config config;
  u_int32_t tlp_cfrv;
  u_int16_t mii3;
  u_int8_t *ieee;
  int i, error = 0;

  /* Start the card. */
  if ((error = startup_card(sc))) return error;

  /* Attach a kernel interface. */
#if NETGRAPH
  if ((error = ng_attach(sc))) return error;
  sc->flags |= FLAG_NETGRAPH;
#endif
#if IFNET
  if ((error = ifnet_attach(sc))) return error;
  sc->flags |= FLAG_IFNET;
#endif

  /* Attach a line protocol stack. */
  sc->config.line_pkg = PKG_RAWIP;
  config = sc->config;	/* get current config */
  config.line_pkg = 0;	/* select external stack */
  config.line_prot = PROT_C_HDLC;
  config.keep_alive = 1;
  config_proto(sc, &config); /* reconfigure */
  sc->config = config;	/* save new configuration */

  /* Print interesting hardware-related things. */
  mii3 = read_mii(sc, 3);
  tlp_cfrv = READ_PCI_CFG(sc, TLP_CFRV);
  printf("%s: PCI rev %d.%d, MII rev %d.%d", NAME_UNIT,
   (tlp_cfrv>>4) & 0xF, tlp_cfrv & 0xF, (mii3>>4) & 0xF, mii3 & 0xF);
  ieee = (u_int8_t *)sc->status.ieee;
  for (i=0; i<3; i++) sc->status.ieee[i] = read_srom(sc, 10+i);
  printf(", IEEE addr %02x:%02x:%02x:%02x:%02x:%02x",
   ieee[0], ieee[1], ieee[2], ieee[3], ieee[4], ieee[5]);
  sc->card->ident(sc);
  printf(" %s\n", intrstr);

  /* Print interesting software-related things. */
  printf("%s: Driver rev %d.%d.%d", NAME_UNIT,
   DRIVER_MAJOR_VERSION, DRIVER_MINOR_VERSION, DRIVER_SUB_VERSION);
  printf(", Options %s%s%s%s%s%s%s%s%s\n",
   NETGRAPH ? "NETGRAPH " : "", GEN_HDLC ? "GEN_HDLC " : "",
   NSPPP ? "SPPP " : "", P2P ? "P2P " : "",
   ALTQ_PRESENT ? "ALTQ " : "", NBPFILTER ? "BPF " : "",
   DEV_POLL ? "POLL " : "", IOREF_CSR ? "IO_CSR " : "MEM_CSR ",
   (BYTE_ORDER == BIG_ENDIAN) ? "BIG_END " : "LITTLE_END ");

  /* Make the local hardware ready. */
  set_status(sc, 1);

  return 0;
  }

/* Detach from the kernel in all ways. */
static void
detach_card(softc_t *sc)
  {
  struct config config;

  /* Make the local hardware NOT ready. */
  set_status(sc, 0);

  /* Detach external line protocol stack. */
  if (sc->config.line_pkg != PKG_RAWIP)
    {
    config = sc->config;
    config.line_pkg = PKG_RAWIP;
    config_proto(sc, &config);
    sc->config = config;
    }

  /* Detach kernel interfaces. */
#if NETGRAPH
  if (sc->flags & FLAG_NETGRAPH)
    {
    IFQ_PURGE(&sc->ng_fastq);
    IFQ_PURGE(&sc->ng_sndq);
    ng_detach(sc);
    sc->flags &= ~FLAG_NETGRAPH;
    }
#endif
#if IFNET
  if (sc->flags & FLAG_IFNET)
    {
    IFQ_PURGE(&sc->ifp->if_snd);
    ifnet_detach(sc);
    sc->flags &= ~FLAG_IFNET;
    }
#endif

  /* Reset the Tulip chip; stops DMA and Interrupts. */
  shutdown_card(sc);
  }

/* This is the I/O configuration interface for FreeBSD */

#ifdef __FreeBSD__

static int
fbsd_probe(device_t dev)
  {
  u_int32_t cfid = pci_read_config(dev, TLP_CFID, 4);
  u_int32_t csid = pci_read_config(dev, TLP_CSID, 4);

  /* Looking for a DEC 21140A chip on any Lan Media Corp card. */
  if (cfid != TLP_CFID_TULIP) return ENXIO;
  switch (csid)
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
      device_set_desc(dev, HSSI_DESC);
      break;
    case TLP_CSID_T3:
      device_set_desc(dev,   T3_DESC);
      break;
    case TLP_CSID_SSI:
      device_set_desc(dev,  SSI_DESC);
      break;
    case TLP_CSID_T1E1:
      device_set_desc(dev, T1E1_DESC);
      break;
    default:
      return ENXIO;
    }
  return 0;
  }

static int
fbsd_detach(device_t dev)
  {
  softc_t *sc = device_get_softc(dev);

  /* Stop the card and detach from the kernel. */
  detach_card(sc);

  /* Release resources. */
  if (sc->irq_cookie != NULL)
    {
    bus_teardown_intr(dev, sc->irq_res, sc->irq_cookie);
    sc->irq_cookie = NULL;
    }
  if (sc->irq_res != NULL)
    {
    bus_release_resource(dev, SYS_RES_IRQ, sc->irq_res_id, sc->irq_res);
    sc->irq_res = NULL;
    }
  if (sc->csr_res != NULL)
    {
    bus_release_resource(dev, sc->csr_res_type, sc->csr_res_id, sc->csr_res);
    sc->csr_res = NULL;
    }

# if (__FreeBSD_version >= 500000)
  mtx_destroy(&sc->top_mtx);
  mtx_destroy(&sc->bottom_mtx);
# endif
  return 0; /* no error */
  }

static void
fbsd_shutdown(device_t dev)
  {
  shutdown_card(device_get_softc(dev));
  }

static int
fbsd_attach(device_t dev)
  {
  softc_t *sc = device_get_softc(dev);
  int error;

  /* READ/WRITE_PCI_CFG need this. */
  sc->dev = dev;

  /* What kind of card are we driving? */
  switch (READ_PCI_CFG(sc, TLP_CSID))
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
      sc->card = &hssi_card;
      break;
    case TLP_CSID_T3:
      sc->card =   &t3_card;
      break;
    case TLP_CSID_SSI:
      sc->card =  &ssi_card;
      break;
    case TLP_CSID_T1E1:
      sc->card =   &t1_card;
      break;
    default:
      return ENXIO;
    }
  sc->dev_desc = device_get_desc(dev);

  /* Allocate PCI memory or IO resources to access the Tulip chip CSRs. */
# if IOREF_CSR
  sc->csr_res_id   = TLP_CBIO;
  sc->csr_res_type = SYS_RES_IOPORT;
# else
  sc->csr_res_id   = TLP_CBMA;
  sc->csr_res_type = SYS_RES_MEMORY;
# endif
  sc->csr_res = bus_alloc_resource(dev, sc->csr_res_type, &sc->csr_res_id,
   0, ~0, 1, RF_ACTIVE);
  if (sc->csr_res == NULL)
    {
    printf("%s: bus_alloc_resource(csr) failed.\n", NAME_UNIT);
    return ENXIO;
    }
  sc->csr_tag    = rman_get_bustag(sc->csr_res);
  sc->csr_handle = rman_get_bushandle(sc->csr_res); 

  /* Allocate PCI interrupt resources for the card. */
  sc->irq_res_id = 0;
  sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_res_id,
   0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
  if (sc->irq_res == NULL)
    {
    printf("%s: bus_alloc_resource(irq) failed.\n", NAME_UNIT);
    fbsd_detach(dev);
    return ENXIO;
    }
  if ((error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
   bsd_interrupt, sc, &sc->irq_cookie)))
    {
    printf("%s: bus_setup_intr() failed; error %d\n", NAME_UNIT, error);
    fbsd_detach(dev);
    return error;
    }

# if (__FreeBSD_version >= 500000)
  /* Initialize the top-half and bottom-half locks. */
  mtx_init(&sc->top_mtx,    NAME_UNIT, "top half lock",    MTX_DEF);
  mtx_init(&sc->bottom_mtx, NAME_UNIT, "bottom half lock", MTX_DEF);
# endif

  /* Start the card and attach a kernel interface and line protocol. */
  if ((error = attach_card(sc, ""))) detach_card(sc);
  return error;
  }

static device_method_t methods[] =
  {
  DEVMETHOD(device_probe,    fbsd_probe),
  DEVMETHOD(device_attach,   fbsd_attach),
  DEVMETHOD(device_detach,   fbsd_detach),
  DEVMETHOD(device_shutdown, fbsd_shutdown),
  /* This driver does not suspend and resume. */
  { 0, 0 }
  };

static driver_t driver =
  {
  .name    = DEVICE_NAME,
  .methods = methods,
# if (__FreeBSD_version >= 500000)
  .size    = sizeof(softc_t),
# else /* FreeBSD-4 */
  .softc   = sizeof(softc_t),
# endif
  };

static devclass_t devclass;

DRIVER_MODULE(if_lmc, pci, driver, devclass, 0, 0);
MODULE_VERSION(if_lmc, 2);
MODULE_DEPEND(if_lmc, pci, 1, 1, 1);
# if NETGRAPH
MODULE_DEPEND(if_lmc, netgraph, NG_ABI_VERSION, NG_ABI_VERSION, NG_ABI_VERSION);
# endif
# if NSPPP
MODULE_DEPEND(if_lmc, sppp, 1, 1, 1);
# endif

#endif  /* __FreeBSD__ */

/* This is the I/O configuration interface for NetBSD. */

#ifdef __NetBSD__

static int
nbsd_match(struct device *parent, struct cfdata *match, void *aux)
  {
  struct pci_attach_args *pa = aux;
  u_int32_t cfid = pci_conf_read(pa->pa_pc, pa->pa_tag, TLP_CFID);
  u_int32_t csid = pci_conf_read(pa->pa_pc, pa->pa_tag, TLP_CSID);	

  /* Looking for a DEC 21140A chip on any Lan Media Corp card. */
  if (cfid != TLP_CFID_TULIP) return 0;
  switch (csid)
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
    case TLP_CSID_T3:
    case TLP_CSID_SSI:
    case TLP_CSID_T1E1:
      return 100;
    default:
      return 0;
    }
  }

static int
nbsd_detach(struct device *self, int flags)
  {
  softc_t *sc = (softc_t *)self; /* device is first in softc */

  /* Stop the card and detach from the kernel. */
  detach_card(sc);

  /* Release resources. */
  if (sc->sdh_cookie != NULL)
    {
    shutdownhook_disestablish(sc->sdh_cookie);
    sc->sdh_cookie = NULL;
    }
  if (sc->irq_cookie != NULL)
    {
    pci_intr_disestablish(sc->pa_pc, sc->irq_cookie);
    sc->irq_cookie = NULL;
    }
  if (sc->csr_handle)
    {
    bus_space_unmap(sc->csr_tag, sc->csr_handle, TLP_CSR_SIZE);
    sc->csr_handle = 0;
    }

  return 0; /* no error */
  }

static void
nbsd_attach(struct device *parent, struct device *self, void *aux)
  {
  softc_t *sc = (softc_t *)self; /* device is first in softc */
  struct pci_attach_args *pa = aux;
  const char *intrstr;
  bus_addr_t csr_addr;
  int error;

  /* READ/WRITE_PCI_CFG need these. */
  sc->pa_pc   = pa->pa_pc;
  sc->pa_tag  = pa->pa_tag;
  /* bus_dma needs this. */
  sc->pa_dmat = pa->pa_dmat;

  /* What kind of card are we driving? */
  switch (READ_PCI_CFG(sc, TLP_CSID))
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
      sc->dev_desc =  HSSI_DESC;
      sc->card     = &hssi_card;
      break;
    case TLP_CSID_T3:
      sc->dev_desc =    T3_DESC;
      sc->card     =   &t3_card;
      break;
    case TLP_CSID_SSI:
      sc->dev_desc =   SSI_DESC;
      sc->card     =  &ssi_card;
      break;
    case TLP_CSID_T1E1:
      sc->dev_desc =  T1E1_DESC;
      sc->card     =   &t1_card;
      break;
    default:
      return;
    }
  printf(": %s\n", sc->dev_desc);

  /* Allocate PCI resources to access the Tulip chip CSRs. */
# if IOREF_CSR
  csr_addr = (bus_addr_t)READ_PCI_CFG(sc, TLP_CBIO) & -2;
  sc->csr_tag = pa->pa_iot;	/* bus_space tag for IO refs */
# else
  csr_addr = (bus_addr_t)READ_PCI_CFG(sc, TLP_CBMA);
  sc->csr_tag = pa->pa_memt;	/* bus_space tag for MEM refs */
# endif
  if ((error = bus_space_map(sc->csr_tag, csr_addr,
   TLP_CSR_SIZE, 0, &sc->csr_handle)))
    {
    printf("%s: bus_space_map() failed; error %d\n", NAME_UNIT, error);
    return;
    }

  /* Allocate PCI interrupt resources. */
  if ((error = pci_intr_map(pa, &sc->intr_handle)))
    {
    printf("%s: pci_intr_map() failed; error %d\n", NAME_UNIT, error);
    nbsd_detach(self, 0);
    return;
    }
  sc->irq_cookie = pci_intr_establish(pa->pa_pc, sc->intr_handle,
   IPL_NET, bsd_interrupt, sc);
  if (sc->irq_cookie == NULL)
    {
    printf("%s: pci_intr_establish() failed\n", NAME_UNIT);
    nbsd_detach(self, 0);
    return;
    }
  intrstr = pci_intr_string(pa->pa_pc, sc->intr_handle);

  /* Install a shutdown hook. */
  sc->sdh_cookie = shutdownhook_establish(shutdown_card, sc);
  if (sc->sdh_cookie == NULL)
    {
    printf("%s: shutdown_hook_establish() failed\n", NAME_UNIT);
    nbsd_detach(self, 0);
    return;
    }

  /* Initialize the top-half and bottom-half locks. */
  simple_lock_init(&sc->top_lock);
  simple_lock_init(&sc->bottom_lock);

  /* Start the card and attach a kernel interface and line protocol. */
  if ((error = attach_card(sc, intrstr))) detach_card(sc);
  }

# if (__NetBSD_Version__ >= 106080000) /* 1.6H */
CFATTACH_DECL(lmc, sizeof(softc_t),
 nbsd_match, nbsd_attach, nbsd_detach, NULL);
# else
struct cfattach lmc_ca =
  {
/*.ca_name	= DEVICE_NAME, */
  .ca_devsize	= sizeof(softc_t),
  .ca_match	= nbsd_match,
  .ca_attach	= nbsd_attach,
  .ca_detach	= nbsd_detach,
  .ca_activate	= NULL,
  };
# endif

# if (__NetBSD_Version__ >= 106080000)
CFDRIVER_DECL(lmc, DV_IFNET, NULL);
# else
static struct cfdriver lmc_cd =
  {
  .cd_name	= DEVICE_NAME,
  .cd_class	= DV_IFNET,
  .cd_ndevs	= 0,
  .cd_devs	= NULL,
  };
# endif

/* cfdata is declared static, unseen outside this module. */
/* It is used for LKM; config builds its own in ioconf.c. */
static struct cfdata lmc_cf =
  {
# if (__NetBSD_Version__ >= 106080000)
  .cf_name	= DEVICE_NAME,
  .cf_atname    = DEVICE_NAME,
# else
  .cf_driver	= &lmc_cd,
  .cf_attach	= &lmc_ca,
# endif
  .cf_unit	= 0,
  .cf_fstate	= FSTATE_STAR,
  };

# if (__NetBSD_Version__ >= 106080000)
MOD_MISC(DEVICE_NAME)
# else
static struct lkm_misc _module =
  {
  .lkm_name	= DEVICE_NAME,
  .lkm_type	= LM_MISC,
  .lkm_offset	= 0,
  .lkm_ver	= LKM_VERSION,
  };
# endif

/* From /sys/dev/pci/pci.c (no public prototype). */
int pciprint(void *, const char *);

static int lkm_nbsd_match(struct pci_attach_args *pa)
  { return nbsd_match(0, 0, pa); }

/* LKM loader finds this by appending "_lkmentry" to filename "if_lmc". */
int if_lmc_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
  {
  int i, error = 0;

  if (ver != LKM_VERSION) return EINVAL;
  switch (cmd)
    {
    case LKM_E_LOAD:
      {
      struct cfdriver* pcicd;

      lkmtp->private.lkm_misc = &_module;
      if ((pcicd = config_cfdriver_lookup("pci")) == NULL)
        {
        printf("%s: config_cfdriver_lookup(pci) failed; error %d\n",
         lmc_cd.cd_name, error);
        return error;
	}
# if (__NetBSD_Version__ >= 106080000)
      if ((error = config_cfdriver_attach(&lmc_cd)))
        {
        printf("%s: config_cfdriver_attach() failed; error %d\n",
         lmc_cd.cd_name, error);
        return error;
        }
      if ((error = config_cfattach_attach(lmc_cd.cd_name, &lmc_ca)))
        {
        printf("%s: config_cfattach_attach() failed; error %d\n",
         lmc_cd.cd_name, error);
        config_cfdriver_detach(&lmc_cd);
        return error;
        }
# endif
      for (i=0; i<pcicd->cd_ndevs; i++)
        {
        int dev;
        /* A pointer to a device is a pointer to its softc. */
        struct pci_softc *sc = pcicd->cd_devs[i];
        if (sc == NULL) continue;
        for (dev=0; dev<sc->sc_maxndevs; dev++)
          {
          struct pci_attach_args pa;
          pcitag_t tag = pci_make_tag(sc->sc_pc, sc->sc_bus, dev, 0);
          if (pci_probe_device(sc, tag, lkm_nbsd_match, &pa) != 0)
            config_attach(pcicd->cd_devs[i], &lmc_cf, &pa, pciprint);
            /* config_attach doesn't return on failure; it calls panic. */
          }
	}
      break;
      }
    case LKM_E_UNLOAD:
      {
      for (i=lmc_cd.cd_ndevs-1; i>=0; i--)
        {
        struct device *dev = lmc_cd.cd_devs[i];
        if (dev == NULL) continue;
        if ((error = config_detach(dev, 0)))
          {
          printf("%s: config_detach() failed; error %d\n",
           dev->dv_xname, error);
          return error;
	  }
	}
# if (__NetBSD_Version__ >= 106080000)
      if ((error = config_cfattach_detach(lmc_cd.cd_name, &lmc_ca)))
        {
        printf("%s: config_cfattach_detach() failed; error %d\n",
         lmc_cd.cd_name, error);
        return error;
        }
      if ((error = config_cfdriver_detach(&lmc_cd)))
        {
        printf("%s: config_cfdriver_detach() failed; error %d\n",
         lmc_cd.cd_name, error);
        return error;
        }
# endif
      break;
      }
    case LKM_E_STAT:
      break;
    }

  return error;
  }

#endif  /* __NetBSD__ */

/* This is the I/O configuration interface for OpenBSD. */

#ifdef __OpenBSD__

static int
obsd_match(struct device *parent, void *match, void *aux)
  {
  struct pci_attach_args *pa = aux;
  u_int32_t cfid = pci_conf_read(pa->pa_pc, pa->pa_tag, TLP_CFID);
  u_int32_t csid = pci_conf_read(pa->pa_pc, pa->pa_tag, TLP_CSID);	

  /* Looking for a DEC 21140A chip on any Lan Media Corp card. */
  if (cfid != TLP_CFID_TULIP) return 0;
  switch (csid)
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
    case TLP_CSID_T3:
    case TLP_CSID_SSI:
    case TLP_CSID_T1E1:
      return 100; /* match better than other 21140 drivers */
    default:
      return 0;
    }
  }

static int
obsd_detach(struct device *self, int flags)
  {
  softc_t *sc = (softc_t *)self; /* device is first in softc */

  /* Stop the card and detach from the kernel. */
  detach_card(sc);

  /* Release resources. */
  if (sc->sdh_cookie != NULL)
    {
    shutdownhook_disestablish(sc->sdh_cookie);
    sc->sdh_cookie = NULL;
    }
  if (sc->irq_cookie != NULL)
    {
    pci_intr_disestablish(sc->pa_pc, sc->irq_cookie);
    sc->irq_cookie = NULL;
    }
  if (sc->csr_handle)
    {
    bus_space_unmap(sc->csr_tag, sc->csr_handle, TLP_CSR_SIZE);
    sc->csr_handle = 0;
    }

  return 0; /* no error */
  }

static void
obsd_attach(struct device *parent, struct device *self, void *aux)
  {
  softc_t *sc = (softc_t *)self; /* device is first in softc */
  struct pci_attach_args *pa = aux;
  const char *intrstr;
  bus_addr_t csr_addr;
  int error;

  /* READ/WRITE_PCI_CFG need these. */
  sc->pa_pc   = pa->pa_pc;
  sc->pa_tag  = pa->pa_tag;
  /* bus_dma needs this. */
  sc->pa_dmat = pa->pa_dmat;

  /* What kind of card are we driving? */
  switch (READ_PCI_CFG(sc, TLP_CSID))
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
      sc->dev_desc =  HSSI_DESC;
      sc->card     = &hssi_card;
      break;
    case TLP_CSID_T3:
      sc->dev_desc =    T3_DESC;
      sc->card     =   &t3_card;
      break;
    case TLP_CSID_SSI:
      sc->dev_desc =   SSI_DESC;
      sc->card     =  &ssi_card;
      break;
    case TLP_CSID_T1E1:
      sc->dev_desc =  T1E1_DESC;
      sc->card     =   &t1_card;
      break;
    default:
      return;
    }
  printf(": %s\n", sc->dev_desc);

  /* Allocate PCI resources to access the Tulip chip CSRs. */
# if IOREF_CSR
  csr_addr = (bus_addr_t)READ_PCI_CFG(sc, TLP_CBIO) & -2;
  sc->csr_tag = pa->pa_iot;	/* bus_space tag for IO refs */
# else
  csr_addr = (bus_addr_t)READ_PCI_CFG(sc, TLP_CBMA);
  sc->csr_tag = pa->pa_memt;	/* bus_space tag for MEM refs */
# endif
  if ((error = bus_space_map(sc->csr_tag, csr_addr,
   TLP_CSR_SIZE, 0, &sc->csr_handle)))
    {
    printf("%s: bus_space_map() failed; error %d\n", NAME_UNIT, error);
    return;
    }

  /* Allocate PCI interrupt resources. */
  if ((error = pci_intr_map(pa, &sc->intr_handle)))
    {
    printf("%s: pci_intr_map() failed; error %d\n", NAME_UNIT, error);
    obsd_detach(self, 0);
    return;
    }
  sc->irq_cookie = pci_intr_establish(pa->pa_pc, sc->intr_handle,
   IPL_NET, bsd_interrupt, sc, self->dv_xname);
  if (sc->irq_cookie == NULL)
    {
    printf("%s: pci_intr_establish() failed\n", NAME_UNIT);
    obsd_detach(self, 0);
    return;
    }
  intrstr = pci_intr_string(pa->pa_pc, sc->intr_handle);

  /* Install a shutdown hook. */
  sc->sdh_cookie = shutdownhook_establish(shutdown_card, sc);
  if (sc->sdh_cookie == NULL)
    {
    printf("%s: shutdown_hook_establish() failed\n", NAME_UNIT);
    obsd_detach(self, 0);
    return;
    }

  /* Initialize the top-half and bottom-half locks. */
  simple_lock_init(&sc->top_lock);
  simple_lock_init(&sc->bottom_lock);

  /* Start the card and attach a kernel interface and line protocol. */
  if ((error = attach_card(sc, intrstr))) detach_card(sc);
  }

struct cfattach lmc_ca =
  {
  .ca_devsize	= sizeof(softc_t),
  .ca_match	= obsd_match,
  .ca_attach	= obsd_attach,
  .ca_detach	= obsd_detach,
  .ca_activate	= NULL,
  };

struct cfdriver lmc_cd =
  {
  .cd_name	= DEVICE_NAME,
  .cd_devs	= NULL,
  .cd_class	= DV_IFNET,
  .cd_indirect	= 0,
  .cd_ndevs	= 0,
  };

/* cfdata is declared static, unseen outside this module. */
/* It is used for LKM; config builds its own in ioconf.c. */
static struct cfdata lmc_cfdata =
  {
  .cf_attach	= &lmc_ca,
  .cf_driver	= &lmc_cd,
  .cf_unit	= 0,
  .cf_fstate	= FSTATE_STAR,
  };

static struct lkm_any _module =
  {
  .lkm_name	= DEVICE_NAME,
  .lkm_type	= LM_MISC,
  .lkm_offset	= 0,
  .lkm_ver	= LKM_VERSION,
  };

/* From /sys/dev/pci/pci.c (no public prototype). */
int pciprint(void *, const char *);

extern struct cfdriver pci_cd;

/* LKM loader finds this by appending "_lkmentry" to filename "if_lmc". */
int if_lmc_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
  {
  int i, error = 0;

  if (ver != LKM_VERSION) return EINVAL;
  switch (cmd)
    {
    case LKM_E_LOAD:
      {  /* XXX This works for ONE card on pci0 of a i386 machine! XXX */
      lkmtp->private.lkm_any = &_module;
      for (i=0; i<pci_cd.cd_ndevs; i++)
        {
        struct pci_attach_args pa;
        struct device *parent = pci_cd.cd_devs[i];
        if (parent == NULL) continue; /* dead clone? */
        if ((parent->dv_unit)!=0) continue; /* only bus zero */
        /* XXX For machine independence, need: pcibus_attach_args. XXX */
        /* XXX See NetBSD's sys/dev/pci/pci.c/pci_probe_device.    XXX */
        /* XXX Why isn't there an LKM network interface module?    XXX */
        pa.pa_pc    = NULL;					/* XXX */
        pa.pa_bus   = 0;					/* XXX */
        pa.pa_iot   = I386_BUS_SPACE_IO;			/* XXX */
        pa.pa_memt  = I386_BUS_SPACE_MEM;			/* XXX */
        pa.pa_dmat  = &pci_bus_dma_tag;				/* XXX */
        for (pa.pa_device=0; pa.pa_device<32; pa.pa_device++)	/* XXX */
          {
          int intr;
          pa.pa_function = 0; /* DEC-21140A has function 0 only    XXX */
          pa.pa_tag = pci_make_tag(pa.pa_pc, pa.pa_bus, pa.pa_device, 0);
          pa.pa_id = pci_conf_read(pa.pa_pc, pa.pa_tag, PCI_ID_REG);
          if ((pa.pa_id & 0xFFFF) == 0xFFFF) continue;
          if ((pa.pa_id & 0xFFFF) == 0) continue;
          /* XXX this only works for pci0 -- no swizzelling        XXX */
          pa.pa_intrswiz = 0;
          pa.pa_intrtag = pa.pa_tag;
          intr = pci_conf_read(pa.pa_pc, pa.pa_tag, PCI_INTERRUPT_REG);
          pa.pa_intrline = PCI_INTERRUPT_LINE(intr);
          pa.pa_intrpin = ((PCI_INTERRUPT_PIN(intr) -1) % 4) +1;
          if (obsd_match(parent, &lmc_cfdata, &pa))
            config_attach(parent, &lmc_cfdata, &pa, pciprint);
          /* config_attach doesn't return on failure; it calls panic. */
          }
	}
      break;
      }
    case LKM_E_UNLOAD:
      {
      for (i=lmc_cd.cd_ndevs-1; i>=0; i--)
        {
        struct device *dev = lmc_cd.cd_devs[i];
        if (dev == NULL) continue;
        if ((error = config_detach(dev, 0)))
          printf("%s: config_detach() failed; error %d\n", dev->dv_xname, error);
        }
      break;
      }
    case LKM_E_STAT:
      break;
    }

  return error;
  }

#endif  /* __OpenBSD__ */

/* This is the I/O configuration interface for BSD/OS. */

#ifdef __bsdi__

static int
bsdi_match(pci_devaddr_t *pa)
  {
  u_int32_t cfid = pci_inl(pa, TLP_CFID);
  u_int32_t csid = pci_inl(pa, TLP_CSID);

  /* Looking for a DEC 21140A chip on any Lan Media Corp card. */
  if (cfid != TLP_CFID_TULIP) return 0;
  switch (csid)
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
    case TLP_CSID_T3:
    case TLP_CSID_SSI:
    case TLP_CSID_T1E1:
      return 1;
    default:
      return 0;
    }
  }

static int
bsdi_probe(struct device *parent, struct cfdata *cf, void *aux)
  {
  struct isa_attach_args *ia = aux;
  pci_devaddr_t *pa = NULL;
  pci_devres_t res;

  /* This must be a PCI bus. */
  if (ia->ia_bustype != BUS_PCI) return 0;

  /* Scan PCI bus for our boards. */
  if ((pa = pci_scan(bsdi_match)) == 0) return 0;

  /* Scan config space for IO and MEM base registers and IRQ info. */
  pci_getres(pa, &res, 1, ia);

  /* Crucial: pass pci_devaddr to bsdi_attach in ia_aux. */
  ia->ia_aux = (void *)pa;

  return 1;
  }

static void
bsdi_attach(struct device *parent, struct device *self, void *aux)
  {
  softc_t *sc = (softc_t *)self; /* device is first in softc */
  struct isa_attach_args *ia = aux;
  pci_devaddr_t *pa = ia->ia_aux; /* this is crucial! */
  int error;

  /* READ/WRITE_PCI_CFG need this. */
  sc->cfgbase = *pa;

  /* What kind of card are we driving? */
  switch (READ_PCI_CFG(sc, TLP_CSID))
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
      sc->dev_desc =  HSSI_DESC;
      sc->card     = &hssi_card;
      break;
    case TLP_CSID_T3:
      sc->dev_desc =    T3_DESC;
      sc->card     =   &t3_card;
      break;
    case TLP_CSID_SSI:
      sc->dev_desc =   SSI_DESC;
      sc->card     =  &ssi_card;
      break;
    case TLP_CSID_T1E1:
      sc->dev_desc =  T1E1_DESC;
      sc->card     =   &t1_card;
      break;
    default:
      return;
    }
  printf(": %s\n", sc->dev_desc);

  /* Allocate PCI memory or IO resources to access the Tulip chip CSRs. */
  sc->csr_iobase  = ia->ia_iobase;
  sc->csr_membase = (u_int32_t *)mapphys((vm_offset_t)ia->ia_maddr, TLP_CSR_SIZE);

  /* Attach to the PCI bus. */
  isa_establish(&sc->id, &sc->dev);

  /* Allocate PCI interrupt resources for the card. */
  sc->ih.ih_fun = bsd_interrupt;
  sc->ih.ih_arg = sc;
  intr_establish(ia->ia_irq, &sc->ih, DV_NET);

  /* Install a shutdown hook. */
  sc->ats.func = shutdown_card;
  sc->ats.arg = sc;
  atshutdown(&sc->ats, ATSH_ADD);

  /* Initialize the top-half and bottom-half locks. */
  simple_lock_init(&sc->top_lock);
  simple_lock_init(&sc->bottom_lock);

  /* Start the card and attach a kernel interface and line protocol. */
  if ((error = attach_card(sc, ""))) detach_card(sc);
  }

struct cfdriver lmccd =
  {
  .cd_devs	= NULL,
  .cd_name	= DEVICE_NAME,
  .cd_match	= bsdi_probe,
  .cd_attach	= bsdi_attach,
  .cd_class	= DV_IFNET,
  .cd_devsize	= sizeof(softc_t),
  };
#endif  /* __bsdi__ */

#ifdef __linux__

/* The kernel calls this procedure when an interrupt happens. */
static irqreturn_t
linux_interrupt(int irq, void *dev, struct pt_regs *regs)
  {
  struct net_device *net_dev = dev;
  softc_t *sc = dev_to_hdlc(net_dev)->priv;

  /* Cut losses early if this is not our interrupt. */
  if ((READ_CSR(TLP_STATUS) & TLP_INT_TXRX) == 0)
    return IRQ_NONE;

  /* Disable card interrupts. */
  WRITE_CSR(TLP_INT_ENBL, TLP_INT_DISABLE);

  /* Handle the card interrupt with the dev->poll method. */
  if (netif_rx_schedule_prep(net_dev))
    __netif_rx_schedule(net_dev);  /* NAPI - add to poll list */
  else
    printk("%s: interrupt while on poll list\n", NAME_UNIT);

  return IRQ_HANDLED;
  }

/* This net_device method services interrupts in a softirq. */
/* With rxintr_cleanup(), it implements input flow control. */
static int
linux_poll(struct net_device *net_dev, int *budget)
  {
  softc_t *sc = dev_to_hdlc(net_dev)->priv;
  int received;

  /* Yes, we do NAPI. */
  /* Allow processing up to net_dev->quota incoming packets. */
  /* This is the ONLY time core_interrupt() may process rx pkts. */
  /* Otherwise (sc->quota == 0) and rxintr_cleanup() is a NOOP. */
  sc->quota = net_dev->quota;

  /* Handle the card interrupt with kernel ints enabled. */
  /* Process rx pkts (and tx pkts, too). */
  /* Card interrupts are disabled. */
  core_interrupt(sc, 0);

  /* Report number of rx packets processed. */
  received = net_dev->quota - sc->quota;
  net_dev->quota -= received;
  *budget        -= received;

  /* if quota prevented processing all rx pkts, leave rx ints disabled */
  if (sc->quota == 0)  /* this is off by one...but harmless */
    {
    WRITE_CSR(TLP_INT_ENBL, TLP_INT_TX);
    return 1; /* more pkts to handle -- reschedule */
    }

  sc->quota = 0;  /* disable rx pkt processing by rxintr_cleanup() */
  netif_rx_complete(net_dev); /* NAPI - remove from poll list */

  /* Enable card interrupts. */
  WRITE_CSR(TLP_INT_ENBL, TLP_INT_TXRX);
  return 0;
  }

/* These next routines are similar to BSD's ifnet kernel/driver interface. */

/* This net_device method hands outgoing packets to the transmitter. */
/* With txintr_setup(), it implements output flow control. */
/* Called from a syscall (user context; no spinlocks). */
static int
linux_start(struct sk_buff *skb, struct net_device *net_dev)
  {
  softc_t *sc = dev_to_hdlc(net_dev)->priv;

  if (sc->tx_skb == NULL)
    {
    /* Put this skb where the transmitter will see it. */
    sc->tx_skb = skb;

    /* Start the transmitter; incoming pkts are NOT processed. */
    user_interrupt(sc, 0);

    /* If the tx didn't take the skb then stop the queue. */
    /* This can happen if another CPU is in core_interrupt(). */
    if (sc->tx_skb != NULL) netif_stop_queue(net_dev);

    return 0;
    }

  /* This shouldn't happen; skb is NOT consumed. */
  if (netif_queue_stopped(net_dev))
    printk("%s: dev->start() called with queue stopped\n", NAME_UNIT);
  else
    netif_stop_queue(net_dev);

  return 1;
  }

/* This net_device method restarts the transmitter if it hangs. */
/* Called from a softirq. */
static void
linux_timeout(struct net_device *net_dev)
  {
  softc_t *sc = dev_to_hdlc(net_dev)->priv;

  /* Start the transmitter; incoming packets are NOT processed. */
  user_interrupt(sc, 1);
  }

/* This net_device method handles IOCTL syscalls. */
/* Called from a syscall (user context; no spinlocks; can sleep). */
static int
linux_ioctl(struct net_device *net_dev, struct ifreq *ifr, int cmd)
  {
  softc_t *sc = dev_to_hdlc(net_dev)->priv;
  int error = 0;

  if ((cmd >= SIOCDEVPRIVATE) && (cmd <= SIOCDEVPRIVATE+15))
    {
    struct iohdr *iohdr = (struct iohdr *)ifr;
    u_int16_t direction = iohdr->direction;
    u_int16_t length = iohdr->length;
    char *user_addr = (char *)iohdr->iohdr;
    char *kern_addr;

    if (iohdr->cookie != NGM_LMC_COOKIE) return -EINVAL;

    /* Emulate a BSD-style IOCTL syscall. */
    kern_addr = kmalloc(length, GFP_KERNEL);
    if (kern_addr == NULL)
      error = -ENOMEM;
    if ((error == 0) && ((direction & DIR_IOW) != 0))
      error = copy_from_user(kern_addr, user_addr, length);
    if (error == 0)
      error = -core_ioctl(sc, (unsigned long)cmd, kern_addr);
    if ((error == 0) && ((direction & DIR_IOR) != 0))
      error = copy_to_user(user_addr, kern_addr, length);
    kfree(kern_addr);
    }
# if GEN_HDLC
  else if (cmd == SIOCWANDEV)
    {
    const size_t size = sizeof(sync_serial_settings);

    switch (ifr->ifr_settings.type)
      {
      case IF_GET_IFACE: /* get interface config */
        {
        ifr->ifr_settings.type = IF_IFACE_SYNC_SERIAL;
        if (ifr->ifr_settings.size < size)
          {
          ifr->ifr_settings.size = size;
          error = -ENOBUFS;
	  }
        else
          {
          if (sc->config.tx_clk_src == CFG_CLKMUX_ST)
            sc->hdlc_settings.clock_type = CLOCK_EXT;
          if (sc->config.tx_clk_src == CFG_CLKMUX_INT)
            sc->hdlc_settings.clock_type = CLOCK_TXINT;
          if (sc->config.tx_clk_src == CFG_CLKMUX_RT)
            sc->hdlc_settings.clock_type = CLOCK_TXFROMRX;
          sc->hdlc_settings.loopback = (sc->config.loop_back != CFG_LOOP_NONE) ? 1:0;
          sc->hdlc_settings.clock_rate = sc->status.tx_speed;
          error = copy_to_user(ifr->ifr_settings.ifs_ifsu.sync,
           &sc->hdlc_settings, size);
	  }
        break;
	}
      case IF_IFACE_SYNC_SERIAL: /* set interface config */
        {
        if (!capable(CAP_NET_ADMIN))
          error = -EPERM;
        if (error == 0) 
          error = copy_from_user(&sc->hdlc_settings,
          ifr->ifr_settings.ifs_ifsu.sync, size);
        /* hdlc_settings are currently ignored. */
        break;
	}
      default:  /* Pass the rest to the line protocol code. */
        {
        error = hdlc_ioctl(net_dev, ifr, cmd);
        break;
	}
      }
    }
# endif /* GEN_HDLC */
  else /* unknown IOCTL command */
    error = -EINVAL;

  if (DRIVER_DEBUG)
    printk("%s: linux_ioctl; cmd=0x%08x error=%d\n",
     NAME_UNIT, cmd, error);

  return error;
  }

/* This net_device method returns a pointer to device statistics. */
static struct net_device_stats *
linux_stats(struct net_device *net_dev)
  {
# if GEN_HDLC
  return &dev_to_hdlc(net_dev)->stats;
# else
  softc_t *sc = net_dev->priv;
  return &sc->net_stats;
# endif
  }

/* Called from a softirq once a second. */
static void
linux_watchdog(unsigned long softc)
  {
  softc_t *sc = (softc_t *)softc;
  u_int8_t old_oper_status = sc->status.oper_status;
  struct event_cntrs *cntrs = &sc->status.cntrs;
  struct net_device_stats *stats = linux_stats(sc->net_dev);

  core_watchdog(sc); /* updates oper_status */

  /* Notice change in link status. */
  if     ((old_oper_status != STATUS_UP) &&
   (sc->status.oper_status == STATUS_UP))  /* link came up */
    {
    hdlc_set_carrier(1, sc->net_dev);
    netif_wake_queue(sc->net_dev);
    }
  if     ((old_oper_status == STATUS_UP) &&
   (sc->status.oper_status != STATUS_UP))  /* link went down */
    {
    hdlc_set_carrier(0, sc->net_dev);
    netif_stop_queue(sc->net_dev);
    }

  /* Notice change in line protocol. */
  if (sc->config.line_pkg == PKG_RAWIP)
    {
    sc->status.line_pkg  = PKG_RAWIP;
    sc->status.line_prot = PROT_IP_HDLC;
    }
# if GEN_HDLC
  else
    {
    sc->status.line_pkg  = PKG_GEN_HDLC;
    switch (sc->hdlc_dev->proto.id)
      {
      case IF_PROTO_PPP:
        sc->status.line_prot = PROT_PPP;
        break;
      case IF_PROTO_CISCO:
        sc->status.line_prot = PROT_C_HDLC;
        break;
      case IF_PROTO_FR:
        sc->status.line_prot = PROT_FRM_RLY;
        break;
      case IF_PROTO_HDLC:
        sc->status.line_prot = PROT_IP_HDLC;
        break;
      case IF_PROTO_X25:
        sc->status.line_prot = PROT_X25;
        break;
      case IF_PROTO_HDLC_ETH:
        sc->status.line_prot = PROT_ETH_HDLC;
        break;
      default:
        sc->status.line_prot = 0;
        break;
      }
    }
# endif /* GEN_HDLC */

  /* Copy statistics from sc to net_dev for get_stats(). */
  stats->rx_packets       = cntrs->ipackets;
  stats->tx_packets       = cntrs->opackets;
  stats->rx_bytes         = cntrs->ibytes;
  stats->tx_bytes         = cntrs->obytes;
  stats->rx_errors        = cntrs->ierrors;
  stats->tx_errors        = cntrs->oerrors;
  stats->rx_dropped       = cntrs->idiscards;
  stats->tx_dropped       = cntrs->odiscards;
  stats->rx_fifo_errors   = cntrs->fifo_over;
  stats->tx_fifo_errors   = cntrs->fifo_under;
  stats->rx_missed_errors = cntrs->missed;
  stats->rx_over_errors   = cntrs->overruns;

  /* Call this procedure again after one second. */
  sc->wd_timer.expires = jiffies + HZ; /* now plus one second */
  add_timer(&sc->wd_timer);
  }

/* This is the I/O configuration interface for Linux. */

/* This net_device method is called when IFF_UP goes false. */
static int
linux_stop(struct net_device *net_dev)
  {
  softc_t *sc = dev_to_hdlc(net_dev)->priv;

  /* Stop the card and detach from the kernel. */
  detach_card(sc);  /* doesn't fail */

  free_irq(net_dev->irq, net_dev); /* doesn't fail */

  del_timer(&sc->wd_timer); /* return value ignored */

  return 0;
  }

/* This net_device method is called when IFF_UP goes true. */
static int
linux_open(struct net_device *net_dev)
  {
  softc_t *sc = dev_to_hdlc(net_dev)->priv;
  int error;

  /* Allocate PCI interrupt resources for the card. */
  if ((error = request_irq(net_dev->irq, &linux_interrupt, SA_SHIRQ,
   NAME_UNIT, net_dev)))
    {
    printk("%s: request_irq() failed; error %d\n", NAME_UNIT, error);
    return error;
    }

  /* Arrange to call linux_watchdog() once a second. */
  init_timer(&sc->wd_timer);
  sc->wd_timer.expires  = jiffies + HZ; /* now plus one second */
  sc->wd_timer.function = &linux_watchdog;
  sc->wd_timer.data     = (unsigned long) sc;
  add_timer(&sc->wd_timer);

  /* Start the card and attach a kernel interface and line protocol. */
  if ((error = -attach_card(sc, "")))
    linux_stop(net_dev);
  else
    {
    net_dev->weight = sc->rxring.num_descs; /* input flow control */
    netif_start_queue(net_dev);            /* output flow control */
    }

  return error;
  }

# if GEN_HDLC
static int
hdlc_attach(struct net_device *net_dev,
 unsigned short encoding, unsigned short parity)
  { return 0; }
# endif

/* This pci_driver method is called during shutdown or module-unload. */
/* This is called from user context; can sleep; no spinlocks! */
static void __exit
linux_remove(struct pci_dev *pci_dev)
  {
  struct net_device *net_dev = (struct net_device *)pci_get_drvdata(pci_dev);
  softc_t *sc = dev_to_hdlc(net_dev)->priv;

  if (net_dev == NULL) return;

  /* Assume that linux_stop() has already been called. */
  if (sc->flags & FLAG_NETDEV)
# if GEN_HDLC
    unregister_hdlc_device(net_dev);
# else
    unregister_netdev(net_dev);
# endif

# if (IOREF_CSR == 0)
  if (sc->csr_membase != NULL)
    iounmap(sc->csr_membase);
# endif

  pci_disable_device(pci_dev);

  if (sc->csr_iobase != 0)
    pci_release_regions(pci_dev);

  pci_set_drvdata(pci_dev, NULL);

  kfree(sc);
  free_netdev(net_dev);
  }

static void
setup_netdev(struct net_device *net_dev)
  {
  /* Initialize the generic network device. */
  /* Note similarity to BSD's ifnet_attach(). */
  net_dev->flags           = IFF_POINTOPOINT;
  net_dev->flags          |= IFF_RUNNING;
  net_dev->open            = linux_open;
  net_dev->stop            = linux_stop;
  net_dev->hard_start_xmit = linux_start;
  net_dev->do_ioctl        = linux_ioctl;
  net_dev->get_stats       = linux_stats;
  net_dev->tx_timeout      = linux_timeout;
  net_dev->poll            = linux_poll;
  net_dev->watchdog_timeo  = 1 * HZ;
  net_dev->tx_queue_len    = SNDQ_MAXLEN;
  net_dev->mtu             = MAX_DESC_LEN;
  net_dev->type            = ARPHRD_RAWHDLC;
/* The receiver generates frag-lists for packets >4032 bytes.   */
/* The transmitter accepts scatter/gather lists and frag-lists. */
/* However Linux linearizes outgoing packets since our hardware */
/*  doesn't compute soft checksums.  All that work for nothing! */
/*net_dev->features       |= NETIF_F_SG; */
/*net_dev->features       |= NETIF_F_FRAGLIST; */
  }

/* This pci_driver method is called during boot or module-load. */
/* This is called from user context; can sleep; no spinlocks! */
static int __init
linux_probe(struct pci_dev *pci_dev, const struct pci_device_id *id)
  {
  u_int32_t cfid, csid;
  struct net_device *net_dev;
  softc_t *sc;
  int error;

  /* Looking for a DEC 21140A chip on any Lan Media Corp card. */
  pci_read_config_dword(pci_dev, TLP_CFID, &cfid);
  if (cfid != TLP_CFID_TULIP) return -ENXIO;
  pci_read_config_dword(pci_dev, TLP_CSID, &csid);
  switch (csid)
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
    case TLP_CSID_T3:
    case TLP_CSID_SSI:
    case TLP_CSID_T1E1:
      break;
    default:
      return -ENXIO;
    }

  /* Declare that these cards use 32-bit single-address PCI cycles. */
  if ((error = pci_set_dma_mask(pci_dev, DMA_32BIT_MASK)))
    {
    printk("%s: pci_set_dma_mask() failed; error %d\n", DEVICE_NAME, error);
    return error;
    }
  pci_set_consistent_dma_mask(pci_dev, DMA_32BIT_MASK); /* can't fail */

# if GEN_HDLC /* generic-hdlc line protocols */

  /* device driver instance data, aka Soft Context or sc */
  if ((sc = kmalloc(sizeof(softc_t), GFP_KERNEL)) == NULL)
    {
    printk("%s: kmalloc() failed\n", DEVICE_NAME);
    return -ENOMEM;
    }
  memset(sc, 0, sizeof(softc_t));

  /* Allocate space for the HDLC network device struct. */
  if ((net_dev = alloc_hdlcdev(sc)) == NULL)
    {
    printk("%s: alloc_hdlcdev() failed\n", DEVICE_NAME);
    kfree(sc);
    return -ENOMEM;
    }

  /* Initialize the network device struct. */
  setup_netdev(net_dev);

  /* Initialize the HDLC extension to the network device. */
  sc->hdlc_dev         = dev_to_hdlc(net_dev);
  sc->hdlc_dev->attach = hdlc_attach; /* noop for this driver */
  sc->hdlc_dev->xmit   = linux_start; /* the REAL hard_start_xmit() */

# else /* GEN_HDLC */ /* no line protocol. */

  /* Allocate space for the bare network device struct. */
  net_dev = alloc_netdev(sizeof(softc_t), DEVICE_NAME"%d", setup_netdev);
  if (net_dev == NULL)
    {
    printk("%s: alloc_netdev() failed\n", DEVICE_NAME);
    return -ENOMEM;
    }
  /* device driver instance data, aka Soft Context or sc */
  sc = net_dev->priv;

# endif /* GEN_HDLC */

  sc->net_dev = net_dev;  /* NAME_UNIT macro needs this */
  sc->pci_dev = pci_dev;  /* READ/WRITE_PCI_CFG macros need this */

  /* Cross-link pci_dev and net_dev. */
  pci_set_drvdata(pci_dev, net_dev);      /* pci_dev->driver_data = net_dev */
  SET_NETDEV_DEV(net_dev, &pci_dev->dev); /* net_dev->class_dev.dev = &pci_dev->dev */
  SET_MODULE_OWNER(net_dev);              /* ??? NOOP in linux-2.6.3. ??? */

  /* Sets cfcs.io and cfcs.mem; sets pci_dev->irq based on cfit.int */
  if ((error = pci_enable_device(pci_dev)))
    {
    printk("%s: pci_enable_device() failed; error %d\n", DEVICE_NAME, error);
    linux_remove(pci_dev);
    return error;
    }
  net_dev->irq = pci_dev->irq; /* linux_open/stop need this */

  /* Allocate PCI memory and IO resources to access the Tulip chip CSRs. */
  if ((error = pci_request_regions(pci_dev, DEVICE_NAME)))
    {
    printk("%s: pci_request_regions() failed; error %d\n", DEVICE_NAME, error);
    linux_remove(pci_dev);
    return error;
    }
  net_dev->base_addr = pci_resource_start(pci_dev, 0);
  net_dev->mem_start = pci_resource_start(pci_dev, 1);
  net_dev->mem_end   = pci_resource_end(pci_dev, 1);
  sc->csr_iobase     = net_dev->base_addr;

# if (IOREF_CSR == 0)
  sc->csr_membase = ioremap_nocache(net_dev->mem_start, TLP_CSR_SIZE);
  if (sc->csr_membase == NULL)
    {
    printk("%s: ioremap_nocache() failed\n", DEVICE_NAME);
    linux_remove(pci_dev);
    return -EFAULT;
    }
# endif

  /* Sets cfcs.master, enabling PCI DMA; checks latency timer value. */
  pci_set_master(pci_dev); /* Later, attach_card() does this too. */

  /* Initialize the top-half and bottom-half locks. */
  /* Top_lock must be initialized before net_dev is registered. */
  init_MUTEX(&sc->top_lock);
  spin_lock_init(&sc->bottom_lock);

# if GEN_HDLC
  if ((error = register_hdlc_device(net_dev)))
    {
    printk("%s: register_hdlc_device() failed; error %d\n", DEVICE_NAME, error);
    linux_remove(pci_dev);
    return error;
    }
# else
  if ((error = register_netdev(net_dev)))
    {
    printk("%s: register_netdev() failed; error %d\n", DEVICE_NAME, error);
    linux_remove(pci_dev);
    return error;
    }
# endif
  /* The NAME_UNIT macro now works.  Use DEVICE_NAME before this. */
  sc->flags |= FLAG_NETDEV;

  /* What kind of card are we driving? */
  switch (READ_PCI_CFG(sc, TLP_CSID))
    {
    case TLP_CSID_HSSI:
    case TLP_CSID_HSSIc:
      sc->dev_desc =  HSSI_DESC;
      sc->card     = &hssi_card;
      break;
    case TLP_CSID_T3:
      sc->dev_desc =    T3_DESC;
      sc->card     =   &t3_card;
      break;
    case TLP_CSID_SSI:
      sc->dev_desc =   SSI_DESC;
      sc->card     =  &ssi_card;
      break;
    case TLP_CSID_T1E1:
      sc->dev_desc =  T1E1_DESC;
      sc->card     =   &t1_card;
      break;
    default: /* shouldn't happen! */
      linux_remove(pci_dev);
      return -ENXIO;
    }

  /* Announce the hardware on the console. */
  printk("%s: <%s> io 0x%04lx/9 mem 0x%08lx/25 rom 0x%08lx/14 irq %d pci %s\n",
   NAME_UNIT, sc->dev_desc, pci_resource_start(pci_dev, 0),
   pci_resource_start(pci_dev, 1), pci_resource_start(pci_dev, 6),
   pci_dev->irq, pci_name(pci_dev));

  return 0;
  }

/* This pci driver knows how to drive these devices: */
static __initdata struct pci_device_id pci_device_id_tbl[] =
  {
  /* Looking for a DEC 21140A chip on any Lan Media Corp card. */
    { 0x1011, 0x0009, 0x1376, PCI_ANY_ID, 0, 0, 0 },
    {      0,      0,      0,          0, 0, 0, 0 }
  };
MODULE_DEVICE_TABLE(pci, pci_device_id_tbl);

static struct pci_driver pci_driver =
  {
  .name	    = DEVICE_NAME,
  .id_table = pci_device_id_tbl,
  .probe    = linux_probe,
  .remove   = __devexit_p(linux_remove),
  /* This driver does not suspend and resume. */
  };

/* This ultimately calls our pci_driver.probe() method. */
static int  __init linux_modload(void)
  { return pci_module_init(&pci_driver); }
module_init(linux_modload);

/* This ultimately calls our pci_driver.remove() method. */
static void __exit linux_modunload(void)
  { pci_unregister_driver(&pci_driver); }
module_exit(linux_modunload);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Device driver for SBE/LMC Wide-Area Network cards");
MODULE_AUTHOR("David Boggs <boggs@boggs.palo-alto.ca.us>");

#endif /* __linux__ */
