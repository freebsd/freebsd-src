/******************************************************************************

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
#include <sys/lock.h>
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
#include <netinet/tcp.h>

#include <machine/bus.h>
#include <machine/in_cksum.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <vm/vm.h>		/* for pmap_mapdev() */
#include <vm/pmap.h>

#include <dev/mxge/mxge_mcp.h>
#include <dev/mxge/mcp_gen_header.h>
#include <dev/mxge/if_mxge_var.h>

/* tunable params */
static int mxge_nvidia_ecrc_enable = 1;
static int mxge_force_firmware = 0;
static int mxge_intr_coal_delay = 30;
static int mxge_deassert_wait = 1;
static int mxge_flow_control = 1;
static int mxge_verbose = 0;
static int mxge_ticks;
static char *mxge_fw_unaligned = "mxge_ethp_z8e";
static char *mxge_fw_aligned = "mxge_eth_z8e";

static int mxge_probe(device_t dev);
static int mxge_attach(device_t dev);
static int mxge_detach(device_t dev);
static int mxge_shutdown(device_t dev);
static void mxge_intr(void *arg);

static device_method_t mxge_methods[] =
{
  /* Device interface */
  DEVMETHOD(device_probe, mxge_probe),
  DEVMETHOD(device_attach, mxge_attach),
  DEVMETHOD(device_detach, mxge_detach),
  DEVMETHOD(device_shutdown, mxge_shutdown),
  {0, 0}
};

static driver_t mxge_driver =
{
  "mxge",
  mxge_methods,
  sizeof(mxge_softc_t),
};

static devclass_t mxge_devclass;

/* Declare ourselves to be a child of the PCI bus.*/
DRIVER_MODULE(mxge, pci, mxge_driver, mxge_devclass, 0, 0);
MODULE_DEPEND(mxge, firmware, 1, 1, 1);

static int mxge_load_firmware(mxge_softc_t *sc);
static int mxge_send_cmd(mxge_softc_t *sc, uint32_t cmd, mxge_cmd_t *data);

static int
mxge_probe(device_t dev)
{
  if ((pci_get_vendor(dev) == MXGE_PCI_VENDOR_MYRICOM) &&
      (pci_get_device(dev) == MXGE_PCI_DEVICE_Z8E)) {
	  device_set_desc(dev, "Myri10G-PCIE-8A");
	  return 0;
  }
  return ENXIO;
}

static void
mxge_enable_wc(mxge_softc_t *sc)
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
	strcpy((char *)&mrdesc.mr_owner, "mxge");
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
mxge_dmamap_callback(void *arg, bus_dma_segment_t *segs, int nsegs,
			 int error)
{
	if (error == 0) {
		*(bus_addr_t *) arg = segs->ds_addr;
	}
}

static int
mxge_dma_alloc(mxge_softc_t *sc, mxge_dma_t *dma, size_t bytes, 
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
			      mxge_dmamap_callback,
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
mxge_dma_free(mxge_dma_t *dma)
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
mxge_parse_strings(mxge_softc_t *sc)
{
#define MXGE_NEXT_STRING(p) while(ptr < limit && *ptr++)

	char *ptr, *limit;
	int i, found_mac;

	ptr = sc->eeprom_strings;
	limit = sc->eeprom_strings + MXGE_EEPROM_STRINGS_SIZE;
	found_mac = 0;
	while (ptr < limit && *ptr != '\0') {
		if (memcmp(ptr, "MAC=", 4) == 0) {
			ptr += 1;
			sc->mac_addr_string = ptr;
			for (i = 0; i < 6; i++) {
				ptr += 3;
				if ((ptr + 2) > limit)
					goto abort;
				sc->mac_addr[i] = strtoul(ptr, NULL, 16);
				found_mac = 1;
			}
		} else if (memcmp(ptr, "PC=", 3) == 0) {
			ptr += 3;
			strncpy(sc->product_code_string, ptr,
				sizeof (sc->product_code_string) - 1);
		} else if (memcmp(ptr, "SN=", 3) == 0) {
			ptr += 3;
			strncpy(sc->serial_number_string, ptr,
				sizeof (sc->serial_number_string) - 1);
		}
		MXGE_NEXT_STRING(ptr);
	}

	if (found_mac)
		return 0;

 abort:
	device_printf(sc->dev, "failed to parse eeprom_strings\n");

	return ENXIO;
}

#if #cpu(i386) || defined __i386 || defined i386 || defined __i386__ || #cpu(x86_64) || defined __x86_64__
static void
mxge_enable_nvidia_ecrc(mxge_softc_t *sc)
{
	uint32_t val;
	unsigned long base, off;
	char *va, *cfgptr;
	device_t pdev, mcp55;
	uint16_t vendor_id, device_id, word;
	uintptr_t bus, slot, func, ivend, idev;
	uint32_t *ptr32;


	if (!mxge_nvidia_ecrc_enable)
		return;

	pdev = device_get_parent(device_get_parent(sc->dev));
	if (pdev == NULL) {
		device_printf(sc->dev, "could not find parent?\n");
		return;
	}
	vendor_id = pci_read_config(pdev, PCIR_VENDOR, 2);
	device_id = pci_read_config(pdev, PCIR_DEVICE, 2);

	if (vendor_id != 0x10de)
		return;

	base = 0;

	if (device_id == 0x005d) {
		/* ck804, base address is magic */
		base = 0xe0000000UL;
	} else if (device_id >= 0x0374 && device_id <= 0x378) {
		/* mcp55, base address stored in chipset */
		mcp55 = pci_find_bsf(0, 0, 0);
		if (mcp55 &&
		    0x10de == pci_read_config(mcp55, PCIR_VENDOR, 2) &&
		    0x0369 == pci_read_config(mcp55, PCIR_DEVICE, 2)) {
			word = pci_read_config(mcp55, 0x90, 2);
			base = ((unsigned long)word & 0x7ffeU) << 25;
		}
	}
	if (!base)
		return;

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
		return;
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
					
	off =  base
		+ 0x00100000UL * (unsigned long)bus
		+ 0x00001000UL * (unsigned long)(func
						 + 8 * slot);

	/* map it into the kernel */
	va = pmap_mapdev(trunc_page((vm_paddr_t)off), PAGE_SIZE);
	

	if (va == NULL) {
		device_printf(sc->dev, "pmap_kenter_temporary didn't\n");
		return;
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
		return;
	}

	ptr32 = (uint32_t*)(cfgptr + 0x178);
	val = *ptr32;

	if (val == 0xffffffff) {
		device_printf(sc->dev, "extended mapping failed\n");
		pmap_unmapdev((vm_offset_t)va, PAGE_SIZE);
		return;
	}
	*ptr32 = val | 0x40;
	pmap_unmapdev((vm_offset_t)va, PAGE_SIZE);
	if (mxge_verbose) 
		device_printf(sc->dev,
			      "Enabled ECRC on upstream Nvidia bridge "
			      "at %d:%d:%d\n",
			      (int)bus, (int)slot, (int)func);
	return;
}
#else
static void
mxge_enable_nvidia_ecrc(mxge_softc_t *sc, device_t pdev)
{
	device_printf(sc->dev,
		      "Nforce 4 chipset on non-x86/amd64!?!?!\n");
	return;
}
#endif


static int
mxge_dma_test(mxge_softc_t *sc, int test_type)
{
	mxge_cmd_t cmd;
	bus_addr_t dmatest_bus = sc->dmabench_dma.bus_addr;
	int status;
	uint32_t len;
	char *test = " ";


	/* Run a small DMA test.
	 * The magic multipliers to the length tell the firmware
	 * to do DMA read, write, or read+write tests.  The
	 * results are returned in cmd.data0.  The upper 16
	 * bits of the return is the number of transfers completed.
	 * The lower 16 bits is the time in 0.5us ticks that the
	 * transfers took to complete.
	 */

	len = sc->tx.boundary;

	cmd.data0 = MXGE_LOWPART_TO_U32(dmatest_bus);
	cmd.data1 = MXGE_HIGHPART_TO_U32(dmatest_bus);
	cmd.data2 = len * 0x10000;
	status = mxge_send_cmd(sc, test_type, &cmd);
	if (status != 0) {
		test = "read";
		goto abort;
	}
	sc->read_dma = ((cmd.data0>>16) * len * 2) /
		(cmd.data0 & 0xffff);
	cmd.data0 = MXGE_LOWPART_TO_U32(dmatest_bus);
	cmd.data1 = MXGE_HIGHPART_TO_U32(dmatest_bus);
	cmd.data2 = len * 0x1;
	status = mxge_send_cmd(sc, test_type, &cmd);
	if (status != 0) {
		test = "write";
		goto abort;
	}
	sc->write_dma = ((cmd.data0>>16) * len * 2) /
		(cmd.data0 & 0xffff);

	cmd.data0 = MXGE_LOWPART_TO_U32(dmatest_bus);
	cmd.data1 = MXGE_HIGHPART_TO_U32(dmatest_bus);
	cmd.data2 = len * 0x10001;
	status = mxge_send_cmd(sc, test_type, &cmd);
	if (status != 0) {
		test = "read/write";
		goto abort;
	}
	sc->read_write_dma = ((cmd.data0>>16) * len * 2 * 2) /
		(cmd.data0 & 0xffff);

abort:
	if (status != 0 && test_type != MXGEFW_CMD_UNALIGNED_TEST)
		device_printf(sc->dev, "DMA %s benchmark failed: %d\n",
			      test, status);

	return status;
}

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

static int
mxge_firmware_probe(mxge_softc_t *sc)
{
	device_t dev = sc->dev;
	int reg, status;
	uint16_t pectl;

	sc->tx.boundary = 4096;
	/*
	 * Verify the max read request size was set to 4KB
	 * before trying the test with 4KB.
	 */
	if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
		pectl = pci_read_config(dev, reg + 0x8, 2);
		if ((pectl & (5 << 12)) != (5 << 12)) {
			device_printf(dev, "Max Read Req. size != 4k (0x%x\n",
				      pectl);
			sc->tx.boundary = 2048;
		}
	}

	/* 
	 * load the optimized firmware (which assumes aligned PCIe
	 * completions) in order to see if it works on this host.
	 */
	sc->fw_name = mxge_fw_aligned;
	status = mxge_load_firmware(sc);
	if (status != 0) {
		return status;
	}

	/* 
	 * Enable ECRC if possible
	 */
	mxge_enable_nvidia_ecrc(sc);

	/* 
	 * Run a DMA test which watches for unaligned completions and
	 * aborts on the first one seen.
	 */

	status = mxge_dma_test(sc, MXGEFW_CMD_UNALIGNED_TEST);
	if (status == 0)
		return 0; /* keep the aligned firmware */

	if (status != E2BIG)
		device_printf(dev, "DMA test failed: %d\n", status);
	if (status == ENOSYS)
		device_printf(dev, "Falling back to ethp! "
			      "Please install up to date fw\n");
	return status;
}

static int
mxge_select_firmware(mxge_softc_t *sc)
{
	int aligned = 0;


	if (mxge_force_firmware != 0) {
		if (mxge_force_firmware == 1)
			aligned = 1;
		else
			aligned = 0;
		if (mxge_verbose)
			device_printf(sc->dev,
				      "Assuming %s completions (forced)\n",
				      aligned ? "aligned" : "unaligned");
		goto abort;
	}

	/* if the PCIe link width is 4 or less, we can use the aligned
	   firmware and skip any checks */
	if (sc->link_width != 0 && sc->link_width <= 4) {
		device_printf(sc->dev,
			      "PCIe x%d Link, expect reduced performance\n",
			      sc->link_width);
		aligned = 1;
		goto abort;
	}

	if (0 == mxge_firmware_probe(sc))
		return 0;

abort:
	if (aligned) {
		sc->fw_name = mxge_fw_aligned;
		sc->tx.boundary = 4096;
	} else {
		sc->fw_name = mxge_fw_unaligned;
		sc->tx.boundary = 2048;
	}
	return (mxge_load_firmware(sc));
}

union qualhack
{
        const char *ro_char;
        char *rw_char;
};

static int
mxge_validate_firmware(mxge_softc_t *sc, const mcp_gen_header_t *hdr)
{


	if (be32toh(hdr->mcp_type) != MCP_TYPE_ETH) {
		device_printf(sc->dev, "Bad firmware type: 0x%x\n", 
			      be32toh(hdr->mcp_type));
		return EIO;
	}

	/* save firmware version for sysctl */
	strncpy(sc->fw_version, hdr->version, sizeof (sc->fw_version));
	if (mxge_verbose)
		device_printf(sc->dev, "firmware id: %s\n", hdr->version);

	sscanf(sc->fw_version, "%d.%d.%d", &sc->fw_ver_major,
	       &sc->fw_ver_minor, &sc->fw_ver_tiny);

	if (!(sc->fw_ver_major == MXGEFW_VERSION_MAJOR
	      && sc->fw_ver_minor == MXGEFW_VERSION_MINOR)) {
		device_printf(sc->dev, "Found firmware version %s\n",
			      sc->fw_version);
		device_printf(sc->dev, "Driver needs %d.%d\n",
			      MXGEFW_VERSION_MAJOR, MXGEFW_VERSION_MINOR);
		return EINVAL;
	}
	return 0;

}

static int
mxge_load_firmware_helper(mxge_softc_t *sc, uint32_t *limit)
{
	const struct firmware *fw;
	const mcp_gen_header_t *hdr;
	unsigned hdr_offset;
	const char *fw_data;
	union qualhack hack;
	int status;
	unsigned int i;
	char dummy;
	

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

	status = mxge_validate_firmware(sc, hdr);
	if (status != 0)
		goto abort_with_fw;

	hack.ro_char = fw_data;
	/* Copy the inflated firmware to NIC SRAM. */
	for (i = 0; i < *limit; i += 256) {
		mxge_pio_copy(sc->sram + MXGE_FW_OFFSET + i,
			      hack.rw_char + i,
			      min(256U, (unsigned)(*limit - i)));
		mb();
		dummy = *sc->sram;
		mb();
	}

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
mxge_dummy_rdma(mxge_softc_t *sc, int enable)
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

	dma_low = MXGE_LOWPART_TO_U32(sc->cmd_dma.bus_addr);
	dma_high = MXGE_HIGHPART_TO_U32(sc->cmd_dma.bus_addr);
	buf[0] = htobe32(dma_high);		/* confirm addr MSW */
	buf[1] = htobe32(dma_low);		/* confirm addr LSW */
	buf[2] = htobe32(0xffffffff);		/* confirm data */
	dma_low = MXGE_LOWPART_TO_U32(sc->zeropad_dma.bus_addr);
	dma_high = MXGE_HIGHPART_TO_U32(sc->zeropad_dma.bus_addr);
	buf[3] = htobe32(dma_high); 		/* dummy addr MSW */
	buf[4] = htobe32(dma_low); 		/* dummy addr LSW */
	buf[5] = htobe32(enable);			/* enable? */


	submit = (volatile char *)(sc->sram + MXGEFW_BOOT_DUMMY_RDMA);

	mxge_pio_copy(submit, buf, 64);
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
mxge_send_cmd(mxge_softc_t *sc, uint32_t cmd, mxge_cmd_t *data)
{
	mcp_cmd_t *buf;
	char buf_bytes[sizeof(*buf) + 8];
	volatile mcp_cmd_response_t *response = sc->cmd;
	volatile char *cmd_addr = sc->sram + MXGEFW_ETH_CMD;
	uint32_t dma_low, dma_high;
	int err, sleep_total = 0;

	/* ensure buf is aligned to 8 bytes */
	buf = (mcp_cmd_t *)((unsigned long)(buf_bytes + 7) & ~7UL);

	buf->data0 = htobe32(data->data0);
	buf->data1 = htobe32(data->data1);
	buf->data2 = htobe32(data->data2);
	buf->cmd = htobe32(cmd);
	dma_low = MXGE_LOWPART_TO_U32(sc->cmd_dma.bus_addr);
	dma_high = MXGE_HIGHPART_TO_U32(sc->cmd_dma.bus_addr);

	buf->response_addr.low = htobe32(dma_low);
	buf->response_addr.high = htobe32(dma_high);
	mtx_lock(&sc->cmd_mtx);
	response->result = 0xffffffff;
	mb();
	mxge_pio_copy((volatile void *)cmd_addr, buf, sizeof (*buf));

	/* wait up to 20ms */
	err = EAGAIN;
	for (sleep_total = 0; sleep_total <  20; sleep_total++) {
		bus_dmamap_sync(sc->cmd_dma.dmat, 
				sc->cmd_dma.map, BUS_DMASYNC_POSTREAD);
		mb();
		switch (be32toh(response->result)) {
		case 0:
			data->data0 = be32toh(response->data);
			err = 0;
			break;
		case 0xffffffff:
			DELAY(1000);
			break;
		case MXGEFW_CMD_UNKNOWN:
			err = ENOSYS;
			break;
		case MXGEFW_CMD_ERROR_UNALIGNED:
			err = E2BIG;
			break;
		default:
			device_printf(sc->dev, 
				      "mxge: command %d "
				      "failed, result = %d\n",
				      cmd, be32toh(response->result));
			err = ENXIO;
			break;
		}
		if (err != EAGAIN)
			break;
	}
	if (err == EAGAIN)
		device_printf(sc->dev, "mxge: command %d timed out"
			      "result = %d\n",
			      cmd, be32toh(response->result));
	mtx_unlock(&sc->cmd_mtx);
	return err;
}

static int
mxge_adopt_running_firmware(mxge_softc_t *sc)
{
	struct mcp_gen_header *hdr;
	const size_t bytes = sizeof (struct mcp_gen_header);
	size_t hdr_offset;
	int status;

	/* find running firmware header */
	hdr_offset = htobe32(*(volatile uint32_t *)
			     (sc->sram + MCP_HEADER_PTR_OFFSET));

	if ((hdr_offset & 3) || hdr_offset + sizeof(*hdr) > sc->sram_size) {
		device_printf(sc->dev, 
			      "Running firmware has bad header offset (%d)\n",
			      (int)hdr_offset);
		return EIO;
	}

	/* copy header of running firmware from SRAM to host memory to
	 * validate firmware */
	hdr = malloc(bytes, M_DEVBUF, M_NOWAIT);
	if (hdr == NULL) {
		device_printf(sc->dev, "could not malloc firmware hdr\n");
		return ENOMEM;
	}
	bus_space_read_region_1(rman_get_bustag(sc->mem_res),
				rman_get_bushandle(sc->mem_res),
				hdr_offset, (char *)hdr, bytes);
	status = mxge_validate_firmware(sc, hdr);
	free(hdr, M_DEVBUF);

	/* 
	 * check to see if adopted firmware has bug where adopting
	 * it will cause broadcasts to be filtered unless the NIC
	 * is kept in ALLMULTI mode
	 */
	if (sc->fw_ver_major == 1 && sc->fw_ver_minor == 4 &&
	    sc->fw_ver_tiny >= 4 && sc->fw_ver_tiny <= 11) {
		sc->adopted_rx_filter_bug = 1;
		device_printf(sc->dev, "Adopting fw %d.%d.%d: "
			      "working around rx filter bug\n",
			      sc->fw_ver_major, sc->fw_ver_minor,
			      sc->fw_ver_tiny);
	}

	return status;
}


static int
mxge_load_firmware(mxge_softc_t *sc)
{
	volatile uint32_t *confirm;
	volatile char *submit;
	char buf_bytes[72];
	uint32_t *buf, size, dma_low, dma_high;
	int status, i;

	buf = (uint32_t *)((unsigned long)(buf_bytes + 7) & ~7UL);

	size = sc->sram_size;
	status = mxge_load_firmware_helper(sc, &size);
	if (status) {
		/* Try to use the currently running firmware, if
		   it is new enough */
		status = mxge_adopt_running_firmware(sc);
		if (status) {
			device_printf(sc->dev,
				      "failed to adopt running firmware\n");
			return status;
		}
		device_printf(sc->dev,
			      "Successfully adopted running firmware\n");
		if (sc->tx.boundary == 4096) {
			device_printf(sc->dev,
				"Using firmware currently running on NIC"
				 ".  For optimal\n");
			device_printf(sc->dev,
				 "performance consider loading optimized "
				 "firmware\n");
		}
		sc->fw_name = mxge_fw_unaligned;
		sc->tx.boundary = 2048;
		return 0;
	}
	/* clear confirmation addr */
	confirm = (volatile uint32_t *)sc->cmd;
	*confirm = 0;
	mb();
	/* send a reload command to the bootstrap MCP, and wait for the
	   response in the confirmation address.  The firmware should
	   write a -1 there to indicate it is alive and well
	*/

	dma_low = MXGE_LOWPART_TO_U32(sc->cmd_dma.bus_addr);
	dma_high = MXGE_HIGHPART_TO_U32(sc->cmd_dma.bus_addr);

	buf[0] = htobe32(dma_high);	/* confirm addr MSW */
	buf[1] = htobe32(dma_low);	/* confirm addr LSW */
	buf[2] = htobe32(0xffffffff);	/* confirm data */

	/* FIX: All newest firmware should un-protect the bottom of
	   the sram before handoff. However, the very first interfaces
	   do not. Therefore the handoff copy must skip the first 8 bytes
	*/
					/* where the code starts*/
	buf[3] = htobe32(MXGE_FW_OFFSET + 8);
	buf[4] = htobe32(size - 8); 	/* length of code */
	buf[5] = htobe32(8);		/* where to copy to */
	buf[6] = htobe32(0);		/* where to jump to */

	submit = (volatile char *)(sc->sram + MXGEFW_BOOT_HANDOFF);
	mxge_pio_copy(submit, buf, 64);
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
	return 0;
}

static int
mxge_update_mac_address(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	uint8_t *addr = sc->mac_addr;
	int status;

	
	cmd.data0 = ((addr[0] << 24) | (addr[1] << 16) 
		     | (addr[2] << 8) | addr[3]);

	cmd.data1 = ((addr[4] << 8) | (addr[5]));

	status = mxge_send_cmd(sc, MXGEFW_SET_MAC_ADDRESS, &cmd);
	return status;
}

static int
mxge_change_pause(mxge_softc_t *sc, int pause)
{	
	mxge_cmd_t cmd;
	int status;

	if (pause)
		status = mxge_send_cmd(sc, MXGEFW_ENABLE_FLOW_CONTROL,
				       &cmd);
	else
		status = mxge_send_cmd(sc, MXGEFW_DISABLE_FLOW_CONTROL,
				       &cmd);

	if (status) {
		device_printf(sc->dev, "Failed to set flow control mode\n");
		return ENXIO;
	}
	sc->pause = pause;
	return 0;
}

static void
mxge_change_promisc(mxge_softc_t *sc, int promisc)
{	
	mxge_cmd_t cmd;
	int status;

	if (promisc)
		status = mxge_send_cmd(sc, MXGEFW_ENABLE_PROMISC,
				       &cmd);
	else
		status = mxge_send_cmd(sc, MXGEFW_DISABLE_PROMISC,
				       &cmd);

	if (status) {
		device_printf(sc->dev, "Failed to set promisc mode\n");
	}
}

static void
mxge_set_multicast_list(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	struct ifmultiaddr *ifma;
	struct ifnet *ifp = sc->ifp;
	int err;

	/* This firmware is known to not support multicast */
	if (!sc->fw_multicast_support)
		return;

	/* Disable multicast filtering while we play with the lists*/
	err = mxge_send_cmd(sc, MXGEFW_ENABLE_ALLMULTI, &cmd);
	if (err != 0) {
		device_printf(sc->dev, "Failed MXGEFW_ENABLE_ALLMULTI,"
		       " error status: %d\n", err);
		return;
	}
	
	if (sc->adopted_rx_filter_bug)
		return;
	
	if (ifp->if_flags & IFF_ALLMULTI)
		/* request to disable multicast filtering, so quit here */
		return;

	/* Flush all the filters */

	err = mxge_send_cmd(sc, MXGEFW_LEAVE_ALL_MULTICAST_GROUPS, &cmd);
	if (err != 0) {
		device_printf(sc->dev, 
			      "Failed MXGEFW_LEAVE_ALL_MULTICAST_GROUPS"
			      ", error status: %d\n", err);
		return;
	}

	/* Walk the multicast list, and add each address */

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		      &cmd.data0, 4);
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr) + 4,
		      &cmd.data1, 2);
		cmd.data0 = htonl(cmd.data0);
		cmd.data1 = htonl(cmd.data1);
		err = mxge_send_cmd(sc, MXGEFW_JOIN_MULTICAST_GROUP, &cmd);
		if (err != 0) {
			device_printf(sc->dev, "Failed "
			       "MXGEFW_JOIN_MULTICAST_GROUP, error status:"
			       "%d\t", err);
			/* abort, leaving multicast filtering off */
			IF_ADDR_UNLOCK(ifp);
			return;
		}
	}
	IF_ADDR_UNLOCK(ifp);
	/* Enable multicast filtering */
	err = mxge_send_cmd(sc, MXGEFW_DISABLE_ALLMULTI, &cmd);
	if (err != 0) {
		device_printf(sc->dev, "Failed MXGEFW_DISABLE_ALLMULTI"
		       ", error status: %d\n", err);
	}
}

static int
mxge_max_mtu(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	int status;

	if (MJUMPAGESIZE - MXGEFW_PAD >  MXGEFW_MAX_MTU)
		return  MXGEFW_MAX_MTU - MXGEFW_PAD;

	/* try to set nbufs to see if it we can
	   use virtually contiguous jumbos */
	cmd.data0 = 0;
	status = mxge_send_cmd(sc, MXGEFW_CMD_ALWAYS_USE_N_BIG_BUFFERS,
			       &cmd);
	if (status == 0)
		return  MXGEFW_MAX_MTU - MXGEFW_PAD;

	/* otherwise, we're limited to MJUMPAGESIZE */
	return MJUMPAGESIZE - MXGEFW_PAD;
}

static int
mxge_reset(mxge_softc_t *sc, int interrupts_setup)
{

	mxge_cmd_t cmd;
	size_t bytes;
	int status;

	/* try to send a reset command to the card to see if it
	   is alive */
	memset(&cmd, 0, sizeof (cmd));
	status = mxge_send_cmd(sc, MXGEFW_CMD_RESET, &cmd);
	if (status != 0) {
		device_printf(sc->dev, "failed reset\n");
		return ENXIO;
	}

	mxge_dummy_rdma(sc, 1);

	if (interrupts_setup) {
		/* Now exchange information about interrupts  */
		bytes = (sc->rx_done.mask + 1) * sizeof (*sc->rx_done.entry);
		memset(sc->rx_done.entry, 0, bytes);
		cmd.data0 = (uint32_t)bytes;
		status = mxge_send_cmd(sc, MXGEFW_CMD_SET_INTRQ_SIZE, &cmd);
		cmd.data0 = MXGE_LOWPART_TO_U32(sc->rx_done.dma.bus_addr);
		cmd.data1 = MXGE_HIGHPART_TO_U32(sc->rx_done.dma.bus_addr);
		status |= mxge_send_cmd(sc, MXGEFW_CMD_SET_INTRQ_DMA, &cmd);
	}

	status |= mxge_send_cmd(sc, 
				MXGEFW_CMD_GET_INTR_COAL_DELAY_OFFSET, &cmd);
	

	sc->intr_coal_delay_ptr = (volatile uint32_t *)(sc->sram + cmd.data0);

	status |= mxge_send_cmd(sc, MXGEFW_CMD_GET_IRQ_ACK_OFFSET, &cmd);
	sc->irq_claim = (volatile uint32_t *)(sc->sram + cmd.data0);


	status |= mxge_send_cmd(sc,  MXGEFW_CMD_GET_IRQ_DEASSERT_OFFSET, 
				&cmd);
	sc->irq_deassert = (volatile uint32_t *)(sc->sram + cmd.data0);
	if (status != 0) {
		device_printf(sc->dev, "failed set interrupt parameters\n");
		return status;
	}
	

	*sc->intr_coal_delay_ptr = htobe32(sc->intr_coal_delay);

	
	/* run a DMA benchmark */
	(void) mxge_dma_test(sc, MXGEFW_DMA_TEST);

	/* reset mcp/driver shared state back to 0 */
	sc->rx_done.idx = 0;
	sc->rx_done.cnt = 0;
	sc->tx.req = 0;
	sc->tx.done = 0;
	sc->tx.pkt_done = 0;
	sc->tx.wake = 0;
	sc->tx_defrag = 0;
	sc->tx.stall = 0;
	sc->rx_big.cnt = 0;
	sc->rx_small.cnt = 0;
	sc->rdma_tags_available = 15;
	sc->fw_stats->valid = 0;
	sc->fw_stats->send_done_count = 0;
	sc->lro_bad_csum = 0;
	sc->lro_queued = 0;
	sc->lro_flushed = 0;
	status = mxge_update_mac_address(sc);
	mxge_change_promisc(sc, 0);
	mxge_change_pause(sc, sc->pause);
	mxge_set_multicast_list(sc);
	return status;
}

static int
mxge_change_intr_coal(SYSCTL_HANDLER_ARGS)
{
        mxge_softc_t *sc;
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

	mtx_lock(&sc->driver_mtx);
	*sc->intr_coal_delay_ptr = htobe32(intr_coal_delay);
	sc->intr_coal_delay = intr_coal_delay;
	
	mtx_unlock(&sc->driver_mtx);
        return err;
}

static int
mxge_change_flow_control(SYSCTL_HANDLER_ARGS)
{
        mxge_softc_t *sc;
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

	mtx_lock(&sc->driver_mtx);
	err = mxge_change_pause(sc, enabled);
	mtx_unlock(&sc->driver_mtx);
        return err;
}

static int
mxge_handle_be32(SYSCTL_HANDLER_ARGS)
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
mxge_add_sysctls(mxge_softc_t *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;
	mcp_irq_data_t *fw;

	ctx = device_get_sysctl_ctx(sc->dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));
	fw = sc->fw_stats;

	/* random information */
	SYSCTL_ADD_STRING(ctx, children, OID_AUTO, 
		       "firmware_version",
		       CTLFLAG_RD, &sc->fw_version,
		       0, "firmware version");
	SYSCTL_ADD_STRING(ctx, children, OID_AUTO, 
		       "serial_number",
		       CTLFLAG_RD, &sc->serial_number_string,
		       0, "serial number");
	SYSCTL_ADD_STRING(ctx, children, OID_AUTO, 
		       "product_code",
		       CTLFLAG_RD, &sc->product_code_string,
		       0, "product_code");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "pcie_link_width",
		       CTLFLAG_RD, &sc->link_width,
		       0, "tx_boundary");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_boundary",
		       CTLFLAG_RD, &sc->tx.boundary,
		       0, "tx_boundary");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "write_combine",
		       CTLFLAG_RD, &sc->wc,
		       0, "write combining PIO?");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "read_dma_MBs",
		       CTLFLAG_RD, &sc->read_dma,
		       0, "DMA Read speed in MB/s");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "write_dma_MBs",
		       CTLFLAG_RD, &sc->write_dma,
		       0, "DMA Write speed in MB/s");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "read_write_dma_MBs",
		       CTLFLAG_RD, &sc->read_write_dma,
		       0, "DMA concurrent Read/Write speed in MB/s");


	/* performance related tunables */
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"intr_coal_delay",
			CTLTYPE_INT|CTLFLAG_RW, sc,
			0, mxge_change_intr_coal, 
			"I", "interrupt coalescing delay in usecs");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"flow_control_enabled",
			CTLTYPE_INT|CTLFLAG_RW, sc,
			0, mxge_change_flow_control,
			"I", "interrupt coalescing delay in usecs");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "deassert_wait",
		       CTLFLAG_RW, &mxge_deassert_wait,
		       0, "Wait for IRQ line to go low in ihandler");

	/* stats block from firmware is in network byte order.  
	   Need to swap it */
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"link_up",
			CTLTYPE_INT|CTLFLAG_RD, &fw->link_up,
			0, mxge_handle_be32,
			"I", "link up");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"rdma_tags_available",
			CTLTYPE_INT|CTLFLAG_RD, &fw->rdma_tags_available,
			0, mxge_handle_be32,
			"I", "rdma_tags_available");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_bad_crc32",
			CTLTYPE_INT|CTLFLAG_RD, 
			&fw->dropped_bad_crc32,
			0, mxge_handle_be32,
			"I", "dropped_bad_crc32");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_bad_phy",
			CTLTYPE_INT|CTLFLAG_RD, 
			&fw->dropped_bad_phy,
			0, mxge_handle_be32,
			"I", "dropped_bad_phy");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_link_error_or_filtered",
			CTLTYPE_INT|CTLFLAG_RD, 
			&fw->dropped_link_error_or_filtered,
			0, mxge_handle_be32,
			"I", "dropped_link_error_or_filtered");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_link_overflow",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_link_overflow,
			0, mxge_handle_be32,
			"I", "dropped_link_overflow");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_multicast_filtered",
			CTLTYPE_INT|CTLFLAG_RD, 
			&fw->dropped_multicast_filtered,
			0, mxge_handle_be32,
			"I", "dropped_multicast_filtered");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_no_big_buffer",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_no_big_buffer,
			0, mxge_handle_be32,
			"I", "dropped_no_big_buffer");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_no_small_buffer",
			CTLTYPE_INT|CTLFLAG_RD, 
			&fw->dropped_no_small_buffer,
			0, mxge_handle_be32,
			"I", "dropped_no_small_buffer");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_overrun",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_overrun,
			0, mxge_handle_be32,
			"I", "dropped_overrun");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_pause",
			CTLTYPE_INT|CTLFLAG_RD, 
			&fw->dropped_pause,
			0, mxge_handle_be32,
			"I", "dropped_pause");
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_runt",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_runt,
			0, mxge_handle_be32,
			"I", "dropped_runt");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"dropped_unicast_filtered",
			CTLTYPE_INT|CTLFLAG_RD, &fw->dropped_unicast_filtered,
			0, mxge_handle_be32,
			"I", "dropped_unicast_filtered");

	/* host counters exported for debugging */
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "rx_small_cnt",
		       CTLFLAG_RD, &sc->rx_small.cnt,
		       0, "rx_small_cnt");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "rx_big_cnt",
		       CTLFLAG_RD, &sc->rx_big.cnt,
		       0, "rx_small_cnt");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_req",
		       CTLFLAG_RD, &sc->tx.req,
		       0, "tx_req");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_done",
		       CTLFLAG_RD, &sc->tx.done,
		       0, "tx_done");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_pkt_done",
		       CTLFLAG_RD, &sc->tx.pkt_done,
		       0, "tx_done");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_stall",
		       CTLFLAG_RD, &sc->tx.stall,
		       0, "tx_stall");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_wake",
		       CTLFLAG_RD, &sc->tx.wake,
		       0, "tx_wake");
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "tx_defrag",
		       CTLFLAG_RD, &sc->tx_defrag,
		       0, "tx_defrag");

	/* verbose printing? */
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "verbose",
		       CTLFLAG_RW, &mxge_verbose,
		       0, "verbose printing");

	/* lro */
	SYSCTL_ADD_INT(ctx, children, OID_AUTO,
		       "lro_cnt", CTLFLAG_RD, &sc->lro_cnt,
		       0, "number of lro merge queues");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO,
		       "lro_flushed", CTLFLAG_RD, &sc->lro_flushed,
		       0, "number of lro merge queues flushed");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO,
		       "lro_queued", CTLFLAG_RD, &sc->lro_queued,
		       0, "number of frames appended to lro merge queues");

}

/* copy an array of mcp_kreq_ether_send_t's to the mcp.  Copy 
   backwards one at a time and handle ring wraps */

static inline void 
mxge_submit_req_backwards(mxge_tx_buf_t *tx, 
			    mcp_kreq_ether_send_t *src, int cnt)
{
        int idx, starting_slot;
        starting_slot = tx->req;
        while (cnt > 1) {
                cnt--;
                idx = (starting_slot + cnt) & tx->mask;
                mxge_pio_copy(&tx->lanai[idx],
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
mxge_submit_req(mxge_tx_buf_t *tx, mcp_kreq_ether_send_t *src, 
                  int cnt)
{
        int idx, i;
        uint32_t *src_ints;
	volatile uint32_t *dst_ints;
        mcp_kreq_ether_send_t *srcp;
	volatile mcp_kreq_ether_send_t *dstp, *dst;
	uint8_t last_flags;
        
        idx = tx->req & tx->mask;

	last_flags = src->flags;
	src->flags = 0;
        mb();
        dst = dstp = &tx->lanai[idx];
        srcp = src;

        if ((idx + cnt) < tx->mask) {
                for (i = 0; i < (cnt - 1); i += 2) {
                        mxge_pio_copy(dstp, srcp, 2 * sizeof(*src));
                        mb(); /* force write every 32 bytes */
                        srcp += 2;
                        dstp += 2;
                }
        } else {
                /* submit all but the first request, and ensure 
                   that it is submitted below */
                mxge_submit_req_backwards(tx, src, cnt);
                i = 0;
        }
        if (i < cnt) {
                /* submit the first request */
                mxge_pio_copy(dstp, srcp, sizeof(*src));
                mb(); /* barrier before setting valid flag */
        }

        /* re-write the last 32-bits with the valid flags */
        src->flags = last_flags;
        src_ints = (uint32_t *)src;
        src_ints+=3;
        dst_ints = (volatile uint32_t *)dst;
        dst_ints+=3;
        *dst_ints =  *src_ints;
        tx->req += cnt;
        mb();
}

static void
mxge_encap(mxge_softc_t *sc, struct mbuf *m)
{
	mcp_kreq_ether_send_t *req;
	bus_dma_segment_t *seg;
	struct mbuf *m_tmp;
	struct ifnet *ifp;
	mxge_tx_buf_t *tx;
	struct ip *ip;
	int cnt, cum_len, err, i, idx, odd_flag, ip_off;
	uint16_t pseudo_hdr_offset;
        uint8_t flags, cksum_offset;



	ifp = sc->ifp;
	tx = &sc->tx;

	ip_off = sizeof (struct ether_header);

	/* (try to) map the frame for DMA */
	idx = tx->req & tx->mask;
	err = bus_dmamap_load_mbuf_sg(tx->dmat, tx->info[idx].map,
				      m, tx->seg_list, &cnt, 
				      BUS_DMA_NOWAIT);
	if (__predict_false(err == EFBIG)) {
		/* Too many segments in the chain.  Try
		   to defrag */
		m_tmp = m_defrag(m, M_NOWAIT);
		if (m_tmp == NULL) {
			goto drop;
		}
		sc->tx_defrag++;
		m = m_tmp;
		err = bus_dmamap_load_mbuf_sg(tx->dmat, 
					      tx->info[idx].map,
					      m, tx->seg_list, &cnt, 
					      BUS_DMA_NOWAIT);
	}
	if (__predict_false(err != 0)) {
		device_printf(sc->dev, "bus_dmamap_load_mbuf_sg returned %d"
			      " packet len = %d\n", err, m->m_pkthdr.len);
		goto drop;
	}
	bus_dmamap_sync(tx->dmat, tx->info[idx].map,
			BUS_DMASYNC_PREWRITE);
	tx->info[idx].m = m;

	req = tx->req_list;
	cksum_offset = 0;
	pseudo_hdr_offset = 0;
	flags = MXGEFW_FLAGS_NO_TSO;

	/* checksum offloading? */
	if (m->m_pkthdr.csum_flags & (CSUM_DELAY_DATA)) {
		/* ensure ip header is in first mbuf, copy
		   it to a scratch buffer if not */
		if (__predict_false(m->m_len < ip_off + sizeof (*ip))) {
			m_copydata(m, 0, ip_off + sizeof (*ip),
				   sc->scratch);
			ip = (struct ip *)(sc->scratch + ip_off);
		} else {
			ip = (struct ip *)(mtod(m, char *) + ip_off);
		}
		cksum_offset = ip_off + (ip->ip_hl << 2);
		pseudo_hdr_offset = cksum_offset +  m->m_pkthdr.csum_data;
		pseudo_hdr_offset = htobe16(pseudo_hdr_offset);
		req->cksum_offset = cksum_offset;
		flags |= MXGEFW_FLAGS_CKSUM;
		odd_flag = MXGEFW_FLAGS_ALIGN_ODD;
	} else {
		odd_flag = 0;
	}
	if (m->m_pkthdr.len < MXGEFW_SEND_SMALL_SIZE)
		flags |= MXGEFW_FLAGS_SMALL;

	/* convert segments into a request list */
	cum_len = 0;
	seg = tx->seg_list;
	req->flags = MXGEFW_FLAGS_FIRST;
	for (i = 0; i < cnt; i++) {
		req->addr_low = 
			htobe32(MXGE_LOWPART_TO_U32(seg->ds_addr));
		req->addr_high = 
			htobe32(MXGE_HIGHPART_TO_U32(seg->ds_addr));
		req->length = htobe16(seg->ds_len);
		req->cksum_offset = cksum_offset;
		if (cksum_offset > seg->ds_len)
			cksum_offset -= seg->ds_len;
		else
			cksum_offset = 0;
		req->pseudo_hdr_offset = pseudo_hdr_offset;
		req->pad = 0; /* complete solid 16-byte block */
		req->rdma_count = 1;
		req->flags |= flags | ((cum_len & 1) * odd_flag);
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
			htobe32(MXGE_LOWPART_TO_U32(sc->zeropad_dma.bus_addr));
		req->addr_high = 
			htobe32(MXGE_HIGHPART_TO_U32(sc->zeropad_dma.bus_addr));
		req->length = htobe16(60 - cum_len);
		req->cksum_offset = 0;
		req->pseudo_hdr_offset = pseudo_hdr_offset;
		req->pad = 0; /* complete solid 16-byte block */
		req->rdma_count = 1;
		req->flags |= flags | ((cum_len & 1) * odd_flag);
		cnt++;
	}

	tx->req_list[0].rdma_count = cnt;
#if 0
	/* print what the firmware will see */
	for (i = 0; i < cnt; i++) {
		printf("%d: addr: 0x%x 0x%x len:%d pso%d,"
		    "cso:%d, flags:0x%x, rdma:%d\n",
		    i, (int)ntohl(tx->req_list[i].addr_high),
		    (int)ntohl(tx->req_list[i].addr_low),
		    (int)ntohs(tx->req_list[i].length),
		    (int)ntohs(tx->req_list[i].pseudo_hdr_offset),
		    tx->req_list[i].cksum_offset, tx->req_list[i].flags,
		    tx->req_list[i].rdma_count);
	}
	printf("--------------\n");
#endif
	tx->info[((cnt - 1) + tx->req) & tx->mask].flag = 1;
	mxge_submit_req(tx, tx->req_list, cnt);
	return;

drop:
	m_freem(m);
	ifp->if_oerrors++;
	return;
}




static inline void
mxge_start_locked(mxge_softc_t *sc)
{
	struct mbuf *m;
	struct ifnet *ifp;
	mxge_tx_buf_t *tx;

	ifp = sc->ifp;
	tx = &sc->tx;
	while ((tx->mask - (tx->req - tx->done)) > tx->max_desc) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL) {
			return;
		}
		/* let BPF see it */
		BPF_MTAP(ifp, m);

		/* give it to the nic */
		mxge_encap(sc, m);
	}
	/* ran out of transmit slots */
	if ((sc->ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0) {
		sc->ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		tx->stall++;
	}
}

static void
mxge_start(struct ifnet *ifp)
{
	mxge_softc_t *sc = ifp->if_softc;


	mtx_lock(&sc->tx_mtx);
	mxge_start_locked(sc);
	mtx_unlock(&sc->tx_mtx);		
}

/*
 * copy an array of mcp_kreq_ether_recv_t's to the mcp.  Copy
 * at most 32 bytes at a time, so as to avoid involving the software
 * pio handler in the nic.   We re-write the first segment's low
 * DMA address to mark it valid only after we write the entire chunk
 * in a burst
 */
static inline void
mxge_submit_8rx(volatile mcp_kreq_ether_recv_t *dst,
		mcp_kreq_ether_recv_t *src)
{
	uint32_t low;

	low = src->addr_low;
	src->addr_low = 0xffffffff;
	mxge_pio_copy(dst, src, 4 * sizeof (*src));
	mb();
	mxge_pio_copy(dst + 4, src + 4, 4 * sizeof (*src));
	mb();
	src->addr_low = low;
	dst->addr_low = low;
	mb();
}

static int
mxge_get_buf_small(mxge_softc_t *sc, bus_dmamap_t map, int idx)
{
	bus_dma_segment_t seg;
	struct mbuf *m;
	mxge_rx_buf_t *rx = &sc->rx_small;
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
		htobe32(MXGE_LOWPART_TO_U32(seg.ds_addr));
	rx->shadow[idx].addr_high = 
		htobe32(MXGE_HIGHPART_TO_U32(seg.ds_addr));

done:
	if ((idx & 7) == 7)
		mxge_submit_8rx(&rx->lanai[idx - 7], &rx->shadow[idx - 7]);
	return err;
}

static int
mxge_get_buf_big(mxge_softc_t *sc, bus_dmamap_t map, int idx)
{
	bus_dma_segment_t seg[3];
	struct mbuf *m;
	mxge_rx_buf_t *rx = &sc->rx_big;
	int cnt, err, i;

	if (rx->cl_size == MCLBYTES)
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	else
		m = m_getjcl(M_DONTWAIT, MT_DATA, M_PKTHDR, rx->cl_size);
	if (m == NULL) {
		rx->alloc_fail++;
		err = ENOBUFS;
		goto done;
	}
	m->m_len = rx->cl_size;
	err = bus_dmamap_load_mbuf_sg(rx->dmat, map, m, 
				      seg, &cnt, BUS_DMA_NOWAIT);
	if (err != 0) {
		m_free(m);
		goto done;
	}
	rx->info[idx].m = m;

	for (i = 0; i < cnt; i++) {
		rx->shadow[idx + i].addr_low = 
			htobe32(MXGE_LOWPART_TO_U32(seg[i].ds_addr));
		rx->shadow[idx + i].addr_high = 
			htobe32(MXGE_HIGHPART_TO_U32(seg[i].ds_addr));
       }


done:
       for (i = 0; i < rx->nbufs; i++) {
		if ((idx & 7) == 7) {
			mxge_submit_8rx(&rx->lanai[idx - 7],
					&rx->shadow[idx - 7]);
		}
		idx++;
	}
	return err;
}

/* 
 *  Myri10GE hardware checksums are not valid if the sender
 *  padded the frame with non-zero padding.  This is because
 *  the firmware just does a simple 16-bit 1s complement
 *  checksum across the entire frame, excluding the first 14
 *  bytes.  It is best to simply to check the checksum and
 *  tell the stack about it only if the checksum is good
 */

static inline uint16_t
mxge_rx_csum(struct mbuf *m, int csum)
{
	struct ether_header *eh;
	struct ip *ip;
	uint16_t c;

	eh = mtod(m, struct ether_header *);

	/* only deal with IPv4 TCP & UDP for now */
	if (__predict_false(eh->ether_type != htons(ETHERTYPE_IP)))
		return 1;
	ip = (struct ip *)(eh + 1);
	if (__predict_false(ip->ip_p != IPPROTO_TCP &&
			    ip->ip_p != IPPROTO_UDP))
		return 1;

	c = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		      htonl(ntohs(csum) + ntohs(ip->ip_len) +
			    - (ip->ip_hl << 2) + ip->ip_p));
	c ^= 0xffff;
	return (c);
}

static void
mxge_vlan_tag_remove(struct mbuf *m, uint32_t *csum)
{
	struct ether_vlan_header *evl;
	struct ether_header *eh;
	struct m_tag *mtag;
	uint32_t partial;

	evl = mtod(m, struct ether_vlan_header *);
	eh = mtod(m, struct ether_header *);

	/*
	 * fix checksum by subtracting ETHER_VLAN_ENCAP_LEN bytes
	 * after what the firmware thought was the end of the ethernet
	 * header.
	 */

	/* put checksum into host byte order */
	*csum = ntohs(*csum); 
	partial = ntohl(*(uint32_t *)(mtod(m, char *) + ETHER_HDR_LEN));
	(*csum) += ~partial;
	(*csum) +=  ((*csum) < ~partial);
	(*csum) = ((*csum) >> 16) + ((*csum) & 0xFFFF);
	(*csum) = ((*csum) >> 16) + ((*csum) & 0xFFFF);

	/* restore checksum to network byte order; 
	   later consumers expect this */
	*csum = htons(*csum);
	/* save the tag */
	mtag = m_tag_alloc(MTAG_VLAN, MTAG_VLAN_TAG, sizeof(u_int),
			   M_NOWAIT);
	if (mtag == NULL)
		return;
	m->m_flags |= M_VLANTAG;
	VLAN_TAG_VALUE(mtag) = ntohs(evl->evl_tag);
	m_tag_prepend(m, mtag);

	/*
	 * Remove the 802.1q header by copying the Ethernet
	 * addresses over it and adjusting the beginning of
	 * the data in the mbuf.  The encapsulated Ethernet
	 * type field is already in place.
	 */
	bcopy((char *)evl, (char *)evl + ETHER_VLAN_ENCAP_LEN,
	      ETHER_HDR_LEN - ETHER_TYPE_LEN);
	m_adj(m, ETHER_VLAN_ENCAP_LEN);
}


static inline void
mxge_rx_done_big(mxge_softc_t *sc, uint32_t len, uint32_t csum)
{
	struct ifnet *ifp;
	struct mbuf *m;
	struct ether_header *eh;
	mxge_rx_buf_t *rx;
	bus_dmamap_t old_map;
	int idx;
	uint16_t tcpudp_csum;

	ifp = sc->ifp;
	rx = &sc->rx_big;
	idx = rx->cnt & rx->mask;
	rx->cnt += rx->nbufs;
	/* save a pointer to the received mbuf */
	m = rx->info[idx].m;
	/* try to replace the received mbuf */
	if (mxge_get_buf_big(sc, rx->extra_map, idx)) {
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
	m->m_data += MXGEFW_PAD;

	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len = len;
	ifp->if_ipackets++;
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_VLAN)) {
		mxge_vlan_tag_remove(m, &csum);
	}
	/* if the checksum is valid, mark it in the mbuf header */
	if (sc->csum_flag && (0 == (tcpudp_csum = mxge_rx_csum(m, csum)))) {
		if (sc->lro_cnt && (0 == mxge_lro_rx(sc, m, csum)))
			return;
		/* otherwise, it was a UDP frame, or a TCP frame which
		   we could not do LRO on.  Tell the stack that the
		   checksum is good */
		m->m_pkthdr.csum_data = 0xffff;
		m->m_pkthdr.csum_flags = CSUM_PSEUDO_HDR | CSUM_DATA_VALID;
	}
	/* pass the frame up the stack */
	(*ifp->if_input)(ifp, m);
}

static inline void
mxge_rx_done_small(mxge_softc_t *sc, uint32_t len, uint32_t csum)
{
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
	mxge_rx_buf_t *rx;
	bus_dmamap_t old_map;
	int idx;
	uint16_t tcpudp_csum;

	ifp = sc->ifp;
	rx = &sc->rx_small;
	idx = rx->cnt & rx->mask;
	rx->cnt++;
	/* save a pointer to the received mbuf */
	m = rx->info[idx].m;
	/* try to replace the received mbuf */
	if (mxge_get_buf_small(sc, rx->extra_map, idx)) {
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
	m->m_data += MXGEFW_PAD;

	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len = len;
	ifp->if_ipackets++;
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_VLAN)) {
		mxge_vlan_tag_remove(m, &csum);
	}
	/* if the checksum is valid, mark it in the mbuf header */
	if (sc->csum_flag && (0 == (tcpudp_csum = mxge_rx_csum(m, csum)))) {
		if (sc->lro_cnt && (0 == mxge_lro_rx(sc, m, csum)))
			return;
		/* otherwise, it was a UDP frame, or a TCP frame which
		   we could not do LRO on.  Tell the stack that the
		   checksum is good */
		m->m_pkthdr.csum_data = 0xffff;
		m->m_pkthdr.csum_flags = CSUM_PSEUDO_HDR | CSUM_DATA_VALID;
	}

	/* pass the frame up the stack */
	(*ifp->if_input)(ifp, m);
}

static inline void
mxge_clean_rx_done(mxge_softc_t *sc)
{
	mxge_rx_done_t *rx_done = &sc->rx_done;
	struct lro_entry *lro;
	int limit = 0;
	uint16_t length;
	uint16_t checksum;


	while (rx_done->entry[rx_done->idx].length != 0) {
		length = ntohs(rx_done->entry[rx_done->idx].length);
		rx_done->entry[rx_done->idx].length = 0;
		checksum = rx_done->entry[rx_done->idx].checksum;
		if (length <= (MHLEN - MXGEFW_PAD))
			mxge_rx_done_small(sc, length, checksum);
		else
			mxge_rx_done_big(sc, length, checksum);
		rx_done->cnt++;
		rx_done->idx = rx_done->cnt & rx_done->mask;

		/* limit potential for livelock */
		if (__predict_false(++limit > 2 * rx_done->mask))
			break;
	}
	while(!SLIST_EMPTY(&sc->lro_active)) {
		lro = SLIST_FIRST(&sc->lro_active);
		SLIST_REMOVE_HEAD(&sc->lro_active, next);
		mxge_lro_flush(sc, lro);
	}
}


static inline void
mxge_tx_done(mxge_softc_t *sc, uint32_t mcp_idx)
{
	struct ifnet *ifp;
	mxge_tx_buf_t *tx;
	struct mbuf *m;
	bus_dmamap_t map;
	int idx, limit;

	limit = 0;
	tx = &sc->tx;
	ifp = sc->ifp;
	while (tx->pkt_done != mcp_idx) {
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
		if (tx->info[idx].flag) {
			tx->info[idx].flag = 0;
			tx->pkt_done++;
		}
		/* limit potential for livelock by only handling
		   2 full tx rings per call */
		if (__predict_false(++limit >  2 * tx->mask))
			break;
	}
	
	/* If we have space, clear IFF_OACTIVE to tell the stack that
           its OK to send packets */

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE &&
	    tx->req - tx->done < (tx->mask + 1)/4) {
		mtx_lock(&sc->tx_mtx);
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		sc->tx.wake++;
		mxge_start_locked(sc);
		mtx_unlock(&sc->tx_mtx);
	}
}

static void
mxge_intr(void *arg)
{
	mxge_softc_t *sc = arg;
	mcp_irq_data_t *stats = sc->fw_stats;
	mxge_tx_buf_t *tx = &sc->tx;
	mxge_rx_done_t *rx_done = &sc->rx_done;
	uint32_t send_done_count;
	uint8_t valid;


	/* make sure the DMA has finished */
	if (!stats->valid) {
		return;
	}
	valid = stats->valid;

	if (!sc->msi_enabled) {
		/* lower legacy IRQ  */
		*sc->irq_deassert = 0;
		if (!mxge_deassert_wait)
			/* don't wait for conf. that irq is low */
			stats->valid = 0;
	} else {
		stats->valid = 0;
	}

	/* loop while waiting for legacy irq deassertion */
	do {
		/* check for transmit completes and receives */
		send_done_count = be32toh(stats->send_done_count);
		while ((send_done_count != tx->pkt_done) ||
		       (rx_done->entry[rx_done->idx].length != 0)) {
			mxge_tx_done(sc, (int)send_done_count);
			mxge_clean_rx_done(sc);
			send_done_count = be32toh(stats->send_done_count);
		}
	} while (*((volatile uint8_t *) &stats->valid));

	if (__predict_false(stats->stats_updated)) {
		if (sc->link_state != stats->link_up) {
			sc->link_state = stats->link_up;
			if (sc->link_state) {
				if_link_state_change(sc->ifp, LINK_STATE_UP);
				if (mxge_verbose)
					device_printf(sc->dev, "link up\n");
			} else {
				if_link_state_change(sc->ifp, LINK_STATE_DOWN);
				if (mxge_verbose)
					device_printf(sc->dev, "link down\n");
			}
		}
		if (sc->rdma_tags_available !=
		    be32toh(sc->fw_stats->rdma_tags_available)) {
			sc->rdma_tags_available = 
				be32toh(sc->fw_stats->rdma_tags_available);
			device_printf(sc->dev, "RDMA timed out! %d tags "
				      "left\n", sc->rdma_tags_available);
		}
		sc->down_cnt += stats->link_down;
	}

	/* check to see if we have rx token to pass back */
	if (valid & 0x1)
	    *sc->irq_claim = be32toh(3);
	*(sc->irq_claim + 1) = be32toh(3);
}

static void
mxge_init(void *arg)
{
}



static void
mxge_free_mbufs(mxge_softc_t *sc)
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

	for (i = 0; i <= sc->rx_small.mask; i++) {
		if (sc->rx_small.info[i].m == NULL)
			continue;
		bus_dmamap_unload(sc->rx_small.dmat,
				  sc->rx_small.info[i].map);
		m_freem(sc->rx_small.info[i].m);
		sc->rx_small.info[i].m = NULL;
	}

	for (i = 0; i <= sc->tx.mask; i++) {
		sc->tx.info[i].flag = 0;
		if (sc->tx.info[i].m == NULL)
			continue;
		bus_dmamap_unload(sc->tx.dmat,
				  sc->tx.info[i].map);
		m_freem(sc->tx.info[i].m);
		sc->tx.info[i].m = NULL;
	}
}

static void
mxge_free_rings(mxge_softc_t *sc)
{
	int i;

	if (sc->rx_done.entry != NULL)
		mxge_dma_free(&sc->rx_done.dma);
	sc->rx_done.entry = NULL;
	if (sc->tx.req_bytes != NULL)
		free(sc->tx.req_bytes, M_DEVBUF);
	if (sc->tx.seg_list != NULL)
		free(sc->tx.seg_list, M_DEVBUF);
	if (sc->rx_small.shadow != NULL)
		free(sc->rx_small.shadow, M_DEVBUF);
	if (sc->rx_big.shadow != NULL)
		free(sc->rx_big.shadow, M_DEVBUF);
	if (sc->tx.info != NULL) {
		if (sc->tx.dmat != NULL) {
			for (i = 0; i <= sc->tx.mask; i++) {
				bus_dmamap_destroy(sc->tx.dmat,
						   sc->tx.info[i].map);
			}
			bus_dma_tag_destroy(sc->tx.dmat);
		}
		free(sc->tx.info, M_DEVBUF);
	}
	if (sc->rx_small.info != NULL) {
		if (sc->rx_small.dmat != NULL) {
			for (i = 0; i <= sc->rx_small.mask; i++) {
				bus_dmamap_destroy(sc->rx_small.dmat,
						   sc->rx_small.info[i].map);
			}
			bus_dmamap_destroy(sc->rx_small.dmat,
					   sc->rx_small.extra_map);
			bus_dma_tag_destroy(sc->rx_small.dmat);
		}
		free(sc->rx_small.info, M_DEVBUF);
	}
	if (sc->rx_big.info != NULL) {
		if (sc->rx_big.dmat != NULL) {
			for (i = 0; i <= sc->rx_big.mask; i++) {
				bus_dmamap_destroy(sc->rx_big.dmat,
						   sc->rx_big.info[i].map);
			}
			bus_dmamap_destroy(sc->rx_big.dmat,
					   sc->rx_big.extra_map);
			bus_dma_tag_destroy(sc->rx_big.dmat);
		}
		free(sc->rx_big.info, M_DEVBUF);
	}
}

static int
mxge_alloc_rings(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	int tx_ring_size, rx_ring_size;
	int tx_ring_entries, rx_ring_entries;
	int i, err;
	unsigned long bytes;
	
	/* get ring sizes */
	err = mxge_send_cmd(sc, MXGEFW_CMD_GET_SEND_RING_SIZE, &cmd);
	tx_ring_size = cmd.data0;
	err |= mxge_send_cmd(sc, MXGEFW_CMD_GET_RX_RING_SIZE, &cmd);
	if (err != 0) {
		device_printf(sc->dev, "Cannot determine ring sizes\n");
		goto abort_with_nothing;
	}

	rx_ring_size = cmd.data0;

	tx_ring_entries = tx_ring_size / sizeof (mcp_kreq_ether_send_t);
	rx_ring_entries = rx_ring_size / sizeof (mcp_dma_addr_t);
	IFQ_SET_MAXLEN(&sc->ifp->if_snd, tx_ring_entries - 1);
	sc->ifp->if_snd.ifq_drv_maxlen = sc->ifp->if_snd.ifq_maxlen;
	IFQ_SET_READY(&sc->ifp->if_snd);

	sc->tx.mask = tx_ring_entries - 1;
	sc->tx.max_desc = MIN(MXGE_MAX_SEND_DESC, tx_ring_entries / 4);
	sc->rx_small.mask = sc->rx_big.mask = rx_ring_entries - 1;
	sc->rx_done.mask = (2 * rx_ring_entries) - 1;

	err = ENOMEM;

	/* allocate interrupt queues */
	bytes = (sc->rx_done.mask + 1) * sizeof (*sc->rx_done.entry);
	err = mxge_dma_alloc(sc, &sc->rx_done.dma, bytes, 4096);
	if (err != 0)
		goto abort_with_nothing;
	sc->rx_done.entry = sc->rx_done.dma.addr;
	bzero(sc->rx_done.entry, bytes);

	/* allocate the tx request copy block */
	bytes = 8 + 
		sizeof (*sc->tx.req_list) * (sc->tx.max_desc + 4);
	sc->tx.req_bytes = malloc(bytes, M_DEVBUF, M_WAITOK);
	if (sc->tx.req_bytes == NULL)
		goto abort_with_alloc;
	/* ensure req_list entries are aligned to 8 bytes */
	sc->tx.req_list = (mcp_kreq_ether_send_t *)
		((unsigned long)(sc->tx.req_bytes + 7) & ~7UL);

	/* allocate the tx busdma segment list */
	bytes = sizeof (*sc->tx.seg_list) * sc->tx.max_desc;
	sc->tx.seg_list = (bus_dma_segment_t *) 
		malloc(bytes, M_DEVBUF, M_WAITOK);
	if (sc->tx.seg_list == NULL)
		goto abort_with_alloc;

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
				 65536 + 256,		/* maxsize */
				 sc->tx.max_desc - 2,	/* num segs */
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
				 3*4096,		/* maxsize */
				 3,			/* num segs */
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
	mxge_free_rings(sc);

abort_with_nothing:
	return err;
}

static void
mxge_choose_params(int mtu, int *big_buf_size, int *cl_size, int *nbufs)
{
	int bufsize = mtu + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN + MXGEFW_PAD;

	if (bufsize < MCLBYTES) {
		/* easy, everything fits in a single buffer */
		*big_buf_size = MCLBYTES;
		*cl_size = MCLBYTES;
		*nbufs = 1;
		return;
	}

	if (bufsize < MJUMPAGESIZE) {
		/* still easy, everything still fits in a single buffer */
		*big_buf_size = MJUMPAGESIZE;
		*cl_size = MJUMPAGESIZE;
		*nbufs = 1;
		return;
	}
	/* now we need to use virtually contiguous buffers */
	*cl_size = MJUM9BYTES;
	*big_buf_size = 4096;
	*nbufs = mtu / 4096 + 1;
	/* needs to be a power of two, so round up */
	if (*nbufs == 3)
		*nbufs = 4;
}

static int 
mxge_open(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	int i, err, big_bytes;
	bus_dmamap_t map;
	bus_addr_t bus;
	struct lro_entry *lro_entry;	

	SLIST_INIT(&sc->lro_free);
	SLIST_INIT(&sc->lro_active);

	for (i = 0; i < sc->lro_cnt; i++) {
		lro_entry = (struct lro_entry *)
			malloc(sizeof (*lro_entry), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (lro_entry == NULL) {
			sc->lro_cnt = i;
			break;
		}
		SLIST_INSERT_HEAD(&sc->lro_free, lro_entry, next);
	}

	/* Copy the MAC address in case it was overridden */
	bcopy(IF_LLADDR(sc->ifp), sc->mac_addr, ETHER_ADDR_LEN);

	err = mxge_reset(sc, 1);
	if (err != 0) {
		device_printf(sc->dev, "failed to reset\n");
		return EIO;
	}

	mxge_choose_params(sc->ifp->if_mtu, &big_bytes,
			   &sc->rx_big.cl_size, &sc->rx_big.nbufs);

	cmd.data0 = sc->rx_big.nbufs;
	err = mxge_send_cmd(sc, MXGEFW_CMD_ALWAYS_USE_N_BIG_BUFFERS,
			    &cmd);
	/* error is only meaningful if we're trying to set 
	   MXGEFW_CMD_ALWAYS_USE_N_BIG_BUFFERS > 1 */
	if (err && sc->rx_big.nbufs > 1) {
		device_printf(sc->dev,
			      "Failed to set alway-use-n to %d\n",
			      sc->rx_big.nbufs);
		return EIO;
	}
	/* get the lanai pointers to the send and receive rings */

	err = mxge_send_cmd(sc, MXGEFW_CMD_GET_SEND_OFFSET, &cmd);
	sc->tx.lanai = 
		(volatile mcp_kreq_ether_send_t *)(sc->sram + cmd.data0);
	err |= mxge_send_cmd(sc, 
				 MXGEFW_CMD_GET_SMALL_RX_OFFSET, &cmd);
	sc->rx_small.lanai = 
		(volatile mcp_kreq_ether_recv_t *)(sc->sram + cmd.data0);
	err |= mxge_send_cmd(sc, MXGEFW_CMD_GET_BIG_RX_OFFSET, &cmd);
	sc->rx_big.lanai = 
		(volatile mcp_kreq_ether_recv_t *)(sc->sram + cmd.data0);

	if (err != 0) {
		device_printf(sc->dev, 
			      "failed to get ring sizes or locations\n");
		return EIO;
	}

	/* stock receive rings */
	for (i = 0; i <= sc->rx_small.mask; i++) {
		map = sc->rx_small.info[i].map;
		err = mxge_get_buf_small(sc, map, i);
		if (err) {
			device_printf(sc->dev, "alloced %d/%d smalls\n",
				      i, sc->rx_small.mask + 1);
			goto abort;
		}
	}
	for (i = 0; i <= sc->rx_big.mask; i++) {
		sc->rx_big.shadow[i].addr_low = 0xffffffff;
		sc->rx_big.shadow[i].addr_high = 0xffffffff;
	}
	for (i = 0; i <= sc->rx_big.mask; i += sc->rx_big.nbufs) {
		map = sc->rx_big.info[i].map;
		err = mxge_get_buf_big(sc, map, i);
		if (err) {
			device_printf(sc->dev, "alloced %d/%d bigs\n",
				      i, sc->rx_big.mask + 1);
			goto abort;
		}
	}

	/* Give the firmware the mtu and the big and small buffer
	   sizes.  The firmware wants the big buf size to be a power
	   of two. Luckily, FreeBSD's clusters are powers of two */
	cmd.data0 = sc->ifp->if_mtu + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	err = mxge_send_cmd(sc, MXGEFW_CMD_SET_MTU, &cmd);
	cmd.data0 = MHLEN - MXGEFW_PAD;
	err |= mxge_send_cmd(sc, MXGEFW_CMD_SET_SMALL_BUFFER_SIZE,
			     &cmd);
	cmd.data0 = big_bytes;
	err |= mxge_send_cmd(sc, MXGEFW_CMD_SET_BIG_BUFFER_SIZE, &cmd);

	if (err != 0) {
		device_printf(sc->dev, "failed to setup params\n");
		goto abort;
	}

	/* Now give him the pointer to the stats block */
	cmd.data0 = MXGE_LOWPART_TO_U32(sc->fw_stats_dma.bus_addr);
	cmd.data1 = MXGE_HIGHPART_TO_U32(sc->fw_stats_dma.bus_addr);
	cmd.data2 = sizeof(struct mcp_irq_data);
	err = mxge_send_cmd(sc, MXGEFW_CMD_SET_STATS_DMA_V2, &cmd);

	if (err != 0) {
		bus = sc->fw_stats_dma.bus_addr;
		bus += offsetof(struct mcp_irq_data, send_done_count);
		cmd.data0 = MXGE_LOWPART_TO_U32(bus);
		cmd.data1 = MXGE_HIGHPART_TO_U32(bus);
		err = mxge_send_cmd(sc,
				    MXGEFW_CMD_SET_STATS_DMA_OBSOLETE,
				    &cmd);
		/* Firmware cannot support multicast without STATS_DMA_V2 */
		sc->fw_multicast_support = 0;
	} else {
		sc->fw_multicast_support = 1;
	}

	if (err != 0) {
		device_printf(sc->dev, "failed to setup params\n");
		goto abort;
	}

	/* Finally, start the firmware running */
	err = mxge_send_cmd(sc, MXGEFW_CMD_ETHERNET_UP, &cmd);
	if (err) {
		device_printf(sc->dev, "Couldn't bring up link\n");
		goto abort;
	}
	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return 0;


abort:
	mxge_free_mbufs(sc);

	return err;
}

static int
mxge_close(mxge_softc_t *sc)
{
	struct lro_entry *lro_entry;
	mxge_cmd_t cmd;
	int err, old_down_cnt;

	sc->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	old_down_cnt = sc->down_cnt;
	mb();
	err = mxge_send_cmd(sc, MXGEFW_CMD_ETHERNET_DOWN, &cmd);
	if (err) {
		device_printf(sc->dev, "Couldn't bring down link\n");
	}
	if (old_down_cnt == sc->down_cnt) {
		/* wait for down irq */
		DELAY(10 * sc->intr_coal_delay);
	}
	if (old_down_cnt == sc->down_cnt) {
		device_printf(sc->dev, "never got down irq\n");
	}

	mxge_free_mbufs(sc);

	while (!SLIST_EMPTY(&sc->lro_free)) {
		lro_entry = SLIST_FIRST(&sc->lro_free);
		SLIST_REMOVE_HEAD(&sc->lro_free, next);
	}
	return 0;
}

static void
mxge_setup_cfg_space(mxge_softc_t *sc)
{
	device_t dev = sc->dev;
	int reg;
	uint16_t cmd, lnk, pectl;

	/* find the PCIe link width and set max read request to 4KB*/
	if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
		lnk = pci_read_config(dev, reg + 0x12, 2);
		sc->link_width = (lnk >> 4) & 0x3f;
		
		pectl = pci_read_config(dev, reg + 0x8, 2);
		pectl = (pectl & ~0x7000) | (5 << 12);
		pci_write_config(dev, reg + 0x8, pectl, 2);
	}

	/* Enable DMA and Memory space access */
	pci_enable_busmaster(dev);
	cmd = pci_read_config(dev, PCIR_COMMAND, 2);
	cmd |= PCIM_CMD_MEMEN;
	pci_write_config(dev, PCIR_COMMAND, cmd, 2);
}

static uint32_t
mxge_read_reboot(mxge_softc_t *sc)
{
	device_t dev = sc->dev;
	uint32_t vs;

	/* find the vendor specific offset */
	if (pci_find_extcap(dev, PCIY_VENDOR, &vs) != 0) {
		device_printf(sc->dev,
			      "could not find vendor specific offset\n");
		return (uint32_t)-1;
	}
	/* enable read32 mode */
	pci_write_config(dev, vs + 0x10, 0x3, 1);
	/* tell NIC which register to read */
	pci_write_config(dev, vs + 0x18, 0xfffffff0, 4);
	return (pci_read_config(dev, vs + 0x14, 4));
}

static void
mxge_watchdog_reset(mxge_softc_t *sc)
{
	int err;
	uint32_t reboot;
	uint16_t cmd;

	err = ENXIO;

	device_printf(sc->dev, "Watchdog reset!\n");

	/* 
	 * check to see if the NIC rebooted.  If it did, then all of
	 * PCI config space has been reset, and things like the
	 * busmaster bit will be zero.  If this is the case, then we
	 * must restore PCI config space before the NIC can be used
	 * again
	 */
	cmd = pci_read_config(sc->dev, PCIR_COMMAND, 2);
	if (cmd == 0xffff) {
		/* 
		 * maybe the watchdog caught the NIC rebooting; wait
		 * up to 100ms for it to finish.  If it does not come
		 * back, then give up 
		 */
		DELAY(1000*100);
		cmd = pci_read_config(sc->dev, PCIR_COMMAND, 2);
		if (cmd == 0xffff) {
			device_printf(sc->dev, "NIC disappeared!\n");
			goto abort;
		}
	}
	if ((cmd & PCIM_CMD_BUSMASTEREN) == 0) {
		/* print the reboot status */
		reboot = mxge_read_reboot(sc);
		device_printf(sc->dev, "NIC rebooted, status = 0x%x\n",
			      reboot);
		/* restore PCI configuration space */

		/* XXXX waiting for pci_cfg_restore() to be exported */
		goto abort; /* just abort for now */

		/* and redo any changes we made to our config space */
		mxge_setup_cfg_space(sc);
	} else {
		device_printf(sc->dev, "NIC did not reboot, ring state:\n");
		device_printf(sc->dev, "tx.req=%d tx.done=%d\n",
			      sc->tx.req, sc->tx.done);
		device_printf(sc->dev, "pkt_done=%d fw=%d\n",
			      sc->tx.pkt_done,
			      be32toh(sc->fw_stats->send_done_count));
	}

	if (sc->ifp->if_drv_flags & IFF_DRV_RUNNING) {
		mxge_close(sc);
		err = mxge_open(sc);
	}

abort:
	/* 
	 * stop the watchdog if the nic is dead, to avoid spamming the
	 * console
	 */
	if (err != 0) {
		callout_stop(&sc->co_hdl);
	}
}

static void
mxge_watchdog(mxge_softc_t *sc)
{
	mxge_tx_buf_t *tx = &sc->tx;

	/* see if we have outstanding transmits, which
	   have been pending for more than mxge_ticks */
	if (tx->req != tx->done &&
	    tx->watchdog_req != tx->watchdog_done &&
	    tx->done == tx->watchdog_done)
		mxge_watchdog_reset(sc);

	tx->watchdog_req = tx->req;
	tx->watchdog_done = tx->done;
}

static void
mxge_tick(void *arg)
{
	mxge_softc_t *sc = arg;


	/* Synchronize with possible callout reset/stop. */
	if (callout_pending(&sc->co_hdl) ||
	    !callout_active(&sc->co_hdl)) {
		mtx_unlock(&sc->driver_mtx);
		return;
	}

	callout_reset(&sc->co_hdl, mxge_ticks, mxge_tick, sc);
	mxge_watchdog(sc);
}

static int
mxge_media_change(struct ifnet *ifp)
{
	return EINVAL;
}

static int
mxge_change_mtu(mxge_softc_t *sc, int mtu)
{
	struct ifnet *ifp = sc->ifp;
	int real_mtu, old_mtu;
	int err = 0;


	real_mtu = mtu + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	if ((real_mtu > sc->max_mtu) || real_mtu < 60)
		return EINVAL;
	mtx_lock(&sc->driver_mtx);
	old_mtu = ifp->if_mtu;
	ifp->if_mtu = mtu;
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		callout_stop(&sc->co_hdl);
		mxge_close(sc);
		err = mxge_open(sc);
		if (err != 0) {
			ifp->if_mtu = old_mtu;
			mxge_close(sc);
			(void) mxge_open(sc);
		}
		callout_reset(&sc->co_hdl, mxge_ticks, mxge_tick, sc);
	}
	mtx_unlock(&sc->driver_mtx);
	return err;
}	

static void
mxge_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	mxge_softc_t *sc = ifp->if_softc;
	

	if (sc == NULL)
		return;
	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_status |= sc->fw_stats->link_up ? IFM_ACTIVE : 0;
	ifmr->ifm_active = IFM_AUTO | IFM_ETHER;
	ifmr->ifm_active |= sc->fw_stats->link_up ? IFM_FDX : 0;
}

static int
mxge_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	mxge_softc_t *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int err, mask;

	err = 0;
	switch (command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		err = ether_ioctl(ifp, command, data);
		break;

	case SIOCSIFMTU:
		err = mxge_change_mtu(sc, ifr->ifr_mtu);
		break;

	case SIOCSIFFLAGS:
		mtx_lock(&sc->driver_mtx);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				err = mxge_open(sc);
				callout_reset(&sc->co_hdl, mxge_ticks,
					      mxge_tick, sc);
			} else {
				/* take care of promis can allmulti
				   flag chages */
				mxge_change_promisc(sc, 
						    ifp->if_flags & IFF_PROMISC);
				mxge_set_multicast_list(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				mxge_close(sc);
				callout_stop(&sc->co_hdl);
			}
		}
		mtx_unlock(&sc->driver_mtx);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mtx_lock(&sc->driver_mtx);
		mxge_set_multicast_list(sc);
		mtx_unlock(&sc->driver_mtx);
		break;

	case SIOCSIFCAP:
		mtx_lock(&sc->driver_mtx);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_TXCSUM) {
			if (IFCAP_TXCSUM & ifp->if_capenable) {
				ifp->if_capenable &= ~(IFCAP_TXCSUM);
				ifp->if_hwassist &= ~(CSUM_TCP | CSUM_UDP);
			} else {
				ifp->if_capenable |= IFCAP_TXCSUM;
				ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
			}
		} else if (mask & IFCAP_RXCSUM) {
			if (IFCAP_RXCSUM & ifp->if_capenable) {
				ifp->if_capenable &= ~IFCAP_RXCSUM;
				sc->csum_flag = 0;
			} else {
				ifp->if_capenable |= IFCAP_RXCSUM;
				sc->csum_flag = 1;
			}
		}
		mtx_unlock(&sc->driver_mtx);

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
mxge_fetch_tunables(mxge_softc_t *sc)
{
	
	TUNABLE_INT_FETCH("hw.mxge.flow_control_enabled", 
			  &mxge_flow_control);
	TUNABLE_INT_FETCH("hw.mxge.intr_coal_delay", 
			  &mxge_intr_coal_delay);	
	TUNABLE_INT_FETCH("hw.mxge.nvidia_ecrc_enable", 
			  &mxge_nvidia_ecrc_enable);	
	TUNABLE_INT_FETCH("hw.mxge.force_firmware", 
			  &mxge_force_firmware);	
	TUNABLE_INT_FETCH("hw.mxge.deassert_wait", 
			  &mxge_deassert_wait);	
	TUNABLE_INT_FETCH("hw.mxge.verbose", 
			  &mxge_verbose);	
	TUNABLE_INT_FETCH("hw.mxge.ticks", &mxge_ticks);
	TUNABLE_INT_FETCH("hw.mxge.lro_cnt", &sc->lro_cnt);

	if (bootverbose)
		mxge_verbose = 1;
	if (mxge_intr_coal_delay < 0 || mxge_intr_coal_delay > 10*1000)
		mxge_intr_coal_delay = 30;
	if (mxge_ticks == 0)
		mxge_ticks = hz;	
	sc->pause = mxge_flow_control;

}

static int 
mxge_attach(device_t dev)
{
	mxge_softc_t *sc = device_get_softc(dev);
	struct ifnet *ifp;
	int count, rid, err;

	sc->dev = dev;
	mxge_fetch_tunables(sc);

	err = bus_dma_tag_create(NULL,			/* parent */
				 1,			/* alignment */
				 4096,			/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 65536 + 256,		/* maxsize */
				 MXGE_MAX_SEND_DESC, 	/* num segs */
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
	snprintf(sc->cmd_mtx_name, sizeof(sc->cmd_mtx_name), "%s:cmd",
		 device_get_nameunit(dev));
	mtx_init(&sc->cmd_mtx, sc->cmd_mtx_name, NULL, MTX_DEF);
	snprintf(sc->tx_mtx_name, sizeof(sc->tx_mtx_name), "%s:tx", 
		 device_get_nameunit(dev));
	mtx_init(&sc->tx_mtx, sc->tx_mtx_name, NULL, MTX_DEF);
	snprintf(sc->driver_mtx_name, sizeof(sc->driver_mtx_name),
		 "%s:drv", device_get_nameunit(dev));
	mtx_init(&sc->driver_mtx, sc->driver_mtx_name,
		 MTX_NETWORK_LOCK, MTX_DEF);

	callout_init_mtx(&sc->co_hdl, &sc->driver_mtx, 0);

	mxge_setup_cfg_space(sc);
	
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
	bzero(sc->eeprom_strings, MXGE_EEPROM_STRINGS_SIZE);
	bus_space_read_region_1(rman_get_bustag(sc->mem_res),
				rman_get_bushandle(sc->mem_res),
				sc->sram_size - MXGE_EEPROM_STRINGS_SIZE,
				sc->eeprom_strings, 
				MXGE_EEPROM_STRINGS_SIZE - 2);
	err = mxge_parse_strings(sc);
	if (err != 0)
		goto abort_with_mem_res;

	/* Enable write combining for efficient use of PCIe bus */
	mxge_enable_wc(sc);

	/* Allocate the out of band dma memory */
	err = mxge_dma_alloc(sc, &sc->cmd_dma, 
			     sizeof (mxge_cmd_t), 64);
	if (err != 0) 
		goto abort_with_mem_res;
	sc->cmd = (mcp_cmd_response_t *) sc->cmd_dma.addr;
	err = mxge_dma_alloc(sc, &sc->zeropad_dma, 64, 64);
	if (err != 0) 
		goto abort_with_cmd_dma;

	err = mxge_dma_alloc(sc, &sc->fw_stats_dma, 
			     sizeof (*sc->fw_stats), 64);
	if (err != 0) 
		goto abort_with_zeropad_dma;
	sc->fw_stats = (mcp_irq_data_t *)sc->fw_stats_dma.addr;

	err = mxge_dma_alloc(sc, &sc->dmabench_dma, 4096, 4096);
	if (err != 0)
		goto abort_with_fw_stats;

	/* Add our ithread  */
	count = pci_msi_count(dev);
	if (count == 1 && pci_alloc_msi(dev, &count) == 0) {
		rid = 1;
		sc->msi_enabled = 1;
	} else {
		rid = 0;
	}
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0,
					 1, RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not alloc interrupt\n");
		goto abort_with_dmabench;
	}
	if (mxge_verbose)
		device_printf(dev, "using %s irq %ld\n",
			      sc->msi_enabled ? "MSI" : "INTx",
			      rman_get_start(sc->irq_res));
	/* select & load the firmware */
	err = mxge_select_firmware(sc);
	if (err != 0)
		goto abort_with_irq_res;
	sc->intr_coal_delay = mxge_intr_coal_delay;
	err = mxge_reset(sc, 0);
	if (err != 0)
		goto abort_with_irq_res;

	err = mxge_alloc_rings(sc);
	if (err != 0) {
		device_printf(sc->dev, "failed to allocate rings\n");
		goto abort_with_irq_res;
	}

	err = bus_setup_intr(sc->dev, sc->irq_res, 
			     INTR_TYPE_NET | INTR_MPSAFE,
			     mxge_intr, sc, &sc->ih);
	if (err != 0) {
		goto abort_with_rings;
	}
	/* hook into the network stack */
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_baudrate = 100000000;
	ifp->if_capabilities = IFCAP_RXCSUM | IFCAP_TXCSUM | IFCAP_VLAN_MTU;

	sc->max_mtu = mxge_max_mtu(sc);
	if (sc->max_mtu >= 9000)
		ifp->if_capabilities |= IFCAP_JUMBO_MTU;
	else
		device_printf(dev, "MTU limited to %d.  Install "
			      "latest firmware for 9000 byte jumbo support\n",
			      sc->max_mtu - ETHER_HDR_LEN);
	ifp->if_hwassist = CSUM_TCP | CSUM_UDP;
	ifp->if_capenable = ifp->if_capabilities;
	sc->csum_flag = 1;
        ifp->if_init = mxge_init;
        ifp->if_softc = sc;
        ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
        ifp->if_ioctl = mxge_ioctl;
        ifp->if_start = mxge_start;
	ether_ifattach(ifp, sc->mac_addr);
	/* ether_ifattach sets mtu to 1500 */
	if (ifp->if_capabilities & IFCAP_JUMBO_MTU)
		ifp->if_mtu = 9000;

	/* Initialise the ifmedia structure */
	ifmedia_init(&sc->media, 0, mxge_media_change, 
		     mxge_media_status);
	ifmedia_add(&sc->media, IFM_ETHER|IFM_AUTO, 0, NULL);
	mxge_add_sysctls(sc);
	return 0;

abort_with_rings:
	mxge_free_rings(sc);
abort_with_irq_res:
	bus_release_resource(dev, SYS_RES_IRQ,
			     sc->msi_enabled ? 1 : 0, sc->irq_res);
	if (sc->msi_enabled)
		pci_release_msi(dev);
abort_with_dmabench:
	mxge_dma_free(&sc->dmabench_dma);
abort_with_fw_stats:
	mxge_dma_free(&sc->fw_stats_dma);
abort_with_zeropad_dma:
	mxge_dma_free(&sc->zeropad_dma);
abort_with_cmd_dma:
	mxge_dma_free(&sc->cmd_dma);
abort_with_mem_res:
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BARS, sc->mem_res);
abort_with_lock:
	pci_disable_busmaster(dev);
	mtx_destroy(&sc->cmd_mtx);
	mtx_destroy(&sc->tx_mtx);
	mtx_destroy(&sc->driver_mtx);
	if_free(ifp);
abort_with_parent_dmat:
	bus_dma_tag_destroy(sc->parent_dmat);

abort_with_nothing:
	return err;
}

static int
mxge_detach(device_t dev)
{
	mxge_softc_t *sc = device_get_softc(dev);

	mtx_lock(&sc->driver_mtx);
	if (sc->ifp->if_drv_flags & IFF_DRV_RUNNING)
		mxge_close(sc);
	callout_stop(&sc->co_hdl);
	mtx_unlock(&sc->driver_mtx);
	ether_ifdetach(sc->ifp);
	ifmedia_removeall(&sc->media);
	mxge_dummy_rdma(sc, 0);
	bus_teardown_intr(sc->dev, sc->irq_res, sc->ih);
	mxge_free_rings(sc);
	bus_release_resource(dev, SYS_RES_IRQ,
			     sc->msi_enabled ? 1 : 0, sc->irq_res);
	if (sc->msi_enabled)
		pci_release_msi(dev);

	sc->rx_done.entry = NULL;
	mxge_dma_free(&sc->rx_done.dma);
	mxge_dma_free(&sc->fw_stats_dma);
	mxge_dma_free(&sc->dmabench_dma);
	mxge_dma_free(&sc->zeropad_dma);
	mxge_dma_free(&sc->cmd_dma);
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BARS, sc->mem_res);
	pci_disable_busmaster(dev);
	mtx_destroy(&sc->cmd_mtx);
	mtx_destroy(&sc->tx_mtx);
	mtx_destroy(&sc->driver_mtx);
	if_free(sc->ifp);
	bus_dma_tag_destroy(sc->parent_dmat);
	return 0;
}

static int
mxge_shutdown(device_t dev)
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
