#ifndef _BS_PRIMS
#define _BS_PRIMS

/* A bitstring is an array of _BS_word. */
typedef unsigned long _BS_word;

#define _BS_CHAR_BIT 8
#define _BS_BITS_PER_WORD (_BS_CHAR_BIT*sizeof(_BS_word))
#define _BS_WORDS_NEEDED(NBITS) ((NBITS+_BS_BITS_PER_WORD-1)/_BS_BITS_PER_WORD)

/* For now, we number the bits in a _BS_word in little-endian order.
   Later, might use machine order. */
#ifdef CHILL_LIB
#ifndef BITS_BIG_ENDIAN
#include "config.h"
#endif
#define _BS_BIGENDIAN BITS_BIG_ENDIAN
#else
#define _BS_BIGENDIAN 0
#endif

/* By "left" we mean where bit number 0 is.
   Hence, so left shift is << if we're numbering the bits in big-endian oder,
   and >> if we're numbering the bits in little-endian order.
   Currently, we always use little-endian order.
   Later, we might use machine-endian order. */
#if _BS_BIGENDIAN
#define _BS_LEFT <<
#define _BS_RIGHT >>
#else
#define _BS_LEFT >>
#define _BS_RIGHT <<
#endif

#if _BS_BIGENDIAN
#define _BS_BITMASK(BITNO) ((_BS_word)1 << (_BS_BITS_PER_WORD - 1 - (BITNO)))
#else
#define _BS_BITMASK(BITNO) ((_BS_word)1 << (BITNO))
#endif

/* Given a PTR which may not be aligned on a _BS_word boundary,
   set NEW_PTR to point to (the beginning of) the corresponding _BS_word.
   Adjust the bit-offset OFFSET to compensate for the difference. */
#define _BS_ADJUST_ALIGNED(NEW_PTR, PTR, OFFSET) \
  ( (NEW_PTR) = (_BS_word*)(((char*)(PTR)-(char*)0) & ~(sizeof(_BS_word)-1)), \
    (OFFSET) += (char*)(PTR) - (char*)(NEW_PTR) )

/* Given a bit pointer (PTR, OFFSET) normalize it so that
   OFFSET < _BS_BITS_PER_WORD. */
#define _BS_NORMALIZE(PTR, OFFSET) \
{ _BS_size_t __tmp_ind = _BS_INDEX (OFFSET); \
  (PTR) += __tmp_ind; \
  (OFFSET) -= __tmp_ind * _BS_BITS_PER_WORD; }

#define _BS_INDEX(I) ((unsigned)(I) / _BS_BITS_PER_WORD)
#define _BS_POS(I) ((I) & (_BS_BITS_PER_WORD -1 ))

#ifndef _BS_size_t
#if __GNUC__ > 1
#define _BS_size_t __SIZE_TYPE__
#else
#define _BS_size_t unsigned long
#endif
#endif

#ifndef __P
#ifdef __STDC__
#define __P(protos) protos
#else
#define __P(protos) ()
#endif
#endif /*!__P*/
#if !defined(__STDC__) && !defined(const)
#define const
#endif
#if !defined(__STDC__) && !defined(void)
#define void int
#endif

/* The 16 2-operand raster-ops:
   These match the correspodning GX codes in X11. */
enum _BS_alu {
  _BS_alu_clear        =  0 /* 0 */,
  _BS_alu_and          =  1 /* src & dst */,
  _BS_alu_andReverse   =  2 /* src & ~dst */,
  _BS_alu_copy         =  3 /* src */,
  _BS_alu_andInverted  =  4 /* ~src & dst */,
  _BS_alu_noop         =  5 /* dst */,
  _BS_alu_xor          =  6 /* src ^ dst */,
  _BS_alu_or           =  7 /* src | dst */,
  _BS_alu_nor          =  8 /* ~src & ~dst */,
  _BS_alu_equiv        =  9 /* ~(src ^ dst) */,
  _BS_alu_invert       = 10 /* ~dst */,
  _BS_alu_orReverse    = 11 /* src | ~dst */,
  _BS_alu_copyInverted = 12 /* ~src */,
  _BS_alu_orInverted   = 13 /* ~src | dst */,
  _BS_alu_nand         = 14 /* ~src | d~st */,
  _BS_alu_set          = 15 /* ~src | dst */
};
#define _BS
#define _BS

#ifdef __cplusplus
extern "C" {
#endif

extern void _BS_and __P((_BS_word*,int, const _BS_word*, int, _BS_size_t));
extern void _BS_blt __P((enum _BS_alu,
			     _BS_word*,int, const _BS_word*,int, _BS_size_t));
extern void _BS_copy __P((_BS_word*,int, const _BS_word*,int, _BS_size_t));
#define _BS_copy_0(DS, SS, LENGTH) _BS_copy(DS, 0, SS, 0, LENGTH)
extern int _BS_count __P((const _BS_word*, int, _BS_size_t));
extern int _BS_any __P((const _BS_word*, int, _BS_size_t));
extern void _BS_clear __P((_BS_word*, int, _BS_size_t));
extern void _BS_set __P((_BS_word*, int, _BS_size_t));
extern void _BS_invert __P((_BS_word*, int, _BS_size_t));
int _BS_lcompare_0 __P((const _BS_word*, _BS_size_t,
			const _BS_word*, _BS_size_t));
extern void _BS_xor __P((_BS_word*,int, const _BS_word*,int, _BS_size_t));

#ifdef __cplusplus
}
#endif

#endif /* !_BS_PRIMS */
