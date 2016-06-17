/* -*- linux-c -*- */
/*
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

/****************************************************/
/****************************************************/
/*	    Begin header file "endian.h"	    */
/****************************************************/
/****************************************************/

#if !defined(_ENDIAN_HP_)
#define _ENDIAN_HP_

/****************************************************/
/*		    header files		    */
/****************************************************/


/****************************************************/
/*  let's see if we know this is a big endian	    */
/****************************************************/
#ifndef INLINE
#define INLINE inline
#endif

#define fnm_assert_stmt(a)

#ifndef BYTE_ORDER
#if defined(AMD29K) || defined(mc68000)

#define BIG_ENDIAN     4321

#endif

/****************************************************/
/*  global macro functions handling endian issues   */
/****************************************************/

#if !defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
#define LITTLE_ENDIAN 1234
#endif
#endif

#define fnm_get_i_big_endian(p_uc, x)	    fnm_get_ui_big_endian(p_uc, x)
#define fnm_get_s_big_endian(p_uc, x)	    fnm_get_us_big_endian(p_uc, x)
#define fnm_get_i_little_endian(p_uc, x)    fnm_get_ui_little_endian(p_uc, x)
#define fnm_get_s_little_endian(p_uc, x)    fnm_get_us_little_endian(p_uc, x)

#define fnm_get_ui_big_endian(p_uc, x)					    \
{									    \
	(x) = (((unsigned int)(*(p_uc)++)) << 24);						\
	(x) += (((unsigned int)(*(p_uc)++)) << 16);						\
	(x) += (((unsigned int)(*(p_uc)++)) << 8);						\
	(x) += ((unsigned int)(*(p_uc)++));							\
}

#define fnm_get_us_big_endian(p_uc, x)						\
{										\
	(x) = (((unsigned short)(*(p_uc)++)) << 8);						\
	(x) += ((unsigned short)(*(p_uc)++));							\
}

#define fnm_get_ui_little_endian(p_uc, x)					\
{										\
	(x) = ((unsigned int)(*(p_uc)++));							\
	(x) += (((unsigned int)(*(p_uc)++)) << 8);						\
	(x) += (((unsigned int)(*(p_uc)++)) << 16);						\
	(x) += (((unsigned int)(*(p_uc)++)) << 24);						\
}

#define fnm_get_us_little_endian(p_uc, x)					\
{										\
	(x) = ((unsigned short)(*(p_uc)++));							\
	(x) += (((unsigned short)(*(p_uc)++)) << 8);						\
}

#define fnm_store_i_big_endian(p_uc, x)		fnm_store_ui_big_endian(p_uc, x)
#define fnm_store_s_big_endian(p_uc, x)		fnm_store_us_big_endian(p_uc, x)
#define fnm_store_i_little_endian(p_uc, x)  fnm_store_ui_little_endian(p_uc, x)
#define fnm_store_s_little_endian(p_uc, x)  fnm_store_us_little_endian(p_uc, x)

#define fnm_store_ui_big_endian(p_uc, x)					\
{										\
	*(p_uc)++ = (((unsigned int)(x)) >> 24);						\
	*(p_uc)++ = (((unsigned int)(x)) >> 16);						\
	*(p_uc)++ = (((unsigned int)(x)) >> 8);						\
	*(p_uc)++ = ((unsigned int)(x));							\
}

#define fnm_store_us_big_endian(p_uc, x)					\
{										\
	*(p_uc)++ = (unsigned char) (((unsigned short)(x)) >> 8);				\
	*(p_uc)++ = (unsigned char) ((unsigned short)(x));					\
}

#define fnm_store_ui_little_endian(p_uc, x)					\
{										\
	*(p_uc)++ = ((unsigned int)(x));							\
	*(p_uc)++ = (((unsigned int)(x)) >> 8);						\
	*(p_uc)++ = (((unsigned int)(x)) >> 16);						\
	*(p_uc)++ = (((unsigned int)(x)) >> 24);						\
}

#define fnm_store_us_little_endian(p_uc, x)					\
{										\
	*(p_uc)++ = ((unsigned short)(x));							\
	*(p_uc)++ = (((unsigned short)(x)) >> 8);						\
}

/* for now lets always use the macroes instead of the inline procedures
   so that we are sure they work */

#if 1 || defined(AMD29K)

#define fnm_convert_us_endian(x)						\
	((unsigned short)((((unsigned short)(x)) << 8) + (((unsigned short)(x)) >> 8)))

#define fnm_convert_ui_endian(x)						\
	((unsigned int)((((unsigned int)(x)) >> 24) + ((((unsigned int)(x)) & 0x00ff0000) >> 8) +		\
		 ((((unsigned int)(x)) & 0x0000ff00) << 8) + (((unsigned int)(x)) << 24)))

#define fnm_make_ui_from_2_us(us_high_part, us_low_part)			\
	((unsigned int)((((unsigned int)(us_high_part)) << 16) + ((unsigned short)(us_low_part))))

#define fnm_make_ui_from_4_uc(p1, p2, p3, p4)					\
	((unsigned int)(((((((unsigned int)((t_uc)p1) << 8) + ((t_uc)p2)) << 8)			\
		+ ((t_uc)p3)) << 8) + ((t_uc)p4)))

#define fnm_make_us_from_2_uc(uc_high_part, uc_low_part)			\
	((unsigned short)((((unsigned short)(uc_high_part)) << 8) + ((t_uc)(uc_low_part))))

#else

INLINE unsigned short fni_convert_us_endian(const unsigned short x)
{
	return((x << 8) + (x >> 8));
}

INLINE unsigned int fni_convert_ui_endian(const unsigned int x)
{
	return((x >> 24) + ((x & 0x00ff0000) >> 8)
	   + ((x & 0x0000ff00) << 8) + (x << 24));
}

INLINE unsigned int fni_make_ui_from_2_us(const unsigned short us_high_part,
				  const unsigned short us_low_part)
{
	return((((unsigned int)us_high_part) << 16) + us_low_part);
}

INLINE unsigned int fni_make_ui_from_4_uc(const unsigned char p1, const unsigned char p2,
				  const unsigned char p3, const unsigned char p4)
{
	return(((((((unsigned int)p1 << 8) + p2) << 8) + p3) << 8) + p4);
}

INLINE unsigned short fni_make_us_from_2_uc(const unsigned char uc_high_part,
				  const unsigned char uc_low_part)
{
	return((((unsigned short)uc_high_part) << 8) + uc_low_part);
}

#define fnm_convert_us_endian(x)	fni_convert_us_endian(x)
#define fnm_convert_ui_endian(x)	fni_convert_ui_endian(x)

#define fnm_make_ui_from_2_us(us_high_part, us_low_part)			\
	fni_make_ui_from_2_us(us_high_part, us_low_part)

#define fnm_make_ui_from_4_uc(p1, p2, p3, p4)					\
	fni_make_ui_from_4_uc(p1, p2, p3, p4)

#define fnm_make_us_from_2_uc(uc_high_part, uc_low_part)			\
	fni_make_us_from_2_uc(uc_high_part, uc_low_part)

#endif

#define fnm_convert_s_endian(x)		((short)(fnm_convert_us_endian(x)))
#define fnm_convert_i_endian(x)		((int)(fnm_convert_ui_endian(x)))

#if defined(BIG_ENDIAN)

#define fnm_convert_us_big_endian(x)		((unsigned short)(x))
#define fnm_convert_s_big_endian(x)		((short)(x))
#define fnm_convert_ui_big_endian(x)		((unsigned int)(x))
#define fnm_convert_i_big_endian(x)		((int)(x))

#define fnm_convert_us_little_endian(x)		fnm_convert_us_endian(x)
#define fnm_convert_s_little_endian(x)		fnm_convert_s_endian(x)
#define fnm_convert_ui_little_endian(x)		fnm_convert_ui_endian(x)
#define fnm_convert_i_little_endian(x)		fnm_convert_i_endian(x)

#else

#define fnm_convert_us_big_endian(x)		fnm_convert_us_endian(x)
#define fnm_convert_s_big_endian(x)		fnm_convert_s_endian(x)
#define fnm_convert_ui_big_endian(x)		fnm_convert_ui_endian(x)
#define fnm_convert_i_big_endian(x)		fnm_convert_i_endian(x)

#define fnm_convert_us_little_endian(x)		((unsigned short)(x))
#define fnm_convert_s_little_endian(x)		((short)(x))
#define fnm_convert_ui_little_endian(x)		((unsigned int)(x))
#define fnm_convert_i_little_endian(x)		((int)(x))

#endif

/****************************************************/
/*  test macro functions handling endian issues		*/
/****************************************************/

#if defined(NDEBUG)

#define fnm_test_definitions()

#else

#define fnm_test_definitions()							\
{										\
	union									\
	{										\
	t_c	a_c[4];								\
	unsigned short	a_us[2];							\
	unsigned int	ul;								\
										\
	} t1 = { "\x01\x02\x03\x04" };						\
										\
	unsigned char	*p;									\
	unsigned short	us_one, us_two;							\
	unsigned int	ul_one;								\
										\
	fnm_assert_stmt((t1.a_c[0] == 1) && (t1.a_c[1] == 2) &&			\
			(t1.a_c[2] == 3) && (t1.a_c[3] == 4));			\
										\
	fnm_assert_stmt(fnm_convert_ui_big_endian(t1.ul) == 0x01020304);		\
	fnm_assert_stmt(fnm_convert_ui_little_endian(t1.ul) == 0x04030201);		\
	fnm_assert_stmt(fnm_convert_us_big_endian(t1.a_us[0]) == 0x0102);		\
	fnm_assert_stmt(fnm_convert_us_little_endian(t1.a_us[0]) == 0x0201);	\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_get_us_little_endian(p, us_one);					\
	fnm_get_us_little_endian(p, us_two);					\
										\
	fnm_assert_stmt((us_one == 0x0201) && (us_two == 0x0403));			\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_get_us_big_endian(p, us_one);						\
	fnm_get_us_big_endian(p, us_two);						\
										\
	fnm_assert_stmt((us_one == 0x0102) && (us_two == 0x0304));			\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_get_ui_little_endian(p, ul_one);					\
										\
	fnm_assert_stmt(ul_one == 0x04030201);					\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_get_ui_big_endian(p, ul_one);						\
										\
	fnm_assert_stmt(ul_one == 0x01020304);					\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_store_us_little_endian(p, 0x1234);					\
	fnm_store_us_little_endian(p, 0x5678);					\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_get_us_little_endian(p, us_one);					\
	fnm_get_us_little_endian(p, us_two);					\
										\
	fnm_assert_stmt((us_one == 0x1234) && (us_two == 0x5678));			\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_store_us_big_endian(p, 0x1234);						\
	fnm_store_us_big_endian(p, 0x5678);						\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_get_us_big_endian(p, us_one);						\
	fnm_get_us_big_endian(p, us_two);						\
										\
	fnm_assert_stmt((us_one == 0x1234) && (us_two == 0x5678));			\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_store_ui_little_endian(p, 0x12345678);					\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_get_ui_little_endian(p, ul_one);					\
										\
	fnm_assert_stmt(ul_one == 0x12345678);					\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_store_ui_big_endian(p, 0x12345678);					\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	p = (unsigned char*)(&t1);								\
										\
	fnm_get_ui_big_endian(p, ul_one);						\
										\
	fnm_assert_stmt(ul_one == 0x12345678);					\
	fnm_assert_stmt((p-4) == ((unsigned char*)(&t1)));					\
										\
	fnm_assert_stmt(fnm_make_ui_from_2_us(1, 2) == 0x00010002);			\
	fnm_assert_stmt(fnm_make_ui_from_4_uc(1, 2, 3, 4) == 0x01020304);		\
	fnm_assert_stmt(fnm_make_us_from_2_uc(1, 2) == 0x0102);			\
										\
}

#endif

#endif

/****************************************************/
/****************************************************/
/*		End header file "endian.h"			*/
/****************************************************/
/****************************************************/
