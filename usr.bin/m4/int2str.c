/*  File   : int2str.c
    Author : Richard A. O'Keefe
    Updated: 6 February 1993
    Defines: int2str()

    int2str(dst, radix, val)
    converts the (long) integer "val" to character form and moves it to
    the destination string "dst" followed by a terminating NUL.  The
    result is normally a pointer to this NUL character, but if the radix
    is dud the result will be NullS and nothing will be changed.

    If radix is -2..-36, val is taken to be SIGNED.
    If radix is  2.. 36, val is taken to be UNSIGNED.
    That is, val is signed if and only if radix is.  You will normally
    use radix -10 only through itoa and ltoa, for radix 2, 8, or 16
    unsigned is what you generally want.
*/

static char dig_vec[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";


char *int2str(dst, radix, val)
    register char *dst;
    register int radix;
    register long val;
    {
	char buffer[65];	/* Ready for 64-bit machines */
	register char *p;

	if (radix < 2 || radix > 36) {	/* Not 2..36 */
	    if (radix > -2 || radix < -36) return (char *)0;
	    if (val < 0) {
		*dst++ = '-';
		val = -val;
	    }
	    radix = -radix;
	}
	/*  The slightly contorted code which follows is due to the
	    fact that few machines directly support unsigned long / and %.
	    Certainly the VAX C compiler generates a subroutine call.  In
	    the interests of efficiency (hollow laugh) I let this happen
	    for the first digit only; after that "val" will be in range so
	    that signed integer division will do.  Sorry 'bout that.
	    CHECK THE CODE PRODUCED BY YOUR C COMPILER.  The first % and /
	    should be unsigned, the second % and / signed, but C compilers
	    tend to be extraordinarily sensitive to minor details of style.
	    This works on a VAX, that's all I claim for it.
	*/
	p = &buffer[sizeof buffer];
	*--p = '\0';
	*--p = dig_vec[(unsigned long)val%(unsigned long)radix];
	val = (unsigned long)val/(unsigned long)radix;
	while (val != 0) *--p = dig_vec[val%radix], val /= radix;
	while (*dst++ = *p++) ;
	return dst-1;
    }

