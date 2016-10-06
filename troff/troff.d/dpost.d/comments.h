/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "comments.h	1.5	05/06/08 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)comments.h	1.5 (gritter) 8/23/05
 */

/*
 *
 * Currently defined file structuring comments from Adobe - plus a few others.
 * Ones that end with a colon expect arguments, while those ending with a newline
 * stand on their own. Truly overkill on Adobe's part and mine for including them
 * all!
 *
 * All PostScript files should begin with a header that starts with one of the
 * following comments.
 *
 */

#define NONCONFORMING			"%!PS\n"
#define MINCONFORMING			"%!PS-Adobe-\n"
#define OLDCONFORMING			"%!PS-Adobe-1.0\n"

#define CONFORMING			"%!PS-Adobe-3.0\n"
#define CONFORMINGEPS			"%!PS-Adobe-3.0 EPS\n"
#define CONFORMINGQUERY			"%!PS-Adobe-3.0 Query\n"
#define CONFORMINGEXITSERVER		"%!PS-Adobe-3.0 ExitServer\n"

/*
 *
 * Header comments - immediately follow the appropriate document classification
 * comment.
 *
 */

#define TITLE				"%%Title:"
#define CREATOR				"%%Creator:"
#define CREATIONDATE			"%%CreationDate:"
#define FOR				"%%For:"
#define ROUTING				"%%Routing:"
#define BOUNDINGBOX			"%%BoundingBox:"
#define PAGES				"%%Pages:"
#define REQUIREMENTS			"%%Requirements:"

#define DOCUMENTFONTS			"%%DocumentFonts:"
#define DOCUMENTNEEDEDFONTS		"%%DocumentNeededFonts:"
#define DOCUMENTSUPPLIEDFONTS		"%%DocumentSuppliedFonts:"
#define DOCUMENTNEEDEDPROCSETS		"%%DocumentNeededProcSets:"
#define DOCUMENTSUPPLIEDPROCSETS	"%%DocumentSuppliedProcSets:"
#define DOCUMENTNEEDEDFILES		"%%DocumentNeededFiles:"
#define DOCUMENTSUPPLIEDFILES		"%%DocumentSuppliedFiles:"
#define DOCUMENTPAPERSIZES		"%%DocumentPaperSizes:"
#define DOCUMENTPAPERFORMS		"%%DocumentPaperForms:"
#define DOCUMENTPAPERCOLORS		"%%DocumentPaperColors:"
#define DOCUMENTPAPERWEIGHTS		"%%DocumentPaperWeights:"
#define DOCUMENTPRINTERREQUIRED		"%%DocumentPrinterREquired:"
#define ENDCOMMENTS			"%%EndComments\n"
#define ENDPROLOG			"%%EndProlog\n"

/*
 *
 * Body comments - can appear anywhere in a document.
 *
 */

#define BEGINSETUP			"%%BeginSetup\n"
#define ENDSETUP			"%%EndSetup\n"
#define BEGINDOCUMENT			"%%BeginDocument:"
#define ENDDOCUMENT			"%%EndDocument\n"
#define BEGINFILE			"%%BeginFile:"
#define ENDFILE				"%%EndFile\n"
#define BEGINPROCSET			"%%BeginProcSet:"
#define ENDPROCSET			"%%EndProcSet\n"
#define BEGINBINARY			"%%BeginBinary:"
#define ENDBINARY			"%%EndBinary\n"
#define BEGINPAPERSIZE			"%%BeginePaperSize:"
#define ENDPAPERSIZE			"%%EndPaperSize\n"
#define BEGINFEATURE			"%%BeginFeature:"
#define ENDFEATURE			"%%EndFeature\n"
#define BEGINEXITSERVER			"%%BeginExitServer:"
#define ENDEXITSERVER			"%%EndExitServer\n"
#define TRAILER				"%%Trailer\n"

/*
 *
 * Page level comments - usually will occur once per page.
 *
 */

#define PAGE				"%%Page:"
#define PAGEFONTS			"%%PageFonts:"
#define PAGEFILES			"%%PageFiles:"
#define PAGEBOUNDINGBOX			"%%PageBoundingBox:"
#define BEGINPAGESETUP			"%%BeginPageSetup\n"
#define BEGINOBJECT			"%%BeginObject:"
#define ENDOBJECT			"%%EndObject\n"

/*
 *
 * Resource requirements - again can appear anywhere in a document.
 *
 */

#define INCLUDEFONT			"%%IncludeFont:"
#define INCLUDEPROCSET			"%%IncludeProcSet:"
#define INCLUDEFILE			"%%IncludeFile:"
#define EXECUTEFILE			"%%ExecuteFile:"
#define CHANGEFONT			"%%ChangeFont:"
#define PAPERFORM			"%%PaparForm:"
#define PAPERCOLOR			"%%PaperColor:"
#define PAPERWEIGHT			"%%PaperWeight:"
#define PAPERSIZE			"%%PaperSize:"
#define FEATURE				"%%Feature:"
#define ENDOFFILE			"%%EOF\n"

#define CONTINUECOMMENT			"%%+"
#define ATEND				"(atend)"

/*
 *
 * Some non-standard document comments. Global definitions are occasionally used
 * in dpost and are marked by BEGINGLOBAL and ENDGLOBAL. The resulting document
 * violates page independence, but can easily be converted to a conforming file
 * using a utililty program.
 *
 */

#define BEGINSCRIPT			"%%BeginScript\n"
#define BEGINGLOBAL			"%%BeginGlobal\n"
#define ENDGLOBAL			"%%EndGlobal\n"
#define ENDPAGE				"%%EndPage:"
#define FORMSPERPAGE			"%%FormsPerPage:"
#define CREATOR				"%%Creator:"
#define	CREATIONDATE			"%%CreationDate:"

