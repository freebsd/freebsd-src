/*    handy.h
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#if !defined(__STDC__)
#ifdef NULL
#undef NULL
#endif
#ifndef I286
#  define NULL 0
#else
#  define NULL 0L
#endif
#endif

#define Null(type) ((type)NULL)

/*
=for apidoc AmU||Nullch
Null character pointer.

=for apidoc AmU||Nullsv
Null SV pointer.

=cut
*/

#define Nullch Null(char*)
#define Nullfp Null(PerlIO*)
#define Nullsv Null(SV*)

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE (1)
#define FALSE (0)


/* XXX Configure ought to have a test for a boolean type, if I can
   just figure out all the headers such a test needs.
   Andy Dougherty	August 1996
*/
/* bool is built-in for g++-2.6.3 and later, which might be used
   for extensions.  <_G_config.h> defines _G_HAVE_BOOL, but we can't
   be sure _G_config.h will be included before this file.  _G_config.h
   also defines _G_HAVE_BOOL for both gcc and g++, but only g++
   actually has bool.  Hence, _G_HAVE_BOOL is pretty useless for us.
   g++ can be identified by __GNUG__.
   Andy Dougherty	February 2000
*/
#ifdef __GNUG__ 	/* GNU g++ has bool built-in */
#  ifndef HAS_BOOL
#    define HAS_BOOL 1
#  endif
#endif

/* The NeXT dynamic loader headers will not build with the bool macro
   So declare them now to clear confusion.
*/
#if defined(NeXT) || defined(__NeXT__)
# undef FALSE
# undef TRUE
  typedef enum bool { FALSE = 0, TRUE = 1 } bool;
# define ENUM_BOOL 1
# ifndef HAS_BOOL
#  define HAS_BOOL 1
# endif /* !HAS_BOOL */
#endif /* NeXT || __NeXT__ */

#ifndef HAS_BOOL
# if defined(UTS) || defined(VMS)
#  define bool int
# else
#  define bool char
# endif
# define HAS_BOOL 1
#endif

/* XXX A note on the perl source internal type system.  The
   original intent was that I32 be *exactly* 32 bits.

   Currently, we only guarantee that I32 is *at least* 32 bits.
   Specifically, if int is 64 bits, then so is I32.  (This is the case
   for the Cray.)  This has the advantage of meshing nicely with
   standard library calls (where we pass an I32 and the library is
   expecting an int), but the disadvantage that an I32 is not 32 bits.
   Andy Dougherty	August 1996

   There is no guarantee that there is *any* integral type with
   exactly 32 bits.  It is perfectly legal for a system to have
   sizeof(short) == sizeof(int) == sizeof(long) == 8.

   Similarly, there is no guarantee that I16 and U16 have exactly 16
   bits.

   For dealing with issues that may arise from various 32/64-bit
   systems, we will ask Configure to check out

   	SHORTSIZE == sizeof(short)
   	INTSIZE == sizeof(int)
   	LONGSIZE == sizeof(long)
	LONGLONGSIZE == sizeof(long long) (if HAS_LONG_LONG)
   	PTRSIZE == sizeof(void *)
	DOUBLESIZE == sizeof(double)
	LONG_DOUBLESIZE == sizeof(long double) (if HAS_LONG_DOUBLE).

*/

#ifdef I_INTTYPES /* e.g. Linux has int64_t without <inttypes.h> */
#   include <inttypes.h>
#endif

typedef I8TYPE I8;
typedef U8TYPE U8;
typedef I16TYPE I16;
typedef U16TYPE U16;
typedef I32TYPE I32;
typedef U32TYPE U32;
#ifdef PERL_CORE
#   ifdef HAS_QUAD
typedef I64TYPE I64;
typedef U64TYPE U64;
#   endif
#endif /* PERL_CORE */

#if defined(HAS_QUAD) && defined(USE_64_BIT_INT)
#   ifndef UINT64_C /* usually from <inttypes.h> */
#       if defined(HAS_LONG_LONG) && QUADKIND == QUAD_IS_LONG_LONG
#           define INT64_C(c)	CAT2(c,LL)
#           define UINT64_C(c)	CAT2(c,ULL)
#       else
#           if LONGSIZE == 8 && QUADKIND == QUAD_IS_LONG
#               define INT64_C(c)	CAT2(c,L)
#               define UINT64_C(c)	CAT2(c,UL)
#           else
#               define INT64_C(c)	((I64TYPE)(c))
#               define UINT64_C(c)	((U64TYPE)(c))
#           endif
#       endif
#   endif
#endif

/* Mention I8SIZE, U8SIZE, I16SIZE, U16SIZE, I32SIZE, U32SIZE,
   I64SIZE, and U64SIZE here so that metaconfig pulls them in. */

#if defined(UINT8_MAX) && defined(INT16_MAX) && defined(INT32_MAX)

/* I8_MAX and I8_MIN constants are not defined, as I8 is an ambiguous type.
   Please search CHAR_MAX in perl.h for further details. */
#define U8_MAX UINT8_MAX
#define U8_MIN UINT8_MIN

#define I16_MAX INT16_MAX
#define I16_MIN INT16_MIN
#define U16_MAX UINT16_MAX
#define U16_MIN UINT16_MIN

#define I32_MAX INT32_MAX
#define I32_MIN INT32_MIN
#define U32_MAX UINT32_MAX
#define U32_MIN UINT32_MIN

#else

/* I8_MAX and I8_MIN constants are not defined, as I8 is an ambiguous type.
   Please search CHAR_MAX in perl.h for further details. */
#define U8_MAX PERL_UCHAR_MAX
#define U8_MIN PERL_UCHAR_MIN

#define I16_MAX PERL_SHORT_MAX
#define I16_MIN PERL_SHORT_MIN
#define U16_MAX PERL_USHORT_MAX
#define U16_MIN PERL_USHORT_MIN

#if LONGSIZE > 4
# define I32_MAX PERL_INT_MAX
# define I32_MIN PERL_INT_MIN
# define U32_MAX PERL_UINT_MAX
# define U32_MIN PERL_UINT_MIN
#else
# define I32_MAX PERL_LONG_MAX
# define I32_MIN PERL_LONG_MIN
# define U32_MAX PERL_ULONG_MAX
# define U32_MIN PERL_ULONG_MIN
#endif

#endif

#define BIT_DIGITS(N)   (((N)*146)/485 + 1)  /* log2(10) =~ 146/485 */
#define TYPE_DIGITS(T)  BIT_DIGITS(sizeof(T) * 8)
#define TYPE_CHARS(T)   (TYPE_DIGITS(T) + 2) /* sign, NUL */

#define Ctl(ch) ((ch) & 037)

/*
=for apidoc Am|bool|strNE|char* s1|char* s2
Test two strings to see if they are different.  Returns true or
false.

=for apidoc Am|bool|strEQ|char* s1|char* s2
Test two strings to see if they are equal.  Returns true or false.

=for apidoc Am|bool|strLT|char* s1|char* s2
Test two strings to see if the first, C<s1>, is less than the second,
C<s2>.  Returns true or false.

=for apidoc Am|bool|strLE|char* s1|char* s2
Test two strings to see if the first, C<s1>, is less than or equal to the
second, C<s2>.  Returns true or false.

=for apidoc Am|bool|strGT|char* s1|char* s2
Test two strings to see if the first, C<s1>, is greater than the second,
C<s2>.  Returns true or false.

=for apidoc Am|bool|strGE|char* s1|char* s2
Test two strings to see if the first, C<s1>, is greater than or equal to
the second, C<s2>.  Returns true or false.

=for apidoc Am|bool|strnNE|char* s1|char* s2|STRLEN len
Test two strings to see if they are different.  The C<len> parameter
indicates the number of bytes to compare.  Returns true or false. (A
wrapper for C<strncmp>).

=for apidoc Am|bool|strnEQ|char* s1|char* s2|STRLEN len
Test two strings to see if they are equal.  The C<len> parameter indicates
the number of bytes to compare.  Returns true or false. (A wrapper for
C<strncmp>).

=cut
*/

#define strNE(s1,s2) (strcmp(s1,s2))
#define strEQ(s1,s2) (!strcmp(s1,s2))
#define strLT(s1,s2) (strcmp(s1,s2) < 0)
#define strLE(s1,s2) (strcmp(s1,s2) <= 0)
#define strGT(s1,s2) (strcmp(s1,s2) > 0)
#define strGE(s1,s2) (strcmp(s1,s2) >= 0)
#define strnNE(s1,s2,l) (strncmp(s1,s2,l))
#define strnEQ(s1,s2,l) (!strncmp(s1,s2,l))

#ifdef HAS_MEMCMP
#  define memNE(s1,s2,l) (memcmp(s1,s2,l))
#  define memEQ(s1,s2,l) (!memcmp(s1,s2,l))
#else
#  define memNE(s1,s2,l) (bcmp(s1,s2,l))
#  define memEQ(s1,s2,l) (!bcmp(s1,s2,l))
#endif

/*
 * Character classes.
 *
 * Unfortunately, the introduction of locales means that we
 * can't trust isupper(), etc. to tell the truth.  And when
 * it comes to /\w+/ with tainting enabled, we *must* be able
 * to trust our character classes.
 *
 * Therefore, the default tests in the text of Perl will be
 * independent of locale.  Any code that wants to depend on
 * the current locale will use the tests that begin with "lc".
 */

#ifdef HAS_SETLOCALE  /* XXX Is there a better test for this? */
#  ifndef CTYPE256
#    define CTYPE256
#  endif
#endif

/*
=for apidoc Am|bool|isALNUM|char ch
Returns a boolean indicating whether the C C<char> is an ASCII alphanumeric
character (including underscore) or digit.

=for apidoc Am|bool|isALPHA|char ch
Returns a boolean indicating whether the C C<char> is an ASCII alphabetic
character.

=for apidoc Am|bool|isSPACE|char ch
Returns a boolean indicating whether the C C<char> is whitespace.

=for apidoc Am|bool|isDIGIT|char ch
Returns a boolean indicating whether the C C<char> is an ASCII
digit.

=for apidoc Am|bool|isUPPER|char ch
Returns a boolean indicating whether the C C<char> is an uppercase
character.

=for apidoc Am|bool|isLOWER|char ch
Returns a boolean indicating whether the C C<char> is a lowercase
character.

=for apidoc Am|char|toUPPER|char ch
Converts the specified character to uppercase.

=for apidoc Am|char|toLOWER|char ch
Converts the specified character to lowercase.

=cut
*/

#define isALNUM(c)	(isALPHA(c) || isDIGIT(c) || (c) == '_')
#define isIDFIRST(c)	(isALPHA(c) || (c) == '_')
#define isALPHA(c)	(isUPPER(c) || isLOWER(c))
#define isSPACE(c) \
	((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) =='\r' || (c) == '\f')
#define isPSXSPC(c)	(isSPACE(c) || (c) == '\v')
#define isBLANK(c)	((c) == ' ' || (c) == '\t')
#define isDIGIT(c)	((c) >= '0' && (c) <= '9')
#ifdef EBCDIC
    /* In EBCDIC we do not do locales: therefore() isupper() is fine. */
#   define isUPPER(c)	isupper(c)
#   define isLOWER(c)	islower(c)
#   define isALNUMC(c)	isalnum(c)
#   define isASCII(c)	isascii(c)
#   define isCNTRL(c)	iscntrl(c)
#   define isGRAPH(c)	isgraph(c)
#   define isPRINT(c)	isprint(c)
#   define isPUNCT(c)	ispunct(c)
#   define isXDIGIT(c)	isxdigit(c)
#   define toUPPER(c)	toupper(c)
#   define toLOWER(c)	tolower(c)
#else
#   define isUPPER(c)	((c) >= 'A' && (c) <= 'Z')
#   define isLOWER(c)	((c) >= 'a' && (c) <= 'z')
#   define isALNUMC(c)	(isALPHA(c) || isDIGIT(c))
#   define isASCII(c)	((c) <= 127)
#   define isCNTRL(c)	((c) < ' ')
#   define isGRAPH(c)	(isALNUM(c) || isPUNCT(c))
#   define isPRINT(c)	(((c) > 32 && (c) < 127) || isSPACE(c))
#   define isPUNCT(c)	(((c) >= 33 && (c) <= 47) || ((c) >= 58 && (c) <= 64)  || ((c) >= 91 && (c) <= 96) || ((c) >= 123 && (c) <= 126))
#   define isXDIGIT(c)  (isdigit(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#   define toUPPER(c)	(isLOWER(c) ? (c) - ('a' - 'A') : (c))
#   define toLOWER(c)	(isUPPER(c) ? (c) + ('a' - 'A') : (c))
#endif

#ifdef USE_NEXT_CTYPE

#  define isALNUM_LC(c) \
	(NXIsAlNum((unsigned int)(c)) || (char)(c) == '_')
#  define isIDFIRST_LC(c) \
	(NXIsAlpha((unsigned int)(c)) || (char)(c) == '_')
#  define isALPHA_LC(c)		NXIsAlpha((unsigned int)(c))
#  define isSPACE_LC(c)		NXIsSpace((unsigned int)(c))
#  define isDIGIT_LC(c)		NXIsDigit((unsigned int)(c))
#  define isUPPER_LC(c)		NXIsUpper((unsigned int)(c))
#  define isLOWER_LC(c)		NXIsLower((unsigned int)(c))
#  define isALNUMC_LC(c)	NXIsAlNum((unsigned int)(c))
#  define isCNTRL_LC(c)		NXIsCntrl((unsigned int)(c))
#  define isGRAPH_LC(c)		NXIsGraph((unsigned int)(c))
#  define isPRINT_LC(c)		NXIsPrint((unsigned int)(c))
#  define isPUNCT_LC(c)		NXIsPunct((unsigned int)(c))
#  define toUPPER_LC(c)		NXToUpper((unsigned int)(c))
#  define toLOWER_LC(c)		NXToLower((unsigned int)(c))

#else /* !USE_NEXT_CTYPE */

#  if defined(CTYPE256) || (!defined(isascii) && !defined(HAS_ISASCII))

#    define isALNUM_LC(c)   (isalnum((unsigned char)(c)) || (char)(c) == '_')
#    define isIDFIRST_LC(c) (isalpha((unsigned char)(c)) || (char)(c) == '_')
#    define isALPHA_LC(c)	isalpha((unsigned char)(c))
#    define isSPACE_LC(c)	isspace((unsigned char)(c))
#    define isDIGIT_LC(c)	isdigit((unsigned char)(c))
#    define isUPPER_LC(c)	isupper((unsigned char)(c))
#    define isLOWER_LC(c)	islower((unsigned char)(c))
#    define isALNUMC_LC(c)	isalnum((unsigned char)(c))
#    define isCNTRL_LC(c)	iscntrl((unsigned char)(c))
#    define isGRAPH_LC(c)	isgraph((unsigned char)(c))
#    define isPRINT_LC(c)	isprint((unsigned char)(c))
#    define isPUNCT_LC(c)	ispunct((unsigned char)(c))
#    define toUPPER_LC(c)	toupper((unsigned char)(c))
#    define toLOWER_LC(c)	tolower((unsigned char)(c))

#  else

#    define isALNUM_LC(c) 	(isascii(c) && (isalnum(c) || (c) == '_'))
#    define isIDFIRST_LC(c)	(isascii(c) && (isalpha(c) || (c) == '_'))
#    define isALPHA_LC(c)	(isascii(c) && isalpha(c))
#    define isSPACE_LC(c)	(isascii(c) && isspace(c))
#    define isDIGIT_LC(c)	(isascii(c) && isdigit(c))
#    define isUPPER_LC(c)	(isascii(c) && isupper(c))
#    define isLOWER_LC(c)	(isascii(c) && islower(c))
#    define isALNUMC_LC(c)	(isascii(c) && isalnum(c))
#    define isCNTRL_LC(c)	(isascii(c) && iscntrl(c))
#    define isGRAPH_LC(c)	(isascii(c) && isgraph(c))
#    define isPRINT_LC(c)	(isascii(c) && isprint(c))
#    define isPUNCT_LC(c)	(isascii(c) && ispunct(c))
#    define toUPPER_LC(c)	toupper(c)
#    define toLOWER_LC(c)	tolower(c)

#  endif
#endif /* USE_NEXT_CTYPE */

#define isPSXSPC_LC(c)		(isSPACE_LC(c) || (c) == '\v')
#define isBLANK_LC(c)		isBLANK(c) /* could be wrong */

#define isALNUM_uni(c)		is_uni_alnum(c)
#define isIDFIRST_uni(c)	is_uni_idfirst(c)
#define isALPHA_uni(c)		is_uni_alpha(c)
#define isSPACE_uni(c)		is_uni_space(c)
#define isDIGIT_uni(c)		is_uni_digit(c)
#define isUPPER_uni(c)		is_uni_upper(c)
#define isLOWER_uni(c)		is_uni_lower(c)
#define isALNUMC_uni(c)		is_uni_alnumc(c)
#define isASCII_uni(c)		is_uni_ascii(c)
#define isCNTRL_uni(c)		is_uni_cntrl(c)
#define isGRAPH_uni(c)		is_uni_graph(c)
#define isPRINT_uni(c)		is_uni_print(c)
#define isPUNCT_uni(c)		is_uni_punct(c)
#define isXDIGIT_uni(c)		is_uni_xdigit(c)
#define toUPPER_uni(c)		to_uni_upper(c)
#define toTITLE_uni(c)		to_uni_title(c)
#define toLOWER_uni(c)		to_uni_lower(c)

#define isPSXSPC_uni(c)		(isSPACE_uni(c) ||(c) == '\f')
#define isBLANK_uni(c)		isBLANK(c) /* could be wrong */

#define isALNUM_LC_uni(c)	(c < 256 ? isALNUM_LC(c) : is_uni_alnum_lc(c))
#define isIDFIRST_LC_uni(c)	(c < 256 ? isIDFIRST_LC(c) : is_uni_idfirst_lc(c))
#define isALPHA_LC_uni(c)	(c < 256 ? isALPHA_LC(c) : is_uni_alpha_lc(c))
#define isSPACE_LC_uni(c)	(c < 256 ? isSPACE_LC(c) : is_uni_space_lc(c))
#define isDIGIT_LC_uni(c)	(c < 256 ? isDIGIT_LC(c) : is_uni_digit_lc(c))
#define isUPPER_LC_uni(c)	(c < 256 ? isUPPER_LC(c) : is_uni_upper_lc(c))
#define isLOWER_LC_uni(c)	(c < 256 ? isLOWER_LC(c) : is_uni_lower_lc(c))
#define isALNUMC_LC_uni(c)	(c < 256 ? isALNUMC_LC(c) : is_uni_alnumc_lc(c))
#define isCNTRL_LC_uni(c)	(c < 256 ? isCNTRL_LC(c) : is_uni_cntrl_lc(c))
#define isGRAPH_LC_uni(c)	(c < 256 ? isGRAPH_LC(c) : is_uni_graph_lc(c))
#define isPRINT_LC_uni(c)	(c < 256 ? isPRINT_LC(c) : is_uni_print_lc(c))
#define isPUNCT_LC_uni(c)	(c < 256 ? isPUNCT_LC(c) : is_uni_punct_lc(c))
#define toUPPER_LC_uni(c)	(c < 256 ? toUPPER_LC(c) : to_uni_upper_lc(c))
#define toTITLE_LC_uni(c)	(c < 256 ? toUPPER_LC(c) : to_uni_title_lc(c))
#define toLOWER_LC_uni(c)	(c < 256 ? toLOWER_LC(c) : to_uni_lower_lc(c))

#define isPSXSPC_LC_uni(c)	(isSPACE_LC_uni(c) ||(c) == '\f')
#define isBLANK_LC_uni(c)	isBLANK(c) /* could be wrong */

#define isALNUM_utf8(p)		is_utf8_alnum(p)
#define isIDFIRST_utf8(p)	is_utf8_idfirst(p)
#define isALPHA_utf8(p)		is_utf8_alpha(p)
#define isSPACE_utf8(p)		is_utf8_space(p)
#define isDIGIT_utf8(p)		is_utf8_digit(p)
#define isUPPER_utf8(p)		is_utf8_upper(p)
#define isLOWER_utf8(p)		is_utf8_lower(p)
#define isALNUMC_utf8(p)	is_utf8_alnumc(p)
#define isASCII_utf8(p)		is_utf8_ascii(p)
#define isCNTRL_utf8(p)		is_utf8_cntrl(p)
#define isGRAPH_utf8(p)		is_utf8_graph(p)
#define isPRINT_utf8(p)		is_utf8_print(p)
#define isPUNCT_utf8(p)		is_utf8_punct(p)
#define isXDIGIT_utf8(p)	is_utf8_xdigit(p)
#define toUPPER_utf8(p)		to_utf8_upper(p)
#define toTITLE_utf8(p)		to_utf8_title(p)
#define toLOWER_utf8(p)		to_utf8_lower(p)

#define isPSXSPC_utf8(c)	(isSPACE_utf8(c) ||(c) == '\f')
#define isBLANK_utf8(c)		isBLANK(c) /* could be wrong */

#define isALNUM_LC_utf8(p)	isALNUM_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isIDFIRST_LC_utf8(p)	isIDFIRST_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isALPHA_LC_utf8(p)	isALPHA_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isSPACE_LC_utf8(p)	isSPACE_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isDIGIT_LC_utf8(p)	isDIGIT_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isUPPER_LC_utf8(p)	isUPPER_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isLOWER_LC_utf8(p)	isLOWER_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isALNUMC_LC_utf8(p)	isALNUMC_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isCNTRL_LC_utf8(p)	isCNTRL_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isGRAPH_LC_utf8(p)	isGRAPH_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isPRINT_LC_utf8(p)	isPRINT_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define isPUNCT_LC_utf8(p)	isPUNCT_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define toUPPER_LC_utf8(p)	toUPPER_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define toTITLE_LC_utf8(p)	toTITLE_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))
#define toLOWER_LC_utf8(p)	toLOWER_LC_uni(utf8_to_uv(p, UTF8_MAXLEN, 0, 0))

#define isPSXSPC_LC_utf8(c)	(isSPACE_LC_utf8(c) ||(c) == '\f')
#define isBLANK_LC_utf8(c)	isBLANK(c) /* could be wrong */

#ifdef EBCDIC
#  define toCTRL(c)	Perl_ebcdic_control(c)
#else
  /* This conversion works both ways, strangely enough. */
#  define toCTRL(c)    (toUPPER(c) ^ 64)
#endif

/* Line numbers are unsigned, 16 bits. */
typedef U16 line_t;
#ifdef lint
#define NOLINE ((line_t)0)
#else
#define NOLINE ((line_t) 65535)
#endif


/*
   XXX LEAKTEST doesn't really work in perl5.  There are direct calls to
   safemalloc() in the source, so LEAKTEST won't pick them up.
   (The main "offenders" are extensions.)
   Further, if you try LEAKTEST, you'll also end up calling
   Safefree, which might call safexfree() on some things that weren't
   malloced with safexmalloc.  The correct "fix" to this, if anyone
   is interested, is to ensure that all calls go through the New and
   Renew macros.
	--Andy Dougherty		August 1996
*/

/*
=for apidoc Am|SV*|NEWSV|int id|STRLEN len
Creates a new SV.  A non-zero C<len> parameter indicates the number of
bytes of preallocated string space the SV should have.  An extra byte for a
tailing NUL is also reserved.  (SvPOK is not set for the SV even if string
space is allocated.)  The reference count for the new SV is set to 1.
C<id> is an integer id between 0 and 1299 (used to identify leaks).

=for apidoc Am|void|New|int id|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<malloc> function.

=for apidoc Am|void|Newc|int id|void* ptr|int nitems|type|cast
The XSUB-writer's interface to the C C<malloc> function, with
cast.

=for apidoc Am|void|Newz|int id|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<malloc> function.  The allocated
memory is zeroed with C<memzero>.

=for apidoc Am|void|Renew|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<realloc> function.

=for apidoc Am|void|Renewc|void* ptr|int nitems|type|cast
The XSUB-writer's interface to the C C<realloc> function, with
cast.

=for apidoc Am|void|Safefree|void* ptr
The XSUB-writer's interface to the C C<free> function.

=for apidoc Am|void|Move|void* src|void* dest|int nitems|type
The XSUB-writer's interface to the C C<memmove> function.  The C<src> is the
source, C<dest> is the destination, C<nitems> is the number of items, and C<type> is
the type.  Can do overlapping moves.  See also C<Copy>.

=for apidoc Am|void|Copy|void* src|void* dest|int nitems|type
The XSUB-writer's interface to the C C<memcpy> function.  The C<src> is the
source, C<dest> is the destination, C<nitems> is the number of items, and C<type> is
the type.  May fail on overlapping copies.  See also C<Move>.

=for apidoc Am|void|Zero|void* dest|int nitems|type

The XSUB-writer's interface to the C C<memzero> function.  The C<dest> is the
destination, C<nitems> is the number of items, and C<type> is the type.

=for apidoc Am|void|StructCopy|type src|type dest|type
This is an architecture-independent macro to copy one structure to another.

=cut
*/

#ifndef lint

#define NEWSV(x,len)	newSV(len)

#ifndef LEAKTEST

#define New(x,v,n,t)	(v = (t*)safemalloc((MEM_SIZE)((n)*sizeof(t))))
#define Newc(x,v,n,t,c)	(v = (c*)safemalloc((MEM_SIZE)((n)*sizeof(t))))
#define Newz(x,v,n,t)	(v = (t*)safemalloc((MEM_SIZE)((n)*sizeof(t)))), \
			memzero((char*)(v), (n)*sizeof(t))
#define Renew(v,n,t) \
	  (v = (t*)saferealloc((Malloc_t)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Renewc(v,n,t,c) \
	  (v = (c*)saferealloc((Malloc_t)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Safefree(d)	safefree((Malloc_t)(d))

#else /* LEAKTEST */

#define New(x,v,n,t)	(v = (t*)safexmalloc((x),(MEM_SIZE)((n)*sizeof(t))))
#define Newc(x,v,n,t,c)	(v = (c*)safexmalloc((x),(MEM_SIZE)((n)*sizeof(t))))
#define Newz(x,v,n,t)	(v = (t*)safexmalloc((x),(MEM_SIZE)((n)*sizeof(t)))), \
			 memzero((char*)(v), (n)*sizeof(t))
#define Renew(v,n,t) \
	  (v = (t*)safexrealloc((Malloc_t)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Renewc(v,n,t,c) \
	  (v = (c*)safexrealloc((Malloc_t)(v),(MEM_SIZE)((n)*sizeof(t))))
#define Safefree(d)	safexfree((Malloc_t)(d))

#define MAXXCOUNT 1400
#define MAXY_SIZE 80
#define MAXYCOUNT 16			/* (MAXY_SIZE/4 + 1) */
extern long xcount[MAXXCOUNT];
extern long lastxcount[MAXXCOUNT];
extern long xycount[MAXXCOUNT][MAXYCOUNT];
extern long lastxycount[MAXXCOUNT][MAXYCOUNT];

#endif /* LEAKTEST */

#define Move(s,d,n,t)	(void)memmove((char*)(d),(char*)(s), (n) * sizeof(t))
#define Copy(s,d,n,t)	(void)memcpy((char*)(d),(char*)(s), (n) * sizeof(t))
#define Zero(d,n,t)	(void)memzero((char*)(d), (n) * sizeof(t))

#else /* lint */

#define New(x,v,n,s)	(v = Null(s *))
#define Newc(x,v,n,s,c)	(v = Null(s *))
#define Newz(x,v,n,s)	(v = Null(s *))
#define Renew(v,n,s)	(v = Null(s *))
#define Move(s,d,n,t)
#define Copy(s,d,n,t)
#define Zero(d,n,t)
#define Safefree(d)	(d) = (d)

#endif /* lint */

#ifdef USE_STRUCT_COPY
#define StructCopy(s,d,t) (*((t*)(d)) = *((t*)(s)))
#else
#define StructCopy(s,d,t) Copy(s,d,1,t)
#endif
