/* resbin.c -- manipulate the Windows binary resource format.
   Copyright 1997, 1998, 1999, 2002, 2003
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

   This file is part of GNU Binutils.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* This file contains functions to convert between the binary resource
   format and the internal structures that we want to use.  The same
   binary resource format is used in both res and COFF files.  */

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "windres.h"

/* Macros to swap in values.  */

#define get_8(s)      (*((unsigned char *)(s)))
#define get_16(be, s) ((be) ? bfd_getb16 (s) : bfd_getl16 (s))
#define get_32(be, s) ((be) ? bfd_getb32 (s) : bfd_getl32 (s))

/* Local functions.  */

static void toosmall (const char *);

static unichar *get_unicode
  (const unsigned char *, unsigned long, int, int *);
static int get_resid
  (struct res_id *, const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_generic
  (enum res_type, const unsigned char *, unsigned long);
static struct res_resource *bin_to_res_cursor
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_menu
  (const unsigned char *, unsigned long, int);
static struct menuitem *bin_to_res_menuitems
  (const unsigned char *, unsigned long, int, int *);
static struct menuitem *bin_to_res_menuexitems
  (const unsigned char *, unsigned long, int, int *);
static struct res_resource *bin_to_res_dialog
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_string
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_fontdir
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_accelerators
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_rcdata
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_group_cursor
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_group_icon
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_version
  (const unsigned char *, unsigned long, int);
static struct res_resource *bin_to_res_userdata
  (const unsigned char *, unsigned long, int);
static void get_version_header
  (const unsigned char *, unsigned long, int, const char *,
   unichar **, int *, int *, int *, int *);

/* Given a resource type ID, a pointer to data, a length, return a
   res_resource structure which represents that resource.  The caller
   is responsible for initializing the res_info and coff_info fields
   of the returned structure.  */

struct res_resource *
bin_to_res (struct res_id type, const unsigned char *data,
	    unsigned long length, int big_endian)
{
  if (type.named)
    return bin_to_res_userdata (data, length, big_endian);
  else
    {
      switch (type.u.id)
	{
	default:
	  return bin_to_res_userdata (data, length, big_endian);
	case RT_CURSOR:
	  return bin_to_res_cursor (data, length, big_endian);
	case RT_BITMAP:
	  return bin_to_res_generic (RES_TYPE_BITMAP, data, length);
	case RT_ICON:
	  return bin_to_res_generic (RES_TYPE_ICON, data, length);
	case RT_MENU:
	  return bin_to_res_menu (data, length, big_endian);
	case RT_DIALOG:
	  return bin_to_res_dialog (data, length, big_endian);
	case RT_STRING:
	  return bin_to_res_string (data, length, big_endian);
	case RT_FONTDIR:
	  return bin_to_res_fontdir (data, length, big_endian);
	case RT_FONT:
	  return bin_to_res_generic (RES_TYPE_FONT, data, length);
	case RT_ACCELERATOR:
	  return bin_to_res_accelerators (data, length, big_endian);
	case RT_RCDATA:
	  return bin_to_res_rcdata (data, length, big_endian);
	case RT_MESSAGETABLE:
	  return bin_to_res_generic (RES_TYPE_MESSAGETABLE, data, length);
	case RT_GROUP_CURSOR:
	  return bin_to_res_group_cursor (data, length, big_endian);
	case RT_GROUP_ICON:
	  return bin_to_res_group_icon (data, length, big_endian);
	case RT_VERSION:
	  return bin_to_res_version (data, length, big_endian);
	}
    }
}

/* Give an error if the binary data is too small.  */

static void
toosmall (const char *msg)
{
  fatal (_("%s: not enough binary data"), msg);
}

/* Swap in a NULL terminated unicode string.  */

static unichar *
get_unicode (const unsigned char *data, unsigned long length,
	     int big_endian, int *retlen)
{
  int c, i;
  unichar *ret;

  c = 0;
  while (1)
    {
      if (length < (unsigned long) c * 2 + 2)
	toosmall (_("null terminated unicode string"));
      if (get_16 (big_endian, data + c * 2) == 0)
	break;
      ++c;
    }

  ret = (unichar *) res_alloc ((c + 1) * sizeof (unichar));

  for (i = 0; i < c; i++)
    ret[i] = get_16 (big_endian, data + i * 2);
  ret[i] = 0;

  if (retlen != NULL)
    *retlen = c;

  return ret;
}

/* Get a resource identifier.  This returns the number of bytes used.  */

static int
get_resid (struct res_id *id, const unsigned char *data,
	   unsigned long length, int big_endian)
{
  int first;

  if (length < 2)
    toosmall (_("resource ID"));

  first = get_16 (big_endian, data);
  if (first == 0xffff)
    {
      if (length < 4)
	toosmall (_("resource ID"));
      id->named = 0;
      id->u.id = get_16 (big_endian, data + 2);
      return 4;
    }
  else
    {
      id->named = 1;
      id->u.n.name = get_unicode (data, length, big_endian, &id->u.n.length);
      return id->u.n.length * 2 + 2;
    }
}

/* Convert a resource which just stores uninterpreted data from
   binary.  */

struct res_resource *
bin_to_res_generic (enum res_type type, const unsigned char *data,
		    unsigned long length)
{
  struct res_resource *r;

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = type;
  r->u.data.data = data;
  r->u.data.length = length;

  return r;
}

/* Convert a cursor resource from binary.  */

struct res_resource *
bin_to_res_cursor (const unsigned char *data, unsigned long length,
		   int big_endian)
{
  struct cursor *c;
  struct res_resource *r;

  if (length < 4)
    toosmall (_("cursor"));

  c = (struct cursor *) res_alloc (sizeof *c);
  c->xhotspot = get_16 (big_endian, data);
  c->yhotspot = get_16 (big_endian, data + 2);
  c->length = length - 4;
  c->data = data + 4;

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_CURSOR;
  r->u.cursor = c;

  return r;
}

/* Convert a menu resource from binary.  */

struct res_resource *
bin_to_res_menu (const unsigned char *data, unsigned long length,
		 int big_endian)
{
  struct res_resource *r;
  struct menu *m;
  int version, read;

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_MENU;

  m = (struct menu *) res_alloc (sizeof *m);
  r->u.menu = m;

  if (length < 2)
    toosmall (_("menu header"));

  version = get_16 (big_endian, data);

  if (version == 0)
    {
      if (length < 4)
	toosmall (_("menu header"));
      m->help = 0;
      m->items = bin_to_res_menuitems (data + 4, length - 4, big_endian,
				       &read);
    }
  else if (version == 1)
    {
      unsigned int offset;

      if (length < 8)
	toosmall (_("menuex header"));
      m->help = get_32 (big_endian, data + 4);
      offset = get_16 (big_endian, data + 2);
      if (offset + 4 >= length)
	toosmall (_("menuex offset"));
      m->items = bin_to_res_menuexitems (data + 4 + offset,
					 length - (4 + offset),
					 big_endian,
					 &read);
    }
  else
    fatal (_("unsupported menu version %d"), version);

  return r;
}

/* Convert menu items from binary.  */

static struct menuitem *
bin_to_res_menuitems (const unsigned char *data, unsigned long length,
		      int big_endian, int *read)
{
  struct menuitem *first, **pp;

  first = NULL;
  pp = &first;

  *read = 0;

  while (length > 0)
    {
      int flags, slen, itemlen;
      unsigned int stroff;
      struct menuitem *mi;

      if (length < 4)
	toosmall (_("menuitem header"));

      mi = (struct menuitem *) res_alloc (sizeof *mi);
      mi->state = 0;
      mi->help = 0;

      flags = get_16 (big_endian, data);
      mi->type = flags &~ (MENUITEM_POPUP | MENUITEM_ENDMENU);

      if ((flags & MENUITEM_POPUP) == 0)
	stroff = 4;
      else
	stroff = 2;

      if (length < stroff + 2)
	toosmall (_("menuitem header"));

      if (get_16 (big_endian, data + stroff) == 0)
	{
	  slen = 0;
	  mi->text = NULL;
	}
      else
	mi->text = get_unicode (data + stroff, length - stroff, big_endian,
				&slen);

      itemlen = stroff + slen * 2 + 2;

      if ((flags & MENUITEM_POPUP) == 0)
	{
	  mi->popup = NULL;
	  mi->id = get_16 (big_endian, data + 2);
	}
      else
	{
	  int subread;

	  mi->id = 0;
	  mi->popup = bin_to_res_menuitems (data + itemlen, length - itemlen,
					    big_endian, &subread);
	  itemlen += subread;
	}

      mi->next = NULL;
      *pp = mi;
      pp = &mi->next;

      data += itemlen;
      length -= itemlen;
      *read += itemlen;

      if ((flags & MENUITEM_ENDMENU) != 0)
	return first;
    }

  return first;
}

/* Convert menuex items from binary.  */

static struct menuitem *
bin_to_res_menuexitems (const unsigned char *data, unsigned long length,
			int big_endian, int *read)
{
  struct menuitem *first, **pp;

  first = NULL;
  pp = &first;

  *read = 0;

  while (length > 0)
    {
      int flags, slen;
      unsigned int itemlen;
      struct menuitem *mi;

      if (length < 14)
	toosmall (_("menuitem header"));

      mi = (struct menuitem *) res_alloc (sizeof *mi);
      mi->type = get_32 (big_endian, data);
      mi->state = get_32 (big_endian, data + 4);
      mi->id = get_16 (big_endian, data + 8);

      flags = get_16 (big_endian, data + 10);

      if (get_16 (big_endian, data + 12) == 0)
	{
	  slen = 0;
	  mi->text = NULL;
	}
      else
	mi->text = get_unicode (data + 12, length - 12, big_endian, &slen);

      itemlen = 12 + slen * 2 + 2;
      itemlen = (itemlen + 3) &~ 3;

      if ((flags & 1) == 0)
	{
	  mi->popup = NULL;
	  mi->help = 0;
	}
      else
	{
	  int subread;

	  if (length < itemlen + 4)
	    toosmall (_("menuitem"));
	  mi->help = get_32 (big_endian, data + itemlen);
	  itemlen += 4;

	  mi->popup = bin_to_res_menuexitems (data + itemlen,
					      length - itemlen,
					      big_endian, &subread);
	  itemlen += subread;
	}

      mi->next = NULL;
      *pp = mi;
      pp = &mi->next;

      data += itemlen;
      length -= itemlen;
      *read += itemlen;

      if ((flags & 0x80) != 0)
	return first;
    }

  return first;
}

/* Convert a dialog resource from binary.  */

static struct res_resource *
bin_to_res_dialog (const unsigned char *data, unsigned long length,
		   int big_endian)
{
  int signature;
  struct dialog *d;
  int c, sublen, i;
  unsigned int off;
  struct dialog_control **pp;
  struct res_resource *r;

  if (length < 18)
    toosmall (_("dialog header"));

  d = (struct dialog *) res_alloc (sizeof *d);

  signature = get_16 (big_endian, data + 2);
  if (signature != 0xffff)
    {
      d->ex = NULL;
      d->style = get_32 (big_endian, data);
      d->exstyle = get_32 (big_endian, data + 4);
      off = 8;
    }
  else
    {
      int version;

      version = get_16 (big_endian, data);
      if (version != 1)
	fatal (_("unexpected DIALOGEX version %d"), version);

      d->ex = (struct dialog_ex *) res_alloc (sizeof (struct dialog_ex));
      d->ex->help = get_32 (big_endian, data + 4);
      d->exstyle = get_32 (big_endian, data + 8);
      d->style = get_32 (big_endian, data + 12);
      off = 16;
    }

  if (length < off + 10)
    toosmall (_("dialog header"));

  c = get_16 (big_endian, data + off);
  d->x = get_16  (big_endian, data + off + 2);
  d->y = get_16 (big_endian, data + off + 4);
  d->width = get_16 (big_endian, data + off + 6);
  d->height = get_16 (big_endian, data + off + 8);

  off += 10;

  sublen = get_resid (&d->menu, data + off, length - off, big_endian);
  off += sublen;

  sublen = get_resid (&d->class, data + off, length - off, big_endian);
  off += sublen;

  d->caption = get_unicode (data + off, length - off, big_endian, &sublen);
  off += sublen * 2 + 2;
  if (sublen == 0)
    d->caption = NULL;

  if ((d->style & DS_SETFONT) == 0)
    {
      d->pointsize = 0;
      d->font = NULL;
      if (d->ex != NULL)
	{
	  d->ex->weight = 0;
	  d->ex->italic = 0;
	  d->ex->charset = 1; /* Default charset.  */
	}
    }
  else
    {
      if (length < off + 2)
	toosmall (_("dialog font point size"));

      d->pointsize = get_16 (big_endian, data + off);
      off += 2;

      if (d->ex != NULL)
	{
	  if (length < off + 4)
	    toosmall (_("dialogex font information"));
	  d->ex->weight = get_16 (big_endian, data + off);
	  d->ex->italic = get_8 (data + off + 2);
	  d->ex->charset = get_8 (data + off + 3);
	  off += 4;
	}

      d->font = get_unicode (data + off, length - off, big_endian, &sublen);
      off += sublen * 2 + 2;
    }

  d->controls = NULL;
  pp = &d->controls;

  for (i = 0; i < c; i++)
    {
      struct dialog_control *dc;
      int datalen;

      off = (off + 3) &~ 3;

      dc = (struct dialog_control *) res_alloc (sizeof *dc);

      if (d->ex == NULL)
	{
	  if (length < off + 8)
	    toosmall (_("dialog control"));

	  dc->style = get_32 (big_endian, data + off);
	  dc->exstyle = get_32 (big_endian, data + off + 4);
	  dc->help = 0;
	  off += 8;
	}
      else
	{
	  if (length < off + 12)
	    toosmall (_("dialogex control"));
	  dc->help = get_32 (big_endian, data + off);
	  dc->exstyle = get_32 (big_endian, data + off + 4);
	  dc->style = get_32 (big_endian, data + off + 8);
	  off += 12;
	}

      if (length < off + 10)
	toosmall (_("dialog control"));

      dc->x = get_16 (big_endian, data + off);
      dc->y = get_16 (big_endian, data + off + 2);
      dc->width = get_16 (big_endian, data + off + 4);
      dc->height = get_16 (big_endian, data + off + 6);

      if (d->ex != NULL)
	dc->id = get_32 (big_endian, data + off + 8);
      else
	dc->id = get_16 (big_endian, data + off + 8);

      off += 10 + (d->ex != NULL ? 2 : 0);

      sublen = get_resid (&dc->class, data + off, length - off, big_endian);
      off += sublen;

      sublen = get_resid (&dc->text, data + off, length - off, big_endian);
      off += sublen;

      if (length < off + 2)
	toosmall (_("dialog control end"));

      datalen = get_16 (big_endian, data + off);
      off += 2;

      if (datalen == 0)
	dc->data = NULL;
      else
	{
	  off = (off + 3) &~ 3;

	  if (length < off + datalen)
	    toosmall (_("dialog control data"));

	  dc->data = ((struct rcdata_item *)
		      res_alloc (sizeof (struct rcdata_item)));
	  dc->data->next = NULL;
	  dc->data->type = RCDATA_BUFFER;
	  dc->data->u.buffer.length = datalen;
	  dc->data->u.buffer.data = data + off;

	  off += datalen;
	}

      dc->next = NULL;
      *pp = dc;
      pp = &dc->next;
    }

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_DIALOG;
  r->u.dialog = d;

  return r;
}

/* Convert a stringtable resource from binary.  */

static struct res_resource *
bin_to_res_string (const unsigned char *data, unsigned long length,
		   int big_endian)
{
  struct stringtable *st;
  int i;
  struct res_resource *r;

  st = (struct stringtable *) res_alloc (sizeof *st);

  for (i = 0; i < 16; i++)
    {
      unsigned int slen;

      if (length < 2)
	toosmall (_("stringtable string length"));
      slen = get_16 (big_endian, data);
      st->strings[i].length = slen;

      if (slen > 0)
	{
	  unichar *s;
	  unsigned int j;

	  if (length < 2 + 2 * slen)
	    toosmall (_("stringtable string"));

	  s = (unichar *) res_alloc (slen * sizeof (unichar));
	  st->strings[i].string = s;

	  for (j = 0; j < slen; j++)
	    s[j] = get_16 (big_endian, data + 2 + j * 2);
	}

      data += 2 + 2 * slen;
      length -= 2 + 2 * slen;
    }

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_STRINGTABLE;
  r->u.stringtable = st;

  return r;
}

/* Convert a fontdir resource from binary.  */

static struct res_resource *
bin_to_res_fontdir (const unsigned char *data, unsigned long length,
		    int big_endian)
{
  int c, i;
  struct fontdir *first, **pp;
  struct res_resource *r;

  if (length < 2)
    toosmall (_("fontdir header"));

  c = get_16 (big_endian, data);

  first = NULL;
  pp = &first;

  for (i = 0; i < c; i++)
    {
      struct fontdir *fd;
      unsigned int off;

      if (length < 56)
	toosmall (_("fontdir"));

      fd = (struct fontdir *) res_alloc (sizeof *fd);
      fd->index = get_16 (big_endian, data);

      /* To work out the length of the fontdir data, we must get the
         length of the device name and face name strings, even though
         we don't store them in the fontdir structure.  The
         documentation says that these are NULL terminated char
         strings, not Unicode strings.  */

      off = 56;

      while (off < length && data[off] != '\0')
	++off;
      if (off >= length)
	toosmall (_("fontdir device name"));
      ++off;

      while (off < length && data[off] != '\0')
	++off;
      if (off >= length)
	toosmall (_("fontdir face name"));
      ++off;

      fd->length = off;
      fd->data = data;

      fd->next = NULL;
      *pp = fd;
      pp = &fd->next;

      /* The documentation does not indicate that any rounding is
         required.  */

      data += off;
      length -= off;
    }

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_FONTDIR;
  r->u.fontdir = first;

  return r;
}

/* Convert an accelerators resource from binary.  */

static struct res_resource *
bin_to_res_accelerators (const unsigned char *data, unsigned long length,
			 int big_endian)
{
  struct accelerator *first, **pp;
  struct res_resource *r;

  first = NULL;
  pp = &first;

  while (1)
    {
      struct accelerator *a;

      if (length < 8)
	toosmall (_("accelerator"));

      a = (struct accelerator *) res_alloc (sizeof *a);

      a->flags = get_16 (big_endian, data);
      a->key = get_16 (big_endian, data + 2);
      a->id = get_16 (big_endian, data + 4);

      a->next = NULL;
      *pp = a;
      pp = &a->next;

      if ((a->flags & ACC_LAST) != 0)
	break;

      data += 8;
      length -= 8;
    }

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_ACCELERATOR;
  r->u.acc = first;

  return r;
}

/* Convert an rcdata resource from binary.  */

static struct res_resource *
bin_to_res_rcdata (const unsigned char *data, unsigned long length,
		   int big_endian ATTRIBUTE_UNUSED)
{
  struct rcdata_item *ri;
  struct res_resource *r;

  ri = (struct rcdata_item *) res_alloc (sizeof *ri);

  ri->next = NULL;
  ri->type = RCDATA_BUFFER;
  ri->u.buffer.length = length;
  ri->u.buffer.data = data;

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_RCDATA;
  r->u.rcdata = ri;

  return r;
}

/* Convert a group cursor resource from binary.  */

static struct res_resource *
bin_to_res_group_cursor (const unsigned char *data, unsigned long length,
			 int big_endian)
{
  int type, c, i;
  struct group_cursor *first, **pp;
  struct res_resource *r;

  if (length < 6)
    toosmall (_("group cursor header"));

  type = get_16 (big_endian, data + 2);
  if (type != 2)
    fatal (_("unexpected group cursor type %d"), type);

  c = get_16 (big_endian, data + 4);

  data += 6;
  length -= 6;

  first = NULL;
  pp = &first;

  for (i = 0; i < c; i++)
    {
      struct group_cursor *gc;

      if (length < 14)
	toosmall (_("group cursor"));

      gc = (struct group_cursor *) res_alloc (sizeof *gc);

      gc->width = get_16 (big_endian, data);
      gc->height = get_16 (big_endian, data + 2);
      gc->planes = get_16 (big_endian, data + 4);
      gc->bits = get_16 (big_endian, data + 6);
      gc->bytes = get_32 (big_endian, data + 8);
      gc->index = get_16 (big_endian, data + 12);

      gc->next = NULL;
      *pp = gc;
      pp = &gc->next;

      data += 14;
      length -= 14;
    }

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_GROUP_CURSOR;
  r->u.group_cursor = first;

  return r;
}

/* Convert a group icon resource from binary.  */

static struct res_resource *
bin_to_res_group_icon (const unsigned char *data, unsigned long length,
		       int big_endian)
{
  int type, c, i;
  struct group_icon *first, **pp;
  struct res_resource *r;

  if (length < 6)
    toosmall (_("group icon header"));

  type = get_16 (big_endian, data + 2);
  if (type != 1)
    fatal (_("unexpected group icon type %d"), type);

  c = get_16 (big_endian, data + 4);

  data += 6;
  length -= 6;

  first = NULL;
  pp = &first;

  for (i = 0; i < c; i++)
    {
      struct group_icon *gi;

      if (length < 14)
	toosmall (_("group icon"));

      gi = (struct group_icon *) res_alloc (sizeof *gi);

      gi->width = data[0];
      gi->height = data[1];
      gi->colors = data[2];
      gi->planes = get_16 (big_endian, data + 4);
      gi->bits = get_16 (big_endian, data + 6);
      gi->bytes = get_32 (big_endian, data + 8);
      gi->index = get_16 (big_endian, data + 12);

      gi->next = NULL;
      *pp = gi;
      pp = &gi->next;

      data += 14;
      length -= 14;
    }

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_GROUP_ICON;
  r->u.group_icon = first;

  return r;
}

/* Extract data from a version header.  If KEY is not NULL, then the
   key must be KEY; otherwise, the key is returned in *PKEY.  This
   sets *LEN to the total length, *VALLEN to the value length, *TYPE
   to the type, and *OFF to the offset to the children.  */

static void
get_version_header (const unsigned char *data, unsigned long length,
		    int big_endian, const char *key, unichar **pkey,
		    int *len, int *vallen, int *type, int *off)
{
  if (length < 8)
    toosmall (key);

  *len = get_16 (big_endian, data);
  *vallen = get_16 (big_endian, data + 2);
  *type = get_16 (big_endian, data + 4);

  *off = 6;

  length -= 6;
  data += 6;

  if (key == NULL)
    {
      int sublen;

      *pkey = get_unicode (data, length, big_endian, &sublen);
      *off += sublen * 2 + 2;
    }
  else
    {
      while (1)
	{
	  if (length < 2)
	    toosmall (key);
	  if (get_16 (big_endian, data) != (unsigned char) *key)
	    fatal (_("unexpected version string"));

	  *off += 2;
	  length -= 2;
	  data += 2;

	  if (*key == '\0')
	    break;

	  ++key;
	}
    }

  *off = (*off + 3) &~ 3;
}

/* Convert a version resource from binary.  */

static struct res_resource *
bin_to_res_version (const unsigned char *data, unsigned long length,
		    int big_endian)
{
  int verlen, vallen, type, off;
  struct fixed_versioninfo *fi;
  struct ver_info *first, **pp;
  struct versioninfo *v;
  struct res_resource *r;

  get_version_header (data, length, big_endian, "VS_VERSION_INFO",
		      (unichar **) NULL, &verlen, &vallen, &type, &off);

  if ((unsigned int) verlen != length)
    fatal (_("version length %d does not match resource length %lu"),
	   verlen, length);

  if (type != 0)
    fatal (_("unexpected version type %d"), type);

  data += off;
  length -= off;

  if (vallen == 0)
    fi = NULL;
  else
    {
      unsigned long signature, fiv;

      if (vallen != 52)
	fatal (_("unexpected fixed version information length %d"), vallen);

      if (length < 52)
	toosmall (_("fixed version info"));

      signature = get_32 (big_endian, data);
      if (signature != 0xfeef04bd)
	fatal (_("unexpected fixed version signature %lu"), signature);

      fiv = get_32 (big_endian, data + 4);
      if (fiv != 0 && fiv != 0x10000)
	fatal (_("unexpected fixed version info version %lu"), fiv);

      fi = (struct fixed_versioninfo *) res_alloc (sizeof *fi);

      fi->file_version_ms = get_32 (big_endian, data + 8);
      fi->file_version_ls = get_32 (big_endian, data + 12);
      fi->product_version_ms = get_32 (big_endian, data + 16);
      fi->product_version_ls = get_32 (big_endian, data + 20);
      fi->file_flags_mask = get_32 (big_endian, data + 24);
      fi->file_flags = get_32 (big_endian, data + 28);
      fi->file_os = get_32 (big_endian, data + 32);
      fi->file_type = get_32 (big_endian, data + 36);
      fi->file_subtype = get_32 (big_endian, data + 40);
      fi->file_date_ms = get_32 (big_endian, data + 44);
      fi->file_date_ls = get_32 (big_endian, data + 48);

      data += 52;
      length -= 52;
    }

  first = NULL;
  pp = &first;

  while (length > 0)
    {
      struct ver_info *vi;
      int ch;

      if (length < 8)
	toosmall (_("version var info"));

      vi = (struct ver_info *) res_alloc (sizeof *vi);

      ch = get_16 (big_endian, data + 6);

      if (ch == 'S')
	{
	  struct ver_stringinfo **ppvs;

	  vi->type = VERINFO_STRING;

	  get_version_header (data, length, big_endian, "StringFileInfo",
			      (unichar **) NULL, &verlen, &vallen, &type,
			      &off);

	  if (vallen != 0)
	    fatal (_("unexpected stringfileinfo value length %d"), vallen);

	  data += off;
	  length -= off;

	  get_version_header (data, length, big_endian, (const char *) NULL,
			      &vi->u.string.language, &verlen, &vallen,
			      &type, &off);

	  if (vallen != 0)
	    fatal (_("unexpected version stringtable value length %d"), vallen);

	  data += off;
	  length -= off;
	  verlen -= off;

	  vi->u.string.strings = NULL;
	  ppvs = &vi->u.string.strings;

	  /* It's convenient to round verlen to a 4 byte alignment,
             since we round the subvariables in the loop.  */
	  verlen = (verlen + 3) &~ 3;

	  while (verlen > 0)
	    {
	      struct ver_stringinfo *vs;
	      int subverlen, vslen, valoff;

	      vs = (struct ver_stringinfo *) res_alloc (sizeof *vs);

	      get_version_header (data, length, big_endian,
				  (const char *) NULL, &vs->key, &subverlen,
				  &vallen, &type, &off);

	      subverlen = (subverlen + 3) &~ 3;

	      data += off;
	      length -= off;

	      vs->value = get_unicode (data, length, big_endian, &vslen);
	      valoff = vslen * 2 + 2;
	      valoff = (valoff + 3) &~ 3;

	      if (off + valoff != subverlen)
		fatal (_("unexpected version string length %d != %d + %d"),
		       subverlen, off, valoff);

	      vs->next = NULL;
	      *ppvs = vs;
	      ppvs = &vs->next;

	      data += valoff;
	      length -= valoff;

	      if (verlen < subverlen)
		fatal (_("unexpected version string length %d < %d"),
		       verlen, subverlen);

	      verlen -= subverlen;
	    }
	}
      else if (ch == 'V')
	{
	  struct ver_varinfo **ppvv;

	  vi->type = VERINFO_VAR;

	  get_version_header (data, length, big_endian, "VarFileInfo",
			      (unichar **) NULL, &verlen, &vallen, &type,
			      &off);

	  if (vallen != 0)
	    fatal (_("unexpected varfileinfo value length %d"), vallen);

	  data += off;
	  length -= off;

	  get_version_header (data, length, big_endian, (const char *) NULL,
			      &vi->u.var.key, &verlen, &vallen, &type, &off);

	  data += off;
	  length -= off;

	  vi->u.var.var = NULL;
	  ppvv = &vi->u.var.var;

	  while (vallen > 0)
	    {
	      struct ver_varinfo *vv;

	      if (length < 4)
		toosmall (_("version varfileinfo"));

	      vv = (struct ver_varinfo *) res_alloc (sizeof *vv);

	      vv->language = get_16 (big_endian, data);
	      vv->charset = get_16 (big_endian, data + 2);

	      vv->next = NULL;
	      *ppvv = vv;
	      ppvv = &vv->next;

	      data += 4;
	      length -= 4;

	      if (vallen < 4)
		fatal (_("unexpected version value length %d"), vallen);

	      vallen -= 4;
	    }
	}
      else
	fatal (_("unexpected version string"));

      vi->next = NULL;
      *pp = vi;
      pp = &vi->next;
    }

  v = (struct versioninfo *) res_alloc (sizeof *v);
  v->fixed = fi;
  v->var = first;

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_VERSIONINFO;
  r->u.versioninfo = v;

  return r;
}

/* Convert an arbitrary user defined resource from binary.  */

static struct res_resource *
bin_to_res_userdata (const unsigned char *data, unsigned long length,
		     int big_endian ATTRIBUTE_UNUSED)
{
  struct rcdata_item *ri;
  struct res_resource *r;

  ri = (struct rcdata_item *) res_alloc (sizeof *ri);

  ri->next = NULL;
  ri->type = RCDATA_BUFFER;
  ri->u.buffer.length = length;
  ri->u.buffer.data = data;

  r = (struct res_resource *) res_alloc (sizeof *r);
  r->type = RES_TYPE_USERDATA;
  r->u.rcdata = ri;

  return r;
}

/* Macros to swap out values.  */

#define put_8(v, s)      (*((unsigned char *) (s)) = (unsigned char) (v))
#define put_16(be, v, s) ((be) ? bfd_putb16 ((v), (s)) : bfd_putl16 ((v), (s)))
#define put_32(be, v, s) ((be) ? bfd_putb32 ((v), (s)) : bfd_putl32 ((v), (s)))

/* Local functions used to convert resources to binary format.  */

static void dword_align_bin (struct bindata ***, unsigned long *);
static struct bindata *resid_to_bin (struct res_id, int);
static struct bindata *unicode_to_bin (const unichar *, int);
static struct bindata *res_to_bin_accelerator
  (const struct accelerator *, int);
static struct bindata *res_to_bin_cursor
  (const struct cursor *, int);
static struct bindata *res_to_bin_group_cursor
  (const struct group_cursor *, int);
static struct bindata *res_to_bin_dialog
  (const struct dialog *, int);
static struct bindata *res_to_bin_fontdir
  (const struct fontdir *, int);
static struct bindata *res_to_bin_group_icon
  (const struct group_icon *, int);
static struct bindata *res_to_bin_menu
  (const struct menu *, int);
static struct bindata *res_to_bin_menuitems
  (const struct menuitem *, int);
static struct bindata *res_to_bin_menuexitems
  (const struct menuitem *, int);
static struct bindata *res_to_bin_rcdata
  (const struct rcdata_item *, int);
static struct bindata *res_to_bin_stringtable
  (const struct stringtable *, int);
static struct bindata *string_to_unicode_bin (const char *, int);
static struct bindata *res_to_bin_versioninfo
  (const struct versioninfo *, int);
static struct bindata *res_to_bin_generic
  (unsigned long, const unsigned char *);

/* Convert a resource to binary.  */

struct bindata *
res_to_bin (const struct res_resource *res, int big_endian)
{
  switch (res->type)
    {
    default:
      abort ();
    case RES_TYPE_BITMAP:
    case RES_TYPE_FONT:
    case RES_TYPE_ICON:
    case RES_TYPE_MESSAGETABLE:
      return res_to_bin_generic (res->u.data.length, res->u.data.data);
    case RES_TYPE_ACCELERATOR:
      return res_to_bin_accelerator (res->u.acc, big_endian);
    case RES_TYPE_CURSOR:
      return res_to_bin_cursor (res->u.cursor, big_endian);
    case RES_TYPE_GROUP_CURSOR:
      return res_to_bin_group_cursor (res->u.group_cursor, big_endian);
    case RES_TYPE_DIALOG:
      return res_to_bin_dialog (res->u.dialog, big_endian);
    case RES_TYPE_FONTDIR:
      return res_to_bin_fontdir (res->u.fontdir, big_endian);
    case RES_TYPE_GROUP_ICON:
      return res_to_bin_group_icon (res->u.group_icon, big_endian);
    case RES_TYPE_MENU:
      return res_to_bin_menu (res->u.menu, big_endian);
    case RES_TYPE_RCDATA:
      return res_to_bin_rcdata (res->u.rcdata, big_endian);
    case RES_TYPE_STRINGTABLE:
      return res_to_bin_stringtable (res->u.stringtable, big_endian);
    case RES_TYPE_USERDATA:
      return res_to_bin_rcdata (res->u.rcdata, big_endian);
    case RES_TYPE_VERSIONINFO:
      return res_to_bin_versioninfo (res->u.versioninfo, big_endian);
    }
}

/* Align to a 32 bit boundary.  PPP points to the of a list of bindata
   structures.  LENGTH points to the length of the structures.  If
   necessary, this adds a new bindata to bring length up to a 32 bit
   boundary.  It updates *PPP and *LENGTH.  */

static void
dword_align_bin (struct bindata ***ppp, unsigned long *length)
{
  int add;
  struct bindata *d;

  if ((*length & 3) == 0)
    return;

  add = 4 - (*length & 3);

  d = (struct bindata *) reswr_alloc (sizeof *d);
  d->length = add;
  d->data = (unsigned char *) reswr_alloc (add);
  memset (d->data, 0, add);

  d->next = NULL;
  **ppp = d;
  *ppp = &(**ppp)->next;

  *length += add;
}

/* Convert a resource ID to binary.  This always returns exactly one
   bindata structure.  */

static struct bindata *
resid_to_bin (struct res_id id, int big_endian)
{
  struct bindata *d;

  d = (struct bindata *) reswr_alloc (sizeof *d);

  if (! id.named)
    {
      d->length = 4;
      d->data = (unsigned char *) reswr_alloc (4);
      put_16 (big_endian, 0xffff, d->data);
      put_16 (big_endian, id.u.id, d->data + 2);
    }
  else
    {
      int i;

      d->length = id.u.n.length * 2 + 2;
      d->data = (unsigned char *) reswr_alloc (d->length);
      for (i = 0; i < id.u.n.length; i++)
	put_16 (big_endian, id.u.n.name[i], d->data + i * 2);
      put_16 (big_endian, 0, d->data + i * 2);
    }

  d->next = NULL;

  return d;
}

/* Convert a null terminated unicode string to binary.  This always
   returns exactly one bindata structure.  */

static struct bindata *
unicode_to_bin (const unichar *str, int big_endian)
{
  int len;
  struct bindata *d;

  len = 0;
  if (str != NULL)
    {
      const unichar *s;

      for (s = str; *s != 0; s++)
	++len;
    }

  d = (struct bindata *) reswr_alloc (sizeof *d);
  d->length = len * 2 + 2;
  d->data = (unsigned char *) reswr_alloc (d->length);

  if (str == NULL)
    put_16 (big_endian, 0, d->data);
  else
    {
      const unichar *s;
      int i;

      for (s = str, i = 0; *s != 0; s++, i++)
	put_16 (big_endian, *s, d->data + i * 2);
      put_16 (big_endian, 0, d->data + i * 2);
    }

  d->next = NULL;

  return d;
}

/* Convert an accelerator resource to binary.  */

static struct bindata *
res_to_bin_accelerator (const struct accelerator *accelerators,
			int big_endian)
{
  struct bindata *first, **pp;
  const struct accelerator *a;

  first = NULL;
  pp = &first;

  for (a = accelerators; a != NULL; a = a->next)
    {
      struct bindata *d;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = 8;
      d->data = (unsigned char *) reswr_alloc (8);

      put_16 (big_endian,
	      a->flags | (a->next != NULL ? 0 : ACC_LAST),
	      d->data);
      put_16 (big_endian, a->key, d->data + 2);
      put_16 (big_endian, a->id, d->data + 4);
      put_16 (big_endian, 0, d->data + 8);

      d->next = NULL;
      *pp = d;
      pp = &d->next;
    }

  return first;
}

/* Convert a cursor resource to binary.  */

static struct bindata *
res_to_bin_cursor (const struct cursor *c, int big_endian)
{
  struct bindata *d;

  d = (struct bindata *) reswr_alloc (sizeof *d);
  d->length = 4;
  d->data = (unsigned char *) reswr_alloc (4);

  put_16 (big_endian, c->xhotspot, d->data);
  put_16 (big_endian, c->yhotspot, d->data + 2);

  d->next = (struct bindata *) reswr_alloc (sizeof *d);
  d->next->length = c->length;
  d->next->data = (unsigned char *) c->data;
  d->next->next = NULL;

  return d;
}

/* Convert a group cursor resource to binary.  */

static struct bindata *
res_to_bin_group_cursor (const struct group_cursor *group_cursors,
			 int big_endian)
{
  struct bindata *first, **pp;
  int c;
  const struct group_cursor *gc;

  first = (struct bindata *) reswr_alloc (sizeof *first);
  first->length = 6;
  first->data = (unsigned char *) reswr_alloc (6);

  put_16 (big_endian, 0, first->data);
  put_16 (big_endian, 2, first->data + 2);

  first->next = NULL;
  pp = &first->next;

  c = 0;
  for (gc = group_cursors; gc != NULL; gc = gc->next)
    {
      struct bindata *d;

      ++c;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = 14;
      d->data = (unsigned char *) reswr_alloc (14);

      put_16 (big_endian, gc->width, d->data);
      put_16 (big_endian, gc->height, d->data + 2);
      put_16 (big_endian, gc->planes, d->data + 4);
      put_16 (big_endian, gc->bits, d->data + 6);
      put_32 (big_endian, gc->bytes, d->data + 8);
      put_16 (big_endian, gc->index, d->data + 12);

      d->next = NULL;
      *pp = d;
      pp = &d->next;
    }

  put_16 (big_endian, c, first->data + 4);

  return first;
}

/* Convert a dialog resource to binary.  */

static struct bindata *
res_to_bin_dialog (const struct dialog *dialog, int big_endian)
{
  int dialogex;
  struct bindata *first, **pp;
  unsigned long length;
  int off, c;
  struct dialog_control *dc;

  dialogex = extended_dialog (dialog);

  first = (struct bindata *) reswr_alloc (sizeof *first);
  first->length = dialogex ? 26 : 18;
  first->data = (unsigned char *) reswr_alloc (first->length);

  length = first->length;

  if (! dialogex)
    {
      put_32 (big_endian, dialog->style, first->data);
      put_32 (big_endian, dialog->exstyle, first->data + 4);
      off = 8;
    }
  else
    {
      put_16 (big_endian, 1, first->data);
      put_16 (big_endian, 0xffff, first->data + 2);

      if (dialog->ex == NULL)
	put_32 (big_endian, 0, first->data + 4);
      else
	put_32 (big_endian, dialog->ex->help, first->data + 4);
      put_32 (big_endian, dialog->exstyle, first->data + 8);
      put_32 (big_endian, dialog->style, first->data + 12);
      off = 16;
    }

  put_16 (big_endian, dialog->x, first->data + off + 2);
  put_16 (big_endian, dialog->y, first->data + off + 4);
  put_16 (big_endian, dialog->width, first->data + off + 6);
  put_16 (big_endian, dialog->height, first->data + off + 8);

  pp = &first->next;

  *pp = resid_to_bin (dialog->menu, big_endian);
  length += (*pp)->length;
  pp = &(*pp)->next;

  *pp = resid_to_bin (dialog->class, big_endian);
  length += (*pp)->length;
  pp = &(*pp)->next;

  *pp = unicode_to_bin (dialog->caption, big_endian);
  length += (*pp)->length;
  pp = &(*pp)->next;

  if ((dialog->style & DS_SETFONT) != 0)
    {
      struct bindata *d;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = dialogex ? 6 : 2;
      d->data = (unsigned char *) reswr_alloc (d->length);

      length += d->length;

      put_16 (big_endian, dialog->pointsize, d->data);

      if (dialogex)
	{
	  if (dialog->ex == NULL)
	    {
	      put_16 (big_endian, 0, d->data + 2);
	      put_8 (0, d->data + 4);
	      put_8 (1, d->data + 5);
	    }
	  else
	    {
	      put_16 (big_endian, dialog->ex->weight, d->data + 2);
	      put_8 (dialog->ex->italic, d->data + 4);
	      put_8 (dialog->ex->charset, d->data + 5);
	    }
	}

      *pp = d;
      pp = &d->next;

      *pp = unicode_to_bin (dialog->font, big_endian);
      length += (*pp)->length;
      pp = &(*pp)->next;
    }

  c = 0;
  for (dc = dialog->controls; dc != NULL; dc = dc->next)
    {
      struct bindata *d;
      int dcoff;

      ++c;

      dword_align_bin (&pp, &length);

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = dialogex ? 24 : 18;
      d->data = (unsigned char *) reswr_alloc (d->length);

      length += d->length;

      if (! dialogex)
	{
	  put_32 (big_endian, dc->style, d->data);
	  put_32 (big_endian, dc->exstyle, d->data + 4);
	  dcoff = 8;
	}
      else
	{
	  put_32 (big_endian, dc->help, d->data);
	  put_32 (big_endian, dc->exstyle, d->data + 4);
	  put_32 (big_endian, dc->style, d->data + 8);
	  dcoff = 12;
	}

      put_16 (big_endian, dc->x, d->data + dcoff);
      put_16 (big_endian, dc->y, d->data + dcoff + 2);
      put_16 (big_endian, dc->width, d->data + dcoff + 4);
      put_16 (big_endian, dc->height, d->data + dcoff + 6);

      if (dialogex)
	put_32 (big_endian, dc->id, d->data + dcoff + 8);
      else
	put_16 (big_endian, dc->id, d->data + dcoff + 8);

      *pp = d;
      pp = &d->next;

      *pp = resid_to_bin (dc->class, big_endian);
      length += (*pp)->length;
      pp = &(*pp)->next;

      *pp = resid_to_bin (dc->text, big_endian);
      length += (*pp)->length;
      pp = &(*pp)->next;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = 2;
      d->data = (unsigned char *) reswr_alloc (2);

      length += 2;

      d->next = NULL;
      *pp = d;
      pp = &d->next;

      if (dc->data == NULL)
	put_16 (big_endian, 0, d->data);
      else
	{
	  unsigned long sublen;

	  dword_align_bin (&pp, &length);

	  *pp = res_to_bin_rcdata (dc->data, big_endian);
	  sublen = 0;
	  while (*pp != NULL)
	    {
	      sublen += (*pp)->length;
	      pp = &(*pp)->next;
	    }

	  put_16 (big_endian, sublen, d->data);

	  length += sublen;
	}
    }
  put_16 (big_endian, c, first->data + off);

  return first;
}

/* Convert a fontdir resource to binary.  */

static struct bindata *
res_to_bin_fontdir (const struct fontdir *fontdirs, int big_endian)
{
  struct bindata *first, **pp;
  int c;
  const struct fontdir *fd;

  first = (struct bindata *) reswr_alloc (sizeof *first);
  first->length = 2;
  first->data = (unsigned char *) reswr_alloc (2);

  first->next = NULL;
  pp = &first->next;

  c = 0;
  for (fd = fontdirs; fd != NULL; fd = fd->next)
    {
      struct bindata *d;

      ++c;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = 2;
      d->data = (unsigned char *) reswr_alloc (2);

      put_16 (big_endian, fd->index, d->data);

      *pp = d;
      pp = &d->next;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = fd->length;
      d->data = (unsigned char *) fd->data;

      d->next = NULL;
      *pp = d;
      pp = &d->next;
    }

  put_16 (big_endian, c, first->data);

  return first;
}

/* Convert a group icon resource to binary.  */

static struct bindata *
res_to_bin_group_icon (const struct group_icon *group_icons, int big_endian)
{
  struct bindata *first, **pp;
  int c;
  const struct group_icon *gi;

  first = (struct bindata *) reswr_alloc (sizeof *first);
  first->length = 6;
  first->data = (unsigned char *) reswr_alloc (6);

  put_16 (big_endian, 0, first->data);
  put_16 (big_endian, 1, first->data + 2);

  first->next = NULL;
  pp = &first->next;

  c = 0;
  for (gi = group_icons; gi != NULL; gi = gi->next)
    {
      struct bindata *d;

      ++c;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = 14;
      d->data = (unsigned char *) reswr_alloc (14);

      d->data[0] = gi->width;
      d->data[1] = gi->height;
      d->data[2] = gi->colors;
      d->data[3] = 0;
      put_16 (big_endian, gi->planes, d->data + 4);
      put_16 (big_endian, gi->bits, d->data + 6);
      put_32 (big_endian, gi->bytes, d->data + 8);
      put_16 (big_endian, gi->index, d->data + 12);

      d->next = NULL;
      *pp = d;
      pp = &d->next;
    }

  put_16 (big_endian, c, first->data + 4);

  return first;
}

/* Convert a menu resource to binary.  */

static struct bindata *
res_to_bin_menu (const struct menu *menu, int big_endian)
{
  int menuex;
  struct bindata *d;

  menuex = extended_menu (menu);

  d = (struct bindata *) reswr_alloc (sizeof *d);
  d->length = menuex ? 8 : 4;
  d->data = (unsigned char *) reswr_alloc (d->length);

  if (! menuex)
    {
      put_16 (big_endian, 0, d->data);
      put_16 (big_endian, 0, d->data + 2);

      d->next = res_to_bin_menuitems (menu->items, big_endian);
    }
  else
    {
      put_16 (big_endian, 1, d->data);
      put_16 (big_endian, 4, d->data + 2);
      put_32 (big_endian, menu->help, d->data + 4);

      d->next = res_to_bin_menuexitems (menu->items, big_endian);
    }

  return d;
}

/* Convert menu items to binary.  */

static struct bindata *
res_to_bin_menuitems (const struct menuitem *items, int big_endian)
{
  struct bindata *first, **pp;
  const struct menuitem *mi;

  first = NULL;
  pp = &first;

  for (mi = items; mi != NULL; mi = mi->next)
    {
      struct bindata *d;
      int flags;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = mi->popup == NULL ? 4 : 2;
      d->data = (unsigned char *) reswr_alloc (d->length);

      flags = mi->type;
      if (mi->next == NULL)
	flags |= MENUITEM_ENDMENU;
      if (mi->popup != NULL)
	flags |= MENUITEM_POPUP;

      put_16 (big_endian, flags, d->data);

      if (mi->popup == NULL)
	put_16 (big_endian, mi->id, d->data + 2);

      *pp = d;
      pp = &d->next;

      *pp = unicode_to_bin (mi->text, big_endian);
      pp = &(*pp)->next;

      if (mi->popup != NULL)
	{
	  *pp = res_to_bin_menuitems (mi->popup, big_endian);
	  while (*pp != NULL)
	    pp = &(*pp)->next;
	}
    }

  return first;
}

/* Convert menuex items to binary.  */

static struct bindata *
res_to_bin_menuexitems (const struct menuitem *items, int big_endian)
{
  struct bindata *first, **pp;
  unsigned long length;
  const struct menuitem *mi;

  first = NULL;
  pp = &first;

  length = 0;

  for (mi = items; mi != NULL; mi = mi->next)
    {
      struct bindata *d;
      int flags;

      dword_align_bin (&pp, &length);

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = 12;
      d->data = (unsigned char *) reswr_alloc (12);

      length += 12;

      put_32 (big_endian, mi->type, d->data);
      put_32 (big_endian, mi->state, d->data + 4);
      put_16 (big_endian, mi->id, d->data + 8);

      flags = 0;
      if (mi->next == NULL)
	flags |= 0x80;
      if (mi->popup != NULL)
	flags |= 1;
      put_16 (big_endian, flags, d->data + 10);

      *pp = d;
      pp = &d->next;

      *pp = unicode_to_bin (mi->text, big_endian);
      length += (*pp)->length;
      pp = &(*pp)->next;

      if (mi->popup != NULL)
	{
	  dword_align_bin (&pp, &length);

	  d = (struct bindata *) reswr_alloc (sizeof *d);
	  d->length = 4;
	  d->data = (unsigned char *) reswr_alloc (4);

	  put_32 (big_endian, mi->help, d->data);

	  *pp = d;
	  pp = &d->next;

	  *pp = res_to_bin_menuexitems (mi->popup, big_endian);
	  while (*pp != NULL)
	    {
	      length += (*pp)->length;
	      pp = &(*pp)->next;
	    }
	}
    }

  return first;
}

/* Convert an rcdata resource to binary.  This is also used to convert
   other information which happens to be stored in rcdata_item lists
   to binary.  */

static struct bindata *
res_to_bin_rcdata (const struct rcdata_item *items, int big_endian)
{
  struct bindata *first, **pp;
  const struct rcdata_item *ri;

  first = NULL;
  pp = &first;

  for (ri = items; ri != NULL; ri = ri->next)
    {
      struct bindata *d;

      d = (struct bindata *) reswr_alloc (sizeof *d);

      switch (ri->type)
	{
	default:
	  abort ();

	case RCDATA_WORD:
	  d->length = 2;
	  d->data = (unsigned char *) reswr_alloc (2);
	  put_16 (big_endian, ri->u.word, d->data);
	  break;

	case RCDATA_DWORD:
	  d->length = 4;
	  d->data = (unsigned char *) reswr_alloc (4);
	  put_32 (big_endian, ri->u.dword, d->data);
	  break;

	case RCDATA_STRING:
	  d->length = ri->u.string.length;
	  d->data = (unsigned char *) ri->u.string.s;
	  break;

	case RCDATA_WSTRING:
	  {
	    unsigned long i;

	    d->length = ri->u.wstring.length * 2;
	    d->data = (unsigned char *) reswr_alloc (d->length);
	    for (i = 0; i < ri->u.wstring.length; i++)
	      put_16 (big_endian, ri->u.wstring.w[i], d->data + i * 2);
	    break;
	  }

	case RCDATA_BUFFER:
	  d->length = ri->u.buffer.length;
	  d->data = (unsigned char *) ri->u.buffer.data;
	  break;
	}

      d->next = NULL;
      *pp = d;
      pp = &d->next;
    }

  return first;
}

/* Convert a stringtable resource to binary.  */

static struct bindata *
res_to_bin_stringtable (const struct stringtable *st, int big_endian)
{
  struct bindata *first, **pp;
  int i;

  first = NULL;
  pp = &first;

  for (i = 0; i < 16; i++)
    {
      int slen, j;
      struct bindata *d;
      unichar *s;

      slen = st->strings[i].length;
      s = st->strings[i].string;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = 2 + slen * 2;
      d->data = (unsigned char *) reswr_alloc (d->length);

      put_16 (big_endian, slen, d->data);

      for (j = 0; j < slen; j++)
	put_16 (big_endian, s[j], d->data + 2 + j * 2);

      d->next = NULL;
      *pp = d;
      pp = &d->next;
    }

  return first;
}

/* Convert an ASCII string to a unicode binary string.  This always
   returns exactly one bindata structure.  */

static struct bindata *
string_to_unicode_bin (const char *s, int big_endian)
{
  size_t len, i;
  struct bindata *d;

  len = strlen (s);

  d = (struct bindata *) reswr_alloc (sizeof *d);
  d->length = len * 2 + 2;
  d->data = (unsigned char *) reswr_alloc (d->length);

  for (i = 0; i < len; i++)
    put_16 (big_endian, s[i], d->data + i * 2);
  put_16 (big_endian, 0, d->data + i * 2);

  d->next = NULL;

  return d;
}

/* Convert a versioninfo resource to binary.  */

static struct bindata *
res_to_bin_versioninfo (const struct versioninfo *versioninfo, int big_endian)
{
  struct bindata *first, **pp;
  unsigned long length;
  struct ver_info *vi;

  first = (struct bindata *) reswr_alloc (sizeof *first);
  first->length = 6;
  first->data = (unsigned char *) reswr_alloc (6);

  length = 6;

  if (versioninfo->fixed == NULL)
    put_16 (big_endian, 0, first->data + 2);
  else
    put_16 (big_endian, 52, first->data + 2);

  put_16 (big_endian, 0, first->data + 4);

  pp = &first->next;

  *pp = string_to_unicode_bin ("VS_VERSION_INFO", big_endian);
  length += (*pp)->length;
  pp = &(*pp)->next;

  dword_align_bin (&pp, &length);

  if (versioninfo->fixed != NULL)
    {
      const struct fixed_versioninfo *fi;
      struct bindata *d;

      d = (struct bindata *) reswr_alloc (sizeof *d);
      d->length = 52;
      d->data = (unsigned char *) reswr_alloc (52);

      length += 52;

      fi = versioninfo->fixed;

      put_32 (big_endian, 0xfeef04bd, d->data);
      put_32 (big_endian, 0x10000, d->data + 4);
      put_32 (big_endian, fi->file_version_ms, d->data + 8);
      put_32 (big_endian, fi->file_version_ls, d->data + 12);
      put_32 (big_endian, fi->product_version_ms, d->data + 16);
      put_32 (big_endian, fi->product_version_ls, d->data + 20);
      put_32 (big_endian, fi->file_flags_mask, d->data + 24);
      put_32 (big_endian, fi->file_flags, d->data + 28);
      put_32 (big_endian, fi->file_os, d->data + 32);
      put_32 (big_endian, fi->file_type, d->data + 36);
      put_32 (big_endian, fi->file_subtype, d->data + 40);
      put_32 (big_endian, fi->file_date_ms, d->data + 44);
      put_32 (big_endian, fi->file_date_ls, d->data + 48);

      d->next = NULL;
      *pp = d;
      pp = &d->next;
    }

  for (vi = versioninfo->var; vi != NULL; vi = vi->next)
    {
      struct bindata *vid;
      unsigned long vilen;

      dword_align_bin (&pp, &length);

      vid = (struct bindata *) reswr_alloc (sizeof *vid);
      vid->length = 6;
      vid->data = (unsigned char *) reswr_alloc (6);

      length += 6;
      vilen = 6;

      put_16 (big_endian, 0, vid->data + 2);
      put_16 (big_endian, 0, vid->data + 4);

      *pp = vid;
      pp = &vid->next;

      switch (vi->type)
	{
	default:
	  abort ();

	case VERINFO_STRING:
	  {
	    unsigned long hold, vslen;
	    struct bindata *vsd;
	    const struct ver_stringinfo *vs;

	    *pp = string_to_unicode_bin ("StringFileInfo", big_endian);
	    length += (*pp)->length;
	    vilen += (*pp)->length;
	    pp = &(*pp)->next;

	    hold = length;
	    dword_align_bin (&pp, &length);
	    vilen += length - hold;

	    vsd = (struct bindata *) reswr_alloc (sizeof *vsd);
	    vsd->length = 6;
	    vsd->data = (unsigned char *) reswr_alloc (6);

	    length += 6;
	    vilen += 6;
	    vslen = 6;

	    put_16 (big_endian, 0, vsd->data + 2);
	    put_16 (big_endian, 0, vsd->data + 4);

	    *pp = vsd;
	    pp = &vsd->next;

	    *pp = unicode_to_bin (vi->u.string.language, big_endian);
	    length += (*pp)->length;
	    vilen += (*pp)->length;
	    vslen += (*pp)->length;
	    pp = &(*pp)->next;

	    for (vs = vi->u.string.strings; vs != NULL; vs = vs->next)
	      {
		struct bindata *vssd;
		unsigned long vsslen;

		hold = length;
		dword_align_bin (&pp, &length);
		vilen += length - hold;
		vslen += length - hold;

		vssd = (struct bindata *) reswr_alloc (sizeof *vssd);
		vssd->length = 6;
		vssd->data = (unsigned char *) reswr_alloc (6);

		length += 6;
		vilen += 6;
		vslen += 6;
		vsslen = 6;

		put_16 (big_endian, 1, vssd->data + 4);

		*pp = vssd;
		pp = &vssd->next;

		*pp = unicode_to_bin (vs->key, big_endian);
		length += (*pp)->length;
		vilen += (*pp)->length;
		vslen += (*pp)->length;
		vsslen += (*pp)->length;
		pp = &(*pp)->next;

		hold = length;
		dword_align_bin (&pp, &length);
		vilen += length - hold;
		vslen += length - hold;
		vsslen += length - hold;

		*pp = unicode_to_bin (vs->value, big_endian);
		put_16 (big_endian, (*pp)->length / 2, vssd->data + 2);
		length += (*pp)->length;
		vilen += (*pp)->length;
		vslen += (*pp)->length;
		vsslen += (*pp)->length;
		pp = &(*pp)->next;

		put_16 (big_endian, vsslen, vssd->data);
	      }

	    put_16 (big_endian, vslen, vsd->data);

	    break;
	  }

	case VERINFO_VAR:
	  {
	    unsigned long hold, vvlen, vvvlen;
	    struct bindata *vvd;
	    const struct ver_varinfo *vv;

	    *pp = string_to_unicode_bin ("VarFileInfo", big_endian);
	    length += (*pp)->length;
	    vilen += (*pp)->length;
	    pp = &(*pp)->next;

	    hold = length;
	    dword_align_bin (&pp, &length);
	    vilen += length - hold;

	    vvd = (struct bindata *) reswr_alloc (sizeof *vvd);
	    vvd->length = 6;
	    vvd->data = (unsigned char *) reswr_alloc (6);

	    length += 6;
	    vilen += 6;
	    vvlen = 6;

	    put_16 (big_endian, 0, vvd->data + 4);

	    *pp = vvd;
	    pp = &vvd->next;

	    *pp = unicode_to_bin (vi->u.var.key, big_endian);
	    length += (*pp)->length;
	    vilen += (*pp)->length;
	    vvlen += (*pp)->length;
	    pp = &(*pp)->next;

	    hold = length;
	    dword_align_bin (&pp, &length);
	    vilen += length - hold;
	    vvlen += length - hold;

	    vvvlen = 0;

	    for (vv = vi->u.var.var; vv != NULL; vv = vv->next)
	      {
		struct bindata *vvsd;

		vvsd = (struct bindata *) reswr_alloc (sizeof *vvsd);
		vvsd->length = 4;
		vvsd->data = (unsigned char *) reswr_alloc (4);

		length += 4;
		vilen += 4;
		vvlen += 4;
		vvvlen += 4;

		put_16 (big_endian, vv->language, vvsd->data);
		put_16 (big_endian, vv->charset, vvsd->data + 2);

		vvsd->next = NULL;
		*pp = vvsd;
		pp = &vvsd->next;
	      }

	    put_16 (big_endian, vvlen, vvd->data);
	    put_16 (big_endian, vvvlen, vvd->data + 2);

	    break;
	  }
	}

      put_16 (big_endian, vilen, vid->data);
    }

  put_16 (big_endian, length, first->data);

  return first;
}

/* Convert a generic resource to binary.  */

static struct bindata *
res_to_bin_generic (unsigned long length, const unsigned char *data)
{
  struct bindata *d;

  d = (struct bindata *) reswr_alloc (sizeof *d);
  d->length = length;
  d->data = (unsigned char *) data;

  d->next = NULL;

  return d;
}
