/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "desktop/printer.h"
#include "amiga/plotters.h"
#include "render/font.h"
#include "amiga/gui.h"
#include "utils/nsoption.h"
#include "amiga/print.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "amiga/utf8.h"

#include <proto/utility.h>
#include <proto/iffparse.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/Picasso96API.h>
#include <devices/printer.h>
#include <devices/prtbase.h>

#include <prefs/prefhdr.h>
#include <prefs/printertxt.h>
#include <libraries/gadtools.h>

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/integer.h>
#include <proto/label.h>
#include <proto/chooser.h>
#include <proto/fuelgauge.h>
#include <classes/window.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/layout.h>
#include <gadgets/integer.h>
#include <gadgets/chooser.h>
#include <images/label.h>

#include <reaction/reaction_macros.h>

bool ami_print_begin(struct print_settings *ps);
bool ami_print_next_page(void);
void ami_print_end(void);
bool ami_print_dump(void);
void ami_print_progress(void);
void ami_print_close_device(void);

const struct printer amiprinter = {
	&amiplot,
	ami_print_begin,
	ami_print_next_page,
	ami_print_end,
};

struct ami_printer_info
{
	struct gui_globals *gg;
	struct IODRPTagsReq *PReq;
	struct PrinterData *PD;
	struct PrinterExtendedData *PED;
	struct MsgPort *msgport;
	struct hlcache_handle *c;
	struct print_settings *ps;
	int page;
	int pages;
	Object *gadgets[GID_LAST];
	Object *objects[OID_LAST];
	struct Window *win;
};

enum
{
	PGID_MAIN=0,
	PGID_PRINTER,
	PGID_SCALE,
	PGID_COPIES,
	PGID_PRINT,
	PGID_CANCEL,
	PGID_LAST
};

#define IFFPrefChunkCnt 2
static LONG IFFPrefChunks[] =
{
	ID_PREF, ID_PRHD,
	ID_PREF, ID_PDEV,
};

static struct ami_printer_info ami_print_info;

static CONST_STRPTR gadlab[PGID_LAST];
static STRPTR printers[11];

void ami_print_ui_setup(void)
{
	gadlab[PGID_PRINTER] = (char *)ami_utf8_easy((char *)messages_get("Printer"));
	gadlab[PGID_SCALE] = (char *)ami_utf8_easy((char *)messages_get("Scale"));
	gadlab[PGID_COPIES] = (char *)ami_utf8_easy((char *)messages_get("Copies"));
	gadlab[PGID_PRINT] = (char *)ami_utf8_easy((char *)messages_get("ObjPrint"));
	gadlab[PGID_CANCEL] = (char *)ami_utf8_easy((char *)messages_get("Cancel"));
}

void ami_print_ui_free(void)
{
	int i;

	for(i = 0; i++; i < PGID_LAST)
		if(gadlab[i]) FreeVec((APTR)gadlab[i]);

	for(i = 0; i++; i < 10)
		if(printers[i]) FreeVec(printers[i]);
}

BOOL ami_print_readunit(CONST_STRPTR filename, char name[],
	uint32 namesize, int unitnum)
{
	/* This is a modified version of a function from the OS4 SDK.
	 * The README says "You can use it in your application",
	 * no licence is specified. (c) 1999 Amiga Inc */

	BPTR fp;
	BOOL ok;
	struct IFFHandle *iff;
	struct ContextNode *cn;
	struct PrefHeader phead;
	struct PrinterDeviceUnitPrefs pdev;

	SNPrintf(name,namesize,"Unit %ld",unitnum);
	fp = Open(filename, MODE_OLDFILE);
	if (fp)
	{
		iff = AllocIFF();
		if (iff)
		{
			iff->iff_Stream = fp;
			InitIFFasDOS(iff);

			if (!OpenIFF(iff, IFFF_READ))
			{
				if (!ParseIFF(iff, IFFPARSE_STEP))
				{
					cn = CurrentChunk(iff);
					if (cn->cn_ID == ID_FORM && cn->cn_Type == ID_PREF)
					{
						if (!StopChunks(iff, IFFPrefChunks, IFFPrefChunkCnt))
						{
							ok = TRUE;
							while (ok)
							{
								if (ParseIFF(iff, IFFPARSE_SCAN))
									break;
								cn = CurrentChunk(iff);
								if (cn->cn_Type == ID_PREF)
								{
									switch (cn->cn_ID)
									{
										case ID_PRHD:
											if (ReadChunkBytes(iff, &phead, sizeof(struct PrefHeader)) != sizeof(struct PrefHeader))
											{
												ok = FALSE;
												break;
											}
											if (phead.ph_Version != 0)
											{
												ok = FALSE;
												break;
											}
											break;
										case ID_PDEV:
											if (ReadChunkBytes(iff, &pdev, sizeof(pdev)) == sizeof(pdev))
											{
												if (pdev.pd_UnitName[0])
													strcpy(name,pdev.pd_UnitName);
											}
											break;
										default:
											break;
									}
								}
							}
						}
					}
				}
				CloseIFF(iff);
			}
			FreeIFF(iff);
		}
		Close(fp);
	}
	else return FALSE;

	return TRUE;
}

void ami_print_ui(struct hlcache_handle *c)
{
	char filename[30];
	int i;

	struct ami_print_window *pw = AllocVecTags(sizeof(struct ami_print_window), AVT_ClearWithValue, 0, TAG_DONE);

	pw->c = c;

	printers[0] = AllocVecTags(50, AVT_ClearWithValue, 0, TAG_DONE);
	ami_print_readunit("ENV:Sys/printer.prefs", printers[0], 50, 0);

	strcpy(filename,"ENV:Sys/printerN.prefs");
	for (i = 1; i < 10; i++)
	{
		filename[15] = '0' + i;
		printers[i] = AllocVecTagList(50, NULL);
		if(!ami_print_readunit(filename, printers[i], 50, i))
		{
			FreeVec(printers[i]);
			printers[i] = NULL;
			break;
		}

	}

	ami_print_ui_setup();

	pw->objects[OID_MAIN] = WindowObject,
      	    WA_ScreenTitle, nsscreentitle,
           	WA_Title, gadlab[PGID_PRINT],
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, TRUE,
           	WA_SizeGadget, FALSE,
			WA_PubScreen, scrn,
			WINDOW_SharedPort, sport,
			WINDOW_UserData, pw,
			WINDOW_IconifyGadget, FALSE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, pw->gadgets[PGID_MAIN] = VGroupObject,
				LAYOUT_AddChild, ChooserObject,
					GA_ID, PGID_PRINTER,
					GA_RelVerify, TRUE,
					GA_TabCycle, TRUE,
					CHOOSER_LabelArray, printers,
					CHOOSER_Selected, nsoption_int(printer_unit),
				ChooserEnd,
				CHILD_Label, LabelObject,
					LABEL_Text, gadlab[PGID_PRINTER],
				LabelEnd,
				LAYOUT_AddChild, IntegerObject,
					GA_ID, PGID_COPIES,
					GA_RelVerify, TRUE,
					GA_TabCycle, TRUE,
					INTEGER_Number, 1,
					INTEGER_Minimum, 1,
					INTEGER_Maximum, 100,
					INTEGER_Arrows, TRUE,
				IntegerEnd,
				CHILD_Label, LabelObject,
					LABEL_Text, gadlab[PGID_COPIES],
				LabelEnd,
				LAYOUT_AddChild, HGroupObject,
					LAYOUT_LabelColumn, PLACETEXT_RIGHT,
					LAYOUT_AddChild, pw->gadgets[PGID_SCALE] = IntegerObject,
						GA_ID, PGID_SCALE,
						GA_RelVerify, TRUE,
						GA_TabCycle, TRUE,
						INTEGER_Number, nsoption_int(print_scale),
						INTEGER_Minimum, 0,
						INTEGER_Maximum, 100,
						INTEGER_Arrows, TRUE,
					IntegerEnd,
					CHILD_WeightedWidth, 0,
					CHILD_Label, LabelObject,
						LABEL_Text, "%",
					LabelEnd,
				LayoutEnd,
				CHILD_Label, LabelObject,
					LABEL_Text, gadlab[PGID_SCALE],
				LabelEnd,
				LAYOUT_AddChild, HGroupObject,
					LAYOUT_AddChild, pw->gadgets[PGID_PRINT] = ButtonObject,
						GA_ID, PGID_PRINT,
						GA_RelVerify,TRUE,
						GA_Text, gadlab[PGID_PRINT],
						GA_TabCycle,TRUE,
					ButtonEnd,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, pw->gadgets[GID_CANCEL] = ButtonObject,
						GA_ID, PGID_CANCEL,
						GA_RelVerify, TRUE,
						GA_Text, gadlab[PGID_CANCEL],
						GA_TabCycle,TRUE,
					ButtonEnd,
				LayoutEnd,
				CHILD_WeightedHeight,0,
			EndGroup,
		EndWindow;

	pw->win = (struct Window *)RA_OpenWindow(pw->objects[OID_MAIN]);

	pw->node = AddObject(window_list, AMINS_PRINTWINDOW);
	pw->node->objstruct = pw;
}

void ami_print_close(struct ami_print_window *pw)
{
	DisposeObject(pw->objects[OID_MAIN]);
	DelObject(pw->node);

	ami_print_ui_free();
}

BOOL ami_print_event(struct ami_print_window *pw)
{
	/* return TRUE if window destroyed */
	ULONG class,result;
	uint16 code;
	struct hlcache_handle *c;
	int copies;
	int print_scale;
	int printer_unit;

	while((result = RA_HandleInput(pw->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
	{
       	switch(result & WMHI_CLASSMASK) // class
   		{
			case WMHI_GADGETUP:
				switch(result & WMHI_GADGETMASK)
				{
					case PGID_PRINT:
						GetAttr(INTEGER_Number, pw->gadgets[PGID_SCALE],
							(ULONG *)&print_scale);
						GetAttr(INTEGER_Number, pw->gadgets[PGID_COPIES],
							(ULONG *)&copies);
						GetAttr(CHOOSER_Selected, pw->gadgets[PGID_PRINTER],
							(ULONG *)&printer_unit);

						nsoption_set_int(print_scale, print_scale);
						nsoption_set_int(printer_unit, printer_unit);

						c = pw->c;
						ami_print_close(pw);
						ami_print(c, copies);
						return TRUE;
					break;

					case PGID_CANCEL:
						ami_print_close(pw);
						return TRUE;
					break;
				}
			break;

			case WMHI_CLOSEWINDOW:
				ami_print_close(pw);
				return TRUE;
			break;
		}
	}
	return FALSE;
}

void ami_print(struct hlcache_handle *c, int copies)
{
	double height;
	float scale = nsoption_int(print_scale) / 100.0;

	if(ami_print_info.msgport == NULL)
		ami_print_init();

	if(!(ami_print_info.PReq =
			(struct IODRPTagsReq *)AllocSysObjectTags(ASOT_IOREQUEST,
				ASOIOR_Size, sizeof(struct IODRPTagsReq),
				ASOIOR_ReplyPort, ami_print_info.msgport,
				ASO_NoTrack, FALSE,
				TAG_DONE))) return;

	if(OpenDevice("printer.device", nsoption_int(printer_unit),
			(struct IORequest *)ami_print_info.PReq, 0))
	{
		warn_user("CompError","printer.device");
		return;
	}

	ami_print_info.PD = (struct PrinterData *)ami_print_info.PReq->io_Device;
	ami_print_info.PED = &ami_print_info.PD->pd_SegmentData->ps_PED;

	ami_print_info.ps = print_make_settings(PRINT_DEFAULT, nsurl_access(hlcache_handle_get_url(c)), &nsfont);
	ami_print_info.ps->page_width = ami_print_info.PED->ped_MaxXDots;
	ami_print_info.ps->page_height = ami_print_info.PED->ped_MaxYDots;
	ami_print_info.ps->scale = scale;

	if(!print_set_up(c, &amiprinter, ami_print_info.ps, &height))
	{
		warn_user("PrintError","print_set_up() returned false");
		ami_print_close_device();
		return;
	}

	height *= ami_print_info.ps->scale;
	ami_print_info.pages = height / ami_print_info.ps->page_height;
	ami_print_info.c = c;

	ami_print_progress();

	while(ami_print_cont()); /* remove while() for async printing */
}

bool ami_print_cont(void)
{
	bool ret = false;

	if(ami_print_info.page <= ami_print_info.pages)
	{
		glob = ami_print_info.gg;
		print_draw_next_page(&amiprinter, ami_print_info.ps);
		ami_print_dump();
		glob = &browserglob;
		ret = true;
	}
	else 
	{
		print_cleanup(ami_print_info.c, &amiprinter, ami_print_info.ps);
		ret = false;
	}

	return ret;
}

struct MsgPort *ami_print_init(void)
{
	ami_print_info.msgport = AllocSysObjectTags(ASOT_PORT,
				ASO_NoTrack,FALSE,
				TAG_DONE);

	return ami_print_info.msgport;
}

void ami_print_free(void)
{
	FreeSysObject(ASOT_PORT, ami_print_info.msgport);
	ami_print_info.msgport = NULL;
}

struct MsgPort *ami_print_get_msgport(void)
{
	return ami_print_info.msgport;
}

bool ami_print_begin(struct print_settings *ps)
{
	ami_print_info.gg = AllocVecTags(sizeof(struct gui_globals), AVT_ClearWithValue, 0, TAG_DONE);
	if(!ami_print_info.gg) return false;

	ami_init_layers(ami_print_info.gg,
				ami_print_info.PED->ped_MaxXDots,
				ami_print_info.PED->ped_MaxYDots);

	ami_print_info.page = 0;

	return true;
}

bool ami_print_next_page(void)
{
	ami_print_info.page++;

	RefreshSetGadgetAttrs((struct Gadget *)ami_print_info.gadgets[GID_STATUS],
				ami_print_info.win, NULL,
				FUELGAUGE_Level, ami_print_info.page,
				TAG_DONE);
	return true;
}

void ami_print_end(void)
{
	ami_free_layers(ami_print_info.gg);
	FreeVec(ami_print_info.gg);
	DisposeObject(ami_print_info.objects[OID_MAIN]);
	glob = &browserglob;

	ami_print_close_device();
	ami_print_free();
}

void ami_print_close_device(void)
{
	CloseDevice((struct IORequest *)ami_print_info.PReq);
	FreeSysObject(ASOT_IOREQUEST,ami_print_info.PReq);
}

bool ami_print_dump(void)
{
	ami_print_info.PReq->io_Command = PRD_DUMPRPORT;
	ami_print_info.PReq->io_Flags = 0;
	ami_print_info.PReq->io_Error = 0;
	ami_print_info.PReq->io_RastPort = ami_print_info.gg->rp;
	ami_print_info.PReq->io_ColorMap = NULL;
	ami_print_info.PReq->io_Modes = 0;
	ami_print_info.PReq->io_SrcX = 0;
	ami_print_info.PReq->io_SrcY = 0;
	ami_print_info.PReq->io_SrcWidth = ami_print_info.PED->ped_MaxXDots;
	ami_print_info.PReq->io_SrcHeight = ami_print_info.PED->ped_MaxYDots;
	ami_print_info.PReq->io_DestCols = ami_print_info.PED->ped_MaxXDots;
	ami_print_info.PReq->io_DestRows = ami_print_info.PED->ped_MaxYDots;
	ami_print_info.PReq->io_Special = 0;

	DoIO((struct IORequest *)ami_print_info.PReq); /* SendIO for async printing */

	return true;
}

void ami_print_progress(void)
{
	ami_print_info.objects[OID_MAIN] = WindowObject,
      	    WA_ScreenTitle,nsscreentitle,
           	WA_Title, messages_get("Printing"),
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
           	WA_SizeGadget, TRUE,
			WA_PubScreen,scrn,
			//WINDOW_SharedPort,sport,
			WINDOW_UserData, &ami_print_info,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_LockHeight,TRUE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, ami_print_info.gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, ami_print_info.gadgets[GID_STATUS] = FuelGaugeObject,
					GA_ID,GID_STATUS,
					FUELGAUGE_Min,0,
					FUELGAUGE_Max,ami_print_info.pages,
					FUELGAUGE_Level,0,
					FUELGAUGE_Ticks,11,
					FUELGAUGE_ShortTicks,TRUE,
					FUELGAUGE_Percent,TRUE,
					FUELGAUGE_Justification,FGJ_CENTER,
				FuelGaugeEnd,
				CHILD_NominalSize,TRUE,
				CHILD_WeightedHeight,0,
/*
				LAYOUT_AddChild, ami_print_info.gadgets[GID_CANCEL] = ButtonObject,
					GA_ID,GID_CANCEL,
					GA_Disabled,TRUE,
					GA_RelVerify,TRUE,
					GA_Text,messages_get("Abort"),
					GA_TabCycle,TRUE,
				ButtonEnd,
*/
			EndGroup,
		EndWindow;

	ami_print_info.win = (struct Window *)RA_OpenWindow(ami_print_info.objects[OID_MAIN]);
}
