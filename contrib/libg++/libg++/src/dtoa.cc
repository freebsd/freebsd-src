/* 
Copyright (C) 1990 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <builtin.h>
#include <math.h>
#include <values.h>
#include <AllocRing.h>

extern AllocRing _libgxx_fmtq;

#ifdef __GNUC__ /* cfront cannot compile this routine */
// OBSOLETE ROUTINE!

char* dtoa(double fpnum,  char cvt, int width, int prec)
{
  // set up workspace

  // max possible digits <= those need to show all of prec + exp
  // <= ceil(log10(HUGE)) plus space for null, etc.

  const int worksiz = int((M_LN2 / M_LN10) * DMAXEXP) + 8; 

  // for fractional part
  char  fwork[worksiz];
  char* fw = fwork;

  // for integer part
  char  iwork[worksiz];
  char* iworkend = &iwork[sizeof(iwork) - 1];
  char* iw = iworkend;
  *iw = 0;

  // for exponent part

  const int eworksiz = int(M_LN2 * _DEXPLEN) + 8;
  char  ework[eworksiz];
  char* eworkend = &ework[sizeof(ework) - 1];
  char* ew = eworkend;
  *ew = 0;

#if (_IEEE != 0)
  if (isinf(fpnum))
  {
    char* inffmt = (char *) _libgxx_fmtq.alloc(5);
    char* inffmtp = inffmt;
    if (fpnum < 0)
      *inffmtp++ = '-';
    strcpy(inffmtp, "Inf");
    return inffmt;
  }

  if (isnan(fpnum))
  {
    char* nanfmt = (char *) _libgxx_fmtq.alloc(4);
    strcpy(nanfmt, "NaN");
    return nanfmt;
  }
#endif

  // grab sign & make non-negative
  int is_neg = fpnum < 0;
  if (is_neg) fpnum = -fpnum;

  // precision matters

  if (prec > worksiz - 2) // can't have more prec than supported
    prec = worksiz - 2;
  
  double powprec;
  if (prec == 6)
    powprec = 1.0e6;
  else
    powprec = pow(10.0, (long) prec);

  double rounder = 0.5 / powprec;

  int f_fmt = cvt == 'f' ||
    ((cvt == 'g') && (fpnum == 0.0 || (fpnum >= 1e-4 && fpnum < powprec)));

  int iwidth = 0;
  int fwidth = 0;
  int ewidth = 0;

  if (f_fmt)  // fixed format
  {
    double ipart;
    double fpart = modf(fpnum, &ipart);

    // convert fractional part

    if (fpart >= rounder || cvt != 'g')
    {
      fpart += rounder;
      if (fpart >= 1.0)
      {
        ipart += 1.0;
        fpart -= 1.0;
      }
      double ffpart = fpart;
      double ifpart;
      for (int i = 0; i < prec; ++i)
      {
        ffpart = modf(ffpart * 10.0, &ifpart);
        *fw++ = '0' + int(ifpart);
        ++fwidth;
      }
      if (cvt == 'g')  // inhibit trailing zeroes if g-fmt
      {
        for (char* p = fw - 1; p >= fwork && *p == '0'; --p)
        {
          *p = 0;
          --fwidth;
        }
      }
    }

    // convert integer part
    if (ipart == 0.0)
    {
      if (cvt != 'g' || fwidth < prec || fwidth < width)
      {
        *--iw = '0'; ++iwidth;
      }
    }
    else if (ipart <= double(MAXLONG)) // a useful speedup
    {
      long li = long(ipart);
      while (li != 0)
      {
        *--iw = '0' + (li % 10);
        li = li / 10;
        ++iwidth;
      }
    }
    else // the slow way
    {
      while (ipart > 0.5)
      {
        double ff = modf(ipart / 10.0, &ipart);
        ff = (ff + 0.05) * 10.0;
        *--iw = '0' + int(ff);
        ++iwidth;
      }
    }

    // g-fmt: kill part of frac if prec/width exceeded
    if (cvt == 'g')
    {
      int m = prec;
      if (m < width)
        m = width;
      int adj = iwidth + fwidth - m;
      if (adj > fwidth)
        adj = fwidth;
      if (adj > 0)
      {
        for (char* f = &fwork[fwidth-1]; f >= fwork && adj > 0; --adj, --f)
        {
          --fwidth;
          char ch = *f;
          *f = 0;
          if (ch > '5') // properly round: unavoidable propagation
          {
            int carry = 1;
            for (char* p = f - 1; p >= fwork && carry; --p)
            {
              ++*p;
              if (*p > '9')
                *p = '0';
              else
                carry = 0;
            }
            if (carry)
            {
              for (p = iworkend - 1; p >= iw && carry; --p)
              {
                ++*p;
                if (*p > '9')
                  *p = '0';
                else
                  carry = 0;
              }
              if (carry)
              {
                *--iw = '1';
                ++iwidth;
                --adj;
              }
            }
          }
        }
      }
    }
              
  }
  else  // e-fmt
  {
    
    // normalize
    int exp = 0;
    while (fpnum >= 10.0)
    {
      fpnum *= 0.1;
      ++exp;
    }
    double almost_one = 1.0 - rounder;
    while (fpnum > 0.0 && fpnum < almost_one)
    {
      fpnum *= 10.0;
      --exp;
    }
    
    double ipart;
    double fpart = modf(fpnum, &ipart);


    if (cvt == 'g')     // used up one digit for int part...
    {
      --prec;
      powprec /= 10.0;
      rounder = 0.5 / powprec;
    }

    // convert fractional part -- almost same as above
    if (fpart >= rounder || cvt != 'g')
    {
      fpart += rounder;
      if (fpart >= 1.0)
      {
        fpart -= 1.0;
        ipart += 1.0;
        if (ipart >= 10.0)
        {
          ++exp;
          ipart /= 10.0;
          fpart /= 10.0;
        }
      }
      double ffpart = fpart;
      double ifpart;
      for (int i = 0; i < prec; ++i)
      {
        ffpart = modf(ffpart * 10.0, &ifpart);
        *fw++ = '0' + int(ifpart);
        ++fwidth;
      }
      if (cvt == 'g')  // inhibit trailing zeroes if g-fmt
      {
        for (char* p = fw - 1; p >= fwork && *p == '0'; --p)
        {
          *p = 0;
          --fwidth;
        }
      }
    }

    
    // convert exponent

    char eneg = exp < 0;
    if (eneg) exp = - exp;

    while (exp > 0)
    {
      *--ew = '0' + (exp % 10);
      exp /= 10;
      ++ewidth;
    }

    while (ewidth < 2)  // ensure at least 2 zeroes
    {
      *--ew = '0';
      ++ewidth;
    }

    *--ew = eneg ? '-' : '+';
    *--ew = 'e';

    ewidth += 2;

    // convert the one-digit integer part
    *--iw = '0' + int(ipart);
    ++iwidth;
    
  }

  // arrange everything in returned string

  int showdot = cvt != 'g' || fwidth > 0;

  int fmtwidth = is_neg + iwidth + showdot + fwidth + ewidth;
  
  int pad = width - fmtwidth;
  if (pad < 0) pad = 0;
  
  char* fmtbase = (char *) _libgxx_fmtq.alloc(fmtwidth + pad + 1);
  char* fmt = fmtbase;
  
  for (int i = 0; i < pad; ++i) *fmt++ = ' ';
  
  if (is_neg) *fmt++ = '-';
  
  for (i = 0; i < iwidth; ++i) *fmt++ = *iw++;
  
  if (showdot)
  {
    *fmt++ = '.';
    fw = fwork;
    for (i = 0; i < fwidth; ++i) *fmt++ = *fw++;
  }
  
  for (i = 0; i < ewidth; ++i) *fmt++ = *ew++;
  
  *fmt = 0;
  
  return fmtbase;
}
#endif
