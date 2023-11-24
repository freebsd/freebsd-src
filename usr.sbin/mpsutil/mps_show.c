/*-
 * Copyright (c) 2015 Netflix, Inc.
 * Written by: Scott Long <scottl@freebsd.org>
 *
 * Copyright (c) 2008 Yahoo!, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/endian.h>
#include <err.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mpsutil.h"

static char * get_device_speed(uint8_t rate);
static char * get_device_type(uint32_t di);
static int show_all(int ac, char **av);
static int show_devices(int ac, char **av);
static int show_enclosures(int ac, char **av);
static int show_expanders(int ac, char **av);

MPS_TABLE(top, show);

#define	STANDALONE_STATE	"ONLINE"

static int
show_adapter(int ac, char **av)
{
	const char* pcie_speed[] = { "2.5", "5.0", "8.0", "16.0", "32.0" };
	const char* temp_units[] = { "", "F", "C" };
	const char* ioc_speeds[] = { "", "Full", "Half", "Quarter", "Eighth" };

	MPI2_CONFIG_PAGE_SASIOUNIT_0	*sas0;
	MPI2_CONFIG_PAGE_SASIOUNIT_1	*sas1;
	MPI2_SAS_IO_UNIT0_PHY_DATA	*phy0;
	MPI2_SAS_IO_UNIT1_PHY_DATA	*phy1;
	MPI2_CONFIG_PAGE_MAN_0 *man0;
	MPI2_CONFIG_PAGE_BIOS_3 *bios3;
	MPI2_CONFIG_PAGE_IO_UNIT_1 *iounit1;
	MPI2_CONFIG_PAGE_IO_UNIT_7 *iounit7;
	MPI2_IOC_FACTS_REPLY *facts;
	U16 IOCStatus;
	char *speed, *minspeed, *maxspeed, *isdisabled, *type;
	char devhandle[8], ctrlhandle[8];
	int error, fd, v, i;

	if (ac != 1) {
		warnx("show adapter: extra arguments");
		return (EINVAL);
	}

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	man0 = mps_read_man_page(fd, 0, NULL);
	if (man0 == NULL) {
		error = errno;
		warn("Failed to get controller info");
		return (error);
	}
	if (man0->Header.PageLength < sizeof(*man0) / 4) {
		warnx("Invalid controller info");
		return (EINVAL);
	}
	printf("mp%s%d Adapter:\n", is_mps ? "s": "r", mps_unit);
	printf("       Board Name: %.16s\n", man0->BoardName);
	printf("   Board Assembly: %.16s\n", man0->BoardAssembly);
	printf("        Chip Name: %.16s\n", man0->ChipName);
	printf("    Chip Revision: %.16s\n", man0->ChipRevision);
	free(man0);

	bios3 = mps_read_config_page(fd, MPI2_CONFIG_PAGETYPE_BIOS, 3, 0, NULL);
	if (bios3 == NULL) {
		error = errno;
		warn("Failed to get BIOS page 3 info");
		return (error);
	}
	v = le32toh(bios3->BiosVersion);
	printf("    BIOS Revision: %d.%02d.%02d.%02d\n",
	    ((v & 0xff000000) >> 24), ((v &0xff0000) >> 16),
	    ((v & 0xff00) >> 8), (v & 0xff));
	free(bios3);

	if ((facts = mps_get_iocfacts(fd)) == NULL) {
		printf("could not get controller IOCFacts\n");
		close(fd);
		return (errno);
	}
	v = facts->FWVersion.Word;
	printf("Firmware Revision: %d.%02d.%02d.%02d\n",
	    ((v & 0xff000000) >> 24), ((v &0xff0000) >> 16),
	    ((v & 0xff00) >> 8), (v & 0xff));
	printf("  Integrated RAID: %s\n",
	    (facts->IOCCapabilities & MPI2_IOCFACTS_CAPABILITY_INTEGRATED_RAID)
	    ? "yes" : "no");
	free(facts);

	iounit1 = mps_read_config_page(fd, MPI2_CONFIG_PAGETYPE_IO_UNIT, 1, 0, NULL);
	if (iounit1 == NULL) {
		error = errno;
		warn("Failed to get IOUNIT page 1 info");
		return (error);
	}
	printf("         SATA NCQ: %s\n",
		((iounit1->Flags & MPI2_IOUNITPAGE1_NATIVE_COMMAND_Q_DISABLE) == 0) ?
		"ENABLED" : "DISABLED");
	free(iounit1);

	iounit7 = mps_read_config_page(fd, MPI2_CONFIG_PAGETYPE_IO_UNIT, 7, 0, NULL);
	if (iounit7 == NULL) {
		error = errno;
		warn("Failed to get IOUNIT page 7 info");
		return (error);
	}
	printf(" PCIe Width/Speed: x%d (%s GB/sec)\n", iounit7->PCIeWidth,
		pcie_speed[iounit7->PCIeSpeed]);
	printf("        IOC Speed: %s\n", ioc_speeds[iounit7->IOCSpeed]);
	printf("      Temperature: ");
	if (iounit7->IOCTemperatureUnits == MPI2_IOUNITPAGE7_IOC_TEMP_NOT_PRESENT)
		printf("Unknown/Unsupported\n");
	else
		printf("%d %s\n", iounit7->IOCTemperature,
			temp_units[iounit7->IOCTemperatureUnits]);
	free(iounit7);

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	sas0 = mps_read_extended_config_page(fd,
	    MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT,
	    MPI2_SASIOUNITPAGE0_PAGEVERSION, 0, 0, &IOCStatus);
	if (sas0 == NULL) {
		error = errno;
		warn("Error retrieving SAS IO Unit page %d", IOCStatus);
		free(sas0);
		close(fd);
		return (error);
	}

	sas1 = mps_read_extended_config_page(fd,
	    MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT,
	    MPI2_SASIOUNITPAGE1_PAGEVERSION, 1, 0, &IOCStatus);
	if (sas1 == NULL) {
		error = errno;
		warn("Error retrieving SAS IO Unit page %d", IOCStatus);
		free(sas0);
		close(fd);
		return (error);
	}
	printf("\n");

	printf("%-8s%-12s%-11s%-10s%-8s%-7s%-7s%s\n", "PhyNum", "CtlrHandle",
	    "DevHandle", "Disabled", "Speed", "Min", "Max", "Device");
	for (i = 0; i < sas0->NumPhys; i++) {
		phy0 = &sas0->PhyData[i];
		phy1 = &sas1->PhyData[i];
		if (phy0->PortFlags &
		     MPI2_SASIOUNIT0_PORTFLAGS_DISCOVERY_IN_PROGRESS) {
			printf("Discovery still in progress\n");
			continue;
		}
		if (phy0->PhyFlags & MPI2_SASIOUNIT0_PHYFLAGS_PHY_DISABLED)
			isdisabled = "Y";
		else
			isdisabled = "N";

		minspeed = get_device_speed(phy1->MaxMinLinkRate);
		maxspeed = get_device_speed(phy1->MaxMinLinkRate >> 4);
		type = get_device_type(le32toh(phy0->ControllerPhyDeviceInfo));

		if (le16toh(phy0->AttachedDevHandle) != 0) {
			snprintf(devhandle, sizeof(devhandle), "%04x",
			    le16toh(phy0->AttachedDevHandle));
			snprintf(ctrlhandle, sizeof(ctrlhandle), "%04x",
			    le16toh(phy0->ControllerDevHandle));
			speed = get_device_speed(phy0->NegotiatedLinkRate);
		} else {
			snprintf(devhandle, sizeof(devhandle), "    ");
			snprintf(ctrlhandle, sizeof(ctrlhandle), "    ");
			speed = "     ";
		}
		printf("%-8d%-12s%-11s%-10s%-8s%-7s%-7s%s\n",
		    i, ctrlhandle, devhandle, isdisabled, speed, minspeed,
		    maxspeed, type);
	}
	free(sas0);
	free(sas1);
	printf("\n");
	close(fd);
	return (0);
}

MPS_COMMAND(show, adapter, show_adapter, "", "display controller information")

static int
show_iocfacts(int ac, char **av)
{
	MPI2_IOC_FACTS_REPLY *facts;
	uint8_t *fb;
	char tmpbuf[128];
	int error, fd;

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	if ((facts = mps_get_iocfacts(fd)) == NULL) {
		printf("could not get controller IOCFacts\n");
		close(fd);
		return (errno);
	}

	fb = (uint8_t *)facts;

#define IOCCAP "\3ScsiTaskFull" "\4DiagTrace" "\5SnapBuf" "\6ExtBuf" \
    "\7EEDP" "\10BiDirTarg" "\11Multicast" "\14TransRetry" "\15IR" \
    "\16EventReplay" "\17RaidAccel" "\20MSIXIndex" "\21HostDisc" \
    "\22FastPath" "\23RDPQArray" "\24AtomicReqDesc" "\25PCIeSRIOV"

	bzero(tmpbuf, sizeof(tmpbuf));
	mps_parse_flags(facts->IOCCapabilities, IOCCAP, tmpbuf, sizeof(tmpbuf));

	printf("          MsgVersion: %d.%d\n",
	    facts->MsgVersion >> 8, facts->MsgVersion & 0xff);
	printf("           MsgLength: %d\n", facts->MsgLength);
	printf("            Function: 0x%x\n", facts->Function);
	printf("       HeaderVersion: %02d,%02d\n",
	    facts->HeaderVersion >> 8, facts->HeaderVersion & 0xff);
	printf("           IOCNumber: %d\n", facts->IOCNumber);
	printf("            MsgFlags: 0x%x\n", facts->MsgFlags);
	printf("               VP_ID: %d\n", facts->VP_ID);
	printf("               VF_ID: %d\n", facts->VF_ID);
	printf("       IOCExceptions: %d\n", facts->IOCExceptions);
	printf("           IOCStatus: %d\n", facts->IOCStatus);
	printf("          IOCLogInfo: 0x%x\n", facts->IOCLogInfo);
	printf("       MaxChainDepth: %d\n", facts->MaxChainDepth);
	printf("             WhoInit: 0x%x\n", facts->WhoInit);
	printf("       NumberOfPorts: %d\n", facts->NumberOfPorts);
	printf("      MaxMSIxVectors: %d\n", facts->MaxMSIxVectors);
	printf("       RequestCredit: %d\n", facts->RequestCredit);
	printf("           ProductID: 0x%x\n", facts->ProductID);
	printf("     IOCCapabilities: 0x%x %s\n", facts->IOCCapabilities,
	    tmpbuf);
	printf("           FWVersion: %02d.%02d.%02d.%02d\n",
	    facts->FWVersion.Struct.Major, facts->FWVersion.Struct.Minor,
	    facts->FWVersion.Struct.Unit, facts->FWVersion.Struct.Dev);
	printf(" IOCRequestFrameSize: %d\n", facts->IOCRequestFrameSize);
	if (is_mps == 0)
		printf(" MaxChainSegmentSize: %d\n", (uint16_t)(fb[0x26]));
	printf("       MaxInitiators: %d\n", facts->MaxInitiators);
	printf("          MaxTargets: %d\n", facts->MaxTargets);
	printf("     MaxSasExpanders: %d\n", facts->MaxSasExpanders);
	printf("       MaxEnclosures: %d\n", facts->MaxEnclosures);

	bzero(tmpbuf, sizeof(tmpbuf));
	mps_parse_flags(facts->ProtocolFlags,
	    "\4NvmeDevices\2ScsiTarget\1ScsiInitiator", tmpbuf, sizeof(tmpbuf));
	printf("       ProtocolFlags: 0x%x %s\n", facts->ProtocolFlags, tmpbuf);
	printf("  HighPriorityCredit: %d\n", facts->HighPriorityCredit);
	printf("MaxRepDescPostQDepth: %d\n",
	    facts->MaxReplyDescriptorPostQueueDepth);
	printf("      ReplyFrameSize: %d\n", facts->ReplyFrameSize);
	printf("          MaxVolumes: %d\n", facts->MaxVolumes);
	printf("        MaxDevHandle: %d\n", facts->MaxDevHandle);
	printf("MaxPersistentEntries: %d\n", facts->MaxPersistentEntries);
	printf("        MinDevHandle: %d\n", facts->MinDevHandle);
	if (is_mps == 0)
		printf(" CurrentHostPageSize: %d\n", (uint8_t)(fb[0x3e]));

	free(facts);
	return (0);
}

MPS_COMMAND(show, iocfacts, show_iocfacts, "", "Show IOC Facts Message");

static int
show_adapters(int ac, char **av)
{
	MPI2_CONFIG_PAGE_MAN_0 *man0;
	MPI2_IOC_FACTS_REPLY *facts;
	int unit, fd, error;

	printf("Device Name\t      Chip Name        Board Name        Firmware\n");
	for (unit = 0; unit < MPS_MAX_UNIT; unit++) {
		fd = mps_open(unit);
		if (fd < 0)
			continue;
		facts = mps_get_iocfacts(fd);
		if (facts == NULL) {
			error = errno;
			warn("Faled to get controller iocfacts");
			close(fd);
			return (error);
		}
		man0 = mps_read_man_page(fd, 0, NULL);
		if (man0 == NULL) {
			error = errno;
			warn("Failed to get controller info");
			close(fd);
			free(facts);
			return (error);
		}
		if (man0->Header.PageLength < sizeof(*man0) / 4) {
			warnx("Invalid controller info");
			close(fd);
			free(man0);
			free(facts);
			return (EINVAL);
		}
		printf("/dev/mp%s%d\t%16s %16s        %08x\n",
		    is_mps ? "s": "r", unit,
		    man0->ChipName, man0->BoardName, facts->FWVersion.Word);
		free(man0);
		free(facts);
		close(fd);
	}
	return (0);
}
MPS_COMMAND(show, adapters, show_adapters, "", "Show a summary of all adapters");

static char *
get_device_type(uint32_t di)
{

	if (di & 0x4000)
		return ("SEP Target    ");
	if (di & 0x2000)
		return ("ATAPI Target  ");
	if (di & 0x400)
		return ("SAS Target    ");
	if (di & 0x200)
		return ("STP Target    ");
	if (di & 0x100)
		return ("SMP Target    ");
	if (di & 0x80)
		return ("SATA Target   ");
	if (di & 0x70)
		return ("SAS Initiator ");
	if (di & 0x8)
		return ("SATA Initiator");
	if ((di & 0x7) == 0)
		return ("No Device     ");
	return ("Unknown Device");
}

static char *
get_enc_type(uint32_t flags, int *issep)
{
	char *type;

	*issep = 0;
	switch (flags & 0xf) {
	case 0x01:
		type = "Direct Attached SES-2";
		*issep = 1;
		break;
	case 0x02:
		type = "Direct Attached SGPIO";
		break;
	case 0x03:
		type = "Expander SGPIO";
		break;
	case 0x04:
		type = "External SES-2";
		*issep = 1;
		break;
	case 0x05:
		type = "Direct Attached GPIO";
		break;
	case 0x0:
	default:
		return ("Unknown");
	}

	return (type);
}

static char *
mps_device_speed[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"1.5",
	"3.0",
	"6.0",
	"12 "
};

static char *
get_device_speed(uint8_t rate)
{
	char *speed;

	rate &= 0xf;
	if (rate >= sizeof(mps_device_speed))
		return ("Unk");

	if ((speed = mps_device_speed[rate]) == NULL)
		return ("???");
	return (speed);
}

static char *
mps_page_name[] = {
	"IO Unit",
	"IOC",
	"BIOS",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"RAID Volume",
	"Manufacturing",
	"RAID Physical Disk",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"SAS IO Unit",
	"SAS Expander",
	"SAS Device",
	"SAS PHY",
	"Log",
	"Enclosure",
	"RAID Configuration",
	"Driver Persistent Mapping",
	"SAS Port",
	"Ethernet Port",
	"Extended Manufacturing"
};

static char *
get_page_name(u_int page)
{
	char *name;

	if (page >= sizeof(mps_page_name))
		return ("Unknown");
	if ((name = mps_page_name[page]) == NULL)
		return ("Unknown");
	return (name);
}

static int
show_all(int ac, char **av)
{
	int error;

	printf("Adapter:\n");
	error = show_adapter(ac, av);
	printf("Devices:\n");
	error = show_devices(ac, av);
	printf("Enclosures:\n");
	error = show_enclosures(ac, av);
	printf("Expanders:\n");
	error = show_expanders(ac, av);
	return (error);
}
MPS_COMMAND(show, all, show_all, "", "Show all devices");

static int
show_devices(int ac, char **av)
{
	MPI2_CONFIG_PAGE_SASIOUNIT_0	*sas0;
	MPI2_SAS_IO_UNIT0_PHY_DATA	*phydata;
	MPI2_CONFIG_PAGE_SAS_DEV_0	*device;
	MPI2_CONFIG_PAGE_EXPANDER_1	*exp1;
	uint16_t IOCStatus, handle, bus, target;
	char *type, *speed, enchandle[8], slot[8], bt[16];
	char buf[256];
	int fd, error, nphys;

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	sas0 = mps_read_extended_config_page(fd,
	    MPI2_CONFIG_EXTPAGETYPE_SAS_IO_UNIT,
	    MPI2_SASIOUNITPAGE0_PAGEVERSION, 0, 0, &IOCStatus);
	if (sas0 == NULL) {
		error = errno;
		warn("Error retrieving SAS IO Unit page %d", IOCStatus);
		return (error);
	}
	nphys = sas0->NumPhys;

	printf("B____%-5s%-17s%-8s%-10s%-14s%-6s%-5s%-6s%s\n",
	    "T", "SAS Address", "Handle", "Parent", "Device", "Speed",
	    "Enc", "Slot", "Wdt");
	handle = 0xffff;
	while (1) {
		device = mps_read_extended_config_page(fd,
		    MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE,
		    MPI2_SASDEVICE0_PAGEVERSION, 0,
		    MPI2_SAS_DEVICE_PGAD_FORM_GET_NEXT_HANDLE | handle,
		    &IOCStatus);
		if (device == NULL) {
			if (IOCStatus == MPI2_IOCSTATUS_CONFIG_INVALID_PAGE)
				break;
			error = errno;
			warn("Error retrieving device page");
			close(fd);
			return (error);
		}
		handle = le16toh(device->DevHandle);

		if (device->ParentDevHandle == 0x0) {
			free(device);
			continue;
		}

		bus = 0xffff;
		target = 0xffff;
		error = mps_map_btdh(fd, &handle, &bus, &target);
		if (error) {
			free(device);
			continue;
		}
		if ((bus == 0xffff) || (target == 0xffff))
			snprintf(bt, sizeof(bt), "       ");
		else
			snprintf(bt, sizeof(bt), "%02d   %02d", bus, target);

		type = get_device_type(le32toh(device->DeviceInfo));

		if (device->DeviceInfo & 0x800) {	/* Direct Attached */
			if (device->PhyNum < nphys) {
				phydata = &sas0->PhyData[device->PhyNum];
				speed = get_device_speed(phydata->NegotiatedLinkRate);
			} else
				speed = "";
		} else if (device->ParentDevHandle > 0) {
			exp1 = mps_read_extended_config_page(fd,
			    MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER,
			    MPI2_SASEXPANDER1_PAGEVERSION, 1,
			    MPI2_SAS_EXPAND_PGAD_FORM_HNDL_PHY_NUM |
			    (device->PhyNum <<
			    MPI2_SAS_EXPAND_PGAD_PHYNUM_SHIFT) |
			    le16toh(device->ParentDevHandle), &IOCStatus);
			if (exp1 == NULL) {
				if (IOCStatus != MPI2_IOCSTATUS_CONFIG_INVALID_PAGE) {
					error = errno;
					warn("Error retrieving expander page 1: 0x%x",
					    IOCStatus);
					close(fd);
					free(device);
					return (error);
				}
				speed = "";
			} else {
				speed = get_device_speed(exp1->NegotiatedLinkRate);
				free(exp1);
			}
		} else
			speed = "";

		if (device->EnclosureHandle != 0) {
			snprintf(enchandle, sizeof(enchandle), "%04x", le16toh(device->EnclosureHandle));
			snprintf(slot, sizeof(slot), "%02d", le16toh(device->Slot));
		} else {
			snprintf(enchandle, sizeof(enchandle), "    ");
			snprintf(slot, sizeof(slot), "  ");
		}
		printf("%-10s", bt);
		snprintf(buf, sizeof(buf), "%08x%08x", le32toh(device->SASAddress.High),
		    le32toh(device->SASAddress.Low));
		printf("%-17s", buf);
		snprintf(buf, sizeof(buf), "%04x", le16toh(device->DevHandle));
		printf("%-8s", buf);
		snprintf(buf, sizeof(buf), "%04x", le16toh(device->ParentDevHandle));
		printf("%-10s", buf);
		printf("%-14s%-6s%-5s%-6s%d\n", type, speed,
		    enchandle, slot, device->MaxPortConnections);
		free(device);
	}
	printf("\n");
	free(sas0);
	close(fd);
	return (0);
}
MPS_COMMAND(show, devices, show_devices, "", "Show attached devices");

static int
show_enclosures(int ac, char **av)
{
	MPI2_CONFIG_PAGE_SAS_ENCLOSURE_0 *enc;
	char *type, sepstr[8];
	uint16_t IOCStatus, handle;
	int fd, error, issep;

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	printf("Slots      Logical ID     SEPHandle  EncHandle    Type\n");
	handle = 0xffff;
	while (1) {
		enc = mps_read_extended_config_page(fd,
		    MPI2_CONFIG_EXTPAGETYPE_ENCLOSURE,
		    MPI2_SASENCLOSURE0_PAGEVERSION, 0,
		    MPI2_SAS_ENCLOS_PGAD_FORM_GET_NEXT_HANDLE | handle,
		    &IOCStatus);
		if (enc == NULL) {
			if (IOCStatus == MPI2_IOCSTATUS_CONFIG_INVALID_PAGE)
				break;
			error = errno;
			warn("Error retrieving enclosure page");
			close(fd);
			return (error);
		}
		type = get_enc_type(le16toh(enc->Flags), &issep);
		if (issep == 0)
			snprintf(sepstr, sizeof(sepstr), "    ");
		else
			snprintf(sepstr, sizeof(sepstr), "%04x", le16toh(enc->SEPDevHandle));
		printf("  %.2d    %08x%08x    %s       %04x     %s\n",
		    le16toh(enc->NumSlots), le32toh(enc->EnclosureLogicalID.High),
		    le32toh(enc->EnclosureLogicalID.Low), sepstr, le16toh(enc->EnclosureHandle),
		    type);
		handle = le16toh(enc->EnclosureHandle);
		free(enc);
	}
	printf("\n");
	close(fd);
	return (0);
}
MPS_COMMAND(show, enclosures, show_enclosures, "", "Show attached enclosures");

static int
show_expanders(int ac, char **av)
{
	MPI2_CONFIG_PAGE_EXPANDER_0	*exp0;
	MPI2_CONFIG_PAGE_EXPANDER_1	*exp1;
	uint16_t IOCStatus, handle;
	char enchandle[8], parent[8], rphy[4], rhandle[8];
	char *speed, *min, *max, *type;
	int fd, error, nphys, i;

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	printf("NumPhys   SAS Address     DevHandle   Parent  EncHandle  SAS Level\n");
	handle = 0xffff;
	while (1) {
		exp0 = mps_read_extended_config_page(fd,
		    MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER,
		    MPI2_SASEXPANDER0_PAGEVERSION, 0,
		    MPI2_SAS_EXPAND_PGAD_FORM_GET_NEXT_HNDL | handle,
		    &IOCStatus);
		if (exp0 == NULL) {
			if (IOCStatus == MPI2_IOCSTATUS_CONFIG_INVALID_PAGE)
				break;
			error = errno;
			warn("Error retrieving expander page 0");
			close(fd);
			return (error);
		}

		nphys = exp0->NumPhys;
		handle = le16toh(exp0->DevHandle);

		if (exp0->EnclosureHandle == 0x00)
			snprintf(enchandle, sizeof(enchandle), "    ");
		else
			snprintf(enchandle, sizeof(enchandle), "%04d", le16toh(exp0->EnclosureHandle));
		if (exp0->ParentDevHandle == 0x0)
			snprintf(parent, sizeof(parent), "    ");
		else
			snprintf(parent, sizeof(parent), "%04x", le16toh(exp0->ParentDevHandle));
		printf("  %02d    %08x%08x    %04x       %s     %s       %d\n",
		    exp0->NumPhys, le32toh(exp0->SASAddress.High), le32toh(exp0->SASAddress.Low),
		    le16toh(exp0->DevHandle), parent, enchandle, exp0->SASLevel);

		printf("\n");
		printf("     Phy  RemotePhy  DevHandle  Speed  Min   Max    Device\n");
		for (i = 0; i < nphys; i++) {
			exp1 = mps_read_extended_config_page(fd,
			    MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER,
			    MPI2_SASEXPANDER1_PAGEVERSION, 1,
			    MPI2_SAS_EXPAND_PGAD_FORM_HNDL_PHY_NUM |
			    (i << MPI2_SAS_EXPAND_PGAD_PHYNUM_SHIFT) |
			    exp0->DevHandle, &IOCStatus);
			if (exp1 == NULL) {
				if (IOCStatus !=
				    MPI2_IOCSTATUS_CONFIG_INVALID_PAGE)
					warn("Error retrieving expander pg 1");
				continue;
			}
			type = get_device_type(le32toh(exp1->AttachedDeviceInfo));
			if ((le32toh(exp1->AttachedDeviceInfo) &0x7) == 0) {
				speed = "   ";
				snprintf(rphy, sizeof(rphy), "  ");
				snprintf(rhandle, sizeof(rhandle), "    ");
			} else {
				speed = get_device_speed(
				    exp1->NegotiatedLinkRate);
				snprintf(rphy, sizeof(rphy), "%02d",
				    exp1->AttachedPhyIdentifier);
				snprintf(rhandle, sizeof(rhandle), "%04x",
				    le16toh(exp1->AttachedDevHandle));
			}
			min = get_device_speed(exp1->HwLinkRate);
			max = get_device_speed(exp1->HwLinkRate >> 4);
			printf("     %02d      %s        %s      %s   %s   %s   %s\n", exp1->Phy, rphy, rhandle, speed, min, max, type);

			free(exp1);
		}
		free(exp0);
	}

	printf("\n");
	close(fd);
	return (0);
}

MPS_COMMAND(show, expanders, show_expanders, "", "Show attached expanders");

static int
show_cfgpage(int ac, char **av)
{
	MPI2_CONFIG_PAGE_HEADER *hdr;
	MPI2_CONFIG_EXTENDED_PAGE_HEADER *ehdr;
	void *data;
	uint32_t addr;
	uint16_t IOCStatus;
	uint8_t page, num;
	int fd, error, len, attrs;
	char *pgname, *pgattr;

	fd = mps_open(mps_unit);
	if (fd < 0) {
		error = errno;
		warn("mps_open");
		return (error);
	}

	addr = 0;
	num = 0;
	page = 0;

	switch (ac) {
	case 4:
		addr = htole32((uint32_t)strtoul(av[3], NULL, 0));
	case 3:
		num = (uint8_t)strtoul(av[2], NULL, 0);
	case 2:
		page = (uint8_t)strtoul(av[1], NULL, 0);
		break;
	default:
		errno = EINVAL;
		warn("cfgpage: not enough arguments");
		return (EINVAL);
	}

	if (page >= 0x10)
		data = mps_read_extended_config_page(fd, page, 0, num, addr,
		    &IOCStatus);
	 else 
		data = mps_read_config_page(fd, page, num, addr, &IOCStatus);

	if (data == NULL) {
		error = errno;
		warn("Error retrieving cfg page: %s\n",
		    mps_ioc_status(IOCStatus));
		return (error);
	}

	if (page >= 0x10) {
		ehdr = data;
		len = le16toh(ehdr->ExtPageLength) * 4;
		page = ehdr->ExtPageType;
		attrs = ehdr->PageType >> 4;
	} else {
		hdr = data;
		len = hdr->PageLength * 4;
		page = hdr->PageType & 0xf;
		attrs = hdr->PageType >> 4;
	}

	pgname = get_page_name(page);
	if (attrs == 0)
		pgattr = "Read-only";
	else if (attrs == 1)
		pgattr = "Read-Write";
	else if (attrs == 2)
		pgattr = "Read-Write Persistent";
	else
		pgattr = "Unknown Page Attribute";

	printf("Page 0x%x: %s %d, %s\n", page, pgname, num, pgattr);
	hexdump(data, len, NULL, HD_REVERSED | 4);
	free(data);
	close(fd);
	return (0);
}

MPS_COMMAND(show, cfgpage, show_cfgpage, "page [num] [addr]", "Display config page");
