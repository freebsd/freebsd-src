//    This is part of the iostream library, providing -*- C++ -*- input/output.
//    Copyright (C) 1991 Per Bothner.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef _IOSTREAM_H
#ifdef __GNUG__
#pragma interface
#endif
#define _IOSTREAM_H

#include <streambuf.h>

class istream; class ostream;
typedef ios& (*__manip)(ios&);
typedef istream& (*__imanip)(istream&);
typedef ostream& (*__omanip)(ostream&);

extern istream& ws(istream& ins);
extern ostream& flush(ostream& outs);
extern ostream& endl(ostream& outs);
extern ostream& ends(ostream& outs);

class ostream : virtual public ios
{
    // NOTE: If fields are changed, you must fix _fake_ostream in stdstreams.C!
    void do_osfx();
  public:
    ostream() { }
    ostream(streambuf* sb, ostream* tied=NULL);
    int opfx() {
	if (!good()) return 0; else { if (_tie) _tie->flush(); return 1;} }
    void osfx() { if (flags() & (ios::unitbuf|ios::stdio))
		      do_osfx(); }
    streambuf* ostreambuf() const { return _strbuf; }
    ostream& flush();
    ostream& put(char c) { _strbuf->sputc(c); return *this; }
    ostream& put(unsigned char c) { return put((char)c); }

    ostream& write(const char *s, int n);
    ostream& write(const unsigned char *s, int n) { return write((const char*)s, n);}
#ifndef _G_BROKEN_SIGNED_CHAR
    ostream& put(signed char c) { return put((char)c); }
    ostream& write(const signed char *s, int n) { return write((const char*)s, n);}
#endif
    ostream& write(const void *s, int n) { return write((const char*)s, n);}
    ostream& seekp(streampos);
    ostream& seekp(streamoff, _seek_dir);
    streampos tellp();
    ostream& form(const char *format ...);
    ostream& vform(const char *format, _G_va_list args);

    ostream& operator<<(char c);
    ostream& operator<<(unsigned char c) { return (*this) << (char)c; }
#ifndef _G_BROKEN_SIGNED_CHAR
    ostream& operator<<(signed char c) { return (*this) << (char)c; }
#endif
    ostream& operator<<(const char *s);
    ostream& operator<<(const unsigned char *s)
	{ return (*this) << (const char*)s; }
#ifndef _G_BROKEN_SIGNED_CHAR
    ostream& operator<<(const signed char *s)
	{ return (*this) << (const char*)s; }
#endif
    ostream& operator<<(const void *p);
    ostream& operator<<(int n);
    ostream& operator<<(unsigned int n);
    ostream& operator<<(long n);
    ostream& operator<<(unsigned long n);
#ifdef __GNUG__
    ostream& operator<<(long long n);
    ostream& operator<<(unsigned long long n);
#endif
    ostream& operator<<(short n) {return operator<<((int)n);}
    ostream& operator<<(unsigned short n) {return operator<<((unsigned int)n);}
    ostream& operator<<(double n);
    ostream& operator<<(float n) { return operator<<((double)n); }
    ostream& operator<<(__omanip func) { return (*func)(*this); }
    ostream& operator<<(__manip func) {(*func)(*this); return *this;}
    ostream& operator<<(streambuf*);
};

class istream : virtual public ios
{
    // NOTE: If fields are changed, you must fix _fake_istream in stdstreams.C!
    _G_ssize_t _gcount;

    int _skip_ws();
  public:
    istream() { _gcount = 0; }
    istream(streambuf* sb, ostream*tied=NULL);
    streambuf* istreambuf() const { return _strbuf; }
    istream& get(char* ptr, int len, char delim = '\n');
    istream& get(unsigned char* ptr, int len, char delim = '\n')
	{ return get((char*)ptr, len, delim); }
    istream& get(char& c);
    istream& get(unsigned char& c) { return get((char&)c); }
    istream& getline(char* ptr, int len, char delim = '\n');
    istream& getline(unsigned char* ptr, int len, char delim = '\n')
	{ return getline((char*)ptr, len, delim); }
#ifndef _G_BROKEN_SIGNED_CHAR
    istream& get(signed char& c)  { return get((char&)c); }
    istream& get(signed char* ptr, int len, char delim = '\n')
	{ return get((char*)ptr, len, delim); }
    istream& getline(signed char* ptr, int len, char delim = '\n')
	{ return getline((char*)ptr, len, delim); }
#endif
    istream& read(char *ptr, int n);
    istream& read(unsigned char *ptr, int n) { return read((char*)ptr, n); }
#ifndef _G_BROKEN_SIGNED_CHAR
    istream& read(signed char *ptr, int n) { return read((char*)ptr, n); }
#endif
    istream& read(void *ptr, int n) { return read((char*)ptr, n); }
    istream& get(streambuf& sb, char delim = '\n');
    istream& gets(char **s, char delim = '\n');
    int ipfx(int need) {
	if (!good()) { set(ios::failbit); return 0; }
	else {
	  if (_tie && (need == 0 || rdbuf()->in_avail() < need)) _tie->flush();
	  if (!need && (flags() & ios::skipws)) return _skip_ws();
	  else return 1;
	}
    }
    int ipfx0() { // Optimized version of ipfx(0).
	if (!good()) { set(ios::failbit); return 0; }
	else {
	  if (_tie) _tie->flush();
	  if (flags() & ios::skipws) return _skip_ws();
	  else return 1;
	}
    }
    int ipfx1() { // Optimized version of ipfx(1).
	if (!good()) { set(ios::failbit); return 0; }
	else {
	  if (_tie && rdbuf()->in_avail() == 0) _tie->flush();
	  return 1;
	}
    }
    void isfx() { }
    int get() { if (!ipfx1()) return EOF;
		else { int ch = _strbuf->sbumpc();
		       if (ch == EOF) set(ios::eofbit);
		       return ch;
		     } }
    int peek();
    _G_ssize_t gcount() { return _gcount; }
    istream& ignore(int n=1, int delim = EOF);
    istream& seekg(streampos);
    istream& seekg(streamoff, _seek_dir);
    streampos tellg();
    istream& putback(char ch) {
	if (good() && _strbuf->sputbackc(ch) == EOF) clear(ios::badbit);
	return *this;}
    istream& unget() {
	if (good() && _strbuf->sungetc() == EOF) clear(ios::badbit);
	return *this;}
    istream& scan(const char *format ...);
    istream& vscan(const char *format, _G_va_list args);
#ifdef _STREAM_COMPAT
    istream& unget(char ch) { return putback(ch); }
    int skip(int i);
#endif

    istream& operator>>(char*);
    istream& operator>>(unsigned char* p) { return operator>>((char*)p); }
#ifndef _G_BROKEN_SIGNED_CHAR
    istream& operator>>(signed char*p) { return operator>>((char*)p); }
#endif
    istream& operator>>(char& c);
    istream& operator>>(unsigned char& c) {return operator>>((char&)c);}
#ifndef _G_BROKEN_SIGNED_CHAR
    istream& operator>>(signed char& c) {return operator>>((char&)c);}
#endif
    istream& operator>>(int&);
    istream& operator>>(long&);
#ifdef __GNUG__
    istream& operator>>(long long&);
#endif
    istream& operator>>(short&);
    istream& operator>>(unsigned int&);
    istream& operator>>(unsigned long&);
#ifdef __GNUG__
    istream& operator>>(unsigned long long&);
#endif
    istream& operator>>(unsigned short&);
    istream& operator>>(float&);
    istream& operator>>(double&);
    istream& operator>>( __manip func) {(*func)(*this); return *this;}
    istream& operator>>(__imanip func) { return (*func)(*this); }
    istream& operator>>(streambuf*);
};


class iostream : public istream, public ostream
{
    _G_ssize_t _gcount;
  public:
    iostream() { _gcount = 0; }
    iostream(streambuf* sb, ostream*tied=NULL);
};

extern istream cin;
extern ostream cout, cerr, clog; // clog->rdbuf() == cerr->rdbuf()

struct Iostream_init { } ;  // Compatibility hack for AT&T library.

inline ios& dec(ios& i)
{ i.setf(ios::dec, ios::dec|ios::hex|ios::oct); return i; }
inline ios& hex(ios& i)
{ i.setf(ios::hex, ios::dec|ios::hex|ios::oct); return i; }
inline ios& oct(ios& i)
{ i.setf(ios::oct, ios::dec|ios::hex|ios::oct); return i; }

#endif /*!_IOSTREAM_H*/
