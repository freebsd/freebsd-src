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
 *	$FreeBSD: src/sys/isa/pnpparse.c,v 1.2.2.2 2000/03/31 19:41:10 dfr Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <isa/isavar.h>
#include <isa/pnpreg.h>
#include <isa/pnpvar.h>

#define	MAXDEP	8

#define I16(p)	((p)[0] + ((p)[1] << 8))
#define I32(p)	(I16(p) + (I16(p+2) << 16))

/*
 * Parse resource data for Logical Devices.
 *
 * This function exits as soon as it gets an error reading *ANY*
 * Resource Data or it reaches the end of Resource Data.
 */
void
pnp_parse_resources(device_t dev, u_char *resources, int len)
{
	device_t parent = device_get_parent(dev);
	u_char tag, *resp, *resinfo;
	int large_len, scanning = len;
	u_int32_t id, compat_id;
	struct isa_config *config;
	int ncfgs = 1;
	int priorities[1 + MAXDEP];
	struct isa_config *configs;
	char buf[100];
	int i;

	id = isa_get_logicalid(dev);
	configs = (struct isa_config *)malloc(sizeof(*configs) * (1 + MAXDEP),
						M_DEVBUF, M_NOWAIT);
	if (configs == NULL) {
		device_printf(dev, "No memory to parse PNP data\n");
		return;
	}
	bzero(configs, sizeof(*configs) * (1 + MAXDEP));
	config = &configs[0];
	priorities[0] = 0;
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
				if (bootverbose) {
					printf("%s: adding irq mask %#04x\n",
					       pnp_eisaformat(id),
					       I16(resinfo));
				}
				if (config->ic_nirq == ISA_NIRQ) {
					device_printf(parent, "too many irqs\n");
					scanning = 0;
					break;
				}
				config->ic_irqmask[config->ic_nirq] =
					I16(resinfo);
				config->ic_nirq++;
				break;

			case PNP_TAG_DMA_FORMAT:
				if (bootverbose) {
					printf("%s: adding dma mask %#02x\n",
					       pnp_eisaformat(id),
					       resinfo[0]);
				}
				if (config->ic_ndrq == ISA_NDRQ) {
					device_printf(parent, "too many drqs\n");
					scanning = 0;
					break;
				}
				config->ic_drqmask[config->ic_ndrq] =
					resinfo[0];
				config->ic_ndrq++;
				break;

			case PNP_TAG_START_DEPENDANT:
				if (bootverbose) {
					printf("%s: start dependant\n",
					       pnp_eisaformat(id));
				}
				if (ncfgs >= MAXDEP) {
					device_printf(parent, "too many dependant configs (%d)\n", MAXDEP);
					scanning = 0;
					break;
				}
				config = &configs[ncfgs];
				/*
				 * If the priority is not specified,
				 * then use the default of
				 * 'acceptable'
				 */
				if (PNP_SRES_LEN(tag) > 0)
					priorities[ncfgs] = resinfo[0];
				else
					priorities[ncfgs] = 1;
				ncfgs++;
				break;

			case PNP_TAG_END_DEPENDANT:
				if (bootverbose) {
					printf("%s: end dependant\n",
					       pnp_eisaformat(id));
				}
				config = &configs[0];	/* back to main config */
				break;

			case PNP_TAG_IO_RANGE:
				if (bootverbose) {
					printf("%s: adding io range "
					       "%#x-%#x, size=%#x, "
					       "align=%#x\n",
					       pnp_eisaformat(id),
					       I16(resinfo + 1),
					       I16(resinfo + 3) + resinfo[6]-1,
					       resinfo[6],
					       resinfo[5]);
				}
				if (config->ic_nport == ISA_NPORT) {
					device_printf(parent, "too many ports\n");
					scanning = 0;
					break;
				}
				config->ic_port[config->ic_nport].ir_start =
					I16(resinfo + 1);
				config->ic_port[config->ic_nport].ir_end =
					I16(resinfo + 3) + resinfo[6] - 1;
				config->ic_port[config->ic_nport].ir_size =
					resinfo[6];
				if (resinfo[5] == 0) {
				    /* Make sure align is at least one */
				    resinfo[5] = 1;
				}
				config->ic_port[config->ic_nport].ir_align =
					resinfo[5];
				config->ic_nport++;
				break;

			case PNP_TAG_IO_FIXED:
				if (bootverbose) {
					printf("%s: adding fixed io range "
					       "%#x-%#x, size=%#x, "
					       "align=%#x\n",
					       pnp_eisaformat(id),
					       I16(resinfo),
					       I16(resinfo) + resinfo[2] - 1,
					       resinfo[2],
					       1);
				}
				if (config->ic_nport == ISA_NPORT) {
					device_printf(parent, "too many ports\n");
					scanning = 0;
					break;
				}
				config->ic_port[config->ic_nport].ir_start =
					I16(resinfo);
				config->ic_port[config->ic_nport].ir_end =
					I16(resinfo) + resinfo[2] - 1;
				config->ic_port[config->ic_nport].ir_size
					= resinfo[2];
				config->ic_port[config->ic_nport].ir_align = 1;
				config->ic_nport++;
				break;

			case PNP_TAG_END:
				if (bootverbose) {
					printf("%s: end config\n",
					       pnp_eisaformat(id));
				}
				scanning = 0;
				break;

			default:
				/* Skip this resource */
				device_printf(parent, "unexpected small tag %d\n",
					      PNP_SRES_NUM(tag));
				break;
			}
		} else {
			/* Large resource */
			if (scanning < 2) {
				scanning = 0;
				continue;
			}
			large_len = I16(resp);
			resp += 2;
			scanning -= 2;

			if (scanning < large_len) {
				scanning = 0;
				continue;
			}
			resinfo = resp;
			resp += large_len;
			scanning -= large_len;

			switch (PNP_LRES_NUM(tag)) {
			case PNP_TAG_ID_ANSI:
				if (large_len > sizeof(buf) - 1)
					large_len = sizeof(buf) - 1;
				bcopy(resinfo, buf, large_len);

				/*
				 * Trim trailing spaces and garbage.
				 */
				while (large_len > 0 && buf[large_len - 1] <= ' ')
					large_len--;
				buf[large_len] = '\0';
				device_set_desc_copy(dev, buf);
				break;
				
			case PNP_TAG_MEMORY_RANGE:
				if (bootverbose) {
					int temp = I16(resinfo + 7) << 8;

					printf("%s: adding memory range "
					       "%#x-%#x, size=%#x, "
					       "align=%#x\n",
					       pnp_eisaformat(id),
					       I16(resinfo + 1)<<8,
					       (I16(resinfo + 3)<<8) + temp - 1,
					       temp,
					       I16(resinfo + 5));
				}

				if (config->ic_nmem == ISA_NMEM) {
					device_printf(parent, "too many memory ranges\n");
					scanning = 0;
					break;
				}

				config->ic_mem[config->ic_nmem].ir_start =
					I16(resinfo + 1)<<8;
				config->ic_mem[config->ic_nmem].ir_end =
					(I16(resinfo + 3)<<8)
					+ (I16(resinfo + 7) << 8) - 1;
				config->ic_mem[config->ic_nmem].ir_size =
					I16(resinfo + 7) << 8;
				config->ic_mem[config->ic_nmem].ir_align =
					I16(resinfo + 5);
				if (!config->ic_mem[config->ic_nmem].ir_align)
					config->ic_mem[config->ic_nmem]
						.ir_align = 0x10000;
				config->ic_nmem++;
				break;

			case PNP_TAG_MEMORY32_RANGE:
				if (bootverbose) {
					printf("%s: adding memory32 range "
					       "%#x-%#x, size=%#x, "
					       "align=%#x\n",
					       pnp_eisaformat(id),
					       I32(resinfo + 1),
					       I32(resinfo + 5)
					       + I32(resinfo + 13) - 1,
					       I32(resinfo + 13),
					       I32(resinfo + 9));
				}

				if (config->ic_nmem == ISA_NMEM) {
					device_printf(parent, "too many memory ranges\n");
					scanning = 0;
					break;
				}

				config->ic_mem[config->ic_nmem].ir_start =
					I32(resinfo + 1);
				config->ic_mem[config->ic_nmem].ir_end =
					I32(resinfo + 5)
					+ I32(resinfo + 13) - 1;
				config->ic_mem[config->ic_nmem].ir_size =
					I32(resinfo + 13);
				config->ic_mem[config->ic_nmem].ir_align =
					I32(resinfo + 9);
				config->ic_nmem++;
				break;

			case PNP_TAG_MEMORY32_FIXED:
				if (I32(resinfo + 5) == 0) {
					if (bootverbose) {
						printf("%s: skipping empty range\n",
						       pnp_eisaformat(id));
					}
					continue;
				}
				if (bootverbose) {
					printf("%s: adding fixed memory32 range "
					       "%#x-%#x, size=%#x\n",
					       pnp_eisaformat(id),
					       I32(resinfo + 1),
					       I32(resinfo + 1)
					       + I32(resinfo + 5) - 1,
					       I32(resinfo + 5));
				}

				if (config->ic_nmem == ISA_NMEM) {
					device_printf(parent, "too many memory ranges\n");
					scanning = 0;
					break;
				}

				config->ic_mem[config->ic_nmem].ir_start =
					I32(resinfo + 1);
				config->ic_mem[config->ic_nmem].ir_end =
					I32(resinfo + 1)
					+ I32(resinfo + 5) - 1;
				config->ic_mem[config->ic_nmem].ir_size =
					I32(resinfo + 5);
				config->ic_mem[config->ic_nmem].ir_align = 1;
				config->ic_nmem++;
				break;

			default:
				/* Skip this resource */
				device_printf(parent, "unexpected large tag %d\n",
					      PNP_SRES_NUM(tag));
			}
		}
	}
	if(ncfgs == 1) {
		/* Single config without dependants */
		(void)ISA_ADD_CONFIG(parent, dev, priorities[0], &configs[0]);
		free(configs, M_DEVBUF);
		return;
	}
	/* Cycle through dependant configs merging primary details */
	for(i = 1; i < ncfgs; i++) {
		int j;
		config = &configs[i];
		for(j = 0; j < configs[0].ic_nmem; j++) {
			if (config->ic_nmem == ISA_NMEM) {
				device_printf(parent, "too many memory ranges\n");
				free(configs, M_DEVBUF);
				return;
			}
			config->ic_mem[config->ic_nmem] = configs[0].ic_mem[j];
			config->ic_nmem++;
		}
		for(j = 0; j < configs[0].ic_nport; j++) {
			if (config->ic_nport == ISA_NPORT) {
				device_printf(parent, "too many port ranges\n");
				free(configs, M_DEVBUF);
				return;
			}
			config->ic_port[config->ic_nport] = configs[0].ic_port[j];
			config->ic_nport++;
		}
		for(j = 0; j < configs[0].ic_nirq; j++) {
			if (config->ic_nirq == ISA_NIRQ) {
				device_printf(parent, "too many irq ranges\n");
				free(configs, M_DEVBUF);
				return;
			}
			config->ic_irqmask[config->ic_nirq] = configs[0].ic_irqmask[j];
			config->ic_nirq++;
		}
		for(j = 0; j < configs[0].ic_ndrq; j++) {
			if (config->ic_ndrq == ISA_NDRQ) {
				device_printf(parent, "too many drq ranges\n");
				free(configs, M_DEVBUF);
				return;
			}
			config->ic_drqmask[config->ic_ndrq] = configs[0].ic_drqmask[j];
			config->ic_ndrq++;
		}
		(void)ISA_ADD_CONFIG(parent, dev, priorities[i], &configs[i]);
	}
	free(configs, M_DEVBUF);
}
