extern int target_flags;
  
enum reg_class
{
  NO_REGS,
  AREG, DREG, CREG, BREG,
  Q_REGS,			 
  SIREG, DIREG,
  INDEX_REGS,			 
  GENERAL_REGS,			 
  FP_TOP_REG, FP_SECOND_REG,	 
  FLOAT_REGS,
  ALL_REGS, LIM_REG_CLASSES
};
extern enum reg_class regclass_map[17 ];
  
   
extern struct rtx_def *i386_compare_op0, *i386_compare_op1;
extern struct rtx_def *(*i386_compare_gen)(), *(*i386_compare_gen_eq)();
extern char *hi_reg_name[];
extern char *qi_reg_name[];
extern char *qi_high_reg_name[];
  
enum machine_mode {
  VOIDmode , 
  QImode , 		 
  HImode , 
  PSImode , 
  SImode , 
  PDImode , 
  DImode , 
  TImode , 
  OImode , 
  QFmode , 
  HFmode , 
  SFmode , 
  DFmode , 
  XFmode ,     
  TFmode , 
  SCmode , 
  DCmode , 
  XCmode , 
  TCmode , 
  CQImode , 
  CHImode , 
  CSImode , 
  CDImode , 
  CTImode , 
  COImode , 
  BLKmode , 
  CCmode , 
  CCFPEQmode ,
MAX_MACHINE_MODE };
extern char *mode_name[];
enum mode_class { MODE_RANDOM, MODE_INT, MODE_FLOAT, MODE_PARTIAL_INT, MODE_CC,
		  MODE_COMPLEX_INT, MODE_COMPLEX_FLOAT, MAX_MODE_CLASS};
extern enum mode_class mode_class[];
extern int mode_size[];
extern int mode_unit_size[];
extern enum machine_mode mode_wider_mode[];
extern enum machine_mode mode_for_size  (unsigned int, enum mode_class, int)  ;
extern enum machine_mode get_best_mode  (int, int, int, enum machine_mode, int)  ;
extern enum machine_mode class_narrowest_mode[];
extern enum machine_mode byte_mode;
extern enum machine_mode word_mode;
typedef int ptrdiff_t;
typedef unsigned int size_t;
typedef short unsigned int wchar_t;
typedef unsigned int UQItype	__attribute__ ((mode (QI)));
typedef 	 int SItype	__attribute__ ((mode (SI)));
typedef unsigned int USItype	__attribute__ ((mode (SI)));
typedef		 int DItype	__attribute__ ((mode (DI)));
typedef unsigned int UDItype	__attribute__ ((mode (DI)));
typedef 	float SFtype	__attribute__ ((mode (SF)));
typedef		float DFtype	__attribute__ ((mode (DF)));
typedef int word_type __attribute__ ((mode (SI)));
  struct DIstruct {SItype low, high;};
typedef union
{
  struct DIstruct s;
  DItype ll;
} DIunion;
extern DItype __fixunssfdi (SFtype a);
extern DItype __fixunsdfdi (DFtype a);
typedef long fpos_t;		 
struct __sbuf {
	unsigned char *_base;
	int	_size;
};
typedef	struct __sFILE {
	unsigned char *_p;	 
	int	_r;		 
	int	_w;		 
	short	_flags;		 
	short	_file;		 
	struct	__sbuf _bf;	 
	int	_lbfsize;	 
	 
	void	*_cookie;	 
	int	(*_close)  (void *) 		;
	int	(*_read)   (void *, char *, int) 		;
	fpos_t	(*_seek)   (void *, fpos_t, int) 		;
	int	(*_write)  (void *, const char *, int) 		;
	 
	struct	__sbuf _ub;	 
	unsigned char *_up;	 
	int	_ur;		 
	 
	unsigned char _ubuf[3];	 
	unsigned char _nbuf[1];	 
	 
	struct	__sbuf _lb;	 
	 
	int	_blksize;	 
	int	_offset;	 
} FILE;
extern FILE __sF[];
	 
void	 clearerr  (FILE *) 		;
int	 fclose  (FILE *) 		;
int	 feof  (FILE *) 		;
int	 ferror  (FILE *) 		;
int	 fflush  (FILE *) 		;
int	 fgetc  (FILE *) 		;
int	 fgetpos  (FILE *, fpos_t *) 		;
char	*fgets  (char *, size_t, FILE *) 		;
FILE	*fopen  (const char *, const char *) 		;
int	 fprintf  (FILE *, const char *, ...) 		;
int	 fputc  (int, FILE *) 		;
int	 fputs  (const char *, FILE *) 		;
int	 fread  (void *, size_t, size_t, FILE *) 		;
FILE	*freopen  (const char *, const char *, FILE *) 		;
int	 fscanf  (FILE *, const char *, ...) 		;
int	 fseek  (FILE *, long, int) 		;
int	 fsetpos  (FILE *, const fpos_t *) 		;
long	 ftell  (const FILE *) 		;
int	 fwrite  (const void *, size_t, size_t, FILE *) 		;
int	 getc  (FILE *) 		;
int	 getchar  (void) 		;
char	*gets  (char *) 		;
extern int sys_nerr;			 
extern char *sys_errlist[];
void	 perror  (const char *) 		;
int	 printf  (const char *, ...) 		;
int	 putc  (int, FILE *) 		;
int	 putchar  (int) 		;
int	 puts  (const char *) 		;
int	 remove  (const char *) 		;
int	 rename   (const char *, const char *) 		;
void	 rewind  (FILE *) 		;
int	 scanf  (const char *, ...) 		;
void	 setbuf  (FILE *, char *) 		;
int	 setvbuf  (FILE *, char *, int, size_t) 		;
int	 sprintf  (char *, const char *, ...) 		;
int	 sscanf  (char *, const char *, ...) 		;
FILE	*tmpfile  (void) 		;
char	*tmpnam  (char *) 		;
int	 ungetc  (int, FILE *) 		;
int	 vfprintf  (FILE *, const char *, char *			) 		;
int	 vprintf  (const char *, char *			) 		;
int	 vsprintf  (char *, const char *, char *			) 		;
char	*ctermid  (char *) 		;
FILE	*fdopen  (int, const char *) 		;
int	 fileno  (FILE *) 		;
char	*fgetline  (FILE *, size_t *) 		;
int	 fpurge  (FILE *) 		;
int	 getw  (FILE *) 		;
int	 pclose  (FILE *) 		;
FILE	*popen  (const char *, const char *) 		;
int	 putw  (int, FILE *) 		;
void	 setbuffer  (FILE *, char *, int) 		;
int	 setlinebuf  (FILE *) 		;
char	*tempnam  (const char *, const char *) 		;
int	 snprintf  (char *, size_t, const char *, ...) 		;
int	 vsnprintf  (char *, size_t, const char *, char *			) 		;
int	 vscanf  (const char *, char *			) 		;
int	 vsscanf  (const char *, const char *, char *			) 		;
FILE	*funopen  (const void *,
		int (*)(void *, char *, int),
		int (*)(void *, const char *, int),
		fpos_t (*)(void *, fpos_t, int),
		int (*)(void *)) 		;
int	__srget  (FILE *) 		;
int	__svfscanf  (FILE *, const char *, char *			) 		;
int	__swbuf  (int, FILE *) 		;
static __inline__ int __sputc(int _c, FILE *_p) {
	if (--_p->_w >= 0 || (_p->_w >= _p->_lbfsize && (char)_c != '\n'))
		return (*_p->_p++ = _c);
	else
		return (__swbuf(_c, _p));
}
void
__eprintf (string, expression, line, filename)
     const char *string;
     const char *expression;
     int line;
     const char *filename;
{
  fprintf ((&__sF[2]) , string, expression, line, filename);
  fflush ((&__sF[2]) );
  abort ();
}
