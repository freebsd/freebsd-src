#define LABEL 257
#define VARIABLE 258
#define NUMBER 259
#define TEXT 260
#define COMMAND_LINE 261
#define DELIMITED 262
#define ORDINAL 263
#define TH 264
#define LEFT_ARROW_HEAD 265
#define RIGHT_ARROW_HEAD 266
#define DOUBLE_ARROW_HEAD 267
#define LAST 268
#define UP 269
#define DOWN 270
#define LEFT 271
#define RIGHT 272
#define BOX 273
#define CIRCLE 274
#define ELLIPSE 275
#define ARC 276
#define LINE 277
#define ARROW 278
#define MOVE 279
#define SPLINE 280
#define HEIGHT 281
#define RADIUS 282
#define FIGNAME 283
#define WIDTH 284
#define DIAMETER 285
#define FROM 286
#define TO 287
#define AT 288
#define WITH 289
#define BY 290
#define THEN 291
#define SOLID 292
#define DOTTED 293
#define DASHED 294
#define CHOP 295
#define SAME 296
#define INVISIBLE 297
#define LJUST 298
#define RJUST 299
#define ABOVE 300
#define BELOW 301
#define OF 302
#define THE 303
#define WAY 304
#define BETWEEN 305
#define AND 306
#define HERE 307
#define DOT_N 308
#define DOT_E 309
#define DOT_W 310
#define DOT_S 311
#define DOT_NE 312
#define DOT_SE 313
#define DOT_NW 314
#define DOT_SW 315
#define DOT_C 316
#define DOT_START 317
#define DOT_END 318
#define DOT_X 319
#define DOT_Y 320
#define DOT_HT 321
#define DOT_WID 322
#define DOT_RAD 323
#define SIN 324
#define COS 325
#define ATAN2 326
#define LOG 327
#define EXP 328
#define SQRT 329
#define K_MAX 330
#define K_MIN 331
#define INT 332
#define RAND 333
#define SRAND 334
#define COPY 335
#define THRU 336
#define TOP 337
#define BOTTOM 338
#define UPPER 339
#define LOWER 340
#define SH 341
#define PRINT 342
#define CW 343
#define CCW 344
#define FOR 345
#define DO 346
#define IF 347
#define ELSE 348
#define ANDAND 349
#define OROR 350
#define NOTEQUAL 351
#define EQUALEQUAL 352
#define LESSEQUAL 353
#define GREATEREQUAL 354
#define LEFT_CORNER 355
#define RIGHT_CORNER 356
#define NORTH 357
#define SOUTH 358
#define EAST 359
#define WEST 360
#define CENTER 361
#define END 362
#define START 363
#define RESET 364
#define UNTIL 365
#define PLOT 366
#define THICKNESS 367
#define FILL 368
#define COLORED 369
#define OUTLINED 370
#define SHADED 371
#define ALIGNED 372
#define SPRINTF 373
#define COMMAND 374
#define DEFINE 375
#define UNDEF 376
typedef union {
	char *str;
	int n;
	double x;
	struct { double x, y; } pair;
	struct { double x; char *body; } if_data;
	struct { char *str; const char *filename; int lineno; } lstr;
	struct { double *v; int nv; int maxv; } dv;
	struct { double val; int is_multiplicative; } by;
	place pl;
	object *obj;
	corner crn;
	path *pth;
	object_spec *spec;
	saved_state *pstate;
	graphics_state state;
	object_type obtype;
} YYSTYPE;
extern YYSTYPE yylval;
