/* atof_tahoe.c - turn a string into a Tahoe floating point number
   Copyright (C) 1987 Free Software Foundation, Inc.
   */

/* This is really a simplified version of atof_vax.c. I glommed it wholesale
   and then shaved it down. I don't even know how it works. (Don't you find
   my honesty refreshing?  bowen@cs.Buffalo.EDU (Devon E Bowen)

   I don't allow uppercase letters in the precision descrpitors. Ie 'f' and
   'd' are allowed but 'F' and 'D' aren't */

#include "as.h"

/* Precision in LittleNums. */
#define MAX_PRECISION (4)
#define D_PRECISION (4)
#define F_PRECISION (2)

/* Precision in chars. */
#define D_PRECISION_CHARS (8)
#define F_PRECISION_CHARS (4)

				/* Length in LittleNums of guard bits. */
#define GUARD (2)

static const long int mask [] = {
  0x00000000,
  0x00000001,
  0x00000003,
  0x00000007,
  0x0000000f,
  0x0000001f,
  0x0000003f,
  0x0000007f,
  0x000000ff,
  0x000001ff,
  0x000003ff,
  0x000007ff,
  0x00000fff,
  0x00001fff,
  0x00003fff,
  0x00007fff,
  0x0000ffff,
  0x0001ffff,
  0x0003ffff,
  0x0007ffff,
  0x000fffff,
  0x001fffff,
  0x003fffff,
  0x007fffff,
  0x00ffffff,
  0x01ffffff,
  0x03ffffff,
  0x07ffffff,
  0x0fffffff,
  0x1fffffff,
  0x3fffffff,
  0x7fffffff,
  0xffffffff
  };


/* Shared between flonum_gen2tahoe and next_bits */
static int		bits_left_in_littlenum;
static LITTLENUM_TYPE *	littlenum_pointer;
static LITTLENUM_TYPE * littlenum_end;

#if __STDC__ == 1

int flonum_gen2tahoe(int format_letter, FLONUM_TYPE *f, LITTLENUM_TYPE *words);

#else /* not __STDC__ */

int flonum_gen2tahoe();

#endif /* not __STDC__ */


static int
next_bits (number_of_bits)
     int		number_of_bits;
{
  int			return_value;

  if(littlenum_pointer<littlenum_end)
  	return 0;
  if (number_of_bits >= bits_left_in_littlenum)
    {
      return_value  = mask [bits_left_in_littlenum] & * littlenum_pointer;
      number_of_bits -= bits_left_in_littlenum;
      return_value <<= number_of_bits;
      bits_left_in_littlenum = LITTLENUM_NUMBER_OF_BITS - number_of_bits;
      littlenum_pointer --;
      if(littlenum_pointer>=littlenum_end)
	return_value |= ((*littlenum_pointer) >> (bits_left_in_littlenum)) &
	  mask [number_of_bits];
    }
  else
    {
      bits_left_in_littlenum -= number_of_bits;
      return_value = mask [number_of_bits] &
	((*littlenum_pointer) >> bits_left_in_littlenum);
    }
  return (return_value);
}

static void
make_invalid_floating_point_number (words)
     LITTLENUM_TYPE *	words;
{
  *words = 0x8000;		/* Floating Reserved Operand Code */
}

static int			/* 0 means letter is OK. */
what_kind_of_float (letter, precisionP, exponent_bitsP)
     char		letter;	/* In: lowercase please. What kind of float? */
     int *		precisionP; /* Number of 16-bit words in the float. */
     long int *		exponent_bitsP;	/* Number of exponent bits. */
{
  int	retval;			/* 0: OK. */

  retval = 0;
  switch (letter)
    {
    case 'f':
      * precisionP = F_PRECISION;
      * exponent_bitsP = 8;
      break;

    case 'd':
      * precisionP = D_PRECISION;
      * exponent_bitsP = 8;
      break;

    default:
      retval = 69;
      break;
    }
  return (retval);
}

/***********************************************************************\
*									*
*	Warning: this returns 16-bit LITTLENUMs, because that is	*
*	what the VAX thinks in. It is up to the caller to figure	*
*	out any alignment problems and to conspire for the bytes/word	*
*	to be emitted in the right order. Bigendians beware!		*
*									*
\***********************************************************************/

char *				/* Return pointer past text consumed. */
atof_tahoe (str, what_kind, words)
     char *		str;	/* Text to convert to binary. */
     char		what_kind; /* 'd', 'f', 'g', 'h' */
     LITTLENUM_TYPE *	words;	/* Build the binary here. */
{
  FLONUM_TYPE		f;
  LITTLENUM_TYPE	bits [MAX_PRECISION + MAX_PRECISION + GUARD];
				/* Extra bits for zeroed low-order bits. */
				/* The 1st MAX_PRECISION are zeroed, */
				/* the last contain flonum bits. */
  char *		return_value;
  int			precision; /* Number of 16-bit words in the format. */
  long int		exponent_bits;

  return_value = str;
  f . low	= bits + MAX_PRECISION;
  f . high	= NULL;
  f . leader	= NULL;
  f . exponent	= NULL;
  f . sign	= '\0';

  if (what_kind_of_float (what_kind, & precision, & exponent_bits))
    {
      return_value = NULL;	/* We lost. */
      make_invalid_floating_point_number (words);
    }
  if (return_value)
    {
      memset(bits, '\0', sizeof(LITTLENUM_TYPE) * MAX_PRECISION);

				/* Use more LittleNums than seems */
				/* necessary: the highest flonum may have */
				/* 15 leading 0 bits, so could be useless. */
      f . high = f . low + precision - 1 + GUARD;

      if (atof_generic (& return_value, ".", "eE", & f))
	{
	  make_invalid_floating_point_number (words);
	  return_value = NULL;	/* we lost */
	}
      else
	{
	  if (flonum_gen2tahoe (what_kind, & f, words))
	    {
	      return_value = NULL;
	    }
	}
    }
  return (return_value);
}

/*
 * In: a flonum, a Tahoe floating point format.
 * Out: a Tahoe floating-point bit pattern.
 */

int				/* 0: OK. */
flonum_gen2tahoe (format_letter, f, words)
     char		format_letter; /* One of 'd' 'f'. */
     FLONUM_TYPE *	f;
     LITTLENUM_TYPE *	words;	/* Deliver answer here. */
{
  LITTLENUM_TYPE *	lp;
  int			precision;
  long int		exponent_bits;
  int			return_value; /* 0 == OK. */

  return_value = what_kind_of_float(format_letter,&precision,&exponent_bits);
  if (return_value != 0)
    {
      make_invalid_floating_point_number (words);
    }
  else
    {
      if (f -> low > f -> leader)
	{
	  /* 0.0e0 seen. */
	  memset(words, '\0', sizeof(LITTLENUM_TYPE) * precision);
	}
      else
	{
	  long int		exponent_1;
	  long int		exponent_2;
	  long int		exponent_3;
	  long int		exponent_4;
	  int		exponent_skippage;
	  LITTLENUM_TYPE	word1;

		/* JF: Deal with new Nan, +Inf and -Inf codes */
	  if(f->sign!='-' && f->sign!='+') {
	    make_invalid_floating_point_number(words);
	    return return_value;
	  }
	  /*
	   * All tahoe floating_point formats have:
	   * Bit 15 is sign bit.
	   * Bits 14:n are excess-whatever exponent.
	   * Bits n-1:0 (if any) are most significant bits of fraction.
	   * Bits 15:0 of the next word are the next most significant bits.
	   * And so on for each other word.
	   *
	   * So we need: number of bits of exponent, number of bits of
	   * mantissa.
	   */
	  
	  bits_left_in_littlenum = LITTLENUM_NUMBER_OF_BITS;
	  littlenum_pointer = f -> leader;
	  littlenum_end = f->low;
	  /* Seek (and forget) 1st significant bit */
	  for (exponent_skippage = 0;
	       ! next_bits(1);
	       exponent_skippage ++)
	    {
	    }
	  exponent_1 = f -> exponent + f -> leader + 1 - f -> low;
	  /* Radix LITTLENUM_RADIX, point just higher than f -> leader. */
	  exponent_2 = exponent_1 * LITTLENUM_NUMBER_OF_BITS;
	  /* Radix 2. */
	  exponent_3 = exponent_2 - exponent_skippage;
	  /* Forget leading zeros, forget 1st bit. */
	  exponent_4 = exponent_3 + (1 << (exponent_bits - 1));
	  /* Offset exponent. */
	  
	  if (exponent_4 & ~ mask [exponent_bits])
	    {
	      /*
	       * Exponent overflow. Lose immediately.
	       */
	      
	      make_invalid_floating_point_number (words);
	      
	      /*
	       * We leave return_value alone: admit we read the
	       * number, but return a floating exception
	       * because we can't encode the number.
	       */
	    }
	  else
	    {
	      lp = words;
	      
	      /* Word 1. Sign, exponent and perhaps high bits. */
	      /* Assume 2's complement integers. */
	      word1 = ((exponent_4 & mask [exponent_bits]) << (15 - exponent_bits))
		|       ((f -> sign == '+') ? 0 : 0x8000)
		  |	next_bits (15 - exponent_bits);
	      * lp ++ = word1;
	      
	      /* The rest of the words are just mantissa bits. */
	      for (; lp < words + precision; lp++)
		{
		  * lp = next_bits (LITTLENUM_NUMBER_OF_BITS);
		}
	      
	      if (next_bits (1))
		{
		  /*
		   * Since the NEXT bit is a 1, round UP the mantissa.
		   * The cunning design of these hidden-1 floats permits
		   * us to let the mantissa overflow into the exponent, and
		   * it 'does the right thing'. However, we lose if the
		   * highest-order bit of the lowest-order word flips.
		   * Is that clear?
		   */
		  
		  unsigned long int	carry;
		  
		  /*
		    #if (sizeof(carry)) < ((sizeof(bits[0]) * BITS_PER_CHAR) + 2)
		    Please allow at least 1 more bit in carry than is in a LITTLENUM.
		    We need that extra bit to hold a carry during a LITTLENUM carry
		    propagation. Another extra bit (kept 0) will assure us that we
		    don't get a sticky sign bit after shifting right, and that
		    permits us to propagate the carry without any masking of bits.
		    #endif
		    */
		  for (carry = 1, lp --;
		       carry && (lp >= words);
		       lp --)
		    {
		      carry = * lp + carry;
		      * lp = carry;
		      carry >>= LITTLENUM_NUMBER_OF_BITS;
		    }
		  
		  if ( (word1 ^ *words) & (1 << (LITTLENUM_NUMBER_OF_BITS - 1)) )
		    {
		      make_invalid_floating_point_number (words);
		      /*
		       * We leave return_value alone: admit we read the
		       * number, but return a floating exception
		       * because we can't encode the number.
		       */
		    }
		}		/* if (we needed to round up) */
	    }			/* if (exponent overflow) */
	}			/* if (0.0e0) */
    }				/* if (float_type was OK) */
  return (return_value);
}

/*
 *		md_atof()
 *
 * In:	input_line_pointer -> the 1st character of a floating-point
 *		number.
 *	1 letter denoting the type of statement that wants a
 *		binary floating point number returned.
 *	Address of where to build floating point literal.
 *		Assumed to be 'big enough'.
 *	Address of where to return size of literal (in chars).
 *
 * Out:	Input_line_pointer -> of next char after floating number.
 *	Error message, or "".
 *	Floating point literal.
 *	Number of chars we used for the literal.
 */

char *
md_atof (what_statement_type, literalP, sizeP)
     char	what_statement_type;
     char *	literalP;
     int *	sizeP;
{
  LITTLENUM_TYPE	words [MAX_PRECISION];
  register char		kind_of_float;
  register int		number_of_chars;
  register LITTLENUM_TYPE * littlenum_pointer;

  switch (what_statement_type)
    {
    case 'f':			/* .ffloat */
    case 'd':			/* .dfloat */
      kind_of_float = what_statement_type;
      break;

    default:
      kind_of_float = 0;
      break;
    };

  if (kind_of_float)
    {
      register LITTLENUM_TYPE * limit;

      input_line_pointer = atof_tahoe (input_line_pointer,
				       kind_of_float,
				       words);
      /*
       * The atof_tahoe() builds up 16-bit numbers.
       * Since the assembler may not be running on
       * a different-endian machine, be very careful about
       * converting words to chars.
       */
      number_of_chars = (kind_of_float == 'f' ? F_PRECISION_CHARS :
			 (kind_of_float == 'd' ? D_PRECISION_CHARS : 0));
      know(number_of_chars<=MAX_PRECISION*sizeof(LITTLENUM_TYPE));
      limit = words + (number_of_chars / sizeof(LITTLENUM_TYPE));
      for (littlenum_pointer = words;
	   littlenum_pointer < limit;
	   littlenum_pointer ++)
	{
	  md_number_to_chars(literalP,*littlenum_pointer,
			     sizeof(LITTLENUM_TYPE));
	  literalP += sizeof(LITTLENUM_TYPE);
	};
    }
  else
    {
      number_of_chars = 0;
    };

  * sizeP = number_of_chars;
  return (kind_of_float ? "" : "Bad call to md_atof()");
}				/* md_atof() */

/* atof_tahoe.c */
