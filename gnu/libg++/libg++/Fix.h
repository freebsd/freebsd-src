//
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

typedef unsigned short uint16;
typedef short int16;
typedef unsigned long  uint32;
typedef long int32;

#define _Fix_min_length 	1
#define _Fix_max_length		65535

#define _Fix_min_value		-1.0
#define _Fix_max_value		1.0

extern uint16 Fix_default_length;
extern int    Fix_default_print_width;

struct _Frep                    // internal Fix representation
{
  uint16 len;          		// length in bits
  uint16 siz;			// allocated storage
  int16 ref;          		// reference count
  uint16 s[1];			// start of ushort array represention
};

typedef _Frep* _Fix;

extern _Frep _Frep_0;
extern _Frep _Frep_m1;
extern _Frep _Frep_quotient_bump;

class Fix
{
  _Fix            rep;

		  Fix(_Fix);

  void		  unique();

public:
		  Fix();
                  Fix(Fix&);
		  Fix(double);
                  Fix(int);
                  Fix(int, const Fix&);
                  Fix(int, double);
                  Fix(int, const _Fix);

                  ~Fix();

  Fix             operator =  (Fix&);
  Fix             operator =  (double);

  friend int      operator == (const Fix&, const Fix& );
  friend int      operator != (const Fix&, const Fix&);

  friend int      operator <  (const Fix&, const Fix&);
  friend int      operator <= (const Fix&, const Fix&);
  friend int      operator >  (const Fix&, const Fix&);
  friend int      operator >= (const Fix&, const Fix&);

  Fix&            operator +  ();
  Fix             operator -  ();

  friend Fix      operator +  (Fix&, Fix&);
  friend Fix      operator -  (Fix&, Fix&);
  friend Fix      operator *  (Fix&, Fix&);
  friend Fix      operator /  (Fix&, Fix&);

  friend Fix      operator *  (Fix&, int);
  friend Fix      operator *  (int, Fix&);
  friend Fix      operator %  (Fix&, int);
  friend Fix      operator << (Fix&, int);
  friend Fix      operator >> (Fix&, int);

#ifdef __GNUG__
  friend Fix     operator <? (Fix&, Fix&); // min
  friend Fix     operator >? (Fix&, Fix&); // max
#endif

  Fix            operator += (Fix&);
  Fix            operator -= (Fix&);
  Fix            operator *= (Fix&);
  Fix            operator /= (Fix&);

  Fix            operator *= (int);
  Fix            operator %= (int);
  Fix            operator <<=(int);
  Fix            operator >>=(int);

  friend char*    Ftoa(Fix&, int width = Fix_default_print_width);
  void		  printon(ostream&, int width = Fix_default_print_width) const;
  friend Fix      atoF(const char*, int len = Fix_default_length);
  
  friend istream& operator >> (istream&, Fix&);
  friend ostream& operator << (ostream&, const Fix&);

  // built-in functions
  friend Fix      abs(Fix);		// absolute value
  friend int      sgn(Fix&);		// -1, 0, +1
  friend Integer  mantissa(Fix&);	// integer representation
  friend double   value(const Fix&);	// double value
  friend int      length(const Fix&);	// field length
  friend void	  show(Fix&);		// show contents

  // error handlers
  void            error(const char* msg);		// error handler
  void            range_error(const char* msg);	// range error handler

  // internal class functions
  friend void	  mask(_Fix);
  friend int      compare(const _Fix, const _Fix = &_Frep_0);

  friend _Fix	  new_Fix(uint16);
  friend _Fix	  new_Fix(uint16, const _Fix);
  friend _Fix	  new_Fix(uint16, double);

  friend _Fix	  copy(const _Fix, _Fix);
  friend _Fix	  negate(_Fix, _Fix = NULL);
  friend _Fix	  add(_Fix, _Fix, _Fix = NULL);
  friend _Fix	  subtract(_Fix, _Fix, _Fix = NULL);
  friend _Fix	  multiply(_Fix, _Fix, _Fix = NULL);
  friend _Fix	  multiply(_Fix, int, _Fix = NULL);
  friend _Fix	  divide(_Fix, _Fix, _Fix = NULL, _Fix = NULL);
  friend _Fix	  shift(_Fix, int, _Fix = NULL);

  // non-operator versions for user
  friend void	  negate(Fix& x, Fix& r);
  friend void	  add(Fix& x, Fix& y, Fix& r);
  friend void	  subtract(Fix& x, Fix& y, Fix& r);
  friend void	  multiply(Fix& x, Fix& y, Fix& r);
  friend void	  divide(Fix& x, Fix& y, Fix& q, Fix& r);
  friend void	  shift(Fix& x, int y, Fix& r);
};

// error handlers

extern void	
  default_Fix_error_handler(const char*),
  default_Fix_range_error_handler(const char*);

extern one_arg_error_handler_t 
  Fix_error_handler,
  Fix_range_error_handler;

extern one_arg_error_handler_t 
  set_Fix_error_handler(one_arg_error_handler_t f),
  set_Fix_range_error_handler(one_arg_error_handler_t f);

typedef void (*Fix_peh)(_Fix&);
extern Fix_peh Fix_overflow_handler;

extern void 
  Fix_overflow_saturate(_Fix&),
  Fix_overflow_wrap(_Fix&),
  Fix_overflow_warning_saturate(_Fix&),
  Fix_overflow_warning(_Fix&),
  Fix_overflow_error(_Fix&);

extern Fix_peh set_overflow_handler(Fix_peh);

extern int Fix_set_default_length(int);

// function definitions


inline void Fix::unique()
{
  if ( rep->ref > 1 )
  {
    rep->ref--;
    rep = new_Fix(rep->len,rep);
  }
}

inline void mask (_Fix x)
{
  int n = x->len & 0x0f;
  if ( n )
    x->s[x->siz - 1] &= 0xffff0000 >> n; 
}

inline _Fix copy(const _Fix from, _Fix to)
{
  uint16 *ts = to->s, *fs = from->s;
  int ilim = to->siz < from->siz ? to->siz : from->siz;
  for ( int i=0; i < ilim; i++ )
    *ts++ = *fs++;
  for ( ; i < to->siz; i++ )
    *ts++ = 0;
  mask(to);
  return to;
}

inline Fix::Fix(_Fix f)
{
  rep = f;
}

inline Fix::Fix()
{
  rep = new_Fix(Fix_default_length);
}

inline Fix::Fix(int len)
{
  if ( len < _Fix_min_length || len > _Fix_max_length )
    error("illegal length in declaration");
  rep = new_Fix((uint16 )len);
}

inline Fix::Fix(double d)
{
  rep = new_Fix(Fix_default_length,d);
}

inline Fix::Fix(Fix&  y)
{
  rep = y.rep; rep->ref++;
}

inline Fix::Fix(int len, const Fix&  y)
{
  if ( len < _Fix_min_length || len > _Fix_max_length )
    error("illegal length in declaration");
  rep = new_Fix((uint16 )len,y.rep);
}

inline Fix::Fix(int len, const _Fix fr)
{
  if ( len < 	1  || len > 	65535  )
    error("illegal length in declaration");
  rep = new_Fix((uint16 )len,fr);
}

inline Fix::Fix(int len, double d)
{
  if ( len < _Fix_min_length || len > _Fix_max_length )
    error("illegal length in declaration");
  rep = new_Fix((uint16 )len,d);
}

inline Fix::~Fix()
{
  if ( --rep->ref <= 0 ) delete rep;
}

inline Fix  Fix::operator = (Fix&  y)
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

inline Fix  Fix::operator = (double d)
{
  int oldlen = rep->len;
  if ( --rep->ref <= 0 ) delete rep;
  rep = new_Fix(oldlen,d);
  return *this;
}

inline int operator == (const Fix&  x, const Fix&  y)
{
  return compare(x.rep, y.rep) == 0; 
}

inline int operator != (const Fix&  x, const Fix&  y)
{
  return compare(x.rep, y.rep) != 0; 
}

inline int operator <  (const Fix&  x, const Fix&  y)
{
  return compare(x.rep, y.rep) <  0; 
}

inline int operator <= (const Fix&  x, const Fix&  y)
{
  return compare(x.rep, y.rep) <= 0; 
}

inline int operator >  (const Fix&  x, const Fix&  y)
{
  return compare(x.rep, y.rep) >  0; 
}

inline int operator >= (const Fix&  x, const Fix&  y)
{
  return compare(x.rep, y.rep) >= 0; 
}

inline Fix& Fix::operator +  ()
{
  return *this;
}

inline Fix Fix::operator -  ()
{
  _Fix r = negate(rep); return r;
}

inline Fix      operator +  (Fix&  x, Fix& y)
{
  _Fix r = add(x.rep, y.rep); return r;
}

inline Fix      operator -  (Fix&  x, Fix& y)
{
  _Fix r = subtract(x.rep, y.rep); return r;
}

inline Fix      operator *  (Fix&  x, Fix& y)
{
  _Fix r = multiply(x.rep, y.rep); return r;
}

inline Fix      operator *  (Fix&  x, int y)
{
  _Fix r = multiply(x.rep, y); return r;
}

inline Fix      operator *  (int  y, Fix& x)
{
  _Fix r = multiply(x.rep, y); return r;
}

inline Fix operator / (Fix& x, Fix& y)
{
  _Fix r = divide(x.rep, y.rep); return r;
}

inline Fix  Fix::operator += (Fix& y)
{
  unique(); add(rep, y.rep, rep); return *this;
}

inline Fix  Fix::operator -= (Fix& y)
{
  unique(); subtract(rep, y.rep, rep); return *this;
}

inline Fix  Fix::operator *= (Fix& y)
{
  unique(); multiply(rep, y.rep, rep); return *this;
}

inline Fix  Fix::operator *= (int y)
{
  unique(); multiply(rep, y, rep); return *this;
}

inline Fix Fix::operator /= (Fix& y)
{
  unique(); divide(rep, y.rep, rep); return *this;
}

inline Fix operator % (Fix& x, int y)
{
  Fix r((int )x.rep->len + y, x); return r;
}

inline Fix      operator << (Fix&  x, int y)
{
  _Fix rep = shift(x.rep, y); return rep;
}

inline Fix      operator >> (Fix&  x, int y)
{  
  _Fix rep = shift(x.rep, -y); return rep;
}

inline Fix Fix::operator <<= (int y)
{
  unique(); shift(rep, y, rep); return *this;
}

inline Fix  Fix::operator >>= (int y)
{
  unique(); shift(rep, -y, rep); return *this;
}

#ifdef __GNUG__
inline Fix operator <? (Fix& x, Fix& y)
{
  if ( compare(x.rep, y.rep) <= 0 ) return x; else return y;
}

inline Fix operator >? (Fix& x, Fix& y)
{
  if ( compare(x.rep, y.rep) >= 0 ) return x; else return y;
}
#endif

inline Fix abs(Fix  x)
{
  _Fix r = (compare(x.rep) >= 0 ? new_Fix(x.rep->len,x.rep) : negate(x.rep));
  return r;
}

inline int sgn(Fix& x)
{
  int a = compare(x.rep);
  return a == 0 ? 0 : (a > 0 ? 1 : -1);
}

inline int length(const Fix& x)
{
  return x.rep->len;
}

inline ostream& operator << (ostream& s, const Fix& y)
{
  if (s.opfx())
    y.printon(s);
  return s;
}

inline void	negate (Fix& x, Fix& r)
{
  negate(x.rep, r.rep);
}

inline void	add (Fix& x, Fix& y, Fix& r)
{
  add(x.rep, y.rep, r.rep);
}

inline void	subtract (Fix& x, Fix& y, Fix& r)
{
  subtract(x.rep, y.rep, r.rep);
}

inline void	multiply (Fix& x, Fix& y, Fix& r)
{
  multiply(x.rep, y.rep, r.rep);
}

inline void	divide (Fix& x, Fix& y, Fix& q, Fix& r)
{
  divide(x.rep, y.rep, q.rep, r.rep);
}

inline void	shift (Fix& x, int y, Fix& r)
{
  shift(x.rep, y, r.rep);
}

#endif
