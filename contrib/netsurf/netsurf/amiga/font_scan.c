/*
 * Copyright 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/** \file
 * Font glyph scanner for Unicode substitutions.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/fuelgauge.h>
#include <classes/window.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/layout.h>

#include <reaction/reaction_macros.h>

#include "amiga/font_scan.h"
#include "amiga/gui.h"
#include "amiga/object.h"
#include "amiga/utf8.h"

#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"

enum {
	FS_OID_MAIN = 0,
	FS_GID_MAIN,
	FS_GID_FONTS,
	FS_GID_GLYPHS,
	FS_GID_LAST
};

struct ami_font_scan_window {
	struct Window *win;
	Object *objects[FS_GID_LAST];
	char *title;
	char *glyphtext;
};

/**
 * Lookup a font that contains a UTF-16 codepoint
 *
 * \param  code           UTF-16 codepoint to lookup
 * \param  glypharray     an array of 0xffff lwc_string pointers
 * \return font name or NULL
 */
const char *ami_font_scan_lookup(uint16 *code, lwc_string **glypharray)
{
	if(*code >= 0xd800 && *code <= 0xdbff) {
		/* This is a multi-byte character, we don't support falback for these yet. */
		return NULL;
	}

	if(glypharray[*code] == NULL) return NULL;
		else return lwc_string_data(glypharray[*code]);
}

/**
 * Open GUI to show font scanning progress
 *
 * \param fonts  number of fonts that are being scanned
 * \return pointer to a struct ami_font_scan_window
 */
struct ami_font_scan_window *ami_font_scan_gui_open(int32 fonts)
{
	struct ami_font_scan_window *fsw =
		AllocVecTagList(sizeof(struct ami_font_scan_window), NULL);

	if(fsw == NULL) return NULL;

	fsw->title = ami_utf8_easy(messages_get("FontScanning"));
	fsw->glyphtext = ami_utf8_easy(messages_get("FontGlyphs"));

	fsw->objects[FS_OID_MAIN] = WindowObject,
      	    WA_ScreenTitle, nsscreentitle,
           	WA_Title, fsw->title,
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
           	WA_SizeGadget, TRUE,
			WA_PubScreen, scrn,
			WA_BusyPointer, TRUE,
			WA_Width, 400,
			WINDOW_UserData, fsw,
			WINDOW_IconifyGadget, FALSE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
			WINDOW_LockHeight, TRUE,
           	WINDOW_ParentGroup, fsw->objects[FS_GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, fsw->objects[FS_GID_FONTS] = FuelGaugeObject,
					GA_ID, FS_GID_FONTS,
					GA_Text, fsw->title,
					FUELGAUGE_Min, 0,
					FUELGAUGE_Max, fonts,
					FUELGAUGE_Level, 0,
					FUELGAUGE_Ticks, 11,
					FUELGAUGE_ShortTicks, TRUE,
					FUELGAUGE_Percent, FALSE,
					FUELGAUGE_Justification, FGJ_CENTER,
				FuelGaugeEnd,
				CHILD_NominalSize, TRUE,
				CHILD_WeightedHeight, 0,
				LAYOUT_AddChild, fsw->objects[FS_GID_GLYPHS] = FuelGaugeObject,
					GA_ID, FS_GID_GLYPHS,
					//GA_Text, "Glyphs",
					FUELGAUGE_Min, 0x0000,
					FUELGAUGE_Max, 0xffff,
					FUELGAUGE_Level, 0,
					FUELGAUGE_Ticks,11,
					FUELGAUGE_ShortTicks, TRUE,
					FUELGAUGE_Percent, FALSE,
					FUELGAUGE_Justification, FGJ_CENTER,
				FuelGaugeEnd,
				CHILD_NominalSize, TRUE,
				CHILD_WeightedHeight, 0,
			EndGroup,
		EndWindow;

	fsw->win = (struct Window *)RA_OpenWindow(fsw->objects[FS_OID_MAIN]);

	return fsw;
}

/**
 * Update GUI showing font scanning progress
 *
 * \param win       pointer to a struct ami_font_scan_window
 * \param font      current font being scanned
 * \param font_num  font number being scanned
 * \param glyphs    number of unique glyphs found
 */
void ami_font_scan_gui_update(struct ami_font_scan_window *fsw, const char *font,
			ULONG font_num, ULONG glyphs)
{
	ULONG va[2];

	if(fsw) {
		RefreshSetGadgetAttrs((struct Gadget *)fsw->objects[FS_GID_FONTS],
						fsw->win, NULL,
						FUELGAUGE_Level,   font_num,
						GA_Text,           font,
						TAG_DONE);

		va[0] = glyphs;
		va[1] = 0;

		RefreshSetGadgetAttrs((struct Gadget *)fsw->objects[FS_GID_GLYPHS],
						fsw->win, NULL,
						GA_Text,           fsw->glyphtext,
						FUELGAUGE_VarArgs, va,
						FUELGAUGE_Level,   glyphs,
						TAG_DONE);
	} else {
		printf("Found %ld glyphs\n", glyphs);
		printf("Scanning font #%ld (%s)...\n", font_num, font);
	}
}

/**
 * Close GUI showing font scanning progress
 *
 * \param fsw pointer to a struct ami_font_scan_window
 */
void ami_font_scan_gui_close(struct ami_font_scan_window *fsw)
{
	if(fsw) {
		DisposeObject(fsw->objects[FS_OID_MAIN]);
		ami_utf8_free(fsw->title);
		FreeVec(fsw);
	}
}

/**
 * Scan a font for glyphs not present in glypharray.
 *
 * \param  fontname       font to scan
 * \param  glypharray     an array of 0xffff lwc_string pointers
 * \return number of new glyphs found
 */
ULONG ami_font_scan_font(const char *fontname, lwc_string **glypharray)
{
	struct OutlineFont *ofont;
	struct MinList *widthlist;
	struct GlyphWidthEntry *gwnode;
	ULONG foundglyphs = 0;
	ULONG serif = 0;
	lwc_error lerror;
	ULONG unicoderanges = 0;

	ofont = OpenOutlineFont(fontname, NULL, OFF_OPEN);

	if(!ofont) return 0;

	if(ESetInfo(&ofont->olf_EEngine,
		OT_PointHeight, 10 * (1 << 16),
		OT_GlyphCode, 0x0000,
		OT_GlyphCode2, 0xffff,
		TAG_END) == OTERR_Success)
	{
		if(EObtainInfo(&ofont->olf_EEngine,
			OT_WidthList, &widthlist,
			TAG_END) == 0)
		{
			gwnode = (struct GlyphWidthEntry *)GetHead((struct List *)widthlist);
			do {
				if(gwnode && (glypharray[gwnode->gwe_Code] == NULL)) {
					lerror = lwc_intern_string(fontname, strlen(fontname) - 5, &glypharray[gwnode->gwe_Code]);
					if(lerror != lwc_error_ok) continue;
					foundglyphs++;
				}
			} while(gwnode = (struct GlyphWidthEntry *)GetSucc((struct Node *)gwnode));
			EReleaseInfo(&ofont->olf_EEngine,
				OT_WidthList, widthlist,
				TAG_END);
		}
	}

	if(EObtainInfo(&ofont->olf_EEngine, OT_UnicodeRanges, &unicoderanges, TAG_END) == 0) {
		if(unicoderanges & UCR_SURROGATES) LOG(("%s supports UTF-16 surrogates", fontname));
		EReleaseInfo(&ofont->olf_EEngine,
			OT_UnicodeRanges, unicoderanges,
			TAG_END);
	}
		
	CloseOutlineFont(ofont, NULL);

	return foundglyphs;
}

/**
 * Scan all fonts for glyphs.
 *
 * \param list min list
 * \param win scan window
 * \param glypharray an array of 0xffff lwc_string pointers
 * \return number of glyphs found
 */
ULONG ami_font_scan_fonts(struct MinList *list,
		struct ami_font_scan_window *win, lwc_string **glypharray)
{
	ULONG found, total = 0, font_num = 0;
	struct nsObject *node;
	struct nsObject *nnode;

	if(IsMinListEmpty(list)) return 0;

	node = (struct nsObject *)GetHead((struct List *)list);

	do {
		nnode = (struct nsObject *)GetSucc((struct Node *)node);
		ami_font_scan_gui_update(win, node->dtz_Node.ln_Name, font_num, total);
		LOG(("Scanning %s", node->dtz_Node.ln_Name));
		found = ami_font_scan_font(node->dtz_Node.ln_Name, glypharray);
		total += found;
		LOG(("Found %ld new glyphs (total = %ld)", found, total));
		font_num++;
	} while(node = nnode);

	return total;
}

/**
 * Add OS fonts to a list.
 *
 * \param  list   list to add font names to
 * \return number of fonts found
 */
ULONG ami_font_scan_list(struct MinList *list)
{
	int afShortage, afSize = 100;
	struct AvailFontsHeader *afh;
	struct AvailFonts *af;
	ULONG found = 0;
	struct nsObject *node;

	do {
		if(afh = (struct AvailFontsHeader *)AllocVecTagList(afSize, NULL)) {
			if(afShortage = AvailFonts(afh, afSize, AFF_DISK | AFF_OTAG | AFF_SCALED)) {
				FreeVec(afh);
				afSize += afShortage;
			}
		} else {
			/* out of memory, bail out */
			return 0;
		}
	} while (afShortage);

	if(afh) {
		af = (struct AvailFonts *)&(afh[1]);

		for(int i = 0; i < afh->afh_NumEntries; i++) {
			if(af[i].af_Attr.ta_Style == FS_NORMAL) {
				if(af[i].af_Attr.ta_Name != NULL) {
					node = (struct nsObject *)FindIName((struct List *)list,
								af[i].af_Attr.ta_Name);
					if(node == NULL) {
						node = AddObject(list, AMINS_UNKNOWN);
						if(node) {
							node->dtz_Node.ln_Name = strdup(af[i].af_Attr.ta_Name);
							found++;
							LOG(("Added %s", af[i].af_Attr.ta_Name));
						}
					}
				}
			}
		}
		FreeVec(afh);
	} else {
		return 0;
	}
	return found;
}

/**
 * Load a font glyph cache
 *
 * \param  filename       name of cache file to load
 * \param  glypharray     an array of 0xffff lwc_string pointers
 * \return number of glyphs loaded
 */
ULONG ami_font_scan_load(const char *filename, lwc_string **glypharray)
{
	ULONG found = 0;
	BPTR fh = 0;
	lwc_error lerror;
	char buffer[256];
	struct RDArgs *rargs = NULL;
	STRPTR template = "CODE/A,FONT/A";
	long rarray[] = {0,0};

	enum {
		A_CODE = 0,
		A_FONT
	};

	rargs = AllocDosObjectTags(DOS_RDARGS, TAG_DONE);

	if(fh = FOpen(filename, MODE_OLDFILE, 0)) {
		LOG(("Loading font glyph cache from %s", filename));

		while(FGets(fh, (UBYTE *)&buffer, 256) != 0)
		{
			rargs->RDA_Source.CS_Buffer = (char *)&buffer;
			rargs->RDA_Source.CS_Length = 256;
			rargs->RDA_Source.CS_CurChr = 0;

			rargs->RDA_DAList = NULL;
			rargs->RDA_Buffer = NULL;
			rargs->RDA_BufSiz = 0;
			rargs->RDA_ExtHelp = NULL;
			rargs->RDA_Flags = 0;

			if(ReadArgs(template, rarray, rargs))
			{
				lerror = lwc_intern_string((const char *)rarray[A_FONT],
							strlen((const char *)rarray[A_FONT]),
							&glypharray[strtoul((const char *)rarray[A_CODE], NULL, 0)]);
				if(lerror != lwc_error_ok) continue;
				found++;
			}
		}
		FClose(fh);
	}

	return found;
}

/**
 * Save a font glyph cache
 *
 * \param  filename       name of cache file to save
 * \param  glypharray     an array of 0xffff lwc_string pointers
 */
void ami_font_scan_save(const char *filename, lwc_string **glypharray)
{
	ULONG i;
	BPTR fh = 0;

	if(fh = FOpen(filename, MODE_NEWFILE, 0)) {
		LOG(("Writing font glyph cache to %s", filename));
		FPrintf(fh, "; This file is auto-generated. To re-create the cache, delete this file.\n");
		FPrintf(fh, "; This file is parsed using ReadArgs() with the following template:\n");
		FPrintf(fh, "; CODE/A,FONT/A\n;\n");

		for(i=0x0000; i<=0xffff; i++)
		{
			if(glypharray[i]) {
				FPrintf(fh, "0x%04lx \"%s\"\n", i, lwc_string_data(glypharray[i]));
			}
		}
		FClose(fh);
	}
}

/**
 * Finalise the font glyph cache.
 *
 * \param  glypharray     an array of 0xffff lwc_string pointers to free
 */
void ami_font_scan_fini(lwc_string **glypharray)
{
	ULONG i;

	for(i=0x0000; i<=0xffff; i++)
	{
		if(glypharray[i]) {
			lwc_string_unref(glypharray[i]);
			glypharray[i] = NULL;
		}
	}
}

/**
 * Initialise the font glyph cache.
 * Reads an existing file or, if not present, generates a new cache.
 *
 * \param  filename       cache file to attempt to read
 * \param  entries        number of entries in list
 * \param  force_scan     force re-creation of cache
 * \param  glypharray     an array of 0xffff lwc_string pointers
 */
void ami_font_scan_init(const char *filename, bool force_scan, bool save,
		lwc_string **glypharray)
{
	ULONG i, found = 0, entries = 0;
	struct MinList *list;
	struct nsObject *node;
	char *unicode_font, *csv;
	struct ami_font_scan_window *win = NULL;

	/* Ensure array zeroed */
	for(i=0x0000; i<=0xffff; i++)
		glypharray[i] = NULL;

	if(force_scan == false)
		found = ami_font_scan_load(filename, glypharray);

	if(found == 0) {
		if(list = NewObjList()) {

			/* add preferred fonts list */
			if(nsoption_charp(font_unicode) &&
					(csv = strdup(nsoption_charp(font_unicode))))
			{
				char *p;

				while(p = strsep(&csv, ",")) {
					asprintf(&unicode_font, "%s.font", p);
					if(unicode_font != NULL) {
						node = AddObject(list, AMINS_UNKNOWN);
						if(node) node->dtz_Node.ln_Name = unicode_font;
						entries++;
					}
				}
				free(csv);
			}

			if(nsoption_bool(font_unicode_only) == false)
				entries += ami_font_scan_list(list);

			LOG(("Found %ld fonts", entries));

			win = ami_font_scan_gui_open(entries);
			found = ami_font_scan_fonts(list, win, glypharray);
			ami_font_scan_gui_close(win);

			FreeObjList(list);

			if(save == true)
				ami_font_scan_save(filename, glypharray);
		}
	}

	LOG(("Initialised with %ld glyphs", found));
}


