#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
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
} yystype;
# define YYSTYPE yystype
#endif
# define	BEG	257
# define	END	258
# define	ACCELERATORS	259
# define	VIRTKEY	260
# define	ASCII	261
# define	NOINVERT	262
# define	SHIFT	263
# define	CONTROL	264
# define	ALT	265
# define	BITMAP	266
# define	CURSOR	267
# define	DIALOG	268
# define	DIALOGEX	269
# define	EXSTYLE	270
# define	CAPTION	271
# define	CLASS	272
# define	STYLE	273
# define	AUTO3STATE	274
# define	AUTOCHECKBOX	275
# define	AUTORADIOBUTTON	276
# define	CHECKBOX	277
# define	COMBOBOX	278
# define	CTEXT	279
# define	DEFPUSHBUTTON	280
# define	EDITTEXT	281
# define	GROUPBOX	282
# define	LISTBOX	283
# define	LTEXT	284
# define	PUSHBOX	285
# define	PUSHBUTTON	286
# define	RADIOBUTTON	287
# define	RTEXT	288
# define	SCROLLBAR	289
# define	STATE3	290
# define	USERBUTTON	291
# define	BEDIT	292
# define	HEDIT	293
# define	IEDIT	294
# define	FONT	295
# define	ICON	296
# define	LANGUAGE	297
# define	CHARACTERISTICS	298
# define	VERSIONK	299
# define	MENU	300
# define	MENUEX	301
# define	MENUITEM	302
# define	SEPARATOR	303
# define	POPUP	304
# define	CHECKED	305
# define	GRAYED	306
# define	HELP	307
# define	INACTIVE	308
# define	MENUBARBREAK	309
# define	MENUBREAK	310
# define	MESSAGETABLE	311
# define	RCDATA	312
# define	STRINGTABLE	313
# define	VERSIONINFO	314
# define	FILEVERSION	315
# define	PRODUCTVERSION	316
# define	FILEFLAGSMASK	317
# define	FILEFLAGS	318
# define	FILEOS	319
# define	FILETYPE	320
# define	FILESUBTYPE	321
# define	BLOCKSTRINGFILEINFO	322
# define	BLOCKVARFILEINFO	323
# define	VALUE	324
# define	BLOCK	325
# define	MOVEABLE	326
# define	FIXED	327
# define	PURE	328
# define	IMPURE	329
# define	PRELOAD	330
# define	LOADONCALL	331
# define	DISCARDABLE	332
# define	NOT	333
# define	QUOTEDSTRING	334
# define	STRING	335
# define	NUMBER	336
# define	SIZEDSTRING	337
# define	IGNORED_TOKEN	338
# define	NEG	339


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
