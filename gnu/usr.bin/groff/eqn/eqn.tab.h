#define OVER 257
#define SMALLOVER 258
#define SQRT 259
#define SUB 260
#define SUP 261
#define LPILE 262
#define RPILE 263
#define CPILE 264
#define PILE 265
#define LEFT 266
#define RIGHT 267
#define TO 268
#define FROM 269
#define SIZE 270
#define FONT 271
#define ROMAN 272
#define BOLD 273
#define ITALIC 274
#define FAT 275
#define ACCENT 276
#define BAR 277
#define UNDER 278
#define ABOVE 279
#define TEXT 280
#define QUOTED_TEXT 281
#define FWD 282
#define BACK 283
#define DOWN 284
#define UP 285
#define MATRIX 286
#define COL 287
#define LCOL 288
#define RCOL 289
#define CCOL 290
#define MARK 291
#define LINEUP 292
#define TYPE 293
#define VCENTER 294
#define PRIME 295
#define SPLIT 296
#define NOSPLIT 297
#define UACCENT 298
#define SPECIAL 299
#define SPACE 300
#define GFONT 301
#define GSIZE 302
#define DEFINE 303
#define NDEFINE 304
#define TDEFINE 305
#define SDEFINE 306
#define UNDEF 307
#define IFDEF 308
#define INCLUDE 309
#define DELIM 310
#define CHARTYPE 311
#define SET 312
#define GRFONT 313
#define GBFONT 314
typedef union {
	char *str;
	box *b;
	pile_box *pb;
	matrix_box *mb;
	int n;
	column *col;
} YYSTYPE;
extern YYSTYPE yylval;
