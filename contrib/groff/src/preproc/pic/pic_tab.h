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
#define SOLID 291
#define DOTTED 292
#define DASHED 293
#define CHOP 294
#define SAME 295
#define INVISIBLE 296
#define LJUST 297
#define RJUST 298
#define ABOVE 299
#define BELOW 300
#define OF 301
#define THE 302
#define WAY 303
#define BETWEEN 304
#define AND 305
#define HERE 306
#define DOT_N 307
#define DOT_E 308
#define DOT_W 309
#define DOT_S 310
#define DOT_NE 311
#define DOT_SE 312
#define DOT_NW 313
#define DOT_SW 314
#define DOT_C 315
#define DOT_START 316
#define DOT_END 317
#define DOT_X 318
#define DOT_Y 319
#define DOT_HT 320
#define DOT_WID 321
#define DOT_RAD 322
#define SIN 323
#define COS 324
#define ATAN2 325
#define LOG 326
#define EXP 327
#define SQRT 328
#define K_MAX 329
#define K_MIN 330
#define INT 331
#define RAND 332
#define SRAND 333
#define COPY 334
#define THRU 335
#define TOP 336
#define BOTTOM 337
#define UPPER 338
#define LOWER 339
#define SH 340
#define PRINT 341
#define CW 342
#define CCW 343
#define FOR 344
#define DO 345
#define IF 346
#define ELSE 347
#define ANDAND 348
#define OROR 349
#define NOTEQUAL 350
#define EQUALEQUAL 351
#define LESSEQUAL 352
#define GREATEREQUAL 353
#define LEFT_CORNER 354
#define RIGHT_CORNER 355
#define NORTH 356
#define SOUTH 357
#define EAST 358
#define WEST 359
#define CENTER 360
#define END 361
#define START 362
#define RESET 363
#define UNTIL 364
#define PLOT 365
#define THICKNESS 366
#define FILL 367
#define COLORED 368
#define OUTLINED 369
#define SHADED 370
#define ALIGNED 371
#define SPRINTF 372
#define COMMAND 373
#define DEFINE 374
#define UNDEF 375
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
