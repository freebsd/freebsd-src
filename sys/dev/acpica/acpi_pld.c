/*
 * Copyright (c) 2026 Takanori Watanabe
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 *  Utilities to handle _PLD object.
 */

#include <sys/cdefs.h>
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/sbuf.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>

int acpi_get_pld_info(struct sbuf *sb, uint8_t *pldbuf, size_t len)
{
	ACPI_PLD_INFO *pld;
	const char *panposstr[] = {"Top", "Bottom", "Left",
				   "Right", "Front", "Back",
				   "Unknown", "Invalid"};
	const char *shapestr[] = {"Round", "Oval", "Square", "VRect", "HRect",
				  "VTrape", "HTrape", "Unknown", "Chamferd",
				  "Rsvd", "Rsvd", "Rsvd", "Rsvd",
				  "Rsvd", "Rsvd", "Rsvd", "Rsvd"};
	const char *vposstr[] = {"Upper", "Center", "Lower", "Invalid"};
	const char *hposstr[] = {"Left", "Center", "Right", "Invalid"};	

	if (ACPI_SUCCESS(AcpiDecodePldBuffer(pldbuf, len, &pld))){
		sbuf_printf(sb, "\tRevision:%d\n", pld->Revision);
		if (!pld->IgnoreColor){
			sbuf_printf(sb,"\tColor:#%02x%02x%02x\n",
				    pld->Red,pld->Green, pld->Blue);
		}
		sbuf_printf(sb, "\tWidth %d mm Height %d mm\n",
			    pld->Width, pld->Height);
		if (pld->UserVisible)
			sbuf_printf(sb, "\tVisible\n");
		if (pld->Dock)
			sbuf_printf(sb, "\tDock\n");
		if (pld->Lid)
			sbuf_printf(sb, "\tLid\n");
		sbuf_printf(sb, "\tPanel: %s\n", panposstr[pld->Panel]);
		if (pld->Panel < ACPI_PLD_PANEL_UNKNOWN) {
			sbuf_printf(sb, "\tVPos: %s\n",
				    vposstr[pld->VerticalPosition]);
			sbuf_printf(sb, "\tHPos: %s\n",
				    hposstr[pld->HorizontalPosition]);
		}
		sbuf_printf(sb, "\tShape: %s\n", shapestr[pld->Shape]);
		sbuf_printf(sb, "\tGroupOrientation %s\n", (pld->GroupOrientation) ?
			    "Vertical" : "Horizontal");
		sbuf_printf(sb, "\tGroupToken %d\n", pld->GroupToken);
		sbuf_printf(sb, "\tGroupPosition %d\n", pld->GroupPosition);
		if(pld->Bay)
			sbuf_printf(sb, "\tBay \n");
		if(pld->Ejectable)
			sbuf_printf(sb, "\tEjectable \n");
		if (pld->OspmEjectRequired)
			sbuf_printf(sb, "\tOSPM Eject Required\n");
		sbuf_printf(sb, "\tCabinetNumber %d\n", pld->CabinetNumber);
		sbuf_printf(sb, "\tCardCageNumber %d\n", pld->CardCageNumber);
		sbuf_printf(sb, "\tReference %d\n", pld->Reference);
		sbuf_printf(sb, "\tRotation %d\n", pld->Rotation);
		if (pld->Revision >= 2
		    && len >= ACPI_PLD_REV2_BUFFER_SIZE
		    && (pld->VerticalOffset != UINT16_MAX ||
			pld->HorizontalOffset != UINT16_MAX))
		  {
			sbuf_printf(sb, "\tVertical Offset %d.%dmm\n",
				    pld->VerticalOffset / 10, pld->VerticalOffset % 10);
			sbuf_printf(sb, "\tHorizontal Offset %d.%dmm\n",
				    pld->HorizontalOffset / 10, pld->HorizontalOffset % 10);
		}
		AcpiOsFree(pld);
		return 0;
	}

	return 1;
}
size_t
acpi_add_pld_sysctl(ACPI_HANDLE ah, struct sysctl_ctx_list *ctx,
		    struct sysctl_oid_list *tree, uint8_t *port_pld)
{
	ACPI_BUFFER buf;
	size_t len;
	len = 0;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;

	if (AcpiEvaluateObject(ah, "_PLD", NULL, &buf) == AE_OK &&
	    buf.Pointer != NULL) {
		ACPI_OBJECT *obj;
		unsigned char *resbuf;

		obj = buf.Pointer;

		if (obj->Type == ACPI_TYPE_PACKAGE
		    && obj->Package.Count > 0
		    && obj->Package.Elements[0].Type == ACPI_TYPE_BUFFER) {
			ACPI_OBJECT *obj1;

			obj1 = &obj->Package.Elements[0];
			len = obj1->Buffer.Length;
			resbuf = obj1->Buffer.Pointer;
		} else if (obj->Type == ACPI_TYPE_BUFFER) {
			len = obj->Buffer.Length;
			resbuf = obj->Buffer.Pointer;
		} else {
			goto skip;
		}
		if (len == 0)
			goto skip;
		len = MIN(len, ACPI_PLD_BUFFER_SIZE);
		memcpy(port_pld, resbuf, len);
		SYSCTL_ADD_OPAQUE(
		    ctx, tree, OID_AUTO,
		    "pldraw", CTLFLAG_RD | CTLFLAG_MPSAFE,
		    port_pld, len, "A", "Raw PLD value");
	skip:
		AcpiOsFree(buf.Pointer);
	}

	return (len);
}

static ACPI_STATUS
acpi_find_child_by_adr_cb(ACPI_HANDLE ah, UINT32 lv, void *ctx, void **ahp)
{
	ACPI_DEVICE_INFO *devinfo;
	ACPI_STATUS ret;
	ACPI_INTEGER *adr = ctx;

	ret = AcpiGetObjectInfo(ah, &devinfo);
	if (ACPI_SUCCESS(ret)) {
		if ((devinfo->Valid & ACPI_VALID_ADR) &&
		    (devinfo->Address == *adr)){
			ret = AE_CTRL_TERMINATE;
			*ahp = ah;
		}
		AcpiOsFree(devinfo);
	}

	return (ret);

}

ACPI_STATUS  acpi_find_child_by_adr(ACPI_HANDLE pah, ACPI_HANDLE *cah, ACPI_INTEGER adr)
{
	*cah = NULL;

	return (AcpiWalkNamespace(ACPI_TYPE_DEVICE, pah, 1, acpi_find_child_by_adr_cb, NULL, &adr, cah));
}
