/*******************************************************************
** m a t h 6 4 . c
** Forth Inspired Command Language - 64 bit math support routines
** Author: John Sadler (john_sadler@alum.mit.edu)
** Created: 25 January 1998
**
*******************************************************************/

#include "ficl.h"
#include "math64.h"


/**************************************************************************
                        m 6 4 A b s
** Returns the absolute value of an INT64
**************************************************************************/
INT64 m64Abs(INT64 x)
{
    if (m64IsNegative(x))
        x = m64Negate(x);

    return x;
}


/**************************************************************************
                        m 6 4 F l o o r e d D i v I
** 
** FROM THE FORTH ANS...
** Floored division is integer division in which the remainder carries
** the sign of the divisor or is zero, and the quotient is rounded to
** its arithmetic floor. Symmetric division is integer division in which
** the remainder carries the sign of the dividend or is zero and the
** quotient is the mathematical quotient rounded towards zero or
** truncated. Examples of each are shown in tables 3.3 and 3.4. 
** 
** Table 3.3 - Floored Division Example
** Dividend        Divisor Remainder       Quotient
** --------        ------- ---------       --------
**  10                7       3                1
** -10                7       4               -2
**  10               -7      -4               -2
** -10               -7      -3                1
** 
** 
** Table 3.4 - Symmetric Division Example
** Dividend        Divisor Remainder       Quotient
** --------        ------- ---------       --------
**  10                7       3                1
** -10                7      -3               -1
**  10               -7       3               -1
** -10               -7      -3                1
**************************************************************************/
INTQR m64FlooredDivI(INT64 num, INT32 den)
{
    INTQR qr;
    UNSQR uqr;
    int signRem = 1;
    int signQuot = 1;

    if (m64IsNegative(num))
    {
        num = m64Negate(num);
        signQuot = -signQuot;
    }

    if (den < 0)
    {
        den      = -den;
        signRem  = -signRem;
        signQuot = -signQuot;
    }

    uqr = ficlLongDiv(m64CastIU(num), (UNS32)den);
    qr = m64CastQRUI(uqr);
    if (signQuot < 0)
    {
        qr.quot = -qr.quot;
        if (qr.rem != 0)
        {
            qr.quot--;
            qr.rem = den - qr.rem;
        }
    }

    if (signRem < 0)
        qr.rem = -qr.rem;

    return qr;
}


/**************************************************************************
                        m 6 4 I s N e g a t i v e
** Returns TRUE if the specified INT64 has its sign bit set.
**************************************************************************/
int m64IsNegative(INT64 x)
{
    return (x.hi < 0);
}


/**************************************************************************
                        m 6 4 M a c
** Mixed precision multiply and accumulate primitive for number building.
** Multiplies UNS64 u by UNS32 mul and adds UNS32 add. Mul is typically
** the numeric base, and add represents a digit to be appended to the 
** growing number. 
** Returns the result of the operation
**************************************************************************/
UNS64 m64Mac(UNS64 u, UNS32 mul, UNS32 add)
{
    UNS64 resultLo = ficlLongMul(u.lo, mul);
    UNS64 resultHi = ficlLongMul(u.hi, mul);
    resultLo.hi += resultHi.lo;
    resultHi.lo = resultLo.lo + add;

    if (resultHi.lo < resultLo.lo)
        resultLo.hi++;

    resultLo.lo = resultHi.lo;

    return resultLo;
}


/**************************************************************************
                        m 6 4 M u l I
** Multiplies a pair of INT32s and returns an INT64 result.
**************************************************************************/
INT64 m64MulI(INT32 x, INT32 y)
{
    UNS64 prod;
    int sign = 1;

    if (x < 0)
    {
        sign = -sign;
        x = -x;
    }

    if (y < 0)
    {
        sign = -sign;
        y = -y;
    }

    prod = ficlLongMul(x, y);
    if (sign > 0)
        return m64CastUI(prod);
    else
        return m64Negate(m64CastUI(prod));
}


/**************************************************************************
                        m 6 4 N e g a t e
** Negates an INT64 by complementing and incrementing.
**************************************************************************/
INT64 m64Negate(INT64 x)
{
    x.hi = ~x.hi;
    x.lo = ~x.lo;
    x.lo ++;
    if (x.lo == 0)
        x.hi++;

    return x;
}


/**************************************************************************
                        m 6 4 P u s h
** Push an INT64 onto the specified stack in the order required
** by ANS Forth (most significant cell on top)
** These should probably be macros...
**************************************************************************/
void  i64Push(FICL_STACK *pStack, INT64 i64)
{
    stackPushINT32(pStack, i64.lo);
    stackPushINT32(pStack, i64.hi);
    return;
}

void  u64Push(FICL_STACK *pStack, UNS64 u64)
{
    stackPushINT32(pStack, u64.lo);
    stackPushINT32(pStack, u64.hi);
    return;
}


/**************************************************************************
                        m 6 4 P o p
** Pops an INT64 off the stack in the order required by ANS Forth
** (most significant cell on top)
** These should probably be macros...
**************************************************************************/
INT64 i64Pop(FICL_STACK *pStack)
{
    INT64 ret;
    ret.hi = stackPopINT32(pStack);
    ret.lo = stackPopINT32(pStack);
    return ret;
}

UNS64 u64Pop(FICL_STACK *pStack)
{
    UNS64 ret;
    ret.hi = stackPopINT32(pStack);
    ret.lo = stackPopINT32(pStack);
    return ret;
}


/**************************************************************************
                        m 6 4 S y m m e t r i c D i v
** Divide an INT64 by an INT32 and return an INT32 quotient and an INT32
** remainder. The absolute values of quotient and remainder are not
** affected by the signs of the numerator and denominator (the operation
** is symmetric on the number line)
**************************************************************************/
INTQR m64SymmetricDivI(INT64 num, INT32 den)
{
    INTQR qr;
    UNSQR uqr;
    int signRem = 1;
    int signQuot = 1;

    if (m64IsNegative(num))
    {
        num = m64Negate(num);
        signRem  = -signRem;
        signQuot = -signQuot;
    }

    if (den < 0)
    {
        den      = -den;
        signQuot = -signQuot;
    }

    uqr = ficlLongDiv(m64CastIU(num), (UNS32)den);
    qr = m64CastQRUI(uqr);
    if (signRem < 0)
        qr.rem = -qr.rem;

    if (signQuot < 0)
        qr.quot = -qr.quot;

    return qr;
}


/**************************************************************************
                        m 6 4 U M o d
** Divides an UNS64 by base (an UNS16) and returns an UNS16 remainder.
** Writes the quotient back to the original UNS64 as a side effect.
** This operation is typically used to convert an UNS64 to a text string
** in any base. See words.c:numberSignS, for example.
** Mechanics: performs 4 ficlLongDivs, each of which produces 16 bits
** of the quotient. C does not provide a way to divide an UNS32 by an
** UNS16 and get an UNS32 quotient (ldiv is closest, but it's signed,
** unfortunately), so I've used ficlLongDiv.
**************************************************************************/
UNS16 m64UMod(UNS64 *pUD, UNS16 base)
{
    UNS64 ud;
    UNSQR qr;
    UNS64 result;

    result.hi = result.lo = 0;

    ud.hi = 0;
    ud.lo = pUD->hi >> 16;
    qr = ficlLongDiv(ud, (UNS32)base);
    result.hi = qr.quot << 16;

    ud.lo = (qr.rem << 16) | (pUD->hi & 0x0000ffff);
    qr = ficlLongDiv(ud, (UNS32)base);
    result.hi |= qr.quot & 0x0000ffff;

    ud.lo = (qr.rem << 16) | (pUD->lo >> 16);
    qr = ficlLongDiv(ud, (UNS32)base);
    result.lo = qr.quot << 16;

    ud.lo = (qr.rem << 16) | (pUD->lo & 0x0000ffff);
    qr = ficlLongDiv(ud, (UNS32)base);
    result.lo |= qr.quot & 0x0000ffff;

    *pUD = result;

    return (UNS16)(qr.rem);
}


