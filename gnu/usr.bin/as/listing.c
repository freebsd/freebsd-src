/* listing.c - mainting assembly listings
   Copyright (C) 1991, 1992 Free Software Foundation, Inc.
   
   This file is part of GAS, the GNU Assembler.
   
   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

/*
  Contributed by Steve Chamberlain
  sac@cygnus.com
  
  
  A listing page looks like:
  
  LISTING_HEADER  sourcefilename pagenumber
  TITLE LINE
  SUBTITLE LINE
  linenumber address data  source
  linenumber address data  source
  linenumber address data  source
  linenumber address data  source
  
  If not overridden, the listing commands are:
  
  .title  "stuff" 
  Put "stuff" onto the title line 
  .sbttl  "stuff"
  Put stuff onto the subtitle line
  
  If these commands come within 10 lines of the top of the page, they
  will affect the page they are on, as well as any subsequent page
  
  .eject
  Thow a page
  .list
  Increment the enable listing counter
  .nolist
  Decrement the enable listing counter
  
  .psize Y[,X]
  Set the paper size to X wide and Y high. Setting a psize Y of
  zero will suppress form feeds except where demanded by .eject
  
  If the counter goes below zero, listing is suppressed. 
  
  
  Listings are a maintained by read calling various listing_<foo>
  functions.  What happens most is that the macro NO_LISTING is not
  defined (from the Makefile), then the macro LISTING_NEWLINE expands
  into a call to listing_newline.  The call is done from read.c, every
  time it sees a newline, and -l is on the command line.
  
  The function listing_newline remembers the frag associated with the
  newline, and creates a new frag - note that this is wasteful, but not
  a big deal, since listing slows things down a lot anyway.  The
  function also rememebers when the filename changes.
  
  When all the input has finished, and gas has had a chance to settle
  down, the listing is output. This is done by running down the list of
  frag/source file records, and opening the files as needed and printing
  out the bytes and chars associated with them.
  
  The only things which the architecture can change about the listing
  are defined in these macros:
  
  LISTING_HEADER		The name of the architecture
  LISTING_WORD_SIZE      The make of the number of bytes in a word, this determines
  the clumping of the output data. eg a value of
  2 makes words look like 1234 5678, whilst 1
  would make the same value look like 12 34 56
  78
  LISTING_LHS_WIDTH      Number of words of above size for the lhs
  
  LISTING_LHS_WIDTH_SECOND   Number of words for the data on the lhs
  for the second line
  
  LISTING_LHS_CONT_LINES	Max number of lines to use up for a continutation
  LISTING_RHS_WIDTH      Number of chars from the input file to print
  on a line
  */

#ifndef lint
static char rcsid[] = "$Id: listing.c,v 1.1 1993/11/03 00:51:54 paul Exp $";
#endif

#include "as.h"

#ifndef NO_LISTING

#include <obstack.h>
#include "input-file.h"
#include "targ-cpu.h"

#ifndef LISTING_HEADER
#define LISTING_HEADER "GAS LISTING"
#endif
#ifndef LISTING_WORD_SIZE
#define LISTING_WORD_SIZE 4
#endif
#ifndef LISTING_LHS_WIDTH
#define LISTING_LHS_WIDTH 1
#endif
#ifndef LISTING_LHS_WIDTH_SECOND
#define LISTING_LHS_WIDTH_SECOND 1
#endif
#ifndef LISTING_RHS_WIDTH
#define LISTING_RHS_WIDTH 100
#endif
#ifndef LISTING_LHS_CONT_LINES 
#define LISTING_LHS_CONT_LINES 4
#endif


/* This structure remembers which .s were used */
typedef struct file_info_struct {
	char *filename;
	int linenum;
	FILE *file;
	struct file_info_struct *next;
	int end_pending;
} file_info_type ;


/* this structure rememebrs which line from which file goes into which frag */
typedef struct list_info_struct {
	/* Frag which this line of source is nearest to */
	fragS *frag;
	/* The actual line in the source file */
	unsigned int line;
	/* Pointer to the file info struct for the file which this line
	   belongs to */
	file_info_type *file;
	
	/* Next in list */
	struct list_info_struct *next;
	
	
	/* Pointer to the file info struct for the high level language
	   source line that belongs here */
	file_info_type *hll_file;
	
	/* High level language source line */
	int hll_line;
	
	
	/* Pointer to any error message associated with this line */
	char *message;
	
	enum {
		EDICT_NONE,
		EDICT_SBTTL,
		EDICT_TITLE,
		EDICT_NOLIST,
		EDICT_LIST,
		EDICT_EJECT,
	} edict;
	char *edict_arg;
} list_info_type;

static struct list_info_struct *head;
struct list_info_struct *listing_tail;
extern int listing;
extern unsigned int  physical_input_line;
extern fragS *frag_now;

static int paper_width = 200;
static int paper_height = 60;

/* this static array is used to keep the text of data to be printed
   before the start of the line.  It is stored so we can give a bit
   more info on the next line.  To much, and large initialized arrays
   will use up lots of paper.  */

static char data_buffer[100];
static unsigned int data_buffer_size;

static void
    listing_message(name, message)
char *name;
char *message;
{
	unsigned int l = strlen(name) + strlen(message) + 1;
	char *n = malloc(l);
	strcpy(n,name);
	strcat(n,message);
	if (listing_tail != (list_info_type *)NULL) {
		listing_tail->message = n;
	}

	return;
} /* lising_message() */

void 
    listing_warning(message)
char *message;
{
	listing_message("Warning:", message);
}

void
    listing_error(message)
char *message;
{
	listing_message("Error:", message);
}

static file_info_type *file_info_head;

static file_info_type *
    file_info(file_name)
char *file_name;
{
	/* Find an entry with this file name */
	file_info_type *p = file_info_head;
	
	while (p != (file_info_type *)NULL) {
		if (strcmp(p->filename, file_name) == 0)
		    return(p);
		p = p->next;  
	}
	
	/* Make new entry */
	
	p = (file_info_type *) xmalloc(sizeof(file_info_type));
	p->next = file_info_head;
	file_info_head = p;
	p->filename = xmalloc(strlen(file_name)+1);
	strcpy(p->filename, file_name);
	p->linenum = 0;
	p->end_pending = 0;
	
	p->file = fopen(p->filename,"r");
	return(p);
} /* file_info() */


static void 
    new_frag()
{
	frag_wane(frag_now);
	frag_new(0);
}

void 
    listing_newline(ps)
char *ps;
{
	char *s = ps;
	extern char *file_name;
	static unsigned int last_line = 0xffff ;  
	
	
	list_info_type *new;
	if (physical_input_line != last_line) {
		last_line = physical_input_line;
		new_frag();
		
		new  = (list_info_type *) malloc(sizeof(list_info_type));
		new->frag = frag_now;
		new->line = physical_input_line ;
		new->file = file_info(file_name);
		
		if (listing_tail) {
			listing_tail->next = new;
		} else {
			head = new;
		}
		
		listing_tail = new;
		new->next = (list_info_type *) NULL;
		new->message = (char *) NULL;
		new->edict = EDICT_NONE;    
		new->hll_file = (file_info_type*) NULL;
		new->hll_line = 0;
		new_frag();
	}

	return;
} /* listing_newline() */


/* This function returns the next source line from the file supplied,
   truncated to size.  It appends a fake line to the end of each input
   file to make.  */

static char *
    buffer_line(file, line, size)
file_info_type *file;
char *line;
unsigned int size;
{
	unsigned int count = 0;
	int c;
	
	char *p = line;
	
	/* If we couldn't open the file, return an empty line */
	if (file->file == (FILE*) NULL) {
		return("");
	}
	
	if (file->end_pending == 10) {
		*p ++ = '\n';
		rewind(file->file);
		file->linenum = 0;
		file->end_pending = 0;
	}  
	
	c = fgetc(file->file);
	size -= 1;			/* leave room for null */
	
	while (c != EOF && c != '\n') {
		if (count < size) 
		    *p++ = c;
		count++;
		
		c = fgetc(file->file);
	}
	
	if (c == EOF) {
		file->end_pending ++;
		*p++ = 'E';
		*p++ = 'O';
		*p++ = 'F';
	}
	
	file->linenum++;  
	*p++ = 0;
	return(line);
} /* buffer_line() */

static char *fn;

static unsigned int eject;	/* Eject pending */
static unsigned int page; 	/* Current page number */
static char *title; 		/* current title */
static char *subtitle;		/* current subtitle */
static unsigned int on_page; 	/* number of lines printed on current page */

static void
    listing_page(list)
list_info_type *list;
{
	/* Grope around, see if we can see a title or subtitle edict
	   coming up soon  (we look down 10 lines of the page and see
	   if it's there).  */

	if ((eject || (on_page >= paper_height)) && paper_height != 0) {
		unsigned int c = 10;
		int had_title = 0;
		int had_subtitle = 0;
		
		page++;
		
		while (c != 0 && list) {
			if (list->edict == EDICT_SBTTL && !had_subtitle) {
				had_subtitle = 1;
				subtitle = list->edict_arg;
			}
			
			if (list->edict == EDICT_TITLE && !had_title) {
				had_title = 1;
				title = list->edict_arg;
			}
			list = list->next;
			--c;
		}    
		
		if (page > 1) {
			printf("\f");
		}
		
		printf("%s %s \t\t\tpage %d\n", LISTING_HEADER, fn, page);
		printf("%s\n", title);
		printf("%s\n", subtitle);
		on_page = 3;
		eject = 0;
	}

	return;
} /* listing_page() */


static unsigned int 
    calc_hex(list)
list_info_type *list;
{
	list_info_type *first = list;
	list_info_type *last = first;
	unsigned int address = ~0;
	
	fragS *frag;
	fragS *frag_ptr;
	
	unsigned int byte_in_frag = 0;
	
	int anything = 0;      
	
	/* Find first frag which says it belongs to this line */
	frag = list->frag; 
	while (frag  && frag->line != list) 
	    frag = frag->fr_next;
	
	frag_ptr = frag;
	
	data_buffer_size = 0;
	
	/* Dump all the frags which belong to this line */
	while (frag_ptr != (fragS *)NULL  && frag_ptr->line == first) {
		/* Print as many bytes from the fixed part as is sensible */
		while (byte_in_frag < frag_ptr->fr_fix && data_buffer_size < sizeof(data_buffer)-10) {
			if (address == ~0) {
				address = frag_ptr->fr_address;
			}
			
			sprintf(data_buffer + data_buffer_size, "%02X", (frag_ptr->fr_literal[byte_in_frag]) & 0xff);
			data_buffer_size += 2;
			byte_in_frag++;
		}
		
		/* Print as many bytes from the variable part as is sensible */
		while (byte_in_frag < frag_ptr->fr_var * frag_ptr->fr_offset 
		       && data_buffer_size < sizeof(data_buffer)-10) {
			if (address == ~0) {
				address =  frag_ptr->fr_address;
			}
			data_buffer[data_buffer_size++] = '*';
			data_buffer[data_buffer_size++] = '*';
			
			byte_in_frag++;
		}
		
		frag_ptr = frag_ptr->fr_next;
	}
	
	data_buffer[data_buffer_size++] = 0;
	return address;
} /* calc_hex() */

static void
    print_lines(list, string, address)
list_info_type *list;
char *string;
unsigned int address;
{
	unsigned int idx;
	unsigned int nchars;
	unsigned int lines;
	unsigned int byte_in_word =0;
	char *src = data_buffer;
	
	/* Print the stuff on the first line */
	listing_page(list);
	nchars = (LISTING_WORD_SIZE * 2 + 1)  * LISTING_LHS_WIDTH ;
	
	/* Print the hex for the first line */
	if (address == ~0) {
		printf("% 4d     ", list->line);
		for (idx = 0; idx < nchars; idx++)
		    printf(" ");
		
		printf("\t%s\n", string ? string : "");
		on_page++;
		listing_page(0);
	} else {
		if (had_errors()) {
			printf("% 4d ???? ", list->line);
		} else {
			printf("% 4d %04x ", list->line, address);
		}
		
		/* And the data to go along with it */
		idx = 0;
		
		while (*src && idx < nchars) {
			printf("%c%c", src[0], src[1]);
			src += 2;
			byte_in_word++;
			
			if (byte_in_word == LISTING_WORD_SIZE) {
				printf(" ");
				idx++;
				byte_in_word = 0;
			}
			idx+=2;
		}	    
		
		for (;idx < nchars; idx++) 
		    printf(" ");
		
		printf("\t%s\n", string ? string : "");
		on_page++;
		listing_page(list);  
		if (list->message) {
			printf("****  %s\n",list->message);
			listing_page(list);
			on_page++;
		}
		
		for (lines = 0; lines < LISTING_LHS_CONT_LINES && *src; lines++) {
			nchars = ((LISTING_WORD_SIZE*2) +1)  * LISTING_LHS_WIDTH_SECOND -1;
			idx = 0;
			/* Print any more lines of data, but more compactly */
			printf("% 4d      ", list->line);
			
			while (*src && idx < nchars) {
				printf("%c%c", src[0], src[1]);
				src+=2;
				idx+=2;
				byte_in_word++;
				if (byte_in_word == LISTING_WORD_SIZE) {
					printf(" ");
					idx++;
					byte_in_word = 0;
				}
			}
			
			printf("\n");
			on_page++;
			listing_page(list);
		}
	}
} /* print_lines() */


static void
    list_symbol_table()
{
	extern symbolS *symbol_rootP;
	symbolS *ptr;
	
	eject = 1;
	listing_page(0);
	printf("DEFINED SYMBOLS\n");
	on_page++;
	
	for (ptr = symbol_rootP; ptr != (symbolS*)NULL; ptr = symbol_next(ptr)) {
		if (ptr->sy_frag->line) {
			if (strlen(S_GET_NAME(ptr))) {
				printf("%20s:%-5d  %2d:%08x %s \n",
				       ptr->sy_frag->line->file->filename,
				       ptr->sy_frag->line->line,
				       S_GET_SEGMENT(ptr),
				       S_GET_VALUE(ptr),
				       S_GET_NAME(ptr));
				
				on_page++;
				listing_page(0);
			}      
		}
		
	}
	
	printf("\n");
	on_page++;
	listing_page(0);
	printf("UNDEFINED SYMBOLS\n");
	on_page++;
	listing_page(0);
	
	for (ptr = symbol_rootP; ptr != (symbolS*)NULL; ptr = symbol_next(ptr)) {
		if (ptr && strlen(S_GET_NAME(ptr)) != 0) {
			if (ptr->sy_frag->line == 0) {
				printf("%s\n",	     S_GET_NAME(ptr));
				on_page++;
				listing_page(0);
			}
		}
	}

	return;
} /* list_symbol_table() */

void 
    print_source(current_file, list, buffer, width)
file_info_type *current_file;
list_info_type *list;
char *buffer;
unsigned int width;
{
	if (current_file->file) {  
		while (current_file->linenum < list->hll_line) {
			char * p = buffer_line(current_file, buffer, width);
			printf("%4d:%-13s **** %s\n", current_file->linenum, current_file->filename, p);
			on_page++;
			listing_page(list);	
		}
	}
	
	return;
} /* print_source() */

/* Sometimes the user doesn't want to be bothered by the debugging
   records inserted by the compiler, see if the line is suspicioous */

static int
    debugging_pseudo(line)
char *line;
{
	while (isspace(*line)) 
	    line++;
	
	if (*line != '.') return 0;
	
	line++;
	
	if (strncmp(line, "def",3) == 0) return 1;
	if (strncmp(line, "val",3) == 0) return 1;
	if (strncmp(line, "scl",3) == 0) return 1;
	if (strncmp(line, "line",4) == 0) return 1;
	if (strncmp(line, "endef",5) == 0) return 1;
	if (strncmp(line, "ln",2) == 0) return 1;
	if (strncmp(line, "type",4) == 0) return 1;
	if (strncmp(line, "size",4) == 0) return 1;
	if (strncmp(line, "dim",3) == 0) return 1;
	if (strncmp(line, "tag",3) == 0) return 1;
	
	return(0);
} /* debugging_pseudo() */

void 
    listing_listing(name)
char *name;
{
	char *buffer;
	char *message;
	char *p;
	file_info_type *current_hll_file = (file_info_type *) NULL;
	int on_page = 0;
	int show_listing = 1;
	list_info_type *list = head;
	unsigned int addr = 0;  
	unsigned int page = 1;
	unsigned int prev  = 0;
	unsigned int width;
	
	buffer = malloc(LISTING_RHS_WIDTH);
	eject = 1;
	list = head;
	
	while (list != (list_info_type *)NULL && 0) {
		if (list->next)
		    list->frag = list->next->frag;  
		list = list->next;
	}
	
	list = head->next;
	
	while (list) {
		width =  LISTING_RHS_WIDTH > paper_width ?  paper_width : LISTING_RHS_WIDTH;
		
		switch (list->edict) {
		case EDICT_LIST:
			show_listing++;
			break;
		case EDICT_NOLIST:
			show_listing--;
			break;
		case EDICT_EJECT:
			break;
		case EDICT_NONE:
			break;
		case EDICT_TITLE:
			title = list->edict_arg;
			break;
		case EDICT_SBTTL:
			subtitle = list->edict_arg;
			break;
		default:
			abort();
		}
		
		if (show_listing > 0) {
			/* Scan down the list and print all the stuff which can be done
			   with this line (or lines) */
			message = 0;
			
			if (list->hll_file) {
				current_hll_file = list->hll_file;
			}
			
			if (current_hll_file && list->hll_line && listing & LISTING_HLL) {
				print_source(current_hll_file, list, buffer, width);
			}
			
			p = buffer_line(list->file, buffer, width);      
			
			if (! ((listing & LISTING_NODEBUG) && debugging_pseudo(p))) {      
				print_lines(list, p,  calc_hex(list));
			}
			
			if (list->edict == EDICT_EJECT) {
				eject = 1;
			}    
		} else {
			
			p = buffer_line(list->file, buffer, width);      
		}
		
		list = list->next;
	}
	free(buffer);
} /* listing_listing() */

void 
    listing_print(name)
char *name;
{
	title = "";
	subtitle = "";  
	
	if (listing & LISTING_NOFORM) 
	    {
		    paper_height = 0;
	    }
	
	if (listing & LISTING_LISTING) 
	    {
		    listing_listing(name);
		    
	    }
	if (listing & LISTING_SYMBOLS) 
	    {
		    list_symbol_table();
	    }
} /* listing_print() */


void
    listing_file(name)
char *name;
{
	fn = name;  
}

void 
    listing_eject()
{
	listing_tail->edict = EDICT_EJECT;  
	return;
}

void
    listing_flags()
{
	
}

void
    listing_list(on)
unsigned int on;
{
	listing_tail->edict = on ? EDICT_LIST : EDICT_NOLIST;
}


void
    listing_psize()
{
	paper_height = get_absolute_expression();
	
	if (paper_height < 0 || paper_height > 1000) {
		paper_height = 0;
		as_warn("strantge paper height, set to no form");
	}
	
	if (*input_line_pointer == ',') {
		input_line_pointer++;
		paper_width = get_absolute_expression();
	}
	
	return;
} /* listing_psize() */


void
    listing_title(depth)
unsigned int depth;
{
	char *start;
	char *title;
	unsigned int length;
	
	SKIP_WHITESPACE();
	
	if (*input_line_pointer == '\"') {
		input_line_pointer++;
		start = input_line_pointer;
		
		while (*input_line_pointer) {
			if (*input_line_pointer == '\"') {
				length = input_line_pointer - start;
				title = malloc(length + 1);
				memcpy(title, start, length);
				title[length] = 0;
				listing_tail->edict = depth ? EDICT_SBTTL : EDICT_TITLE;
				listing_tail->edict_arg = title;
				input_line_pointer++;
				demand_empty_rest_of_line();
				return;      
			} else if (*input_line_pointer == '\n') {
				as_bad("New line in title");
				demand_empty_rest_of_line();
				return;
			} else {
				input_line_pointer++;
			}
		}
	} else {
		as_bad("expecting title in quotes");
	}
	
	return;
} /* listing_title() */



void
    listing_source_line(line)
unsigned int line;
{
	new_frag();
	listing_tail->hll_line = line;
	new_frag();
	return;
} /* lising_source_line() */

void
    listing_source_file(file)
char *file;
{
	listing_tail->hll_file = file_info(file);
}

#endif /* not NO_LISTING */

/* end of listing.c */
