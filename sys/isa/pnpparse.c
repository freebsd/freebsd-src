/*-
 * Copyright (c) 1999 Doug Rabson
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <isa/isavar.h>
#include <isa/pnpreg.h>
#include <isa/pnpvar.h>

/*
 * Parse resource data for Logical Devices.
 *
 * This function exits as soon as it gets an error reading *ANY*
 * Resource Data or ir reaches the end of Resource Data.  In the first
 * case the return value will be TRUE, FALSE otherwise.
 */
void
pnp_parse_resources(device_t dev, u_char *resources, int len)
{
	device_t parent = device_get_parent(dev);
	u_char tag, *resp, *resinfo;
	int large_len, scanning = len;
	u_int32_t compat_id;
	struct isa_config logdev, alt;
	struct isa_config *config;
	int priority = 0;
	int seenalt = 0;
	char buf[100];

	bzero(&logdev, sizeof logdev);
	bzero(&alt, sizeof alt);
	config = &logdev;
	resp = resources;
	while (scanning > 0) {
		tag = *resp++;
		scanning--;
		if (PNP_RES_TYPE(tag) == 0) {
			/* Small resource */
			if (scanning < PNP_SRES_LEN(tag)) {
				scanning = 0;
				continue;
			}
			resinfo = resp;
			resp += PNP_SRES_LEN(tag);
			scanning -= PNP_SRES_LEN(tag);;
			
			switch (PNP_SRES_NUM(tag)) {
			case PNP_TAG_COMPAT_DEVICE:
				/*
				 * Got a compatible device id
				 * resource. Should keep a list of
				 * compat ids in the device.
				 */
				bcopy(resinfo, &compat_id, 4);
				isa_set_compatid(dev, compat_id);
				break;
		    
			case PNP_TAG_IRQ_FORMAT:
				if (config->ic_nirq == ISA_NIRQ) {
					device_printf(parent, "too many irqs");
					scanning = 0;
					break;
				}
				config->ic_irqmask[config->ic_nirq] =
					resinfo[0] + (resinfo[1]<<8);
				config->ic_nirq++;
				break;

			case PNP_TAG_DMA_FORMAT:
				if (config->ic_ndrq == ISA_NDRQ) {
					device_printf(parent, "too many drqs");
					scanning = 0;
					break;
				}
				config->ic_drqmask[config->ic_ndrq] =
					resinfo[0];
				config->ic_ndrq++;
				break;

			case PNP_TAG_START_DEPENDANT:
				if (config == &alt) {
					ISA_ADD_CONFIG(parent, dev,
						       priority, config);
				} else if (config != &logdev) {
					device_printf(parent, "malformed\n");
					scanning = 0;
					break;
				}
				/*
				 * If the priority is not specified,
				 * then use the default of
				 * 'acceptable'
				 */
				if (PNP_SRES_LEN(tag) > 0)
					priority = resinfo[0];
				else
					priority = 1;
				alt = logdev;
				config = &alt;
				break;

			case PNP_TAG_END_DEPENDANT:
				ISA_ADD_CONFIG(parent, dev, priority, config);
				config = &logdev;
				seenalt = 1;
				break;

			case PNP_TAG_IO_RANGE:
				if (config->ic_nport == ISA_NPORT) {
					device_printf(parent, "too many ports");
					scanning = 0;
					break;
				}
				config->ic_port[config->ic_nport].ir_start =
					resinfo[1] + (resinfo[2]<<8);
				config->ic_port[config->ic_nport].ir_end =
					resinfo[3] + (resinfo[4]<<8)
					+ resinfo[6] - 1;
				config->ic_port[config->ic_nport].ir_size
					=
					resinfo[6];
				config->ic_port[config->ic_nport].ir_align =
					resinfo[5];
				config->ic_nport++;
				break;

			case PNP_TAG_IO_FIXED:
				if (config->ic_nport == ISA_NPORT) {
					device_printf(parent, "too many ports");
					scanning = 0;
					break;
				}
				config->ic_port[config->ic_nport].ir_start =
					resinfo[0] + (resinfo[1]<<8);
				config->ic_port[config->ic_nport].ir_end =
					resinfo[0] + (resinfo[1]<<8)
					+ resinfo[2] - 1;
				config->ic_port[config->ic_nport].ir_size
					= resinfo[2];
				config->ic_port[config->ic_nport].ir_align = 1;
				config->ic_nport++;
				break;

			default:
				/* Skip this resource */
				device_printf(parent, "unexpected tag %d\n",
					      PNP_SRES_NUM(tag));
				break;
			}
		} else {
			/* Large resource */
			if (scanning < 2) {
				scanning = 0;
				continue;
			}
			large_len = resp[0] + (resp[1] << 8);
			resp += 2;
			scanning -= 2;

			if (scanning < large_len) {
				scanning = 0;
				continue;
			}
			resinfo = resp;
			resp += large_len;
			scanning -= large_len;

			if (PNP_LRES_NUM(tag) == PNP_TAG_ID_ANSI) {
				if (large_len > sizeof(buf) - 1)
					large_len = sizeof(buf) - 1;
				bcopy(resinfo, buf, large_len);

				/*
				 * Trim trailing spaces.
				 */
				while (buf[large_len-1] == ' ')
					large_len--;
				buf[large_len] = '\0';
				device_set_desc_copy(dev, buf);
				continue;
			}

			if (PNP_LRES_NUM(tag) != PNP_TAG_MEMORY_RANGE) {
				/* skip */
				continue;
			}

			if (config->ic_nmem == ISA_NMEM) {
				device_printf(parent, "too many memory ranges");
				scanning = 0;
				break;
			}

			config->ic_mem[config->ic_nmem].ir_start =
				(resinfo[4]<<8) + (resinfo[5]<<16);
			config->ic_mem[config->ic_nmem].ir_end =
				(resinfo[6]<<8) + (resinfo[7]<<16);
			config->ic_mem[config->ic_nmem].ir_size =
				(resinfo[10]<<8) + (resinfo[11]<<16);
			config->ic_mem[config->ic_nmem].ir_align =
				resinfo[8] + (resinfo[9]<<8);
			if (!config->ic_mem[config->ic_nmem].ir_align)
				config->ic_mem[config->ic_nmem].ir_align =
					0x10000;
			config->ic_nmem++;
		}
	}

	/*
	 * Some devices (e.g. network cards) don't have start
	 * dependant tags and only have a single configuration. If we
	 * finish parsing without seeing an end dependant tag, add the 
	 * non-dependant configuration to the device.
	 */
	if (!seenalt)
		ISA_ADD_CONFIG(parent, dev, 1, config);
}

