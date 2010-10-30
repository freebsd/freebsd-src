// elfcpp_swap.h -- Handle swapping for elfcpp   -*- C++ -*-

// This header file defines basic template classes to efficiently swap
// numbers between host form and target form.  When the host and
// target have the same endianness, these turn into no-ops.

#ifndef ELFCPP_SWAP_H
#define ELFCPP_SWAP_H

#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

namespace elfcpp
{

// Endian simply indicates whether the host is big endian or not.

struct Endian
{
 public:
  // Used for template specializations.
  static const bool host_big_endian = __BYTE_ORDER == __BIG_ENDIAN;
};

// Valtype_base is a template based on size (8, 16, 32, 64) which
// defines the type Valtype as the unsigned integer of the specified
// size.

template<int size>
struct Valtype_base;

template<>
struct Valtype_base<8>
{
  typedef unsigned char Valtype;
};

template<>
struct Valtype_base<16>
{
  typedef uint16_t Valtype;
};

template<>
struct Valtype_base<32>
{
  typedef uint32_t Valtype;
};

template<>
struct Valtype_base<64>
{
  typedef uint64_t Valtype;
};

// Convert_endian is a template based on size and on whether the host
// and target have the same endianness.  It defines the type Valtype
// as Valtype_base does, and also defines a function convert_host
// which takes an argument of type Valtype and returns the same value,
// but swapped if the host and target have different endianness.

template<int size, bool same_endian>
struct Convert_endian;

template<int size>
struct Convert_endian<size, true>
{
  typedef typename Valtype_base<size>::Valtype Valtype;

  static inline Valtype
  convert_host(Valtype v)
  { return v; }
};

template<>
struct Convert_endian<8, false>
{
  typedef Valtype_base<8>::Valtype Valtype;

  static inline Valtype
  convert_host(Valtype v)
  { return v; }
};

template<>
struct Convert_endian<16, false>
{
  typedef Valtype_base<16>::Valtype Valtype;

  static inline Valtype
  convert_host(Valtype v)
  { return bswap_16(v); }
};

template<>
struct Convert_endian<32, false>
{
  typedef Valtype_base<32>::Valtype Valtype;

  static inline Valtype
  convert_host(Valtype v)
  { return bswap_32(v); }
};

template<>
struct Convert_endian<64, false>
{
  typedef Valtype_base<64>::Valtype Valtype;

  static inline Valtype
  convert_host(Valtype v)
  { return bswap_64(v); }
};

// Convert is a template based on size and on whether the target is
// big endian.  It defines Valtype and convert_host like
// Convert_endian.  That is, it is just like Convert_endian except in
// the meaning of the second template parameter.

template<int size, bool big_endian>
struct Convert
{
  typedef typename Valtype_base<size>::Valtype Valtype;

  static inline Valtype
  convert_host(Valtype v)
  {
    return Convert_endian<size, big_endian == Endian::host_big_endian>
      ::convert_host(v);
  }
};

// Swap is a template based on size and on whether the target is big
// endian.  It defines the type Valtype and the functions readval and
// writeval.  The functions read and write values of the appropriate
// size out of buffers, swapping them if necessary.  readval and
// writeval are overloaded to take pointers to the appropriate type or
// pointers to unsigned char.

template<int size, bool big_endian>
struct Swap
{
  typedef typename Valtype_base<size>::Valtype Valtype;

  static inline Valtype
  readval(const Valtype* wv)
  { return Convert<size, big_endian>::convert_host(*wv); }

  static inline void
  writeval(Valtype* wv, Valtype v)
  { *wv = Convert<size, big_endian>::convert_host(v); }

  static inline Valtype
  readval(const unsigned char* wv)
  { return readval(reinterpret_cast<const Valtype*>(wv)); }

  static inline void
  writeval(unsigned char* wv, Valtype v)
  { writeval(reinterpret_cast<Valtype*>(wv), v); }
};

// We need to specialize the 8-bit version of Swap to avoid
// conflicting overloads, since both versions of readval and writeval
// will have the same type parameters.

template<bool big_endian>
struct Swap<8, big_endian>
{
  typedef typename Valtype_base<8>::Valtype Valtype;

  static inline Valtype
  readval(const Valtype* wv)
  { return *wv; }

  static inline void
  writeval(Valtype* wv, Valtype v)
  { *wv = v; }
};

// Swap_unaligned is a template based on size and on whether the
// target is big endian.  It defines the type Valtype and the
// functions readval and writeval.  The functions read and write
// values of the appropriate size out of buffers which may be
// misaligned.

template<int size, bool big_endian>
struct Swap_unaligned;

template<bool big_endian>
struct Swap_unaligned<8, big_endian>
{
  typedef typename Valtype_base<8>::Valtype Valtype;

  static inline Valtype
  readval(const unsigned char* wv)
  { return *wv; }

  static inline void
  writeval(unsigned char* wv, Valtype v)
  { *wv = v; }
};

template<>
struct Swap_unaligned<16, false>
{
  typedef Valtype_base<16>::Valtype Valtype;

  static inline Valtype
  readval(const unsigned char* wv)
  {
    return (wv[1] << 8) | wv[0];
  }

  static inline void
  writeval(unsigned char* wv, Valtype v)
  {
    wv[1] = v >> 8;
    wv[0] = v;
  }
};

template<>
struct Swap_unaligned<16, true>
{
  typedef Valtype_base<16>::Valtype Valtype;

  static inline Valtype
  readval(const unsigned char* wv)
  {
    return (wv[0] << 8) | wv[1];
  }

  static inline void
  writeval(unsigned char* wv, Valtype v)
  {
    wv[0] = v >> 8;
    wv[1] = v;
  }
};

template<>
struct Swap_unaligned<32, false>
{
  typedef Valtype_base<32>::Valtype Valtype;

  static inline Valtype
  readval(const unsigned char* wv)
  {
    return (wv[3] << 24) | (wv[2] << 16) | (wv[1] << 8) | wv[0];
  }

  static inline void
  writeval(unsigned char* wv, Valtype v)
  {
    wv[3] = v >> 24;
    wv[2] = v >> 16;
    wv[1] = v >> 8;
    wv[0] = v;
  }
};

template<>
struct Swap_unaligned<32, true>
{
  typedef Valtype_base<32>::Valtype Valtype;

  static inline Valtype
  readval(const unsigned char* wv)
  {
    return (wv[0] << 24) | (wv[1] << 16) | (wv[2] << 8) | wv[3];
  }

  static inline void
  writeval(unsigned char* wv, Valtype v)
  {
    wv[0] = v >> 24;
    wv[1] = v >> 16;
    wv[2] = v >> 8;
    wv[3] = v;
  }
};

template<>
struct Swap_unaligned<64, false>
{
  typedef Valtype_base<64>::Valtype Valtype;

  static inline Valtype
  readval(const unsigned char* wv)
  {
    return ((static_cast<Valtype>(wv[7]) << 56)
	    | (static_cast<Valtype>(wv[6]) << 48)
	    | (static_cast<Valtype>(wv[5]) << 40)
	    | (static_cast<Valtype>(wv[4]) << 32)
	    | (static_cast<Valtype>(wv[3]) << 24)
	    | (static_cast<Valtype>(wv[2]) << 16)
	    | (static_cast<Valtype>(wv[1]) << 8)
	    | static_cast<Valtype>(wv[0]));
  }

  static inline void
  writeval(unsigned char* wv, Valtype v)
  {
    wv[7] = v >> 56;
    wv[6] = v >> 48;
    wv[5] = v >> 40;
    wv[4] = v >> 32;
    wv[3] = v >> 24;
    wv[2] = v >> 16;
    wv[1] = v >> 8;
    wv[0] = v;
  }
};

template<>
struct Swap_unaligned<64, true>
{
  typedef Valtype_base<64>::Valtype Valtype;

  static inline Valtype
  readval(const unsigned char* wv)
  {
    return ((static_cast<Valtype>(wv[0]) << 56)
	    | (static_cast<Valtype>(wv[1]) << 48)
	    | (static_cast<Valtype>(wv[2]) << 40)
	    | (static_cast<Valtype>(wv[3]) << 32)
	    | (static_cast<Valtype>(wv[4]) << 24)
	    | (static_cast<Valtype>(wv[5]) << 16)
	    | (static_cast<Valtype>(wv[6]) << 8)
	    | static_cast<Valtype>(wv[7]));
  }

  static inline void
  writeval(unsigned char* wv, Valtype v)
  {
    wv[7] = v >> 56;
    wv[6] = v >> 48;
    wv[5] = v >> 40;
    wv[4] = v >> 32;
    wv[3] = v >> 24;
    wv[2] = v >> 16;
    wv[1] = v >> 8;
    wv[0] = v;
  }
};

} // End namespace elfcpp.

#endif // !defined(ELFCPP_SWAP_H)
