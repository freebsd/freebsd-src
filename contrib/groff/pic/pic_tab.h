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
#define WIDTH 283
#define DIAMETER 284
#define FROM 285
#define TO 286
#define AT 287
#define WITH 288
#define BY 289
#define THEN 290
#define DOTTED 291
#define DASHED 292
#define CHOP 293
#define SAME 294
#define INVISIBLE 295
#define LJUST 296
#define RJUST 297
#define ABOVE 298
#define BELOW 299
#define OF 300
#define THE 301
#define WAY 302
#define BETWEEN 303
#define AND 304
#define HERE 305
#define DOT_N 306
#define DOT_E 307
#define DOT_W 308
#define DOT_S 309
#define DOT_NE 310
#define DOT_SE 311
#define DOT_NW 312
#define DOT_SW 313
#define DOT_C 314
#define DOT_START 315
#define DOT_END 316
#define DOT_X 317
#define DOT_Y 318
#define DOT_HT 319
#define DOT_WID 320
#define DOT_RAD 321
#define SIN 322
#define COS 323
#define ATAN2 324
#define LOG 325
#define EXP 326
#define SQRT 327
#define K_MAX 328
#define K_MIN 329
#define INT 330
#define RAND 331
#define SRAND 332
#define COPY 333
#define THRU 334
#define TOP 335
#define BOTTOM 336
#define UPPER 337
#define LOWER 338
#define SH 339
#define PRINT 340
#define CW 341
#define CCW 342
#define FOR 343
#define DO 344
#define IF 345
#define ELSE 346
#define ANDAND 347
#define OROR 348
#define NOTEQUAL 349
#define EQUALEQUAL 350
#define LESSEQUAL 351
#define GREATEREQUAL 352
#define LEFT_CORNER 353
#define RIGHT_CORNER 354
#define CENTER 355
#define END 356
#define START 357
#define RESET 358
#define UNTIL 359
#define PLOT 360
#define THICKNESS 361
#define FILL 362
#define ALIGNED 363
#define SPRINTF 364
#define COMMAND 365
#define DEFINE 366
#define UNDEF 367
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
