/* BFD back-end for s-record objects.
   Copyright 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support <sac@cygnus.com>.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
SUBSECTION
	S-Record handling

DESCRIPTION
	
	Ordinary S-Records cannot hold anything but addresses and
	data, so that's all that we implement.
   
	The only interesting thing is that S-Records may come out of
	order and there is no header, so an initial scan is required
	to discover the minimum and maximum addresses used to create
	the vma and size of the only section we create.  We
	arbitrarily call this section ".text". 

	When bfd_get_section_contents is called the file is read
	again, and this time the data is placed into a bfd_alloc'd
	area.

	Any number of sections may be created for output, we save them
	up and output them when it's time to close the bfd.

	An s record looks like:
	
EXAMPLE
	S<type><length><address><data><checksum>
	
DESCRIPTION
	Where
	o length
	is the number of bytes following upto the checksum. Note that
	this is not the number of chars following, since it takes two
	chars to represent a byte.
	o type
	is one of:
	0) header record
	1) two byte address data record
	2) three byte address data record
	3) four byte address data record
	7) four byte address termination record
	8) three byte address termination record
	9) two byte address termination record
	
	o address
	is the start address of the data following, or in the case of
	a termination record, the start address of the image
	o data
	is the data.
	o checksum
	is the sum of all the raw byte data in the record, from the length
	upwards, modulo 256 and subtracted from 255.


SUBSECTION
	Symbol S-Record handling

DESCRIPTION
	Some ICE equipment understands an addition to the standard
	S-Record format; symbols and their addresses can be sent
	before the data.

	The format of this is:
	($$ <modulename>
		(<space> <symbol> <address>)*)
	$$

	so a short symbol table could look like:

EXAMPLE
	$$ flash.x
	$$ flash.c
	  _port6 $0
	  _delay $4
	  _start $14
	  _etext $8036
	  _edata $8036
 	  _end $8036
	$$ 

DESCRIPTION
	We allow symbols to be anywhere in the data stream - the module names
	are always ignored.
		
*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

/* Macros for converting between hex and binary */

static CONST char digs[] = "0123456789ABCDEF";

static char hex_value[1 + (unsigned char)~0];

#define NOT_HEX 20
#define NIBBLE(x) hex_value[(unsigned char)(x)]
#define HEX(buffer) ((NIBBLE((buffer)[0])<<4) + NIBBLE((buffer)[1]))
#define TOHEX(d, x, ch) \
	d[1] = digs[(x) & 0xf]; \
	d[0] = digs[((x)>>4)&0xf]; \
	ch += ((x) & 0xff);
#define	ISHEX(x)  (hex_value[(unsigned char)(x)] != NOT_HEX)



static void
DEFUN_VOID(srec_init) 
{
    unsigned int i;
    static boolean inited = false;
    
    if (inited == false) 
    {
	
	inited = true;
	
	for (i = 0; i < sizeof (hex_value); i++) 
	{
	    hex_value[i] = NOT_HEX;
	}
    
	for (i = 0; i < 10; i++) 
	{
	    hex_value[i + '0'] = i;
	
	}
	for (i = 0; i < 6; i++) 
	{
	    hex_value[i + 'a'] = i+10;
	    hex_value[i + 'A'] = i+10;
	}
    }    
}


/* The maximum number of bytes on a line is FF */
#define MAXCHUNK 0xff 
/* The number of bytes we fit onto a line on output */
#define CHUNK 21

/* We cannot output our srecords as we see them, we have to glue them
   together, this is done in this structure : */

struct srec_data_list_struct
{
    unsigned    char *data;
    bfd_vma where;
    bfd_size_type size;
    struct srec_data_list_struct *next;

    
} ;
typedef struct srec_data_list_struct srec_data_list_type;


typedef struct  srec_data_struct
{
    srec_data_list_type *head;    
    unsigned int type;
    
    int done_symbol_read;
    int count;
    asymbol *symbols;
    char *strings;
    int symbol_idx;
    int string_size;
    int string_idx;
} tdata_type;


/* 
   called once per input S-Record, used to work out vma and size of data.
 */

static bfd_vma low,high;

static void
size_symbols(abfd, buf, len, val)
bfd *abfd;
char *buf;
int len;
int val;
{
  abfd->symcount ++;
  abfd->tdata.srec_data->string_size  += len + 1;
}

static void
fillup_symbols(abfd, buf, len, val)
bfd *abfd;
char *buf;
int len;
int val;
{
  if (!abfd->tdata.srec_data->done_symbol_read)
  {
    asymbol *p;
    if (abfd->tdata.srec_data->symbols == 0)
    {
      abfd->tdata.srec_data->symbols = (asymbol *)bfd_alloc(abfd, abfd->symcount * sizeof(asymbol));
      abfd->tdata.srec_data->strings = (char*)bfd_alloc(abfd, abfd->tdata.srec_data->string_size);
      abfd->tdata.srec_data->symbol_idx = 0;
      abfd->tdata.srec_data->string_idx = 0;
    }

    p = abfd->tdata.srec_data->symbols + abfd->tdata.srec_data->symbol_idx++;
    p->the_bfd = abfd;
    p->name = abfd->tdata.srec_data->strings + abfd->tdata.srec_data->string_idx;
    memcpy((char *)(p->name), buf, len+1);
    abfd->tdata.srec_data->string_idx += len + 1;
    p->value = val;
    p->flags = BSF_EXPORT | BSF_GLOBAL;
    p->section = &bfd_abs_section;
    p->udata = 0;
  }
}
static void
DEFUN(size_srec,(abfd, section, address, raw, length),
      bfd *abfd AND
      asection *section AND
      bfd_vma address AND
      bfd_byte *raw AND
      unsigned int length)
{
  if (address < low)
    low = address;
  if (address + length > high) 
    high = address + length -1;
}


/*
 called once per input S-Record, copies data from input into bfd_alloc'd area
 */

static void
DEFUN(fillup,(abfd, section, address, raw, length),
bfd *abfd AND
asection *section AND
bfd_vma address AND
bfd_byte *raw AND
unsigned int length)
{
    unsigned int i;
    bfd_byte *dst =
     (bfd_byte *)(section->used_by_bfd) + address - section->vma;
    /* length -1 because we don't read in the checksum */
    for (i = 0; i < length -1 ; i++) {
	    *dst = HEX(raw);
	    dst++;
	    raw+=2;
	}
}

/* Pass over an S-Record file, calling one of the above functions on each
   record.  */

static int white(x)
char x;
{
return (x== ' ' || x == '\t' || x == '\n' || x == '\r');
}
static int
skipwhite(src,abfd)
char *src;
bfd *abfd;
{
  int eof = 0;
  while (white(*src) && !eof)
  {
    eof =  (boolean)(bfd_read(src, 1, 1, abfd) != 1);	  
  }
  return eof;
}

static boolean
DEFUN(srec_mkobject, (abfd), 
      bfd *abfd)
{
  if (abfd->tdata.srec_data == 0) 
  {
    tdata_type *tdata = (tdata_type *)bfd_alloc(abfd,  sizeof(tdata_type));
    abfd->tdata.srec_data = tdata;
    tdata->type = 1;
    tdata->head = (srec_data_list_type *)NULL;
  }
  return true;
    
}

static void
DEFUN(pass_over,(abfd, func, symbolfunc, section),
      bfd *abfd AND
      void (*func)() AND
      void (*symbolfunc)() AND
      asection *section)
{
  unsigned int bytes_on_line;
  boolean eof = false;

  srec_mkobject(abfd);
  /* To the front of the file */
  bfd_seek(abfd, (file_ptr)0, SEEK_SET);
  while (eof == false)
  {
    char buffer[MAXCHUNK];
    char *src = buffer;
    char type;
    bfd_vma address = 0;

    /* Find first 'S' or $ */
    eof =  (boolean)(bfd_read(src, 1, 1, abfd) != 1);
    switch (*src) 
    {
     default:
      eof = (boolean)(bfd_read(src, 1, 1, abfd) != 1);
      if (eof)  return;
      break;

     case '$':
      /* Inside a symbol definition - just ignore the module name */
      while (*src != '\n' && !eof) 
      {
	eof =  (boolean)(bfd_read(src, 1, 1, abfd) != 1);	  
      }
      break;

     case ' ':
      /* spaces - maybe just before a symbol */
      while (*src != '\n' && white(*src)) {
      eof = skipwhite(src, abfd);

{
	int val = 0;
	int slen = 0;
	char symbol[MAXCHUNK];

	/* get the symbol part */
	while (!eof && !white(*src) && slen < MAXCHUNK)
	{
	  symbol[slen++] = *src;
	  eof =  (boolean)(bfd_read(src, 1, 1, abfd) != 1);	  
	}
	symbol[slen] = 0;
	eof = skipwhite(src, abfd);
	/* skip the $ for the hex value */
	if (*src == '$') 
	{
	  eof =  (boolean)(bfd_read(src, 1, 1, abfd) != 1);
	}

	/* Scan off the hex number */
	while (isxdigit(*src ))
	{
	  val *= 16;
	  if (isdigit(*src))
	   val += *src - '0';
	  else if (isupper(*src)) {
	    val += *src - 'A' + 10;
	  }
	  else {
	    val += *src - 'a' + 10;
	  }
	  eof =  (boolean)(bfd_read(src, 1, 1, abfd) != 1);
	}
	symbolfunc(abfd, symbol, slen, val);
      }
}
      break;
     case 'S':
      src++;

      /* Fetch the type and the length */
      bfd_read(src, 1, 3, abfd);

      type = *src++;

      if (!ISHEX (src[0]) || !ISHEX (src[1]))
       break;

      bytes_on_line = HEX(src);

      if (bytes_on_line > MAXCHUNK/2)
       break;
      src+=2 ;

      bfd_read(src, 1 , bytes_on_line * 2, abfd);

      switch (type) {
       case '0':
       case '5':
	/* Prologue - ignore */
	break;
       case '3':
	address = HEX(src);
	src+=2;
	bytes_on_line--;
		
       case '2':
	address = HEX(src) | (address<<8) ;
	src+=2;
	bytes_on_line--;
       case '1':
	address = HEX(src) | (address<<8) ;
	src+=2;
	address = HEX(src) | (address<<8) ;
	src+=2;
	bytes_on_line-=2;
	func(abfd,section, address, src, bytes_on_line);
	break;
       default:
	return;
      }
    }
  }

}

static bfd_target *
object_p(abfd)
bfd *abfd;
{
  asection *section;
  /* We create one section called .text for all the contents, 
     and allocate enough room for the entire file.  */
  
  section =  bfd_make_section(abfd, ".text");
  section->_raw_size = 0;
  section->vma = 0xffffffff;
  low = 0xffffffff;
  high = 0;
  pass_over(abfd, size_srec, size_symbols, section);
  section->_raw_size = high - low;
  section->vma = low;
  section->flags = SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC;

  if (abfd->symcount)
   abfd->flags |= HAS_SYMS;  
  return abfd->xvec;
}

static bfd_target *
DEFUN(srec_object_p, (abfd),
      bfd *abfd)
{
  char b[4];

  srec_init();
  
  bfd_seek(abfd, (file_ptr)0, SEEK_SET);
  bfd_read(b, 1, 4, abfd);

  if (b[0] != 'S' || !ISHEX(b[1]) || !ISHEX(b[2]) || !ISHEX(b[3]))
   return (bfd_target*) NULL;
  
  /* We create one section called .text for all the contents, 
     and allocate enough room for the entire file.  */

  return object_p(abfd); 
}


static bfd_target *
DEFUN(symbolsrec_object_p, (abfd),
      bfd *abfd)
{
  char b[4];

  srec_init();
  
  bfd_seek(abfd, (file_ptr)0, SEEK_SET);
  bfd_read(b, 1, 4, abfd);

  if (b[0] != '$' || b[1] != '$')
    return (bfd_target*) NULL;

  return object_p(abfd);   
}


static boolean
DEFUN(srec_get_section_contents,(abfd, section, location, offset, count),
      bfd *abfd AND
      asection *section AND
      PTR location AND
      file_ptr offset AND
      bfd_size_type count)
{
    if (section->used_by_bfd == (PTR)NULL) 
    {
	section->used_by_bfd = (PTR)bfd_alloc (abfd, section->_raw_size);
	
	pass_over(abfd, fillup, fillup_symbols, section);
    }
    (void) memcpy((PTR)location,
		  (PTR)((char *)(section->used_by_bfd) + offset),
		  count);
    return true;
}
      


boolean
DEFUN(srec_set_arch_mach,(abfd, arch, machine),
      bfd *abfd AND
      enum bfd_architecture arch AND
      unsigned long machine)
{
  return bfd_default_set_arch_mach(abfd, arch, machine);
}


/* we have to save up all the Srecords for a splurge before output,
   also remember   */

static boolean
DEFUN(srec_set_section_contents,(abfd, section, location, offset, bytes_to_do),
      bfd *abfd AND
      sec_ptr section AND
      PTR location AND
      file_ptr offset AND
      bfd_size_type bytes_to_do)
{
  tdata_type  *tdata = abfd->tdata.srec_data;
  srec_data_list_type *entry = (srec_data_list_type *)
    bfd_alloc(abfd, sizeof(srec_data_list_type));

  if ((section->flags & SEC_ALLOC)
      && (section->flags & SEC_LOAD)) 
    {
      unsigned  char *data = (unsigned char *) bfd_alloc(abfd, bytes_to_do);
      memcpy(data, location, bytes_to_do);

      if ((section->lma + offset + bytes_to_do) <= 0xffff)  
	{

	}
      else if ((section->lma + offset + bytes_to_do) <= 0xffffff 
	       && tdata->type < 2) 
	{
	  tdata->type = 2;
	}
      else 
	{
	  tdata->type = 3;
	}

      entry->data = data;
      entry->where = section->lma + offset;
      entry->size = bytes_to_do;
      entry->next = tdata->head;
      tdata->head = entry;
    }
  return true;    
}

/* Write a record of type, of the supplied number of bytes. The
   supplied bytes and length don't have a checksum. That's worked out
   here
*/
static
void DEFUN(srec_write_record,(abfd, type, address, data, end),
	   bfd *abfd AND
	   char type AND
	   bfd_vma address AND
	   CONST unsigned char *data AND
	   CONST unsigned char *end)

{
    char buffer[MAXCHUNK];
    
    unsigned int check_sum = 0;
    unsigned CONST char *src = data;
    char *dst =buffer;
    char *length;
    

    *dst++ = 'S';
    *dst++ = '0' + type;

    length = dst;
    dst+=2;			/* leave room for dst*/
    
    switch (type) 
    {
      case 3:
      case 7:
	TOHEX(dst, (address >> 24), check_sum);
	dst+=2;
      case 8:
      case 2:
	TOHEX(dst, (address >> 16), check_sum);
	dst+=2;
      case 9:
      case 1:
      case 0:
	TOHEX(dst, (address >> 8), check_sum);
	dst+=2;
	TOHEX(dst, (address), check_sum);
	dst+=2;
	break;

    }
    for (src = data; src < end; src++) 
    {
	TOHEX(dst, *src, check_sum);
	dst+=2;
    }

    /* Fill in the length */
    TOHEX(length, (dst - length)/2, check_sum);
    check_sum &= 0xff;
    check_sum = 255 - check_sum;
    TOHEX(dst, check_sum, check_sum);
    dst+=2;
    
    *dst ++ = '\r';
    *dst ++ = '\n';
    bfd_write((PTR)buffer, 1, dst - buffer , abfd);
}



static void
DEFUN(srec_write_header,(abfd),
      bfd *abfd)
{
    unsigned char buffer[MAXCHUNK];
    unsigned char *dst = buffer;
    unsigned int i;

    /* I'll put an arbitary 40 char limit on header size */
    for (i = 0; i < 40 && abfd->filename[i];  i++) 
    {
	*dst++ = abfd->filename[i];
    }
    srec_write_record(abfd,0, 0, buffer, dst);
}

static void
DEFUN(srec_write_section,(abfd, tdata, list),
       bfd *abfd AND
       tdata_type *tdata AND
       srec_data_list_type *list)
{
    unsigned int bytes_written = 0;
    unsigned char *location = list->data;

    while (bytes_written < list->size)
    {
	bfd_vma address;
	
	unsigned int bytes_this_chunk = list->size - bytes_written;

	if (bytes_this_chunk > CHUNK) 
	{
	    bytes_this_chunk = CHUNK;
	}

	address = list->where +  bytes_written;

	srec_write_record(abfd,
			  tdata->type,
			  address,
			  location,
			  location + bytes_this_chunk);

	bytes_written += bytes_this_chunk;
	location += bytes_this_chunk;
    }

}

static void
DEFUN(srec_write_terminator,(abfd, tdata),
      bfd *abfd AND
      tdata_type *tdata)
{
    unsigned    char buffer[2];
    
    srec_write_record(abfd, 10 - tdata->type,
		      abfd->start_address, buffer, buffer);
}


      
static void
srec_write_symbols(abfd)
     bfd *abfd;
{
  char buffer[MAXCHUNK];
  /* Dump out the symbols of a bfd */
  int i;
  int len = bfd_get_symcount(abfd);

  if (len) 
  {
    asymbol **table = bfd_get_outsymbols(abfd);
    sprintf(buffer, "$$ %s\r\n", abfd->filename);

    bfd_write(buffer, strlen(buffer), 1, abfd);

    for (i = 0; i < len; i++) 
    {
      asymbol *s = table[i];
#if 0
      int len = strlen(s->name);

      /* If this symbol has a .[ocs] in it, it's probably a file name
	 and we'll output that as the module name */

      if (len > 3 && s->name[len-2] == '.') 
      {
	int l;
	sprintf(buffer, "$$ %s\r\n", s->name);
	l = strlen(buffer);
	bfd_write(buffer, l, 1, abfd);
      }
      else
#endif
       if (s->flags & (BSF_GLOBAL | BSF_LOCAL) 
	       && (s->flags & BSF_DEBUGGING) == 0
	       && s->name[0] != '.'
	       && s->name[0] != 't')
      {
	/* Just dump out non debug symbols */

	int l;
	char buf2[40], *p;

	sprintf_vma (buf2, s->value + s->section->lma);
	p = buf2;
	while (p[0] == '0' && p[1] != 0)
	  p++;
	sprintf (buffer, "  %s $%s\r\n", s->name, p);
	l = strlen(buffer);
	bfd_write(buffer, l, 1,abfd);
      }
    }
    sprintf(buffer, "$$ \r\n");
    bfd_write(buffer, strlen(buffer), 1, abfd);
  }
}

static boolean
internal_srec_write_object_contents(abfd, symbols)
     bfd *abfd;
     int symbols;
{
    int bytes_written;
    tdata_type *tdata = abfd->tdata.srec_data;
    srec_data_list_type *list;

    bytes_written = 0;
    
    
    if (symbols)
     srec_write_symbols(abfd);

    srec_write_header(abfd);

    /* Now wander though all the sections provided and output them */
    list = tdata->head;

    while (list != (srec_data_list_type*)NULL) 
    {
	srec_write_section(abfd, tdata, list); 
	list = list->next;
    }
    srec_write_terminator(abfd, tdata);
    return true;
}

static boolean
srec_write_object_contents(abfd)
     bfd *abfd;
{
  return internal_srec_write_object_contents(abfd, 0);
}

static boolean
symbolsrec_write_object_contents(abfd)
     bfd *abfd;
{
  return internal_srec_write_object_contents(abfd, 1);
}

static int 
DEFUN(srec_sizeof_headers,(abfd, exec),
      bfd *abfd AND
      boolean exec)
{
return 0;
}

static asymbol *
DEFUN(srec_make_empty_symbol, (abfd),
      bfd*abfd)
{
  asymbol *new=  (asymbol *)bfd_zalloc (abfd, sizeof (asymbol));
  new->the_bfd = abfd;
  return new;
}

static unsigned int
srec_get_symtab_upper_bound(abfd)
bfd *abfd;
{
  /* Read in all the info */
  srec_get_section_contents(abfd,abfd->sections,0,0,0);
  return (bfd_get_symcount(abfd) + 1) * (sizeof(asymbol *));
}

static unsigned int
DEFUN(srec_get_symtab, (abfd, alocation),
      bfd            *abfd AND
      asymbol       **alocation)
{
  int lim = abfd->symcount;
  int i;
  for (i = 0; i < lim; i++) {
    alocation[i] = abfd->tdata.srec_data->symbols + i;
  }
  alocation[i] = 0;
  return lim;
}

void 
DEFUN(srec_get_symbol_info,(ignore_abfd, symbol, ret),
      bfd *ignore_abfd AND
      asymbol *symbol AND
      symbol_info *ret)
{
  bfd_symbol_info (symbol, ret);
}

void 
DEFUN(srec_print_symbol,(ignore_abfd, afile, symbol, how),
      bfd *ignore_abfd AND
      PTR afile AND
      asymbol *symbol AND
      bfd_print_symbol_type how)
{
  FILE *file = (FILE *)afile;
  switch (how) 
  {
   case bfd_print_symbol_name:
    fprintf (file, "%s", symbol->name);
    break;
   default:
    bfd_print_symbol_vandf ((PTR) file, symbol);
    fprintf (file, " %-5s %s",
	     symbol->section->name,
	     symbol->name);

  }
}

#define FOO PROTO
#define srec_new_section_hook (FOO(boolean, (*), (bfd *, asection *)))bfd_true

#define srec_get_reloc_upper_bound (FOO(unsigned int, (*),(bfd*, asection *)))bfd_false
#define srec_canonicalize_reloc (FOO(unsigned int, (*),(bfd*,asection *, arelent **, asymbol **))) bfd_0



#define srec_openr_next_archived_file (FOO(bfd *, (*), (bfd*,bfd*))) bfd_nullvoidptr
#define srec_find_nearest_line (FOO(boolean, (*),(bfd*,asection*,asymbol**,bfd_vma, CONST char**, CONST char**, unsigned int *))) bfd_false
#define srec_generic_stat_arch_elt  (FOO(int, (*), (bfd *,struct stat *))) bfd_0


#define srec_core_file_failing_command (char *(*)())(bfd_nullvoidptr)
#define srec_core_file_failing_signal (int (*)())bfd_0
#define srec_core_file_matches_executable_p (FOO(boolean, (*),(bfd*, bfd*)))bfd_false
#define srec_slurp_armap bfd_true
#define srec_slurp_extended_name_table bfd_true
#define srec_truncate_arname (void (*)())bfd_nullvoidptr
#define srec_write_armap  (FOO( boolean, (*),(bfd *, unsigned int, struct orl *, unsigned int, int))) bfd_nullvoidptr
#define srec_get_lineno (struct lineno_cache_entry *(*)())bfd_nullvoidptr
#define	srec_close_and_cleanup	bfd_generic_close_and_cleanup
#define srec_bfd_debug_info_start bfd_void
#define srec_bfd_debug_info_end bfd_void
#define srec_bfd_debug_info_accumulate  (FOO(void, (*), (bfd *,	 asection *))) bfd_void
#define srec_bfd_get_relocated_section_contents bfd_generic_get_relocated_section_contents
#define srec_bfd_relax_section bfd_generic_relax_section
#define srec_bfd_seclet_link bfd_generic_seclet_link
#define srec_bfd_reloc_type_lookup \
  ((CONST struct reloc_howto_struct *(*) PARAMS ((bfd *, bfd_reloc_code_real_type))) bfd_nullvoidptr)
#define srec_bfd_make_debug_symbol \
  ((asymbol *(*) PARAMS ((bfd *, void *, unsigned long))) bfd_nullvoidptr)

bfd_target srec_vec =
{
    "srec",			/* name */
    bfd_target_srec_flavour,
    true,			/* target byte order */
    true,			/* target headers byte order */
    (HAS_RELOC | EXEC_P |	/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | DYNAMIC | WP_TEXT | D_PAGED),
    (SEC_CODE|SEC_DATA|SEC_ROM|SEC_HAS_CONTENTS
     |SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
     0,				/* leading underscore */
    ' ',			/* ar_pad_char */
    16,				/* ar_max_namelen */
    1,				/* minimum alignment */
    bfd_getb64, bfd_getb_signed_64, bfd_putb64,
      bfd_getb32, bfd_getb_signed_32,     bfd_putb32,
      bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
    bfd_getb64, bfd_getb_signed_64, bfd_putb64,
      bfd_getb32, bfd_getb_signed_32,     bfd_putb32,
      bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

  {
      _bfd_dummy_target,
      srec_object_p,		/* bfd_check_format */
      (struct bfd_target *(*)()) bfd_nullvoidptr,
      (struct bfd_target *(*)())     bfd_nullvoidptr,
  },
  {
      bfd_false,
      srec_mkobject,
      _bfd_generic_mkarchive,
      bfd_false,
  },
  {				/* bfd_write_contents */
      bfd_false,
      srec_write_object_contents,
      _bfd_write_archive_contents,
      bfd_false,
  },
    JUMP_TABLE(srec)
 };



bfd_target symbolsrec_vec =
{
    "symbolsrec",		/* name */
    bfd_target_srec_flavour,
    true,			/* target byte order */
    true,			/* target headers byte order */
    (HAS_RELOC | EXEC_P |	/* object flags */
     HAS_LINENO | HAS_DEBUG |
     HAS_SYMS | HAS_LOCALS | DYNAMIC | WP_TEXT | D_PAGED),
    (SEC_CODE|SEC_DATA|SEC_ROM|SEC_HAS_CONTENTS
     |SEC_ALLOC | SEC_LOAD | SEC_RELOC), /* section flags */
     0,				/* leading underscore */
    ' ',			/* ar_pad_char */
    16,				/* ar_max_namelen */
    1,				/* minimum alignment */
    bfd_getb64, bfd_getb_signed_64, bfd_putb64,
      bfd_getb32, bfd_getb_signed_32,     bfd_putb32,
      bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* data */
    bfd_getb64, bfd_getb_signed_64, bfd_putb64,
      bfd_getb32, bfd_getb_signed_32,     bfd_putb32,
      bfd_getb16, bfd_getb_signed_16, bfd_putb16, /* hdrs */

  {
      _bfd_dummy_target,
      symbolsrec_object_p,		/* bfd_check_format */
      (struct bfd_target *(*)()) bfd_nullvoidptr,
      (struct bfd_target *(*)())     bfd_nullvoidptr,
  },
  {
      bfd_false,
      srec_mkobject,
      _bfd_generic_mkarchive,
      bfd_false,
  },
  {				/* bfd_write_contents */
      bfd_false,
      symbolsrec_write_object_contents,
      _bfd_write_archive_contents,
      bfd_false,
  },
    JUMP_TABLE(srec),
    (PTR) 0
 };

