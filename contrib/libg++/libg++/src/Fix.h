// -*- C++ -*-
// Fix.h : variable length fixed point data type 
//

#ifndef _Fix_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Fix_h 1

#include <stream.h>
#include <std.h>
#include <stddef.h>
#include <Integer.h>
#include <builtin.h>

class Fix
{
  struct Rep                    // internal Fix representation
  {
    _G_uint16_t len;		// length in bits
    _G_uint16_t siz;		// allocated storage
    _G_int16_t  ref;		// reference count
    _G_uint16_t s[1];		// start of ushort array represention
  };

public:

  typedef void (*PEH)(Rep*);

private:

  Rep*		  rep;

		  Fix(Rep*);
                  Fix(int, const Rep*);

  void		  unique();

  static const _G_uint16_t min_length =     1;
  static const _G_uint16_t max_length = 65535;
  static const double min_value  =  -1.0;
  static const double max_value  =   1.0;

  static _G_uint16_t   default_length;
  static int 	  default_print_width;
  static Rep	  Rep_0;
  static Rep 	  Rep_m1;
  static Rep 	  Rep_quotient_bump;

  // internal class functions
  static void	  mask(Rep*);
  static int      compare(const Rep*, const Rep* = &Rep_0);

  static Rep*	  new_Fix(_G_uint16_t);
  static Rep*	  new_Fix(_G_uint16_t, const Rep*);
  static Rep*	  new_Fix(_G_uint16_t, double);

  static Rep*	  copy(const Rep*, Rep*);
  static Rep*	  negate(const Rep*, Rep* = NULL);
  static Rep*	  add(const Rep*, const Rep*, Rep* = NULL);
  static Rep*	  subtract(const Rep*, const Rep*, Rep* = NULL);
  static Rep*	  multiply(const Rep*, const Rep*, Rep* = NULL);
  static Rep*	  multiply(const Rep*, int, Rep* = NULL);
  static Rep*	  divide(const Rep*, const Rep*, Rep* = NULL,
			 Rep* = NULL);
  static Rep*	  shift(const Rep*, int, Rep* = NULL);

  static one_arg_error_handler_t error_handler;
  static one_arg_error_handler_t range_error_handler;

  static PEH overflow_handler;

public:
		  Fix();
                  Fix(const Fix&);
		  Fix(double);
                  Fix(int);
                  Fix(_G_uint16_t);
                  Fix(int, const Fix&);
                  Fix(int, double);

                  ~Fix();

  Fix             operator =  (const Fix&);
  Fix             operator =  (double);

  friend int      operator == (const Fix&, const Fix&);
  friend int      operator != (const Fix&, const Fix&);

  friend int      operator <  (const Fix&, const Fix&);
  friend int      operator <= (const Fix&, const Fix&);
  friend int      operator >  (const Fix&, const Fix&);
  friend int      operator >= (const Fix&, const Fix&);

  Fix&            operator +  ();
  Fix             operator -  ();

  friend Fix      operator +  (const Fix&, const Fix&);
  friend Fix      operator -  (const Fix&, const Fix&);
  friend Fix      operator *  (const Fix&, const Fix&);
  friend Fix      operator /  (const Fix&, const Fix&);

  friend Fix      operator *  (const Fix&, int);
  friend Fix      operator *  (int, const Fix&);
  friend Fix      operator %  (const Fix&, int);
  friend Fix      operator << (const Fix&, int);
  friend Fix      operator >> (const Fix&, int);

#if defined (__GNUG__) && ! defined (__STRICT_ANSI__)
  friend Fix     operator <? (const Fix&, const Fix&); // min
  friend Fix     operator >? (const Fix&, const Fix&); // max
#endif

  Fix            operator += (const Fix&);
  Fix            operator -= (const Fix&);
  Fix            operator *= (const Fix&);
  Fix            operator /= (const Fix&);

  Fix            operator *= (int);
  Fix            operator %= (int);
  Fix            operator <<=(int);
  Fix            operator >>=(int);

  friend char*    Ftoa(const Fix&, int width = default_print_width);
  void		  printon(ostream&, int width = default_print_width) const;
  friend Fix      atoF(const char*, int len = default_length);
  
  friend istream& operator >> (istream&, Fix&);
  friend ostream& operator << (ostream&, const Fix&);

  // built-in functions
  friend Fix      abs(Fix);             // absolute value
  friend int      sgn(const Fix&);	// -1, 0, +1
  friend Integer  mantissa(const Fix&);	// integer representation
  friend double   value(const Fix&);	// double value
  friend int      length(const Fix&);	// field length
  friend void	  show(const Fix&);	// show contents

  // error handlers
  static void     error(const char* msg); // error handler
  static void     range_error(const char* msg);	// range error handler

  static one_arg_error_handler_t set_error_handler(one_arg_error_handler_t f);
  static one_arg_error_handler_t
    set_range_error_handler(one_arg_error_handler_t f);

  static void	  default_error_handler (const char *);
  static void	  default_range_error_handler (const char *);

  // non-operator versions for user
  friend void	  negate(const Fix& x, Fix& r);
  friend void	  add(const Fix& x, const Fix& y, Fix& r);
  friend void	  subtract(const Fix& x, const Fix& y, Fix& r);
  friend void	  multiply(const Fix& x, const Fix& y, Fix& r);
  friend void	  divide(const Fix& x, const Fix& y, Fix& q, Fix& r);
  friend void	  shift(const Fix& x, int y, Fix& r);

  // overflow handlers
  static void overflow_saturate(Fix::Rep*);
  static void overflow_wrap(Fix::Rep*);
  static void overflow_warning_saturate(Fix::Rep*);
  static void overflow_warning(Fix::Rep*);
  static void overflow_error(Fix::Rep*);

  static PEH set_overflow_handler(PEH);

  static int set_default_length(int);
};

// function definitions

inline void
Fix::unique()
{
  if ( rep->ref > 1 )
  {
    rep->ref--;
    rep = new_Fix(rep->len,rep);
  }
}

inline void
Fix::mask (Fix::Rep* x)
{
  int n = x->len & 0x0f;
  if ( n )
    x->s[x->siz - 1] &= 0xffff0000 >> n; 
}

inline Fix::Rep*
Fix::copy(const Fix::Rep* from, Fix::Rep* to)
{
  _G_uint16_t *ts = to->s;
  const _G_uint16_t *fs = from->s;
  int ilim = to->siz < from->siz ? to->siz : from->siz;
  int i;
  for ( i=0; i < ilim; i++ )
    *ts++ = *fs++;
  for ( ; i < to->siz; i++ )
    *ts++ = 0;
  mask(to);
  return to;
}

inline
Fix::Fix(Rep* f)
{
  rep = f;
}

inline
Fix::Fix()
{
  rep = new_Fix(default_length);
}

inline
Fix::Fix(int len)
{
  if ( len < min_length || len > max_length )
    error("illegal length in declaration");
  rep = new_Fix((_G_uint16_t) len);
}

inline
Fix::Fix(_G_uint16_t len)
{
  if ( len < min_length || len > max_length )
    error("illegal length in declaration");
  rep = new_Fix(len);
}

inline
Fix::Fix(double d)
{
  rep = new_Fix(default_length,d);
}

inline
Fix::Fix(const Fix&  y)
{
  rep = y.rep; rep->ref++;
}

inline
Fix::Fix(int len, const Fix&  y)
{
  if ( len < Fix::min_length || len > Fix::max_length )
    error("illegal length in declaration");
  rep = new_Fix((_G_uint16_t) len,y.rep);
}

inline
Fix::Fix(int len, const Rep* fr)
{
  if ( len < Fix::min_length || len > Fix::max_length )
    error("illegal length in declaration");
  rep = new_Fix((_G_uint16_t) len,fr);
}

inline
Fix::Fix(int len, double d)
{
  if ( len < Fix::min_length || len > Fix::max_length )
    error("illegal length in declaration");
  rep = new_Fix((_G_uint16_t) len,d);
}

inline
Fix::~Fix()
{
  if ( --rep->ref <= 0 ) delete rep;
}

inline Fix
Fix::operator = (const Fix&  y)
{
  if ( rep->len == y.rep->len ) {
    ++y.rep->ref;
    if ( --rep->ref <= 0 ) delete rep;
    rep = y.rep; 
  }
  else {
    unique();
    copy(y.rep,rep);
  }
  return *this;
}

inline Fix
Fix::operator = (double d)
{
  int oldlen = rep->len;
  if ( --rep->ref <= 0 ) delete rep;
  rep = new_Fix(oldlen,d);
  return *this;
}

inline int
operator == (const Fix&  x, const Fix&  y)
{
  return Fix::compare(x.rep, y.rep) == 0; 
}

inline int
operator != (const Fix&  x, const Fix&  y)
{
  return Fix::compare(x.rep, y.rep) != 0; 
}

inline int
operator <  (const Fix&  x, const Fix&  y)
{
  return Fix::compare(x.rep, y.rep) <  0; 
}

inline int
operator <= (const Fix&  x, const Fix&  y)
{
  return Fix::compare(x.rep, y.rep) <= 0; 
}

inline int
operator >  (const Fix&  x, const Fix&  y)
{
  return Fix::compare(x.rep, y.rep) >  0; 
}

inline int
operator >= (const Fix&  x, const Fix&  y)
{
  return Fix::compare(x.rep, y.rep) >= 0; 
}

inline Fix&
Fix::operator +  ()
{
  return *this;
}

inline Fix
Fix::operator -  ()
{
  Rep* r = negate(rep); return r;
}

inline Fix
operator +  (const Fix& x, const Fix& y)
{
  Fix::Rep* r = Fix::add(x.rep, y.rep); return r;
}

inline Fix
operator -  (const Fix& x, const Fix& y)
{
  Fix::Rep* r = Fix::subtract(x.rep, y.rep); return r;
}

inline Fix
operator *  (const Fix& x, const Fix& y)
{
  Fix::Rep* r = Fix::multiply(x.rep, y.rep); return r;
}

inline Fix
operator *  (const Fix& x, int y)
{
  Fix::Rep* r = Fix::multiply(x.rep, y); return r;
}

inline Fix
operator *  (int y, const Fix& x)
{
  Fix::Rep* r = Fix::multiply(x.rep, y); return r;
}

inline Fix
operator / (const Fix& x, const Fix& y)
{
  Fix::Rep* r = Fix::divide(x.rep, y.rep); return r;
}

inline Fix
Fix::operator += (const Fix& y)
{
  unique(); Fix::add(rep, y.rep, rep); return *this;
}

inline Fix
Fix::operator -= (const Fix& y)
{
  unique(); Fix::subtract(rep, y.rep, rep); return *this;
}

inline Fix
Fix::operator *= (const Fix& y)
{
  unique(); Fix::multiply(rep, y.rep, rep); return *this;
}

inline Fix
Fix::operator *= (int y)
{
  unique(); Fix::multiply(rep, y, rep); return *this;
}

inline Fix
Fix::operator /= (const Fix& y)
{
  unique(); Fix::divide(rep, y.rep, rep); return *this;
}

inline Fix
operator % (const Fix& x, int y)
{
  Fix r((int) x.rep->len + y, x); return r;
}

inline Fix
operator << (const Fix&  x, int y)
{
  Fix::Rep* rep = Fix::shift(x.rep, y); return rep;
}

inline Fix
operator >> (const Fix&  x, int y)
{  
  Fix::Rep* rep = Fix::shift(x.rep, -y); return rep;
}

inline Fix
Fix::operator <<= (int y)
{
  unique(); Fix::shift(rep, y, rep); return *this;
}

inline Fix
Fix::operator >>= (int y)
{
  unique(); Fix::shift(rep, -y, rep); return *this;
}

#if defined (__GNUG__) && ! defined (__STRICT_ANSI__)
inline Fix
operator <? (const Fix& x, const Fix& y)
{
  if ( Fix::compare(x.rep, y.rep) <= 0 ) return x; else return y;
}

inline Fix
operator >? (const Fix& x, const Fix& y)
{
  if ( Fix::compare(x.rep, y.rep) >= 0 ) return x; else return y;
}
#endif

inline Fix
abs(Fix  x)
{
  Fix::Rep* r = (Fix::compare(x.rep) >= 0 ? Fix::new_Fix(x.rep->len,x.rep) :
		 Fix::negate(x.rep));
  return r;
}

inline int
sgn(const Fix& x)
{
  int a = Fix::compare(x.rep);
  return a == 0 ? 0 : (a > 0 ? 1 : -1);
}

inline int
length(const Fix& x)
{
  return x.rep->len;
}

inline ostream&
operator << (ostream& s, const Fix& y)
{
  if (s.opfx())
    y.printon(s);
  return s;
}

inline void
negate (const Fix& x, Fix& r)
{
  Fix::negate(x.rep, r.rep);
}

inline void
add (const Fix& x, const Fix& y, Fix& r)
{
  Fix::add(x.rep, y.rep, r.rep);
}

inline void
subtract (const Fix& x, const Fix& y, Fix& r)
{
  Fix::subtract(x.rep, y.rep, r.rep);
}

inline void
multiply (const Fix& x, const Fix& y, Fix& r)
{
  Fix::multiply(x.rep, y.rep, r.rep);
}

inline void
divide (const Fix& x, const Fix& y, Fix& q, Fix& r)
{
  Fix::divide(x.rep, y.rep, q.rep, r.rep);
}

inline void
shift (const Fix& x, int y, Fix& r)
{
  Fix::shift(x.rep, y, r.rep);
}

#endif
