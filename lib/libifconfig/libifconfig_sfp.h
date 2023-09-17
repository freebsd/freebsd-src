/*-
 * Copyright (c) 2014, Alexander V. Chernikov
 * Copyright (c) 2020, Ryan Moeller <freqlabs@FreeBSD.org>
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
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <libifconfig.h>
#include <libifconfig_sfp_tables.h>

/** SFP module information in raw numeric form
 * These are static properties of the hardware.
 */
struct ifconfig_sfp_info;

/** SFP module information formatted as strings
 * These are static strings that do not need to be freed.
 */
struct ifconfig_sfp_info_strings;

#define SFF_VENDOR_STRING_SIZE	16	/**< max chars in a vendor string */
#define SFF_VENDOR_DATE_SIZE	6	/**< chars in a vendor date code */

/** SFP module vendor info strings */
struct ifconfig_sfp_vendor_info {
	char name[SFF_VENDOR_STRING_SIZE + 1];	/**< vendor name */
	char pn[SFF_VENDOR_STRING_SIZE + 1];	/**< vendor part number */
	char sn[SFF_VENDOR_STRING_SIZE + 1];	/**< vendor serial number */
	char date[SFF_VENDOR_DATE_SIZE + 5];	/**< formatted vendor date */
};

/** SFP module status
 * These are dynamic properties of the hardware.
 */
struct ifconfig_sfp_status {
	double temp;		/**< module temperature in degrees C,
				     valid range -40.0 to 125.0 */
	double voltage;		/**< module voltage in volts */
	struct sfp_channel {
		uint16_t rx;	/**< channel receive power, LSB 0.1uW */
		uint16_t tx;	/**< channel transmit bias current, LSB 2uA */
	} *channel;		/**< array of channel rx/tx status */
	uint32_t bitrate;	/**< link bitrate,
				     only present for QSFP modules,
				     zero for SFP modules */
};

#define SFF_DUMP_SIZE	256	/**< size of the memory dump buffer */

#define SFP_DUMP_START	0	/**< start address of an SFP module dump */
#define SFP_DUMP_SIZE	128	/**< bytes in an SFP module dump */

#define QSFP_DUMP0_START	0	/**< start address of the first region
					     in a QSFP module dump */
#define QSFP_DUMP0_SIZE		82	/**< bytes in the first region
					     in a QSFP module dump */
#define QSFP_DUMP1_START	128	/**< start address of the second region
					     in a QSFP module dump */
#define QSFP_DUMP1_SIZE		128	/**< bytes in the second region
					     in a QSFP module dump */

/** SFP module I2C memory dump
 * SFP modules have one region, QSFP modules have two regions.
 */
struct ifconfig_sfp_dump {
	uint8_t data[SFF_DUMP_SIZE];	/**< memory dump data */
};

/** Get information about the static properties of an SFP/QSFP module
 * The information is returned in numeric form.
 * @see ifconfig_sfp_get_sfp_info_strings to get corresponding strings.
 * @param h	An open ifconfig state handle
 * @param name	The name of an interface
 * @param sfp	Pointer to an object to fill, will be zeroed by this function
 * @return	0 if successful, -1 with error info set in the handle otherwise
 */
int ifconfig_sfp_get_sfp_info(ifconfig_handle_t *h, const char *name,
    struct ifconfig_sfp_info *sfp);

/** Get the number of channels present on the given module
 * @param sfp	Pointer to a filled SFP module info object
 * @return	The number of channels or 0 if unknown
 */
size_t ifconfig_sfp_channel_count(const struct ifconfig_sfp_info *sfp);

/** Is the given module ID a QSFP
 * NB: This convenience function is implemented in the header to keep the
 * classification criteria visible to the user.
 * @param id	The sfp_id field of a SFP module info object
 * @return	A bool true if QSFP-type sfp_id otherwise false
 */
static inline bool
ifconfig_sfp_id_is_qsfp(enum sfp_id id)
{
	switch (id) {
	case SFP_ID_QSFP:
	case SFP_ID_QSFPPLUS:
	case SFP_ID_QSFP28:
		return (true);
	default:
		return (false);
	}
}

/** Get string descriptions of the given SFP/QSFP module info
 * The strings are static and do not need to be freed.
 * @see ifconfig_sfp_get_sfp_info to obtain the input info.
 * @param sfp	Pointer to a filled SFP module info object
 * @param strings	Pointer to an object to be filled with pointers to
 *                      static strings describing the given info
 */
void ifconfig_sfp_get_sfp_info_strings(const struct ifconfig_sfp_info *sfp,
    struct ifconfig_sfp_info_strings *strings);

/** Get a string describing the given SFP/QSFP module's physical layer spec
 * The correct field in ifconfig_sfp_info varies depending on the module.  This
 * function chooses the appropriate string based on the provided module info.
 * The string returned is static and does not need to be freed.
 * @param sfp	Pointer to a filled SFP module info object
 * @param strings	Pointer to a filled SFP module strings object
 * @return	Pointer to a static string describing the module's spec
 */
const char *ifconfig_sfp_physical_spec(const struct ifconfig_sfp_info *sfp,
    const struct ifconfig_sfp_info_strings *strings);

/** Get the vendor info strings from an SFP/QSFP module
 * @param h	An open ifconfig state handle
 * @param name	The name of an interface
 * @param vi	Pointer to an object to be filled with the vendor info strings,
 *              will be zeroed by this function
 * @return	0 if successful, -1 with error info set in the handle otherwise
 */
int ifconfig_sfp_get_sfp_vendor_info(ifconfig_handle_t *h, const char *name,
    struct ifconfig_sfp_vendor_info *vi);

/** Get the status of an SFP/QSFP module's dynamic properties
 * @see ifconfig_sfp_free_sfp_status to free the allocations
 * @param h	An open ifconfig state handle
 * @param name	The name of an interface
 * @param ss	Pointer to an object to be filled with the module's status
 * @return	0 if successful, -1 with error info set in the handle otherwise
 *              where the errcode `ENXIO` indicates an SFP module that is not
 *              calibrated or does not provide diagnostic status measurements
 */
int ifconfig_sfp_get_sfp_status(ifconfig_handle_t *h, const char *name,
    struct ifconfig_sfp_status *ss);

/** Free the memory allocations in an ifconfig_sfp_status struct
 * @param ss	Pointer to an object whose internal allocations are to be freed
 * 		if not NULL
 */
void ifconfig_sfp_free_sfp_status(struct ifconfig_sfp_status *ss);

/** Dump the I2C memory of an SFP/QSFP module
 * SFP modules have one memory region dumped, QSFP modules have two.
 * @param h	An open ifconfig state handle
 * @param name	The name of an interface
 * @param buf	Pointer to a dump data buffer object
 * @return	0 if successful, -1 with error info set in the handle otherwise
 */
int ifconfig_sfp_get_sfp_dump(ifconfig_handle_t *h, const char *name,
    struct ifconfig_sfp_dump *buf);

/** Get the number of I2C memory dump regions present in the given dump
 * @param dp	Pointer to a filled dump data buffer object
 * @return	The number of regions or 0 if unknown
 */
size_t ifconfig_sfp_dump_region_count(const struct ifconfig_sfp_dump *dp);

/** Convert channel power to milliwatts power
 * This is provided as a convenience for displaying channel power levels.
 * @see (struct ifconfig_sfp_status).channel
 * @param power	Power in 0.1 mW units
 * @return	Power in milliwatts (mW)
 */
double power_mW(uint16_t power);

/** Convert channel power to decibel-milliwats power level
 * This is provided as a convenience for displaying channel power levels.
 * @see (struct ifconfig_sfp_status).channel
 * @param power	Power in 0.1 mW units
 * @return	Power level in decibel-milliwatts (dBm)
 */

double power_dBm(uint16_t power);

/** Convert channel bias current to milliamps
 * This is provided as a convenience for displaying channel bias currents.
 * @see (struct ifconfig_sfp_status).channel
 * @param bias	Bias current in 2 mA units
 * @return	Bias current in milliamps (mA)
 */
double bias_mA(uint16_t bias);
