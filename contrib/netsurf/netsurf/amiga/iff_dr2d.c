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

#ifdef WITH_NS_SVG

#include <stdio.h>
#include <svgtiny.h>
#include <proto/exec.h>
#include <string.h>
#include "iff_dr2d.h"
#include <proto/dos.h>

struct ColorRegister cm[1000];
ULONG numcols;

ULONG findcolour(ULONG newcol)
{
	ULONG i;
	ULONG colr = 0xFFFFFFFF;
	UBYTE red,grn,blu;

	red = svgtiny_RED(newcol);
	grn = svgtiny_GREEN(newcol);
	blu = svgtiny_BLUE(newcol);

	for(i=0;i<numcols;i++)
	{
		if((cm[i].red == red) && (cm[i].green == grn) && (cm[i].blue == blu))
			colr = i;
	}

	return colr;
}

void addcolour(ULONG newcol)
{
	ULONG colr;
	UBYTE red,grn,blu;

	colr = findcolour(newcol);

	if(colr == 0xFFFFFFFF)
	{
		cm[numcols].red = svgtiny_RED(newcol);
		cm[numcols].green = svgtiny_GREEN(newcol);
		cm[numcols].blue = svgtiny_BLUE(newcol);

		numcols++;
	}
}

bool ami_svg_to_dr2d(struct IFFHandle *iffh, const char *buffer,
		uint32 size, const char *url)
{
	struct svgtiny_diagram *diagram;
	svgtiny_code code;
	unsigned int i;
	unsigned int j;
	BOOL fons_written = FALSE;
	struct fons_struct *fons;
	struct stxt_struct *stxt;
	struct attr_struct *attr;

	/* create svgtiny object */
	diagram = svgtiny_create();
	if (!diagram) {
		fprintf(stderr, "svgtiny_create failed\n");
		return 1;
	}

	/* parse */
	code = svgtiny_parse(diagram, buffer, size, url, 1000, 1000);
	if (code != svgtiny_OK) {
		fprintf(stderr, "svgtiny_parse failed: ");
		switch (code) {
		case svgtiny_OUT_OF_MEMORY:
			fprintf(stderr, "svgtiny_OUT_OF_MEMORY");
			break;
		case svgtiny_LIBDOM_ERROR:
			fprintf(stderr, "svgtiny_LIBDOM_ERROR");
			break;
		case svgtiny_NOT_SVG:
			fprintf(stderr, "svgtiny_NOT_SVG");
			break;
		case svgtiny_SVG_ERROR:
			fprintf(stderr, "svgtiny_SVG_ERROR: line %i: %s",
					diagram->error_line,
					diagram->error_message);
			break;
		default:
			fprintf(stderr, "unknown svgtiny_code %i", code);
			break;
		}
		fprintf(stderr, "\n");
	}

	if(!(PushChunk(iffh,ID_DR2D,ID_FORM,IFFSIZE_UNKNOWN)))
	{
		if(!(PushChunk(iffh,0,ID_NAME,IFFSIZE_UNKNOWN)))
		{
			WriteChunkBytes(iffh,url,strlen(url));
			PopChunk(iffh);
		}

		if(!(PushChunk(iffh,0,ID_ANNO,18)))
		{
			WriteChunkBytes(iffh,"Created by NetSurf",18);
			PopChunk(iffh);
		}

		if(!(PushChunk(iffh,0,ID_DRHD,16)))
		{
			struct drhd_struct drhd;
			drhd.XLeft = (float) 0.0;
			drhd.YTop = (float) 0.0;
			drhd.XRight = (float) diagram->width;
			drhd.YBot = (float) diagram->height;

			WriteChunkBytes(iffh,&drhd,16);
			PopChunk(iffh);
		}

		if(!(PushChunk(iffh,0,ID_DASH,IFFSIZE_UNKNOWN)))
		{
			struct dash_struct dash;
			dash.DashID = 1;
			dash.NumDashes = 0;

			WriteChunkBytes(iffh,&dash,sizeof(struct dash_struct));
			PopChunk(iffh);
		}

		if(!(PushChunk(iffh,0,ID_CMAP,IFFSIZE_UNKNOWN)))
		{
			for (i = 0; i != diagram->shape_count; i++) {
				if(diagram->shape[i].fill != svgtiny_TRANSPARENT)
				{
					addcolour(diagram->shape[i].fill);
				}

				if(diagram->shape[i].stroke != svgtiny_TRANSPARENT)
				{
					addcolour(diagram->shape[i].stroke);
				}
			}

			WriteChunkBytes(iffh,cm,3*numcols);
			PopChunk(iffh);
		}

	for (i = 0; i != diagram->shape_count; i++) {
		attr = AllocVecTagList(sizeof(struct attr_struct), NULL);
		if (diagram->shape[i].fill == svgtiny_TRANSPARENT)
			attr->FillType = FT_NONE;
		else
		{
			attr->FillType = FT_COLOR;
			attr->FillValue = findcolour(diagram->shape[i].fill);
		}
		if (diagram->shape[i].stroke == svgtiny_TRANSPARENT)
			attr->DashPattern = 0;
		else
		{
			attr->DashPattern = 1;
			attr->EdgeValue = findcolour(diagram->shape[i].stroke);
		}
		attr->EdgeThick = (float) diagram->shape[i].stroke_width;

		if(!(PushChunk(iffh,0,ID_ATTR,IFFSIZE_UNKNOWN)))
		{
			WriteChunkBytes(iffh,attr,14);
			PopChunk(iffh);
		}
		FreeVec(attr);

		if (diagram->shape[i].path) {
			union {
				float PolyPoints;
				ULONG val;
			} poly[(diagram->shape[i].path_length)*2];

			USHORT NumPoints;
			long type;
			float curx,cury;

			curx = 0.0;
			cury = 0.0;
			NumPoints = 0;
			type = ID_OPLY;

			for (j = 0;
					j != diagram->shape[i].path_length; ) {
				switch ((int) diagram->shape[i].path[j]) {
				case svgtiny_PATH_MOVE:
					if(j != 0)
					{
						poly[NumPoints*2].val = INDICATOR;
						poly[(NumPoints*2)+1].val = IND_MOVETO;
						NumPoints++;
					}
					poly[(NumPoints*2)].PolyPoints = diagram->shape[i].path[j + 1];
						poly[(NumPoints*2)+1].PolyPoints = diagram->shape[i].path[j + 2];
						NumPoints++;
						curx = (float) diagram->shape[i].path[j + 1];
						cury = (float) diagram->shape[i].path[j + 2];

						j += 3;
					break;
					case svgtiny_PATH_CLOSE:
						type = ID_CPLY;
						j += 1;
					break;
					case svgtiny_PATH_LINE:
						poly[(NumPoints*2)].PolyPoints = (float) diagram->shape[i].path[j + 1];
						poly[(NumPoints*2)+1].PolyPoints = (float) diagram->shape[i].path[j + 2];
						NumPoints++;
						curx = (float) diagram->shape[i].path[j + 1];
						cury = (float) diagram->shape[i].path[j + 2];
						j += 3;
					break;
					case svgtiny_PATH_BEZIER:
						poly[NumPoints*2].val = INDICATOR;
						poly[(NumPoints*2)+1].val = IND_CURVE;
						NumPoints++;
						poly[(NumPoints*2)].PolyPoints = curx;
						poly[(NumPoints*2)+1].PolyPoints = cury;
						NumPoints++;
						poly[(NumPoints*2)].PolyPoints = (float) diagram->shape[i].path[j + 1];
						poly[(NumPoints*2)+1].PolyPoints = (float) diagram->shape[i].path[j + 2];
						NumPoints++;
						poly[(NumPoints*2)].PolyPoints = (float) diagram->shape[i].path[j + 3];
						poly[(NumPoints*2)+1].PolyPoints = (float) diagram->shape[i].path[j + 4];
						NumPoints++;
						poly[(NumPoints*2)].PolyPoints = (float) diagram->shape[i].path[j + 5];
						poly[(NumPoints*2)+1].PolyPoints = (float) diagram->shape[i].path[j + 6];
						curx = poly[(NumPoints*2)].PolyPoints;
						cury = poly[(NumPoints*2)+1].PolyPoints;
						NumPoints++;
						j += 7;
						break;
					default:
						printf("error\n");
						j += 1;
					}
				}
				if(!(PushChunk(iffh,0,type,IFFSIZE_UNKNOWN)))
				{
					WriteChunkBytes(iffh,&NumPoints,sizeof(USHORT));
					WriteChunkBytes(iffh,poly,NumPoints*2*4);
					PopChunk(iffh);
				}
			} else if (diagram->shape[i].text) {
				stxt = AllocVecTagList(sizeof(struct stxt_struct), NULL);
				stxt->BaseX = diagram->shape[i].text_x;
				stxt->BaseY = diagram->shape[i].text_y;
				stxt->NumChars = strlen(diagram->shape[i].text);
				if(!fons_written)
				{
					fons = AllocVecTagList(sizeof(struct fons_struct), NULL);
					if(!(PushChunk(iffh,0,ID_FONS,IFFSIZE_UNKNOWN)))
					{
						WriteChunkBytes(iffh,fons,sizeof(struct fons_struct));
						WriteChunkBytes(iffh,"Topaz",5);
						PopChunk(iffh);
					}
					FreeVec(fons);
					fons_written = TRUE;
				}

				if(!(PushChunk(iffh,0,ID_STXT,IFFSIZE_UNKNOWN)))
				{
					WriteChunkBytes(iffh,stxt,26);
					WriteChunkBytes(iffh,diagram->shape[i].text,strlen(diagram->shape[i].text));
					PopChunk(iffh);
				}
				FreeVec(stxt);
			}
		}

		PopChunk(iffh);
	}

	svgtiny_free(diagram);

	return 0;
}

#ifndef AMIGA_DR2D_STANDALONE
bool ami_save_svg(struct hlcache_handle *c,char *filename)
{
	struct IFFHandle *iffh;
	const char *source_data;
	ULONG source_size;

	if(!ami_download_check_overwrite(filename, NULL, 0)) return false;

	if(iffh = AllocIFF())
	{
		if(iffh->iff_Stream = Open(filename,MODE_NEWFILE))
		{
			InitIFFasDOS(iffh);
		}
		else return false;
	}

	if((OpenIFF(iffh,IFFF_WRITE))) return false;

	if((source_data = content_get_source_data(c, &source_size)))
		ami_svg_to_dr2d(iffh, source_data, source_size, nsurl_access(hlcache_handle_get_url(c)));

	if(iffh) CloseIFF(iffh);
	if(iffh->iff_Stream) Close((BPTR)iffh->iff_Stream);
	if(iffh) FreeIFF(iffh);

}
#else
/*
 * This code can be compiled as a standalone program for testing etc.
 * Use something like the following line:
 * gcc -o svg2dr2d iff_dr2d.c -lauto -lsvgtiny -lpthread -lz -use-dynld
 * -DWITH_NS_SVG -DAMIGA_DR2D_STANDALONE -D__USE_INLINE__
 */
const char USED ver[] = "\0$VER: svg2dr2d 1.1 (18.05.2009)\0";

int main(int argc, char **argv)
{
	BPTR fh = 0;
	char *buffer;
	struct IFFHandle *iffh = NULL;
	int64 size;
	LONG rarray[] = {0,0};
	struct RDArgs *args;
	STRPTR template = "SVG=INPUT/A,DR2D=OUTPUT/A";
	enum
	{
		A_SVG,
		A_DR2D
	};

	args = ReadArgs(template,rarray,NULL);

	if(!args)
	{
		printf("Required argument missing\n");
		return 20;
	}

	if(fh = Open((char *)rarray[A_SVG],MODE_OLDFILE))
	{
		size = GetFileSize(fh);	

		buffer = AllocVecTagList((uint32)size, NULL);

		Read(fh,buffer,(uint32)size);
		Close(fh);
	}
	else
	{
		printf("Unable to open file\n");
		return 20;
	}

	if(iffh = AllocIFF())
	{
		if(iffh->iff_Stream = Open((char *)rarray[A_DR2D],MODE_NEWFILE))
		{
			InitIFFasDOS(iffh);
		}
		else return 20;
	}

	if((OpenIFF(iffh,IFFF_WRITE))) return 20;

	ami_svg_to_dr2d(iffh,buffer,size,(char *)rarray[A_SVG]);

	FreeVec(buffer);
	if(iffh) CloseIFF(iffh);
	if(iffh->iff_Stream) Close((BPTR)iffh->iff_Stream);
	if(iffh) FreeIFF(iffh);
	FreeArgs(args);
}

#endif // AMIGA_DR2D_STANDALONE
#endif // WITH_NS_SVG
