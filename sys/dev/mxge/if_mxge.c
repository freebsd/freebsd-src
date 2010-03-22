/******************************************************************************

Copyright (c) 2006-2009, Myricom Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Myricom Inc, nor the names of its
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
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>

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
#include <sys/smp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h> /* XXX for pci_cfg_restore */

#include <vm/vm.h>		/* for pmap_mapdev() */
#include <vm/pmap.h>

#if defined(__i386) || defined(__amd64)
#include <machine/specialreg.h>
#endif

#include <dev/mxge/mxge_mcp.h>
#include <dev/mxge/mcp_gen_header.h>
/*#define MXGE_FAKE_IFP*/
#include <dev/mxge/if_mxge_var.h>

/* tunable params */
static int mxge_nvidia_ecrc_enable = 1;
static int mxge_force_firmware = 0;
static int mxge_intr_coal_delay = 30;
static int mxge_deassert_wait = 1;
static int mxge_flow_control = 1;
static int mxge_verbose = 0;
static int mxge_lro_cnt = 8;
static int mxge_ticks;
static int mxge_max_slices = 1;
static int mxge_rss_hash_type = MXGEFW_RSS_HASH_TYPE_SRC_PORT;
static int mxge_always_promisc = 0;
static int mxge_initial_mtu = ETHERMTU_JUMBO;
static int mxge_throttle = 0;
static char *mxge_fw_unaligned = "mxge_ethp_z8e";
static char *mxge_fw_aligned = "mxge_eth_z8e";
static char *mxge_fw_rss_aligned = "mxge_rss_eth_z8e";
static char *mxge_fw_rss_unaligned = "mxge_rss_ethp_z8e";

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
MODULE_DEPEND(mxge, zlib, 1, 1, 1);

static int mxge_load_firmware(mxge_softc_t *sc, int adopt);
static int mxge_send_cmd(mxge_softc_t *sc, uint32_t cmd, mxge_cmd_t *data);
static int mxge_close(mxge_softc_t *sc, int down);
static int mxge_open(mxge_softc_t *sc);
static void mxge_tick(void *arg);

static int
mxge_probe(device_t dev)
{
	int rev;


	if ((pci_get_vendor(dev) == MXGE_PCI_VENDOR_MYRICOM) &&
	    ((pci_get_device(dev) == MXGE_PCI_DEVICE_Z8E) ||
	     (pci_get_device(dev) == MXGE_PCI_DEVICE_Z8E_9))) {
		rev = pci_get_revid(dev);
		switch (rev) {
		case MXGE_PCI_REV_Z8E:
			device_set_desc(dev, "Myri10G-PCIE-8A");
			break;
		case MXGE_PCI_REV_Z8ES:
			device_set_desc(dev, "Myri10G-PCIE-8B");
			break;
		default:
			device_set_desc(dev, "Myri10G-PCIE-8??");
			device_printf(dev, "Unrecognized rev %d NIC\n",
				      rev);
			break;	
		}
		return 0;
	}
	return ENXIO;
}

static void
mxge_enable_wc(mxge_softc_t *sc)
{
#if defined(__i386) || defined(__amd64)
	vm_offset_t len;
	int err;

	sc->wc = 1;
	len = rman_get_size(sc->mem_res);
	err = pmap_change_attr((vm_offset_t) sc->sram,
			       len, PAT_WRITE_COMBINING);
	if (err != 0) {
		device_printf(sc->dev, "pmap_change_attr failed, %d\n",
			      err);
		sc->wc = 0;
	}
#endif		
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
	bus_size_t boundary, maxsegsize;

	if (bytes > 4096 && alignment == 4096) {
		boundary = 0;
		maxsegsize = bytes;
	} else {
		boundary = 4096;
		maxsegsize = 4096;
	}

	/* allocate DMAable memory tags */
	err = bus_dma_tag_create(sc->parent_dmat,	/* parent */
				 alignment,		/* alignment */
				 boundary,		/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 bytes,			/* maxsize */
				 1,			/* num segs */
				 maxsegsize,		/* maxsegsize */
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
mxge_enable_nvidia_ecrc(mxge_softc_t *sc)
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

	len = sc->tx_boundary;

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
 * larger than 2KB by setting the tx_boundary to 2KB.  If ECRC is
 * enabled, then the driver should use the aligned (eth_z8e.dat)
 * firmware image, and set tx_boundary to 4KB.
 */

static int
mxge_firmware_probe(mxge_softc_t *sc)
{
	device_t dev = sc->dev;
	int reg, status;
	uint16_t pectl;

	sc->tx_boundary = 4096;
	/*
	 * Verify the max read request size was set to 4KB
	 * before trying the test with 4KB.
	 */
	if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
		pectl = pci_read_config(dev, reg + 0x8, 2);
		if ((pectl & (5 << 12)) != (5 << 12)) {
			device_printf(dev, "Max Read Req. size != 4k (0x%x\n",
				      pectl);
			sc->tx_boundary = 2048;
		}
	}

	/* 
	 * load the optimized firmware (which assumes aligned PCIe
	 * completions) in order to see if it works on this host.
	 */
	sc->fw_name = mxge_fw_aligned;
	status = mxge_load_firmware(sc, 1);
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
	int force_firmware = mxge_force_firmware;

	if (sc->throttle)
		force_firmware = sc->throttle;

	if (force_firmware != 0) {
		if (force_firmware == 1)
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
		sc->tx_boundary = 4096;
	} else {
		sc->fw_name = mxge_fw_unaligned;
		sc->tx_boundary = 2048;
	}
	return (mxge_load_firmware(sc, 0));
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

static void *
z_alloc(void *nil, u_int items, u_int size)
{
        void *ptr;

        ptr = malloc(items * size, M_TEMP, M_NOWAIT);
        return ptr;
}

static void
z_free(void *nil, void *ptr)
{
        free(ptr, M_TEMP);
}


static int
mxge_load_firmware_helper(mxge_softc_t *sc, uint32_t *limit)
{
	z_stream zs;
	char *inflate_buffer;
	const struct firmware *fw;
	const mcp_gen_header_t *hdr;
	unsigned hdr_offset;
	int status;
	unsigned int i;
	char dummy;
	size_t fw_len;

	fw = firmware_get(sc->fw_name);
	if (fw == NULL) {
		device_printf(sc->dev, "Could not find firmware image %s\n",
			      sc->fw_name);
		return ENOENT;
	}



	/* setup zlib and decompress f/w */
	bzero(&zs, sizeof (zs));
	zs.zalloc = z_alloc;
	zs.zfree = z_free;
	status = inflateInit(&zs);
	if (status != Z_OK) {
		status = EIO;
		goto abort_with_fw;
	}

	/* the uncompressed size is stored as the firmware version,
	   which would otherwise go unused */
	fw_len = (size_t) fw->version; 
	inflate_buffer = malloc(fw_len, M_TEMP, M_NOWAIT);
	if (inflate_buffer == NULL)
		goto abort_with_zs;
	zs.avail_in = fw->datasize;
	zs.next_in = __DECONST(char *, fw->data);
	zs.avail_out = fw_len;
	zs.next_out = inflate_buffer;
	status = inflate(&zs, Z_FINISH);
	if (status != Z_STREAM_END) {
		device_printf(sc->dev, "zlib %d\n", status);
		status = EIO;
		goto abort_with_buffer;
	}

	/* check id */
	hdr_offset = htobe32(*(const uint32_t *)
			     (inflate_buffer + MCP_HEADER_PTR_OFFSET));
	if ((hdr_offset & 3) || hdr_offset + sizeof(*hdr) > fw_len) {
		device_printf(sc->dev, "Bad firmware file");
		status = EIO;
		goto abort_with_buffer;
	}
	hdr = (const void*)(inflate_buffer + hdr_offset); 

	status = mxge_validate_firmware(sc, hdr);
	if (status != 0)
		goto abort_with_buffer;

	/* Copy the inflated firmware to NIC SRAM. */
	for (i = 0; i < fw_len; i += 256) {
		mxge_pio_copy(sc->sram + MXGE_FW_OFFSET + i,
			      inflate_buffer + i,
			      min(256U, (unsigned)(fw_len - i)));
		mb();
		dummy = *sc->sram;
		mb();
	}

	*limit = fw_len;
	status = 0;
abort_with_buffer:
	free(inflate_buffer, M_TEMP);
abort_with_zs:
	inflateEnd(&zs);
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
		case MXGEFW_CMD_ERROR_BUSY:
			err = EBUSY;
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
mxge_load_firmware(mxge_softc_t *sc, int adopt)
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
		if (!adopt)
			return status;
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
		if (sc->tx_boundary == 4096) {
			device_printf(sc->dev,
				"Using firmware currently running on NIC"
				 ".  For optimal\n");
			device_printf(sc->dev,
				 "performance consider loading optimized "
				 "firmware\n");
		}
		sc->fw_name = mxge_fw_unaligned;
		sc->tx_boundary = 2048;
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

	if (mxge_always_promisc)
		promisc = 1;

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
	struct mxge_slice_state *ss;
	mxge_rx_done_t *rx_done;
	volatile uint32_t *irq_claim;
	mxge_cmd_t cmd;
	int slice, status;

	/* try to send a reset command to the card to see if it
	   is alive */
	memset(&cmd, 0, sizeof (cmd));
	status = mxge_send_cmd(sc, MXGEFW_CMD_RESET, &cmd);
	if (status != 0) {
		device_printf(sc->dev, "failed reset\n");
		return ENXIO;
	}

	mxge_dummy_rdma(sc, 1);


	/* set the intrq size */
	cmd.data0 = sc->rx_ring_size;
	status = mxge_send_cmd(sc, MXGEFW_CMD_SET_INTRQ_SIZE, &cmd);

	/* 
	 * Even though we already know how many slices are supported
	 * via mxge_slice_probe(), MXGEFW_CMD_GET_MAX_RSS_QUEUES
	 * has magic side effects, and must be called after a reset.
	 * It must be called prior to calling any RSS related cmds,
	 * including assigning an interrupt queue for anything but
	 * slice 0.  It must also be called *after*
	 * MXGEFW_CMD_SET_INTRQ_SIZE, since the intrq size is used by
	 * the firmware to compute offsets.
	 */
	 
	if (sc->num_slices > 1) {
		/* ask the maximum number of slices it supports */
		status = mxge_send_cmd(sc, MXGEFW_CMD_GET_MAX_RSS_QUEUES,
					   &cmd);
		if (status != 0) {
			device_printf(sc->dev, 
				      "failed to get number of slices\n");
			return status;
		}
		/* 
		 * MXGEFW_CMD_ENABLE_RSS_QUEUES must be called prior
		 * to setting up the interrupt queue DMA
		 */
		cmd.data0 = sc->num_slices;
		cmd.data1 = MXGEFW_SLICE_INTR_MODE_ONE_PER_SLICE;
		status = mxge_send_cmd(sc, MXGEFW_CMD_ENABLE_RSS_QUEUES,
					   &cmd);
		if (status != 0) {
			device_printf(sc->dev,
				      "failed to set number of slices\n");
			return status;
		}
	}


	if (interrupts_setup) {
		/* Now exchange information about interrupts  */
		for (slice = 0; slice < sc->num_slices; slice++) {
			rx_done = &sc->ss[slice].rx_done;
			memset(rx_done->entry, 0, sc->rx_ring_size);
			cmd.data0 = MXGE_LOWPART_TO_U32(rx_done->dma.bus_addr);
			cmd.data1 = MXGE_HIGHPART_TO_U32(rx_done->dma.bus_addr);
			cmd.data2 = slice;
			status |= mxge_send_cmd(sc,
						MXGEFW_CMD_SET_INTRQ_DMA,
						&cmd);
		}
	}

	status |= mxge_send_cmd(sc, 
				MXGEFW_CMD_GET_INTR_COAL_DELAY_OFFSET, &cmd);
	

	sc->intr_coal_delay_ptr = (volatile uint32_t *)(sc->sram + cmd.data0);

	status |= mxge_send_cmd(sc, MXGEFW_CMD_GET_IRQ_ACK_OFFSET, &cmd);
	irq_claim = (volatile uint32_t *)(sc->sram + cmd.data0);


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

	for (slice = 0; slice < sc->num_slices; slice++) {
		ss = &sc->ss[slice];

		ss->irq_claim = irq_claim + (2 * slice);
		/* reset mcp/driver shared state back to 0 */
		ss->rx_done.idx = 0;
		ss->rx_done.cnt = 0;
		ss->tx.req = 0;
		ss->tx.done = 0;
		ss->tx.pkt_done = 0;
		ss->tx.wake = 0;
		ss->tx.defrag = 0;
		ss->tx.stall = 0;
		ss->rx_big.cnt = 0;
		ss->rx_small.cnt = 0;
		ss->lro_bad_csum = 0;
		ss->lro_queued = 0;
		ss->lro_flushed = 0;
		if (ss->fw_stats != NULL) {
			bzero(ss->fw_stats, sizeof *ss->fw_stats);
		}
	}
	sc->rdma_tags_available = 15;
	status = mxge_update_mac_address(sc);
	mxge_change_promisc(sc, sc->ifp->if_flags & IFF_PROMISC);
	mxge_change_pause(sc, sc->pause);
	mxge_set_multicast_list(sc);
	if (sc->throttle) {
		cmd.data0 = sc->throttle;
		if (mxge_send_cmd(sc, MXGEFW_CMD_SET_THROTTLE_FACTOR,
				  &cmd)) {
			device_printf(sc->dev,
				      "can't enable throttle\n");
		}
	}
	return status;
}

static int
mxge_change_throttle(SYSCTL_HANDLER_ARGS)
{
	mxge_cmd_t cmd;
	mxge_softc_t *sc;
	int err;
	unsigned int throttle;

	sc = arg1;
	throttle = sc->throttle;
	err = sysctl_handle_int(oidp, &throttle, arg2, req);
        if (err != 0) {
                return err;
        }

	if (throttle == sc->throttle)
		return 0;

        if (throttle < MXGE_MIN_THROTTLE || throttle > MXGE_MAX_THROTTLE)
                return EINVAL;
	
	mtx_lock(&sc->driver_mtx);
	cmd.data0 = throttle;
	err = mxge_send_cmd(sc, MXGEFW_CMD_SET_THROTTLE_FACTOR, &cmd);
	if (err == 0)
		sc->throttle = throttle;
	mtx_unlock(&sc->driver_mtx);	
	return err;
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
mxge_change_lro_locked(mxge_softc_t *sc, int lro_cnt)
{
	struct ifnet *ifp;
	int err = 0;

	ifp = sc->ifp;
	if (lro_cnt == 0) 
		ifp->if_capenable &= ~IFCAP_LRO;
	else
		ifp->if_capenable |= IFCAP_LRO;
	sc->lro_cnt = lro_cnt;
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		mxge_close(sc, 0);
		err = mxge_open(sc);
	}
	return err;
}

static int
mxge_change_lro(SYSCTL_HANDLER_ARGS)
{
	mxge_softc_t *sc;
	unsigned int lro_cnt;
	int err;

	sc = arg1;
	lro_cnt = sc->lro_cnt;
	err = sysctl_handle_int(oidp, &lro_cnt, arg2, req);
	if (err != 0)
		return err;

	if (lro_cnt == sc->lro_cnt)
		return 0;

	if (lro_cnt > 128)
		return EINVAL;

	mtx_lock(&sc->driver_mtx);
	err = mxge_change_lro_locked(sc, lro_cnt);
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
mxge_rem_sysctls(mxge_softc_t *sc)
{
	struct mxge_slice_state *ss;
	int slice;

	if (sc->slice_sysctl_tree == NULL)
		return;

	for (slice = 0; slice < sc->num_slices; slice++) {
		ss = &sc->ss[slice];
		if (ss == NULL || ss->sysctl_tree == NULL)
			continue;
		sysctl_ctx_free(&ss->sysctl_ctx);
		ss->sysctl_tree = NULL;
	}
	sysctl_ctx_free(&sc->slice_sysctl_ctx);
	sc->slice_sysctl_tree = NULL;
}

static void
mxge_add_sysctls(mxge_softc_t *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;
	mcp_irq_data_t *fw;
	struct mxge_slice_state *ss;
	int slice;
	char slice_num[8];

	ctx = device_get_sysctl_ctx(sc->dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));
	fw = sc->ss[0].fw_stats;

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
		       CTLFLAG_RD, &sc->tx_boundary,
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
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "watchdog_resets",
		       CTLFLAG_RD, &sc->watchdog_resets,
		       0, "Number of times NIC was reset");


	/* performance related tunables */
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"intr_coal_delay",
			CTLTYPE_INT|CTLFLAG_RW, sc,
			0, mxge_change_intr_coal, 
			"I", "interrupt coalescing delay in usecs");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"throttle",
			CTLTYPE_INT|CTLFLAG_RW, sc,
			0, mxge_change_throttle, 
			"I", "transmit throttling");

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

	/* verbose printing? */
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		       "verbose",
		       CTLFLAG_RW, &mxge_verbose,
		       0, "verbose printing");

	/* lro */
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO, 
			"lro_cnt",
			CTLTYPE_INT|CTLFLAG_RW, sc,
			0, mxge_change_lro,
			"I", "number of lro merge queues");


	/* add counters exported for debugging from all slices */
	sysctl_ctx_init(&sc->slice_sysctl_ctx);
	sc->slice_sysctl_tree = 
		SYSCTL_ADD_NODE(&sc->slice_sysctl_ctx, children, OID_AUTO,
				"slice", CTLFLAG_RD, 0, "");

	for (slice = 0; slice < sc->num_slices; slice++) {
		ss = &sc->ss[slice];
		sysctl_ctx_init(&ss->sysctl_ctx);
		ctx = &ss->sysctl_ctx;
		children = SYSCTL_CHILDREN(sc->slice_sysctl_tree);
		sprintf(slice_num, "%d", slice);
		ss->sysctl_tree = 
			SYSCTL_ADD_NODE(ctx, children, OID_AUTO, slice_num,
					CTLFLAG_RD, 0, "");
		children = SYSCTL_CHILDREN(ss->sysctl_tree);
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
			       "rx_small_cnt",
			       CTLFLAG_RD, &ss->rx_small.cnt,
			       0, "rx_small_cnt");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
			       "rx_big_cnt",
			       CTLFLAG_RD, &ss->rx_big.cnt,
			       0, "rx_small_cnt");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
			       "tx_req",
			       CTLFLAG_RD, &ss->tx.req,
			       0, "tx_req");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO,
			       "lro_flushed", CTLFLAG_RD, &ss->lro_flushed,
			       0, "number of lro merge queues flushed");

		SYSCTL_ADD_INT(ctx, children, OID_AUTO,
			       "lro_queued", CTLFLAG_RD, &ss->lro_queued,
			       0, "number of frames appended to lro merge"
			       "queues");

		/* only transmit from slice 0 for now */
		if (slice > 0)
			continue;

		SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
			       "tx_done",
			       CTLFLAG_RD, &ss->tx.done,
			       0, "tx_done");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
			       "tx_pkt_done",
			       CTLFLAG_RD, &ss->tx.pkt_done,
			       0, "tx_done");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
			       "tx_stall",
			       CTLFLAG_RD, &ss->tx.stall,
			       0, "tx_stall");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
			       "tx_wake",
			       CTLFLAG_RD, &ss->tx.wake,
			       0, "tx_wake");
		SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
			       "tx_defrag",
			       CTLFLAG_RD, &ss->tx.defrag,
			       0, "tx_defrag");
	}
}

/* copy an array of mcp_kreq_ether_send_t's to the mcp.  Copy 
   backwards one at a time and handle ring wraps */

static inline void 
mxge_submit_req_backwards(mxge_tx_ring_t *tx, 
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
mxge_submit_req(mxge_tx_ring_t *tx, mcp_kreq_ether_send_t *src, 
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

#if IFCAP_TSO4

static void
mxge_encap_tso(struct mxge_slice_state *ss, struct mbuf *m,
	       int busdma_seg_cnt, int ip_off)
{
	mxge_tx_ring_t *tx;
	mcp_kreq_ether_send_t *req;
	bus_dma_segment_t *seg;
	struct ip *ip;
	struct tcphdr *tcp;
	uint32_t low, high_swapped;
	int len, seglen, cum_len, cum_len_next;
	int next_is_first, chop, cnt, rdma_count, small;
	uint16_t pseudo_hdr_offset, cksum_offset, mss;
	uint8_t flags, flags_next;
	static int once;

	mss = m->m_pkthdr.tso_segsz;

	/* negative cum_len signifies to the
	 * send loop that we are still in the
	 * header portion of the TSO packet.
	 */

	/* ensure we have the ethernet, IP and TCP
	   header together in the first mbuf, copy
	   it to a scratch buffer if not */
	if (__predict_false(m->m_len < ip_off + sizeof (*ip))) {
		m_copydata(m, 0, ip_off + sizeof (*ip),
			   ss->scratch);
		ip = (struct ip *)(ss->scratch + ip_off);
	} else {
		ip = (struct ip *)(mtod(m, char *) + ip_off);
	}
	if (__predict_false(m->m_len < ip_off + (ip->ip_hl << 2)
			    + sizeof (*tcp))) {
		m_copydata(m, 0, ip_off + (ip->ip_hl << 2)
			   + sizeof (*tcp),  ss->scratch);
		ip = (struct ip *)(mtod(m, char *) + ip_off);
	} 

	tcp = (struct tcphdr *)((char *)ip + (ip->ip_hl << 2));
	cum_len = -(ip_off + ((ip->ip_hl + tcp->th_off) << 2));

	/* TSO implies checksum offload on this hardware */
	cksum_offset = ip_off + (ip->ip_hl << 2);
	flags = MXGEFW_FLAGS_TSO_HDR | MXGEFW_FLAGS_FIRST;

	
	/* for TSO, pseudo_hdr_offset holds mss.
	 * The firmware figures out where to put
	 * the checksum by parsing the header. */
	pseudo_hdr_offset = htobe16(mss);

	tx = &ss->tx;
	req = tx->req_list;
	seg = tx->seg_list;
	cnt = 0;
	rdma_count = 0;
	/* "rdma_count" is the number of RDMAs belonging to the
	 * current packet BEFORE the current send request. For
	 * non-TSO packets, this is equal to "count".
	 * For TSO packets, rdma_count needs to be reset
	 * to 0 after a segment cut.
	 *
	 * The rdma_count field of the send request is
	 * the number of RDMAs of the packet starting at
	 * that request. For TSO send requests with one ore more cuts
	 * in the middle, this is the number of RDMAs starting
	 * after the last cut in the request. All previous
	 * segments before the last cut implicitly have 1 RDMA.
	 *
	 * Since the number of RDMAs is not known beforehand,
	 * it must be filled-in retroactively - after each
	 * segmentation cut or at the end of the entire packet.
	 */

	while (busdma_seg_cnt) {
		/* Break the busdma segment up into pieces*/
		low = MXGE_LOWPART_TO_U32(seg->ds_addr);
		high_swapped = 	htobe32(MXGE_HIGHPART_TO_U32(seg->ds_addr));
		len = seg->ds_len;

		while (len) {
			flags_next = flags & ~MXGEFW_FLAGS_FIRST;
			seglen = len;
			cum_len_next = cum_len + seglen;
			(req-rdma_count)->rdma_count = rdma_count + 1;
			if (__predict_true(cum_len >= 0)) {
				/* payload */
				chop = (cum_len_next > mss);
				cum_len_next = cum_len_next % mss;
				next_is_first = (cum_len_next == 0);
				flags |= chop * MXGEFW_FLAGS_TSO_CHOP;
				flags_next |= next_is_first *
					MXGEFW_FLAGS_FIRST;
				rdma_count |= -(chop | next_is_first);
				rdma_count += chop & !next_is_first;
			} else if (cum_len_next >= 0) {
				/* header ends */
				rdma_count = -1;
				cum_len_next = 0;
				seglen = -cum_len;
				small = (mss <= MXGEFW_SEND_SMALL_SIZE);
				flags_next = MXGEFW_FLAGS_TSO_PLD |
					MXGEFW_FLAGS_FIRST | 
					(small * MXGEFW_FLAGS_SMALL);
			    }
			
			req->addr_high = high_swapped;
			req->addr_low = htobe32(low);
			req->pseudo_hdr_offset = pseudo_hdr_offset;
			req->pad = 0;
			req->rdma_count = 1;
			req->length = htobe16(seglen);
			req->cksum_offset = cksum_offset;
			req->flags = flags | ((cum_len & 1) *
					      MXGEFW_FLAGS_ALIGN_ODD);
			low += seglen;
			len -= seglen;
			cum_len = cum_len_next;
			flags = flags_next;
			req++;
			cnt++;
			rdma_count++;
			if (__predict_false(cksum_offset > seglen))
				cksum_offset -= seglen;
			else
				cksum_offset = 0;
			if (__predict_false(cnt > tx->max_desc))
				goto drop;
		}
		busdma_seg_cnt--;
		seg++;
	}
	(req-rdma_count)->rdma_count = rdma_count;

	do {
		req--;
		req->flags |= MXGEFW_FLAGS_TSO_LAST;
	} while (!(req->flags & (MXGEFW_FLAGS_TSO_CHOP | MXGEFW_FLAGS_FIRST)));

	tx->info[((cnt - 1) + tx->req) & tx->mask].flag = 1;
	mxge_submit_req(tx, tx->req_list, cnt);
	return;

drop:
	bus_dmamap_unload(tx->dmat, tx->info[tx->req & tx->mask].map);
	m_freem(m);
	ss->sc->ifp->if_oerrors++;
	if (!once) {
		printf("tx->max_desc exceeded via TSO!\n");
		printf("mss = %d, %ld, %d!\n", mss,
		       (long)seg - (long)tx->seg_list, tx->max_desc);
		once = 1;
	}
	return;

}

#endif /* IFCAP_TSO4 */

#ifdef MXGE_NEW_VLAN_API
/* 
 * We reproduce the software vlan tag insertion from
 * net/if_vlan.c:vlan_start() here so that we can advertise "hardware"
 * vlan tag insertion. We need to advertise this in order to have the
 * vlan interface respect our csum offload flags.
 */
static struct mbuf *
mxge_vlan_tag_insert(struct mbuf *m)
{
	struct ether_vlan_header *evl;

	M_PREPEND(m, ETHER_VLAN_ENCAP_LEN, M_DONTWAIT);
	if (__predict_false(m == NULL))
		return NULL;
	if (m->m_len < sizeof(*evl)) {
		m = m_pullup(m, sizeof(*evl));
		if (__predict_false(m == NULL))
			return NULL;
	}
	/*
	 * Transform the Ethernet header into an Ethernet header
	 * with 802.1Q encapsulation.
	 */
	evl = mtod(m, struct ether_vlan_header *);
	bcopy((char *)evl + ETHER_VLAN_ENCAP_LEN,
	      (char *)evl, ETHER_HDR_LEN - ETHER_TYPE_LEN);
	evl->evl_encap_proto = htons(ETHERTYPE_VLAN);
	evl->evl_tag = htons(m->m_pkthdr.ether_vtag);
	m->m_flags &= ~M_VLANTAG;
	return m;
}
#endif /* MXGE_NEW_VLAN_API */

static void
mxge_encap(struct mxge_slice_state *ss, struct mbuf *m)
{
	mxge_softc_t *sc;
	mcp_kreq_ether_send_t *req;
	bus_dma_segment_t *seg;
	struct mbuf *m_tmp;
	struct ifnet *ifp;
	mxge_tx_ring_t *tx;
	struct ip *ip;
	int cnt, cum_len, err, i, idx, odd_flag, ip_off;
	uint16_t pseudo_hdr_offset;
        uint8_t flags, cksum_offset;


	sc = ss->sc;
	ifp = sc->ifp;
	tx = &ss->tx;

	ip_off = sizeof (struct ether_header);
#ifdef MXGE_NEW_VLAN_API
	if (m->m_flags & M_VLANTAG) {
		m = mxge_vlan_tag_insert(m);
		if (__predict_false(m == NULL))
			goto drop;
		ip_off += ETHER_VLAN_ENCAP_LEN;
	}
#endif
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
		ss->tx.defrag++;
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

#if IFCAP_TSO4
	/* TSO is different enough, we handle it in another routine */
	if (m->m_pkthdr.csum_flags & (CSUM_TSO)) {
		mxge_encap_tso(ss, m, cnt, ip_off);
		return;
	}
#endif

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
				   ss->scratch);
			ip = (struct ip *)(ss->scratch + ip_off);
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
mxge_start_locked(struct mxge_slice_state *ss)
{
	mxge_softc_t *sc;
	struct mbuf *m;
	struct ifnet *ifp;
	mxge_tx_ring_t *tx;

	sc = ss->sc;
	ifp = sc->ifp;
	tx = &ss->tx;
	while ((tx->mask - (tx->req - tx->done)) > tx->max_desc) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL) {
			return;
		}
		/* let BPF see it */
		BPF_MTAP(ifp, m);

		/* give it to the nic */
		mxge_encap(ss, m);
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
	struct mxge_slice_state *ss;

	/* only use the first slice for now */
	ss = &sc->ss[0];
	mtx_lock(&ss->tx.mtx);
	mxge_start_locked(ss);
	mtx_unlock(&ss->tx.mtx);		
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
mxge_get_buf_small(struct mxge_slice_state *ss, bus_dmamap_t map, int idx)
{
	bus_dma_segment_t seg;
	struct mbuf *m;
	mxge_rx_ring_t *rx = &ss->rx_small;
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
mxge_get_buf_big(struct mxge_slice_state *ss, bus_dmamap_t map, int idx)
{
	bus_dma_segment_t seg[3];
	struct mbuf *m;
	mxge_rx_ring_t *rx = &ss->rx_big;
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
	m->m_len = rx->mlen;
	err = bus_dmamap_load_mbuf_sg(rx->dmat, map, m, 
				      seg, &cnt, BUS_DMA_NOWAIT);
	if (err != 0) {
		m_free(m);
		goto done;
	}
	rx->info[idx].m = m;
	rx->shadow[idx].addr_low = 
		htobe32(MXGE_LOWPART_TO_U32(seg->ds_addr));
	rx->shadow[idx].addr_high = 
		htobe32(MXGE_HIGHPART_TO_U32(seg->ds_addr));

#if MXGE_VIRT_JUMBOS
	for (i = 1; i < cnt; i++) {
		rx->shadow[idx + i].addr_low = 
			htobe32(MXGE_LOWPART_TO_U32(seg[i].ds_addr));
		rx->shadow[idx + i].addr_high = 
			htobe32(MXGE_HIGHPART_TO_U32(seg[i].ds_addr));
       }
#endif

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
#ifdef MXGE_NEW_VLAN_API	
	m->m_pkthdr.ether_vtag = ntohs(evl->evl_tag);
#else
	{
		struct m_tag *mtag;
		mtag = m_tag_alloc(MTAG_VLAN, MTAG_VLAN_TAG, sizeof(u_int),
				   M_NOWAIT);
		if (mtag == NULL)
			return;
		VLAN_TAG_VALUE(mtag) = ntohs(evl->evl_tag);
		m_tag_prepend(m, mtag);
	}

#endif
	m->m_flags |= M_VLANTAG;

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
mxge_rx_done_big(struct mxge_slice_state *ss, uint32_t len, uint32_t csum)
{
	mxge_softc_t *sc;
	struct ifnet *ifp;
	struct mbuf *m;
	struct ether_header *eh;
	mxge_rx_ring_t *rx;
	bus_dmamap_t old_map;
	int idx;
	uint16_t tcpudp_csum;

	sc = ss->sc;
	ifp = sc->ifp;
	rx = &ss->rx_big;
	idx = rx->cnt & rx->mask;
	rx->cnt += rx->nbufs;
	/* save a pointer to the received mbuf */
	m = rx->info[idx].m;
	/* try to replace the received mbuf */
	if (mxge_get_buf_big(ss, rx->extra_map, idx)) {
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
	ss->ipackets++;
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_VLAN)) {
		mxge_vlan_tag_remove(m, &csum);
	}
	/* if the checksum is valid, mark it in the mbuf header */
	if (sc->csum_flag && (0 == (tcpudp_csum = mxge_rx_csum(m, csum)))) {
		if (sc->lro_cnt && (0 == mxge_lro_rx(ss, m, csum)))
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
mxge_rx_done_small(struct mxge_slice_state *ss, uint32_t len, uint32_t csum)
{
	mxge_softc_t *sc;
	struct ifnet *ifp;
	struct ether_header *eh;
	struct mbuf *m;
	mxge_rx_ring_t *rx;
	bus_dmamap_t old_map;
	int idx;
	uint16_t tcpudp_csum;

	sc = ss->sc;
	ifp = sc->ifp;
	rx = &ss->rx_small;
	idx = rx->cnt & rx->mask;
	rx->cnt++;
	/* save a pointer to the received mbuf */
	m = rx->info[idx].m;
	/* try to replace the received mbuf */
	if (mxge_get_buf_small(ss, rx->extra_map, idx)) {
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
	ss->ipackets++;
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_VLAN)) {
		mxge_vlan_tag_remove(m, &csum);
	}
	/* if the checksum is valid, mark it in the mbuf header */
	if (sc->csum_flag && (0 == (tcpudp_csum = mxge_rx_csum(m, csum)))) {
		if (sc->lro_cnt && (0 == mxge_lro_rx(ss, m, csum)))
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
mxge_clean_rx_done(struct mxge_slice_state *ss)
{
	mxge_rx_done_t *rx_done = &ss->rx_done;
	struct lro_entry *lro;
	int limit = 0;
	uint16_t length;
	uint16_t checksum;


	while (rx_done->entry[rx_done->idx].length != 0) {
		length = ntohs(rx_done->entry[rx_done->idx].length);
		rx_done->entry[rx_done->idx].length = 0;
		checksum = rx_done->entry[rx_done->idx].checksum;
		if (length <= (MHLEN - MXGEFW_PAD))
			mxge_rx_done_small(ss, length, checksum);
		else
			mxge_rx_done_big(ss, length, checksum);
		rx_done->cnt++;
		rx_done->idx = rx_done->cnt & rx_done->mask;

		/* limit potential for livelock */
		if (__predict_false(++limit > rx_done->mask / 2))
			break;
	}
	while (!SLIST_EMPTY(&ss->lro_active)) {
		lro = SLIST_FIRST(&ss->lro_active);
		SLIST_REMOVE_HEAD(&ss->lro_active, next);
		mxge_lro_flush(ss, lro);
	}
}


static inline void
mxge_tx_done(struct mxge_slice_state *ss, uint32_t mcp_idx)
{
	struct ifnet *ifp;
	mxge_tx_ring_t *tx;
	struct mbuf *m;
	bus_dmamap_t map;
	int idx;

	tx = &ss->tx;
	ifp = ss->sc->ifp;
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
	}
	
	/* If we have space, clear IFF_OACTIVE to tell the stack that
           its OK to send packets */

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE &&
	    tx->req - tx->done < (tx->mask + 1)/4) {
		mtx_lock(&ss->tx.mtx);
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		ss->tx.wake++;
		mxge_start_locked(ss);
		mtx_unlock(&ss->tx.mtx);
	}
}

static struct mxge_media_type mxge_xfp_media_types[] =
{
	{IFM_10G_CX4,	0x7f, 		"10GBASE-CX4 (module)"},
	{IFM_10G_SR, 	(1 << 7),	"10GBASE-SR"},
	{IFM_10G_LR, 	(1 << 6),	"10GBASE-LR"},
	{0,		(1 << 5),	"10GBASE-ER"},
	{IFM_10G_LRM,	(1 << 4),	"10GBASE-LRM"},
	{0,		(1 << 3),	"10GBASE-SW"},
	{0,		(1 << 2),	"10GBASE-LW"},
	{0,		(1 << 1),	"10GBASE-EW"},
	{0,		(1 << 0),	"Reserved"}
};
static struct mxge_media_type mxge_sfp_media_types[] =
{
	{0,		(1 << 7),	"Reserved"},
	{IFM_10G_LRM,	(1 << 6),	"10GBASE-LRM"},
	{IFM_10G_LR, 	(1 << 5),	"10GBASE-LR"},
	{IFM_10G_SR,	(1 << 4),	"10GBASE-SR"}
};

static void
mxge_set_media(mxge_softc_t *sc, int type)
{
	sc->media_flags |= type;
	ifmedia_add(&sc->media, sc->media_flags, 0, NULL);
	ifmedia_set(&sc->media, sc->media_flags);
}


/*
 * Determine the media type for a NIC.  Some XFPs will identify
 * themselves only when their link is up, so this is initiated via a
 * link up interrupt.  However, this can potentially take up to
 * several milliseconds, so it is run via the watchdog routine, rather
 * than in the interrupt handler itself.   This need only be done
 * once, not each time the link is up.
 */
static void
mxge_media_probe(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	char *cage_type = NULL; /* -Wuninitialized */
	char *ptr;
	struct mxge_media_type *mxge_media_types = NULL;
	int i, err, ms, mxge_media_type_entries = 0; /* -Wuninitialized */
	uint32_t byte = 0; /*  -Wuninitialized */

	sc->need_media_probe = 0;

	/* if we've already set a media type, we're done */
	if (sc->media_flags  != (IFM_ETHER | IFM_AUTO))
		return;

	/* 
	 * parse the product code to deterimine the interface type
	 * (CX4, XFP, Quad Ribbon Fiber) by looking at the character
	 * after the 3rd dash in the driver's cached copy of the
	 * EEPROM's product code string.
	 */
	ptr = sc->product_code_string;
	if (ptr == NULL) {
		device_printf(sc->dev, "Missing product code\n");
	}

	for (i = 0; i < 3; i++, ptr++) {
		ptr = index(ptr, '-');
		if (ptr == NULL) {
			device_printf(sc->dev,
				      "only %d dashes in PC?!?\n", i);
			return;
		}
	}
	if (*ptr == 'C') {
		/* -C is CX4 */
		mxge_set_media(sc, IFM_10G_CX4);
		return;
	}
	else if (*ptr == 'Q') {
		/* -Q is Quad Ribbon Fiber */
		device_printf(sc->dev, "Quad Ribbon Fiber Media\n");
		/* FreeBSD has no media type for Quad ribbon fiber */
		return;
	}

	if (*ptr == 'R') {
		/* -R is XFP */
		mxge_media_types = mxge_xfp_media_types;
		mxge_media_type_entries = 
			sizeof (mxge_xfp_media_types) /
			sizeof (mxge_xfp_media_types[0]);
		byte = MXGE_XFP_COMPLIANCE_BYTE;
		cage_type = "XFP";
	}

	if (*ptr == 'S' || *(ptr +1) == 'S') {
		/* -S or -2S is SFP+ */
		mxge_media_types = mxge_sfp_media_types;
		mxge_media_type_entries = 
			sizeof (mxge_sfp_media_types) /
			sizeof (mxge_sfp_media_types[0]);
		cage_type = "SFP+";
		byte = 3;
	}

	if (mxge_media_types == NULL) {
		device_printf(sc->dev, "Unknown media type: %c\n", *ptr);
		return;
	}

	/*
	 * At this point we know the NIC has an XFP cage, so now we
	 * try to determine what is in the cage by using the
	 * firmware's XFP I2C commands to read the XFP 10GbE compilance
	 * register.  We read just one byte, which may take over
	 * a millisecond
	 */

	cmd.data0 = 0;	 /* just fetch 1 byte, not all 256 */
	cmd.data1 = byte;
	err = mxge_send_cmd(sc, MXGEFW_CMD_I2C_READ, &cmd);
	if (err == MXGEFW_CMD_ERROR_I2C_FAILURE) {
		device_printf(sc->dev, "failed to read XFP\n");
	}
	if (err == MXGEFW_CMD_ERROR_I2C_ABSENT) {
		device_printf(sc->dev, "Type R/S with no XFP!?!?\n");
	}
	if (err != MXGEFW_CMD_OK) {
		return;
	}

	/* now we wait for the data to be cached */
	cmd.data0 = byte;
	err = mxge_send_cmd(sc, MXGEFW_CMD_I2C_BYTE, &cmd);
	for (ms = 0; (err == EBUSY) && (ms < 50); ms++) {
		DELAY(1000);
		cmd.data0 = byte;
		err = mxge_send_cmd(sc, MXGEFW_CMD_I2C_BYTE, &cmd);
	}
	if (err != MXGEFW_CMD_OK) {
		device_printf(sc->dev, "failed to read %s (%d, %dms)\n",
			      cage_type, err, ms);
		return;
	}
		
	if (cmd.data0 == mxge_media_types[0].bitmask) {
		if (mxge_verbose)
			device_printf(sc->dev, "%s:%s\n", cage_type,
				      mxge_media_types[0].name);
		mxge_set_media(sc, IFM_10G_CX4);
		return;
	}
	for (i = 1; i < mxge_media_type_entries; i++) {
		if (cmd.data0 & mxge_media_types[i].bitmask) {
			if (mxge_verbose)
				device_printf(sc->dev, "%s:%s\n",
					      cage_type,
					      mxge_media_types[i].name);

			mxge_set_media(sc, mxge_media_types[i].flag);
			return;
		}
	}
	device_printf(sc->dev, "%s media 0x%x unknown\n", cage_type,
		      cmd.data0);

	return;
}

static void
mxge_intr(void *arg)
{
	struct mxge_slice_state *ss = arg;
	mxge_softc_t *sc = ss->sc;
	mcp_irq_data_t *stats = ss->fw_stats;
	mxge_tx_ring_t *tx = &ss->tx;
	mxge_rx_done_t *rx_done = &ss->rx_done;
	uint32_t send_done_count;
	uint8_t valid;


	/* an interrupt on a non-zero slice is implicitly valid
	   since MSI-X irqs are not shared */
	if (ss != sc->ss) {
		mxge_clean_rx_done(ss);
		*ss->irq_claim = be32toh(3);
		return;
	}

	/* make sure the DMA has finished */
	if (!stats->valid) {
		return;
	}
	valid = stats->valid;

	if (sc->legacy_irq) {
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
			mxge_tx_done(ss, (int)send_done_count);
			mxge_clean_rx_done(ss);
			send_done_count = be32toh(stats->send_done_count);
		}
		if (sc->legacy_irq && mxge_deassert_wait)
			mb();
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
			sc->need_media_probe = 1;
		}
		if (sc->rdma_tags_available !=
		    be32toh(stats->rdma_tags_available)) {
			sc->rdma_tags_available = 
				be32toh(stats->rdma_tags_available);
			device_printf(sc->dev, "RDMA timed out! %d tags "
				      "left\n", sc->rdma_tags_available);
		}

		if (stats->link_down) {
			sc->down_cnt += stats->link_down;
			sc->link_state = 0;
			if_link_state_change(sc->ifp, LINK_STATE_DOWN);
		}
	}

	/* check to see if we have rx token to pass back */
	if (valid & 0x1)
	    *ss->irq_claim = be32toh(3);
	*(ss->irq_claim + 1) = be32toh(3);
}

static void
mxge_init(void *arg)
{
}



static void
mxge_free_slice_mbufs(struct mxge_slice_state *ss)
{
	struct lro_entry *lro_entry;
	int i;

	while (!SLIST_EMPTY(&ss->lro_free)) {
		lro_entry = SLIST_FIRST(&ss->lro_free);
		SLIST_REMOVE_HEAD(&ss->lro_free, next);
		free(lro_entry, M_DEVBUF);
	}

	for (i = 0; i <= ss->rx_big.mask; i++) {
		if (ss->rx_big.info[i].m == NULL)
			continue;
		bus_dmamap_unload(ss->rx_big.dmat,
				  ss->rx_big.info[i].map);
		m_freem(ss->rx_big.info[i].m);
		ss->rx_big.info[i].m = NULL;
	}

	for (i = 0; i <= ss->rx_small.mask; i++) {
		if (ss->rx_small.info[i].m == NULL)
			continue;
		bus_dmamap_unload(ss->rx_small.dmat,
				  ss->rx_small.info[i].map);
		m_freem(ss->rx_small.info[i].m);
		ss->rx_small.info[i].m = NULL;
	}

	/* transmit ring used only on the first slice */
	if (ss->tx.info == NULL)
		return;

	for (i = 0; i <= ss->tx.mask; i++) {
		ss->tx.info[i].flag = 0;
		if (ss->tx.info[i].m == NULL)
			continue;
		bus_dmamap_unload(ss->tx.dmat,
				  ss->tx.info[i].map);
		m_freem(ss->tx.info[i].m);
		ss->tx.info[i].m = NULL;
	}
}

static void
mxge_free_mbufs(mxge_softc_t *sc)
{
	int slice;

	for (slice = 0; slice < sc->num_slices; slice++)
		mxge_free_slice_mbufs(&sc->ss[slice]);
}

static void
mxge_free_slice_rings(struct mxge_slice_state *ss)
{
	int i;


	if (ss->rx_done.entry != NULL)
		mxge_dma_free(&ss->rx_done.dma);
	ss->rx_done.entry = NULL;

	if (ss->tx.req_bytes != NULL)
		free(ss->tx.req_bytes, M_DEVBUF);
	ss->tx.req_bytes = NULL;

	if (ss->tx.seg_list != NULL)
		free(ss->tx.seg_list, M_DEVBUF);
	ss->tx.seg_list = NULL;

	if (ss->rx_small.shadow != NULL)
		free(ss->rx_small.shadow, M_DEVBUF);
	ss->rx_small.shadow = NULL;

	if (ss->rx_big.shadow != NULL)
		free(ss->rx_big.shadow, M_DEVBUF);
	ss->rx_big.shadow = NULL;

	if (ss->tx.info != NULL) {
		if (ss->tx.dmat != NULL) {
			for (i = 0; i <= ss->tx.mask; i++) {
				bus_dmamap_destroy(ss->tx.dmat,
						   ss->tx.info[i].map);
			}
			bus_dma_tag_destroy(ss->tx.dmat);
		}
		free(ss->tx.info, M_DEVBUF);
	}
	ss->tx.info = NULL;

	if (ss->rx_small.info != NULL) {
		if (ss->rx_small.dmat != NULL) {
			for (i = 0; i <= ss->rx_small.mask; i++) {
				bus_dmamap_destroy(ss->rx_small.dmat,
						   ss->rx_small.info[i].map);
			}
			bus_dmamap_destroy(ss->rx_small.dmat,
					   ss->rx_small.extra_map);
			bus_dma_tag_destroy(ss->rx_small.dmat);
		}
		free(ss->rx_small.info, M_DEVBUF);
	}
	ss->rx_small.info = NULL;

	if (ss->rx_big.info != NULL) {
		if (ss->rx_big.dmat != NULL) {
			for (i = 0; i <= ss->rx_big.mask; i++) {
				bus_dmamap_destroy(ss->rx_big.dmat,
						   ss->rx_big.info[i].map);
			}
			bus_dmamap_destroy(ss->rx_big.dmat,
					   ss->rx_big.extra_map);
			bus_dma_tag_destroy(ss->rx_big.dmat);
		}
		free(ss->rx_big.info, M_DEVBUF);
	}
	ss->rx_big.info = NULL;
}

static void
mxge_free_rings(mxge_softc_t *sc)
{
	int slice;

	for (slice = 0; slice < sc->num_slices; slice++)
		mxge_free_slice_rings(&sc->ss[slice]);
}

static int
mxge_alloc_slice_rings(struct mxge_slice_state *ss, int rx_ring_entries,
		       int tx_ring_entries)
{
	mxge_softc_t *sc = ss->sc;
	size_t bytes;
	int err, i;

	err = ENOMEM;

	/* allocate per-slice receive resources */

	ss->rx_small.mask = ss->rx_big.mask = rx_ring_entries - 1;
	ss->rx_done.mask = (2 * rx_ring_entries) - 1;

	/* allocate the rx shadow rings */
	bytes = rx_ring_entries * sizeof (*ss->rx_small.shadow);
	ss->rx_small.shadow = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (ss->rx_small.shadow == NULL)
		return err;;

	bytes = rx_ring_entries * sizeof (*ss->rx_big.shadow);
	ss->rx_big.shadow = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (ss->rx_big.shadow == NULL)
		return err;;

	/* allocate the rx host info rings */
	bytes = rx_ring_entries * sizeof (*ss->rx_small.info);
	ss->rx_small.info = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (ss->rx_small.info == NULL)
		return err;;

	bytes = rx_ring_entries * sizeof (*ss->rx_big.info);
	ss->rx_big.info = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (ss->rx_big.info == NULL)
		return err;;

	/* allocate the rx busdma resources */
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
				 &ss->rx_small.dmat);	/* tag */
	if (err != 0) {
		device_printf(sc->dev, "Err %d allocating rx_small dmat\n",
			      err);
		return err;;
	}

	err = bus_dma_tag_create(sc->parent_dmat,	/* parent */
				 1,			/* alignment */
#if MXGE_VIRT_JUMBOS
				 4096,			/* boundary */
#else
				 0,			/* boundary */
#endif
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 3*4096,		/* maxsize */
#if MXGE_VIRT_JUMBOS
				 3,			/* num segs */
				 4096,			/* maxsegsize*/
#else
				 1,			/* num segs */
				 MJUM9BYTES,		/* maxsegsize*/
#endif
				 BUS_DMA_ALLOCNOW,	/* flags */
				 NULL, NULL,		/* lock */
				 &ss->rx_big.dmat);	/* tag */
	if (err != 0) {
		device_printf(sc->dev, "Err %d allocating rx_big dmat\n",
			      err);
		return err;;
	}
	for (i = 0; i <= ss->rx_small.mask; i++) {
		err = bus_dmamap_create(ss->rx_small.dmat, 0, 
					&ss->rx_small.info[i].map);
		if (err != 0) {
			device_printf(sc->dev, "Err %d  rx_small dmamap\n",
				      err);
			return err;;
		}
	}
	err = bus_dmamap_create(ss->rx_small.dmat, 0, 
				&ss->rx_small.extra_map);
	if (err != 0) {
		device_printf(sc->dev, "Err %d extra rx_small dmamap\n",
			      err);
		return err;;
	}

	for (i = 0; i <= ss->rx_big.mask; i++) {
		err = bus_dmamap_create(ss->rx_big.dmat, 0, 
					&ss->rx_big.info[i].map);
		if (err != 0) {
			device_printf(sc->dev, "Err %d  rx_big dmamap\n",
				      err);
			return err;;
		}
	}
	err = bus_dmamap_create(ss->rx_big.dmat, 0, 
				&ss->rx_big.extra_map);
	if (err != 0) {
		device_printf(sc->dev, "Err %d extra rx_big dmamap\n",
			      err);
		return err;;
	}

	/* now allocate TX resouces */

	/* only use a single TX ring for now */
	if (ss != ss->sc->ss)
		return 0;

	ss->tx.mask = tx_ring_entries - 1;
	ss->tx.max_desc = MIN(MXGE_MAX_SEND_DESC, tx_ring_entries / 4);

	
	/* allocate the tx request copy block */
	bytes = 8 + 
		sizeof (*ss->tx.req_list) * (ss->tx.max_desc + 4);
	ss->tx.req_bytes = malloc(bytes, M_DEVBUF, M_WAITOK);
	if (ss->tx.req_bytes == NULL)
		return err;;
	/* ensure req_list entries are aligned to 8 bytes */
	ss->tx.req_list = (mcp_kreq_ether_send_t *)
		((unsigned long)(ss->tx.req_bytes + 7) & ~7UL);

	/* allocate the tx busdma segment list */
	bytes = sizeof (*ss->tx.seg_list) * ss->tx.max_desc;
	ss->tx.seg_list = (bus_dma_segment_t *) 
		malloc(bytes, M_DEVBUF, M_WAITOK);
	if (ss->tx.seg_list == NULL)
		return err;;

	/* allocate the tx host info ring */
	bytes = tx_ring_entries * sizeof (*ss->tx.info);
	ss->tx.info = malloc(bytes, M_DEVBUF, M_ZERO|M_WAITOK);
	if (ss->tx.info == NULL)
		return err;;
	
	/* allocate the tx busdma resources */
	err = bus_dma_tag_create(sc->parent_dmat,	/* parent */
				 1,			/* alignment */
				 sc->tx_boundary,	/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 65536 + 256,		/* maxsize */
				 ss->tx.max_desc - 2,	/* num segs */
				 sc->tx_boundary,	/* maxsegsz */
				 BUS_DMA_ALLOCNOW,	/* flags */
				 NULL, NULL,		/* lock */
				 &ss->tx.dmat);		/* tag */
	
	if (err != 0) {
		device_printf(sc->dev, "Err %d allocating tx dmat\n",
			      err);
		return err;;
	}

	/* now use these tags to setup dmamaps for each slot
	   in the ring */
	for (i = 0; i <= ss->tx.mask; i++) {
		err = bus_dmamap_create(ss->tx.dmat, 0, 
					&ss->tx.info[i].map);
		if (err != 0) {
			device_printf(sc->dev, "Err %d  tx dmamap\n",
				      err);
			return err;;
		}
	}
	return 0;

}

static int
mxge_alloc_rings(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	int tx_ring_size;
	int tx_ring_entries, rx_ring_entries;
	int err, slice;
	
	/* get ring sizes */
	err = mxge_send_cmd(sc, MXGEFW_CMD_GET_SEND_RING_SIZE, &cmd);
	tx_ring_size = cmd.data0;
	if (err != 0) {
		device_printf(sc->dev, "Cannot determine tx ring sizes\n");
		goto abort;
	}

	tx_ring_entries = tx_ring_size / sizeof (mcp_kreq_ether_send_t);
	rx_ring_entries = sc->rx_ring_size / sizeof (mcp_dma_addr_t);
	IFQ_SET_MAXLEN(&sc->ifp->if_snd, tx_ring_entries - 1);
	sc->ifp->if_snd.ifq_drv_maxlen = sc->ifp->if_snd.ifq_maxlen;
	IFQ_SET_READY(&sc->ifp->if_snd);

	for (slice = 0; slice < sc->num_slices; slice++) {
		err = mxge_alloc_slice_rings(&sc->ss[slice],
					     rx_ring_entries,
					     tx_ring_entries);
		if (err != 0)
			goto abort;
	}
	return 0;

abort:
	mxge_free_rings(sc);
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
#if MXGE_VIRT_JUMBOS
	/* now we need to use virtually contiguous buffers */
	*cl_size = MJUM9BYTES;
	*big_buf_size = 4096;
	*nbufs = mtu / 4096 + 1;
	/* needs to be a power of two, so round up */
	if (*nbufs == 3)
		*nbufs = 4;
#else
	*cl_size = MJUM9BYTES;
	*big_buf_size = MJUM9BYTES;
	*nbufs = 1;
#endif
}

static int
mxge_slice_open(struct mxge_slice_state *ss, int nbufs, int cl_size)
{
	mxge_softc_t *sc;
	mxge_cmd_t cmd;
	bus_dmamap_t map;
	struct lro_entry *lro_entry;	
	int err, i, slice;


	sc = ss->sc;
	slice = ss - sc->ss;

	SLIST_INIT(&ss->lro_free);
	SLIST_INIT(&ss->lro_active);

	for (i = 0; i < sc->lro_cnt; i++) {
		lro_entry = (struct lro_entry *)
			malloc(sizeof (*lro_entry), M_DEVBUF,
			       M_NOWAIT | M_ZERO);
		if (lro_entry == NULL) {
			sc->lro_cnt = i;
			break;
		}
		SLIST_INSERT_HEAD(&ss->lro_free, lro_entry, next);
	}
	/* get the lanai pointers to the send and receive rings */

	err = 0;
	/* We currently only send from the first slice */
	if (slice == 0) {
		cmd.data0 = slice;
		err = mxge_send_cmd(sc, MXGEFW_CMD_GET_SEND_OFFSET, &cmd);
		ss->tx.lanai = 
			(volatile mcp_kreq_ether_send_t *)(sc->sram + cmd.data0);
	}
	cmd.data0 = slice;
	err |= mxge_send_cmd(sc, 
			     MXGEFW_CMD_GET_SMALL_RX_OFFSET, &cmd);
	ss->rx_small.lanai = 
		(volatile mcp_kreq_ether_recv_t *)(sc->sram + cmd.data0);
	cmd.data0 = slice;
	err |= mxge_send_cmd(sc, MXGEFW_CMD_GET_BIG_RX_OFFSET, &cmd);
	ss->rx_big.lanai = 
		(volatile mcp_kreq_ether_recv_t *)(sc->sram + cmd.data0);

	if (err != 0) {
		device_printf(sc->dev, 
			      "failed to get ring sizes or locations\n");
		return EIO;
	}

	/* stock receive rings */
	for (i = 0; i <= ss->rx_small.mask; i++) {
		map = ss->rx_small.info[i].map;
		err = mxge_get_buf_small(ss, map, i);
		if (err) {
			device_printf(sc->dev, "alloced %d/%d smalls\n",
				      i, ss->rx_small.mask + 1);
			return ENOMEM;
		}
	}
	for (i = 0; i <= ss->rx_big.mask; i++) {
		ss->rx_big.shadow[i].addr_low = 0xffffffff;
		ss->rx_big.shadow[i].addr_high = 0xffffffff;
	}
	ss->rx_big.nbufs = nbufs;
	ss->rx_big.cl_size = cl_size;
	ss->rx_big.mlen = ss->sc->ifp->if_mtu + ETHER_HDR_LEN +
		ETHER_VLAN_ENCAP_LEN + MXGEFW_PAD;
	for (i = 0; i <= ss->rx_big.mask; i += ss->rx_big.nbufs) {
		map = ss->rx_big.info[i].map;
		err = mxge_get_buf_big(ss, map, i);
		if (err) {
			device_printf(sc->dev, "alloced %d/%d bigs\n",
				      i, ss->rx_big.mask + 1);
			return ENOMEM;
		}
	}
	return 0;
}

static int 
mxge_open(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	int err, big_bytes, nbufs, slice, cl_size, i;
	bus_addr_t bus;
	volatile uint8_t *itable;

	/* Copy the MAC address in case it was overridden */
	bcopy(IF_LLADDR(sc->ifp), sc->mac_addr, ETHER_ADDR_LEN);

	err = mxge_reset(sc, 1);
	if (err != 0) {
		device_printf(sc->dev, "failed to reset\n");
		return EIO;
	}

	if (sc->num_slices > 1) {
		/* setup the indirection table */
		cmd.data0 = sc->num_slices;
		err = mxge_send_cmd(sc, MXGEFW_CMD_SET_RSS_TABLE_SIZE,
				    &cmd);

		err |= mxge_send_cmd(sc, MXGEFW_CMD_GET_RSS_TABLE_OFFSET,
				     &cmd);
		if (err != 0) {
			device_printf(sc->dev,
				      "failed to setup rss tables\n");
			return err;
		}

		/* just enable an identity mapping */
		itable = sc->sram + cmd.data0;
		for (i = 0; i < sc->num_slices; i++)
			itable[i] = (uint8_t)i;

		cmd.data0 = 1;
		cmd.data1 = mxge_rss_hash_type;
		err = mxge_send_cmd(sc, MXGEFW_CMD_SET_RSS_ENABLE, &cmd);
		if (err != 0) {
			device_printf(sc->dev, "failed to enable slices\n");
			return err;
		}
	}


	mxge_choose_params(sc->ifp->if_mtu, &big_bytes, &cl_size, &nbufs);

	cmd.data0 = nbufs;
	err = mxge_send_cmd(sc, MXGEFW_CMD_ALWAYS_USE_N_BIG_BUFFERS,
			    &cmd);
	/* error is only meaningful if we're trying to set 
	   MXGEFW_CMD_ALWAYS_USE_N_BIG_BUFFERS > 1 */
	if (err && nbufs > 1) {
		device_printf(sc->dev,
			      "Failed to set alway-use-n to %d\n",
			      nbufs);
		return EIO;
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
	cmd.data0 = MXGE_LOWPART_TO_U32(sc->ss->fw_stats_dma.bus_addr);
	cmd.data1 = MXGE_HIGHPART_TO_U32(sc->ss->fw_stats_dma.bus_addr);
	cmd.data2 = sizeof(struct mcp_irq_data);
	err = mxge_send_cmd(sc, MXGEFW_CMD_SET_STATS_DMA_V2, &cmd);

	if (err != 0) {
		bus = sc->ss->fw_stats_dma.bus_addr;
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

	for (slice = 0; slice < sc->num_slices; slice++) {
		err = mxge_slice_open(&sc->ss[slice], nbufs, cl_size);
		if (err != 0) {
			device_printf(sc->dev, "couldn't open slice %d\n",
				      slice);
			goto abort;
		}
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
mxge_close(mxge_softc_t *sc, int down)
{
	mxge_cmd_t cmd;
	int err, old_down_cnt;

	sc->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	if (!down) {
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
		mb();
		if (old_down_cnt == sc->down_cnt) {
			device_printf(sc->dev, "never got down irq\n");
		}
	}
	mxge_free_mbufs(sc);

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

		if (sc->pectl == 0) {
			pectl = pci_read_config(dev, reg + 0x8, 2);
			pectl = (pectl & ~0x7000) | (5 << 12);
			pci_write_config(dev, reg + 0x8, pectl, 2);
			sc->pectl = pectl;
		} else {
			/* restore saved pectl after watchdog reset */
			pci_write_config(dev, reg + 0x8, sc->pectl, 2);
		}
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
	struct pci_devinfo *dinfo;
	struct mxge_slice_state *ss;
	int err, running, s, num_tx_slices = 1;
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
		}
	}
	if ((cmd & PCIM_CMD_BUSMASTEREN) == 0) {
		/* print the reboot status */
		reboot = mxge_read_reboot(sc);
		device_printf(sc->dev, "NIC rebooted, status = 0x%x\n",
			      reboot);
		running = sc->ifp->if_drv_flags & IFF_DRV_RUNNING;
		if (running) {

			/* 
			 * quiesce NIC so that TX routines will not try to
			 * xmit after restoration of BAR
			 */

			/* Mark the link as down */
			if (sc->link_state) {
				sc->link_state = 0;
				if_link_state_change(sc->ifp,
						     LINK_STATE_DOWN);
			}
#ifdef IFNET_BUF_RING
			num_tx_slices = sc->num_slices;
#endif
			/* grab all TX locks to ensure no tx  */
			for (s = 0; s < num_tx_slices; s++) {
				ss = &sc->ss[s];
				mtx_lock(&ss->tx.mtx);
			}
			mxge_close(sc, 1);
		}
		/* restore PCI configuration space */
		dinfo = device_get_ivars(sc->dev);
		pci_cfg_restore(sc->dev, dinfo);

		/* and redo any changes we made to our config space */
		mxge_setup_cfg_space(sc);

		/* reload f/w */
		err = mxge_load_firmware(sc, 0);
		if (err) {
			device_printf(sc->dev,
				      "Unable to re-load f/w\n");
		}
		if (running) {
			if (!err)
				err = mxge_open(sc);
			/* release all TX locks */
			for (s = 0; s < num_tx_slices; s++) {
				ss = &sc->ss[s];
#ifdef IFNET_BUF_RING
				mxge_start_locked(ss);
#endif
				mtx_unlock(&ss->tx.mtx);
			}
		}
		sc->watchdog_resets++;
	} else {
		device_printf(sc->dev,
			      "NIC did not reboot, not resetting\n");
		err = 0;
	}
	if (err) {
		device_printf(sc->dev, "watchdog reset failed\n");
	} else {
		if (sc->dying == 2)
			sc->dying = 0;
		callout_reset(&sc->co_hdl, mxge_ticks, mxge_tick, sc);
	}
}

static void
mxge_watchdog_task(void *arg, int pending)
{
	mxge_softc_t *sc = arg;


	mtx_lock(&sc->driver_mtx);
	mxge_watchdog_reset(sc);
	mtx_unlock(&sc->driver_mtx);
}

static void
mxge_warn_stuck(mxge_softc_t *sc, mxge_tx_ring_t *tx, int slice)
{
	tx = &sc->ss[slice].tx;
	device_printf(sc->dev, "slice %d struck? ring state:\n", slice);
	device_printf(sc->dev, "tx.req=%d tx.done=%d\n",
		      tx->req, tx->done);
	device_printf(sc->dev, "pkt_done=%d fw=%d\n",
		      tx->pkt_done,
		      be32toh(sc->ss->fw_stats->send_done_count));
}

static int
mxge_watchdog(mxge_softc_t *sc)
{
	mxge_tx_ring_t *tx = &sc->ss->tx;
	uint32_t rx_pause = be32toh(sc->ss->fw_stats->dropped_pause);
	int err = 0;

	/* see if we have outstanding transmits, which
	   have been pending for more than mxge_ticks */
	if (tx->req != tx->done &&
	    tx->watchdog_req != tx->watchdog_done &&
	    tx->done == tx->watchdog_done) {
		/* check for pause blocking before resetting */
		if (tx->watchdog_rx_pause == rx_pause) {
			mxge_warn_stuck(sc, tx, 0);
			taskqueue_enqueue(sc->tq, &sc->watchdog_task);
			return (ENXIO);
		} else {
			device_printf(sc->dev, "Flow control blocking "
				      "xmits, check link partner\n");
		}
	}

	tx->watchdog_req = tx->req;
	tx->watchdog_done = tx->done;
	tx->watchdog_rx_pause = rx_pause;

	if (sc->need_media_probe)
		mxge_media_probe(sc);
	return (err);
}

static u_long
mxge_update_stats(mxge_softc_t *sc)
{
	struct mxge_slice_state *ss;
	u_long ipackets = 0;
	int slice;

	for(slice = 0; slice < sc->num_slices; slice++) {
		ss = &sc->ss[slice];
		ipackets += ss->ipackets;
	}
	sc->ifp->if_ipackets = ipackets;
	return ipackets;
}

static void
mxge_tick(void *arg)
{
	mxge_softc_t *sc = arg;
	u_long pkts = 0;
	int err = 0;
	int running, ticks;
	uint16_t cmd;

	ticks = mxge_ticks;
	running = sc->ifp->if_drv_flags & IFF_DRV_RUNNING;
	if (running) {
		/* aggregate stats from different slices */
		pkts = mxge_update_stats(sc);
		if (!sc->watchdog_countdown) {
			err = mxge_watchdog(sc);
			sc->watchdog_countdown = 4;
		}
		sc->watchdog_countdown--;
	}
	if (pkts == 0) {
		/* ensure NIC did not suffer h/w fault while idle */
		cmd = pci_read_config(sc->dev, PCIR_COMMAND, 2);		
		if ((cmd & PCIM_CMD_BUSMASTEREN) == 0) {
			sc->dying = 2;
			taskqueue_enqueue(sc->tq, &sc->watchdog_task);
			err = ENXIO;
		}
		/* look less often if NIC is idle */
		ticks *= 4;
	}

	if (err == 0)
		callout_reset(&sc->co_hdl, ticks, mxge_tick, sc);

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
		mxge_close(sc, 0);
		err = mxge_open(sc);
		if (err != 0) {
			ifp->if_mtu = old_mtu;
			mxge_close(sc, 0);
			(void) mxge_open(sc);
		}
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
	ifmr->ifm_status |= sc->link_state ? IFM_ACTIVE : 0;
	ifmr->ifm_active = IFM_AUTO | IFM_ETHER;
	ifmr->ifm_active |= sc->link_state ? IFM_FDX : 0;
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
		if (sc->dying) {
			mtx_unlock(&sc->driver_mtx);
			return EINVAL;
		}
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				err = mxge_open(sc);
			} else {
				/* take care of promis can allmulti
				   flag chages */
				mxge_change_promisc(sc, 
						    ifp->if_flags & IFF_PROMISC);
				mxge_set_multicast_list(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				mxge_close(sc, 0);
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
				ifp->if_capenable &= ~(IFCAP_TXCSUM|IFCAP_TSO4);
				ifp->if_hwassist &= ~(CSUM_TCP | CSUM_UDP
						      | CSUM_TSO);
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
		if (mask & IFCAP_TSO4) {
			if (IFCAP_TSO4 & ifp->if_capenable) {
				ifp->if_capenable &= ~IFCAP_TSO4;
				ifp->if_hwassist &= ~CSUM_TSO;
			} else if (IFCAP_TXCSUM & ifp->if_capenable) {
				ifp->if_capenable |= IFCAP_TSO4;
				ifp->if_hwassist |= CSUM_TSO;
			} else {
				printf("mxge requires tx checksum offload"
				       " be enabled to use TSO\n");
				err = EINVAL;
			}
		}
		if (mask & IFCAP_LRO) {
			if (IFCAP_LRO & ifp->if_capenable) 
				err = mxge_change_lro_locked(sc, 0);
			else
				err = mxge_change_lro_locked(sc, mxge_lro_cnt);
		}
		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		mtx_unlock(&sc->driver_mtx);
		VLAN_CAPABILITIES(ifp);

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

	TUNABLE_INT_FETCH("hw.mxge.max_slices", &mxge_max_slices);
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
	TUNABLE_INT_FETCH("hw.mxge.always_promisc", &mxge_always_promisc);
	TUNABLE_INT_FETCH("hw.mxge.rss_hash_type", &mxge_rss_hash_type);
	TUNABLE_INT_FETCH("hw.mxge.rss_hashtype", &mxge_rss_hash_type);
	TUNABLE_INT_FETCH("hw.mxge.initial_mtu", &mxge_initial_mtu);
	TUNABLE_INT_FETCH("hw.mxge.throttle", &mxge_throttle);
	if (sc->lro_cnt != 0)
		mxge_lro_cnt = sc->lro_cnt;

	if (bootverbose)
		mxge_verbose = 1;
	if (mxge_intr_coal_delay < 0 || mxge_intr_coal_delay > 10*1000)
		mxge_intr_coal_delay = 30;
	if (mxge_ticks == 0)
		mxge_ticks = hz / 2;
	sc->pause = mxge_flow_control;
	if (mxge_rss_hash_type < MXGEFW_RSS_HASH_TYPE_IPV4 
	    || mxge_rss_hash_type > MXGEFW_RSS_HASH_TYPE_MAX) {
		mxge_rss_hash_type = MXGEFW_RSS_HASH_TYPE_SRC_PORT;
	}
	if (mxge_initial_mtu > ETHERMTU_JUMBO ||
	    mxge_initial_mtu < ETHER_MIN_LEN)
		mxge_initial_mtu = ETHERMTU_JUMBO;

	if (mxge_throttle && mxge_throttle > MXGE_MAX_THROTTLE)
		mxge_throttle = MXGE_MAX_THROTTLE;
	if (mxge_throttle && mxge_throttle < MXGE_MIN_THROTTLE)
		mxge_throttle = MXGE_MIN_THROTTLE;
	sc->throttle = mxge_throttle;
}


static void
mxge_free_slices(mxge_softc_t *sc)
{
	struct mxge_slice_state *ss;
	int i;


	if (sc->ss == NULL)
		return;

	for (i = 0; i < sc->num_slices; i++) {
		ss = &sc->ss[i];
		if (ss->fw_stats != NULL) {
			mxge_dma_free(&ss->fw_stats_dma);
			ss->fw_stats = NULL;
			mtx_destroy(&ss->tx.mtx);
		}
		if (ss->rx_done.entry != NULL) {
			mxge_dma_free(&ss->rx_done.dma);
			ss->rx_done.entry = NULL;
		}
	}
	free(sc->ss, M_DEVBUF);
	sc->ss = NULL;
}

static int
mxge_alloc_slices(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	struct mxge_slice_state *ss;
	size_t bytes;
	int err, i, max_intr_slots;

	err = mxge_send_cmd(sc, MXGEFW_CMD_GET_RX_RING_SIZE, &cmd);
	if (err != 0) {
		device_printf(sc->dev, "Cannot determine rx ring size\n");
		return err;
	}
	sc->rx_ring_size = cmd.data0;
	max_intr_slots = 2 * (sc->rx_ring_size / sizeof (mcp_dma_addr_t));
	
	bytes = sizeof (*sc->ss) * sc->num_slices;
	sc->ss = malloc(bytes, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->ss == NULL)
		return (ENOMEM);
	for (i = 0; i < sc->num_slices; i++) {
		ss = &sc->ss[i];

		ss->sc = sc;

		/* allocate per-slice rx interrupt queues */
		
		bytes = max_intr_slots * sizeof (*ss->rx_done.entry);
		err = mxge_dma_alloc(sc, &ss->rx_done.dma, bytes, 4096);
		if (err != 0)
			goto abort;
		ss->rx_done.entry = ss->rx_done.dma.addr;
		bzero(ss->rx_done.entry, bytes);

		/* 
		 * allocate the per-slice firmware stats; stats
		 * (including tx) are used used only on the first
		 * slice for now
		 */
		if (i > 0)
			continue;

		bytes = sizeof (*ss->fw_stats);
		err = mxge_dma_alloc(sc, &ss->fw_stats_dma, 
				     sizeof (*ss->fw_stats), 64);
		if (err != 0)
			goto abort;
		ss->fw_stats = (mcp_irq_data_t *)ss->fw_stats_dma.addr;
		snprintf(ss->tx.mtx_name, sizeof(ss->tx.mtx_name),
			 "%s:tx(%d)", device_get_nameunit(sc->dev), i);
		mtx_init(&ss->tx.mtx, ss->tx.mtx_name, NULL, MTX_DEF);
	}

	return (0);

abort:
	mxge_free_slices(sc);
	return (ENOMEM);
}

static void
mxge_slice_probe(mxge_softc_t *sc)
{
	mxge_cmd_t cmd;
	char *old_fw;
	int msix_cnt, status, max_intr_slots;

	sc->num_slices = 1;
	/* 
	 *  don't enable multiple slices if they are not enabled,
	 *  or if this is not an SMP system 
	 */
	
	if (mxge_max_slices == 0 || mxge_max_slices == 1 || mp_ncpus < 2)
		return;

	/* see how many MSI-X interrupts are available */
	msix_cnt = pci_msix_count(sc->dev);
	if (msix_cnt < 2)
		return;

	/* now load the slice aware firmware see what it supports */
	old_fw = sc->fw_name;
	if (old_fw == mxge_fw_aligned)
		sc->fw_name = mxge_fw_rss_aligned;
	else
		sc->fw_name = mxge_fw_rss_unaligned;
	status = mxge_load_firmware(sc, 0);
	if (status != 0) {
		device_printf(sc->dev, "Falling back to a single slice\n");
		return;
	}
	
	/* try to send a reset command to the card to see if it
	   is alive */
	memset(&cmd, 0, sizeof (cmd));
	status = mxge_send_cmd(sc, MXGEFW_CMD_RESET, &cmd);
	if (status != 0) {
		device_printf(sc->dev, "failed reset\n");
		goto abort_with_fw;
	}

	/* get rx ring size */
	status = mxge_send_cmd(sc, MXGEFW_CMD_GET_RX_RING_SIZE, &cmd);
	if (status != 0) {
		device_printf(sc->dev, "Cannot determine rx ring size\n");
		goto abort_with_fw;
	}
	max_intr_slots = 2 * (cmd.data0 / sizeof (mcp_dma_addr_t));

	/* tell it the size of the interrupt queues */
	cmd.data0 = max_intr_slots * sizeof (struct mcp_slot);
	status = mxge_send_cmd(sc, MXGEFW_CMD_SET_INTRQ_SIZE, &cmd);
	if (status != 0) {
		device_printf(sc->dev, "failed MXGEFW_CMD_SET_INTRQ_SIZE\n");
		goto abort_with_fw;
	}

	/* ask the maximum number of slices it supports */
	status = mxge_send_cmd(sc, MXGEFW_CMD_GET_MAX_RSS_QUEUES, &cmd);
	if (status != 0) {
		device_printf(sc->dev,
			      "failed MXGEFW_CMD_GET_MAX_RSS_QUEUES\n");
		goto abort_with_fw;
	}
	sc->num_slices = cmd.data0;
	if (sc->num_slices > msix_cnt)
		sc->num_slices = msix_cnt;

	if (mxge_max_slices == -1) {
		/* cap to number of CPUs in system */
		if (sc->num_slices > mp_ncpus)
			sc->num_slices = mp_ncpus;
	} else {
		if (sc->num_slices > mxge_max_slices)
			sc->num_slices = mxge_max_slices;
	}
	/* make sure it is a power of two */
	while (sc->num_slices & (sc->num_slices - 1))
		sc->num_slices--;

	if (mxge_verbose)
		device_printf(sc->dev, "using %d slices\n",
			      sc->num_slices);
	
	return;

abort_with_fw:
	sc->fw_name = old_fw;
	(void) mxge_load_firmware(sc, 0);
}

static int
mxge_add_msix_irqs(mxge_softc_t *sc)
{
	size_t bytes;
	int count, err, i, rid;

	rid = PCIR_BAR(2);
	sc->msix_table_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
						    &rid, RF_ACTIVE);

	if (sc->msix_table_res == NULL) {
		device_printf(sc->dev, "couldn't alloc MSIX table res\n");
		return ENXIO;
	}

	count = sc->num_slices;
	err = pci_alloc_msix(sc->dev, &count);
	if (err != 0) {
		device_printf(sc->dev, "pci_alloc_msix: failed, wanted %d"
			      "err = %d \n", sc->num_slices, err);
		goto abort_with_msix_table;
	}
	if (count < sc->num_slices) {
		device_printf(sc->dev, "pci_alloc_msix: need %d, got %d\n",
			      count, sc->num_slices);
		device_printf(sc->dev,
			      "Try setting hw.mxge.max_slices to %d\n",
			      count);
		err = ENOSPC;
		goto abort_with_msix;
	}
	bytes = sizeof (*sc->msix_irq_res) * sc->num_slices;
	sc->msix_irq_res = malloc(bytes, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (sc->msix_irq_res == NULL) {
		err = ENOMEM;
		goto abort_with_msix;
	}

	for (i = 0; i < sc->num_slices; i++) {
		rid = i + 1;
		sc->msix_irq_res[i] = bus_alloc_resource_any(sc->dev,
							  SYS_RES_IRQ,
							  &rid, RF_ACTIVE);
		if (sc->msix_irq_res[i] == NULL) {
			device_printf(sc->dev, "couldn't allocate IRQ res"
				      " for message %d\n", i);
			err = ENXIO;
			goto abort_with_res;
		}
	}

	bytes = sizeof (*sc->msix_ih) * sc->num_slices;
	sc->msix_ih =  malloc(bytes, M_DEVBUF, M_NOWAIT|M_ZERO);

	for (i = 0; i < sc->num_slices; i++) {
		err = bus_setup_intr(sc->dev, sc->msix_irq_res[i], 
				     INTR_TYPE_NET | INTR_MPSAFE,
#if __FreeBSD_version > 700030
				     NULL,
#endif
				     mxge_intr, &sc->ss[i], &sc->msix_ih[i]);
		if (err != 0) {
			device_printf(sc->dev, "couldn't setup intr for "
				      "message %d\n", i);
			goto abort_with_intr;
		}
	}

	if (mxge_verbose) {
		device_printf(sc->dev, "using %d msix IRQs:",
			      sc->num_slices);
		for (i = 0; i < sc->num_slices; i++)
			printf(" %ld",  rman_get_start(sc->msix_irq_res[i]));
		printf("\n");
	}
	return (0);

abort_with_intr:
	for (i = 0; i < sc->num_slices; i++) {
		if (sc->msix_ih[i] != NULL) {
			bus_teardown_intr(sc->dev, sc->msix_irq_res[i],
					  sc->msix_ih[i]);
			sc->msix_ih[i] = NULL;
		}
	}
	free(sc->msix_ih, M_DEVBUF);


abort_with_res:
	for (i = 0; i < sc->num_slices; i++) {
		rid = i + 1;
		if (sc->msix_irq_res[i] != NULL)
			bus_release_resource(sc->dev, SYS_RES_IRQ, rid,
					     sc->msix_irq_res[i]);
		sc->msix_irq_res[i] = NULL;
	}
	free(sc->msix_irq_res, M_DEVBUF);


abort_with_msix:
	pci_release_msi(sc->dev);

abort_with_msix_table:
	bus_release_resource(sc->dev, SYS_RES_MEMORY, PCIR_BAR(2),
			     sc->msix_table_res);

	return err;
}

static int
mxge_add_single_irq(mxge_softc_t *sc)
{
	int count, err, rid;

	count = pci_msi_count(sc->dev);
	if (count == 1 && pci_alloc_msi(sc->dev, &count) == 0) {
		rid = 1;
	} else {
		rid = 0;
		sc->legacy_irq = 1;
	}
	sc->irq_res = bus_alloc_resource(sc->dev, SYS_RES_IRQ, &rid, 0, ~0,
					 1, RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(sc->dev, "could not alloc interrupt\n");
		return ENXIO;
	}
	if (mxge_verbose)
		device_printf(sc->dev, "using %s irq %ld\n",
			      sc->legacy_irq ? "INTx" : "MSI",
			      rman_get_start(sc->irq_res));
	err = bus_setup_intr(sc->dev, sc->irq_res, 
			     INTR_TYPE_NET | INTR_MPSAFE,
#if __FreeBSD_version > 700030
			     NULL,
#endif
			     mxge_intr, &sc->ss[0], &sc->ih);
	if (err != 0) {
		bus_release_resource(sc->dev, SYS_RES_IRQ,
				     sc->legacy_irq ? 0 : 1, sc->irq_res);
		if (!sc->legacy_irq)
			pci_release_msi(sc->dev);
	}
	return err;
}

static void
mxge_rem_msix_irqs(mxge_softc_t *sc)
{
	int i, rid;

	for (i = 0; i < sc->num_slices; i++) {
		if (sc->msix_ih[i] != NULL) {
			bus_teardown_intr(sc->dev, sc->msix_irq_res[i],
					  sc->msix_ih[i]);
			sc->msix_ih[i] = NULL;
		}
	}
	free(sc->msix_ih, M_DEVBUF);

	for (i = 0; i < sc->num_slices; i++) {
		rid = i + 1;
		if (sc->msix_irq_res[i] != NULL)
			bus_release_resource(sc->dev, SYS_RES_IRQ, rid,
					     sc->msix_irq_res[i]);
		sc->msix_irq_res[i] = NULL;
	}
	free(sc->msix_irq_res, M_DEVBUF);

	bus_release_resource(sc->dev, SYS_RES_MEMORY, PCIR_BAR(2),
			     sc->msix_table_res);

	pci_release_msi(sc->dev);
	return;
}

static void
mxge_rem_single_irq(mxge_softc_t *sc)
{
	bus_teardown_intr(sc->dev, sc->irq_res, sc->ih);
	bus_release_resource(sc->dev, SYS_RES_IRQ,
			     sc->legacy_irq ? 0 : 1, sc->irq_res);
	if (!sc->legacy_irq)
		pci_release_msi(sc->dev);
}

static void
mxge_rem_irq(mxge_softc_t *sc)
{
	if (sc->num_slices > 1)
		mxge_rem_msix_irqs(sc);
	else
		mxge_rem_single_irq(sc);
}

static int
mxge_add_irq(mxge_softc_t *sc)
{
	int err;

	if (sc->num_slices > 1)
		err = mxge_add_msix_irqs(sc);
	else
		err = mxge_add_single_irq(sc);
	
	if (0 && err == 0 && sc->num_slices > 1) {
		mxge_rem_msix_irqs(sc);
		err = mxge_add_msix_irqs(sc);
	}
	return err;
}


static int 
mxge_attach(device_t dev)
{
	mxge_softc_t *sc = device_get_softc(dev);
	struct ifnet *ifp;
	int err, rid;

	sc->dev = dev;
	mxge_fetch_tunables(sc);

	TASK_INIT(&sc->watchdog_task, 1, mxge_watchdog_task, sc);
	sc->tq = taskqueue_create_fast("mxge_taskq", M_WAITOK,
				       taskqueue_thread_enqueue,
				       &sc->tq);
	if (sc->tq == NULL) {
		err = ENOMEM;
		goto abort_with_nothing;
	}

	err = bus_dma_tag_create(NULL,			/* parent */
				 1,			/* alignment */
				 0,			/* boundary */
				 BUS_SPACE_MAXADDR,	/* low */
				 BUS_SPACE_MAXADDR,	/* high */
				 NULL, NULL,		/* filter */
				 65536 + 256,		/* maxsize */
				 MXGE_MAX_SEND_DESC, 	/* num segs */
				 65536,			/* maxsegsize */
				 0,			/* flags */
				 NULL, NULL,		/* lock */
				 &sc->parent_dmat);	/* tag */

	if (err != 0) {
		device_printf(sc->dev, "Err %d allocating parent dmat\n",
			      err);
		goto abort_with_tq;
	}

	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		err = ENOSPC;
		goto abort_with_parent_dmat;
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));

	snprintf(sc->cmd_mtx_name, sizeof(sc->cmd_mtx_name), "%s:cmd",
		 device_get_nameunit(dev));
	mtx_init(&sc->cmd_mtx, sc->cmd_mtx_name, NULL, MTX_DEF);
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

	err = mxge_dma_alloc(sc, &sc->dmabench_dma, 4096, 4096);
	if (err != 0)
		goto abort_with_zeropad_dma;

	/* select & load the firmware */
	err = mxge_select_firmware(sc);
	if (err != 0)
		goto abort_with_dmabench;
	sc->intr_coal_delay = mxge_intr_coal_delay;

	mxge_slice_probe(sc);
	err = mxge_alloc_slices(sc);
	if (err != 0)
		goto abort_with_dmabench;

	err = mxge_reset(sc, 0);
	if (err != 0)
		goto abort_with_slices;

	err = mxge_alloc_rings(sc);
	if (err != 0) {
		device_printf(sc->dev, "failed to allocate rings\n");
		goto abort_with_slices;
	}

	err = mxge_add_irq(sc);
	if (err != 0) {
		device_printf(sc->dev, "failed to add irq\n");
		goto abort_with_rings;
	}

	ifp->if_baudrate = IF_Gbps(10UL);
	ifp->if_capabilities = IFCAP_RXCSUM | IFCAP_TXCSUM | IFCAP_TSO4 |
		IFCAP_VLAN_MTU | IFCAP_LRO;

#ifdef MXGE_NEW_VLAN_API
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWCSUM;
#endif

	sc->max_mtu = mxge_max_mtu(sc);
	if (sc->max_mtu >= 9000)
		ifp->if_capabilities |= IFCAP_JUMBO_MTU;
	else
		device_printf(dev, "MTU limited to %d.  Install "
			      "latest firmware for 9000 byte jumbo support\n",
			      sc->max_mtu - ETHER_HDR_LEN);
	ifp->if_hwassist = CSUM_TCP | CSUM_UDP | CSUM_TSO;
	ifp->if_capenable = ifp->if_capabilities;
	if (sc->lro_cnt == 0)
		ifp->if_capenable &= ~IFCAP_LRO;
	sc->csum_flag = 1;
        ifp->if_init = mxge_init;
        ifp->if_softc = sc;
        ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
        ifp->if_ioctl = mxge_ioctl;
        ifp->if_start = mxge_start;
	/* Initialise the ifmedia structure */
	ifmedia_init(&sc->media, 0, mxge_media_change, 
		     mxge_media_status);
	mxge_set_media(sc, IFM_ETHER | IFM_AUTO);
	mxge_media_probe(sc);
	sc->dying = 0;
	ether_ifattach(ifp, sc->mac_addr);
	/* ether_ifattach sets mtu to ETHERMTU */
	if (mxge_initial_mtu != ETHERMTU)
		mxge_change_mtu(sc, mxge_initial_mtu);

	mxge_add_sysctls(sc);
	taskqueue_start_threads(&sc->tq, 1, PI_NET, "%s taskq",
				device_get_nameunit(sc->dev));
	callout_reset(&sc->co_hdl, mxge_ticks, mxge_tick, sc);
	return 0;

abort_with_rings:
	mxge_free_rings(sc);
abort_with_slices:
	mxge_free_slices(sc);
abort_with_dmabench:
	mxge_dma_free(&sc->dmabench_dma);
abort_with_zeropad_dma:
	mxge_dma_free(&sc->zeropad_dma);
abort_with_cmd_dma:
	mxge_dma_free(&sc->cmd_dma);
abort_with_mem_res:
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BARS, sc->mem_res);
abort_with_lock:
	pci_disable_busmaster(dev);
	mtx_destroy(&sc->cmd_mtx);
	mtx_destroy(&sc->driver_mtx);
	if_free(ifp);
abort_with_parent_dmat:
	bus_dma_tag_destroy(sc->parent_dmat);
abort_with_tq:
	if (sc->tq != NULL) {
		taskqueue_drain(sc->tq, &sc->watchdog_task);
		taskqueue_free(sc->tq);
		sc->tq = NULL;
	}
abort_with_nothing:
	return err;
}

static int
mxge_detach(device_t dev)
{
	mxge_softc_t *sc = device_get_softc(dev);

	if (mxge_vlans_active(sc)) {
		device_printf(sc->dev,
			      "Detach vlans before removing module\n");
		return EBUSY;
	}
	mtx_lock(&sc->driver_mtx);
	sc->dying = 1;
	if (sc->ifp->if_drv_flags & IFF_DRV_RUNNING)
		mxge_close(sc, 0);
	mtx_unlock(&sc->driver_mtx);
	ether_ifdetach(sc->ifp);
	if (sc->tq != NULL) {
		taskqueue_drain(sc->tq, &sc->watchdog_task);
		taskqueue_free(sc->tq);
		sc->tq = NULL;
	}
	callout_drain(&sc->co_hdl);
	ifmedia_removeall(&sc->media);
	mxge_dummy_rdma(sc, 0);
	mxge_rem_sysctls(sc);
	mxge_rem_irq(sc);
	mxge_free_rings(sc);
	mxge_free_slices(sc);
	mxge_dma_free(&sc->dmabench_dma);
	mxge_dma_free(&sc->zeropad_dma);
	mxge_dma_free(&sc->cmd_dma);
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BARS, sc->mem_res);
	pci_disable_busmaster(dev);
	mtx_destroy(&sc->cmd_mtx);
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
