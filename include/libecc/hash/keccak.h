/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __KECCAK_H__
#define __KECCAK_H__

#include <libecc/words/words.h>

#define _KECCAK_ROTL_(x, y) (((x) << (y)) | ((x) >> ((sizeof(u64) * 8) - (y))))

/* We handle the case where one of the shifts is more than 64-bit: in this
 * case, behaviour is undefined as per ANSI C definition. In this case, we
 * return the untouched input.
 */
#define KECCAK_ROTL(x, y) ((((y) < (sizeof(u64) * 8)) && ((y) > 0)) ? (_KECCAK_ROTL_(x, y)) : (x))

/*
 * Round transformation of the state. Notations are the
 * same as the ones used in:
 * http://keccak.noekeon.org/specs_summary.html
 */
#define KECCAK_WORD_LOG 6
#define KECCAK_ROUNDS   (12 + (2 * KECCAK_WORD_LOG))
#define KECCAK_SLICES   5

static const u64 keccak_rc[KECCAK_ROUNDS] =
{
	0x0000000000000001ULL, 0x0000000000008082ULL,
	0x800000000000808AULL, 0x8000000080008000ULL,
	0x000000000000808BULL, 0x0000000080000001ULL,
	0x8000000080008081ULL, 0x8000000000008009ULL,
	0x000000000000008AULL, 0x0000000000000088ULL,
	0x0000000080008009ULL, 0x000000008000000AULL,
	0x000000008000808BULL, 0x800000000000008BULL,
	0x8000000000008089ULL, 0x8000000000008003ULL,
	0x8000000000008002ULL, 0x8000000000000080ULL,
	0x000000000000800AULL, 0x800000008000000AULL,
	0x8000000080008081ULL, 0x8000000000008080ULL,
	0x0000000080000001ULL, 0x8000000080008008ULL
};

static const u8 keccak_rot[KECCAK_SLICES][KECCAK_SLICES] =
{
	{  0, 36,  3, 41, 18 },
	{  1, 44, 10, 45,  2 },
	{ 62,  6, 43, 15, 61 },
	{ 28, 55, 25, 21, 56 },
	{ 27, 20, 39,  8, 14 },
};


/* Macro to handle endianness conversion */
#define SWAP64_Idx(a)   ((sizeof(u64) * ((u8)(a) / sizeof(u64))) + (sizeof(u64) - 1 - ((u8)(a) % sizeof(u64))))

#define Idx_slices(x, y)	((x) + (KECCAK_SLICES * (y)))
#define Idx(A, x, y)		((A)[Idx_slices(x, y)])

#define KECCAKROUND(A, RC) do {								\
	int x, y;									\
	u64 tmp;									\
	/* Temporary B, C and D arrays */						\
	u64 BCD[KECCAK_SLICES * KECCAK_SLICES];						\
	/* Theta step */								\
	for(x = 0; x < KECCAK_SLICES; x++){						\
		Idx(BCD, x, 0) = Idx(A, x, 0) ^ Idx(A, x, 1) ^ Idx(A, x, 2) ^		\
				 Idx(A, x, 3) ^ Idx(A, x, 4);				\
	}										\
	for(x = 0; x < KECCAK_SLICES; x++){						\
		tmp = Idx(BCD, (x + 4) % 5, 0) ^					\
		      KECCAK_ROTL(Idx(BCD, (x + 1) % 5, 0), 1);				\
		for(y = 0; y < KECCAK_SLICES; y++){					\
			Idx(A, x, y) ^= tmp;						\
		}									\
	}										\
	/* Rho and Pi steps */								\
	for(x = 0; x < KECCAK_SLICES; x++){						\
		for(y = 0; y < KECCAK_SLICES; y++){					\
			Idx(BCD, y, ((2*x)+(3*y)) % 5) =				\
			KECCAK_ROTL(Idx(A, x, y), keccak_rot[x][y]);			\
		}									\
	}										\
	/* Chi step */									\
	for(x = 0; x < KECCAK_SLICES; x++){						\
		for(y = 0; y < KECCAK_SLICES; y++){					\
			Idx(A, x, y) = Idx(BCD, x, y) ^					\
				(~Idx(BCD, (x+1) % 5, y) & Idx(BCD, (x+2)%5, y));	\
		}									\
	}										\
	/* Iota step */									\
	Idx(A, 0, 0) ^= (RC);								\
} while(0)

#define KECCAKF(A) do {									\
	int round;									\
	for(round = 0; round < KECCAK_ROUNDS; round++){					\
		KECCAKROUND(A, keccak_rc[round]);					\
	}										\
} while(0)

#endif /* __KECCAK_H__ */
