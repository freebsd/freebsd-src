/*-
 * Copyright (c) 2005-2008 Olivier Houchard.  All rights reserved.
 * Copyright (c) 2005-2012 Warner Losh.  All rights reserved.
 * Copyright (c) 2007-2014 Ian Lepore.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Board init code for the TSC4370, and all other current TSC mainboards.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/systm.h>

#include <arm/at91/at91_pioreg.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91_pmcreg.h>
#include <arm/at91/at91_pmcvar.h>
#include <arm/at91/at91_twireg.h>
#include <arm/at91/at91_usartreg.h>
#include <arm/at91/at91board.h>
#include <arm/at91/at91var.h>
#include <arm/at91/at91rm9200var.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/if_atereg.h>
#include <machine/board.h>
#include <machine/cpu.h>
#include <machine/machdep.h>
#include <net/ethernet.h>
#include <sys/reboot.h>

/*
 * RD4HW()/WR4HW() read and write at91rm9200 hardware register space directly.
 * They serve the same purpose as the RD4()/WR4() idiom you see in many drivers,
 * except that those translate to bus_space calls, but in this code we need to
 * access the registers directly before the at91 bus_space stuff is set up.
 */

static inline uint32_t 
RD4HW(uint32_t devbase, uint32_t regoff)
{
	return *(volatile uint32_t *)(AT91_BASE + devbase + regoff);
}

static inline void
WR4HW(uint32_t devbase, uint32_t regoff, uint32_t val)
{
	*(volatile uint32_t *)(AT91_BASE + devbase + regoff) = val;
}

/*
 * This is the same calculation the at91 uart driver does, we use it to update
 * the console uart baud rate after changing the MCK rate.
 */
#ifndef BAUD2DIVISOR
#define BAUD2DIVISOR(b) \
	((((at91_master_clock * 10) / ((b) * 16)) + 5) / 10)
#endif

/*
 * If doing an in-house build, use tsc_bootinfo.h which is shared with our
 * custom boot2.  Otherwise define some crucial bits of it here, enough to allow
 * this code to compile.
 */
#ifdef TSC_BUILD
#include <machine/tsc_bootinfo.h>
#else
struct tsc_bootinfo {
	uint32_t	bi_size;
	uint32_t	bi_version;
	uint32_t	bi_flags; /* RB_xxxxx flags from sys/reboot.h */
	char	bi_rootdevname[64];
};
#define TSC_BOOTINFO_MAGIC	0x06C30000 
#endif

static struct arm_boot_params	boot_params;
static struct tsc_bootinfo	inkernel_bootinfo;

/*
 * Change the master clock config and wait for it to stabilize.
 */
static void
change_mckr(uint32_t mckr)
{
	int i;

	WR4HW(AT91RM92_PMC_BASE, PMC_MCKR, mckr);

	for (i = 0; i < 1000; ++i)
		if ((RD4HW(AT91RM92_PMC_BASE, PMC_SR) & PMC_IER_MCKRDY))
			return;
}

/*
 * Allow the master clock frequency to be changed from whatever the bootloader
 * set up, because sometimes it's harder to change/update a bootloader than it
 * is to change/update the kernel once a product is in the field.
 */
static void
master_clock_init(void)
{
	uint32_t mckr = RD4HW(AT91RM92_PMC_BASE, PMC_MCKR);
	int hintvalue = 0;
	int newmckr = 0;

	 /*
	 * If there's a hint that specifies the contents of MCKR, use it
	 * without question (it had better be right).
	 *
	 * If there's a "mckfreq" hint it might be in hertz or mhz (convert the
	 * latter to hz).  Calculate the new MCK divider.  If the CPU frequency
	 * is not a sane multiple of the hinted MCK frequency this is likely to
	 * behave badly.  The moral is: don't hint at impossibilities.
	 */

	if (resource_int_value("at91", 0, "mckr", &hintvalue) == 0) {
		newmckr = hintvalue;
	} else {
		hintvalue = 90; /* Default to 90mhz if not specified. */
		resource_int_value("at91", 0, "mckfreq", &hintvalue);
		if (hintvalue != 0) {
			if (hintvalue < 1000)
				hintvalue *= 1000000;
			if (hintvalue != at91_master_clock) {
				uint32_t divider;
				struct at91_pmc_clock * cpuclk;
				cpuclk = at91_pmc_clock_ref("cpu");
				divider = (cpuclk->hz / hintvalue) - 1;
				newmckr = (mckr & 0xFFFFFCFF) | ((divider & 0x03) << 8);
				at91_pmc_clock_deref(cpuclk);
			}
		}
	}

	/* If the new mckr value is different than what's in the register now,
	 * make the change and wait for the clocks to settle (MCKRDY status).
	 *
	 * MCKRDY will never be asserted unless either the selected clock or the
	 * prescaler value changes (but not both at once) [this is detailed in
	 * the rm9200 errata]. This code assumes the prescaler value is always
	 * zero and that by time we get to here we're running on something other
	 * than the slow clock, so to change the mckr divider we first change
	 * back to the slow clock (keeping prescaler and divider unchanged),
	 * then go back to the original selected clock with the new divider.
	 *
	 * After changing MCK, go re-init everything clock-related, and reset
	 * the baud rate generator for the console (doing this here is kind of a
	 * rude hack, but hey, you do what you have to to run MCK faster).
	 */

	if (newmckr != 0 && newmckr != mckr) {
		if (mckr & 0x03)
			change_mckr(mckr & ~0x03);
		change_mckr(newmckr);
		at91_pmc_init_clock();
		WR4HW(AT91RM92_DBGU_BASE, USART_BRGR, BAUD2DIVISOR(115200));
	}
}

/*
 * TSC-specific code to read the ID eeprom on the mainboard and extract the
 * unit's EUI-64 which gets translated into a MAC-48 for ethernet.
 */
static void
eeprom_init(void)
{
	const uint32_t twiHz    = 400000;
	const uint32_t twiCkDiv = 1 << 16;
	const uint32_t twiChDiv = ((at91_master_clock / twiHz) - 2) << 8;
	const uint32_t twiClDiv = ((at91_master_clock / twiHz) - 2);

	/*
	 * Set the TWCK and TWD lines for Periph A, no pullup, open-drain.
	 */
	at91_pio_use_periph_a(AT91RM92_PIOA_BASE,
	    AT91C_PIO_PA25 | AT91C_PIO_PA26, 0);
	at91_pio_gpio_high_z(AT91RM92_PIOA_BASE, AT91C_PIO_PA25, 1);

	/*
	 * Enable TWI power (irq numbers are also device IDs for power)
	 */
	WR4HW(AT91RM92_PMC_BASE, PMC_PCER, 1u << AT91RM92_IRQ_TWI);

	/*
	 * Disable TWI interrupts, reset device, enable Master mode,
	 * disable Slave mode, set the clock.
	 */
	WR4HW(AT91RM92_TWI_BASE, TWI_IDR, 0xffffffff);
	WR4HW(AT91RM92_TWI_BASE, TWI_CR, TWI_CR_SWRST);
	WR4HW(AT91RM92_TWI_BASE, TWI_CR, TWI_CR_MSEN | TWI_CR_SVDIS);
	WR4HW(AT91RM92_TWI_BASE, TWI_CWGR, twiCkDiv | twiChDiv | twiClDiv);
}

static int
eeprom_read(uint32_t EE_DEV_ADDR, uint32_t ee_off, void * buf, uint32_t size)
{
	uint8_t *bufptr = (uint8_t *)buf;
	uint32_t status;
	uint32_t count;

	/* Clean out any old status and received byte. */
	status = RD4HW(AT91RM92_TWI_BASE, TWI_SR);
	status = RD4HW(AT91RM92_TWI_BASE, TWI_RHR);

	/* Set the TWI Master Mode Register */
	WR4HW(AT91RM92_TWI_BASE, TWI_MMR,
	    TWI_MMR_DADR(EE_DEV_ADDR) | TWI_MMR_IADRSZ(2) | TWI_MMR_MREAD);

	/* Set TWI Internal Address Register */
	WR4HW(AT91RM92_TWI_BASE, TWI_IADR, ee_off);

	/* Start transfer */
	WR4HW(AT91RM92_TWI_BASE, TWI_CR, TWI_CR_START);

	status = RD4HW(AT91RM92_TWI_BASE, TWI_SR);

	while (size-- > 1){
		/* Wait until Receive Holding Register is full */
		count = 1000000;
		while (!(RD4HW(AT91RM92_TWI_BASE, TWI_SR) & TWI_SR_RXRDY) && 
		    --count != 0)
			continue;
		if (count <= 0)
			return -1;
		/* Read and store byte */
		*bufptr++ = (uint8_t)RD4HW(AT91RM92_TWI_BASE, TWI_RHR);
	}
	WR4HW(AT91RM92_TWI_BASE, TWI_CR, TWI_CR_STOP);

	status = RD4HW(AT91RM92_TWI_BASE, TWI_SR);

	/* Wait until transfer is finished */
	while (!(RD4HW(AT91RM92_TWI_BASE, TWI_SR) & TWI_SR_TXCOMP))
		continue;

	/* Read last byte */
	*bufptr = (uint8_t)RD4HW(AT91RM92_TWI_BASE, TWI_RHR);

	return 0;
}

static int
set_mac_from_idprom(void)
{
#define SIGNATURE_SIZE          4
#define EETYPE_SIZE             2
#define BSLENGTH_SIZE           2
#define RAW_SIZE                52
#define EUI64_SIZE              8
#define BS_SIGNATURE            0x21706d69
#define BSO_SIGNATURE           0x216f7362
#define DEVOFFSET_BSO_SIGNATURE 0x20
#define OFFSET_BS_SIGNATURE     0
#define SIZE_BS_SIGNATURE       SIGNATURE_SIZE
#define OFFSET_EETYPE           (OFFSET_BS_SIGNATURE + SIZE_BS_SIGNATURE)
#define SIZE_EETYPE             EETYPE_SIZE
#define OFFSET_BOOTSECTSIZE     (OFFSET_EETYPE + SIZE_EETYPE)
#define SIZE_BOOTSECTSIZE       BSLENGTH_SIZE
#define OFFSET_RAW              (OFFSET_BOOTSECTSIZE + SIZE_BOOTSECTSIZE)
#define OFFSET_EUI64            (OFFSET_RAW + RAW_SIZE)
#define EE_DEV_ADDR             0xA0    /* eeprom is AT24C256 at address 0xA0 */

	int status;
	uint32_t dev_offset = 0;
	uint32_t sig;
	uint8_t eui64[EUI64_SIZE];
	uint8_t eaddr[ETHER_ADDR_LEN];

	eeprom_init();

	/* Check for the boot section signature at offset 0. */
	status = eeprom_read(EE_DEV_ADDR, OFFSET_BS_SIGNATURE, &sig, sizeof(sig));
	if (status == -1)
		return -1;

	if (sig != BS_SIGNATURE) {
		/* Check for the boot section offset signature. */
		status = eeprom_read(EE_DEV_ADDR, 
		    DEVOFFSET_BSO_SIGNATURE, &sig, sizeof(sig));
		if ((status == -1) || (sig != BSO_SIGNATURE))
				return -1;

		/* Read the device offset of the boot section structure. */
		status = eeprom_read(EE_DEV_ADDR, 
		    DEVOFFSET_BSO_SIGNATURE + sizeof(sig), 
		    &dev_offset, sizeof(dev_offset));
		if (status == -1)
				return -1;

		/* Check for the boot section signature. */
		status = eeprom_read(EE_DEV_ADDR, 
		    dev_offset + OFFSET_BS_SIGNATURE, &sig, sizeof(sig));
		if ((status == -1) || (sig != BS_SIGNATURE))
				return -1;
	}
	dev_offset += OFFSET_EUI64;

	/* Read the EUI64 from the device.  */
	if (eeprom_read(EE_DEV_ADDR, dev_offset, eui64, sizeof(eui64)) == -1)
		return -1;

	/* Transcribe the EUI-64 to a MAC-48.
	 *
	 * Given an EUI-64 of aa:bb:cc:dd:ee:ff:gg:hh
	 *
	 *   if (ff is zero and ee is non-zero)
	 *      mac is aa:bb:cc:ee:gg:hh
	 *   else
	 *      mac is aa:bb:cc:ff:gg:hh
	 *
	 * This logic fixes a glitch in our mfg process in which the ff byte was
	 * always zero and the ee byte contained a non-zero value.  This
	 * resulted in duplicate MAC addresses because we discarded the ee byte.
	 * Now they've fixed the process so that the ff byte is non-zero and
	 * unique addresses are formed from the ff:gg:hh bytes.  If the ff byte
	 * is zero, then we have a unit manufactured during the glitch era, and
	 * we fix the problem by grabbing the ee byte rather than the ff byte.
	 */
	eaddr[0] = eui64[0];
	eaddr[1] = eui64[1];
	eaddr[2] = eui64[2];
	eaddr[3] = eui64[5];
	eaddr[4] = eui64[6];
	eaddr[5] = eui64[7];

	if (eui64[5] == 0 && eui64[4] != 0) {
		eaddr[3] = eui64[4];
	}

	/*
	 * Set the address in the hardware regs where the ate driver
	 * looks for it.
	 */
	WR4HW(AT91RM92_EMAC_BASE, ETH_SA1L, 
	    (eaddr[3] << 24) | (eaddr[2] << 16) | (eaddr[1] << 8) | eaddr[0]);
	WR4HW(AT91RM92_EMAC_BASE, ETH_SA1H, 
	    (eaddr[5] << 8) | (eaddr[4]));

	printf(
	    "ID: EUI-64 %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n"
	    "    MAC-48 %02x:%02x:%02x:%02x:%02x:%02x\n"
	    "    read from i2c device 0x%02X offset 0x%x\n",
	    eui64[0], eui64[1], eui64[2], eui64[3], 
	    eui64[4], eui64[5], eui64[6], eui64[7], 
	    eaddr[0], eaddr[1], eaddr[2], 
	    eaddr[3], eaddr[4], eaddr[5], 
	    EE_DEV_ADDR, dev_offset);

	return (0);
}

/*
 * Assign SPI chip select pins based on which chip selects are found in hints.
 */
static void
assign_spi_pins(void)
{
	struct {
		uint32_t     num;
		const char * name;
	} chipsel_pins[] = {
		{ AT91C_PIO_PA3, "PA3", },
		{ AT91C_PIO_PA4, "PA4", },
		{ AT91C_PIO_PA5, "PA5", },
		{ AT91C_PIO_PA6, "PA6", },
	};
	int anchor = 0;
	uint32_t chipsel_inuse = 0;

	/*
	 * Search through all device hints looking for any that have
	 * ".at=spibus0".  For each one found, ensure that there is also a
	 * chip select hint ".cs=<num>" and that <num> is 0-3, and assign the
	 * corresponding pin to the SPI peripheral.  Whine if we find a SPI
	 * device with a missing or invalid chipsel hint.
	 */
	for (;;) {
		const char * rName = "";
		int unit = 0;
		int cs = 0;
		int ret;

		ret = resource_find_match(&anchor, &rName, &unit, "at", "spibus0");
		if (ret != 0)
			break;

		ret = resource_int_value(rName, unit, "cs", &cs);
		if (ret != 0) {
			printf( "Error: hint for SPI device %s%d "
				"without a chip select hint; "
				"device will not function.\n",
				rName, unit);
			continue;
		}
		if (cs < 0 || cs > 3) {
			printf( "Error: hint for SPI device %s%d "
				"contains an invalid chip select "
				"value: %d\n",
				rName, unit, cs);
			continue;
		}
		if (chipsel_inuse & (1 << cs)) {
			printf( "Error: hint for SPI device %s%d "
				"specifies chip select %d, which "
				"is already used by another device\n",
				rName, unit, cs);
			continue;
		}
		chipsel_inuse |= 1 << cs;
		at91_pio_use_periph_a(AT91RM92_PIOA_BASE, 
			chipsel_pins[cs].num, 1);
		printf( "Configured pin %s as SPI chip "
			"select %d for %s%d\n",
			chipsel_pins[cs].name, cs, rName, unit);
	}

	/*
	 * If there were hints for any SPI devices, assign the basic SPI IO pins
	 * and enable SPI power (irq numbers are also device IDs for power).
	 */
	if (chipsel_inuse != 0) {
		at91_pio_use_periph_a(AT91RM92_PIOA_BASE, 
			AT91C_PIO_PA1 | AT91C_PIO_PA0 | AT91C_PIO_PA2, 0);
		WR4HW(AT91RM92_PMC_BASE, PMC_PCER, 1u << AT91RM92_IRQ_SPI);
	}
}

BOARD_INIT long
board_init(void)
{
	int is_bga, rev_mii;

	/*
	 * Deal with bootinfo (if any) passed in from the boot2 bootloader and
	 * copied to the static inkernel_bootinfo earlier in the init.  Do this
	 * early so that bootverbose is set from this point on.
	 */
	if (inkernel_bootinfo.bi_size > 0 && 
	    (inkernel_bootinfo.bi_flags & RB_BOOTINFO)) {
		struct tsc_bootinfo *bip = &inkernel_bootinfo;
		printf("TSC_BOOTINFO: size %u howtoflags=0x%08x rootdev='%s'\n", 
		    bip->bi_size, bip->bi_flags, bip->bi_rootdevname);
		boothowto = bip->bi_flags;
		bootverbose = (boothowto & RB_VERBOSE);
		if (bip->bi_rootdevname[0] != 0)
			rootdevnames[0] = bip->bi_rootdevname;
	}

	/*
	 * The only way to know if we're in a BGA package (and thus have PIOD)
	 * is to be told via a hint; there's nothing detectable in the silicon.
	 * This is esentially an rm92-specific extension to getting the chip ID
	 * (which was done by at91_machdep just before calling this routine).
	 * If it is the BGA package, enable the clock for PIOD.
	 */
	is_bga = 0;
	resource_int_value("at91", 0, "is_bga_package", &is_bga);
	
	if (is_bga)
		WR4HW(AT91RM92_PMC_BASE, PMC_PCER, 1u << AT91RM92_IRQ_PIOD);
	
#if __FreeBSD_version >= 1000000
	at91rm9200_set_subtype(is_bga ? AT91_ST_RM9200_BGA : 
	    AT91_ST_RM9200_PQFP);
#endif

	/*
	 * Go reprogram the MCK frequency based on hints.
	 */
	master_clock_init();

	/* From this point on you can use printf. */

	/*
	 * Configure UARTs.
	 */
	at91rm9200_config_uart(AT91_ID_DBGU, 0, 0);   /* DBGU just Tx and Rx */
	at91rm9200_config_uart(AT91RM9200_ID_USART0, 1, 0);   /* Tx and Rx */
	at91rm9200_config_uart(AT91RM9200_ID_USART1, 2, 0);   /* Tx and Rx */
	at91rm9200_config_uart(AT91RM9200_ID_USART2, 3, 0);   /* Tx and Rx */
	at91rm9200_config_uart(AT91RM9200_ID_USART3, 4, 0);   /* Tx and Rx */

	/*
	 * Configure MCI (sdcard)
	 */
	at91rm9200_config_mci(0);

	/*
	 * Assign the pins needed by the emac device, and power it up. Also,
	 * configure it for RMII operation unless the 'revmii_mode' hint is set,
	 * in which case configure the full set of MII pins.  The revmii_mode
	 * hint is for so-called reverse-MII, used for connections to a Broadcom
	 * 5325E switch on some boards.  Note that order is important here:
	 * configure pins, then power on the device, then access the device's
	 * config registers.
	 */
	rev_mii = 0;
	resource_int_value("ate", 0, "phy_revmii_mode", &rev_mii);

	at91_pio_use_periph_a(AT91RM92_PIOA_BASE, 
		AT91C_PIO_PA7 | AT91C_PIO_PA8 | AT91C_PIO_PA9 |
		AT91C_PIO_PA10 | AT91C_PIO_PA11 | AT91C_PIO_PA12 |
		AT91C_PIO_PA13 | AT91C_PIO_PA14 | AT91C_PIO_PA15 |
		AT91C_PIO_PA16, 0);
	if (rev_mii) {
		at91_pio_use_periph_b(AT91RM92_PIOB_BASE,
		    AT91C_PIO_PB12 | AT91C_PIO_PB13  | AT91C_PIO_PB14 |
		    AT91C_PIO_PB15 | AT91C_PIO_PB16  | AT91C_PIO_PB17 |
		    AT91C_PIO_PB18 | AT91C_PIO_PB19, 0);
	}
	WR4HW(AT91RM92_PMC_BASE, PMC_PCER, 1u << AT91RM92_IRQ_EMAC);
	if (!rev_mii) {
		WR4HW(AT91RM92_EMAC_BASE, ETH_CFG, 
		    RD4HW(AT91RM92_EMAC_BASE, ETH_CFG) | ETH_CFG_RMII);
	}

	/*
	 * Get our ethernet MAC address from the ID eeprom.
	 * Configures TWI as a side effect.
	 */
	set_mac_from_idprom();

	/*
	 * Configure SPI
	 */
	assign_spi_pins();

	/*
	 * Configure SSC
	 */
	at91_pio_use_periph_a(
	    AT91RM92_PIOB_BASE,
	    AT91C_PIO_PB6 | AT91C_PIO_PB7 | AT91C_PIO_PB8 |   /* transmit */
	    AT91C_PIO_PB9 | AT91C_PIO_PB10 | AT91C_PIO_PB11,  /* receive */
	    0);                                               /* no pullup */

	/*
	 *  We're using TC1's A1 input for PPS measurements that drive the
	 *  kernel PLL and our NTP refclock.  On some old boards we route a 5mhz
	 *  signal to TC1's A2 input (pin PA21), but we have never used that
	 *  clock (it rolls over too fast for hz=100), and now newer boards are
	 *  using pin PA21 as a CTS0 for USART1, so we no longer assign it to
	 *  the timer block like we used to here.
	 */
	at91_pio_use_periph_b(AT91RM92_PIOA_BASE, AT91C_PIO_PA19, 0);

	/*
	 * Configure pins used to bitbang-upload the firmware to the main FPGA.
	 */
	at91_pio_use_gpio(AT91RM92_PIOB_BASE,
	    AT91C_PIO_PB16 | AT91C_PIO_PB17 | AT91C_PIO_PB18 | AT91C_PIO_PB19);

	return (at91_ramsize());
}

/*
 * Override the default boot param parser (supplied via weak linkage) with one
 * that knows how to handle our custom tsc_bootinfo passed in from boot2.
 */
vm_offset_t
parse_boot_param(struct arm_boot_params *abp)
{

	boot_params = *abp;

	/*
	 * If the right magic is in r0 and a non-NULL pointer is in r1, then
	 * it's our bootinfo, copy it.  The pointer in r1 is a physical address
	 * passed from boot2.  This routine is called immediately upon entry to
	 * initarm() and is in very nearly the same environment as boot2.  In
	 * particular, va=pa and we can safely copy the args before we lose easy
	 * access to the memory they're stashed in right now.
	 *
	 * Note that all versions of boot2 that we've ever shipped have put
	 * zeroes into r2 and r3.  Maybe that'll be useful some day.
	 */
	if (abp->abp_r0 == TSC_BOOTINFO_MAGIC && abp->abp_r1 != 0) {
		inkernel_bootinfo = *(struct tsc_bootinfo *)(abp->abp_r1);
	}

	return fake_preload_metadata(abp, NULL, 0);
}

ARM_BOARD(NONE, "TSC4370 Controller Board");

