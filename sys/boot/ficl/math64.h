/*******************************************************************
** m a t h 6 4 . h
** Forth Inspired Command Language - 64 bit math support routines
** Author: John Sadler (john_sadler@alum.mit.edu)
** Created: 25 January 1998
** 
*******************************************************************/
/*
** N O T I C E -- DISCLAIMER OF WARRANTY
** 
** Ficl is freeware. Use it in any way that you like, with
** the understanding that the code is not supported.
** 
** Any third party may reproduce, distribute, or modify the ficl
** software code or any derivative  works thereof without any 
** compensation or license, provided that the author information
** and this disclaimer text are retained in the source code files.
** The ficl software code is provided on an "as is"  basis without
** warranty of any kind, including, without limitation, the implied
** warranties of merchantability and fitness for a particular purpose
** and their equivalents under the laws of any jurisdiction.  
** 
** I am interested in hearing from anyone who uses ficl. If you have
** a problem, a success story, a defect, an enhancement request, or
** if you would like to contribute to the ficl release (yay!), please
** send me email at the address above. 
*/

#if !defined (__MATH64_H__)
#define __MATH64_H__

#ifdef __cplusplus
extern "C" {
#endif

INT64 m64Abs(INT64 x);
int   m64IsNegative(INT64 x);
UNS64 m64Mac(UNS64 u, UNS32 mul, UNS32 add);
INT64 m64MulI(INT32 x, INT32 y);
INT64 m64Negate(INT64 x);
INTQR m64FlooredDivI(INT64 num, INT32 den);
void  i64Push(FICL_STACK *pStack, INT64 i64);
INT64 i64Pop(FICL_STACK *pStack);
void  u64Push(FICL_STACK *pStack, UNS64 u64);
UNS64 u64Pop(FICL_STACK *pStack);
INTQR m64SymmetricDivI(INT64 num, INT32 den);
UNS16 m64UMod(UNS64 *pUD, UNS16 base);

#define i64Extend(i64) (i64).hi = ((i64).lo < 0) ? -1L : 0 
#define m64CastIU(i64) (*(UNS64 *)(&(i64)))
#define m64CastUI(u64) (*(INT64 *)(&(u64)))
#define m64CastQRIU(iqr) (*(UNSQR *)(&(iqr)))
#define m64CastQRUI(uqr) (*(INTQR *)(&(uqr)))

#ifdef __cplusplus
}
#endif

#endif

