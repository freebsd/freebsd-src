/*
 * Copyright (c) 1996-1999 Distributed Processing Technology Corporation
 * All rights reserved.
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of x driver software, even if advised
 * of the possibility of such damage.
 *
 * DPT Alignment Description File
 *
 * $FreeBSD$
 */
#if (!defined(__DPTALIGN_H))
#define       __DPTALIGN_H

/*
 *      File -  DPTALIGN.H
 *
 *      Description:  This file contains basic Alignment support definitions.
 *
 *      Copyright Distributed Processing Technology, Corp.
 *        140 Candace Dr.
 *        Maitland, Fl. 32751   USA
 *        Phone: (407) 830-5522  Fax: (407) 260-5366
 *        All Rights Reserved
 *
 *      Author: Mark Salyzyn
 *      Date:   Aug 29 1996
 *
 *
 *	Fifth Gen product enhancements and additions
 *      Author: Ben Ghofrani
 *      Date:   April 6 1998
 */

/*
 *      Description: Support macros for active alignment
 *      Requires:
 *              osdLocal2(x)
 *              osdLocal4(x)
 *              osdSwap2(x)
 *              osdSwap4(x)
 */
#if (!defined(__FAR__))
# if (defined(__BORLANDC__))
#  define __FAR__ far
# else
#  define __FAR__
# endif
#endif


#if (defined(sun)) && (!defined(_ILP32))
#  define DPT_4_BYTES int 	/* 64 bit OS */
#else
#  define DPT_4_BYTES long
#endif

#if (!defined(osdSwap2))
/*
 *      Name: osdSwap2(value)
 *      Description: Mandatory byte swapping routine for words. We allow an
 *      override of x routine if the OS supplies it's own byte swapping
 *      routine, inline or macro.
 */
# define osdSwap2(x) (((unsigned short)(x) >> 8) \
                    | ((unsigned short)((unsigned char)(x)) << 8))
#endif
#if (!defined(osdSwap4))
/*
 *      Name: osdSwap4(value)
 *      Description: Mandatory byte swapping routine for DPT_4_BYTES words. We allow
 *      an override of x routine if the OS supplies it's own byte swapping
 *      routine, inline or macro. The following is universal, but may be
 *      more optimally performed by an OS or driver processor dependant
 *      routine.
 */
# define osdSwap4(x)     (                                                   \
    (((unsigned DPT_4_BYTES)(x)) >> 24L)                                            \
  | ((unsigned DPT_4_BYTES)(((unsigned short)((unsigned DPT_4_BYTES)(x) >> 8L)) & 0xFF00)) \
  | (((unsigned DPT_4_BYTES)(((unsigned short)(x)) & 0xFF00)) << 8L)                \
  | (((unsigned DPT_4_BYTES)((unsigned char)(x))) << 24L))
#endif



#if (!defined(osdLocal2))
/*
 *      Name: osdLocal2(pointer)
 *      Description: Local byte order to Big Endian Format for short words.
 *      Could be replaced with an OS defined localization routine, macro or
 *      inline.
 */
# if (defined(_DPT_BIG_ENDIAN))
#  define osdLocal2(x)   (*((unsigned short __FAR__ *)(x)))
#  if (defined(osdSwap2))
#   define osdSLocal2(x) osdSwap2(osdLocal2(x))
#  else
#   define osdSLocal2(x) ((unsigned short)(((unsigned char __FAR__ *)(x))[1])\
               + ((unsigned int)((unsigned short)(((unsigned char __FAR__ *)(x))[0])) << 8))
#  endif
# else
#  define osdSLocal2(x)  (*((unsigned short __FAR__ *)(x)))
#  if (defined(osdSwap2))
#   define osdLocal2(x)  osdSwap2(osdSLocal2(x))
#  else
#   define osdLocal2(x)  ((unsigned short)(((unsigned char __FAR__*)(x))[1]) \
                + (((unsigned short)(((unsigned char __FAR__*)(x))[0])) << 8))
#  endif
# endif
#endif
#if (!defined(osdLocal3))
/*
 *      Name: osdLocal3(pointer)
 *      Description: Local byte order to Big Endian Format for DPT_4_BYTES words.
 *      Could be replaced with an OS defined localization routine, macro or
 *      inline.
 */
# if (defined(_DPT_BIG_ENDIAN))
#  define osdLocal3(x)  (*((unsigned DPT_4_BYTES __FAR__ *)(x)))
# else
#  if (defined(osdSwap3))
#   define osdLocal3(x) osdSwap3(*((unsigned DPT_4_BYTES __FAR__ *)(x)))
#  else
#   define osdLocal3(x) ((unsigned DPT_4_BYTES)osdLocal2(((unsigned char __FAR__ *) \
       (x)+1)) + (((unsigned DPT_4_BYTES)(((unsigned char __FAR__ *)(x))[0])) << 16))
#  endif
# endif
#endif



#if (!defined(osdLocal4))
/*
 *      Name: osdLocal4(pointer)
 *      Description: Local byte order to Big Endian Format for DPT_4_BYTES words.
 *      Could be replaced with an OS defined localization routine, macro or
 *      inline.
 */
# if (defined(_DPT_BIG_ENDIAN))
#  define osdLocal4(x)   (*(unsigned DPT_4_BYTES __FAR__ *)(x))
#  if (defined(osdSwap4))
#   define osdSLocal4(x) osdSwap4(osdLocal4(x))
#  else
#   define osdSLocal4(x) ((unsigned DPT_4_BYTES)osdSLocal2(((unsigned char __FAR__ *)\
    (x)+2)) + (((unsigned DPT_4_BYTES)((unsigned char __FAR__ *)(x))[1]) << 16) \
            + (((unsigned DPT_4_BYTES)((unsigned char __FAR__ *)(x))[0]) << 24))
#  endif
# else
#  define osdSLocal4(x) (*(unsigned DPT_4_BYTES __FAR__ *)(x))
#  if (defined(osdSwap4))
#   define osdLocal4(x) osdSwap4(osdSLocal4(x))
#  else
#   define osdLocal4(x) ((unsigned DPT_4_BYTES)osdLocal2(((unsigned char __FAR__ *) \
        (x)+2)) + (((unsigned DPT_4_BYTES)((unsigned char __FAR__ *)(x))[1]) << 16) \
                + (((unsigned DPT_4_BYTES)((unsigned char __FAR__ *)(x))[0]) << 24))
#  endif
# endif
#endif

#define I2O_TID_MASK	((unsigned DPT_4_BYTES) ((1L<<I2O_TID_SZ)-1))

/*
 *      Now the access macros used throughout in order to methodize the
 * active alignment.
 */
#define getUP1(x,y)  (((unsigned char __FAR__ *)(x))+(unsigned DPT_4_BYTES)(y))
#define getU1(x,y)   (*getUP1(x,y))
#define setU1(x,y,z) (*((unsigned char *)getUP1(x,y)) = (unsigned char)(z))
#define orU1(x,y,z)  (*getUP1(x,y) |= (unsigned char)(z))
#define andU1(x,y,z) (*getUP1(x,y) &= (unsigned char)(z))
#define getUP2(x,y)  ((unsigned short __FAR__ *)(((unsigned char __FAR__ *) \
                                (x))+(unsigned DPT_4_BYTES)(y)))
#define getBU2(x,y)   ((unsigned short)osdLocal2((unsigned short __FAR__ *)  \
                                getUP1(x,y)))
#define getLU2(x,y)  ((unsigned short)osdSLocal2((unsigned short __FAR__ *) \
                                getUP1(x,y)))
/* to be deleted  */
#define getU2(x,y)   ((unsigned short)osdLocal2((unsigned short __FAR__ *)  \
                                getUP1(x,y)))
#if (!defined(setU2))
# define setU2(x,y,z) { unsigned short hold = (unsigned short)(z);  \
                        *((unsigned short __FAR__ *)getUP1(x,y))    \
                          = osdLocal2(&hold);                       \
                      }
#endif
#if (!defined(setBU2))
# define setBU2(x,y,z) { unsigned short hold = (unsigned short)(z);  \
                        *((unsigned short __FAR__ *)getUP1(x,y))    \
                          = osdLocal2(&hold);                       \
                      }
#endif
#if (!defined(setLU2))
# define setLU2(x,y,z) { unsigned short hold = (unsigned short)(z); \
                         *((unsigned short __FAR__ *)getUP1(x,y))   \
                           = osdSLocal2(&hold);                     \
                       }
#endif

/* to be deleted */
#define getU3(x,y)   ((unsigned DPT_4_BYTES)osdLocal3((unsigned DPT_4_BYTES __FAR__ *) \
                                getUP1(x,y)))
#if (!defined(setU3))
# if (defined(_DPT_BIG_ENDIAN))
#  define setU3(x,y,z)                                     \
        {   unsigned DPT_4_BYTES hold = z;                        \
            *(getUP1(x,y)) = (unsigned char)(hold >> 16L); \
            *((unsigned short __FAR__ *)(getUP1(x,y) + 1)) \
              = (unsigned short)hold;                      \
        }
# else
#  define setU3(x,y,z) \
        {   unsigned DPT_4_BYTES hold = z;                            \
            *(getUP1(x,y) + 0) = (unsigned char)(hold >> 16) ; \
            *(getUP1(x,y) + 1) = (unsigned char)(hold >> 8L);  \
            *(getUP1(x,y) + 2) = (unsigned char)(hold);        \
        }
# endif
#endif
/* up to here to be deleted */

#define getBU3(x,y)   ((unsigned DPT_4_BYTES)osdLocal3((unsigned DPT_4_BYTES __FAR__ *) \
                                getUP1(x,y)))
#if (!defined(setBU3))
# if (defined(_DPT_BIG_ENDIAN))
#  define setBU3(x,y,z)                                     \
        {   unsigned DPT_4_BYTES hold = z;                        \
            *(getUP1(x,y)) = (unsigned char)(hold >> 16L); \
            *((unsigned short __FAR__ *)(getUP1(x,y) + 1)) \
              = (unsigned short)hold;                      \
        }
# else
#  define setBU3(x,y,z) \
        {   unsigned DPT_4_BYTES hold = z;                            \
            *(getUP1(x,y) + 0) = (unsigned char)(hold >> 16) ; \
            *(getUP1(x,y) + 1) = (unsigned char)(hold >> 8L);  \
            *(getUP1(x,y) + 2) = (unsigned char)(hold);        \
        }
# endif
#endif
#define getUP4(x,y)  ((unsigned DPT_4_BYTES __FAR__ *)(((unsigned char __FAR__ *) \
                                (x))+(unsigned DPT_4_BYTES)(y)))
#define getBU4(x,y)   ((unsigned DPT_4_BYTES)osdLocal4((unsigned DPT_4_BYTES __FAR__ *)   \
                                getUP1(x,y)))
#define getLU4(x,y)  ((unsigned DPT_4_BYTES)osdSLocal4((unsigned DPT_4_BYTES __FAR__ *)  \
                                getUP1(x,y)))
/* to be deleted */
#define getU4(x,y)  ((unsigned DPT_4_BYTES)osdSLocal4((unsigned DPT_4_BYTES __FAR__ *)  \
                                getUP1(x,y)))
#if (!defined(setU4))
# define setU4(x,y,z) { unsigned DPT_4_BYTES hold = z;                 \
                        *((unsigned DPT_4_BYTES __FAR__ *)getUP1(x,y)) \
                          = osdLocal4(&hold);                   \
                      }
#endif
/* up to here */
#if (!defined(setBU4))
# define setBU4(x,y,z) { unsigned DPT_4_BYTES hold = z;                 \
                        *((unsigned DPT_4_BYTES __FAR__ *)getUP1(x,y)) \
                          = osdLocal4(&hold);                   \
                      }
#endif
#if (!defined(setLU4))
# define setLU4(x,y,z) { unsigned DPT_4_BYTES hold = z;                 \
                         *((unsigned DPT_4_BYTES __FAR__ *)getUP1(x,y)) \
                           = osdSLocal4(&hold);                  \
                       }
#endif

	
#define osdSwap16bit(x)	( (((unsigned short )x & 0xf000) >> 12) | \
			  (((unsigned short )x & 0x0f00) >> 4) | \
			  (((unsigned short )x & 0x00f0) << 4)  | \
			  (((unsigned short )x & 0x000f) << 12 )   )	

/*
 * note that in big endian a 12 bit number (0x123) is stored as   1203
 */

#define osdSwap12bit(x)	(( (((unsigned short )x & 0x0f00) >> 8) | \
			((unsigned short )x & 0x00f0)  | \
			(((unsigned short )x & 0x000f) << 8 )  ) )	

#define osdSwap8bit(x)	( (((unsigned char )x & 0x0f) << 4) | \
			(((unsigned char )x &0xf0) >> 4 ) )

#define getL24bit1(w,x,y)   ((unsigned DPT_4_BYTES)((unsigned char __FAR__ *)(&w->x))[0+(y)] \
			+ ((((unsigned DPT_4_BYTES)((unsigned char __FAR__ *)(&w->x))[1+(y)]) << 8) & 0xFF00) \
			+ ((((unsigned DPT_4_BYTES)((unsigned char __FAR__ *)(&w->x))[2+(y)]) << 16) & 0xFF0000))

#define setL24bit1(w,x,y,z)  { ((unsigned char __FAR__ *)(&w->x))[0+(y)] = (z); \
			   ((unsigned char __FAR__ *)(&w->x))[1+(y)] = ((z) >> 8) & 0xFF; \
			   ((unsigned char __FAR__ *)(&w->x))[2+(y)] = ((z) >> 16) & 0xFF; \
			   }

#define getL16bit(w,x,y)   ((unsigned short)((unsigned char __FAR__ *)(&w->x))[0+(y)] \
			 + ((((unsigned short)((unsigned char __FAR__ *)(&w->x))[1+(y)]) << 8) & 0xFF00))

#define setL16bit(w,x,y,z)  { ((unsigned char __FAR__ *)(&w->x))[0+(y)] = (z); \
			   ((unsigned char __FAR__ *)(&w->x))[1+(y)] = ((z) >> 8) & 0xFF; \
			   }

#define getL16bit2(w,x,y)   ((unsigned short)((unsigned char __FAR__ *)(&w->x))[2+(y)] \
			 + ((((unsigned short)((unsigned char __FAR__ *)(&w->x))[3+(y)]) << 8) & 0xFF00))

#define setL16bit2(w,x,y,z)  { ((unsigned char __FAR__ *)(&w->x))[2+(y)] = (z); \
			   ((unsigned char __FAR__ *)(&w->x))[3+(y)] = ((z) >> 8) & 0xFF; \
			   }

/* y is the number of bytes from beg of DPT_4_BYTES to get upper 4 bit of the addressed byte */
#define getL4bit(w,x,y) \
	((unsigned char)(((unsigned char __FAR__ *)(&w->x))[0+(y)] >> 4) & 0x0f)

#define setL4bit(w,x,y,z) { \
			   ((unsigned char __FAR__ *)(&w->x))[0+(y)] &= 0xF0; \
				((unsigned char __FAR__ *)(&w->x))[0+(y)] |= ((z) << 4) & 0xF0; \
				}
/* y is number of bytes from beg of DPT_4_BYTES */
#define getL1bit(w,x,y) \
	((unsigned char)(((unsigned char __FAR__ *)(&w->x))[0+(y)] ) & 0x01)

#define setL1bit(w,x,y,z) { \
			   ((unsigned char __FAR__ *)(&w->x))[0+(y)] &= 0xFE; \
				((unsigned char __FAR__ *)(&w->x))[0+(y)] |= (z) & 0x01; \
				}
#define getL1bit1(w,x,y) \
	((unsigned char)(((unsigned char __FAR__ *)(&w->x))[0+(y)] >> 1) & 0x01)

#define setL1bit1(w,x,y,z) { \
			   ((unsigned char __FAR__ *)(&w->x))[0+(y)] &= 0xFD; \
				((unsigned char __FAR__ *)(&w->x))[0+(y)] |= (z << 1) & 0x02; \
				}



/* 12 bit at the first 12 bits of a DPT_4_BYTES word */
#define getL12bit(w,x,y)   ((unsigned short)((unsigned char __FAR__ *)(&w->x))[0+(y)] \
			 + ((((unsigned short)((unsigned char __FAR__ *)(&w->x))[1+(y)]) << 8) & 0xF00))

#define setL12bit(w,x,y,z) { ((unsigned char __FAR__ *)(&w->x))[0+(y)] = (z); \
			   ((unsigned char __FAR__ *)(&w->x))[1+(y)] &= 0xF0; \
			   ((unsigned char __FAR__ *)(&w->x))[1+(y)] |= ((z) >> 8) & 0xF; \
			   }
/* 12 bit after another 12 bit in DPT_4_BYTES word */
#define getL12bit1(w,x,y)   (((unsigned short)((unsigned char __FAR__ *)(&w->x))[1+(y)]) >> 4 \
			 + ((((unsigned short)((unsigned char __FAR__ *)(&w->x))[2+(y)]) << 4) ))

#define setL12bit1(w,x,y,z) { ((unsigned char __FAR__ *)(&w->x))[1+(y)] &= 0x0F; \
			   ((unsigned char __FAR__ *)(&w->x))[1+(y)] |= ((z) & 0xF) << 4; \
			   ((unsigned char __FAR__ *)(&w->x))[2+(y)] &= 0x00;\
			   ((unsigned char __FAR__ *)(&w->x))[2+(y)] |= ((z) >> 8) & 0xff;\
			   }

/* 12 at the 3rd byte in a DPT_4_BYTES word */
#define getL12bit2(w,x,y)   ((unsigned short)((unsigned char __FAR__ *)(&w->x))[2+(y)] \
			 + ((((unsigned short)((unsigned char __FAR__ *)(&w->x))[3+(y)]) << 8) & 0xF00))

#define setL12bit2(w,x,y,z) { ((unsigned char __FAR__ *)(&w->x))[2+(y)] = (z); \
			   ((unsigned char __FAR__ *)(&w->x))[3+(y)] &= 0xF0; \
			   ((unsigned char __FAR__ *)(&w->x))[3+(y)] |= ((z) >> 8) & 0xF; \
			   }

#define getL8bit(w,x,y)    (\
	(*(((unsigned char __FAR__ *)(&((w)->x)))\
		+ y)) )

#define setL8bit(w,x,y,z)  {\
	(*(((unsigned char __FAR__ *)(&((w)->x)))\
		+ y) = (z));\
	}


#endif /* __DPTALIGN_H */
