/*******************************************************************************

Copyright (c) 2006, Myricom Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Myricom Inc, nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/memrange.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sx.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/zlib.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <machine/clock.h>      /* for DELAY */
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <vm/vm.h>		/* for pmap_mapdev() */
#include <vm/pmap.h>

#include <dev/myri10ge/myri10ge_mcp.h>
#include <dev/myri10ge/mcp_gen_header.h>
#include <dev/myri10ge/if_myri10ge_var.h>

/* tunable params */
static int myri10ge_nvidia_ecrc_enable = 1;
static int myri10ge_max_intr_slots = 128;
static int myri10ge_intr_coal_delay = 30;
static int myri10ge_skip_pio_read = 0;
static int myri10ge_flow_control = 1;
static char *myri10ge_fw_unaligned = "myri10ge_ethp_z8e";
static char *myri10ge_fw_aligned = "myri10ge_eth_z8e";

static int myri10ge_probe(device_t dev);
static int myri10ge_attach(device_t dev);
static int myri10ge_detach(device_t dev);
static int myri10ge_shutdown(device_t dev);
static void myri10ge_intr(void *arg);

static device_method_t myri10ge_methods[] =
{
  /* Device interface */
  DEVMETHOD(device_probe, myri10ge_probe),
  DEVMETHOD(device_attach, myri10ge_attach),
  DEVMETHOD(device_detach, myri10ge_detach),
  DEVMETHOD(device_shutdown, myri10ge_shutdown),
  {0, 0}
};

static driver_t myri10ge_driver =
{
  "myri10ge",
  myri10ge_methods,
  sizeof(myri10ge_softc_t),
};

static devclass_t myri10ge_devclass;

/* Declare ourselves to be a child of the PCI bus.*/
DRIVER_MODULE(myri10ge, pci, myri10ge_driver, myri10ge_devclass, 0, 0);
MODULE_DEPEND(myri10ge, firmware, 1, 1, 1);

static int
myri10ge_probe(device_t dev)
{
  if ((pci_get_vendor(dev) == MYRI10GE_PCI_VENDOR_MYRICOM) &&
      (pci_get_device(dev) == MYRI10GE_PCI_DEVICE_Z8E)) {
	  device_set_desc(dev, "Myri10G-PCIE-8A");
	  return 0;
  }
  return ENXIO;
}

static void
myri10ge_enable_wc(myri10ge_softc_t *sc)
{
	struct mem_range_desc mrdesc;
	vm_paddr_t pa;
	vm_offset_t len;
	int err, action;

	pa = rman_get_start(sc->mem_res);
	len = rman_get_size(sc->mem_res);
	mrdesc.mr_base = pa;
	mrdesc.mr_len = len;
	mrdesc.mr_flags = MDF_WRITECOMBINE;
	action = MEMRANGE_SET_UPDATE;
	strcpy((char *)&mrdesc.mr_owner, "myri10ge");
	err = mem_range_attr_set(&mrdesc, &action);
	if (err != 0) {
		device_printf(sc->dev, 
			      "w/c failed for pa 0x%lx, len 0x%lx, err = %d\n",
			      (unsigned long)pa, (unsigned long)len, err);
	} else {
		sc->wc = 1;
	}
}


/* callback to get our DMA address */
static void
myri10ge_dmamap_callback(void *arg, bus_dma_segment_t *segs, int nsegs,
			 int error)
{
	if (error == 0) {
		*(bus_addr_t *) arg = segs->ds_addr;
	}
}

static int
myri10ge_dma_alloc(myri10ge_softc_t *sc, myri10ge_dma_t *dma, size_t bytes, 
		   bus_size_t alignment)
{
	int err;
	device_t dev = sc->dev;

	/* allocate DMAable memory tags */
	err = bus_dma_tag_create(sc->parent_dmat,	/* parent */
				 alignment,		/* alignment */
				 4096,			/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 bytes,			/* maxsize */
				 1,			/* num segs */
				 4096,			/* maxsegsize */
				 BUS_DMA_COHERENT,	/* flags */
				 NULL, NULL,		/* lock */
				 &dma->dmat);		/* tag */
	if (err != 0) {
		device_printf(dev, "couldn't alloc tag (err = %d)\n", err);
		return err;
	}

	/* allocate DMAable memory & map */
	err = bus_dmamem_alloc(dma->dmat, &dma->addr, 
			       (BUS_DMA_WAITOK | BUS_DMA_COHERENT 
				| BUS_DMA_ZERO),  &dma->map);
	if (err != 0) {
		device_printf(dev, "couldn't alloc mem (err = %d)\n", err);
		goto abort_with_dmat;
	}

	/* load the memory */
	err = bus_dmamap_load(dma->dmat, dma->map, dma->addr, bytes,
			      myri10ge_dmamap_callback,
			      (void *)&dma->bus_addr, 0);
	if (err != 0) {
		device_printf(dev, "couldn't load map (err = %d)\n", err);
		goto abort_with_mem;
	}
	return 0;

abort_with_mem:
	bus_dmamem_free(dma->dmat, dma->addr, dma->map);
abort_with_dmat:
	(void)bus_dma_tag_destroy(dma->dmat);
	return err;
}


static void
myri10ge_dma_free(myri10ge_dma_t *dma)
{
	bus_dmamap_unload(dma->dmat, dma->map);
	bus_dmamem_free(dma->dmat, dma->addr, dma->map);
	(void)bus_dma_tag_destroy(dma->dmat);
}

/*
 * The eeprom strings on the lanaiX have the format
 * SN=x\0
 * MAC=x:x:x:x:x:x\0
 * PC=text\0
 */

static int
myri10ge_parse_strings(myri10ge_softc_t *sc)
{
#define MYRI10GE_NEXT_STRING(p) while(ptr < limit && *ptr++)

	char *ptr, *limit;
	int i, found_mac;

	ptr = sc->eeprom_strings;
	limit = sc->eeprom_strings + MYRI10GE_EEPROM_STRINGS_SIZE;
	found_mac = 0;
	while (ptr < limit && *ptr != '\0') {
		if (memcmp(ptr, "MAC=", 4) == 0) {
			ptr+=4;
			sc->mac_addr_string = ptr;
			for (i = 0; i < 6; i++) {
				if ((ptr + 2) > limit)
					goto abort;
				sc->mac_addr[i] = strtoul(ptr, NULL, 16);
				found_mac = 1;
				ptr += 3;
			}
		} else if (memcmp(ptr, "PC=", 4) == 0) {
			sc->product_code_string = ptr;
		}
		MYRI10GE_NEXT_STRING(ptr);
	}

	if (found_mac)
		return 0;

 abort:
	device_printf(sc->dev, "failed to parse eeprom_strings\n");

	return ENXIO;
}

#if #cpu(i386) || defined __i386 || defined i386 || defined __i386__ || #cpu(x86_64) || defined __x86_64__
static int
myri10ge_enable_nvidia_ecrc(myri10ge_softc_t *sc, device_t pdev)
{
	uint32_t val;
	unsigned long off;
	char *va, *cfgptr;
	uint16_t vendor_id, device_id;
	uintptr_t bus, slot, func, ivend, idev;
	uint32_t *ptr32;

	/* XXXX
	   Test below is commented because it is believed that doing
	   config read/write beyond 0xff will access the config space
	   for the next larger function.  Uncomment this and remove 
	   the hacky pmap_mapdev() way of accessing config space when
	   FreeBSD grows support for extended pcie config space access
	*/
#if 0	
	/* See if we can, by some miracle, access the extended
	   config space */
	val = pci_read_config(pdev, 0x178, 4);
	if (val != 0xffffffff) {
		val |= 0x40;
		pci_write_config(pdev, 0x178, val, 4);
		return 0;
	}
#endif
	/* Rather than using normal pci config space writes, we must
	 * map the Nvidia config space ourselves.  This is because on
	 * opteron/nvidia class machine the 0xe000000 mapping is
	 * handled by the nvidia chipset, that means the internal PCI
	 * device (the on-chip northbridge), or the amd-8131 bridge
	 * and things behind them are not visible by this method.
	 */

	BUS_READ_IVAR(device_get_parent(pdev), pdev,
		      PCI_IVAR_BUS, &bus);
	BUS_READ_IVAR(device_get_parent(pdev), pdev,
		      PCI_IVAR_SLOT, &slot);
	BUS_READ_IVAR(device_get_parent(pdev), pdev,
		      PCI_IVAR_FUNCTION, &func);
	BUS_READ_IVAR(device_get_parent(pdev), pdev,
		      PCI_IVAR_VENDOR, &ivend);
	BUS_READ_IVAR(device_get_parent(pdev), pdev,
		      PCI_IVAR_DEVICE, &idev);
					
	off =  0xe0000000UL 
		+ 0x00100000UL * (unsigned long)bus
		+ 0x00001000UL * (unsigned long)(func
						 + 8 * slot);

	/* map it into the kernel */
	va = pmap_mapdev(trunc_page((vm_paddr_t)off), PAGE_SIZE);
	

	if (va == NULL) {
		device_printf(sc->dev, "pmap_kenter_temporary didn't\n");
		return EIO;
	}
	/* get a pointer to the config space mapped into the kernel */
	cfgptr = va + (off & PAGE_MASK);

	/* make sure that we can really access it */
	vendor_id = *(uint16_t *)(cfgptr + PCIR_VENDOR);
	device_id = *(uint16_t *)(cfgptr + PCIR_DEVICE);
	if (! (vendor_id == ivend && device_id == idev)) {
		device_printf(sc->dev, "mapping failed: 0x%x:0x%x\n",
			      vendor_id, device_id);
		pmap_unmapdev((vm_offset_t)va, PAGE_SIZE);
		return EIO;
	}

	ptr32 = (uint32_t*)(cfgptr + 0x178);
	val = *ptr32;

	if (val == 0xffffffff) {
		device_printf(sc->dev, "extended mapping failed\n");
		pmap_unmapdev((vm_offset_t)va, PAGE_SIZE);
		return EIO;
	}
	*ptr32 = val | 0x40;
	pmap_unmapdev((vm_offset_t)va, PAGE_SIZE);
	device_printf(sc->dev,
		      "Enabled ECRC on upstream Nvidia bridge at %d:%d:%d\n",
		      (int)bus, (int)slot, (int)func);
	return 0;
}
#else
static int
myri10ge_enable_nvidia_ecrc(myri10ge_softc_t *sc, device_t pdev)
{
	device_printf(sc->dev,
		      "Nforce 4 chipset on non-x86/amd64!?!?!\n");
	return ENXIO;
}
#endif
/*
 * The Lanai Z8E PCI-E interface achieves higher Read-DMA throughput
 * when the PCI-E Completion packets are aligned on an 8-byte
 * boundary.  Some PCI-E chip sets always align Completion packets; on
 * the ones that do not, the alignment can be enforced by enabling
 * ECRC generation (if supported).
 *
 * When PCI-E Completion packets are not aligned, it is actually more
 * efficient to limit Read-DMA transactions to 2KB, rather than 4KB.
 *
 * If the driver can neither enable ECRC nor verify that it has
 * already been enabled, then it must use a firmware image which works
 * around unaligned completion packets (ethp_z8e.dat), and it should
 * also ensure that it never gives the device a Read-DMA which is
 * larger than 2KB by setting the tx.boundary to 2KB.  If ECRC is
 * enabled, then the driver should use the aligned (eth_z8e.dat)
 * firmware image, and set tx.boundary to 4KB.
 */

static void
myri10ge_select_firmware(myri10ge_softc_t *sc)
{
	int err, aligned = 0;
	device_t pdev;
	uint16_t pvend, pdid;

	pdev = device_get_parent(device_get_parent(sc->dev));
	if (pdev == NULL) {
		device_printf(sc->dev, "could not find parent?\n");
		goto abort;
	}
	pvend = pci_read_config(pdev, PCIR_VENDOR, 2);
	pdid = pci_read_config(pdev, PCIR_DEVICE, 2);

	/* see if we can enable ECRC's on an upstream
	   Nvidia bridge */
	if (myri10ge_nvidia_ecrc_enable &&
	    (pvend == 0x10de && pdid == 0x005d)) {
		err = myri10ge_enable_nvidia_ecrc(sc, pdev);
		if (err == 0) {
			aligned = 1;
			device_printf(sc->dev, 
				      "Assuming aligned completions (ECRC)\n");
		}
	}
	/* see if the upstream bridge is known to
	   provided aligned completions */
	if (/* HT2000  */ (pvend == 0x1166 && pdid == 0x0132) ||
	    /* Ontario */ (pvend == 0x10b5 && pdid == 0x8532)) {
		device_printf(sc->dev,
			      "Assuming aligned completions (0x%x:0x%x)\n",
			      pvend, pdid);
	}

abort:
	if (aligned) {
		sc->fw_name = myri10ge_fw_aligned;
		sc->tx.boundary = 4096;
	} else {
		sc->fw_name = myri10ge_fw_unaligned;
		sc->tx.boundary = 2048;
	}
}

union qualhack
{
        const char *ro_char;
        char *rw_char;
};


static int
myri10ge_load_firmware_helper(myri10ge_softc_t *sc, uint32_t *limit)
{
	struct firmware *fw;
	const mcp_gen_header_t *hdr;
	unsigned hdr_offset;
	const char *fw_data;
	union qualhack hack;
	int status;
	

	fw = firmware_get(sc->fw_name);

	if (fw == NULL) {
		device_printf(sc->dev, "Could not find firmware image %s\n",
			      sc->fw_name);
		return ENOENT;
	}
	if (fw->datasize > *limit || 
	    fw->datasize < MCP_HEADER_PTR_OFFSET + 4) {
		device_printf(sc->dev, "Firmware image %s too large (%d/%d)\n",
			      sc->fw_name, (int)fw->datasize, (int) *limit);
		status = ENOSPC;
		goto abort_with_fw;
	}
	*limit = fw->datasize;

	/* check id */
	fw_data = (const char *)fw->data;
	hdr_offset = htobe32(*(const uint32_t *)
			     (fw_data + MCP_HEADER_PTR_OFFSET));
	if ((hdr_offset & 3) || hdr_offset + sizeof(*hdr) > fw->datasize) {
		device_printf(sc->dev, "Bad firmware file");
		status = EIO;
		goto abort_with_fw;
	}
	hdr = (const void*)(fw_data + hdr_offset); 
	if (be32toh(hdr->mcp_type) != MCP_TYPE_ETH) {
		device_printf(sc->dev, "Bad firmware type: 0x%x\n", 
			      be32toh(hdr->mcp_type));
		status = EIO;
		goto abort_with_fw;
	}

	/* save firmware version for sysctl */
	strncpy(sc->fw_version, hdr->version, sizeof (sc->fw_version));
	device_printf(sc->dev, "firmware id: %s\n", hdr->version);

	hack.ro_char = fw_data;
	/* Copy the inflated firmware to NIC SRAM. */
	myri10ge_pio_copy(&sc->sram[MYRI10GE_FW_OFFSET], 
			  hack.rw_char,  *limit);

	status = 0;
abort_with_fw:
	firmware_put(fw, FIRMWARE_UNLOAD);
	return status;
}

/*
 * Enable or disable periodic RDMAs from the host to make certain
 * chipsets resend dropped PCIe messages
 */

static void
myri10ge_dummy_rdma(myri10ge_softc_t *sc, int enable)
{
	char buf_bytes[72];
	volatile uint32_t *confirm;
	volatile char *submit;
	uint32_t *buf, dma_low, dma_high;
	int i;

	buf = (uint32_t *)((unsigned long)(buf_bytes + 7) & ~7UL);

	/* clear confirmation addr */
	confirm = (volatile uint32_t *)sc->cmd;
	*confirm = 0;
	mb();

	/* send an rdma command to the PCIe engine, and wait for the
	   response in the confirmation address.  The firmware should
	   write a -1 there to indicate it is alive and well
	*/

	dma_low = MYRI10GE_LOWPART_TO_U32(sc->cmd_dma.bus_addr);
	dma_high = MYRI10GE_HIGHPART_TO_U32(sc->cmd_dma.bus_addr);
	buf[0] = htobe32(dma_high);		/* confirm addr MSW */
	buf[1] = htobe32(dma_low);		/* confirm addr LSW */
	buf[2] = htobe32(0xffffffff);		/* confirm data */
	dma_low = MYRI10GE_LOWPART_TO_U32(sc->zeropad_dma.bus_addr);
	dma_high = MYRI10GE_HIGHPART_TO_U32(sc->zeropad_dma.bus_addr);
	buf[3] = htobe32(dma_high); 		/* dummy addr MSW */
	buf[4] = htobe32(dma_low); 		/* dummy addr LSW */
	buf[5] = htobe32(enable);			/* enable? */


	submit = (volatile char *)(sc->sram + 0xfc01c0);

	myri10ge_pio_copy(submit, buf, 64);
	mb();
	DELAY(1000);
	mb();
	i = 0;
	while (*confirm != 0xffffffff && i < 20) {
		DELAY(1000);
		i++;
	}
	if (*confirm != 0xffffffff) {
		device_printf(sc->dev, "dummy rdma %s failed (%p = 0x%x)", 
			      (enable ? "enable" : "disable"), confirm, 
			      *confirm);
	}
	return;
}

static int 
myri10ge_send_cmd(myri10ge_softc_t *sc, uint32_t cmd, 
		  myri10ge_cmd_t *data)
{
	mcp_cmd_t *buf;
	char buf_bytes[sizeof(*buf) + 8];
	volatile mcp_cmd_response_t *response = sc->cmd;
	volatile char *cmd_addr = sc->sram + MYRI10GE_MCP_CMD_OFFSET;
	uint32_t dma_low, dma_high;
	int sleep_total = 0;

	/* ensure buf is aligned to 8 bytes */
	buf = (mcp_cmd_t *)((unsigned long)(buf_bytes + 7) & ~7UL);

	buf->data0 = htobe32(data->data0);
	buf->data1 = htobe32(data->data1);
	buf->data2 = htobe32(data->data2);
	buf->cmd = htobe32(cmd);
	dma_low = MYRI10GE_LOWPART_TO_U32(sc->cmd_dma.bus_addr);
	dma_high = MYRI10GE_HIGHPART_TO_U32(sc->cmd_dma.bus_addr);

	buf->response_addr.low = htobe32(dma_low);
	buf->response_addr.high = htobe32(dma_high);
	mtx_lock(&sc->cmd_lock);
	response->result = 0xffffffff;
	mb();
	myri10ge_pio_copy((volatile void *)cmd_addr, buf, sizeof (*buf));

	/* wait up to 2 seconds */
	for (sleep_total = 0; sleep_total <  (2 * 1000); sleep_total += 10) {
		bus_dmamap_sync(sc->cmd_dma.dmat, 
				sc->cmd_dma.map, BUS_DMASYNC_POSTREAD);
		mb();
		if (response->result != 0xffffffff) {
			if (response->result == 0) {
				data->data0 = be32toh(response->data);
				mtx_unlock(&sc->cmd_lock);
				return 0;
			} else {
				device_printf(sc->dev, 
					      "myri10ge: command %d "
					      "failed, result = %d\n",
					      cmd, be32toh(response->result));
				mtx_unlock(&sc->cmd_lock);
				return ENXIO;
			}
		}
		DELAY(1000 * 10);
	}
	mtx_unlock(&sc->cmd_lock);
	device_printf(sc->dev, "myri10ge: command %d timed out"
		      "result = %d\n",
		      cmd, be32toh(response->result));
	return EAGAIN;
}


static int
myri10ge_load_firmware(myri10ge_softc_t *sc)
{
	volatile uint32_t *confirm;
	volatile char *submit;
	char buf_bytes[72];
	uint32_t *buf, size, dma_low, dma_high;
	int status, i;

	buf = (uint32_t *)((unsigned long)(buf_bytes + 7) & ~7UL);

	size = sc->sram_size;
	status = myri10ge_load_firmware_helper(sc, &size);
	if (status) {
		device_printf(sc->dev, "firmware loading failed\n");
		return status;
	}
	/* clear confirmation addr */
	confirm = (volatile uint32_t *)sc->cmd;
	*confirm = 0;
	mb();
	/* send a reload command to the bootstrap MCP, and wait for the
	   response in the confirmation address.  The firmware should
	   write a -1 there to indicate it is alive and well
	*/

	dma_low = MYRI10GE_LOWPART_TO_U32(sc->cmd_dma.bus_addr);
	dma_high = MYRI10GE_HIGHPART_TO_U32(sc->cmd_dma.bus_addr);

	buf[0] = htobe32(dma_high);	/* confirm addr MSW */
	buf[1] = htobe32(dma_low);	/* confirm addr LSW */
	buf[2] = htobe32(0xffffffff);	/* confirm data */

	/* FIX: All newest firmware should un-protect the bottom of
	   the sram before handoff. However, the very first interfaces
	   do not. Therefore the handoff copy must skip the first 8 bytes
	*/
					/* where the code starts*/
	buf[3] = htobe32(MYRI10GE_FW_OFFSET + 8);
	buf[4] = htobe32(size - 8); 	/* length of code */
	buf[5] = htobe32(8);		/* where to copy to */
	buf[6] = htobe32(0);		/* where to jump to */

	submit = (volatile char *)(sc->sram + 0xfc0000);
	myri10ge_pio_copy(submit, buf, 64);
	mb();
	DELAY(1000);
	mb();
	i = 0;
	while (*confirm != 0xffffffff && i < 20) {
		DELAY(1000*10);
		i++;
		bus_dmamap_sync(sc->cmd_dma.dmat, 
				sc->cmd_dma.map, BUS_DMASYNC_POSTREAD);
	}
	if (*confirm != 0xffffffff) {
		device_printf(sc->dev,"handoff failed (%p = 0x%x)", 
			confirm, *confirm);
		
		return ENXIO;
	}
	myri10ge_dummy_rdma(sc, 1);
	return 0;
}

static int
myri10ge_update_mac_address(myri10ge_softc_t *sc)
{
	myri10ge_cmd_t cmd;
	uint8_t *addr = sc->mac_addr;
	int status;

	
	cmd.data0 = ((addr[0] << 24) | (addr[1] << 16) 
		     | (addr[2] << 8) | addr[3]);

	cmd.data1 = ((addr[4] << 8) | (addr[5]));

	status = myri10ge_send_cmd(sc, MYRI10GE_MCP_SET_MAC_ADDRESS, &cmd);
	return status;
}

static int
myri10ge_change_pause(myri10ge_softc_t *sc, int pause)
{	
	myri10ge_cmd_t cmd;
	int status;

	if (pause)
		status = myri10ge_send_cmd(sc, 
					   MYRI10GE_MCP_ENABLE_FLOW_CONTROL, 
					   &cmd);
	else
		status = myri10ge_send_cmd(sc, 
					   MYRI10GE_MCP_DISABLE_FLOW_CONTROL, 
					   &cmd);

	if (status) {
		device_printf(sc->dev, "Failed to set flow control mode\n");
		return ENXIO;
	}
	sc->pause = pause;
	return 0;
}

static void
myri10ge_change_promisc(myri10ge_softc_t *sc, int promisc)
{	
	myri10ge_cmd_t cmd;
	int status;

	if (promisc)
		status = myri10ge_send_cmd(sc, 
					   MYRI10GE_MCP_ENABLE_PROMISC, 
					   &cmd);
	else
		status = myri10ge_send_cmd(sc, 
					   MYRI10GE_MCP_DISABLE_PROMISC, 
					   &cmd);

	if (status) {
		device_printf(sc->dev, "Failed to set promisc mode\n");
	}
}

static int
myri10ge_reset(myri10ge_softc_t *sc)
{

	myri10ge_cmd_t cmd;
	int status, i;

	/* try to send a reset command to the card to see if it
	   is alive */
	memset(&cmd, 0, sizeof (cmd));
	status = myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_RESET, &cmd);
	if (status != 0) {
		device_printf(sc->dev, "failed reset\n");
		return ENXIO;
	}

	/* Now exchange information about interrupts  */

	cmd.data0 = (uint32_t) 
		(myri10ge_max_intr_slots * sizeof (*sc->intr.q[0]));
	status = myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_SET_INTRQ_SIZE, &cmd);
	for (i = 0; (status == 0) && (i < MYRI10GE_NUM_INTRQS); i++) {
		cmd.data0 = MYRI10GE_LOWPART_TO_U32(sc->intr.dma[i].bus_addr);
		cmd.data1 = MYRI10GE_HIGHPART_TO_U32(sc->intr.dma[i].bus_addr);
		status |= 
			myri10ge_send_cmd(sc, (i + 
					       MYRI10GE_MCP_CMD_SET_INTRQ0_DMA),
					  &cmd);
	}

	cmd.data0 = sc->intr_coal_delay = myri10ge_intr_coal_delay;
	status |= myri10ge_send_cmd(sc, 
				    MYRI10GE_MCP_CMD_SET_INTR_COAL_DELAY, &cmd);
	
	if (sc->msi_enabled) {
		status |= myri10ge_send_cmd
			(sc,  MYRI10GE_MCP_CMD_GET_IRQ_ACK_OFFSET, &cmd);
	} else {
		status |= myri10ge_send_cmd
			(sc,  MYRI10GE_MCP_CMD_GET_IRQ_ACK_DEASSERT_OFFSET, 
			 &cmd);
	}
	if (status != 0) {
		device_printf(sc->dev, "failed set interrupt parameters\n");
		return status;
	}
	sc->irq_claim = (volatile uint32_t *)(sc->sram + cmd.data0);

	/* reset mcp/driver shared state back to 0 */
	sc->intr.seqnum = 0;
	sc->intr.intrq = 0;
	sc->intr.slot = 0;
	sc->tx.req = 0;
	sc->tx.done = 0;
	sc->rx_big.cnt = 0;
	sc->rx_small.cnt = 0;
	sc->rdma_tags_available = 15;
	status = myri10ge_update_mac_address(sc);
	myri10ge_change_promisc(sc, 0);
	myri10ge_change_pause(sc, sc->pause);
	return status;
}

static int
myri10ge_change_intr_coal(SYSCTL_HANDLER_ARGS)
{
        myri10ge_cmd_t cmd;
        myri10ge_softc_t *sc;
        unsigned int intr_coal_delay;
        int err;

        sc = arg1;
        intr_coal_delay = sc->intr_coal_delay;
        err = sysctl_handle_int(oidp, &intr_coal_delay, arg2, req);
        if (err != 0) {
                return err;
        }
        if (intr_coal_delay == sc->intr_coal_delay)
                return 0;

        if (intr_coal_delay == 0 || intr_coal_delay > 1000*1000)
                return EINVAL;

	sx_xlock(&sc->driver_lock);
        cmd.data0 = intr_coal_delay;
        err = myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_SET_INTR_COAL_DELAY, 
				  &cmd);
        if (err == 0) {
		sc->intr_coal_delay = intr_coal_delay;
	}
	sx_xunlock(&sc->driver_lock);
        return err;
}

static int
myri10ge_change_flow_control(SYSCTL_HANDLER_ARGS)
{
        myri10ge_softc_t *sc;
        unsigned int enabled;
        int err;

        sc = arg1;
        enabled = sc->pause;
        err = sysctl_handle_int(oidp, &enabled, arg2, req);
        if (err != 0) {
                return err;
        }
        if (enabled == sc->pause)
                return 0;

	sx_xlock(&sc->driver_lock);
	err = myri10ge_change_pause(sc, enabled);
	sx_xunlock(&sc->driver_lock);
        return err;
}

static int
myri10ge_handle_be32(SYSCTL_HANDLER_ARGS)
{
        int err;

        if (arg1 == NULL)
                return EFAULT;
        arg2 = be32toh(*(int *)arg1);
        arg1 = NULL;
        err = sysctl_handle_int(oidp, arg1, arg2, req);

        return err;
}

static void
myri10ge_add_sysctls(myri10ge_softc_t *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;
	mcp_stats_t *fw;

	ctx = device_get_sysctl_ctx(sc->dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));
	fw = sc->fw_stats;

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"intr_coal_delay",
			CTLTYPE_INT|CTLFLAG_RW, sc,
			0, myri10ge_change_intr_coal, 
			"I", "interrupt coalescing delay in usecs");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"flow_control_enabled",
			CTLTYPE_INT|CTLFLAG_RW, sc,
			0, myri10ge_change_flow_control,
			"I", "interrupt coalescing delay in usecs");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "skip_pio_read",
		       CTLFLAG_RW, &myri10ge_skip_pio_read,
		       0, "Skip pio read in interrupt handler");

	/* stats block from firmware is in network byte order.  
	   Need to swap it */
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"link_up",
			CTLTYPE_INT|CTLFLAG_RD, &fw->link_up,
			0, myri10ge_handle_be32,
			"I", "link up");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"rdma_tags_available",
			CTLTYPE_INT|CTLFLAG_RD, &fw->rdma_tags_available,
			0, myri10ge_handle_be32,
			"I", "rdma_tags_available");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_link_overflow",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_link_overflow,
			0, myri10ge_handle_be32,
			"I", "dropped_link_overflow");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_link_error_or_filtered",
			CTLTYPE_INT|CTLFLAG_RD, 
			&fw->dropped_link_error_or_filtered,
			0, myri10ge_handle_be32,
			"I", "dropped_link_error_or_filtered");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_runt",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_runt,
			0, myri10ge_handle_be32,
			"I", "dropped_runt");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_overrun",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_overrun,
			0, myri10ge_handle_be32,
			"I", "dropped_overrun");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_no_small_buffer",
			CTLTYPE_INT|CTLFLAG_RD, 
			&fw->dropped_no_small_buffer,
			0, myri10ge_handle_be32,
			"I", "dropped_no_small_buffer");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_no_big_buffer",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_no_big_buffer,
			0, myri10ge_handle_be32,
			"I", "dropped_no_big_buffer");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_interrupt_busy",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_interrupt_busy,
			0, myri10ge_handle_be32,
			"I", "dropped_interrupt_busy");

	/* host counters exported for debugging */
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_req",
		       CTLFLAG_RD, &sc->tx.req,
		       0, "tx_req");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_done",
		       CTLFLAG_RD, &sc->tx.done,
		       0, "tx_done");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "rx_small_cnt",
		       CTLFLAG_RD, &sc->rx_small.cnt,
		       0, "rx_small_cnt");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "rx_big_cnt",
		       CTLFLAG_RD, &sc->rx_big.cnt,
		       0, "rx_small_cnt");

}

/* copy an array of mcp_kreq_ether_send_t's to the mcp.  Copy 
   backwards one at a time and handle ring wraps */

static inline void 
myri10ge_submit_req_backwards(myri10ge_tx_buf_t *tx, 
			    mcp_kreq_ether_send_t *src, int cnt)
{
        int idx, starting_slot;
        starting_slot = tx->req;
        while (cnt > 1) {
                cnt--;
                idx = (starting_slot + cnt) & tx->mask;
                myri10ge_pio_copy(&tx->lanai[idx],
				&src[cnt], sizeof(*src));
                mb();
        }
}

/*
 * copy an array of mcp_kreq_ether_send_t's to the mcp.  Copy
 * at most 32 bytes at a time, so as to avoid involving the software
 * pio handler in the nic.   We re-write the first segment's flags
 * to mark them valid only after writing the entire chain 
 */

static inline void 
myri10ge_submit_req(myri10ge_tx_buf_t *tx, mcp_kreq_ether_send_t *src, 
                  int cnt)
{
        int idx, i;
        uint32_t *src_ints;
	volatile uint32_t *dst_ints;
        mcp_kreq_ether_send_t *srcp;
	volatile mcp_kreq_ether_send_t *dstp, *dst;

        
        idx = tx->req & tx->mask;

        src->flags &= ~(htobe16(MYRI10GE_MCP_ETHER_FLAGS_VALID));
        mb();
        dst = dstp = &tx->lanai[idx];
        srcp = src;

        if ((idx + cnt) < tx->mask) {
                for (i = 0; i < (cnt - 1); i += 2) {
                        myri10ge_pio_copy(dstp, srcp, 2 * sizeof(*src));
                        mb(); /* force write every 32 bytes */
                        srcp += 2;
                        dstp += 2;
                }
        } else {
                /* submit all but the first request, and ensure 
                   that it is submitted below */
                myri10ge_submit_req_backwards(tx, src, cnt);
                i = 0;
        }
        if (i < cnt) {
                /* submit the first request */
                myri10ge_pio_copy(dstp, srcp, sizeof(*src));
                mb(); /* barrier before setting valid flag */
        }

        /* re-write the last 32-bits with the valid flags */
        src->flags |= htobe16(MYRI10GE_MCP_ETHER_FLAGS_VALID);
        src_ints = (uint32_t *)src;
        src_ints+=3;
        dst_ints = (volatile uint32_t *)dst;
        dst_ints+=3;
        *dst_ints =  *src_ints;
        tx->req += cnt;
        mb();
}

static inline void
myri10ge_submit_req_wc(myri10ge_tx_buf_t *tx,
		     mcp_kreq_ether_send_t *src, int cnt)
{
    tx->req += cnt;
    mb();
    while (cnt >= 4) {
	    myri10ge_pio_copy((volatile char *)tx->wc_fifo, src, 64);
	    mb();
	    src += 4;
	    cnt -= 4;
    }
    if (cnt > 0) {
	    /* pad it to 64 bytes.  The src is 64 bytes bigger than it
	       needs to be so that we don't overrun it */
	    myri10ge_pio_copy(tx->wc_fifo + (cnt<<18), src, 64);
	    mb();
    }
}

static void
myri10ge_encap(myri10ge_softc_t *sc, struct mbuf *m)
{
	mcp_kreq_ether_send_t *req;
	bus_dma_segment_t seg_list[MYRI10GE_MCP_ETHER_MAX_SEND_DESC];
	bus_dma_segment_t *seg;
	struct mbuf *m_tmp;
	struct ifnet *ifp;
	myri10ge_tx_buf_t *tx;
	struct ether_header *eh;
	struct ip *ip;
	int cnt, cum_len, err, i, idx;
	uint16_t flags, pseudo_hdr_offset;
        uint8_t cksum_offset;



	ifp = sc->ifp;
	tx = &sc->tx;

	/* (try to) map the frame for DMA */
	idx = tx->req & tx->mask;
	err = bus_dmamap_load_mbuf_sg(tx->dmat, tx->info[idx].map,
				      m, seg_list, &cnt, 
				      BUS_DMA_NOWAIT);
	if (err == EFBIG) {
		/* Too many segments in the chain.  Try
		   to defrag */
		m_tmp = m_defrag(m, M_NOWAIT);
		if (m_tmp == NULL) {
			goto drop;
		}
		m = m_tmp;
		err = bus_dmamap_load_mbuf_sg(tx->dmat, 
					      tx->info[idx].map,
					      m, seg_list, &cnt, 
					      BUS_DMA_NOWAIT);
	}
	if (err != 0) {
		device_printf(sc->dev, "bus_dmamap_load_mbuf_sg returned %d\n",
			      err);
		goto drop;
	}
	bus_dmamap_sync(tx->dmat, tx->info[idx].map,
			BUS_DMASYNC_PREWRITE);
	
	req = tx->req_list;
	cksum_offset = 0;
	flags = htobe16(MYRI10GE_MCP_ETHER_FLAGS_VALID | 
			MYRI10GE_MCP_ETHER_FLAGS_NOT_LAST);

	/* checksum offloading? */
	if (m->m_pkthdr.csum_flags & (CSUM_DELAY_DATA)) {
		eh = mtod(m, struct ether_header *);
		ip = (struct ip *) (eh + 1);
		cksum_offset = sizeof(*eh) + (ip->ip_hl << 2);
		pseudo_hdr_offset = cksum_offset +  m->m_pkthdr.csum_data;
		req->pseudo_hdr_offset = htobe16(pseudo_hdr_offset);
		req->cksum_offset = cksum_offset;
		flags |= htobe16(MYRI10GE_MCP_ETHER_FLAGS_CKSUM);
	}
	if (m->m_pkthdr.len < 512)
		req->flags = htobe16(MYRI10GE_MCP_ETHER_FLAGS_FIRST |
				     MYRI10GE_MCP_ETHER_FLAGS_SMALL);
	else
		req->flags = htobe16(MYRI10GE_MCP_ETHER_FLAGS_FIRST);

	/* convert segments into a request list */
	cum_len = 0;
	seg = seg_list;
	for (i = 0; i < cnt; i++) {
		req->addr_low = 
			htobe32(MYRI10GE_LOWPART_TO_U32(seg->ds_addr));
		req->addr_high = 
			htobe32(MYRI10GE_HIGHPART_TO_U32(seg->ds_addr));
		req->length = htobe16(seg->ds_len);
		req->cksum_offset = cksum_offset;
		if (cksum_offset > seg->ds_len)
			cksum_offset -= seg->ds_len;
		else
			cksum_offset = 0;
		req->flags |= flags | ((cum_len & 1) *
				       htobe16(MYRI10GE_MCP_ETHER_FLAGS_ALIGN_ODD));
		cum_len += seg->ds_len;
		seg++;
		req++;
		req->flags = 0;
	}
	req--;
	/* pad runts to 60 bytes */
	if (cum_len < 60) {
		req++;
		req->addr_low = 
			htobe32(MYRI10GE_LOWPART_TO_U32(sc->zeropad_dma.bus_addr));
		req->addr_high = 
			htobe32(MYRI10GE_HIGHPART_TO_U32(sc->zeropad_dma.bus_addr));
		req->length = htobe16(60 - cum_len);
		req->cksum_offset = cksum_offset;
		req->flags |= flags | ((cum_len & 1) * 
                                       htobe16(MYRI10GE_MCP_ETHER_FLAGS_ALIGN_ODD));
		cnt++;
	}
	req->flags &= ~(htobe16(MYRI10GE_MCP_ETHER_FLAGS_NOT_LAST));
	tx->info[idx].m = m;
	if (tx->wc_fifo == NULL)
		myri10ge_submit_req(tx, tx->req_list, cnt);
	else
		myri10ge_submit_req_wc(tx, tx->req_list, cnt);
	return;

drop:
	m_freem(m);
	ifp->if_oerrors++;
	return;
}


static void
myri10ge_start_locked(myri10ge_softc_t *sc)
{
	int avail;
	struct mbuf *m;
	struct ifnet *ifp;


	ifp = sc->ifp;
	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		 /* dequeue the packet */
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

		/* let BPF see it */
		BPF_MTAP(ifp, m);

		/* give it to the nic */
		myri10ge_encap(sc, m);

		/* leave an extra slot keep the ring from wrapping */
		avail = sc->tx.mask - (sc->tx.req - sc->tx.done);
		if (avail < MYRI10GE_MCP_ETHER_MAX_SEND_DESC) {
			sc->ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			return;
		}
	}
}

static void
myri10ge_start(struct ifnet *ifp)
{
	myri10ge_softc_t *sc = ifp->if_softc;


	mtx_lock(&sc->tx_lock);
	myri10ge_start_locked(sc);
	mtx_unlock(&sc->tx_lock);		
}

static int
myri10ge_get_buf_small(myri10ge_softc_t *sc, bus_dmamap_t map, int idx)
{
	bus_dma_segment_t seg;
	struct mbuf *m;
	myri10ge_rx_buf_t *rx = &sc->rx_small;
	int cnt, err;

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		rx->alloc_fail++;
		err = ENOBUFS;
		goto done;
	}
	m->m_len = MHLEN;
	err = bus_dmamap_load_mbuf_sg(rx->dmat, map, m, 
				      &seg, &cnt, BUS_DMA_NOWAIT);
	if (err != 0) {
		m_free(m);
		goto done;
	}
	rx->info[idx].m = m;
	rx->shadow[idx].addr_low = 
		htobe32(MYRI10GE_LOWPART_TO_U32(seg.ds_addr));
	rx->shadow[idx].addr_high = 
		htobe32(MYRI10GE_HIGHPART_TO_U32(seg.ds_addr));

done:
	if ((idx & 7) == 7) {
                myri10ge_pio_copy(&rx->lanai[idx - 7], 
				  &rx->shadow[idx - 7],
                                  8 * sizeof (*rx->lanai));
                mb();
        }
	return err;
}

static int
myri10ge_get_buf_big(myri10ge_softc_t *sc, bus_dmamap_t map, int idx)
{
	bus_dma_segment_t seg;
	struct mbuf *m;
	myri10ge_rx_buf_t *rx = &sc->rx_big;
	int cnt, err;

	m = m_getjcl(M_DONTWAIT, MT_DATA, M_PKTHDR, sc->big_bytes);
	if (m == NULL) {
		rx->alloc_fail++;
		err = ENOBUFS;
		goto done;
	}
	m->m_len = sc->big_bytes;
	err = bus_dmamap_load_mbuf_sg(rx->dmat, map, m, 
				      &seg, &cnt, BUS_DMA_NOWAIT);
	if (err != 0) {
		m_free(m);
		goto done;
	}
	rx->info[idx].m = m;
	rx->shadow[idx].addr_low = 
		htobe32(MYRI10GE_LOWPART_TO_U32(seg.ds_addr));
	rx->shadow[idx].addr_high = 
		htobe32(MYRI10GE_HIGHPART_TO_U32(seg.ds_addr));

done:
	if ((idx & 7) == 7) {
                myri10ge_pio_copy(&rx->lanai[idx - 7], 
				  &rx->shadow[idx - 7],
                                  8 * sizeof (*rx->lanai));
                mb();
        }
	return err;
}

static inline void 
myri10ge_rx_done_big(myri10ge_softc_t *sc, int len, int csum, int flags)
{
	struct ifnet *ifp;
	struct mbuf *m = 0; 		/* -Wunitialized */
	struct mbuf *m_prev = 0;	/* -Wunitialized */
	struct mbuf *m_head = 0;
	bus_dmamap_t old_map;
	myri10ge_rx_buf_t *rx;
	int idx;


	rx = &sc->rx_big;
	ifp = sc->ifp;
	while (len > 0) {
		idx = rx->cnt & rx->mask;
                rx->cnt++;
		/* save a pointer to the received mbuf */
		m = rx->info[idx].m;
		/* try to replace the received mbuf */
		if (myri10ge_get_buf_big(sc, rx->extra_map, idx)) {
			goto drop;
		}
		/* unmap the received buffer */
		old_map = rx->info[idx].map;
		bus_dmamap_sync(rx->dmat, old_map, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rx->dmat, old_map);

		/* swap the bus_dmamap_t's */
		rx->info[idx].map = rx->extra_map;
		rx->extra_map = old_map;

		/* chain multiple segments together */
		if (!m_head) {
			m_head = m;
			/* mcp implicitly skips 1st bytes so that
			 * packet is properly aligned */
			m->m_data += MYRI10GE_MCP_ETHER_PAD;
			m->m_pkthdr.len = len;
			m->m_len = sc->big_bytes - MYRI10GE_MCP_ETHER_PAD;
		} else {
			m->m_len = sc->big_bytes;
			m->m_flags &= ~M_PKTHDR;
			m_prev->m_next = m;
		}
		len -= m->m_len;
		m_prev = m;
	}

	/* trim trailing garbage from the last mbuf in the chain.  If
	 * there is any garbage, len will be negative */
	m->m_len += len;

	/* if the checksum is valid, mark it in the mbuf header */
	if (sc->csum_flag & flags) {
		m_head->m_pkthdr.csum_data = csum;
		m_head->m_pkthdr.csum_flags = CSUM_DATA_VALID;
	}
	
	/* pass the frame up the stack */
	m_head->m_pkthdr.rcvif = ifp;
	ifp->if_ipackets++;
	(*ifp->if_input)(ifp, m_head);
	return;

drop:
	/* drop the frame -- the old mbuf(s) are re-cycled by running
	   every slot through the allocator */
        if (m_head) {
                len -= sc->big_bytes;
                m_freem(m_head);
        } else {
                len -= (sc->big_bytes + MYRI10GE_MCP_ETHER_PAD);
        }
        while ((int)len > 0) {
                idx = rx->cnt & rx->mask;
                rx->cnt++;
                m = rx->info[idx].m;
                if (0 == (myri10ge_get_buf_big(sc, rx->extra_map, idx))) {
			m_freem(m);
			/* unmap the received buffer */
			old_map = rx->info[idx].map;
			bus_dmamap_sync(rx->dmat, old_map, 
					BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rx->dmat, old_map);

			/* swap the bus_dmamap_t's */
			rx->info[idx].map = rx->extra_map;
			rx->extra_map = old_map;
		}
                len -= sc->big_bytes;
        }

	ifp->if_ierrors++;

}


static inline void
myri10ge_rx_done_small(myri10ge_softc_t *sc, uint32_t len, 
                       uint32_t csum, uint32_t flags)
{
	struct ifnet *ifp;
	struct mbuf *m;
	myri10ge_rx_buf_t *rx;
	bus_dmamap_t old_map;
	int idx;

	ifp = sc->ifp;
	rx = &sc->rx_small;
	idx = rx->cnt & rx->mask;
	rx->cnt++;
	/* save a pointer to the received mbuf */
	m = rx->info[idx].m;
	/* try to replace the received mbuf */
	if (myri10ge_get_buf_small(sc, rx->extra_map, idx)) {
		/* drop the frame -- the old mbuf is re-cycled */
		ifp->if_ierrors++;
		return;
	}

	/* unmap the received buffer */
	old_map = rx->info[idx].map;
	bus_dmamap_sync(rx->dmat, old_map, BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(rx->dmat, old_map);

	/* swap the bus_dmamap_t's */
	rx->info[idx].map = rx->extra_map;
	rx->extra_map = old_map;

	/* mcp implicitly skips 1st 2 bytes so that packet is properly
	 * aligned */
	m->m_data += MYRI10GE_MCP_ETHER_PAD;

	/* if the checksum is valid, mark it in the mbuf header */
	if (sc->csum_flag & flags) {
		m->m_pkthdr.csum_data = csum;
		m->m_pkthdr.csum_flags = CSUM_DATA_VALID;
	}

	/* pass the frame up the stack */
	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len = len;
	ifp->if_ipackets++;
	(*ifp->if_input)(ifp, m);
}

static inline void
myri10ge_tx_done(myri10ge_softc_t *sc, uint32_t mcp_idx)
{
	struct ifnet *ifp;
	myri10ge_tx_buf_t *tx;
	struct mbuf *m;
	bus_dmamap_t map;
	int idx;

	tx = &sc->tx;
	ifp = sc->ifp;
	while (tx->done != mcp_idx) {
		idx = tx->done & tx->mask;
		tx->done++;
		m = tx->info[idx].m;
		/* mbuf and DMA map only attached to the first
		   segment per-mbuf */
		if (m != NULL) {
			ifp->if_opackets++;
			tx->info[idx].m = NULL;
			map = tx->info[idx].map;
			bus_dmamap_unload(tx->dmat, map);
			m_freem(m);
		}
	}
	
	/* If we have space, clear IFF_OACTIVE to tell the stack that
           its OK to send packets */

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE &&
	    tx->req - tx->done < (tx->mask + 1)/4) {
		mtx_lock(&sc->tx_lock);
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		myri10ge_start_locked(sc);
		mtx_unlock(&sc->tx_lock);
	}
}

static void
myri10ge_dump_interrupt_queues(myri10ge_softc_t *sc, int maxslot)
{
  int intrq, slot, type;
  static int call_cnt = 0;

  /* only do it a few times to avoid filling the message buffer */
  if (call_cnt > 10)
    return;

  call_cnt++;

  device_printf(sc->dev, "--------- Dumping interrupt queue state ----- \n");
  device_printf(sc->dev, "currently expecting interrupts on queue %d\n", 
		sc->intr.intrq);
  device_printf(sc->dev, " q  slot  status \n");
  device_printf(sc->dev, "--- ---- -------- \n");
  for (intrq = 0; intrq < 2; intrq++) {
	  for (slot = 0; slot <= maxslot; slot++) {
      type = sc->intr.q[intrq][slot].type;
#if 0
      if (type == 0 && slot != 0)
        continue;
#endif
      device_printf(sc->dev, "[%d]:[%d]: type   = 0x%x\n", intrq, slot,
		    type);
      device_printf(sc->dev, "[%d]:[%d]: flag    = 0x%x\n", intrq, slot,
                sc->intr.q[intrq][slot].flag);
      device_printf(sc->dev, "[%d]:[%d]: index  = 0x%x\n", intrq, slot,
                be16toh(sc->intr.q[intrq][slot].index));
      device_printf(sc->dev, "[%d]:[%d]: seqnum = 0x%x\n", intrq, slot, 
                (unsigned int)be32toh(sc->intr.q[intrq][slot].seqnum));
      device_printf(sc->dev, "[%d]:[%d]: data0  = 0x%x\n", intrq, slot,
                (unsigned int)be32toh(sc->intr.q[intrq][slot].data0));
      device_printf(sc->dev, "[%d]:[%d]: data1  = 0x%x\n", intrq, slot,
                (unsigned int)be32toh(sc->intr.q[intrq][slot].data1));
      
    }
  }

}

static inline void
myri10ge_claim_irq(myri10ge_softc_t *sc)
{
	volatile uint32_t dontcare;


	*sc->irq_claim = 0;
	mb();

	/* do a PIO read to ensure that PIO write to claim the irq has
	   hit the nic before we exit the interrupt handler */
	if (!myri10ge_skip_pio_read) {
		dontcare = *(volatile uint32_t *)sc->sram;
		mb();
	}
}

static void
myri10ge_intr(void *arg)
{
	myri10ge_softc_t *sc = arg;
	int intrq, claimed, flags, count, length, ip_csum;
        uint32_t raw, slot;
	uint8_t type;


	intrq = sc->intr.intrq;
	claimed = 0;
	bus_dmamap_sync(sc->intr.dma[intrq].dmat, 
			sc->intr.dma[intrq].map, BUS_DMASYNC_POSTREAD);
	if (sc->msi_enabled) {
		/* We know we can immediately claim the interrupt */
		myri10ge_claim_irq(sc);
		claimed = 1;
	} else {
		/* Check to see if we have the last event in the queue
		   ready.  If so, ack it as early as possible.  This
		   allows more time to get the interrupt line
		   de-asserted prior to the EOI and reduces the chance
		   of seeing a spurious irq caused by the interrupt
		   line remaining high after EOI */

		slot = be16toh(sc->intr.q[intrq][0].index) - 1;
		if (slot < myri10ge_max_intr_slots && 
		    sc->intr.q[intrq][slot].type  != 0 &&
		    sc->intr.q[intrq][slot].flag != 0) {
			myri10ge_claim_irq(sc);
			claimed = 1;
		} 
	}

	/* walk each slot in the current queue, processing events until
	   we reach an event with a zero type */
	for (slot = sc->intr.slot; slot < myri10ge_max_intr_slots; slot++) {
		type = sc->intr.q[intrq][slot].type;

		/* check for partially completed DMA of events when
		   using non-MSI interrupts */
		if (__predict_false(!claimed)) {
			mb();
			/* look if there is somscing in the queue */
			if (type == 0) {
				/* save the current slot for the next
				 * time we (re-)enter this routine */
				if (sc->intr.slot == slot) {
					sc->intr.spurious++;
				}
				sc->intr.slot = slot;
				return;
			}
		}
		if (__predict_false(htobe32(sc->intr.q[intrq][slot].seqnum) != 
			     sc->intr.seqnum++)) {
			device_printf(sc->dev, "Bad interrupt!\n");
			device_printf(sc->dev, 
				      "bad irq seqno" 
				      "(got 0x%x, expected 0x%x) \n", 
				      (unsigned int)htobe32(sc->intr.q[intrq][slot].seqnum), 
				      sc->intr.seqnum);
			device_printf(sc->dev, "intrq = %d, slot = %d\n",
				      intrq, slot);
			myri10ge_dump_interrupt_queues(sc, slot);
			device_printf(sc->dev, 
				      "Disabling futher interrupt handling\n");
			bus_teardown_intr(sc->dev, sc->irq_res, 
					  sc->ih);
			sc->ih = NULL;
			return;
		}

		switch (type) {
		case MYRI10GE_MCP_INTR_ETHER_SEND_DONE:
			myri10ge_tx_done(sc, be32toh(sc->intr.q[intrq][slot].data0));

			if (__predict_true(sc->intr.q[intrq][slot].data1 == 0))
				break;

			/* check the link state.  Don't bother to
			 * byteswap, since it can just be 0 or 1 */
			if (sc->link_state != sc->fw_stats->link_up) {
				sc->link_state = sc->fw_stats->link_up;
				if (sc->link_state) {
					if_link_state_change(sc->ifp, 
							     LINK_STATE_UP);
					device_printf(sc->dev,
						      "link up\n");
				} else {
					if_link_state_change(sc->ifp, 
							     LINK_STATE_DOWN);
					device_printf(sc->dev,
						      "link down\n");
				}
			}
			if (sc->rdma_tags_available != 
			    be32toh(sc->fw_stats->rdma_tags_available)) {
				sc->rdma_tags_available = 
					be32toh(sc->fw_stats->rdma_tags_available);
				device_printf(sc->dev, "RDMA timed out!"
					      " %d tags left\n",
					      sc->rdma_tags_available);
			}

			break;


		case MYRI10GE_MCP_INTR_ETHER_RECV_SMALL:
			raw = be32toh(sc->intr.q[intrq][slot].data0);
			count = 0xff & raw;
			flags = raw >> 8;
			raw = be32toh(sc->intr.q[intrq][slot].data1);
			ip_csum = raw >> 16;
			length = 0xffff & raw;
			myri10ge_rx_done_small(sc, length, ip_csum, 
					       flags);
			break;

		case MYRI10GE_MCP_INTR_ETHER_RECV_BIG:
			raw = be32toh(sc->intr.q[intrq][slot].data0);
			count = 0xff & raw;
			flags = raw >> 8;
			raw = be32toh(sc->intr.q[intrq][slot].data1);
			ip_csum = raw >> 16;
			length = 0xffff & raw;
			myri10ge_rx_done_big(sc, length, ip_csum, 
					     flags);

			break;

		case MYRI10GE_MCP_INTR_LINK_CHANGE:
			/* not yet implemented in firmware */
			break;

		case MYRI10GE_MCP_INTR_ETHER_DOWN:
			sc->down_cnt++;
			wakeup(&sc->down_cnt);
			break;

		default:
			device_printf(sc->dev, "Unknown interrupt type %d\n",
				      type);
		}
		sc->intr.q[intrq][slot].type = 0;
		if (sc->intr.q[intrq][slot].flag != 0) {
			if (!claimed) {
				myri10ge_claim_irq(sc);
			}
			sc->intr.slot = 0;
			sc->intr.q[intrq][slot].flag = 0;
			sc->intr.intrq = ((intrq + 1) & 1);
			return;
		}
	}

	/* we should never be here unless we're on a shared irq and we have
	   not finished setting up the device */
	return;
}

static void
myri10ge_watchdog(struct ifnet *ifp)
{
	printf("%s called\n", __FUNCTION__);
}

static void
myri10ge_init(void *arg)
{
}



static void
myri10ge_free_mbufs(myri10ge_softc_t *sc)
{
	int i;

	for (i = 0; i <= sc->rx_big.mask; i++) {
		if (sc->rx_big.info[i].m == NULL)
			continue;
		bus_dmamap_unload(sc->rx_big.dmat,
				  sc->rx_big.info[i].map);
		m_freem(sc->rx_big.info[i].m);
		sc->rx_big.info[i].m = NULL;
	}

	for (i = 0; i <= sc->rx_big.mask; i++) {
		if (sc->rx_big.info[i].m == NULL)
			continue;
		bus_dmamap_unload(sc->rx_big.dmat,
				  sc->rx_big.info[i].map);
		m_freem(sc->rx_big.info[i].m);
		sc->rx_big.info[i].m = NULL;
	}

	for (i = 0; i <= sc->tx.mask; i++) {
		if (sc->tx.info[i].m == NULL)
			continue;
		bus_dmamap_unload(sc->tx.dmat,
				  sc->tx.info[i].map);
		m_freem(sc->tx.info[i].m);
		sc->tx.info[i].m = NULL;
	}
}

static void
myri10ge_free_rings(myri10ge_softc_t *sc)
{
	int i;

	if (sc->tx.req_bytes != NULL) {
		free(sc->tx.req_bytes, M_DEVBUF);
	}
	if (sc->rx_small.shadow != NULL)
		free(sc->rx_small.shadow, M_DEVBUF);
	if (sc->rx_big.shadow != NULL)
		free(sc->rx_big.shadow, M_DEVBUF);
	if (sc->tx.info != NULL) {
		for (i = 0; i <= sc->tx.mask; i++) {
			if (sc->tx.info[i].map != NULL) 
				bus_dmamap_destroy(sc->tx.dmat,
						   sc->tx.info[i].map);
		}
		free(sc->tx.info, M_DEVBUF);
	}
	if (sc->rx_small.info != NULL) {
		for (i = 0; i <= sc->rx_small.mask; i++) {
			if (sc->rx_small.info[i].map != NULL) 
				bus_dmamap_destroy(sc->rx_small.dmat,
						   sc->rx_small.info[i].map);
		}
		free(sc->rx_small.info, M_DEVBUF);
	}
	if (sc->rx_big.info != NULL) {
		for (i = 0; i <= sc->rx_big.mask; i++) {
			if (sc->rx_big.info[i].map != NULL) 
				bus_dmamap_destroy(sc->rx_big.dmat,
						   sc->rx_big.info[i].map);
		}
		free(sc->rx_big.info, M_DEVBUF);
	}
	if (sc->rx_big.extra_map != NULL)
		bus_dmamap_destroy(sc->rx_big.dmat,
				   sc->rx_big.extra_map);
	if (sc->rx_small.extra_map != NULL)
		bus_dmamap_destroy(sc->rx_small.dmat,
				   sc->rx_small.extra_map);
	if (sc->tx.dmat != NULL) 
		bus_dma_tag_destroy(sc->tx.dmat);
	if (sc->rx_small.dmat != NULL) 
		bus_dma_tag_destroy(sc->rx_small.dmat);
	if (sc->rx_big.dmat != NULL) 
		bus_dma_tag_destroy(sc->rx_big.dmat);
}

static int
myri10ge_alloc_rings(myri10ge_softc_t *sc)
{
	myri10ge_cmd_t cmd;
	int tx_ring_size, rx_ring_size;
	int tx_ring_entries, rx_ring_entries;
	int i, err;
	unsigned long bytes;
	
	/* get ring sizes */
	err = myri10ge_send_cmd(sc, 
				MYRI10GE_MCP_CMD_GET_SEND_RING_SIZE,
				&cmd);
	tx_ring_size = cmd.data0;
	err |= myri10ge_send_cmd(sc, 
				 MYRI10GE_MCP_CMD_GET_RX_RING_SIZE, 
				 &cmd);
	if (err != 0) {
		device_printf(sc->dev, "Cannot determine ring sizes\n");
		goto abort_with_nothing;
	}

	rx_ring_size = cmd.data0;

	tx_ring_entries = tx_ring_size / sizeof (mcp_kreq_ether_send_t);
	rx_ring_entries = rx_ring_size / sizeof (mcp_dma_addr_t);
	sc->ifp->if_snd.ifq_maxlen = tx_ring_entries - 1;
	sc->ifp->if_snd.ifq_drv_maxlen = sc->ifp->if_snd.ifq_maxlen;

	sc->tx.mask = tx_ring_entries - 1;
	sc->rx_small.mask = sc->rx_big.mask = rx_ring_entries - 1;

	err = ENOMEM;

	/* allocate the tx request copy block */
	bytes = 8 + 
		sizeof (*sc->tx.req_list) * (MYRI10GE_MCP_ETHER_MAX_SEND_DESC + 4);
	sc->tx.req_bytes = malloc(bytes, M_DEVBUF, M_WAITOK);
	if (sc->tx.req_bytes == NULL)
		goto abort_with_nothing;
	/* ensure req_list entries are aligned to 8 bytes */
	sc->tx.req_list = (mcp_kreq_ether_send_t *)
		((unsigned long)(sc->tx.req_bytes + 7) & ~7UL);

	/* allocate the rx shadow rings */
	bytes = rx_ring_entries * sizeof (*sc->rx_small.shadow);
	sc->rx_small.shadow = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (sc->rx_small.shadow == NULL)
		goto abort_with_alloc;

	bytes = rx_ring_entries * sizeof (*sc->rx_big.shadow);
	sc->rx_big.shadow = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (sc->rx_big.shadow == NULL)
		goto abort_with_alloc;

	/* allocate the host info rings */
	bytes = tx_ring_entries * sizeof (*sc->tx.info);
	sc->tx.info = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (sc->tx.info == NULL)
		goto abort_with_alloc;
	
	bytes = rx_ring_entries * sizeof (*sc->rx_small.info);
	sc->rx_small.info = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (sc->rx_small.info == NULL)
		goto abort_with_alloc;

	bytes = rx_ring_entries * sizeof (*sc->rx_big.info);
	sc->rx_big.info = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (sc->rx_big.info == NULL)
		goto abort_with_alloc;

	/* allocate the busdma resources */
	err = bus_dma_tag_create(sc->parent_dmat,	/* parent */
				 1,			/* alignment */
				 sc->tx.boundary,	/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 MYRI10GE_MAX_ETHER_MTU,/* maxsize */
				 MYRI10GE_MCP_ETHER_MAX_SEND_DESC,/* num segs */
				 sc->tx.boundary,	/* maxsegsize */
				 BUS_DMA_ALLOCNOW,	/* flags */
				 NULL, NULL,		/* lock */
				 &sc->tx.dmat);		/* tag */
	
	if (err != 0) {
		device_printf(sc->dev, "Err %d allocating tx dmat\n",
			      err);
		goto abort_with_alloc;
	}

	err = bus_dma_tag_create(sc->parent_dmat,	/* parent */
				 1,			/* alignment */
				 4096,			/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 MHLEN,			/* maxsize */
				 1,			/* num segs */
				 MHLEN,			/* maxsegsize */
				 BUS_DMA_ALLOCNOW,	/* flags */
				 NULL, NULL,		/* lock */
				 &sc->rx_small.dmat);	/* tag */
	if (err != 0) {
		device_printf(sc->dev, "Err %d allocating rx_small dmat\n",
			      err);
		goto abort_with_alloc;
	}

	err = bus_dma_tag_create(sc->parent_dmat,	/* parent */
				 1,			/* alignment */
				 4096,			/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 4096,			/* maxsize */
				 1,			/* num segs */
				 4096,			/* maxsegsize */
				 BUS_DMA_ALLOCNOW,	/* flags */
				 NULL, NULL,		/* lock */
				 &sc->rx_big.dmat);	/* tag */
	if (err != 0) {
		device_printf(sc->dev, "Err %d allocating rx_big dmat\n",
			      err);
		goto abort_with_alloc;
	}

	/* now use these tags to setup dmamaps for each slot
	   in each ring */
	for (i = 0; i <= sc->tx.mask; i++) {
		err = bus_dmamap_create(sc->tx.dmat, 0, 
					&sc->tx.info[i].map);
		if (err != 0) {
			device_printf(sc->dev, "Err %d  tx dmamap\n",
			      err);
			goto abort_with_alloc;
		}
	}
	for (i = 0; i <= sc->rx_small.mask; i++) {
		err = bus_dmamap_create(sc->rx_small.dmat, 0, 
					&sc->rx_small.info[i].map);
		if (err != 0) {
			device_printf(sc->dev, "Err %d  rx_small dmamap\n",
			      err);
			goto abort_with_alloc;
		}
	}
	err = bus_dmamap_create(sc->rx_small.dmat, 0, 
				&sc->rx_small.extra_map);
	if (err != 0) {
		device_printf(sc->dev, "Err %d extra rx_small dmamap\n",
			      err);
			goto abort_with_alloc;
	}

	for (i = 0; i <= sc->rx_big.mask; i++) {
		err = bus_dmamap_create(sc->rx_big.dmat, 0, 
					&sc->rx_big.info[i].map);
		if (err != 0) {
			device_printf(sc->dev, "Err %d  rx_big dmamap\n",
			      err);
			goto abort_with_alloc;
		}
	}
	err = bus_dmamap_create(sc->rx_big.dmat, 0, 
				&sc->rx_big.extra_map);
	if (err != 0) {
		device_printf(sc->dev, "Err %d extra rx_big dmamap\n",
			      err);
			goto abort_with_alloc;
	}
	return 0;

abort_with_alloc:
	myri10ge_free_rings(sc);

abort_with_nothing:
	return err;
}

static int 
myri10ge_open(myri10ge_softc_t *sc)
{
	myri10ge_cmd_t cmd;
	int i, err;
	bus_dmamap_t map;


	err = myri10ge_reset(sc);
	if (err != 0) {
		device_printf(sc->dev, "failed to reset\n");
		return EIO;
	}

	if (MCLBYTES >= 
	    sc->ifp->if_mtu + ETHER_HDR_LEN + MYRI10GE_MCP_ETHER_PAD)
		sc->big_bytes = MCLBYTES;
	else
		sc->big_bytes = MJUMPAGESIZE;

	err = myri10ge_alloc_rings(sc);
	if (err != 0) {
		device_printf(sc->dev, "failed to allocate rings\n");
		return err;
	}

	err = bus_setup_intr(sc->dev, sc->irq_res, 
			     INTR_TYPE_NET | INTR_MPSAFE,
			     myri10ge_intr, sc, &sc->ih);
	if (err != 0) {
		goto abort_with_rings;
	}

	/* get the lanai pointers to the send and receive rings */

	err = myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_GET_SEND_OFFSET, &cmd);
	sc->tx.lanai = 
		(volatile mcp_kreq_ether_send_t *)(sc->sram + cmd.data0);
	err |= myri10ge_send_cmd(sc, 
				 MYRI10GE_MCP_CMD_GET_SMALL_RX_OFFSET, &cmd);
	sc->rx_small.lanai = 
		(volatile mcp_kreq_ether_recv_t *)(sc->sram + cmd.data0);
	err |= myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_GET_BIG_RX_OFFSET, &cmd);
	sc->rx_big.lanai = 
		(volatile mcp_kreq_ether_recv_t *)(sc->sram + cmd.data0);

	if (err != 0) {
		device_printf(sc->dev, 
			      "failed to get ring sizes or locations\n");
		err = EIO;
		goto abort_with_irq;
	}

	if (sc->wc) {
		sc->tx.wc_fifo = sc->sram + 0x200000;
		sc->rx_small.wc_fifo = sc->sram + 0x300000;
		sc->rx_big.wc_fifo = sc->sram + 0x340000;
	} else {
		sc->tx.wc_fifo = 0;
		sc->rx_small.wc_fifo = 0;
		sc->rx_big.wc_fifo = 0;
	}
	

	/* stock receive rings */
	for (i = 0; i <= sc->rx_small.mask; i++) {
		map = sc->rx_small.info[i].map;
		err = myri10ge_get_buf_small(sc, map, i);
		if (err) {
			device_printf(sc->dev, "alloced %d/%d smalls\n",
				      i, sc->rx_small.mask + 1);
			goto abort;
		}
	}
	for (i = 0; i <= sc->rx_big.mask; i++) {
		map = sc->rx_big.info[i].map;
		err = myri10ge_get_buf_big(sc, map, i);
		if (err) {
			device_printf(sc->dev, "alloced %d/%d bigs\n",
				      i, sc->rx_big.mask + 1);
			goto abort;
		}
	}

	/* Give the firmware the mtu and the big and small buffer
	   sizes.  The firmware wants the big buf size to be a power
	   of two. Luckily, FreeBSD's clusters are powers of two */
	cmd.data0 = sc->ifp->if_mtu + ETHER_HDR_LEN;
	err = myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_SET_MTU, &cmd);
	cmd.data0 = MHLEN;
	err |= myri10ge_send_cmd(sc, 
				 MYRI10GE_MCP_CMD_SET_SMALL_BUFFER_SIZE,
				 &cmd);
	cmd.data0 = sc->big_bytes;
	err  |= myri10ge_send_cmd(sc, 
				  MYRI10GE_MCP_CMD_SET_BIG_BUFFER_SIZE, 
				  &cmd);
	/* Now give him the pointer to the stats block */
	cmd.data0 = MYRI10GE_LOWPART_TO_U32(sc->fw_stats_dma.bus_addr);
	cmd.data1 = MYRI10GE_HIGHPART_TO_U32(sc->fw_stats_dma.bus_addr);
	err = myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_SET_STATS_DMA, &cmd);

	if (err != 0) {
		device_printf(sc->dev, "failed to setup params\n");
		goto abort;
	}

	/* Finally, start the firmware running */
	err = myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_ETHERNET_UP, &cmd);
	if (err) {
		device_printf(sc->dev, "Couldn't bring up link\n");
		goto abort;
	}
	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return 0;


abort:
	myri10ge_free_mbufs(sc);
abort_with_irq:
	bus_teardown_intr(sc->dev, sc->irq_res, sc->ih);
abort_with_rings:
	myri10ge_free_rings(sc);
	return err;
}

static int
myri10ge_close(myri10ge_softc_t *sc)
{
	myri10ge_cmd_t cmd;
	int err, old_down_cnt;

	sc->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	old_down_cnt = sc->down_cnt;
	mb();
	err = myri10ge_send_cmd(sc, MYRI10GE_MCP_CMD_ETHERNET_DOWN, &cmd);
	if (err) {
		device_printf(sc->dev, "Couldn't bring down link\n");
	}
	if (old_down_cnt == sc->down_cnt) {
		/* wait for down irq */
		(void)tsleep(&sc->down_cnt, PWAIT, "down myri10ge", hz);
	}
	if (old_down_cnt == sc->down_cnt) {
		device_printf(sc->dev, "never got down irq\n");
	}
	if (sc->ih != NULL)
		bus_teardown_intr(sc->dev, sc->irq_res, sc->ih);
	myri10ge_free_mbufs(sc);
	myri10ge_free_rings(sc);
	return 0;
}


static int
myri10ge_media_change(struct ifnet *ifp)
{
	return EINVAL;
}

static int
myri10ge_change_mtu(myri10ge_softc_t *sc, int mtu)
{
	struct ifnet *ifp = sc->ifp;
	int real_mtu, old_mtu;
	int err = 0;


	real_mtu = mtu + ETHER_HDR_LEN;
	if ((real_mtu > MYRI10GE_MAX_ETHER_MTU) ||
	    real_mtu < 60)
		return EINVAL;
	sx_xlock(&sc->driver_lock);
	old_mtu = ifp->if_mtu;
	ifp->if_mtu = mtu;
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		myri10ge_close(sc);
		err = myri10ge_open(sc);
		if (err != 0) {
			ifp->if_mtu = old_mtu;
			myri10ge_close(sc);
			(void) myri10ge_open(sc);
		}
	}
	sx_xunlock(&sc->driver_lock);
	return err;
}	

static void
myri10ge_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	myri10ge_softc_t *sc = ifp->if_softc;
	

	if (sc == NULL)
		return;
	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_status |= sc->fw_stats->link_up ? IFM_ACTIVE : 0;
	ifmr->ifm_active = IFM_AUTO | IFM_ETHER;
	ifmr->ifm_active |= sc->fw_stats->link_up ? IFM_FDX : 0;
}

static int
myri10ge_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	myri10ge_softc_t *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int err, mask;

	err = 0;
	switch (command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		err = ether_ioctl(ifp, command, data);
		break;

	case SIOCSIFMTU:
		err = myri10ge_change_mtu(sc, ifr->ifr_mtu);
		break;

	case SIOCSIFFLAGS:
		sx_xlock(&sc->driver_lock);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				err = myri10ge_open(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				myri10ge_close(sc);
		}
		sx_xunlock(&sc->driver_lock);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		err = 0;
		break;

	case SIOCSIFCAP:
		sx_xlock(&sc->driver_lock);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_TXCSUM) {
			if (IFCAP_TXCSUM & ifp->if_capenable) {
				ifp->if_capenable &= ~IFCAP_TXCSUM;
				ifp->if_hwassist &= ~(CSUM_TCP | CSUM_UDP);
			} else {
				ifp->if_capenable |= IFCAP_TXCSUM;
				ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
			}
		} else if (mask & IFCAP_RXCSUM) {
			if (IFCAP_RXCSUM & ifp->if_capenable) {
				ifp->if_capenable &= ~IFCAP_RXCSUM;
				sc->csum_flag &= ~MYRI10GE_MCP_ETHER_FLAGS_CKSUM;
			} else {
				ifp->if_capenable |= IFCAP_RXCSUM;
				sc->csum_flag |= MYRI10GE_MCP_ETHER_FLAGS_CKSUM;
			}
		}
		sx_xunlock(&sc->driver_lock);
		break;

	case SIOCGIFMEDIA:
		err = ifmedia_ioctl(ifp, (struct ifreq *)data, 
				    &sc->media, command);
                break;

	default:
		err = ENOTTY;
        }
	return err;
}

static void
myri10ge_fetch_tunables(myri10ge_softc_t *sc)
{
	
	TUNABLE_INT_FETCH("hw.myri10ge.flow_control_enabled", 
			  &myri10ge_flow_control);
	TUNABLE_INT_FETCH("hw.myri10ge.intr_coal_delay", 
			  &myri10ge_intr_coal_delay);	
	TUNABLE_INT_FETCH("hw.myri10ge.nvidia_ecrc_enable", 
			  &myri10ge_nvidia_ecrc_enable);	
	TUNABLE_INT_FETCH("hw.myri10ge.skip_pio_read", 
			  &myri10ge_skip_pio_read);	

	if (myri10ge_intr_coal_delay < 0 || 
	    myri10ge_intr_coal_delay > 10*1000)
		myri10ge_intr_coal_delay = 30;
	sc->pause = myri10ge_flow_control;
}

static int 
myri10ge_attach(device_t dev)
{
	myri10ge_softc_t *sc = device_get_softc(dev);
	struct ifnet *ifp;
	size_t bytes;
	int rid, err, i;
	uint16_t cmd;

	sc->dev = dev;
	myri10ge_fetch_tunables(sc);

	err = bus_dma_tag_create(NULL,			/* parent */
				 1,			/* alignment */
				 4096,			/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 MYRI10GE_MAX_ETHER_MTU,/* maxsize */
				 MYRI10GE_MCP_ETHER_MAX_SEND_DESC, /* num segs */
				 4096,			/* maxsegsize */
				 0,			/* flags */
				 NULL, NULL,		/* lock */
				 &sc->parent_dmat);	/* tag */

	if (err != 0) {
		device_printf(sc->dev, "Err %d allocating parent dmat\n",
			      err);
		goto abort_with_nothing;
	}

	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		err = ENOSPC;
		goto abort_with_parent_dmat;
	}
	mtx_init(&sc->cmd_lock, NULL,
		 MTX_NETWORK_LOCK, MTX_DEF);
	mtx_init(&sc->tx_lock, device_get_nameunit(dev),
		 MTX_NETWORK_LOCK, MTX_DEF);
	sx_init(&sc->driver_lock, device_get_nameunit(dev));

	/* Enable DMA and Memory space access */
	pci_enable_busmaster(dev);
	cmd = pci_read_config(dev, PCIR_COMMAND, 2);
	cmd |= PCIM_CMD_MEMEN;
	pci_write_config(dev, PCIR_COMMAND, cmd, 2);

	/* Map the board into the kernel */
	rid = PCIR_BARS;
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0,
					 ~0, 1, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not map memory\n");
		err = ENXIO;
		goto abort_with_lock;
	}
	sc->sram = rman_get_virtual(sc->mem_res);
	sc->sram_size = 2*1024*1024 - (2*(48*1024)+(32*1024)) - 0x100;
	if (sc->sram_size > rman_get_size(sc->mem_res)) {
		device_printf(dev, "impossible memory region size %ld\n",
			      rman_get_size(sc->mem_res));
		err = ENXIO;
		goto abort_with_mem_res;
	}

	/* make NULL terminated copy of the EEPROM strings section of
	   lanai SRAM */
	bzero(sc->eeprom_strings, MYRI10GE_EEPROM_STRINGS_SIZE);
	bus_space_read_region_1(rman_get_bustag(sc->mem_res),
				rman_get_bushandle(sc->mem_res),
				sc->sram_size - MYRI10GE_EEPROM_STRINGS_SIZE,
				sc->eeprom_strings, 
				MYRI10GE_EEPROM_STRINGS_SIZE - 2);
	err = myri10ge_parse_strings(sc);
	if (err != 0)
		goto abort_with_mem_res;

	/* Enable write combining for efficient use of PCIe bus */
	myri10ge_enable_wc(sc);

	/* Allocate the out of band dma memory */
	err = myri10ge_dma_alloc(sc, &sc->cmd_dma, 
				 sizeof (myri10ge_cmd_t), 64);
	if (err != 0) 
		goto abort_with_mem_res;
	sc->cmd = (mcp_cmd_response_t *) sc->cmd_dma.addr;
	err = myri10ge_dma_alloc(sc, &sc->zeropad_dma, 64, 64);
	if (err != 0) 
		goto abort_with_cmd_dma;

	err = myri10ge_dma_alloc(sc, &sc->fw_stats_dma, 
				 sizeof (*sc->fw_stats), 64);
	if (err != 0) 
		goto abort_with_zeropad_dma;
	sc->fw_stats = (mcp_stats_t *)sc->fw_stats_dma.addr;


	/* allocate interrupt queues */
	bytes = myri10ge_max_intr_slots * sizeof (*sc->intr.q[0]);
	for (i = 0; i < MYRI10GE_NUM_INTRQS; i++) {
		err = myri10ge_dma_alloc(sc, &sc->intr.dma[i],
					 bytes, 4096);
		if (err != 0)
			goto abort_with_intrq;
		sc->intr.q[i] = (mcp_slot_t *)sc->intr.dma[i].addr;
	}

	/* Add our ithread  */
	rid = 0;
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 
					 1, RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not alloc interrupt\n");
		goto abort_with_intrq;
	}

	/* load the firmware */
	myri10ge_select_firmware(sc);	

	err = myri10ge_load_firmware(sc);
	if (err != 0)
		goto abort_with_irq_res;
	err = myri10ge_reset(sc);
	if (err != 0)
		goto abort_with_irq_res;

	/* hook into the network stack */
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_baudrate = 100000000;
	ifp->if_capabilities = IFCAP_RXCSUM | IFCAP_TXCSUM;
	ifp->if_hwassist = CSUM_TCP | CSUM_UDP;
	ifp->if_capenable = ifp->if_capabilities;
	sc->csum_flag |= MYRI10GE_MCP_ETHER_FLAGS_CKSUM;
        ifp->if_init = myri10ge_init;
        ifp->if_softc = sc;
        ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
        ifp->if_ioctl = myri10ge_ioctl;
        ifp->if_start = myri10ge_start;
	ifp->if_watchdog = myri10ge_watchdog;
	ether_ifattach(ifp, sc->mac_addr);
	/* ether_ifattach sets mtu to 1500 */
	ifp->if_mtu = MYRI10GE_MAX_ETHER_MTU - ETHER_HDR_LEN;

	/* Initialise the ifmedia structure */
	ifmedia_init(&sc->media, 0, myri10ge_media_change, 
		     myri10ge_media_status);
	ifmedia_add(&sc->media, IFM_ETHER|IFM_AUTO, 0, NULL);
	myri10ge_add_sysctls(sc);
	return 0;

abort_with_irq_res:
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
abort_with_intrq:
	for (i = 0;  i < MYRI10GE_NUM_INTRQS; i++) {
		if (sc->intr.q[i] == NULL)
			continue;
		sc->intr.q[i] = NULL;
		myri10ge_dma_free(&sc->intr.dma[i]);
	}
	myri10ge_dma_free(&sc->fw_stats_dma);
abort_with_zeropad_dma:
	myri10ge_dma_free(&sc->zeropad_dma);
abort_with_cmd_dma:
	myri10ge_dma_free(&sc->cmd_dma);
abort_with_mem_res:
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BARS, sc->mem_res);
abort_with_lock:
	pci_disable_busmaster(dev);
	mtx_destroy(&sc->cmd_lock);
	mtx_destroy(&sc->tx_lock);
	sx_destroy(&sc->driver_lock);
	if_free(ifp);
abort_with_parent_dmat:
	bus_dma_tag_destroy(sc->parent_dmat);

abort_with_nothing:
	return err;
}

static int
myri10ge_detach(device_t dev)
{
	myri10ge_softc_t *sc = device_get_softc(dev);
	int i;

	sx_xlock(&sc->driver_lock);
	if (sc->ifp->if_drv_flags & IFF_DRV_RUNNING)
		myri10ge_close(sc);
	sx_xunlock(&sc->driver_lock);
	ether_ifdetach(sc->ifp);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	for (i = 0;  i < MYRI10GE_NUM_INTRQS; i++) {
		if (sc->intr.q[i] == NULL)
			continue;
		sc->intr.q[i] = NULL;
		myri10ge_dma_free(&sc->intr.dma[i]);
	}
	myri10ge_dma_free(&sc->fw_stats_dma);
	myri10ge_dma_free(&sc->zeropad_dma);
	myri10ge_dma_free(&sc->cmd_dma);
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BARS, sc->mem_res);
	pci_disable_busmaster(dev);
	mtx_destroy(&sc->cmd_lock);
	mtx_destroy(&sc->tx_lock);
	sx_destroy(&sc->driver_lock);
	if_free(sc->ifp);
	bus_dma_tag_destroy(sc->parent_dmat);
	return 0;
}

static int
myri10ge_shutdown(device_t dev)
{
	return 0;
}

/*
  This file uses Myri10GE driver indentation.

  Local Variables:
  c-file-style:"linux"
  tab-width:8
  End:
*/
