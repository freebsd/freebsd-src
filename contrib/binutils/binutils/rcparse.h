typedef union
{
  struct accelerator acc;
  struct accelerator *pacc;
  struct dialog_control *dialog_control;
  struct menuitem *menuitem;
  struct
  {
    struct rcdata_item *first;
    struct rcdata_item *last;
  } rcdata;
  struct rcdata_item *rcdata_item;
  struct stringtable_data *stringtable;
  struct fixed_versioninfo *fixver;
  struct ver_info *verinfo;
  struct ver_stringinfo *verstring;
  struct ver_varinfo *vervar;
  struct res_id id;
  struct res_res_info res_info;
  struct
  {
    unsigned short on;
    unsigned short off;
  } memflags;
  struct
  {
    unsigned long val;
    /* Nonzero if this number was explicitly specified as long.  */
    int dword;
  } i;
  unsigned long il;
  unsigned short is;
  const char *s;
  struct
  {
    unsigned long length;
    const char *s;
  } ss;
} YYSTYPE;
#define	BEG	258
#define	END	259
#define	ACCELERATORS	260
#define	VIRTKEY	261
#define	ASCII	262
#define	NOINVERT	263
#define	SHIFT	264
#define	CONTROL	265
#define	ALT	266
#define	BITMAP	267
#define	CURSOR	268
#define	DIALOG	269
#define	DIALOGEX	270
#define	EXSTYLE	271
#define	CAPTION	272
#define	CLASS	273
#define	STYLE	274
#define	AUTO3STATE	275
#define	AUTOCHECKBOX	276
#define	AUTORADIOBUTTON	277
#define	CHECKBOX	278
#define	COMBOBOX	279
#define	CTEXT	280
#define	DEFPUSHBUTTON	281
#define	EDITTEXT	282
#define	GROUPBOX	283
#define	LISTBOX	284
#define	LTEXT	285
#define	PUSHBOX	286
#define	PUSHBUTTON	287
#define	RADIOBUTTON	288
#define	RTEXT	289
#define	SCROLLBAR	290
#define	STATE3	291
#define	USERBUTTON	292
#define	BEDIT	293
#define	HEDIT	294
#define	IEDIT	295
#define	FONT	296
#define	ICON	297
#define	LANGUAGE	298
#define	CHARACTERISTICS	299
#define	VERSIONK	300
#define	MENU	301
#define	MENUEX	302
#define	MENUITEM	303
#define	SEPARATOR	304
#define	POPUP	305
#define	CHECKED	306
#define	GRAYED	307
#define	HELP	308
#define	INACTIVE	309
#define	MENUBARBREAK	310
#define	MENUBREAK	311
#define	MESSAGETABLE	312
#define	RCDATA	313
#define	STRINGTABLE	314
#define	VERSIONINFO	315
#define	FILEVERSION	316
#define	PRODUCTVERSION	317
#define	FILEFLAGSMASK	318
#define	FILEFLAGS	319
#define	FILEOS	320
#define	FILETYPE	321
#define	FILESUBTYPE	322
#define	BLOCKSTRINGFILEINFO	323
#define	BLOCKVARFILEINFO	324
#define	VALUE	325
#define	BLOCK	326
#define	MOVEABLE	327
#define	FIXED	328
#define	PURE	329
#define	IMPURE	330
#define	PRELOAD	331
#define	LOADONCALL	332
#define	DISCARDABLE	333
#define	NOT	334
#define	QUOTEDSTRING	335
#define	STRING	336
#define	NUMBER	337
#define	SIZEDSTRING	338
#define	NEG	339


extern YYSTYPE yylval;
