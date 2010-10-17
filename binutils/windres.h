/* windres.h -- header file for windres program.
   Copyright 1997, 1998, 2000, 2002, 2003 Free Software Foundation, Inc.
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

#include "ansidecl.h"

/* This is the header file for the windres program.  It defines
   structures and declares functions used within the program.  */

#include "winduni.h"

/* We represent resources internally as a tree, similar to the tree
   used in the .rsrc section of a COFF file.  The root is a
   res_directory structure.  */

struct res_directory
{
  /* Resource flags.  According to the MS docs, this is currently
     always zero.  */
  unsigned long characteristics;
  /* Time/date stamp.  */
  unsigned long time;
  /* Major version number.  */
  unsigned short major;
  /* Minor version number.  */
  unsigned short minor;
  /* Directory entries.  */
  struct res_entry *entries;
};

/* A resource ID is stored in a res_id structure.  */

struct res_id
{
  /* Non-zero if this entry has a name rather than an ID.  */
  unsigned int named : 1;
  union
  {
    /* If the named field is non-zero, this is the name.  */
    struct
    {
      /* Length of the name.  */
      int length;
      /* Pointer to the name, which is a Unicode string.  */
      unichar *name;
    } n;
    /* If the named field is zero, this is the ID.  */
    unsigned long id;
  } u;
};

/* Each entry in the tree is a res_entry structure.  We mix
   directories and resources because in a COFF file all entries in a
   directory are sorted together, whether the entries are
   subdirectories or resources.  */

struct res_entry
{
  /* Next entry.  */
  struct res_entry *next;
  /* Resource ID.  */
  struct res_id id;
  /* Non-zero if this entry is a subdirectory rather than a leaf.  */
  unsigned int subdir : 1;
  union
  {
    /* If the subdir field is non-zero, this is a pointer to the
       subdirectory.  */
    struct res_directory *dir;
    /* If the subdir field is zero, this is a pointer to the resource
       data.  */
    struct res_resource *res;
  } u;
};

/* Types of resources.  */

enum res_type
{
  RES_TYPE_UNINITIALIZED,
  RES_TYPE_ACCELERATOR,
  RES_TYPE_BITMAP,
  RES_TYPE_CURSOR,
  RES_TYPE_GROUP_CURSOR,
  RES_TYPE_DIALOG,
  RES_TYPE_FONT,
  RES_TYPE_FONTDIR,
  RES_TYPE_ICON,
  RES_TYPE_GROUP_ICON,
  RES_TYPE_MENU,
  RES_TYPE_MESSAGETABLE,
  RES_TYPE_RCDATA,
  RES_TYPE_STRINGTABLE,
  RES_TYPE_USERDATA,
  RES_TYPE_VERSIONINFO
};

/* A res file and a COFF file store information differently.  The
   res_info structures holds data which in a res file is stored with
   each resource, but in a COFF file is stored elsewhere.  */

struct res_res_info
{
  /* Language.  In a COFF file, the third level of the directory is
     keyed by the language, so the language of a resource is defined
     by its location in the resource tree.  */
  unsigned short language;
  /* Characteristics of the resource.  Entirely user defined.  In a
     COFF file, the res_directory structure has a characteristics
     field, but I don't know if it's related to the one in the res
     file.  */
  unsigned long characteristics;
  /* Version of the resource.  Entirely user defined.  In a COFF file,
     the res_directory structure has a characteristics field, but I
     don't know if it's related to the one in the res file.  */
  unsigned long version;
  /* Memory flags.  This is a combination of the MEMFLAG values
     defined below.  Most of these values are historical, and are not
     meaningful for win32.  I don't think there is any way to store
     this information in a COFF file.  */
  unsigned short memflags;
};

/* Each resource in a COFF file has some information which can does
   not appear in a res file.  */

struct res_coff_info
{
  /* The code page used for the data.  I don't really know what this
     should be.  */
  unsigned long codepage;
  /* A resource entry in a COFF file has a reserved field, which we
     record here when reading a COFF file.  When writing a COFF file,
     we set this field to zero.  */
  unsigned long reserved;
};

/* Resource data is stored in a res_resource structure.  */

struct res_resource
{
  /* The type of resource.  */
  enum res_type type;
  /* The data for the resource.  */
  union
  {
    struct
    {
      unsigned long length;
      const unsigned char *data;
    } data;
    struct accelerator *acc;
    struct cursor *cursor;
    struct group_cursor *group_cursor;
    struct dialog *dialog;
    struct fontdir *fontdir;
    struct group_icon *group_icon;
    struct menu *menu;
    struct rcdata_item *rcdata;
    struct stringtable *stringtable;
    struct rcdata_item *userdata;
    struct versioninfo *versioninfo;
  } u;
  /* Information from a res file.  */
  struct res_res_info res_info;
  /* Information from a COFF file.  */
  struct res_coff_info coff_info;
};

#define SUBLANG_SHIFT 10

/* Memory flags in the memflags field of a struct res_resource.  */

#define MEMFLAG_MOVEABLE	0x10
#define MEMFLAG_PURE		0x20
#define MEMFLAG_PRELOAD		0x40
#define MEMFLAG_DISCARDABLE	0x1000

/* Standard resource type codes.  These are used in the ID field of a
   res_entry structure.  */

#define RT_CURSOR		 1
#define RT_BITMAP		 2
#define RT_ICON			 3
#define RT_MENU			 4
#define RT_DIALOG		 5
#define RT_STRING		 6
#define RT_FONTDIR		 7
#define RT_FONT			 8
#define RT_ACCELERATOR		 9
#define RT_RCDATA		10
#define RT_MESSAGETABLE		11
#define RT_GROUP_CURSOR		12
#define RT_GROUP_ICON		14
#define RT_VERSION		16
#define RT_DLGINCLUDE		17
#define RT_PLUGPLAY		19
#define RT_VXD			20
#define RT_ANICURSOR		21
#define RT_ANIICON		22

/* An accelerator resource is a linked list of these structures.  */

struct accelerator
{
  /* Next accelerator.  */
  struct accelerator *next;
  /* Flags.  A combination of the ACC values defined below.  */
  unsigned short flags;
  /* Key value.  */
  unsigned short key;
  /* Resource ID.  */
  unsigned short id;
};

/* Accelerator flags in the flags field of a struct accelerator.
   These are the same values that appear in a res file.  I hope.  */

#define ACC_VIRTKEY	0x01
#define ACC_NOINVERT	0x02
#define ACC_SHIFT	0x04
#define ACC_CONTROL	0x08
#define ACC_ALT		0x10
#define ACC_LAST	0x80

/* A cursor resource.  */

struct cursor
{
  /* X coordinate of hotspot.  */
  short xhotspot;
  /* Y coordinate of hotspot.  */
  short yhotspot;
  /* Length of bitmap data.  */
  unsigned long length;
  /* Data.  */
  const unsigned char *data;
};

/* A group_cursor resource is a list of group_cursor structures.  */

struct group_cursor
{
  /* Next cursor in group.  */
  struct group_cursor *next;
  /* Width.  */
  unsigned short width;
  /* Height.  */
  unsigned short height;
  /* Planes.  */
  unsigned short planes;
  /* Bits per pixel.  */
  unsigned short bits;
  /* Number of bytes in cursor resource.  */
  unsigned long bytes;
  /* Index of cursor resource.  */
  unsigned short index;
};

/* A dialog resource.  */

struct dialog
{
  /* Basic window style.  */
  unsigned long style;
  /* Extended window style.  */
  unsigned long exstyle;
  /* X coordinate.  */
  unsigned short x;
  /* Y coordinate.  */
  unsigned short y;
  /* Width.  */
  unsigned short width;
  /* Height.  */
  unsigned short height;
  /* Menu name.  */
  struct res_id menu;
  /* Class name.  */
  struct res_id class;
  /* Caption.  */
  unichar *caption;
  /* Font point size.  */
  unsigned short pointsize;
  /* Font name.  */
  unichar *font;
  /* Extended information for a dialogex.  */
  struct dialog_ex *ex;
  /* Controls.  */
  struct dialog_control *controls;
};

/* An extended dialog has additional information.  */

struct dialog_ex
{
  /* Help ID.  */
  unsigned long help;
  /* Font weight.  */
  unsigned short weight;
  /* Whether the font is italic.  */
  unsigned char italic;
  /* Character set.  */
  unsigned char charset;
};

/* Window style flags, from the winsup Defines.h header file.  These
   can appear in the style field of a struct dialog or a struct
   dialog_control.  */

#define CW_USEDEFAULT	(0x80000000)
#define WS_BORDER	(0x800000L)
#define WS_CAPTION	(0xc00000L)
#define WS_CHILD	(0x40000000L)
#define WS_CHILDWINDOW	(0x40000000L)
#define WS_CLIPCHILDREN	(0x2000000L)
#define WS_CLIPSIBLINGS	(0x4000000L)
#define WS_DISABLED	(0x8000000L)
#define WS_DLGFRAME	(0x400000L)
#define WS_GROUP	(0x20000L)
#define WS_HSCROLL	(0x100000L)
#define WS_ICONIC	(0x20000000L)
#define WS_MAXIMIZE	(0x1000000L)
#define WS_MAXIMIZEBOX	(0x10000L)
#define WS_MINIMIZE	(0x20000000L)
#define WS_MINIMIZEBOX	(0x20000L)
#define WS_OVERLAPPED	(0L)
#define WS_OVERLAPPEDWINDOW	(0xcf0000L)
#define WS_POPUP	(0x80000000L)
#define WS_POPUPWINDOW	(0x80880000L)
#define WS_SIZEBOX	(0x40000L)
#define WS_SYSMENU	(0x80000L)
#define WS_TABSTOP	(0x10000L)
#define WS_THICKFRAME	(0x40000L)
#define WS_TILED	(0L)
#define WS_TILEDWINDOW	(0xcf0000L)
#define WS_VISIBLE	(0x10000000L)
#define WS_VSCROLL	(0x200000L)
#define MDIS_ALLCHILDSTYLES	(0x1)
#define BS_3STATE	(0x5L)
#define BS_AUTO3STATE	(0x6L)
#define BS_AUTOCHECKBOX	(0x3L)
#define BS_AUTORADIOBUTTON	(0x9L)
#define BS_BITMAP	(0x80L)
#define BS_BOTTOM	(0x800L)
#define BS_CENTER	(0x300L)
#define BS_CHECKBOX	(0x2L)
#define BS_DEFPUSHBUTTON	(0x1L)
#define BS_GROUPBOX	(0x7L)
#define BS_ICON	(0x40L)
#define BS_LEFT	(0x100L)
#define BS_LEFTTEXT	(0x20L)
#define BS_MULTILINE	(0x2000L)
#define BS_NOTIFY	(0x4000L)
#define BS_OWNERDRAW	(0xbL)
#define BS_PUSHBOX	(0xcL)		/* FIXME!  What should this be?  */
#define BS_PUSHBUTTON	(0L)
#define BS_PUSHLIKE	(0x1000L)
#define BS_RADIOBUTTON	(0x4L)
#define BS_RIGHT	(0x200L)
#define BS_RIGHTBUTTON	(0x20L)
#define BS_TEXT	(0L)
#define BS_TOP	(0x400L)
#define BS_USERBUTTON	(0x8L)
#define BS_VCENTER	(0xc00L)
#define CBS_AUTOHSCROLL	(0x40L)
#define CBS_DISABLENOSCROLL	(0x800L)
#define CBS_DROPDOWN	(0x2L)
#define CBS_DROPDOWNLIST	(0x3L)
#define CBS_HASSTRINGS	(0x200L)
#define CBS_LOWERCASE	(0x4000L)
#define CBS_NOINTEGRALHEIGHT	(0x400L)
#define CBS_OEMCONVERT	(0x80L)
#define CBS_OWNERDRAWFIXED	(0x10L)
#define CBS_OWNERDRAWVARIABLE	(0x20L)
#define CBS_SIMPLE	(0x1L)
#define CBS_SORT	(0x100L)
#define CBS_UPPERCASE	(0x2000L)
#define ES_AUTOHSCROLL	(0x80L)
#define ES_AUTOVSCROLL	(0x40L)
#define ES_CENTER	(0x1L)
#define ES_LEFT	(0L)
#define ES_LOWERCASE	(0x10L)
#define ES_MULTILINE	(0x4L)
#define ES_NOHIDESEL	(0x100L)
#define ES_NUMBER	(0x2000L)
#define ES_OEMCONVERT	(0x400L)
#define ES_PASSWORD	(0x20L)
#define ES_READONLY	(0x800L)
#define ES_RIGHT	(0x2L)
#define ES_UPPERCASE	(0x8L)
#define ES_WANTRETURN	(0x1000L)
#define LBS_DISABLENOSCROLL	(0x1000L)
#define LBS_EXTENDEDSEL	(0x800L)
#define LBS_HASSTRINGS	(0x40L)
#define LBS_MULTICOLUMN	(0x200L)
#define LBS_MULTIPLESEL	(0x8L)
#define LBS_NODATA	(0x2000L)
#define LBS_NOINTEGRALHEIGHT	(0x100L)
#define LBS_NOREDRAW	(0x4L)
#define LBS_NOSEL	(0x4000L)
#define LBS_NOTIFY	(0x1L)
#define LBS_OWNERDRAWFIXED	(0x10L)
#define LBS_OWNERDRAWVARIABLE	(0x20L)
#define LBS_SORT	(0x2L)
#define LBS_STANDARD	(0xa00003L)
#define LBS_USETABSTOPS	(0x80L)
#define LBS_WANTKEYBOARDINPUT	(0x400L)
#define SBS_BOTTOMALIGN	(0x4L)
#define SBS_HORZ	(0L)
#define SBS_LEFTALIGN	(0x2L)
#define SBS_RIGHTALIGN	(0x4L)
#define SBS_SIZEBOX	(0x8L)
#define SBS_SIZEBOXBOTTOMRIGHTALIGN	(0x4L)
#define SBS_SIZEBOXTOPLEFTALIGN	(0x2L)
#define SBS_SIZEGRIP	(0x10L)
#define SBS_TOPALIGN	(0x2L)
#define SBS_VERT	(0x1L)
#define SS_BITMAP	(0xeL)
#define SS_BLACKFRAME	(0x7L)
#define SS_BLACKRECT	(0x4L)
#define SS_CENTER	(0x1L)
#define SS_CENTERIMAGE	(0x200L)
#define SS_ENHMETAFILE	(0xfL)
#define SS_ETCHEDFRAME	(0x12L)
#define SS_ETCHEDHORZ	(0x10L)
#define SS_ETCHEDVERT	(0x11L)
#define SS_GRAYFRAME	(0x8L)
#define SS_GRAYRECT	(0x5L)
#define SS_ICON	(0x3L)
#define SS_LEFT	(0L)
#define SS_LEFTNOWORDWRAP	(0xcL)
#define SS_NOPREFIX	(0x80L)
#define SS_NOTIFY	(0x100L)
#define SS_OWNERDRAW	(0xdL)
#define SS_REALSIZEIMAGE	(0x800L)
#define SS_RIGHT	(0x2L)
#define SS_RIGHTJUST	(0x400L)
#define SS_SIMPLE	(0xbL)
#define SS_SUNKEN	(0x1000L)
#define SS_USERITEM     (0xaL)
#define SS_WHITEFRAME	(0x9L)
#define SS_WHITERECT	(0x6L)
#define DS_3DLOOK	(0x4L)
#define DS_ABSALIGN	(0x1L)
#define DS_CENTER	(0x800L)
#define DS_CENTERMOUSE	(0x1000L)
#define DS_CONTEXTHELP	(0x2000L)
#define DS_CONTROL	(0x400L)
#define DS_FIXEDSYS	(0x8L)
#define DS_LOCALEDIT	(0x20L)
#define DS_MODALFRAME	(0x80L)
#define DS_NOFAILCREATE	(0x10L)
#define DS_NOIDLEMSG	(0x100L)
#define DS_SETFONT	(0x40L)
#define DS_SETFOREGROUND	(0x200L)
#define DS_SYSMODAL	(0x2L)

/* A dialog control.  */

struct dialog_control
{
  /* Next control.  */
  struct dialog_control *next;
  /* ID.  */
  unsigned short id;
  /* Style.  */
  unsigned long style;
  /* Extended style.  */
  unsigned long exstyle;
  /* X coordinate.  */
  unsigned short x;
  /* Y coordinate.  */
  unsigned short y;
  /* Width.  */
  unsigned short width;
  /* Height.  */
  unsigned short height;
  /* Class name.  */
  struct res_id class;
  /* Associated text.  */
  struct res_id text;
  /* Extra data for the window procedure.  */
  struct rcdata_item *data;
  /* Help ID.  Only used in an extended dialog.  */
  unsigned long help;
};

/* Control classes.  These can be used as the ID field in a struct
   dialog_control.  */

#define CTL_BUTTON	0x80
#define CTL_EDIT	0x81
#define CTL_STATIC	0x82
#define CTL_LISTBOX	0x83
#define CTL_SCROLLBAR	0x84
#define CTL_COMBOBOX	0x85

/* A fontdir resource is a list of fontdir structures.  */

struct fontdir
{
  struct fontdir *next;
  /* Index of font entry.  */
  short index;
  /* Length of font information.  */
  unsigned long length;
  /* Font information.  */
  const unsigned char *data;
};

/* A group_icon resource is a list of group_icon structures.  */

struct group_icon
{
  /* Next icon in group.  */
  struct group_icon *next;
  /* Width.  */
  unsigned char width;
  /* Height.  */
  unsigned char height;
  /* Color count.  */
  unsigned char colors;
  /* Planes.  */
  unsigned short planes;
  /* Bits per pixel.  */
  unsigned short bits;
  /* Number of bytes in cursor resource.  */
  unsigned long bytes;
  /* Index of cursor resource.  */
  unsigned short index;
};

/* A menu resource.  */

struct menu
{
  /* List of menuitems.  */
  struct menuitem *items;
  /* Help ID.  I don't think there is any way to set this in an rc
     file, but it can appear in the binary format.  */
  unsigned long help;
};

/* A menu resource is a list of menuitem structures.  */

struct menuitem
{
  /* Next menuitem.  */
  struct menuitem *next;
  /* Type.  In a normal menu, rather than a menuex, this is the flags
     field.  */
  unsigned long type;
  /* State.  This is only used in a menuex.  */
  unsigned long state;
  /* Id.  */
  unsigned short id;
  /* Unicode text.  */
  unichar *text;
  /* Popup menu items for a popup.  */
  struct menuitem *popup;
  /* Help ID.  This is only used in a menuex.  */
  unsigned long help;
};

/* Menu item flags.  These can appear in the flags field of a struct
   menuitem.  */

#define MENUITEM_GRAYED		0x001
#define MENUITEM_INACTIVE	0x002
#define MENUITEM_BITMAP		0x004
#define MENUITEM_OWNERDRAW	0x100
#define MENUITEM_CHECKED	0x008
#define MENUITEM_POPUP		0x010
#define MENUITEM_MENUBARBREAK	0x020
#define MENUITEM_MENUBREAK	0x040
#define MENUITEM_ENDMENU	0x080
#define MENUITEM_HELP	       0x4000

/* An rcdata resource is a pointer to a list of rcdata_item
   structures.  */

struct rcdata_item
{
  /* Next data item.  */
  struct rcdata_item *next;
  /* Type of data.  */
  enum
  {
    RCDATA_WORD,
    RCDATA_DWORD,
    RCDATA_STRING,
    RCDATA_WSTRING,
    RCDATA_BUFFER
  } type;
  union
  {
    unsigned int word;
    unsigned long dword;
    struct
    {
      unsigned long length;
      const char *s;
    } string;
    struct
    {
      unsigned long length;
      const unichar *w;
    } wstring;
    struct
    {
      unsigned long length;
      const unsigned char *data;
    } buffer;
  } u;
};

/* A stringtable resource is a pointer to a stringtable structure.  */

struct stringtable
{
  /* Each stringtable resource is a list of 16 unicode strings.  */
  struct
  {
    /* Length of string.  */
    int length;
    /* String data if length > 0.  */
    unichar *string;
  } strings[16];
};

/* A versioninfo resource points to a versioninfo structure.  */

struct versioninfo
{
  /* Fixed version information.  */
  struct fixed_versioninfo *fixed;
  /* Variable version information.  */
  struct ver_info *var;
};

/* The fixed portion of a versioninfo resource.  */

struct fixed_versioninfo
{
  /* The file version, which is two 32 bit integers.  */
  unsigned long file_version_ms;
  unsigned long file_version_ls;
  /* The product version, which is two 32 bit integers.  */
  unsigned long product_version_ms;
  unsigned long product_version_ls;
  /* The file flags mask.  */
  unsigned long file_flags_mask;
  /* The file flags.  */
  unsigned long file_flags;
  /* The OS type.  */
  unsigned long file_os;
  /* The file type.  */
  unsigned long file_type;
  /* The file subtype.  */
  unsigned long file_subtype;
  /* The date, which in Windows is two 32 bit integers.  */
  unsigned long file_date_ms;
  unsigned long file_date_ls;
};

/* A list of variable version information.  */

struct ver_info
{
  /* Next item.  */
  struct ver_info *next;
  /* Type of data.  */
  enum { VERINFO_STRING, VERINFO_VAR } type;
  union
  {
    /* StringFileInfo data.  */
    struct
    {
      /* Language.  */
      unichar *language;
      /* Strings.  */
      struct ver_stringinfo *strings;
    } string;
    /* VarFileInfo data.  */
    struct
    {
      /* Key.  */
      unichar *key;
      /* Values.  */
      struct ver_varinfo *var;
    } var;
  } u;
};

/* A list of string version information.  */

struct ver_stringinfo
{
  /* Next string.  */
  struct ver_stringinfo *next;
  /* Key.  */
  unichar *key;
  /* Value.  */
  unichar *value;
};

/* A list of variable version information.  */

struct ver_varinfo
{
  /* Next item.  */
  struct ver_varinfo *next;
  /* Language ID.  */
  unsigned short language;
  /* Character set ID.  */
  unsigned short charset;
};

/* This structure is used when converting resource information to
   binary.  */

struct bindata
{
  /* Next data.  */
  struct bindata *next;
  /* Length of data.  */
  unsigned long length;
  /* Data.  */
  unsigned char *data;
};

extern int verbose;

/* Function declarations.  */

extern struct res_directory *read_rc_file
  (const char *, const char *, const char *, int, int);
extern struct res_directory *read_res_file (const char *);
extern struct res_directory *read_coff_rsrc (const char *, const char *);
extern void write_rc_file (const char *, const struct res_directory *);
extern void write_res_file (const char *, const struct res_directory *);
extern void write_coff_file
  (const char *, const char *, const struct res_directory *);

extern struct res_resource *bin_to_res
  (struct res_id, const unsigned char *, unsigned long, int);
extern struct bindata *res_to_bin (const struct res_resource *, int);

extern FILE *open_file_search
  (const char *, const char *, const char *, char **);

extern void *res_alloc (size_t);
extern void *reswr_alloc (size_t);

/* Resource ID handling.  */

extern int res_id_cmp (struct res_id, struct res_id);
extern void res_id_print (FILE *, struct res_id, int);
extern void res_ids_print (FILE *, int, const struct res_id *);
extern void res_string_to_id (struct res_id *, const char *);

/* Manipulation of the resource tree.  */

extern struct res_resource *define_resource
  (struct res_directory **, int, const struct res_id *, int);
extern struct res_resource *define_standard_resource
  (struct res_directory **, int, struct res_id, int, int);

extern int extended_dialog (const struct dialog *);
extern int extended_menu (const struct menu *);

/* Communication between the rc file support and the parser and lexer.  */

extern int yydebug;
extern FILE *yyin;
extern char *rc_filename;
extern int rc_lineno;

extern int yyparse (void);
extern int yylex (void);
extern void yyerror (const char *);
extern void rcparse_warning (const char *);
extern void rcparse_set_language (int);
extern void rcparse_discard_strings (void);
extern void rcparse_rcdata (void);
extern void rcparse_normal (void);

extern void define_accelerator
  (struct res_id, const struct res_res_info *, struct accelerator *);
extern void define_bitmap
  (struct res_id, const struct res_res_info *, const char *);
extern void define_cursor
  (struct res_id, const struct res_res_info *, const char *);
extern void define_dialog
  (struct res_id, const struct res_res_info *, const struct dialog *);
extern struct dialog_control *define_control
  (const struct res_id, unsigned long, unsigned long, unsigned long,
   unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);
extern struct dialog_control *define_icon_control
  (struct res_id, unsigned long, unsigned long, unsigned long, unsigned long,
   unsigned long, unsigned long, struct rcdata_item *, struct dialog_ex *);
extern void define_font
  (struct res_id, const struct res_res_info *, const char *);
extern void define_icon
  (struct res_id, const struct res_res_info *, const char *);
extern void define_menu
  (struct res_id, const struct res_res_info *, struct menuitem *);
extern struct menuitem *define_menuitem
  (const char *, int, unsigned long, unsigned long, unsigned long,
   struct menuitem *);
extern void define_messagetable
  (struct res_id, const struct res_res_info *, const char *);
extern void define_rcdata
  (struct res_id, const struct res_res_info *, struct rcdata_item *);
extern struct rcdata_item *define_rcdata_string
  (const char *, unsigned long);
extern struct rcdata_item *define_rcdata_number (unsigned long, int);
extern void define_stringtable
  (const struct res_res_info *, unsigned long, const char *);
extern void define_user_data
  (struct res_id, struct res_id, const struct res_res_info *,
   struct rcdata_item *);
extern void define_user_file
  (struct res_id, struct res_id, const struct res_res_info *, const char *);
extern void define_versioninfo
  (struct res_id, int, struct fixed_versioninfo *, struct ver_info *);
extern struct ver_info *append_ver_stringfileinfo
  (struct ver_info *, const char *, struct ver_stringinfo *);
extern struct ver_info *append_ver_varfileinfo
  (struct ver_info *, const char *, struct ver_varinfo *);
extern struct ver_stringinfo *append_verval
  (struct ver_stringinfo *, const char *, const char *);
extern struct ver_varinfo *append_vertrans
  (struct ver_varinfo *, unsigned long, unsigned long);
