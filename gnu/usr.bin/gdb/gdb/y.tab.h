#define INT 257
#define HEX 258
#define ERROR 259
#define UINT 260
#define M2_TRUE 261
#define M2_FALSE 262
#define CHAR 263
#define FLOAT 264
#define STRING 265
#define NAME 266
#define BLOCKNAME 267
#define IDENT 268
#define VARNAME 269
#define TYPENAME 270
#define SIZE 271
#define CAP 272
#define ORD 273
#define HIGH 274
#define ABS 275
#define MIN_FUNC 276
#define MAX_FUNC 277
#define FLOAT_FUNC 278
#define VAL 279
#define CHR 280
#define ODD 281
#define TRUNC 282
#define INC 283
#define DEC 284
#define INCL 285
#define EXCL 286
#define COLONCOLON 287
#define LAST 288
#define REGNAME 289
#define INTERNAL_VAR 290
#define ABOVE_COMMA 291
#define ASSIGN 292
#define LEQ 293
#define GEQ 294
#define NOTEQUAL 295
#define IN 296
#define OROR 297
#define LOGICAL_AND 298
#define DIV 299
#define MOD 300
#define UNARY 301
#define DOT 302
#define NOT 303
#define QID 304
typedef union
  {
    LONGEST lval;
    unsigned LONGEST ulval;
    double dval;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    int voidval;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;

    struct type **tvec;
    int *ivec;
  } YYSTYPE;
extern YYSTYPE m2_lval;
