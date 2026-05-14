/*  $OpenBSD: libcrux_mlkem768_sha3.h,v 1.4 2025/11/13 05:13:06 djm Exp $ */

/* Extracted from libcrux revision 026a87ab6d88ad3626b9fbbf3710d1e0483c1849 */

/*
 * MIT License
 *
 * Copyright (c) 2024 Cryspen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if !defined(__GNUC__) || (__GNUC__ < 2)
# define __attribute__(x)
#endif
#define KRML_MUSTINLINE inline
#define KRML_NOINLINE __attribute__((noinline, unused))
#define KRML_HOST_EPRINTF(...)
#define KRML_HOST_EXIT(x) fatal_f("internal error")

static inline void
store64_le(uint8_t dst[8], uint64_t src)
{
	dst[0] = src & 0xff;
	dst[1] = (src >> 8) & 0xff;
	dst[2] = (src >> 16) & 0xff;
	dst[3] = (src >> 24) & 0xff;
	dst[4] = (src >> 32) & 0xff;
	dst[5] = (src >> 40) & 0xff;
	dst[6] = (src >> 48) & 0xff;
	dst[7] = (src >> 56) & 0xff;
}

static inline void
store32_le(uint8_t dst[4], uint32_t src)
{
	dst[0] = src & 0xff;
	dst[1] = (src >> 8) & 0xff;
	dst[2] = (src >> 16) & 0xff;
	dst[3] = (src >> 24) & 0xff;
}

static inline void
store32_be(uint8_t dst[4], uint32_t src)
{
	dst[0] = (src >> 24) & 0xff;
	dst[1] = (src >> 16) & 0xff;
	dst[2] = (src >> 8) & 0xff;
	dst[3] = src & 0xff;
}

static inline uint64_t
load64_le(uint8_t src[8])
{
	return (uint64_t)(src[0]) |
	    ((uint64_t)(src[1]) << 8) |
	    ((uint64_t)(src[2]) << 16) |
	    ((uint64_t)(src[3]) << 24) |
	    ((uint64_t)(src[4]) << 32) |
	    ((uint64_t)(src[5]) << 40) |
	    ((uint64_t)(src[6]) << 48) |
	    ((uint64_t)(src[7]) << 56);
}

static inline uint32_t
load32_le(uint8_t src[4])
{
	return (uint32_t)(src[0]) |
	    ((uint32_t)(src[1]) << 8) |
	    ((uint32_t)(src[2]) << 16) |
	    ((uint32_t)(src[3]) << 24);
}

#ifdef MISSING_BUILTIN_POPCOUNT
static inline unsigned int
__builtin_popcount(unsigned int num)
{
  const int v[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
  return v[num & 0xf] + v[(num >> 4) & 0xf];
}
#endif

/* from libcrux/libcrux-ml-kem/extracts/c_header_only/generated/eurydice_glue.h */
#pragma once


#ifdef _MSC_VER
// For __popcnt
#endif


// C++ HELPERS

#if defined(__cplusplus)

#ifndef KRML_HOST_EPRINTF
#define KRML_HOST_EPRINTF(...) fprintf(stderr, __VA_ARGS__)
#endif


#ifndef __cpp_lib_type_identity
template <class T>
struct type_identity {
  using type = T;
};

template <class T>
using type_identity_t = typename type_identity<T>::type;
#else
using std::type_identity_t;
#endif

#define KRML_UNION_CONSTRUCTOR(T)                              \
  template <typename V>                                        \
  constexpr T(int t, V U::*m, type_identity_t<V> v) : tag(t) { \
    val.*m = std::move(v);                                     \
  }                                                            \
  T() = default;

#endif

// GENERAL-PURPOSE STUFF

#define LowStar_Ignore_ignore(e, t, _ret_t) ((void)e)

#define EURYDICE_ASSERT(test, msg)                                            \
  do {                                                                        \
    if (!(test)) {                                                            \
      fprintf(stderr, "assertion \"%s\" failed: file \"%s\", line %d\n", msg, \
              __FILE__, __LINE__);                                            \
      exit(255);                                                              \
    }                                                                         \
  } while (0)

// SLICES, ARRAYS, ETC.

// We represent a slice as a pair of an (untyped) pointer, along with the length
// of the slice, i.e. the number of elements in the slice (this is NOT the
// number of bytes). This design choice has two important consequences.
// - if you need to use `ptr`, you MUST cast it to a proper type *before*
//   performing pointer arithmetic on it (remember that C desugars pointer
//   arithmetic based on the type of the address)
// - if you need to use `len` for a C style function (e.g. memcpy, memcmp), you
//   need to multiply it by sizeof t, where t is the type of the elements.
//
// Empty slices have `len == 0` and `ptr` always needs to be a valid pointer
// that is not NULL (otherwise the construction in EURYDICE_SLICE computes `NULL
// + start`).
typedef struct {
  void *ptr;
  size_t len;
} Eurydice_slice;

#if defined(__cplusplus)
#define KRML_CLITERAL(type) type
#else
#define KRML_CLITERAL(type) (type)
#endif

#if defined(__cplusplus) && defined(__cpp_designated_initializers) || \
    !(defined(__cplusplus))
#define EURYDICE_CFIELD(X) X
#else
#define EURYDICE_CFIELD(X)
#endif

// Helper macro to create a slice out of a pointer x, a start index in x
// (included), and an end index in x (excluded). The argument x must be suitably
// cast to something that can decay (see remark above about how pointer
// arithmetic works in C), meaning either pointer or array type.
#define EURYDICE_SLICE(x, start, end) \
  (KRML_CLITERAL(Eurydice_slice){(void *)(x + start), end - start})

// Slice length
#define EURYDICE_SLICE_LEN(s, _) (s).len
#define Eurydice_slice_len(s, _) (s).len

// This macro is a pain because in case the dereferenced element type is an
// array, you cannot simply write `t x` as it would yield `int[4] x` instead,
// which is NOT correct C syntax, so we add a dedicated phase in Eurydice that
// adds an extra argument to this macro at the last minute so that we have the
// correct type of *pointers* to elements.
#define Eurydice_slice_index(s, i, t, t_ptr_t) (((t_ptr_t)s.ptr)[i])

// The following functions get sub slices from a slice.

#define Eurydice_slice_subslice(s, r, t, _0, _1) \
  EURYDICE_SLICE((t *)s.ptr, r.start, r.end)

// Variant for when the start and end indices are statically known (i.e., the
// range argument `r` is a literal).
#define Eurydice_slice_subslice2(s, start, end, t) \
  EURYDICE_SLICE((t *)s.ptr, (start), (end))

// Previous version above does not work when t is an array type (as usual). Will
// be deprecated soon.
#define Eurydice_slice_subslice3(s, start, end, t_ptr) \
  EURYDICE_SLICE((t_ptr)s.ptr, (start), (end))

#define Eurydice_slice_subslice_to(s, subslice_end_pos, t, _0, _1) \
  EURYDICE_SLICE((t *)s.ptr, 0, subslice_end_pos)

#define Eurydice_slice_subslice_from(s, subslice_start_pos, t, _0, _1) \
  EURYDICE_SLICE((t *)s.ptr, subslice_start_pos, s.len)

#define Eurydice_array_to_slice(end, x, t) \
  EURYDICE_SLICE(x, 0,                     \
                 end) /* x is already at an array type, no need for cast */
#define Eurydice_array_to_subslice(_arraylen, x, r, t, _0, _1) \
  EURYDICE_SLICE((t *)x, r.start, r.end)

// Same as above, variant for when start and end are statically known
#define Eurydice_array_to_subslice2(x, start, end, t) \
  EURYDICE_SLICE((t *)x, (start), (end))

// Same as above, variant for when start and end are statically known
#define Eurydice_array_to_subslice3(x, start, end, t_ptr) \
  EURYDICE_SLICE((t_ptr)x, (start), (end))

#define Eurydice_array_repeat(dst, len, init, t) \
  ERROR "should've been desugared"

// The following functions convert an array into a slice.

#define Eurydice_array_to_subslice_to(_size, x, r, t, _range_t, _0) \
  EURYDICE_SLICE((t *)x, 0, r)
#define Eurydice_array_to_subslice_from(size, x, r, t, _range_t, _0) \
  EURYDICE_SLICE((t *)x, r, size)

// Copy a slice with memcopy
#define Eurydice_slice_copy(dst, src, t) \
  memcpy(dst.ptr, src.ptr, dst.len * sizeof(t))

#define core_array___Array_T__N___as_slice(len_, ptr_, t, _ret_t) \
  KRML_CLITERAL(Eurydice_slice) { ptr_, len_ }

#define core_array__core__clone__Clone_for__Array_T__N___clone( \
    len, src, dst, elem_type, _ret_t)                           \
  (memcpy(dst, src, len * sizeof(elem_type)))
#define TryFromSliceError uint8_t
#define core_array_TryFromSliceError uint8_t

#define Eurydice_array_eq(sz, a1, a2, t) (memcmp(a1, a2, sz * sizeof(t)) == 0)

// core::cmp::PartialEq<&0 (@Slice<U>)> for @Array<T, N>
#define Eurydice_array_eq_slice(sz, a1, s2, t, _) \
  (memcmp(a1, (s2)->ptr, sz * sizeof(t)) == 0)

#define core_array_equality___core__cmp__PartialEq__Array_U__N___for__Array_T__N____eq( \
    sz, a1, a2, t, _, _ret_t)                                                           \
  Eurydice_array_eq(sz, a1, a2, t, _)
#define core_array_equality___core__cmp__PartialEq__0___Slice_U____for__Array_T__N___3__eq( \
    sz, a1, a2, t, _, _ret_t)                                                               \
  Eurydice_array_eq(sz, a1, ((a2)->ptr), t, _)

#define Eurydice_slice_split_at(slice, mid, element_type, ret_t)          \
  KRML_CLITERAL(ret_t) {                                                  \
    EURYDICE_CFIELD(.fst =)                                               \
    EURYDICE_SLICE((element_type *)(slice).ptr, 0, mid),                  \
        EURYDICE_CFIELD(.snd =)                                           \
            EURYDICE_SLICE((element_type *)(slice).ptr, mid, (slice).len) \
  }

#define Eurydice_slice_split_at_mut(slice, mid, element_type, ret_t)  \
  KRML_CLITERAL(ret_t) {                                              \
    EURYDICE_CFIELD(.fst =)                                           \
    KRML_CLITERAL(Eurydice_slice){EURYDICE_CFIELD(.ptr =)(slice.ptr), \
                                  EURYDICE_CFIELD(.len =) mid},       \
        EURYDICE_CFIELD(.snd =) KRML_CLITERAL(Eurydice_slice) {       \
      EURYDICE_CFIELD(.ptr =)                                         \
      ((char *)slice.ptr + mid * sizeof(element_type)),               \
          EURYDICE_CFIELD(.len =)(slice.len - mid)                    \
    }                                                                 \
  }

// Conversion of slice to an array, rewritten (by Eurydice) to name the
// destination array, since arrays are not values in C.
// N.B.: see note in karamel/lib/Inlining.ml if you change this.
#define Eurydice_slice_to_array2(dst, src, _0, t_arr, _1)                 \
  Eurydice_slice_to_array3(&(dst)->tag, (char *)&(dst)->val.case_Ok, src, \
                           sizeof(t_arr))

static inline void Eurydice_slice_to_array3(uint8_t *dst_tag, char *dst_ok,
                                            Eurydice_slice src, size_t sz) {
  *dst_tag = 0;
  memcpy(dst_ok, src.ptr, sz);
}

// SUPPORT FOR DSTs (Dynamically-Sized Types)

// A DST is a fat pointer that keeps tracks of the size of it flexible array
// member. Slices are a specific case of DSTs, where [T; N] implements
// Unsize<[T]>, meaning an array of statically known size can be converted to a
// fat pointer, i.e. a slice.
//
// Unlike slices, DSTs have a built-in definition that gets monomorphized, of
// the form:
//
// typedef struct {
//   T *ptr;
//   size_t len; // number of elements
// } Eurydice_dst;
//
// Furthermore, T = T0<[U0]> where `struct T0<U: ?Sized>`, where the `U` is the
// last field. This means that there are two monomorphizations of T0 in the
// program. One is `T0<[V; N]>`
// -- this is directly converted to a Eurydice_dst via suitable codegen (no
// macro). The other is `T = T0<[U]>`, where `[U]` gets emitted to
// `Eurydice_derefed_slice`, a type that only appears in that precise situation
// and is thus defined to give rise to a flexible array member.

typedef char Eurydice_derefed_slice[];

#define Eurydice_slice_of_dst(fam_ptr, len_, t, _) \
  ((Eurydice_slice){.ptr = (void *)(fam_ptr), .len = len_})

#define Eurydice_slice_of_boxed_array(ptr_, len_, t, _) \
  ((Eurydice_slice){.ptr = (void *)(ptr_), .len = len_})

// CORE STUFF (conversions, endianness, ...)

// We slap extern "C" on declarations that intend to implement a prototype
// generated by Eurydice, because Eurydice prototypes are always emitted within
// an extern "C" block, UNLESS you use -fcxx17-compat, in which case, you must
// pass -DKRML_CXX17_COMPAT="" to your C++ compiler.
#if defined(__cplusplus) && !defined(KRML_CXX17_COMPAT)
extern "C" {
#endif

static inline void core_num__u32__to_be_bytes(uint32_t src, uint8_t dst[4]) {
  store32_be(dst, src);
}

static inline void core_num__u32__to_le_bytes(uint32_t src, uint8_t dst[4]) {
  store32_le(dst, src);
}

static inline uint32_t core_num__u32__from_le_bytes(uint8_t buf[4]) {
  return load32_le(buf);
}

static inline void core_num__u64__to_le_bytes(uint64_t v, uint8_t buf[8]) {
  store64_le(buf, v);
}

static inline uint64_t core_num__u64__from_le_bytes(uint8_t buf[8]) {
  return load64_le(buf);
}

static inline int64_t core_convert_num___core__convert__From_i32__for_i64__from(
    int32_t x) {
  return x;
}

static inline uint64_t core_convert_num___core__convert__From_u8__for_u64__from(
    uint8_t x) {
  return x;
}

static inline uint64_t
core_convert_num___core__convert__From_u16__for_u64__from(uint16_t x) {
  return x;
}

static inline size_t
core_convert_num___core__convert__From_u16__for_usize__from(uint16_t x) {
  return x;
}

static inline uint32_t core_num__u8__count_ones(uint8_t x0) {
#ifdef _MSC_VER
  return __popcnt(x0);
#else
  return __builtin_popcount(x0);
#endif
}

static inline uint32_t core_num__i32__count_ones(int32_t x0) {
#ifdef _MSC_VER
  return __popcnt(x0);
#else
  return __builtin_popcount(x0);
#endif
}

static inline size_t core_cmp_impls___core__cmp__Ord_for_usize__min(size_t a,
                                                                    size_t b) {
  if (a <= b)
    return a;
  else
    return b;
}

// unsigned overflow wraparound semantics in C
static inline uint16_t core_num__u16__wrapping_add(uint16_t x, uint16_t y) {
  return x + y;
}
static inline uint8_t core_num__u8__wrapping_sub(uint8_t x, uint8_t y) {
  return x - y;
}
static inline uint64_t core_num__u64__rotate_left(uint64_t x0, uint32_t x1) {
  return (x0 << x1 | x0 >> (64 - x1));
}

static inline void core_ops_arith__i32__add_assign(int32_t *x0, int32_t *x1) {
  *x0 = *x0 + *x1;
}

static inline uint8_t Eurydice_bitand_pv_u8(uint8_t *p, uint8_t v) {
  return (*p) & v;
}
static inline uint8_t Eurydice_shr_pv_u8(uint8_t *p, int32_t v) {
  return (*p) >> v;
}
static inline uint32_t Eurydice_min_u32(uint32_t x, uint32_t y) {
  return x < y ? x : y;
}

static inline uint8_t
core_ops_bit___core__ops__bit__BitAnd_u8__u8__for___a__u8___46__bitand(
    uint8_t *x0, uint8_t x1) {
  return Eurydice_bitand_pv_u8(x0, x1);
}

static inline uint8_t
core_ops_bit___core__ops__bit__Shr_i32__u8__for___a__u8___792__shr(uint8_t *x0,
                                                                   int32_t x1) {
  return Eurydice_shr_pv_u8(x0, x1);
}

#define core_num_nonzero_private_NonZeroUsizeInner size_t
static inline core_num_nonzero_private_NonZeroUsizeInner
core_num_nonzero_private___core__clone__Clone_for_core__num__nonzero__private__NonZeroUsizeInner__26__clone(
    core_num_nonzero_private_NonZeroUsizeInner *x0) {
  return *x0;
}

#if defined(__cplusplus) && !defined(KRML_CXX17_COMPAT)
}
#endif

// ITERATORS

#define Eurydice_range_iter_next(iter_ptr, t, ret_t)      \
  (((iter_ptr)->start >= (iter_ptr)->end)                 \
       ? (KRML_CLITERAL(ret_t){EURYDICE_CFIELD(.tag =) 0, \
                               EURYDICE_CFIELD(.f0 =) 0}) \
       : (KRML_CLITERAL(ret_t){EURYDICE_CFIELD(.tag =) 1, \
                               EURYDICE_CFIELD(.f0 =)(iter_ptr)->start++}))

#define core_iter_range___core__iter__traits__iterator__Iterator_A__for_core__ops__range__Range_A__TraitClause_0___6__next \
  Eurydice_range_iter_next

// See note in karamel/lib/Inlining.ml if you change this
#define Eurydice_into_iter(x, t, _ret_t, _) (x)
#define core_iter_traits_collect___core__iter__traits__collect__IntoIterator_Clause1_Item__I__for_I__1__into_iter \
  Eurydice_into_iter

typedef struct {
  Eurydice_slice slice;
  size_t chunk_size;
} Eurydice_chunks;

// Can't use macros Eurydice_slice_subslice_{to,from} because they require a
// type, and this static inline function cannot receive a type as an argument.
// Instead, we receive the element size and use it to peform manual offset
// computations rather than going through the macros.
static inline Eurydice_slice chunk_next(Eurydice_chunks *chunks,
                                        size_t element_size) {
  size_t chunk_size = chunks->slice.len >= chunks->chunk_size
                          ? chunks->chunk_size
                          : chunks->slice.len;
  Eurydice_slice curr_chunk;
  curr_chunk.ptr = chunks->slice.ptr;
  curr_chunk.len = chunk_size;
  chunks->slice.ptr = (char *)(chunks->slice.ptr) + chunk_size * element_size;
  chunks->slice.len = chunks->slice.len - chunk_size;
  return curr_chunk;
}

#define core_slice___Slice_T___chunks(slice_, sz_, t, _ret_t) \
  ((Eurydice_chunks){.slice = slice_, .chunk_size = sz_})
#define core_slice___Slice_T___chunks_exact(slice_, sz_, t, _ret_t)         \
  ((Eurydice_chunks){                                                       \
      .slice = {.ptr = slice_.ptr, .len = slice_.len - (slice_.len % sz_)}, \
      .chunk_size = sz_})
#define core_slice_iter_Chunks Eurydice_chunks
#define core_slice_iter_ChunksExact Eurydice_chunks
#define Eurydice_chunks_next(iter, t, ret_t)                     \
  (((iter)->slice.len == 0) ? ((ret_t){.tag = core_option_None}) \
                            : ((ret_t){.tag = core_option_Some,  \
                                       .f0 = chunk_next(iter, sizeof(t))}))
#define core_slice_iter___core__iter__traits__iterator__Iterator_for_core__slice__iter__Chunks__a__T___70__next \
  Eurydice_chunks_next
// This name changed on 20240627
#define core_slice_iter___core__iter__traits__iterator__Iterator_for_core__slice__iter__Chunks__a__T___71__next \
  Eurydice_chunks_next
#define core_slice_iter__core__slice__iter__ChunksExact__a__T__89__next( \
    iter, t, _ret_t)                                                     \
  core_slice_iter__core__slice__iter__Chunks__a__T__70__next(iter, t)

typedef struct {
  Eurydice_slice s;
  size_t index;
} Eurydice_slice_iterator;

#define core_slice___Slice_T___iter(x, t, _ret_t) \
  ((Eurydice_slice_iterator){.s = x, .index = 0})
#define core_slice_iter_Iter Eurydice_slice_iterator
#define core_slice_iter__core__slice__iter__Iter__a__T__181__next(iter, t, \
                                                                  ret_t)   \
  (((iter)->index == (iter)->s.len)                                        \
       ? (KRML_CLITERAL(ret_t){.tag = core_option_None})                   \
       : (KRML_CLITERAL(ret_t){                                            \
             .tag = core_option_Some,                                      \
             .f0 = ((iter)->index++,                                       \
                    &((t *)((iter)->s.ptr))[(iter)->index - 1])}))
#define core_option__core__option__Option_T__TraitClause_0___is_some(X, _0, \
                                                                     _1)    \
  ((X)->tag == 1)
// STRINGS

typedef const char *Prims_string;

// MISC (UNTESTED)

typedef void *core_fmt_Formatter;
typedef void *core_fmt_Arguments;
typedef void *core_fmt_rt_Argument;
#define core_fmt_rt__core__fmt__rt__Argument__a__1__new_display(x1, x2, x3, \
                                                                x4)         \
  NULL

// BOXES

// Crimes.
static inline char *malloc_and_init(size_t sz, char *init) {
  char *ptr = (char *)malloc(sz);
  memcpy(ptr, init, sz);
  return ptr;
}

#define Eurydice_box_new(init, t, t_dst) \
  ((t_dst)(malloc_and_init(sizeof(t), (char *)(&init))))

#define Eurydice_box_new_array(len, ptr, t, t_dst) \
  ((t_dst)(malloc_and_init(len * sizeof(t), (char *)(ptr))))

/* from libcrux/libcrux-ml-kem/extracts/c_header_only/generated/libcrux_mlkem_core.h */
/*
 * SPDX-FileCopyrightText: 2025 Cryspen Sarl <info@cryspen.com>
 *
 * SPDX-License-Identifier: MIT or Apache-2.0
 *
 * This code was generated with the following revisions:
 * Charon: 667d2fc98984ff7f3df989c2367e6c1fa4a000e7
 * Eurydice: 2381cbc416ef2ad0b561c362c500bc84f36b6785
 * Karamel: 80f5435f2fc505973c469a4afcc8d875cddd0d8b
 * F*: 71d8221589d4d438af3706d89cb653cf53e18aab
 * Libcrux: 68dfed5a4a9e40277f62828471c029afed1ecdcc
 */

#ifndef libcrux_mlkem_core_H
#define libcrux_mlkem_core_H


#if defined(__cplusplus)
extern "C" {
#endif

/**
A monomorphic instance of core.ops.range.Range
with types size_t

*/
typedef struct core_ops_range_Range_08_s {
  size_t start;
  size_t end;
} core_ops_range_Range_08;

static inline uint16_t core_num__u16__wrapping_add(uint16_t x0, uint16_t x1);

static inline uint64_t core_num__u64__from_le_bytes(uint8_t x0[8U]);

static inline uint64_t core_num__u64__rotate_left(uint64_t x0, uint32_t x1);

static inline void core_num__u64__to_le_bytes(uint64_t x0, uint8_t x1[8U]);

static inline uint32_t core_num__u8__count_ones(uint8_t x0);

static inline uint8_t core_num__u8__wrapping_sub(uint8_t x0, uint8_t x1);

#define LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE ((size_t)32U)

#define LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_COEFFICIENT ((size_t)12U)

#define LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT ((size_t)256U)

#define LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_RING_ELEMENT \
  (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT * (size_t)12U)

#define LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT \
  (LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_RING_ELEMENT / (size_t)8U)

#define LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE ((size_t)32U)

#define LIBCRUX_ML_KEM_CONSTANTS_G_DIGEST_SIZE ((size_t)64U)

#define LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE ((size_t)32U)

/**
 K * BITS_PER_RING_ELEMENT / 8

 [eurydice] Note that we can't use const generics here because that breaks
            C extraction with eurydice.
*/
static inline size_t libcrux_ml_kem_constants_ranked_bytes_per_ring_element(
    size_t rank) {
  return rank * LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_RING_ELEMENT / (size_t)8U;
}

/**
This function found in impl {libcrux_secrets::traits::Classify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.classify_27
with types uint8_t

*/
static KRML_MUSTINLINE uint8_t
libcrux_secrets_int_public_integers_classify_27_90(uint8_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types int16_t

*/
static KRML_MUSTINLINE int16_t
libcrux_secrets_int_public_integers_declassify_d8_39(int16_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for i16}
*/
static KRML_MUSTINLINE uint8_t libcrux_secrets_int_as_u8_f5(int16_t self) {
  return libcrux_secrets_int_public_integers_classify_27_90(
      (uint8_t)libcrux_secrets_int_public_integers_declassify_d8_39(self));
}

/**
This function found in impl {libcrux_secrets::traits::Classify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.classify_27
with types int16_t

*/
static KRML_MUSTINLINE int16_t
libcrux_secrets_int_public_integers_classify_27_39(int16_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types uint8_t

*/
static KRML_MUSTINLINE uint8_t
libcrux_secrets_int_public_integers_declassify_d8_90(uint8_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for u8}
*/
static KRML_MUSTINLINE int16_t libcrux_secrets_int_as_i16_59(uint8_t self) {
  return libcrux_secrets_int_public_integers_classify_27_39(
      (int16_t)libcrux_secrets_int_public_integers_declassify_d8_90(self));
}

/**
This function found in impl {libcrux_secrets::traits::Classify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.classify_27
with types int32_t

*/
static KRML_MUSTINLINE int32_t
libcrux_secrets_int_public_integers_classify_27_a8(int32_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for i16}
*/
static KRML_MUSTINLINE int32_t libcrux_secrets_int_as_i32_f5(int16_t self) {
  return libcrux_secrets_int_public_integers_classify_27_a8(
      (int32_t)libcrux_secrets_int_public_integers_declassify_d8_39(self));
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types int32_t

*/
static KRML_MUSTINLINE int32_t
libcrux_secrets_int_public_integers_declassify_d8_a8(int32_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for i32}
*/
static KRML_MUSTINLINE int16_t libcrux_secrets_int_as_i16_36(int32_t self) {
  return libcrux_secrets_int_public_integers_classify_27_39(
      (int16_t)libcrux_secrets_int_public_integers_declassify_d8_a8(self));
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types uint32_t

*/
static KRML_MUSTINLINE uint32_t
libcrux_secrets_int_public_integers_declassify_d8_df(uint32_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for u32}
*/
static KRML_MUSTINLINE int32_t libcrux_secrets_int_as_i32_b8(uint32_t self) {
  return libcrux_secrets_int_public_integers_classify_27_a8(
      (int32_t)libcrux_secrets_int_public_integers_declassify_d8_df(self));
}

/**
This function found in impl {libcrux_secrets::traits::Classify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.classify_27
with types uint16_t

*/
static KRML_MUSTINLINE uint16_t
libcrux_secrets_int_public_integers_classify_27_de(uint16_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for i16}
*/
static KRML_MUSTINLINE uint16_t libcrux_secrets_int_as_u16_f5(int16_t self) {
  return libcrux_secrets_int_public_integers_classify_27_de(
      (uint16_t)libcrux_secrets_int_public_integers_declassify_d8_39(self));
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types uint16_t

*/
static KRML_MUSTINLINE uint16_t
libcrux_secrets_int_public_integers_declassify_d8_de(uint16_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for u16}
*/
static KRML_MUSTINLINE int16_t libcrux_secrets_int_as_i16_ca(uint16_t self) {
  return libcrux_secrets_int_public_integers_classify_27_39(
      (int16_t)libcrux_secrets_int_public_integers_declassify_d8_de(self));
}

/**
This function found in impl {libcrux_secrets::traits::Classify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.classify_27
with types uint64_t

*/
static KRML_MUSTINLINE uint64_t
libcrux_secrets_int_public_integers_classify_27_49(uint64_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for u16}
*/
static KRML_MUSTINLINE uint64_t libcrux_secrets_int_as_u64_ca(uint16_t self) {
  return libcrux_secrets_int_public_integers_classify_27_49(
      (uint64_t)libcrux_secrets_int_public_integers_declassify_d8_de(self));
}

/**
This function found in impl {libcrux_secrets::traits::Classify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.classify_27
with types uint32_t

*/
static KRML_MUSTINLINE uint32_t
libcrux_secrets_int_public_integers_classify_27_df(uint32_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types uint64_t

*/
static KRML_MUSTINLINE uint64_t
libcrux_secrets_int_public_integers_declassify_d8_49(uint64_t self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::int::CastOps for u64}
*/
static KRML_MUSTINLINE uint32_t libcrux_secrets_int_as_u32_a3(uint64_t self) {
  return libcrux_secrets_int_public_integers_classify_27_df(
      (uint32_t)libcrux_secrets_int_public_integers_declassify_d8_49(self));
}

/**
This function found in impl {libcrux_secrets::int::CastOps for u32}
*/
static KRML_MUSTINLINE int16_t libcrux_secrets_int_as_i16_b8(uint32_t self) {
  return libcrux_secrets_int_public_integers_classify_27_39(
      (int16_t)libcrux_secrets_int_public_integers_declassify_d8_df(self));
}

/**
This function found in impl {libcrux_secrets::int::CastOps for i16}
*/
static KRML_MUSTINLINE int16_t libcrux_secrets_int_as_i16_f5(int16_t self) {
  return libcrux_secrets_int_public_integers_classify_27_39(
      libcrux_secrets_int_public_integers_declassify_d8_39(self));
}

typedef struct libcrux_ml_kem_utils_extraction_helper_Keypair768_s {
  uint8_t fst[1152U];
  uint8_t snd[1184U];
} libcrux_ml_kem_utils_extraction_helper_Keypair768;

#define Ok 0
#define Err 1

typedef uint8_t Result_b2_tags;

/**
A monomorphic instance of core.result.Result
with types uint8_t[24size_t], core_array_TryFromSliceError

*/
typedef struct Result_b2_s {
  Result_b2_tags tag;
  union {
    uint8_t case_Ok[24U];
    TryFromSliceError case_Err;
  } val;
} Result_b2;

/**
This function found in impl {core::result::Result<T, E>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of core.result.unwrap_26
with types uint8_t[24size_t], core_array_TryFromSliceError

*/
static inline void unwrap_26_70(Result_b2 self, uint8_t ret[24U]) {
  if (self.tag == Ok) {
    uint8_t f0[24U];
    memcpy(f0, self.val.case_Ok, (size_t)24U * sizeof(uint8_t));
    memcpy(ret, f0, (size_t)24U * sizeof(uint8_t));
  } else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unwrap not Ok");
    KRML_HOST_EXIT(255U);
  }
}

/**
A monomorphic instance of core.result.Result
with types uint8_t[20size_t], core_array_TryFromSliceError

*/
typedef struct Result_e1_s {
  Result_b2_tags tag;
  union {
    uint8_t case_Ok[20U];
    TryFromSliceError case_Err;
  } val;
} Result_e1;

/**
This function found in impl {core::result::Result<T, E>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of core.result.unwrap_26
with types uint8_t[20size_t], core_array_TryFromSliceError

*/
static inline void unwrap_26_20(Result_e1 self, uint8_t ret[20U]) {
  if (self.tag == Ok) {
    uint8_t f0[20U];
    memcpy(f0, self.val.case_Ok, (size_t)20U * sizeof(uint8_t));
    memcpy(ret, f0, (size_t)20U * sizeof(uint8_t));
  } else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unwrap not Ok");
    KRML_HOST_EXIT(255U);
  }
}

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 32
*/
static KRML_MUSTINLINE void libcrux_ml_kem_utils_into_padded_array_9e(
    Eurydice_slice slice, uint8_t ret[32U]) {
  uint8_t out[32U] = {0U};
  uint8_t *uu____0 = out;
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          uu____0, (size_t)0U, Eurydice_slice_len(slice, uint8_t), uint8_t *),
      slice, uint8_t);
  memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.types.MlKemPrivateKey
with const generics
- $2400size_t
*/
typedef struct libcrux_ml_kem_types_MlKemPrivateKey_d9_s {
  uint8_t value[2400U];
} libcrux_ml_kem_types_MlKemPrivateKey_d9;

/**
This function found in impl {core::default::Default for
libcrux_ml_kem::types::MlKemPrivateKey<SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.default_d3
with const generics
- SIZE= 2400
*/
static inline libcrux_ml_kem_types_MlKemPrivateKey_d9
libcrux_ml_kem_types_default_d3_28(void) {
  return (
      KRML_CLITERAL(libcrux_ml_kem_types_MlKemPrivateKey_d9){.value = {0U}});
}

/**
A monomorphic instance of libcrux_ml_kem.types.MlKemPublicKey
with const generics
- $1184size_t
*/
typedef struct libcrux_ml_kem_types_MlKemPublicKey_30_s {
  uint8_t value[1184U];
} libcrux_ml_kem_types_MlKemPublicKey_30;

/**
This function found in impl {core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemPublicKey<SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_fd
with const generics
- SIZE= 1184
*/
static inline libcrux_ml_kem_types_MlKemPublicKey_30
libcrux_ml_kem_types_from_fd_d0(uint8_t value[1184U]) {
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_value[1184U];
  memcpy(copy_of_value, value, (size_t)1184U * sizeof(uint8_t));
  libcrux_ml_kem_types_MlKemPublicKey_30 lit;
  memcpy(lit.value, copy_of_value, (size_t)1184U * sizeof(uint8_t));
  return lit;
}

typedef struct libcrux_ml_kem_mlkem768_MlKem768KeyPair_s {
  libcrux_ml_kem_types_MlKemPrivateKey_d9 sk;
  libcrux_ml_kem_types_MlKemPublicKey_30 pk;
} libcrux_ml_kem_mlkem768_MlKem768KeyPair;

/**
This function found in impl
{libcrux_ml_kem::types::MlKemKeyPair<PRIVATE_KEY_SIZE, PUBLIC_KEY_SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_17
with const generics
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
*/
static inline libcrux_ml_kem_mlkem768_MlKem768KeyPair
libcrux_ml_kem_types_from_17_74(libcrux_ml_kem_types_MlKemPrivateKey_d9 sk,
                                libcrux_ml_kem_types_MlKemPublicKey_30 pk) {
  return (KRML_CLITERAL(libcrux_ml_kem_mlkem768_MlKem768KeyPair){.sk = sk,
                                                                 .pk = pk});
}

/**
This function found in impl {core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemPrivateKey<SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_77
with const generics
- SIZE= 2400
*/
static inline libcrux_ml_kem_types_MlKemPrivateKey_d9
libcrux_ml_kem_types_from_77_28(uint8_t value[2400U]) {
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_value[2400U];
  memcpy(copy_of_value, value, (size_t)2400U * sizeof(uint8_t));
  libcrux_ml_kem_types_MlKemPrivateKey_d9 lit;
  memcpy(lit.value, copy_of_value, (size_t)2400U * sizeof(uint8_t));
  return lit;
}

/**
A monomorphic instance of core.result.Result
with types uint8_t[32size_t], core_array_TryFromSliceError

*/
typedef struct Result_fb_s {
  Result_b2_tags tag;
  union {
    uint8_t case_Ok[32U];
    TryFromSliceError case_Err;
  } val;
} Result_fb;

/**
This function found in impl {core::result::Result<T, E>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of core.result.unwrap_26
with types uint8_t[32size_t], core_array_TryFromSliceError

*/
static inline void unwrap_26_b3(Result_fb self, uint8_t ret[32U]) {
  if (self.tag == Ok) {
    uint8_t f0[32U];
    memcpy(f0, self.val.case_Ok, (size_t)32U * sizeof(uint8_t));
    memcpy(ret, f0, (size_t)32U * sizeof(uint8_t));
  } else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unwrap not Ok");
    KRML_HOST_EXIT(255U);
  }
}

typedef struct libcrux_ml_kem_mlkem768_MlKem768Ciphertext_s {
  uint8_t value[1088U];
} libcrux_ml_kem_mlkem768_MlKem768Ciphertext;

/**
A monomorphic instance of K.
with types libcrux_ml_kem_types_MlKemCiphertext[[$1088size_t]],
uint8_t[32size_t]

*/
typedef struct tuple_c2_s {
  libcrux_ml_kem_mlkem768_MlKem768Ciphertext fst;
  uint8_t snd[32U];
} tuple_c2;

/**
This function found in impl {core::convert::From<@Array<u8, SIZE>> for
libcrux_ml_kem::types::MlKemCiphertext<SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.from_e0
with const generics
- SIZE= 1088
*/
static inline libcrux_ml_kem_mlkem768_MlKem768Ciphertext
libcrux_ml_kem_types_from_e0_80(uint8_t value[1088U]) {
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_value[1088U];
  memcpy(copy_of_value, value, (size_t)1088U * sizeof(uint8_t));
  libcrux_ml_kem_mlkem768_MlKem768Ciphertext lit;
  memcpy(lit.value, copy_of_value, (size_t)1088U * sizeof(uint8_t));
  return lit;
}

/**
This function found in impl {libcrux_ml_kem::types::MlKemPublicKey<SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_slice_e6
with const generics
- SIZE= 1184
*/
static inline uint8_t *libcrux_ml_kem_types_as_slice_e6_d0(
    libcrux_ml_kem_types_MlKemPublicKey_30 *self) {
  return self->value;
}

/**
This function found in impl {libcrux_ml_kem::types::MlKemCiphertext<SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_slice_a9
with const generics
- SIZE= 1088
*/
static inline uint8_t *libcrux_ml_kem_types_as_slice_a9_80(
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *self) {
  return self->value;
}

/**
A monomorphic instance of libcrux_ml_kem.utils.prf_input_inc
with const generics
- K= 3
*/
static KRML_MUSTINLINE uint8_t libcrux_ml_kem_utils_prf_input_inc_e0(
    uint8_t (*prf_inputs)[33U], uint8_t domain_separator) {
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    prf_inputs[i0][32U] = domain_separator;
    domain_separator = (uint32_t)domain_separator + 1U;
  }
  return domain_separator;
}

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 33
*/
static KRML_MUSTINLINE void libcrux_ml_kem_utils_into_padded_array_c8(
    Eurydice_slice slice, uint8_t ret[33U]) {
  uint8_t out[33U] = {0U};
  uint8_t *uu____0 = out;
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          uu____0, (size_t)0U, Eurydice_slice_len(slice, uint8_t), uint8_t *),
      slice, uint8_t);
  memcpy(ret, out, (size_t)33U * sizeof(uint8_t));
}

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 34
*/
static KRML_MUSTINLINE void libcrux_ml_kem_utils_into_padded_array_b6(
    Eurydice_slice slice, uint8_t ret[34U]) {
  uint8_t out[34U] = {0U};
  uint8_t *uu____0 = out;
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          uu____0, (size_t)0U, Eurydice_slice_len(slice, uint8_t), uint8_t *),
      slice, uint8_t);
  memcpy(ret, out, (size_t)34U * sizeof(uint8_t));
}

/**
This function found in impl {core::convert::AsRef<@Slice<u8>> for
libcrux_ml_kem::types::MlKemCiphertext<SIZE>}
*/
/**
A monomorphic instance of libcrux_ml_kem.types.as_ref_d3
with const generics
- SIZE= 1088
*/
static inline Eurydice_slice libcrux_ml_kem_types_as_ref_d3_80(
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *self) {
  return Eurydice_array_to_slice((size_t)1088U, self->value, uint8_t);
}

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 1120
*/
static KRML_MUSTINLINE void libcrux_ml_kem_utils_into_padded_array_15(
    Eurydice_slice slice, uint8_t ret[1120U]) {
  uint8_t out[1120U] = {0U};
  uint8_t *uu____0 = out;
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          uu____0, (size_t)0U, Eurydice_slice_len(slice, uint8_t), uint8_t *),
      slice, uint8_t);
  memcpy(ret, out, (size_t)1120U * sizeof(uint8_t));
}

/**
 Pad the `slice` with `0`s at the end.
*/
/**
A monomorphic instance of libcrux_ml_kem.utils.into_padded_array
with const generics
- LEN= 64
*/
static KRML_MUSTINLINE void libcrux_ml_kem_utils_into_padded_array_24(
    Eurydice_slice slice, uint8_t ret[64U]) {
  uint8_t out[64U] = {0U};
  uint8_t *uu____0 = out;
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          uu____0, (size_t)0U, Eurydice_slice_len(slice, uint8_t), uint8_t *),
      slice, uint8_t);
  memcpy(ret, out, (size_t)64U * sizeof(uint8_t));
}

typedef struct Eurydice_slice_uint8_t_x4_s {
  Eurydice_slice fst;
  Eurydice_slice snd;
  Eurydice_slice thd;
  Eurydice_slice f3;
} Eurydice_slice_uint8_t_x4;

typedef struct Eurydice_slice_uint8_t_x2_s {
  Eurydice_slice fst;
  Eurydice_slice snd;
} Eurydice_slice_uint8_t_x2;

/**
 Unpack an incoming private key into it's different parts.

 We have this here in types to extract into a common core for C.
*/
/**
A monomorphic instance of libcrux_ml_kem.types.unpack_private_key
with const generics
- CPA_SECRET_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
*/
static inline Eurydice_slice_uint8_t_x4
libcrux_ml_kem_types_unpack_private_key_b4(Eurydice_slice private_key) {
  Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
      private_key, (size_t)1152U, uint8_t, Eurydice_slice_uint8_t_x2);
  Eurydice_slice ind_cpa_secret_key = uu____0.fst;
  Eurydice_slice secret_key0 = uu____0.snd;
  Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
      secret_key0, (size_t)1184U, uint8_t, Eurydice_slice_uint8_t_x2);
  Eurydice_slice ind_cpa_public_key = uu____1.fst;
  Eurydice_slice secret_key = uu____1.snd;
  Eurydice_slice_uint8_t_x2 uu____2 = Eurydice_slice_split_at(
      secret_key, LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t,
      Eurydice_slice_uint8_t_x2);
  Eurydice_slice ind_cpa_public_key_hash = uu____2.fst;
  Eurydice_slice implicit_rejection_value = uu____2.snd;
  return (
      KRML_CLITERAL(Eurydice_slice_uint8_t_x4){.fst = ind_cpa_secret_key,
                                               .snd = ind_cpa_public_key,
                                               .thd = ind_cpa_public_key_hash,
                                               .f3 = implicit_rejection_value});
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types uint8_t[24size_t]

*/
static KRML_MUSTINLINE void
libcrux_secrets_int_public_integers_declassify_d8_d2(uint8_t self[24U],
                                                     uint8_t ret[24U]) {
  memcpy(ret, self, (size_t)24U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types uint8_t[20size_t]

*/
static KRML_MUSTINLINE void
libcrux_secrets_int_public_integers_declassify_d8_57(uint8_t self[20U],
                                                     uint8_t ret[20U]) {
  memcpy(ret, self, (size_t)20U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types uint8_t[8size_t]

*/
static KRML_MUSTINLINE void
libcrux_secrets_int_public_integers_declassify_d8_76(uint8_t self[8U],
                                                     uint8_t ret[8U]) {
  memcpy(ret, self, (size_t)8U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_secrets::traits::Declassify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.declassify_d8
with types uint8_t[2size_t]

*/
static KRML_MUSTINLINE void
libcrux_secrets_int_public_integers_declassify_d8_d4(uint8_t self[2U],
                                                     uint8_t ret[2U]) {
  memcpy(ret, self, (size_t)2U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_secrets::traits::Classify<T> for T}
*/
/**
A monomorphic instance of libcrux_secrets.int.public_integers.classify_27
with types int16_t[16size_t]

*/
static KRML_MUSTINLINE void libcrux_secrets_int_public_integers_classify_27_46(
    int16_t self[16U], int16_t ret[16U]) {
  memcpy(ret, self, (size_t)16U * sizeof(int16_t));
}

/**
This function found in impl {libcrux_secrets::traits::ClassifyRef<&'a
(@Slice<T>)> for &'a (@Slice<T>)}
*/
/**
A monomorphic instance of libcrux_secrets.int.classify_public.classify_ref_9b
with types uint8_t

*/
static KRML_MUSTINLINE Eurydice_slice
libcrux_secrets_int_classify_public_classify_ref_9b_90(Eurydice_slice self) {
  return self;
}

/**
This function found in impl {libcrux_secrets::traits::ClassifyRef<&'a
(@Slice<T>)> for &'a (@Slice<T>)}
*/
/**
A monomorphic instance of libcrux_secrets.int.classify_public.classify_ref_9b
with types int16_t

*/
static KRML_MUSTINLINE Eurydice_slice
libcrux_secrets_int_classify_public_classify_ref_9b_39(Eurydice_slice self) {
  return self;
}

/**
A monomorphic instance of core.result.Result
with types int16_t[16size_t], core_array_TryFromSliceError

*/
typedef struct Result_0a_s {
  Result_b2_tags tag;
  union {
    int16_t case_Ok[16U];
    TryFromSliceError case_Err;
  } val;
} Result_0a;

/**
This function found in impl {core::result::Result<T, E>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of core.result.unwrap_26
with types int16_t[16size_t], core_array_TryFromSliceError

*/
static inline void unwrap_26_00(Result_0a self, int16_t ret[16U]) {
  if (self.tag == Ok) {
    int16_t f0[16U];
    memcpy(f0, self.val.case_Ok, (size_t)16U * sizeof(int16_t));
    memcpy(ret, f0, (size_t)16U * sizeof(int16_t));
  } else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unwrap not Ok");
    KRML_HOST_EXIT(255U);
  }
}

/**
A monomorphic instance of core.result.Result
with types uint8_t[8size_t], core_array_TryFromSliceError

*/
typedef struct Result_15_s {
  Result_b2_tags tag;
  union {
    uint8_t case_Ok[8U];
    TryFromSliceError case_Err;
  } val;
} Result_15;

/**
This function found in impl {core::result::Result<T, E>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of core.result.unwrap_26
with types uint8_t[8size_t], core_array_TryFromSliceError

*/
static inline void unwrap_26_68(Result_15 self, uint8_t ret[8U]) {
  if (self.tag == Ok) {
    uint8_t f0[8U];
    memcpy(f0, self.val.case_Ok, (size_t)8U * sizeof(uint8_t));
    memcpy(ret, f0, (size_t)8U * sizeof(uint8_t));
  } else {
    KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                      "unwrap not Ok");
    KRML_HOST_EXIT(255U);
  }
}

#if defined(__cplusplus)
}
#endif

#define libcrux_mlkem_core_H_DEFINED
#endif /* libcrux_mlkem_core_H */

/* from libcrux/libcrux-ml-kem/extracts/c_header_only/generated/libcrux_ct_ops.h */
/*
 * SPDX-FileCopyrightText: 2025 Cryspen Sarl <info@cryspen.com>
 *
 * SPDX-License-Identifier: MIT or Apache-2.0
 *
 * This code was generated with the following revisions:
 * Charon: 667d2fc98984ff7f3df989c2367e6c1fa4a000e7
 * Eurydice: 2381cbc416ef2ad0b561c362c500bc84f36b6785
 * Karamel: 80f5435f2fc505973c469a4afcc8d875cddd0d8b
 * F*: 71d8221589d4d438af3706d89cb653cf53e18aab
 * Libcrux: 68dfed5a4a9e40277f62828471c029afed1ecdcc
 */

#ifndef libcrux_ct_ops_H
#define libcrux_ct_ops_H


#if defined(__cplusplus)
extern "C" {
#endif


/**
 Return 1 if `value` is not zero and 0 otherwise.
*/
static KRML_NOINLINE uint8_t
libcrux_ml_kem_constant_time_ops_inz(uint8_t value) {
  uint16_t value0 = (uint16_t)value;
  uint8_t result =
      (uint8_t)((uint32_t)core_num__u16__wrapping_add(~value0, 1U) >> 8U);
  return (uint32_t)result & 1U;
}

static KRML_NOINLINE uint8_t
libcrux_ml_kem_constant_time_ops_is_non_zero(uint8_t value) {
  return libcrux_ml_kem_constant_time_ops_inz(value);
}

/**
 Return 1 if the bytes of `lhs` and `rhs` do not exactly
 match and 0 otherwise.
*/
static KRML_NOINLINE uint8_t libcrux_ml_kem_constant_time_ops_compare(
    Eurydice_slice lhs, Eurydice_slice rhs) {
  uint8_t r = 0U;
  for (size_t i = (size_t)0U; i < Eurydice_slice_len(lhs, uint8_t); i++) {
    size_t i0 = i;
    uint8_t nr = (uint32_t)r |
                 ((uint32_t)Eurydice_slice_index(lhs, i0, uint8_t, uint8_t *) ^
                  (uint32_t)Eurydice_slice_index(rhs, i0, uint8_t, uint8_t *));
    r = nr;
  }
  return libcrux_ml_kem_constant_time_ops_is_non_zero(r);
}

static KRML_NOINLINE uint8_t
libcrux_ml_kem_constant_time_ops_compare_ciphertexts_in_constant_time(
    Eurydice_slice lhs, Eurydice_slice rhs) {
  return libcrux_ml_kem_constant_time_ops_compare(lhs, rhs);
}

/**
 If `selector` is not zero, return the bytes in `rhs`; return the bytes in
 `lhs` otherwise.
*/
static KRML_NOINLINE void libcrux_ml_kem_constant_time_ops_select_ct(
    Eurydice_slice lhs, Eurydice_slice rhs, uint8_t selector,
    uint8_t ret[32U]) {
  uint8_t mask = core_num__u8__wrapping_sub(
      libcrux_ml_kem_constant_time_ops_is_non_zero(selector), 1U);
  uint8_t out[32U] = {0U};
  for (size_t i = (size_t)0U; i < LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE;
       i++) {
    size_t i0 = i;
    uint8_t outi =
        ((uint32_t)Eurydice_slice_index(lhs, i0, uint8_t, uint8_t *) &
         (uint32_t)mask) |
        ((uint32_t)Eurydice_slice_index(rhs, i0, uint8_t, uint8_t *) &
         (uint32_t)~mask);
    out[i0] = outi;
  }
  memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

static KRML_NOINLINE void
libcrux_ml_kem_constant_time_ops_select_shared_secret_in_constant_time(
    Eurydice_slice lhs, Eurydice_slice rhs, uint8_t selector,
    uint8_t ret[32U]) {
  libcrux_ml_kem_constant_time_ops_select_ct(lhs, rhs, selector, ret);
}

static KRML_NOINLINE void
libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time(
    Eurydice_slice lhs_c, Eurydice_slice rhs_c, Eurydice_slice lhs_s,
    Eurydice_slice rhs_s, uint8_t ret[32U]) {
  uint8_t selector =
      libcrux_ml_kem_constant_time_ops_compare_ciphertexts_in_constant_time(
          lhs_c, rhs_c);
  uint8_t ret0[32U];
  libcrux_ml_kem_constant_time_ops_select_shared_secret_in_constant_time(
      lhs_s, rhs_s, selector, ret0);
  memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

#if defined(__cplusplus)
}
#endif

#define libcrux_ct_ops_H_DEFINED
#endif /* libcrux_ct_ops_H */

/* from libcrux/libcrux-ml-kem/extracts/c_header_only/generated/libcrux_sha3_portable.h */
/*
 * SPDX-FileCopyrightText: 2025 Cryspen Sarl <info@cryspen.com>
 *
 * SPDX-License-Identifier: MIT or Apache-2.0
 *
 * This code was generated with the following revisions:
 * Charon: 667d2fc98984ff7f3df989c2367e6c1fa4a000e7
 * Eurydice: 2381cbc416ef2ad0b561c362c500bc84f36b6785
 * Karamel: 80f5435f2fc505973c469a4afcc8d875cddd0d8b
 * F*: 71d8221589d4d438af3706d89cb653cf53e18aab
 * Libcrux: 68dfed5a4a9e40277f62828471c029afed1ecdcc
 */

#ifndef libcrux_sha3_portable_H
#define libcrux_sha3_portable_H


#if defined(__cplusplus)
extern "C" {
#endif


/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
static KRML_MUSTINLINE uint64_t libcrux_sha3_simd_portable_zero_d2(void) {
  return 0ULL;
}

static KRML_MUSTINLINE uint64_t libcrux_sha3_simd_portable__veor5q_u64(
    uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
  return (((a ^ b) ^ c) ^ d) ^ e;
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
static KRML_MUSTINLINE uint64_t libcrux_sha3_simd_portable_xor5_d2(
    uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
  return libcrux_sha3_simd_portable__veor5q_u64(a, b, c, d, e);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 1
- RIGHT= 63
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_76(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)1);
}

static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vrax1q_u64(uint64_t a, uint64_t b) {
  uint64_t uu____0 = a;
  return uu____0 ^ libcrux_sha3_simd_portable_rotate_left_76(b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left1_and_xor_d2(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vrax1q_u64(a, b);
}

static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vbcaxq_u64(uint64_t a, uint64_t b, uint64_t c) {
  return a ^ (b & ~c);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_and_not_xor_d2(uint64_t a, uint64_t b, uint64_t c) {
  return libcrux_sha3_simd_portable__vbcaxq_u64(a, b, c);
}

static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__veorq_n_u64(uint64_t a, uint64_t c) {
  return a ^ c;
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_constant_d2(uint64_t a, uint64_t c) {
  return libcrux_sha3_simd_portable__veorq_n_u64(a, c);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
static KRML_MUSTINLINE uint64_t libcrux_sha3_simd_portable_xor_d2(uint64_t a,
                                                                  uint64_t b) {
  return a ^ b;
}

static const uint64_t
    libcrux_sha3_generic_keccak_constants_ROUNDCONSTANTS[24U] = {
        1ULL,
        32898ULL,
        9223372036854808714ULL,
        9223372039002292224ULL,
        32907ULL,
        2147483649ULL,
        9223372039002292353ULL,
        9223372036854808585ULL,
        138ULL,
        136ULL,
        2147516425ULL,
        2147483658ULL,
        2147516555ULL,
        9223372036854775947ULL,
        9223372036854808713ULL,
        9223372036854808579ULL,
        9223372036854808578ULL,
        9223372036854775936ULL,
        32778ULL,
        9223372039002259466ULL,
        9223372039002292353ULL,
        9223372036854808704ULL,
        2147483649ULL,
        9223372039002292232ULL};

typedef struct size_t_x2_s {
  size_t fst;
  size_t snd;
} size_t_x2;

/**
A monomorphic instance of libcrux_sha3.generic_keccak.KeccakState
with types uint64_t
with const generics
- $1size_t
*/
typedef struct libcrux_sha3_generic_keccak_KeccakState_17_s {
  uint64_t st[25U];
} libcrux_sha3_generic_keccak_KeccakState_17;

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.new_80
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE libcrux_sha3_generic_keccak_KeccakState_17
libcrux_sha3_generic_keccak_new_80_04(void) {
  libcrux_sha3_generic_keccak_KeccakState_17 lit;
  uint64_t repeat_expression[25U];
  for (size_t i = (size_t)0U; i < (size_t)25U; i++) {
    repeat_expression[i] = libcrux_sha3_simd_portable_zero_d2();
  }
  memcpy(lit.st, repeat_expression, (size_t)25U * sizeof(uint64_t));
  return lit;
}

/**
A monomorphic instance of libcrux_sha3.traits.get_ij
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE uint64_t *libcrux_sha3_traits_get_ij_04(uint64_t *arr,
                                                               size_t i,
                                                               size_t j) {
  return &arr[(size_t)5U * j + i];
}

/**
A monomorphic instance of libcrux_sha3.traits.set_ij
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void libcrux_sha3_traits_set_ij_04(uint64_t *arr,
                                                          size_t i, size_t j,
                                                          uint64_t value) {
  arr[(size_t)5U * j + i] = value;
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_block_f8(
    uint64_t *state, Eurydice_slice blocks, size_t start) {
  uint64_t state_flat[25U] = {0U};
  for (size_t i = (size_t)0U; i < (size_t)72U / (size_t)8U; i++) {
    size_t i0 = i;
    size_t offset = start + (size_t)8U * i0;
    uint8_t uu____0[8U];
    Result_15 dst;
    Eurydice_slice_to_array2(
        &dst,
        Eurydice_slice_subslice3(blocks, offset, offset + (size_t)8U,
                                 uint8_t *),
        Eurydice_slice, uint8_t[8U], TryFromSliceError);
    unwrap_26_68(dst, uu____0);
    state_flat[i0] = core_num__u64__from_le_bytes(uu____0);
  }
  for (size_t i = (size_t)0U; i < (size_t)72U / (size_t)8U; i++) {
    size_t i0 = i;
    libcrux_sha3_traits_set_ij_04(
        state, i0 / (size_t)5U, i0 % (size_t)5U,
        libcrux_sha3_traits_get_ij_04(state, i0 / (size_t)5U,
                                      i0 % (size_t)5U)[0U] ^
            state_flat[i0]);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block_a1
with const generics
- RATE= 72
*/
static inline void libcrux_sha3_simd_portable_load_block_a1_f8(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_f8(self->st, input[0U], start);
}

/**
This function found in impl {core::ops::index::Index<(usize, usize), T> for
libcrux_sha3::generic_keccak::KeccakState<T, N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.index_c2
with types uint64_t
with const generics
- N= 1
*/
static inline uint64_t *libcrux_sha3_generic_keccak_index_c2_04(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, size_t_x2 index) {
  return libcrux_sha3_traits_get_ij_04(self->st, index.fst, index.snd);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.theta_80
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_theta_80_04(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, uint64_t ret[5U]) {
  uint64_t c[5U] = {
      libcrux_sha3_simd_portable_xor5_d2(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)0U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)0U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)0U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)0U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)0U}))[0U]),
      libcrux_sha3_simd_portable_xor5_d2(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)1U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)1U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)1U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)1U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)1U}))[0U]),
      libcrux_sha3_simd_portable_xor5_d2(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)2U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)2U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)2U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)2U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)2U}))[0U]),
      libcrux_sha3_simd_portable_xor5_d2(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)3U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)3U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)3U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)3U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)3U}))[0U]),
      libcrux_sha3_simd_portable_xor5_d2(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)4U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)4U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)4U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)4U}))[0U],
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)4U}))[0U])};
  uint64_t uu____0 = libcrux_sha3_simd_portable_rotate_left1_and_xor_d2(
      c[((size_t)0U + (size_t)4U) % (size_t)5U],
      c[((size_t)0U + (size_t)1U) % (size_t)5U]);
  uint64_t uu____1 = libcrux_sha3_simd_portable_rotate_left1_and_xor_d2(
      c[((size_t)1U + (size_t)4U) % (size_t)5U],
      c[((size_t)1U + (size_t)1U) % (size_t)5U]);
  uint64_t uu____2 = libcrux_sha3_simd_portable_rotate_left1_and_xor_d2(
      c[((size_t)2U + (size_t)4U) % (size_t)5U],
      c[((size_t)2U + (size_t)1U) % (size_t)5U]);
  uint64_t uu____3 = libcrux_sha3_simd_portable_rotate_left1_and_xor_d2(
      c[((size_t)3U + (size_t)4U) % (size_t)5U],
      c[((size_t)3U + (size_t)1U) % (size_t)5U]);
  ret[0U] = uu____0;
  ret[1U] = uu____1;
  ret[2U] = uu____2;
  ret[3U] = uu____3;
  ret[4U] = libcrux_sha3_simd_portable_rotate_left1_and_xor_d2(
      c[((size_t)4U + (size_t)4U) % (size_t)5U],
      c[((size_t)4U + (size_t)1U) % (size_t)5U]);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.set_80
with types uint64_t
with const generics
- N= 1
*/
static inline void libcrux_sha3_generic_keccak_set_80_04(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, size_t i, size_t j,
    uint64_t v) {
  libcrux_sha3_traits_set_ij_04(self->st, i, j, v);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 36
- RIGHT= 28
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_02(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)36);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 36
- RIGHT= 28
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_02(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_02(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 36
- RIGHT= 28
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_02(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_02(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 3
- RIGHT= 61
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_ac(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)3);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 3
- RIGHT= 61
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_ac(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_ac(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 3
- RIGHT= 61
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_ac(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_ac(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 41
- RIGHT= 23
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_020(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)41);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 41
- RIGHT= 23
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_020(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_020(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 41
- RIGHT= 23
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_020(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_020(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 18
- RIGHT= 46
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_a9(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)18);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 18
- RIGHT= 46
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_a9(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_a9(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 18
- RIGHT= 46
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_a9(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_a9(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 1
- RIGHT= 63
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_76(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_76(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 1
- RIGHT= 63
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_76(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_76(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 44
- RIGHT= 20
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_58(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)44);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 44
- RIGHT= 20
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_58(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_58(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 44
- RIGHT= 20
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_58(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_58(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 10
- RIGHT= 54
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_e0(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)10);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 10
- RIGHT= 54
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_e0(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_e0(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 10
- RIGHT= 54
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_e0(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_e0(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 45
- RIGHT= 19
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_63(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)45);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 45
- RIGHT= 19
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_63(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_63(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 45
- RIGHT= 19
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_63(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_63(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 2
- RIGHT= 62
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_6a(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)2);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 2
- RIGHT= 62
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_6a(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_6a(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 2
- RIGHT= 62
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_6a(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_6a(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 62
- RIGHT= 2
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_ab(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)62);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 62
- RIGHT= 2
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_ab(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_ab(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 62
- RIGHT= 2
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_ab(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_ab(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 6
- RIGHT= 58
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_5b(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)6);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 6
- RIGHT= 58
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_5b(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_5b(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 6
- RIGHT= 58
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_5b(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_5b(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 43
- RIGHT= 21
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_6f(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)43);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 43
- RIGHT= 21
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_6f(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_6f(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 43
- RIGHT= 21
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_6f(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_6f(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 15
- RIGHT= 49
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_62(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)15);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 15
- RIGHT= 49
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_62(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_62(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 15
- RIGHT= 49
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_62(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_62(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 61
- RIGHT= 3
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_23(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)61);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 61
- RIGHT= 3
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_23(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_23(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 61
- RIGHT= 3
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_23(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_23(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 28
- RIGHT= 36
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_37(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)28);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 28
- RIGHT= 36
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_37(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_37(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 28
- RIGHT= 36
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_37(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_37(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 55
- RIGHT= 9
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_bb(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)55);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 55
- RIGHT= 9
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_bb(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_bb(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 55
- RIGHT= 9
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_bb(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_bb(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 25
- RIGHT= 39
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_b9(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)25);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 25
- RIGHT= 39
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_b9(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_b9(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 25
- RIGHT= 39
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_b9(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_b9(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 21
- RIGHT= 43
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_54(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)21);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 21
- RIGHT= 43
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_54(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_54(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 21
- RIGHT= 43
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_54(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_54(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 56
- RIGHT= 8
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_4c(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)56);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 56
- RIGHT= 8
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_4c(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_4c(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 56
- RIGHT= 8
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_4c(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_4c(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 27
- RIGHT= 37
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_ce(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)27);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 27
- RIGHT= 37
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_ce(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_ce(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 27
- RIGHT= 37
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_ce(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_ce(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 20
- RIGHT= 44
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_77(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)20);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 20
- RIGHT= 44
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_77(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_77(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 20
- RIGHT= 44
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_77(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_77(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 39
- RIGHT= 25
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_25(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)39);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 39
- RIGHT= 25
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_25(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_25(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 39
- RIGHT= 25
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_25(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_25(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 8
- RIGHT= 56
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_af(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)8);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 8
- RIGHT= 56
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_af(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_af(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 8
- RIGHT= 56
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_af(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_af(a, b);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.rotate_left
with const generics
- LEFT= 14
- RIGHT= 50
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_rotate_left_fd(uint64_t x) {
  return core_num__u64__rotate_left(x, (uint32_t)(int32_t)14);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable._vxarq_u64
with const generics
- LEFT= 14
- RIGHT= 50
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable__vxarq_u64_fd(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable_rotate_left_fd(a ^ b);
}

/**
This function found in impl {libcrux_sha3::traits::KeccakItem<1usize> for u64}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.xor_and_rotate_d2
with const generics
- LEFT= 14
- RIGHT= 50
*/
static KRML_MUSTINLINE uint64_t
libcrux_sha3_simd_portable_xor_and_rotate_d2_fd(uint64_t a, uint64_t b) {
  return libcrux_sha3_simd_portable__vxarq_u64_fd(a, b);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.rho_80
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_rho_80_04(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, uint64_t t[5U]) {
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)0U, (size_t)0U,
      libcrux_sha3_simd_portable_xor_d2(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)0U}))[0U],
          t[0U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____0 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____0, (size_t)1U, (size_t)0U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_02(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)0U}))[0U],
          t[0U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____1 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____1, (size_t)2U, (size_t)0U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_ac(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)0U}))[0U],
          t[0U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____2 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____2, (size_t)3U, (size_t)0U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_020(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)0U}))[0U],
          t[0U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____3 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____3, (size_t)4U, (size_t)0U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_a9(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)0U}))[0U],
          t[0U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____4 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____4, (size_t)0U, (size_t)1U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_76(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)1U}))[0U],
          t[1U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____5 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____5, (size_t)1U, (size_t)1U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_58(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)1U}))[0U],
          t[1U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____6 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____6, (size_t)2U, (size_t)1U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_e0(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)1U}))[0U],
          t[1U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____7 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____7, (size_t)3U, (size_t)1U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_63(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)1U}))[0U],
          t[1U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____8 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____8, (size_t)4U, (size_t)1U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_6a(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)1U}))[0U],
          t[1U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____9 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____9, (size_t)0U, (size_t)2U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_ab(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)2U}))[0U],
          t[2U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____10 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____10, (size_t)1U, (size_t)2U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_5b(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)2U}))[0U],
          t[2U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____11 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____11, (size_t)2U, (size_t)2U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_6f(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)2U}))[0U],
          t[2U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____12 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____12, (size_t)3U, (size_t)2U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_62(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)2U}))[0U],
          t[2U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____13 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____13, (size_t)4U, (size_t)2U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_23(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)2U}))[0U],
          t[2U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____14 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____14, (size_t)0U, (size_t)3U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_37(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)3U}))[0U],
          t[3U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____15 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____15, (size_t)1U, (size_t)3U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_bb(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)3U}))[0U],
          t[3U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____16 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____16, (size_t)2U, (size_t)3U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_b9(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)3U}))[0U],
          t[3U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____17 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____17, (size_t)3U, (size_t)3U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_54(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)3U}))[0U],
          t[3U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____18 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____18, (size_t)4U, (size_t)3U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_4c(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)3U}))[0U],
          t[3U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____19 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____19, (size_t)0U, (size_t)4U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_ce(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)4U}))[0U],
          t[4U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____20 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____20, (size_t)1U, (size_t)4U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_77(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                              .snd = (size_t)4U}))[0U],
          t[4U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____21 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____21, (size_t)2U, (size_t)4U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_25(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                              .snd = (size_t)4U}))[0U],
          t[4U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____22 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____22, (size_t)3U, (size_t)4U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_af(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                              .snd = (size_t)4U}))[0U],
          t[4U]));
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____23 = self;
  libcrux_sha3_generic_keccak_set_80_04(
      uu____23, (size_t)4U, (size_t)4U,
      libcrux_sha3_simd_portable_xor_and_rotate_d2_fd(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                              .snd = (size_t)4U}))[0U],
          t[4U]));
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.pi_80
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_pi_80_04(
    libcrux_sha3_generic_keccak_KeccakState_17 *self) {
  libcrux_sha3_generic_keccak_KeccakState_17 old = self[0U];
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)1U, (size_t)0U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                          .snd = (size_t)3U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)2U, (size_t)0U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                          .snd = (size_t)1U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)3U, (size_t)0U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                          .snd = (size_t)4U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)4U, (size_t)0U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                          .snd = (size_t)2U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)0U, (size_t)1U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                          .snd = (size_t)1U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)1U, (size_t)1U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                          .snd = (size_t)4U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)2U, (size_t)1U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                          .snd = (size_t)2U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)3U, (size_t)1U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                          .snd = (size_t)0U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)4U, (size_t)1U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)1U,
                                          .snd = (size_t)3U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)0U, (size_t)2U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                          .snd = (size_t)2U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)1U, (size_t)2U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                          .snd = (size_t)0U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)2U, (size_t)2U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                          .snd = (size_t)3U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)3U, (size_t)2U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                          .snd = (size_t)1U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)4U, (size_t)2U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)2U,
                                          .snd = (size_t)4U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)0U, (size_t)3U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                          .snd = (size_t)3U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)1U, (size_t)3U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                          .snd = (size_t)1U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)2U, (size_t)3U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                          .snd = (size_t)4U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)3U, (size_t)3U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                          .snd = (size_t)2U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)4U, (size_t)3U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)3U,
                                          .snd = (size_t)0U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)0U, (size_t)4U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                          .snd = (size_t)4U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)1U, (size_t)4U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                          .snd = (size_t)2U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)2U, (size_t)4U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                          .snd = (size_t)0U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)3U, (size_t)4U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                          .snd = (size_t)3U}))[0U]);
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)4U, (size_t)4U,
      libcrux_sha3_generic_keccak_index_c2_04(
          &old, (KRML_CLITERAL(size_t_x2){.fst = (size_t)4U,
                                          .snd = (size_t)1U}))[0U]);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.chi_80
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_chi_80_04(
    libcrux_sha3_generic_keccak_KeccakState_17 *self) {
  libcrux_sha3_generic_keccak_KeccakState_17 old = self[0U];
  for (size_t i0 = (size_t)0U; i0 < (size_t)5U; i0++) {
    size_t i1 = i0;
    for (size_t i = (size_t)0U; i < (size_t)5U; i++) {
      size_t j = i;
      libcrux_sha3_generic_keccak_set_80_04(
          self, i1, j,
          libcrux_sha3_simd_portable_and_not_xor_d2(
              libcrux_sha3_generic_keccak_index_c2_04(
                  self, (KRML_CLITERAL(size_t_x2){.fst = i1, .snd = j}))[0U],
              libcrux_sha3_generic_keccak_index_c2_04(
                  &old,
                  (KRML_CLITERAL(size_t_x2){
                      .fst = i1, .snd = (j + (size_t)2U) % (size_t)5U}))[0U],
              libcrux_sha3_generic_keccak_index_c2_04(
                  &old,
                  (KRML_CLITERAL(size_t_x2){
                      .fst = i1, .snd = (j + (size_t)1U) % (size_t)5U}))[0U]));
    }
  }
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.iota_80
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_iota_80_04(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, size_t i) {
  libcrux_sha3_generic_keccak_set_80_04(
      self, (size_t)0U, (size_t)0U,
      libcrux_sha3_simd_portable_xor_constant_d2(
          libcrux_sha3_generic_keccak_index_c2_04(
              self, (KRML_CLITERAL(size_t_x2){.fst = (size_t)0U,
                                              .snd = (size_t)0U}))[0U],
          libcrux_sha3_generic_keccak_constants_ROUNDCONSTANTS[i]));
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.keccakf1600_80
with types uint64_t
with const generics
- N= 1
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_keccakf1600_80_04(
    libcrux_sha3_generic_keccak_KeccakState_17 *self) {
  for (size_t i = (size_t)0U; i < (size_t)24U; i++) {
    size_t i0 = i;
    uint64_t t[5U];
    libcrux_sha3_generic_keccak_theta_80_04(self, t);
    libcrux_sha3_generic_keccak_KeccakState_17 *uu____0 = self;
    uint64_t uu____1[5U];
    memcpy(uu____1, t, (size_t)5U * sizeof(uint64_t));
    libcrux_sha3_generic_keccak_rho_80_04(uu____0, uu____1);
    libcrux_sha3_generic_keccak_pi_80_04(self);
    libcrux_sha3_generic_keccak_chi_80_04(self);
    libcrux_sha3_generic_keccak_iota_80_04(self, i0);
  }
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block_80
with types uint64_t
with const generics
- N= 1
- RATE= 72
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_block_80_c6(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *blocks,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_a1_f8(self, blocks, start);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last
with const generics
- RATE= 72
- DELIMITER= 6
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_last_96(
    uint64_t *state, Eurydice_slice blocks, size_t start, size_t len) {
  uint8_t buffer[72U] = {0U};
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(buffer, (size_t)0U, len, uint8_t *),
      Eurydice_slice_subslice3(blocks, start, start + len, uint8_t *), uint8_t);
  buffer[len] = 6U;
  size_t uu____0 = (size_t)72U - (size_t)1U;
  buffer[uu____0] = (uint32_t)buffer[uu____0] | 128U;
  libcrux_sha3_simd_portable_load_block_f8(
      state, Eurydice_array_to_slice((size_t)72U, buffer, uint8_t), (size_t)0U);
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last_a1
with const generics
- RATE= 72
- DELIMITER= 6
*/
static inline void libcrux_sha3_simd_portable_load_last_a1_96(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_96(self->st, input[0U], start, len);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final_80
with types uint64_t
with const generics
- N= 1
- RATE= 72
- DELIM= 6
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_final_80_9e(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *last,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_a1_96(self, last, start, len);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.store_block
with const generics
- RATE= 72
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_store_block_f8(
    uint64_t *s, Eurydice_slice out, size_t start, size_t len) {
  size_t octets = len / (size_t)8U;
  for (size_t i = (size_t)0U; i < octets; i++) {
    size_t i0 = i;
    Eurydice_slice uu____0 = Eurydice_slice_subslice3(
        out, start + (size_t)8U * i0, start + (size_t)8U * i0 + (size_t)8U,
        uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, i0 / (size_t)5U, i0 % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
  }
  size_t remaining = len % (size_t)8U;
  if (remaining > (size_t)0U) {
    Eurydice_slice uu____1 = Eurydice_slice_subslice3(
        out, start + len - remaining, start + len, uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, octets / (size_t)5U,
                                      octets % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____1,
        Eurydice_array_to_subslice3(ret, (size_t)0U, remaining, uint8_t *),
        uint8_t);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Squeeze1<u64> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.squeeze_13
with const generics
- RATE= 72
*/
static inline void libcrux_sha3_simd_portable_squeeze_13_f8(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_store_block_f8(self->st, out, start, len);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.portable.keccak1
with const generics
- RATE= 72
- DELIM= 6
*/
static inline void libcrux_sha3_generic_keccak_portable_keccak1_96(
    Eurydice_slice data, Eurydice_slice out) {
  libcrux_sha3_generic_keccak_KeccakState_17 s =
      libcrux_sha3_generic_keccak_new_80_04();
  size_t data_len = Eurydice_slice_len(data, uint8_t);
  for (size_t i = (size_t)0U; i < data_len / (size_t)72U; i++) {
    size_t i0 = i;
    Eurydice_slice buf[1U] = {data};
    libcrux_sha3_generic_keccak_absorb_block_80_c6(&s, buf, i0 * (size_t)72U);
  }
  size_t rem = data_len % (size_t)72U;
  Eurydice_slice buf[1U] = {data};
  libcrux_sha3_generic_keccak_absorb_final_80_9e(&s, buf, data_len - rem, rem);
  size_t outlen = Eurydice_slice_len(out, uint8_t);
  size_t blocks = outlen / (size_t)72U;
  size_t last = outlen - outlen % (size_t)72U;
  if (blocks == (size_t)0U) {
    libcrux_sha3_simd_portable_squeeze_13_f8(&s, out, (size_t)0U, outlen);
  } else {
    libcrux_sha3_simd_portable_squeeze_13_f8(&s, out, (size_t)0U, (size_t)72U);
    for (size_t i = (size_t)1U; i < blocks; i++) {
      size_t i0 = i;
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_f8(&s, out, i0 * (size_t)72U,
                                               (size_t)72U);
    }
    if (last < outlen) {
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_f8(&s, out, last, outlen - last);
    }
  }
}

/**
 A portable SHA3 512 implementation.
*/
static KRML_MUSTINLINE void libcrux_sha3_portable_sha512(Eurydice_slice digest,
                                                         Eurydice_slice data) {
  libcrux_sha3_generic_keccak_portable_keccak1_96(data, digest);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_block_5b(
    uint64_t *state, Eurydice_slice blocks, size_t start) {
  uint64_t state_flat[25U] = {0U};
  for (size_t i = (size_t)0U; i < (size_t)136U / (size_t)8U; i++) {
    size_t i0 = i;
    size_t offset = start + (size_t)8U * i0;
    uint8_t uu____0[8U];
    Result_15 dst;
    Eurydice_slice_to_array2(
        &dst,
        Eurydice_slice_subslice3(blocks, offset, offset + (size_t)8U,
                                 uint8_t *),
        Eurydice_slice, uint8_t[8U], TryFromSliceError);
    unwrap_26_68(dst, uu____0);
    state_flat[i0] = core_num__u64__from_le_bytes(uu____0);
  }
  for (size_t i = (size_t)0U; i < (size_t)136U / (size_t)8U; i++) {
    size_t i0 = i;
    libcrux_sha3_traits_set_ij_04(
        state, i0 / (size_t)5U, i0 % (size_t)5U,
        libcrux_sha3_traits_get_ij_04(state, i0 / (size_t)5U,
                                      i0 % (size_t)5U)[0U] ^
            state_flat[i0]);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block_a1
with const generics
- RATE= 136
*/
static inline void libcrux_sha3_simd_portable_load_block_a1_5b(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_5b(self->st, input[0U], start);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block_80
with types uint64_t
with const generics
- N= 1
- RATE= 136
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_block_80_c60(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *blocks,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_a1_5b(self, blocks, start);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last
with const generics
- RATE= 136
- DELIMITER= 6
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_last_ad(
    uint64_t *state, Eurydice_slice blocks, size_t start, size_t len) {
  uint8_t buffer[136U] = {0U};
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(buffer, (size_t)0U, len, uint8_t *),
      Eurydice_slice_subslice3(blocks, start, start + len, uint8_t *), uint8_t);
  buffer[len] = 6U;
  size_t uu____0 = (size_t)136U - (size_t)1U;
  buffer[uu____0] = (uint32_t)buffer[uu____0] | 128U;
  libcrux_sha3_simd_portable_load_block_5b(
      state, Eurydice_array_to_slice((size_t)136U, buffer, uint8_t),
      (size_t)0U);
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last_a1
with const generics
- RATE= 136
- DELIMITER= 6
*/
static inline void libcrux_sha3_simd_portable_load_last_a1_ad(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_ad(self->st, input[0U], start, len);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final_80
with types uint64_t
with const generics
- N= 1
- RATE= 136
- DELIM= 6
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_final_80_9e0(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *last,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_a1_ad(self, last, start, len);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.store_block
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_store_block_5b(
    uint64_t *s, Eurydice_slice out, size_t start, size_t len) {
  size_t octets = len / (size_t)8U;
  for (size_t i = (size_t)0U; i < octets; i++) {
    size_t i0 = i;
    Eurydice_slice uu____0 = Eurydice_slice_subslice3(
        out, start + (size_t)8U * i0, start + (size_t)8U * i0 + (size_t)8U,
        uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, i0 / (size_t)5U, i0 % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
  }
  size_t remaining = len % (size_t)8U;
  if (remaining > (size_t)0U) {
    Eurydice_slice uu____1 = Eurydice_slice_subslice3(
        out, start + len - remaining, start + len, uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, octets / (size_t)5U,
                                      octets % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____1,
        Eurydice_array_to_subslice3(ret, (size_t)0U, remaining, uint8_t *),
        uint8_t);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Squeeze1<u64> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.squeeze_13
with const generics
- RATE= 136
*/
static inline void libcrux_sha3_simd_portable_squeeze_13_5b(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_store_block_5b(self->st, out, start, len);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.portable.keccak1
with const generics
- RATE= 136
- DELIM= 6
*/
static inline void libcrux_sha3_generic_keccak_portable_keccak1_ad(
    Eurydice_slice data, Eurydice_slice out) {
  libcrux_sha3_generic_keccak_KeccakState_17 s =
      libcrux_sha3_generic_keccak_new_80_04();
  size_t data_len = Eurydice_slice_len(data, uint8_t);
  for (size_t i = (size_t)0U; i < data_len / (size_t)136U; i++) {
    size_t i0 = i;
    Eurydice_slice buf[1U] = {data};
    libcrux_sha3_generic_keccak_absorb_block_80_c60(&s, buf, i0 * (size_t)136U);
  }
  size_t rem = data_len % (size_t)136U;
  Eurydice_slice buf[1U] = {data};
  libcrux_sha3_generic_keccak_absorb_final_80_9e0(&s, buf, data_len - rem, rem);
  size_t outlen = Eurydice_slice_len(out, uint8_t);
  size_t blocks = outlen / (size_t)136U;
  size_t last = outlen - outlen % (size_t)136U;
  if (blocks == (size_t)0U) {
    libcrux_sha3_simd_portable_squeeze_13_5b(&s, out, (size_t)0U, outlen);
  } else {
    libcrux_sha3_simd_portable_squeeze_13_5b(&s, out, (size_t)0U, (size_t)136U);
    for (size_t i = (size_t)1U; i < blocks; i++) {
      size_t i0 = i;
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_5b(&s, out, i0 * (size_t)136U,
                                               (size_t)136U);
    }
    if (last < outlen) {
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_5b(&s, out, last, outlen - last);
    }
  }
}

/**
 A portable SHA3 256 implementation.
*/
static KRML_MUSTINLINE void libcrux_sha3_portable_sha256(Eurydice_slice digest,
                                                         Eurydice_slice data) {
  libcrux_sha3_generic_keccak_portable_keccak1_ad(data, digest);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last
with const generics
- RATE= 136
- DELIMITER= 31
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_last_ad0(
    uint64_t *state, Eurydice_slice blocks, size_t start, size_t len) {
  uint8_t buffer[136U] = {0U};
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(buffer, (size_t)0U, len, uint8_t *),
      Eurydice_slice_subslice3(blocks, start, start + len, uint8_t *), uint8_t);
  buffer[len] = 31U;
  size_t uu____0 = (size_t)136U - (size_t)1U;
  buffer[uu____0] = (uint32_t)buffer[uu____0] | 128U;
  libcrux_sha3_simd_portable_load_block_5b(
      state, Eurydice_array_to_slice((size_t)136U, buffer, uint8_t),
      (size_t)0U);
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last_a1
with const generics
- RATE= 136
- DELIMITER= 31
*/
static inline void libcrux_sha3_simd_portable_load_last_a1_ad0(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_ad0(self->st, input[0U], start, len);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final_80
with types uint64_t
with const generics
- N= 1
- RATE= 136
- DELIM= 31
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_final_80_9e1(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *last,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_a1_ad0(self, last, start, len);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.portable.keccak1
with const generics
- RATE= 136
- DELIM= 31
*/
static inline void libcrux_sha3_generic_keccak_portable_keccak1_ad0(
    Eurydice_slice data, Eurydice_slice out) {
  libcrux_sha3_generic_keccak_KeccakState_17 s =
      libcrux_sha3_generic_keccak_new_80_04();
  size_t data_len = Eurydice_slice_len(data, uint8_t);
  for (size_t i = (size_t)0U; i < data_len / (size_t)136U; i++) {
    size_t i0 = i;
    Eurydice_slice buf[1U] = {data};
    libcrux_sha3_generic_keccak_absorb_block_80_c60(&s, buf, i0 * (size_t)136U);
  }
  size_t rem = data_len % (size_t)136U;
  Eurydice_slice buf[1U] = {data};
  libcrux_sha3_generic_keccak_absorb_final_80_9e1(&s, buf, data_len - rem, rem);
  size_t outlen = Eurydice_slice_len(out, uint8_t);
  size_t blocks = outlen / (size_t)136U;
  size_t last = outlen - outlen % (size_t)136U;
  if (blocks == (size_t)0U) {
    libcrux_sha3_simd_portable_squeeze_13_5b(&s, out, (size_t)0U, outlen);
  } else {
    libcrux_sha3_simd_portable_squeeze_13_5b(&s, out, (size_t)0U, (size_t)136U);
    for (size_t i = (size_t)1U; i < blocks; i++) {
      size_t i0 = i;
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_5b(&s, out, i0 * (size_t)136U,
                                               (size_t)136U);
    }
    if (last < outlen) {
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_5b(&s, out, last, outlen - last);
    }
  }
}

/**
 A portable SHAKE256 implementation.
*/
static KRML_MUSTINLINE void libcrux_sha3_portable_shake256(
    Eurydice_slice digest, Eurydice_slice data) {
  libcrux_sha3_generic_keccak_portable_keccak1_ad0(data, digest);
}

typedef libcrux_sha3_generic_keccak_KeccakState_17
    libcrux_sha3_portable_KeccakState;

/**
 Create a new SHAKE-128 state object.
*/
static KRML_MUSTINLINE libcrux_sha3_generic_keccak_KeccakState_17
libcrux_sha3_portable_incremental_shake128_init(void) {
  return libcrux_sha3_generic_keccak_new_80_04();
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_block_3a(
    uint64_t *state, Eurydice_slice blocks, size_t start) {
  uint64_t state_flat[25U] = {0U};
  for (size_t i = (size_t)0U; i < (size_t)168U / (size_t)8U; i++) {
    size_t i0 = i;
    size_t offset = start + (size_t)8U * i0;
    uint8_t uu____0[8U];
    Result_15 dst;
    Eurydice_slice_to_array2(
        &dst,
        Eurydice_slice_subslice3(blocks, offset, offset + (size_t)8U,
                                 uint8_t *),
        Eurydice_slice, uint8_t[8U], TryFromSliceError);
    unwrap_26_68(dst, uu____0);
    state_flat[i0] = core_num__u64__from_le_bytes(uu____0);
  }
  for (size_t i = (size_t)0U; i < (size_t)168U / (size_t)8U; i++) {
    size_t i0 = i;
    libcrux_sha3_traits_set_ij_04(
        state, i0 / (size_t)5U, i0 % (size_t)5U,
        libcrux_sha3_traits_get_ij_04(state, i0 / (size_t)5U,
                                      i0 % (size_t)5U)[0U] ^
            state_flat[i0]);
  }
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last
with const generics
- RATE= 168
- DELIMITER= 31
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_last_c6(
    uint64_t *state, Eurydice_slice blocks, size_t start, size_t len) {
  uint8_t buffer[168U] = {0U};
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(buffer, (size_t)0U, len, uint8_t *),
      Eurydice_slice_subslice3(blocks, start, start + len, uint8_t *), uint8_t);
  buffer[len] = 31U;
  size_t uu____0 = (size_t)168U - (size_t)1U;
  buffer[uu____0] = (uint32_t)buffer[uu____0] | 128U;
  libcrux_sha3_simd_portable_load_block_3a(
      state, Eurydice_array_to_slice((size_t)168U, buffer, uint8_t),
      (size_t)0U);
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last_a1
with const generics
- RATE= 168
- DELIMITER= 31
*/
static inline void libcrux_sha3_simd_portable_load_last_a1_c6(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_c6(self->st, input[0U], start, len);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final_80
with types uint64_t
with const generics
- N= 1
- RATE= 168
- DELIM= 31
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_final_80_9e2(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *last,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_a1_c6(self, last, start, len);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
 Absorb
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_incremental_shake128_absorb_final(
    libcrux_sha3_generic_keccak_KeccakState_17 *s, Eurydice_slice data0) {
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____0 = s;
  Eurydice_slice uu____1[1U] = {data0};
  libcrux_sha3_generic_keccak_absorb_final_80_9e2(
      uu____0, uu____1, (size_t)0U, Eurydice_slice_len(data0, uint8_t));
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.store_block
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_store_block_3a(
    uint64_t *s, Eurydice_slice out, size_t start, size_t len) {
  size_t octets = len / (size_t)8U;
  for (size_t i = (size_t)0U; i < octets; i++) {
    size_t i0 = i;
    Eurydice_slice uu____0 = Eurydice_slice_subslice3(
        out, start + (size_t)8U * i0, start + (size_t)8U * i0 + (size_t)8U,
        uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, i0 / (size_t)5U, i0 % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
  }
  size_t remaining = len % (size_t)8U;
  if (remaining > (size_t)0U) {
    Eurydice_slice uu____1 = Eurydice_slice_subslice3(
        out, start + len - remaining, start + len, uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, octets / (size_t)5U,
                                      octets % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____1,
        Eurydice_array_to_subslice3(ret, (size_t)0U, remaining, uint8_t *),
        uint8_t);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Squeeze1<u64> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.squeeze_13
with const generics
- RATE= 168
*/
static inline void libcrux_sha3_simd_portable_squeeze_13_3a(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_store_block_3a(self->st, out, start, len);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<u64,
1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of
libcrux_sha3.generic_keccak.portable.squeeze_first_three_blocks_b4 with const
generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_portable_squeeze_first_three_blocks_b4_3a(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out) {
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, (size_t)0U, (size_t)168U);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, (size_t)168U,
                                           (size_t)168U);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, (size_t)2U * (size_t)168U,
                                           (size_t)168U);
}

/**
 Squeeze three blocks
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_incremental_shake128_squeeze_first_three_blocks(
    libcrux_sha3_generic_keccak_KeccakState_17 *s, Eurydice_slice out0) {
  libcrux_sha3_generic_keccak_portable_squeeze_first_three_blocks_b4_3a(s,
                                                                        out0);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<u64,
1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of
libcrux_sha3.generic_keccak.portable.squeeze_next_block_b4 with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_portable_squeeze_next_block_b4_3a(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out,
    size_t start) {
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, start, (size_t)168U);
}

/**
 Squeeze another block
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_incremental_shake128_squeeze_next_block(
    libcrux_sha3_generic_keccak_KeccakState_17 *s, Eurydice_slice out0) {
  libcrux_sha3_generic_keccak_portable_squeeze_next_block_b4_3a(s, out0,
                                                                (size_t)0U);
}

#define libcrux_sha3_Algorithm_Sha224 1
#define libcrux_sha3_Algorithm_Sha256 2
#define libcrux_sha3_Algorithm_Sha384 3
#define libcrux_sha3_Algorithm_Sha512 4

typedef uint8_t libcrux_sha3_Algorithm;

typedef uint8_t libcrux_sha3_Sha3_224Digest[28U];

typedef uint8_t libcrux_sha3_Sha3_256Digest[32U];

typedef uint8_t libcrux_sha3_Sha3_384Digest[48U];

typedef uint8_t libcrux_sha3_Sha3_512Digest[64U];

/**
 Returns the output size of a digest.
*/
static inline size_t libcrux_sha3_digest_size(libcrux_sha3_Algorithm mode) {
  switch (mode) {
    case libcrux_sha3_Algorithm_Sha224: {
      break;
    }
    case libcrux_sha3_Algorithm_Sha256: {
      return (size_t)32U;
    }
    case libcrux_sha3_Algorithm_Sha384: {
      return (size_t)48U;
    }
    case libcrux_sha3_Algorithm_Sha512: {
      return (size_t)64U;
    }
    default: {
      KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__,
                        __LINE__);
      KRML_HOST_EXIT(253U);
    }
  }
  return (size_t)28U;
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_block_2c(
    uint64_t *state, Eurydice_slice blocks, size_t start) {
  uint64_t state_flat[25U] = {0U};
  for (size_t i = (size_t)0U; i < (size_t)144U / (size_t)8U; i++) {
    size_t i0 = i;
    size_t offset = start + (size_t)8U * i0;
    uint8_t uu____0[8U];
    Result_15 dst;
    Eurydice_slice_to_array2(
        &dst,
        Eurydice_slice_subslice3(blocks, offset, offset + (size_t)8U,
                                 uint8_t *),
        Eurydice_slice, uint8_t[8U], TryFromSliceError);
    unwrap_26_68(dst, uu____0);
    state_flat[i0] = core_num__u64__from_le_bytes(uu____0);
  }
  for (size_t i = (size_t)0U; i < (size_t)144U / (size_t)8U; i++) {
    size_t i0 = i;
    libcrux_sha3_traits_set_ij_04(
        state, i0 / (size_t)5U, i0 % (size_t)5U,
        libcrux_sha3_traits_get_ij_04(state, i0 / (size_t)5U,
                                      i0 % (size_t)5U)[0U] ^
            state_flat[i0]);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block_a1
with const generics
- RATE= 144
*/
static inline void libcrux_sha3_simd_portable_load_block_a1_2c(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_2c(self->st, input[0U], start);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block_80
with types uint64_t
with const generics
- N= 1
- RATE= 144
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_block_80_c61(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *blocks,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_a1_2c(self, blocks, start);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last
with const generics
- RATE= 144
- DELIMITER= 6
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_last_1e(
    uint64_t *state, Eurydice_slice blocks, size_t start, size_t len) {
  uint8_t buffer[144U] = {0U};
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(buffer, (size_t)0U, len, uint8_t *),
      Eurydice_slice_subslice3(blocks, start, start + len, uint8_t *), uint8_t);
  buffer[len] = 6U;
  size_t uu____0 = (size_t)144U - (size_t)1U;
  buffer[uu____0] = (uint32_t)buffer[uu____0] | 128U;
  libcrux_sha3_simd_portable_load_block_2c(
      state, Eurydice_array_to_slice((size_t)144U, buffer, uint8_t),
      (size_t)0U);
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last_a1
with const generics
- RATE= 144
- DELIMITER= 6
*/
static inline void libcrux_sha3_simd_portable_load_last_a1_1e(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_1e(self->st, input[0U], start, len);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final_80
with types uint64_t
with const generics
- N= 1
- RATE= 144
- DELIM= 6
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_final_80_9e3(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *last,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_a1_1e(self, last, start, len);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.store_block
with const generics
- RATE= 144
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_store_block_2c(
    uint64_t *s, Eurydice_slice out, size_t start, size_t len) {
  size_t octets = len / (size_t)8U;
  for (size_t i = (size_t)0U; i < octets; i++) {
    size_t i0 = i;
    Eurydice_slice uu____0 = Eurydice_slice_subslice3(
        out, start + (size_t)8U * i0, start + (size_t)8U * i0 + (size_t)8U,
        uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, i0 / (size_t)5U, i0 % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
  }
  size_t remaining = len % (size_t)8U;
  if (remaining > (size_t)0U) {
    Eurydice_slice uu____1 = Eurydice_slice_subslice3(
        out, start + len - remaining, start + len, uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, octets / (size_t)5U,
                                      octets % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____1,
        Eurydice_array_to_subslice3(ret, (size_t)0U, remaining, uint8_t *),
        uint8_t);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Squeeze1<u64> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.squeeze_13
with const generics
- RATE= 144
*/
static inline void libcrux_sha3_simd_portable_squeeze_13_2c(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_store_block_2c(self->st, out, start, len);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.portable.keccak1
with const generics
- RATE= 144
- DELIM= 6
*/
static inline void libcrux_sha3_generic_keccak_portable_keccak1_1e(
    Eurydice_slice data, Eurydice_slice out) {
  libcrux_sha3_generic_keccak_KeccakState_17 s =
      libcrux_sha3_generic_keccak_new_80_04();
  size_t data_len = Eurydice_slice_len(data, uint8_t);
  for (size_t i = (size_t)0U; i < data_len / (size_t)144U; i++) {
    size_t i0 = i;
    Eurydice_slice buf[1U] = {data};
    libcrux_sha3_generic_keccak_absorb_block_80_c61(&s, buf, i0 * (size_t)144U);
  }
  size_t rem = data_len % (size_t)144U;
  Eurydice_slice buf[1U] = {data};
  libcrux_sha3_generic_keccak_absorb_final_80_9e3(&s, buf, data_len - rem, rem);
  size_t outlen = Eurydice_slice_len(out, uint8_t);
  size_t blocks = outlen / (size_t)144U;
  size_t last = outlen - outlen % (size_t)144U;
  if (blocks == (size_t)0U) {
    libcrux_sha3_simd_portable_squeeze_13_2c(&s, out, (size_t)0U, outlen);
  } else {
    libcrux_sha3_simd_portable_squeeze_13_2c(&s, out, (size_t)0U, (size_t)144U);
    for (size_t i = (size_t)1U; i < blocks; i++) {
      size_t i0 = i;
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_2c(&s, out, i0 * (size_t)144U,
                                               (size_t)144U);
    }
    if (last < outlen) {
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_2c(&s, out, last, outlen - last);
    }
  }
}

/**
 A portable SHA3 224 implementation.
*/
static KRML_MUSTINLINE void libcrux_sha3_portable_sha224(Eurydice_slice digest,
                                                         Eurydice_slice data) {
  libcrux_sha3_generic_keccak_portable_keccak1_1e(data, digest);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_block_7a(
    uint64_t *state, Eurydice_slice blocks, size_t start) {
  uint64_t state_flat[25U] = {0U};
  for (size_t i = (size_t)0U; i < (size_t)104U / (size_t)8U; i++) {
    size_t i0 = i;
    size_t offset = start + (size_t)8U * i0;
    uint8_t uu____0[8U];
    Result_15 dst;
    Eurydice_slice_to_array2(
        &dst,
        Eurydice_slice_subslice3(blocks, offset, offset + (size_t)8U,
                                 uint8_t *),
        Eurydice_slice, uint8_t[8U], TryFromSliceError);
    unwrap_26_68(dst, uu____0);
    state_flat[i0] = core_num__u64__from_le_bytes(uu____0);
  }
  for (size_t i = (size_t)0U; i < (size_t)104U / (size_t)8U; i++) {
    size_t i0 = i;
    libcrux_sha3_traits_set_ij_04(
        state, i0 / (size_t)5U, i0 % (size_t)5U,
        libcrux_sha3_traits_get_ij_04(state, i0 / (size_t)5U,
                                      i0 % (size_t)5U)[0U] ^
            state_flat[i0]);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block_a1
with const generics
- RATE= 104
*/
static inline void libcrux_sha3_simd_portable_load_block_a1_7a(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_7a(self->st, input[0U], start);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block_80
with types uint64_t
with const generics
- N= 1
- RATE= 104
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_block_80_c62(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *blocks,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_a1_7a(self, blocks, start);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last
with const generics
- RATE= 104
- DELIMITER= 6
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_load_last_7c(
    uint64_t *state, Eurydice_slice blocks, size_t start, size_t len) {
  uint8_t buffer[104U] = {0U};
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(buffer, (size_t)0U, len, uint8_t *),
      Eurydice_slice_subslice3(blocks, start, start + len, uint8_t *), uint8_t);
  buffer[len] = 6U;
  size_t uu____0 = (size_t)104U - (size_t)1U;
  buffer[uu____0] = (uint32_t)buffer[uu____0] | 128U;
  libcrux_sha3_simd_portable_load_block_7a(
      state, Eurydice_array_to_slice((size_t)104U, buffer, uint8_t),
      (size_t)0U);
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_last_a1
with const generics
- RATE= 104
- DELIMITER= 6
*/
static inline void libcrux_sha3_simd_portable_load_last_a1_7c(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_7c(self->st, input[0U], start, len);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_final_80
with types uint64_t
with const generics
- N= 1
- RATE= 104
- DELIM= 6
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_final_80_9e4(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *last,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_load_last_a1_7c(self, last, start, len);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.simd.portable.store_block
with const generics
- RATE= 104
*/
static KRML_MUSTINLINE void libcrux_sha3_simd_portable_store_block_7a(
    uint64_t *s, Eurydice_slice out, size_t start, size_t len) {
  size_t octets = len / (size_t)8U;
  for (size_t i = (size_t)0U; i < octets; i++) {
    size_t i0 = i;
    Eurydice_slice uu____0 = Eurydice_slice_subslice3(
        out, start + (size_t)8U * i0, start + (size_t)8U * i0 + (size_t)8U,
        uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, i0 / (size_t)5U, i0 % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)8U, ret, uint8_t), uint8_t);
  }
  size_t remaining = len % (size_t)8U;
  if (remaining > (size_t)0U) {
    Eurydice_slice uu____1 = Eurydice_slice_subslice3(
        out, start + len - remaining, start + len, uint8_t *);
    uint8_t ret[8U];
    core_num__u64__to_le_bytes(
        libcrux_sha3_traits_get_ij_04(s, octets / (size_t)5U,
                                      octets % (size_t)5U)[0U],
        ret);
    Eurydice_slice_copy(
        uu____1,
        Eurydice_array_to_subslice3(ret, (size_t)0U, remaining, uint8_t *),
        uint8_t);
  }
}

/**
This function found in impl {libcrux_sha3::traits::Squeeze1<u64> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.squeeze_13
with const generics
- RATE= 104
*/
static inline void libcrux_sha3_simd_portable_squeeze_13_7a(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out,
    size_t start, size_t len) {
  libcrux_sha3_simd_portable_store_block_7a(self->st, out, start, len);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.portable.keccak1
with const generics
- RATE= 104
- DELIM= 6
*/
static inline void libcrux_sha3_generic_keccak_portable_keccak1_7c(
    Eurydice_slice data, Eurydice_slice out) {
  libcrux_sha3_generic_keccak_KeccakState_17 s =
      libcrux_sha3_generic_keccak_new_80_04();
  size_t data_len = Eurydice_slice_len(data, uint8_t);
  for (size_t i = (size_t)0U; i < data_len / (size_t)104U; i++) {
    size_t i0 = i;
    Eurydice_slice buf[1U] = {data};
    libcrux_sha3_generic_keccak_absorb_block_80_c62(&s, buf, i0 * (size_t)104U);
  }
  size_t rem = data_len % (size_t)104U;
  Eurydice_slice buf[1U] = {data};
  libcrux_sha3_generic_keccak_absorb_final_80_9e4(&s, buf, data_len - rem, rem);
  size_t outlen = Eurydice_slice_len(out, uint8_t);
  size_t blocks = outlen / (size_t)104U;
  size_t last = outlen - outlen % (size_t)104U;
  if (blocks == (size_t)0U) {
    libcrux_sha3_simd_portable_squeeze_13_7a(&s, out, (size_t)0U, outlen);
  } else {
    libcrux_sha3_simd_portable_squeeze_13_7a(&s, out, (size_t)0U, (size_t)104U);
    for (size_t i = (size_t)1U; i < blocks; i++) {
      size_t i0 = i;
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_7a(&s, out, i0 * (size_t)104U,
                                               (size_t)104U);
    }
    if (last < outlen) {
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_7a(&s, out, last, outlen - last);
    }
  }
}

/**
 A portable SHA3 384 implementation.
*/
static KRML_MUSTINLINE void libcrux_sha3_portable_sha384(Eurydice_slice digest,
                                                         Eurydice_slice data) {
  libcrux_sha3_generic_keccak_portable_keccak1_7c(data, digest);
}

/**
 SHA3 224

 Preconditions:
 - `digest.len() == 28`
*/
static inline void libcrux_sha3_sha224_ema(Eurydice_slice digest,
                                           Eurydice_slice payload) {
  libcrux_sha3_portable_sha224(digest, payload);
}

/**
 SHA3 224
*/
static inline void libcrux_sha3_sha224(Eurydice_slice data, uint8_t ret[28U]) {
  uint8_t out[28U] = {0U};
  libcrux_sha3_sha224_ema(Eurydice_array_to_slice((size_t)28U, out, uint8_t),
                          data);
  memcpy(ret, out, (size_t)28U * sizeof(uint8_t));
}

/**
 SHA3 256
*/
static inline void libcrux_sha3_sha256_ema(Eurydice_slice digest,
                                           Eurydice_slice payload) {
  libcrux_sha3_portable_sha256(digest, payload);
}

/**
 SHA3 256
*/
static inline void libcrux_sha3_sha256(Eurydice_slice data, uint8_t ret[32U]) {
  uint8_t out[32U] = {0U};
  libcrux_sha3_sha256_ema(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                          data);
  memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
 SHA3 384
*/
static inline void libcrux_sha3_sha384_ema(Eurydice_slice digest,
                                           Eurydice_slice payload) {
  libcrux_sha3_portable_sha384(digest, payload);
}

/**
 SHA3 384
*/
static inline void libcrux_sha3_sha384(Eurydice_slice data, uint8_t ret[48U]) {
  uint8_t out[48U] = {0U};
  libcrux_sha3_sha384_ema(Eurydice_array_to_slice((size_t)48U, out, uint8_t),
                          data);
  memcpy(ret, out, (size_t)48U * sizeof(uint8_t));
}

/**
 SHA3 512
*/
static inline void libcrux_sha3_sha512_ema(Eurydice_slice digest,
                                           Eurydice_slice payload) {
  libcrux_sha3_portable_sha512(digest, payload);
}

/**
 SHA3 512
*/
static inline void libcrux_sha3_sha512(Eurydice_slice data, uint8_t ret[64U]) {
  uint8_t out[64U] = {0U};
  libcrux_sha3_sha512_ema(Eurydice_array_to_slice((size_t)64U, out, uint8_t),
                          data);
  memcpy(ret, out, (size_t)64U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_sha3::traits::Absorb<1usize> for
libcrux_sha3::generic_keccak::KeccakState<u64, 1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of libcrux_sha3.simd.portable.load_block_a1
with const generics
- RATE= 168
*/
static inline void libcrux_sha3_simd_portable_load_block_a1_3a(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *input,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_3a(self->st, input[0U], start);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<T,
N>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.absorb_block_80
with types uint64_t
with const generics
- N= 1
- RATE= 168
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_absorb_block_80_c63(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice *blocks,
    size_t start) {
  libcrux_sha3_simd_portable_load_block_a1_3a(self, blocks, start);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.portable.keccak1
with const generics
- RATE= 168
- DELIM= 31
*/
static inline void libcrux_sha3_generic_keccak_portable_keccak1_c6(
    Eurydice_slice data, Eurydice_slice out) {
  libcrux_sha3_generic_keccak_KeccakState_17 s =
      libcrux_sha3_generic_keccak_new_80_04();
  size_t data_len = Eurydice_slice_len(data, uint8_t);
  for (size_t i = (size_t)0U; i < data_len / (size_t)168U; i++) {
    size_t i0 = i;
    Eurydice_slice buf[1U] = {data};
    libcrux_sha3_generic_keccak_absorb_block_80_c63(&s, buf, i0 * (size_t)168U);
  }
  size_t rem = data_len % (size_t)168U;
  Eurydice_slice buf[1U] = {data};
  libcrux_sha3_generic_keccak_absorb_final_80_9e2(&s, buf, data_len - rem, rem);
  size_t outlen = Eurydice_slice_len(out, uint8_t);
  size_t blocks = outlen / (size_t)168U;
  size_t last = outlen - outlen % (size_t)168U;
  if (blocks == (size_t)0U) {
    libcrux_sha3_simd_portable_squeeze_13_3a(&s, out, (size_t)0U, outlen);
  } else {
    libcrux_sha3_simd_portable_squeeze_13_3a(&s, out, (size_t)0U, (size_t)168U);
    for (size_t i = (size_t)1U; i < blocks; i++) {
      size_t i0 = i;
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_3a(&s, out, i0 * (size_t)168U,
                                               (size_t)168U);
    }
    if (last < outlen) {
      libcrux_sha3_generic_keccak_keccakf1600_80_04(&s);
      libcrux_sha3_simd_portable_squeeze_13_3a(&s, out, last, outlen - last);
    }
  }
}

/**
 A portable SHAKE128 implementation.
*/
static KRML_MUSTINLINE void libcrux_sha3_portable_shake128(
    Eurydice_slice digest, Eurydice_slice data) {
  libcrux_sha3_generic_keccak_portable_keccak1_c6(data, digest);
}

/**
 SHAKE 128

 Writes `out.len()` bytes.
*/
static inline void libcrux_sha3_shake128_ema(Eurydice_slice out,
                                             Eurydice_slice data) {
  libcrux_sha3_portable_shake128(out, data);
}

/**
 SHAKE 256

 Writes `out.len()` bytes.
*/
static inline void libcrux_sha3_shake256_ema(Eurydice_slice out,
                                             Eurydice_slice data) {
  libcrux_sha3_portable_shake256(out, data);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<u64,
1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of
libcrux_sha3.generic_keccak.portable.squeeze_first_five_blocks_b4 with const
generics
- RATE= 168
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_portable_squeeze_first_five_blocks_b4_3a(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out) {
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, (size_t)0U, (size_t)168U);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, (size_t)168U,
                                           (size_t)168U);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, (size_t)2U * (size_t)168U,
                                           (size_t)168U);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, (size_t)3U * (size_t)168U,
                                           (size_t)168U);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
  libcrux_sha3_simd_portable_squeeze_13_3a(self, out, (size_t)4U * (size_t)168U,
                                           (size_t)168U);
}

/**
 Squeeze five blocks
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_incremental_shake128_squeeze_first_five_blocks(
    libcrux_sha3_generic_keccak_KeccakState_17 *s, Eurydice_slice out0) {
  libcrux_sha3_generic_keccak_portable_squeeze_first_five_blocks_b4_3a(s, out0);
}

/**
 Absorb some data for SHAKE-256 for the last time
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_incremental_shake256_absorb_final(
    libcrux_sha3_generic_keccak_KeccakState_17 *s, Eurydice_slice data) {
  libcrux_sha3_generic_keccak_KeccakState_17 *uu____0 = s;
  Eurydice_slice uu____1[1U] = {data};
  libcrux_sha3_generic_keccak_absorb_final_80_9e1(
      uu____0, uu____1, (size_t)0U, Eurydice_slice_len(data, uint8_t));
}

/**
 Create a new SHAKE-256 state object.
*/
static KRML_MUSTINLINE libcrux_sha3_generic_keccak_KeccakState_17
libcrux_sha3_portable_incremental_shake256_init(void) {
  return libcrux_sha3_generic_keccak_new_80_04();
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<u64,
1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of
libcrux_sha3.generic_keccak.portable.squeeze_first_block_b4 with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_portable_squeeze_first_block_b4_5b(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out) {
  libcrux_sha3_simd_portable_squeeze_13_5b(self, out, (size_t)0U, (size_t)136U);
}

/**
 Squeeze the first SHAKE-256 block
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_incremental_shake256_squeeze_first_block(
    libcrux_sha3_generic_keccak_KeccakState_17 *s, Eurydice_slice out) {
  libcrux_sha3_generic_keccak_portable_squeeze_first_block_b4_5b(s, out);
}

/**
This function found in impl {libcrux_sha3::generic_keccak::KeccakState<u64,
1usize>[core::marker::Sized<u64>,
libcrux_sha3::simd::portable::{libcrux_sha3::traits::KeccakItem<1usize> for
u64}]}
*/
/**
A monomorphic instance of
libcrux_sha3.generic_keccak.portable.squeeze_next_block_b4 with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void
libcrux_sha3_generic_keccak_portable_squeeze_next_block_b4_5b(
    libcrux_sha3_generic_keccak_KeccakState_17 *self, Eurydice_slice out,
    size_t start) {
  libcrux_sha3_generic_keccak_keccakf1600_80_04(self);
  libcrux_sha3_simd_portable_squeeze_13_5b(self, out, start, (size_t)136U);
}

/**
 Squeeze the next SHAKE-256 block
*/
static KRML_MUSTINLINE void
libcrux_sha3_portable_incremental_shake256_squeeze_next_block(
    libcrux_sha3_generic_keccak_KeccakState_17 *s, Eurydice_slice out) {
  libcrux_sha3_generic_keccak_portable_squeeze_next_block_b4_5b(s, out,
                                                                (size_t)0U);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.KeccakXofState
with types uint64_t
with const generics
- $1size_t
- $136size_t
*/
typedef struct libcrux_sha3_generic_keccak_xof_KeccakXofState_e2_s {
  libcrux_sha3_generic_keccak_KeccakState_17 inner;
  uint8_t buf[1U][136U];
  size_t buf_len;
  bool sponge;
} libcrux_sha3_generic_keccak_xof_KeccakXofState_e2;

typedef libcrux_sha3_generic_keccak_xof_KeccakXofState_e2
    libcrux_sha3_portable_incremental_Shake256Xof;

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.fill_buffer_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 136
*/
static inline size_t libcrux_sha3_generic_keccak_xof_fill_buffer_35_c6(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 *self,
    Eurydice_slice *inputs) {
  size_t input_len = Eurydice_slice_len(inputs[0U], uint8_t);
  size_t consumed = (size_t)0U;
  if (self->buf_len > (size_t)0U) {
    if (self->buf_len + input_len >= (size_t)136U) {
      consumed = (size_t)136U - self->buf_len;
      for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
        size_t i0 = i;
        Eurydice_slice uu____0 = Eurydice_array_to_subslice_from(
            (size_t)136U, self->buf[i0], self->buf_len, uint8_t, size_t,
            uint8_t[]);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_slice_subslice_to(inputs[i0], consumed, uint8_t, size_t,
                                       uint8_t[]),
            uint8_t);
      }
      self->buf_len = self->buf_len + consumed;
    }
  }
  return consumed;
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.absorb_full_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 136
*/
static inline size_t libcrux_sha3_generic_keccak_xof_absorb_full_35_c6(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 *self,
    Eurydice_slice *inputs) {
  size_t input_consumed =
      libcrux_sha3_generic_keccak_xof_fill_buffer_35_c6(self, inputs);
  if (input_consumed > (size_t)0U) {
    Eurydice_slice borrowed[1U];
    for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
      uint8_t buf[136U] = {0U};
      borrowed[i] = core_array___Array_T__N___as_slice((size_t)136U, buf,
                                                       uint8_t, Eurydice_slice);
    }
    for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
      size_t i0 = i;
      borrowed[i0] =
          Eurydice_array_to_slice((size_t)136U, self->buf[i0], uint8_t);
    }
    libcrux_sha3_simd_portable_load_block_a1_5b(&self->inner, borrowed,
                                                (size_t)0U);
    libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
    self->buf_len = (size_t)0U;
  }
  size_t input_to_consume =
      Eurydice_slice_len(inputs[0U], uint8_t) - input_consumed;
  size_t num_blocks = input_to_consume / (size_t)136U;
  size_t remainder = input_to_consume % (size_t)136U;
  for (size_t i = (size_t)0U; i < num_blocks; i++) {
    size_t i0 = i;
    libcrux_sha3_simd_portable_load_block_a1_5b(
        &self->inner, inputs, input_consumed + i0 * (size_t)136U);
    libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
  }
  return remainder;
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.absorb_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 136
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_xof_absorb_35_c6(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 *self,
    Eurydice_slice *inputs) {
  size_t input_remainder_len =
      libcrux_sha3_generic_keccak_xof_absorb_full_35_c6(self, inputs);
  if (input_remainder_len > (size_t)0U) {
    size_t input_len = Eurydice_slice_len(inputs[0U], uint8_t);
    for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
      size_t i0 = i;
      Eurydice_slice_copy(Eurydice_array_to_subslice3(
                              self->buf[i0], self->buf_len,
                              self->buf_len + input_remainder_len, uint8_t *),
                          Eurydice_slice_subslice_from(
                              inputs[i0], input_len - input_remainder_len,
                              uint8_t, size_t, uint8_t[]),
                          uint8_t);
    }
    self->buf_len = self->buf_len + input_remainder_len;
  }
}

/**
 Shake256 absorb
*/
/**
This function found in impl {libcrux_sha3::portable::incremental::Xof<136usize>
for libcrux_sha3::portable::incremental::Shake256Xof}
*/
static inline void libcrux_sha3_portable_incremental_absorb_42(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 *self,
    Eurydice_slice input) {
  Eurydice_slice buf[1U] = {input};
  libcrux_sha3_generic_keccak_xof_absorb_35_c6(self, buf);
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.absorb_final_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 136
- DELIMITER= 31
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_xof_absorb_final_35_9e(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 *self,
    Eurydice_slice *inputs) {
  libcrux_sha3_generic_keccak_xof_absorb_35_c6(self, inputs);
  Eurydice_slice borrowed[1U];
  for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
    uint8_t buf[136U] = {0U};
    borrowed[i] = core_array___Array_T__N___as_slice((size_t)136U, buf, uint8_t,
                                                     Eurydice_slice);
  }
  for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
    size_t i0 = i;
    borrowed[i0] =
        Eurydice_array_to_slice((size_t)136U, self->buf[i0], uint8_t);
  }
  libcrux_sha3_simd_portable_load_last_a1_ad0(&self->inner, borrowed,
                                              (size_t)0U, self->buf_len);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
}

/**
 Shake256 absorb final
*/
/**
This function found in impl {libcrux_sha3::portable::incremental::Xof<136usize>
for libcrux_sha3::portable::incremental::Shake256Xof}
*/
static inline void libcrux_sha3_portable_incremental_absorb_final_42(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 *self,
    Eurydice_slice input) {
  Eurydice_slice buf[1U] = {input};
  libcrux_sha3_generic_keccak_xof_absorb_final_35_9e(self, buf);
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.zero_block_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 136
*/
static inline void libcrux_sha3_generic_keccak_xof_zero_block_35_c6(
    uint8_t ret[136U]) {
  memset(ret, 0U, 136U * sizeof(uint8_t));
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.new_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 136
*/
static inline libcrux_sha3_generic_keccak_xof_KeccakXofState_e2
libcrux_sha3_generic_keccak_xof_new_35_c6(void) {
  libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 lit;
  lit.inner = libcrux_sha3_generic_keccak_new_80_04();
  uint8_t repeat_expression[1U][136U];
  for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
    libcrux_sha3_generic_keccak_xof_zero_block_35_c6(repeat_expression[i]);
  }
  memcpy(lit.buf, repeat_expression, (size_t)1U * sizeof(uint8_t[136U]));
  lit.buf_len = (size_t)0U;
  lit.sponge = false;
  return lit;
}

/**
 Shake256 new state
*/
/**
This function found in impl {libcrux_sha3::portable::incremental::Xof<136usize>
for libcrux_sha3::portable::incremental::Shake256Xof}
*/
static inline libcrux_sha3_generic_keccak_xof_KeccakXofState_e2
libcrux_sha3_portable_incremental_new_42(void) {
  return libcrux_sha3_generic_keccak_xof_new_35_c6();
}

/**
 Squeeze `N` x `LEN` bytes. Only `N = 1` for now.
*/
/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, 1usize,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.squeeze_85
with types uint64_t
with const generics
- RATE= 136
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_xof_squeeze_85_c7(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 *self,
    Eurydice_slice out) {
  if (self->sponge) {
    libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
  }
  size_t out_len = Eurydice_slice_len(out, uint8_t);
  if (out_len > (size_t)0U) {
    if (out_len <= (size_t)136U) {
      libcrux_sha3_simd_portable_squeeze_13_5b(&self->inner, out, (size_t)0U,
                                               out_len);
    } else {
      size_t blocks = out_len / (size_t)136U;
      for (size_t i = (size_t)0U; i < blocks; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
        libcrux_sha3_simd_portable_squeeze_13_5b(
            &self->inner, out, i0 * (size_t)136U, (size_t)136U);
      }
      size_t remaining = out_len % (size_t)136U;
      if (remaining > (size_t)0U) {
        libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
        libcrux_sha3_simd_portable_squeeze_13_5b(
            &self->inner, out, blocks * (size_t)136U, remaining);
      }
    }
    self->sponge = true;
  }
}

/**
 Shake256 squeeze
*/
/**
This function found in impl {libcrux_sha3::portable::incremental::Xof<136usize>
for libcrux_sha3::portable::incremental::Shake256Xof}
*/
static inline void libcrux_sha3_portable_incremental_squeeze_42(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_e2 *self,
    Eurydice_slice out) {
  libcrux_sha3_generic_keccak_xof_squeeze_85_c7(self, out);
}

/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.KeccakXofState
with types uint64_t
with const generics
- $1size_t
- $168size_t
*/
typedef struct libcrux_sha3_generic_keccak_xof_KeccakXofState_97_s {
  libcrux_sha3_generic_keccak_KeccakState_17 inner;
  uint8_t buf[1U][168U];
  size_t buf_len;
  bool sponge;
} libcrux_sha3_generic_keccak_xof_KeccakXofState_97;

typedef libcrux_sha3_generic_keccak_xof_KeccakXofState_97
    libcrux_sha3_portable_incremental_Shake128Xof;

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.fill_buffer_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 168
*/
static inline size_t libcrux_sha3_generic_keccak_xof_fill_buffer_35_c60(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_97 *self,
    Eurydice_slice *inputs) {
  size_t input_len = Eurydice_slice_len(inputs[0U], uint8_t);
  size_t consumed = (size_t)0U;
  if (self->buf_len > (size_t)0U) {
    if (self->buf_len + input_len >= (size_t)168U) {
      consumed = (size_t)168U - self->buf_len;
      for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
        size_t i0 = i;
        Eurydice_slice uu____0 = Eurydice_array_to_subslice_from(
            (size_t)168U, self->buf[i0], self->buf_len, uint8_t, size_t,
            uint8_t[]);
        Eurydice_slice_copy(
            uu____0,
            Eurydice_slice_subslice_to(inputs[i0], consumed, uint8_t, size_t,
                                       uint8_t[]),
            uint8_t);
      }
      self->buf_len = self->buf_len + consumed;
    }
  }
  return consumed;
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.absorb_full_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 168
*/
static inline size_t libcrux_sha3_generic_keccak_xof_absorb_full_35_c60(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_97 *self,
    Eurydice_slice *inputs) {
  size_t input_consumed =
      libcrux_sha3_generic_keccak_xof_fill_buffer_35_c60(self, inputs);
  if (input_consumed > (size_t)0U) {
    Eurydice_slice borrowed[1U];
    for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
      uint8_t buf[168U] = {0U};
      borrowed[i] = core_array___Array_T__N___as_slice((size_t)168U, buf,
                                                       uint8_t, Eurydice_slice);
    }
    for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
      size_t i0 = i;
      borrowed[i0] =
          Eurydice_array_to_slice((size_t)168U, self->buf[i0], uint8_t);
    }
    libcrux_sha3_simd_portable_load_block_a1_3a(&self->inner, borrowed,
                                                (size_t)0U);
    libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
    self->buf_len = (size_t)0U;
  }
  size_t input_to_consume =
      Eurydice_slice_len(inputs[0U], uint8_t) - input_consumed;
  size_t num_blocks = input_to_consume / (size_t)168U;
  size_t remainder = input_to_consume % (size_t)168U;
  for (size_t i = (size_t)0U; i < num_blocks; i++) {
    size_t i0 = i;
    libcrux_sha3_simd_portable_load_block_a1_3a(
        &self->inner, inputs, input_consumed + i0 * (size_t)168U);
    libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
  }
  return remainder;
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.absorb_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 168
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_xof_absorb_35_c60(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_97 *self,
    Eurydice_slice *inputs) {
  size_t input_remainder_len =
      libcrux_sha3_generic_keccak_xof_absorb_full_35_c60(self, inputs);
  if (input_remainder_len > (size_t)0U) {
    size_t input_len = Eurydice_slice_len(inputs[0U], uint8_t);
    for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
      size_t i0 = i;
      Eurydice_slice_copy(Eurydice_array_to_subslice3(
                              self->buf[i0], self->buf_len,
                              self->buf_len + input_remainder_len, uint8_t *),
                          Eurydice_slice_subslice_from(
                              inputs[i0], input_len - input_remainder_len,
                              uint8_t, size_t, uint8_t[]),
                          uint8_t);
    }
    self->buf_len = self->buf_len + input_remainder_len;
  }
}

/**
This function found in impl {libcrux_sha3::portable::incremental::Xof<168usize>
for libcrux_sha3::portable::incremental::Shake128Xof}
*/
static inline void libcrux_sha3_portable_incremental_absorb_26(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_97 *self,
    Eurydice_slice input) {
  Eurydice_slice buf[1U] = {input};
  libcrux_sha3_generic_keccak_xof_absorb_35_c60(self, buf);
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.absorb_final_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 168
- DELIMITER= 31
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_xof_absorb_final_35_9e0(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_97 *self,
    Eurydice_slice *inputs) {
  libcrux_sha3_generic_keccak_xof_absorb_35_c60(self, inputs);
  Eurydice_slice borrowed[1U];
  for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
    uint8_t buf[168U] = {0U};
    borrowed[i] = core_array___Array_T__N___as_slice((size_t)168U, buf, uint8_t,
                                                     Eurydice_slice);
  }
  for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
    size_t i0 = i;
    borrowed[i0] =
        Eurydice_array_to_slice((size_t)168U, self->buf[i0], uint8_t);
  }
  libcrux_sha3_simd_portable_load_last_a1_c6(&self->inner, borrowed, (size_t)0U,
                                             self->buf_len);
  libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
}

/**
This function found in impl {libcrux_sha3::portable::incremental::Xof<168usize>
for libcrux_sha3::portable::incremental::Shake128Xof}
*/
static inline void libcrux_sha3_portable_incremental_absorb_final_26(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_97 *self,
    Eurydice_slice input) {
  Eurydice_slice buf[1U] = {input};
  libcrux_sha3_generic_keccak_xof_absorb_final_35_9e0(self, buf);
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.zero_block_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 168
*/
static inline void libcrux_sha3_generic_keccak_xof_zero_block_35_c60(
    uint8_t ret[168U]) {
  memset(ret, 0U, 168U * sizeof(uint8_t));
}

/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, PARALLEL_LANES,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.new_35
with types uint64_t
with const generics
- PARALLEL_LANES= 1
- RATE= 168
*/
static inline libcrux_sha3_generic_keccak_xof_KeccakXofState_97
libcrux_sha3_generic_keccak_xof_new_35_c60(void) {
  libcrux_sha3_generic_keccak_xof_KeccakXofState_97 lit;
  lit.inner = libcrux_sha3_generic_keccak_new_80_04();
  uint8_t repeat_expression[1U][168U];
  for (size_t i = (size_t)0U; i < (size_t)1U; i++) {
    libcrux_sha3_generic_keccak_xof_zero_block_35_c60(repeat_expression[i]);
  }
  memcpy(lit.buf, repeat_expression, (size_t)1U * sizeof(uint8_t[168U]));
  lit.buf_len = (size_t)0U;
  lit.sponge = false;
  return lit;
}

/**
This function found in impl {libcrux_sha3::portable::incremental::Xof<168usize>
for libcrux_sha3::portable::incremental::Shake128Xof}
*/
static inline libcrux_sha3_generic_keccak_xof_KeccakXofState_97
libcrux_sha3_portable_incremental_new_26(void) {
  return libcrux_sha3_generic_keccak_xof_new_35_c60();
}

/**
 Squeeze `N` x `LEN` bytes. Only `N = 1` for now.
*/
/**
This function found in impl
{libcrux_sha3::generic_keccak::xof::KeccakXofState<STATE, 1usize,
RATE>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_sha3.generic_keccak.xof.squeeze_85
with types uint64_t
with const generics
- RATE= 168
*/
static KRML_MUSTINLINE void libcrux_sha3_generic_keccak_xof_squeeze_85_13(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_97 *self,
    Eurydice_slice out) {
  if (self->sponge) {
    libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
  }
  size_t out_len = Eurydice_slice_len(out, uint8_t);
  if (out_len > (size_t)0U) {
    if (out_len <= (size_t)168U) {
      libcrux_sha3_simd_portable_squeeze_13_3a(&self->inner, out, (size_t)0U,
                                               out_len);
    } else {
      size_t blocks = out_len / (size_t)168U;
      for (size_t i = (size_t)0U; i < blocks; i++) {
        size_t i0 = i;
        libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
        libcrux_sha3_simd_portable_squeeze_13_3a(
            &self->inner, out, i0 * (size_t)168U, (size_t)168U);
      }
      size_t remaining = out_len % (size_t)168U;
      if (remaining > (size_t)0U) {
        libcrux_sha3_generic_keccak_keccakf1600_80_04(&self->inner);
        libcrux_sha3_simd_portable_squeeze_13_3a(
            &self->inner, out, blocks * (size_t)168U, remaining);
      }
    }
    self->sponge = true;
  }
}

/**
 Shake128 squeeze
*/
/**
This function found in impl {libcrux_sha3::portable::incremental::Xof<168usize>
for libcrux_sha3::portable::incremental::Shake128Xof}
*/
static inline void libcrux_sha3_portable_incremental_squeeze_26(
    libcrux_sha3_generic_keccak_xof_KeccakXofState_97 *self,
    Eurydice_slice out) {
  libcrux_sha3_generic_keccak_xof_squeeze_85_13(self, out);
}

/**
This function found in impl {core::clone::Clone for
libcrux_sha3::portable::KeccakState}
*/
static inline libcrux_sha3_generic_keccak_KeccakState_17
libcrux_sha3_portable_clone_fe(
    libcrux_sha3_generic_keccak_KeccakState_17 *self) {
  return self[0U];
}

/**
This function found in impl {core::convert::From<libcrux_sha3::Algorithm> for
u32}
*/
static inline uint32_t libcrux_sha3_from_6c(libcrux_sha3_Algorithm v) {
  switch (v) {
    case libcrux_sha3_Algorithm_Sha224: {
      break;
    }
    case libcrux_sha3_Algorithm_Sha256: {
      return 2U;
    }
    case libcrux_sha3_Algorithm_Sha384: {
      return 3U;
    }
    case libcrux_sha3_Algorithm_Sha512: {
      return 4U;
    }
    default: {
      KRML_HOST_EPRINTF("KaRaMeL incomplete match at %s:%d\n", __FILE__,
                        __LINE__);
      KRML_HOST_EXIT(253U);
    }
  }
  return 1U;
}

/**
This function found in impl {core::convert::From<u32> for
libcrux_sha3::Algorithm}
*/
static inline libcrux_sha3_Algorithm libcrux_sha3_from_29(uint32_t v) {
  switch (v) {
    case 1U: {
      break;
    }
    case 2U: {
      return libcrux_sha3_Algorithm_Sha256;
    }
    case 3U: {
      return libcrux_sha3_Algorithm_Sha384;
    }
    case 4U: {
      return libcrux_sha3_Algorithm_Sha512;
    }
    default: {
      KRML_HOST_EPRINTF("KaRaMeL abort at %s:%d\n%s\n", __FILE__, __LINE__,
                        "panic!");
      KRML_HOST_EXIT(255U);
    }
  }
  return libcrux_sha3_Algorithm_Sha224;
}

#if defined(__cplusplus)
}
#endif

#define libcrux_sha3_portable_H_DEFINED
#endif /* libcrux_sha3_portable_H */

/* from libcrux/libcrux-ml-kem/extracts/c_header_only/generated/libcrux_mlkem768_portable.h */
/*
 * SPDX-FileCopyrightText: 2025 Cryspen Sarl <info@cryspen.com>
 *
 * SPDX-License-Identifier: MIT or Apache-2.0
 *
 * This code was generated with the following revisions:
 * Charon: 667d2fc98984ff7f3df989c2367e6c1fa4a000e7
 * Eurydice: 2381cbc416ef2ad0b561c362c500bc84f36b6785
 * Karamel: 80f5435f2fc505973c469a4afcc8d875cddd0d8b
 * F*: 71d8221589d4d438af3706d89cb653cf53e18aab
 * Libcrux: 68dfed5a4a9e40277f62828471c029afed1ecdcc
 */

#ifndef libcrux_mlkem768_portable_H
#define libcrux_mlkem768_portable_H


#if defined(__cplusplus)
extern "C" {
#endif


static inline void libcrux_ml_kem_hash_functions_portable_G(
    Eurydice_slice input, uint8_t ret[64U]) {
  uint8_t digest[64U] = {0U};
  libcrux_sha3_portable_sha512(
      Eurydice_array_to_slice((size_t)64U, digest, uint8_t), input);
  memcpy(ret, digest, (size_t)64U * sizeof(uint8_t));
}

static inline void libcrux_ml_kem_hash_functions_portable_H(
    Eurydice_slice input, uint8_t ret[32U]) {
  uint8_t digest[32U] = {0U};
  libcrux_sha3_portable_sha256(
      Eurydice_array_to_slice((size_t)32U, digest, uint8_t), input);
  memcpy(ret, digest, (size_t)32U * sizeof(uint8_t));
}

static const int16_t libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[128U] =
    {(int16_t)-1044, (int16_t)-758,  (int16_t)-359,  (int16_t)-1517,
     (int16_t)1493,  (int16_t)1422,  (int16_t)287,   (int16_t)202,
     (int16_t)-171,  (int16_t)622,   (int16_t)1577,  (int16_t)182,
     (int16_t)962,   (int16_t)-1202, (int16_t)-1474, (int16_t)1468,
     (int16_t)573,   (int16_t)-1325, (int16_t)264,   (int16_t)383,
     (int16_t)-829,  (int16_t)1458,  (int16_t)-1602, (int16_t)-130,
     (int16_t)-681,  (int16_t)1017,  (int16_t)732,   (int16_t)608,
     (int16_t)-1542, (int16_t)411,   (int16_t)-205,  (int16_t)-1571,
     (int16_t)1223,  (int16_t)652,   (int16_t)-552,  (int16_t)1015,
     (int16_t)-1293, (int16_t)1491,  (int16_t)-282,  (int16_t)-1544,
     (int16_t)516,   (int16_t)-8,    (int16_t)-320,  (int16_t)-666,
     (int16_t)-1618, (int16_t)-1162, (int16_t)126,   (int16_t)1469,
     (int16_t)-853,  (int16_t)-90,   (int16_t)-271,  (int16_t)830,
     (int16_t)107,   (int16_t)-1421, (int16_t)-247,  (int16_t)-951,
     (int16_t)-398,  (int16_t)961,   (int16_t)-1508, (int16_t)-725,
     (int16_t)448,   (int16_t)-1065, (int16_t)677,   (int16_t)-1275,
     (int16_t)-1103, (int16_t)430,   (int16_t)555,   (int16_t)843,
     (int16_t)-1251, (int16_t)871,   (int16_t)1550,  (int16_t)105,
     (int16_t)422,   (int16_t)587,   (int16_t)177,   (int16_t)-235,
     (int16_t)-291,  (int16_t)-460,  (int16_t)1574,  (int16_t)1653,
     (int16_t)-246,  (int16_t)778,   (int16_t)1159,  (int16_t)-147,
     (int16_t)-777,  (int16_t)1483,  (int16_t)-602,  (int16_t)1119,
     (int16_t)-1590, (int16_t)644,   (int16_t)-872,  (int16_t)349,
     (int16_t)418,   (int16_t)329,   (int16_t)-156,  (int16_t)-75,
     (int16_t)817,   (int16_t)1097,  (int16_t)603,   (int16_t)610,
     (int16_t)1322,  (int16_t)-1285, (int16_t)-1465, (int16_t)384,
     (int16_t)-1215, (int16_t)-136,  (int16_t)1218,  (int16_t)-1335,
     (int16_t)-874,  (int16_t)220,   (int16_t)-1187, (int16_t)-1659,
     (int16_t)-1185, (int16_t)-1530, (int16_t)-1278, (int16_t)794,
     (int16_t)-1510, (int16_t)-854,  (int16_t)-870,  (int16_t)478,
     (int16_t)-108,  (int16_t)-308,  (int16_t)996,   (int16_t)991,
     (int16_t)958,   (int16_t)-1460, (int16_t)1522,  (int16_t)1628};

static KRML_MUSTINLINE int16_t libcrux_ml_kem_polynomial_zeta(size_t i) {
  return libcrux_ml_kem_polynomial_ZETAS_TIMES_MONTGOMERY_R[i];
}

#define LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT ((size_t)16U)

#define LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR ((size_t)16U)

#define LIBCRUX_ML_KEM_VECTOR_TRAITS_MONTGOMERY_R_SQUARED_MOD_FIELD_MODULUS \
  ((int16_t)1353)

#define LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS ((int16_t)3329)

#define LIBCRUX_ML_KEM_VECTOR_TRAITS_INVERSE_OF_MODULUS_MOD_MONTGOMERY_R \
  (62209U)

typedef struct libcrux_ml_kem_vector_portable_vector_type_PortableVector_s {
  int16_t elements[16U];
} libcrux_ml_kem_vector_portable_vector_type_PortableVector;

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_vector_type_from_i16_array(
    Eurydice_slice array) {
  libcrux_ml_kem_vector_portable_vector_type_PortableVector lit;
  int16_t ret[16U];
  Result_0a dst;
  Eurydice_slice_to_array2(
      &dst, Eurydice_slice_subslice3(array, (size_t)0U, (size_t)16U, int16_t *),
      Eurydice_slice, int16_t[16U], TryFromSliceError);
  unwrap_26_00(dst, ret);
  memcpy(lit.elements, ret, (size_t)16U * sizeof(int16_t));
  return lit;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_from_i16_array_b8(Eurydice_slice array) {
  return libcrux_ml_kem_vector_portable_vector_type_from_i16_array(
      libcrux_secrets_int_classify_public_classify_ref_9b_39(array));
}

typedef struct int16_t_x8_s {
  int16_t fst;
  int16_t snd;
  int16_t thd;
  int16_t f3;
  int16_t f4;
  int16_t f5;
  int16_t f6;
  int16_t f7;
} int16_t_x8;

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_vector_type_zero(void) {
  libcrux_ml_kem_vector_portable_vector_type_PortableVector lit;
  int16_t ret[16U];
  int16_t buf[16U] = {0U};
  libcrux_secrets_int_public_integers_classify_27_46(buf, ret);
  memcpy(lit.elements, ret, (size_t)16U * sizeof(int16_t));
  return lit;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ZERO_b8(void) {
  return libcrux_ml_kem_vector_portable_vector_type_zero();
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_add(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    size_t uu____0 = i0;
    lhs.elements[uu____0] = lhs.elements[uu____0] + rhs->elements[i0];
  }
  return lhs;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_add_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs) {
  return libcrux_ml_kem_vector_portable_arithmetic_add(lhs, rhs);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_sub(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    size_t uu____0 = i0;
    lhs.elements[uu____0] = lhs.elements[uu____0] - rhs->elements[i0];
  }
  return lhs;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_sub_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs) {
  return libcrux_ml_kem_vector_portable_arithmetic_sub(lhs, rhs);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_multiply_by_constant(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec, int16_t c) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    size_t uu____0 = i0;
    vec.elements[uu____0] = vec.elements[uu____0] * c;
  }
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_multiply_by_constant_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec, int16_t c) {
  return libcrux_ml_kem_vector_portable_arithmetic_multiply_by_constant(vec, c);
}

/**
 Note: This function is not secret independent
 Only use with public values.
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_cond_subtract_3329(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    if (libcrux_secrets_int_public_integers_declassify_d8_39(
            vec.elements[i0]) >= (int16_t)3329) {
      size_t uu____0 = i0;
      vec.elements[uu____0] = vec.elements[uu____0] - (int16_t)3329;
    }
  }
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_cond_subtract_3329_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v) {
  return libcrux_ml_kem_vector_portable_arithmetic_cond_subtract_3329(v);
}

#define LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_BARRETT_MULTIPLIER \
  ((int32_t)20159)

#define LIBCRUX_ML_KEM_VECTOR_TRAITS_BARRETT_SHIFT ((int32_t)26)

#define LIBCRUX_ML_KEM_VECTOR_TRAITS_BARRETT_R \
  ((int32_t)1 << (uint32_t)LIBCRUX_ML_KEM_VECTOR_TRAITS_BARRETT_SHIFT)

/**
 Signed Barrett Reduction

 Given an input `value`, `barrett_reduce` outputs a representative `result`
 such that:

 - result ≡ value (mod FIELD_MODULUS)
 - the absolute value of `result` is bound as follows:

 `|result| ≤ FIELD_MODULUS / 2 · (|value|/BARRETT_R + 1)

 Note: The input bound is 28296 to prevent overflow in the multiplication of
 quotient by FIELD_MODULUS

*/
static inline int16_t
libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce_element(
    int16_t value) {
  int32_t t = libcrux_secrets_int_as_i32_f5(value) *
                  LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_BARRETT_MULTIPLIER +
              (LIBCRUX_ML_KEM_VECTOR_TRAITS_BARRETT_R >> 1U);
  int16_t quotient = libcrux_secrets_int_as_i16_36(
      t >> (uint32_t)LIBCRUX_ML_KEM_VECTOR_TRAITS_BARRETT_SHIFT);
  return value - quotient * LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS;
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    int16_t vi =
        libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce_element(
            vec.elements[i0]);
    vec.elements[i0] = vi;
  }
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_barrett_reduce_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vector) {
  return libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce(vector);
}

#define LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_MONTGOMERY_SHIFT (16U)

/**
 Signed Montgomery Reduction

 Given an input `value`, `montgomery_reduce` outputs a representative `o`
 such that:

 - o ≡ value · MONTGOMERY_R^(-1) (mod FIELD_MODULUS)
 - the absolute value of `o` is bound as follows:

 `|result| ≤ ceil(|value| / MONTGOMERY_R) + 1665

 In particular, if `|value| ≤ FIELD_MODULUS-1 * FIELD_MODULUS-1`, then `|o| <=
 FIELD_MODULUS-1`. And, if `|value| ≤ pow2 16 * FIELD_MODULUS-1`, then `|o| <=
 FIELD_MODULUS + 1664

*/
static inline int16_t
libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
    int32_t value) {
  int32_t k =
      libcrux_secrets_int_as_i32_f5(libcrux_secrets_int_as_i16_36(value)) *
      libcrux_secrets_int_as_i32_b8(
          libcrux_secrets_int_public_integers_classify_27_df(
              LIBCRUX_ML_KEM_VECTOR_TRAITS_INVERSE_OF_MODULUS_MOD_MONTGOMERY_R));
  int32_t k_times_modulus =
      libcrux_secrets_int_as_i32_f5(libcrux_secrets_int_as_i16_36(k)) *
      libcrux_secrets_int_as_i32_f5(
          libcrux_secrets_int_public_integers_classify_27_39(
              LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS));
  int16_t c = libcrux_secrets_int_as_i16_36(
      k_times_modulus >>
      (uint32_t)LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_MONTGOMERY_SHIFT);
  int16_t value_high = libcrux_secrets_int_as_i16_36(
      value >>
      (uint32_t)LIBCRUX_ML_KEM_VECTOR_PORTABLE_ARITHMETIC_MONTGOMERY_SHIFT);
  return value_high - c;
}

/**
 If `fe` is some field element 'x' of the Kyber field and `fer` is congruent to
 `y · MONTGOMERY_R`, this procedure outputs a value that is congruent to
 `x · y`, as follows:

    `fe · fer ≡ x · y · MONTGOMERY_R (mod FIELD_MODULUS)`

 `montgomery_reduce` takes the value `x · y · MONTGOMERY_R` and outputs a
 representative `x · y · MONTGOMERY_R * MONTGOMERY_R^{-1} ≡ x · y (mod
 FIELD_MODULUS)`.
*/
static KRML_MUSTINLINE int16_t
libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_fe_by_fer(
    int16_t fe, int16_t fer) {
  int32_t product =
      libcrux_secrets_int_as_i32_f5(fe) * libcrux_secrets_int_as_i32_f5(fer);
  return libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
      product);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_by_constant(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec, int16_t c) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    vec.elements[i0] =
        libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_fe_by_fer(
            vec.elements[i0], c);
  }
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vector,
    int16_t constant) {
  return libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_by_constant(
      vector, libcrux_secrets_int_public_integers_classify_27_39(constant));
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_bitwise_and_with_constant(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec, int16_t c) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    size_t uu____0 = i0;
    vec.elements[uu____0] = vec.elements[uu____0] & c;
  }
  return vec;
}

/**
A monomorphic instance of libcrux_ml_kem.vector.portable.arithmetic.shift_right
with const generics
- SHIFT_BY= 15
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_shift_right_ef(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    vec.elements[i0] = vec.elements[i0] >> (uint32_t)(int32_t)15;
  }
  return vec;
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_arithmetic_to_unsigned_representative(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  libcrux_ml_kem_vector_portable_vector_type_PortableVector t =
      libcrux_ml_kem_vector_portable_arithmetic_shift_right_ef(a);
  libcrux_ml_kem_vector_portable_vector_type_PortableVector fm =
      libcrux_ml_kem_vector_portable_arithmetic_bitwise_and_with_constant(
          t, LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS);
  return libcrux_ml_kem_vector_portable_arithmetic_add(a, &fm);
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_to_unsigned_representative_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  return libcrux_ml_kem_vector_portable_arithmetic_to_unsigned_representative(
      a);
}

/**
 The `compress_*` functions implement the `Compress` function specified in the
 NIST FIPS 203 standard (Page 18, Expression 4.5), which is defined as:

 ```plaintext
 Compress_d: ℤq -> ℤ_{2ᵈ}
 Compress_d(x) = ⌈(2ᵈ/q)·x⌋
 ```

 Since `⌈x⌋ = ⌊x + 1/2⌋` we have:

 ```plaintext
 Compress_d(x) = ⌊(2ᵈ/q)·x + 1/2⌋
               = ⌊(2^{d+1}·x + q) / 2q⌋
 ```

 For further information about the function implementations, consult the
 `implementation_notes.pdf` document in this directory.

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
static inline uint8_t
libcrux_ml_kem_vector_portable_compress_compress_message_coefficient(
    uint16_t fe) {
  int16_t shifted =
      libcrux_secrets_int_public_integers_classify_27_39((int16_t)1664) -
      libcrux_secrets_int_as_i16_ca(fe);
  int16_t mask = shifted >> 15U;
  int16_t shifted_to_positive = mask ^ shifted;
  int16_t shifted_positive_in_range = shifted_to_positive - (int16_t)832;
  int16_t r0 = shifted_positive_in_range >> 15U;
  int16_t r1 = r0 & (int16_t)1;
  return libcrux_secrets_int_as_u8_f5(r1);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_compress_1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    a.elements[i0] = libcrux_secrets_int_as_i16_59(
        libcrux_ml_kem_vector_portable_compress_compress_message_coefficient(
            libcrux_secrets_int_as_u16_f5(a.elements[i0])));
  }
  return a;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_1_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  return libcrux_ml_kem_vector_portable_compress_compress_1(a);
}

static KRML_MUSTINLINE uint32_t
libcrux_ml_kem_vector_portable_arithmetic_get_n_least_significant_bits(
    uint8_t n, uint32_t value) {
  return value & ((1U << (uint32_t)n) - 1U);
}

static inline int16_t
libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(
    uint8_t coefficient_bits, uint16_t fe) {
  uint64_t compressed = libcrux_secrets_int_as_u64_ca(fe)
                        << (uint32_t)coefficient_bits;
  compressed = compressed + 1664ULL;
  compressed = compressed * 10321340ULL;
  compressed = compressed >> 35U;
  return libcrux_secrets_int_as_i16_b8(
      libcrux_ml_kem_vector_portable_arithmetic_get_n_least_significant_bits(
          coefficient_bits, libcrux_secrets_int_as_u32_a3(compressed)));
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_decompress_1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  libcrux_ml_kem_vector_portable_vector_type_PortableVector z =
      libcrux_ml_kem_vector_portable_vector_type_zero();
  libcrux_ml_kem_vector_portable_vector_type_PortableVector s =
      libcrux_ml_kem_vector_portable_arithmetic_sub(z, &a);
  libcrux_ml_kem_vector_portable_vector_type_PortableVector res =
      libcrux_ml_kem_vector_portable_arithmetic_bitwise_and_with_constant(
          s, (int16_t)1665);
  return res;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_decompress_1_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  return libcrux_ml_kem_vector_portable_compress_decompress_1(a);
}

static KRML_MUSTINLINE void libcrux_ml_kem_vector_portable_ntt_ntt_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *vec,
    int16_t zeta, size_t i, size_t j) {
  int16_t t =
      libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_fe_by_fer(
          vec->elements[j],
          libcrux_secrets_int_public_integers_classify_27_39(zeta));
  int16_t a_minus_t = vec->elements[i] - t;
  int16_t a_plus_t = vec->elements[i] + t;
  vec->elements[j] = a_minus_t;
  vec->elements[i] = a_plus_t;
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_ntt_layer_1_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec,
    int16_t zeta0, int16_t zeta1, int16_t zeta2, int16_t zeta3) {
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta0, (size_t)0U,
                                              (size_t)2U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta0, (size_t)1U,
                                              (size_t)3U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta1, (size_t)4U,
                                              (size_t)6U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta1, (size_t)5U,
                                              (size_t)7U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta2, (size_t)8U,
                                              (size_t)10U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta2, (size_t)9U,
                                              (size_t)11U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta3, (size_t)12U,
                                              (size_t)14U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta3, (size_t)13U,
                                              (size_t)15U);
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_layer_1_step_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta0,
    int16_t zeta1, int16_t zeta2, int16_t zeta3) {
  return libcrux_ml_kem_vector_portable_ntt_ntt_layer_1_step(a, zeta0, zeta1,
                                                             zeta2, zeta3);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_ntt_layer_2_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec,
    int16_t zeta0, int16_t zeta1) {
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta0, (size_t)0U,
                                              (size_t)4U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta0, (size_t)1U,
                                              (size_t)5U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta0, (size_t)2U,
                                              (size_t)6U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta0, (size_t)3U,
                                              (size_t)7U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta1, (size_t)8U,
                                              (size_t)12U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta1, (size_t)9U,
                                              (size_t)13U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta1, (size_t)10U,
                                              (size_t)14U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta1, (size_t)11U,
                                              (size_t)15U);
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_layer_2_step_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta0,
    int16_t zeta1) {
  return libcrux_ml_kem_vector_portable_ntt_ntt_layer_2_step(a, zeta0, zeta1);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_ntt_layer_3_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec,
    int16_t zeta) {
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta, (size_t)0U,
                                              (size_t)8U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta, (size_t)1U,
                                              (size_t)9U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta, (size_t)2U,
                                              (size_t)10U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta, (size_t)3U,
                                              (size_t)11U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta, (size_t)4U,
                                              (size_t)12U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta, (size_t)5U,
                                              (size_t)13U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta, (size_t)6U,
                                              (size_t)14U);
  libcrux_ml_kem_vector_portable_ntt_ntt_step(&vec, zeta, (size_t)7U,
                                              (size_t)15U);
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_layer_3_step_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta) {
  return libcrux_ml_kem_vector_portable_ntt_ntt_layer_3_step(a, zeta);
}

static KRML_MUSTINLINE void libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *vec,
    int16_t zeta, size_t i, size_t j) {
  int16_t a_minus_b = vec->elements[j] - vec->elements[i];
  int16_t a_plus_b = vec->elements[j] + vec->elements[i];
  int16_t o0 = libcrux_ml_kem_vector_portable_arithmetic_barrett_reduce_element(
      a_plus_b);
  int16_t o1 =
      libcrux_ml_kem_vector_portable_arithmetic_montgomery_multiply_fe_by_fer(
          a_minus_b, libcrux_secrets_int_public_integers_classify_27_39(zeta));
  vec->elements[i] = o0;
  vec->elements[j] = o1;
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_1_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec,
    int16_t zeta0, int16_t zeta1, int16_t zeta2, int16_t zeta3) {
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta0, (size_t)0U,
                                                  (size_t)2U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta0, (size_t)1U,
                                                  (size_t)3U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta1, (size_t)4U,
                                                  (size_t)6U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta1, (size_t)5U,
                                                  (size_t)7U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta2, (size_t)8U,
                                                  (size_t)10U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta2, (size_t)9U,
                                                  (size_t)11U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta3, (size_t)12U,
                                                  (size_t)14U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta3, (size_t)13U,
                                                  (size_t)15U);
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_inv_ntt_layer_1_step_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta0,
    int16_t zeta1, int16_t zeta2, int16_t zeta3) {
  return libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_1_step(
      a, zeta0, zeta1, zeta2, zeta3);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_2_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec,
    int16_t zeta0, int16_t zeta1) {
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta0, (size_t)0U,
                                                  (size_t)4U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta0, (size_t)1U,
                                                  (size_t)5U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta0, (size_t)2U,
                                                  (size_t)6U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta0, (size_t)3U,
                                                  (size_t)7U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta1, (size_t)8U,
                                                  (size_t)12U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta1, (size_t)9U,
                                                  (size_t)13U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta1, (size_t)10U,
                                                  (size_t)14U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta1, (size_t)11U,
                                                  (size_t)15U);
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_inv_ntt_layer_2_step_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta0,
    int16_t zeta1) {
  return libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_2_step(a, zeta0,
                                                                 zeta1);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_3_step(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vec,
    int16_t zeta) {
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta, (size_t)0U,
                                                  (size_t)8U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta, (size_t)1U,
                                                  (size_t)9U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta, (size_t)2U,
                                                  (size_t)10U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta, (size_t)3U,
                                                  (size_t)11U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta, (size_t)4U,
                                                  (size_t)12U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta, (size_t)5U,
                                                  (size_t)13U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta, (size_t)6U,
                                                  (size_t)14U);
  libcrux_ml_kem_vector_portable_ntt_inv_ntt_step(&vec, zeta, (size_t)7U,
                                                  (size_t)15U);
  return vec;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_inv_ntt_layer_3_step_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a, int16_t zeta) {
  return libcrux_ml_kem_vector_portable_ntt_inv_ntt_layer_3_step(a, zeta);
}

/**
 Compute the product of two Kyber binomials with respect to the
 modulus `X² - zeta`.

 This function almost implements <strong>Algorithm 11</strong> of the
 NIST FIPS 203 standard, which is reproduced below:

 ```plaintext
 Input:  a₀, a₁, b₀, b₁ ∈ ℤq.
 Input: γ ∈ ℤq.
 Output: c₀, c₁ ∈ ℤq.

 c₀ ← a₀·b₀ + a₁·b₁·γ
 c₁ ← a₀·b₁ + a₁·b₀
 return c₀, c₁
 ```
 We say "almost" because the coefficients output by this function are in
 the Montgomery domain (unlike in the specification).

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *a,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *b, int16_t zeta,
    size_t i, libcrux_ml_kem_vector_portable_vector_type_PortableVector *out) {
  int16_t ai = a->elements[(size_t)2U * i];
  int16_t bi = b->elements[(size_t)2U * i];
  int16_t aj = a->elements[(size_t)2U * i + (size_t)1U];
  int16_t bj = b->elements[(size_t)2U * i + (size_t)1U];
  int32_t ai_bi =
      libcrux_secrets_int_as_i32_f5(ai) * libcrux_secrets_int_as_i32_f5(bi);
  int32_t aj_bj_ =
      libcrux_secrets_int_as_i32_f5(aj) * libcrux_secrets_int_as_i32_f5(bj);
  int16_t aj_bj =
      libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
          aj_bj_);
  int32_t aj_bj_zeta = libcrux_secrets_int_as_i32_f5(aj_bj) *
                       libcrux_secrets_int_as_i32_f5(zeta);
  int32_t ai_bi_aj_bj = ai_bi + aj_bj_zeta;
  int16_t o0 =
      libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
          ai_bi_aj_bj);
  int32_t ai_bj =
      libcrux_secrets_int_as_i32_f5(ai) * libcrux_secrets_int_as_i32_f5(bj);
  int32_t aj_bi =
      libcrux_secrets_int_as_i32_f5(aj) * libcrux_secrets_int_as_i32_f5(bi);
  int32_t ai_bj_aj_bi = ai_bj + aj_bi;
  int16_t o1 =
      libcrux_ml_kem_vector_portable_arithmetic_montgomery_reduce_element(
          ai_bj_aj_bi);
  out->elements[(size_t)2U * i] = o0;
  out->elements[(size_t)2U * i + (size_t)1U] = o1;
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_ntt_multiply(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs,
    int16_t zeta0, int16_t zeta1, int16_t zeta2, int16_t zeta3) {
  int16_t nzeta0 = -zeta0;
  int16_t nzeta1 = -zeta1;
  int16_t nzeta2 = -zeta2;
  int16_t nzeta3 = -zeta3;
  libcrux_ml_kem_vector_portable_vector_type_PortableVector out =
      libcrux_ml_kem_vector_portable_vector_type_zero();
  libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
      lhs, rhs, libcrux_secrets_int_public_integers_classify_27_39(zeta0),
      (size_t)0U, &out);
  libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
      lhs, rhs, libcrux_secrets_int_public_integers_classify_27_39(nzeta0),
      (size_t)1U, &out);
  libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
      lhs, rhs, libcrux_secrets_int_public_integers_classify_27_39(zeta1),
      (size_t)2U, &out);
  libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
      lhs, rhs, libcrux_secrets_int_public_integers_classify_27_39(nzeta1),
      (size_t)3U, &out);
  libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
      lhs, rhs, libcrux_secrets_int_public_integers_classify_27_39(zeta2),
      (size_t)4U, &out);
  libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
      lhs, rhs, libcrux_secrets_int_public_integers_classify_27_39(nzeta2),
      (size_t)5U, &out);
  libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
      lhs, rhs, libcrux_secrets_int_public_integers_classify_27_39(zeta3),
      (size_t)6U, &out);
  libcrux_ml_kem_vector_portable_ntt_ntt_multiply_binomials(
      lhs, rhs, libcrux_secrets_int_public_integers_classify_27_39(nzeta3),
      (size_t)7U, &out);
  return out;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_ntt_multiply_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *lhs,
    libcrux_ml_kem_vector_portable_vector_type_PortableVector *rhs,
    int16_t zeta0, int16_t zeta1, int16_t zeta2, int16_t zeta3) {
  return libcrux_ml_kem_vector_portable_ntt_ntt_multiply(lhs, rhs, zeta0, zeta1,
                                                         zeta2, zeta3);
}

static KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[2U]) {
  uint8_t result0 =
      (((((((uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[0U]) |
            (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[1U]) << 1U) |
           (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[2U]) << 2U) |
          (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[3U]) << 3U) |
         (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[4U]) << 4U) |
        (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[5U]) << 5U) |
       (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[6U]) << 6U) |
      (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[7U]) << 7U;
  uint8_t result1 =
      (((((((uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[8U]) |
            (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[9U]) << 1U) |
           (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[10U]) << 2U) |
          (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[11U]) << 3U) |
         (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[12U]) << 4U) |
        (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[13U]) << 5U) |
       (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[14U]) << 6U) |
      (uint32_t)libcrux_secrets_int_as_u8_f5(v.elements[15U]) << 7U;
  ret[0U] = result0;
  ret[1U] = result1;
}

static inline void libcrux_ml_kem_vector_portable_serialize_1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[2U]) {
  uint8_t ret0[2U];
  libcrux_ml_kem_vector_portable_serialize_serialize_1(a, ret0);
  libcrux_secrets_int_public_integers_declassify_d8_d4(ret0, ret);
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline void libcrux_ml_kem_vector_portable_serialize_1_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[2U]) {
  libcrux_ml_kem_vector_portable_serialize_1(a, ret);
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_1(Eurydice_slice v) {
  int16_t result0 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)0U, uint8_t, uint8_t *) & 1U);
  int16_t result1 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)0U, uint8_t, uint8_t *) >> 1U &
      1U);
  int16_t result2 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)0U, uint8_t, uint8_t *) >> 2U &
      1U);
  int16_t result3 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)0U, uint8_t, uint8_t *) >> 3U &
      1U);
  int16_t result4 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)0U, uint8_t, uint8_t *) >> 4U &
      1U);
  int16_t result5 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)0U, uint8_t, uint8_t *) >> 5U &
      1U);
  int16_t result6 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)0U, uint8_t, uint8_t *) >> 6U &
      1U);
  int16_t result7 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)0U, uint8_t, uint8_t *) >> 7U &
      1U);
  int16_t result8 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)1U, uint8_t, uint8_t *) & 1U);
  int16_t result9 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)1U, uint8_t, uint8_t *) >> 1U &
      1U);
  int16_t result10 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)1U, uint8_t, uint8_t *) >> 2U &
      1U);
  int16_t result11 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)1U, uint8_t, uint8_t *) >> 3U &
      1U);
  int16_t result12 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)1U, uint8_t, uint8_t *) >> 4U &
      1U);
  int16_t result13 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)1U, uint8_t, uint8_t *) >> 5U &
      1U);
  int16_t result14 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)1U, uint8_t, uint8_t *) >> 6U &
      1U);
  int16_t result15 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(v, (size_t)1U, uint8_t, uint8_t *) >> 7U &
      1U);
  return (
      KRML_CLITERAL(libcrux_ml_kem_vector_portable_vector_type_PortableVector){
          .elements = {result0, result1, result2, result3, result4, result5,
                       result6, result7, result8, result9, result10, result11,
                       result12, result13, result14, result15}});
}

static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_1(Eurydice_slice a) {
  return libcrux_ml_kem_vector_portable_serialize_deserialize_1(
      libcrux_secrets_int_classify_public_classify_ref_9b_90(a));
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_1_b8(Eurydice_slice a) {
  return libcrux_ml_kem_vector_portable_deserialize_1(a);
}

typedef struct uint8_t_x4_s {
  uint8_t fst;
  uint8_t snd;
  uint8_t thd;
  uint8_t f3;
} uint8_t_x4;

static KRML_MUSTINLINE uint8_t_x4
libcrux_ml_kem_vector_portable_serialize_serialize_4_int(Eurydice_slice v) {
  uint8_t result0 = (uint32_t)libcrux_secrets_int_as_u8_f5(
                        Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *))
                        << 4U |
                    (uint32_t)libcrux_secrets_int_as_u8_f5(Eurydice_slice_index(
                        v, (size_t)0U, int16_t, int16_t *));
  uint8_t result1 = (uint32_t)libcrux_secrets_int_as_u8_f5(
                        Eurydice_slice_index(v, (size_t)3U, int16_t, int16_t *))
                        << 4U |
                    (uint32_t)libcrux_secrets_int_as_u8_f5(Eurydice_slice_index(
                        v, (size_t)2U, int16_t, int16_t *));
  uint8_t result2 = (uint32_t)libcrux_secrets_int_as_u8_f5(
                        Eurydice_slice_index(v, (size_t)5U, int16_t, int16_t *))
                        << 4U |
                    (uint32_t)libcrux_secrets_int_as_u8_f5(Eurydice_slice_index(
                        v, (size_t)4U, int16_t, int16_t *));
  uint8_t result3 = (uint32_t)libcrux_secrets_int_as_u8_f5(
                        Eurydice_slice_index(v, (size_t)7U, int16_t, int16_t *))
                        << 4U |
                    (uint32_t)libcrux_secrets_int_as_u8_f5(Eurydice_slice_index(
                        v, (size_t)6U, int16_t, int16_t *));
  return (KRML_CLITERAL(uint8_t_x4){
      .fst = result0, .snd = result1, .thd = result2, .f3 = result3});
}

static KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_4(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[8U]) {
  uint8_t_x4 result0_3 =
      libcrux_ml_kem_vector_portable_serialize_serialize_4_int(
          Eurydice_array_to_subslice3(v.elements, (size_t)0U, (size_t)8U,
                                      int16_t *));
  uint8_t_x4 result4_7 =
      libcrux_ml_kem_vector_portable_serialize_serialize_4_int(
          Eurydice_array_to_subslice3(v.elements, (size_t)8U, (size_t)16U,
                                      int16_t *));
  ret[0U] = result0_3.fst;
  ret[1U] = result0_3.snd;
  ret[2U] = result0_3.thd;
  ret[3U] = result0_3.f3;
  ret[4U] = result4_7.fst;
  ret[5U] = result4_7.snd;
  ret[6U] = result4_7.thd;
  ret[7U] = result4_7.f3;
}

static inline void libcrux_ml_kem_vector_portable_serialize_4(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[8U]) {
  uint8_t ret0[8U];
  libcrux_ml_kem_vector_portable_serialize_serialize_4(a, ret0);
  libcrux_secrets_int_public_integers_declassify_d8_76(ret0, ret);
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline void libcrux_ml_kem_vector_portable_serialize_4_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[8U]) {
  libcrux_ml_kem_vector_portable_serialize_4(a, ret);
}

static KRML_MUSTINLINE int16_t_x8
libcrux_ml_kem_vector_portable_serialize_deserialize_4_int(
    Eurydice_slice bytes) {
  int16_t v0 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(bytes, (size_t)0U, uint8_t, uint8_t *) &
      15U);
  int16_t v1 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(bytes, (size_t)0U, uint8_t, uint8_t *) >>
          4U &
      15U);
  int16_t v2 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *) &
      15U);
  int16_t v3 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *) >>
          4U &
      15U);
  int16_t v4 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *) &
      15U);
  int16_t v5 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *) >>
          4U &
      15U);
  int16_t v6 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(bytes, (size_t)3U, uint8_t, uint8_t *) &
      15U);
  int16_t v7 = libcrux_secrets_int_as_i16_59(
      (uint32_t)Eurydice_slice_index(bytes, (size_t)3U, uint8_t, uint8_t *) >>
          4U &
      15U);
  return (KRML_CLITERAL(int16_t_x8){.fst = v0,
                                    .snd = v1,
                                    .thd = v2,
                                    .f3 = v3,
                                    .f4 = v4,
                                    .f5 = v5,
                                    .f6 = v6,
                                    .f7 = v7});
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_4(Eurydice_slice bytes) {
  int16_t_x8 v0_7 = libcrux_ml_kem_vector_portable_serialize_deserialize_4_int(
      Eurydice_slice_subslice3(bytes, (size_t)0U, (size_t)4U, uint8_t *));
  int16_t_x8 v8_15 = libcrux_ml_kem_vector_portable_serialize_deserialize_4_int(
      Eurydice_slice_subslice3(bytes, (size_t)4U, (size_t)8U, uint8_t *));
  return (
      KRML_CLITERAL(libcrux_ml_kem_vector_portable_vector_type_PortableVector){
          .elements = {v0_7.fst, v0_7.snd, v0_7.thd, v0_7.f3, v0_7.f4, v0_7.f5,
                       v0_7.f6, v0_7.f7, v8_15.fst, v8_15.snd, v8_15.thd,
                       v8_15.f3, v8_15.f4, v8_15.f5, v8_15.f6, v8_15.f7}});
}

static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_4(Eurydice_slice a) {
  return libcrux_ml_kem_vector_portable_serialize_deserialize_4(
      libcrux_secrets_int_classify_public_classify_ref_9b_90(a));
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_4_b8(Eurydice_slice a) {
  return libcrux_ml_kem_vector_portable_deserialize_4(a);
}

typedef struct uint8_t_x5_s {
  uint8_t fst;
  uint8_t snd;
  uint8_t thd;
  uint8_t f3;
  uint8_t f4;
} uint8_t_x5;

static KRML_MUSTINLINE uint8_t_x5
libcrux_ml_kem_vector_portable_serialize_serialize_10_int(Eurydice_slice v) {
  uint8_t r0 = libcrux_secrets_int_as_u8_f5(
      Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *) & (int16_t)255);
  uint8_t r1 =
      (uint32_t)libcrux_secrets_int_as_u8_f5(
          Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *) & (int16_t)63)
          << 2U |
      (uint32_t)libcrux_secrets_int_as_u8_f5(
          Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *) >> 8U &
          (int16_t)3);
  uint8_t r2 =
      (uint32_t)libcrux_secrets_int_as_u8_f5(
          Eurydice_slice_index(v, (size_t)2U, int16_t, int16_t *) & (int16_t)15)
          << 4U |
      (uint32_t)libcrux_secrets_int_as_u8_f5(
          Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *) >> 6U &
          (int16_t)15);
  uint8_t r3 =
      (uint32_t)libcrux_secrets_int_as_u8_f5(
          Eurydice_slice_index(v, (size_t)3U, int16_t, int16_t *) & (int16_t)3)
          << 6U |
      (uint32_t)libcrux_secrets_int_as_u8_f5(
          Eurydice_slice_index(v, (size_t)2U, int16_t, int16_t *) >> 4U &
          (int16_t)63);
  uint8_t r4 = libcrux_secrets_int_as_u8_f5(
      Eurydice_slice_index(v, (size_t)3U, int16_t, int16_t *) >> 2U &
      (int16_t)255);
  return (KRML_CLITERAL(uint8_t_x5){
      .fst = r0, .snd = r1, .thd = r2, .f3 = r3, .f4 = r4});
}

static KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_10(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[20U]) {
  uint8_t_x5 r0_4 = libcrux_ml_kem_vector_portable_serialize_serialize_10_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)0U, (size_t)4U,
                                  int16_t *));
  uint8_t_x5 r5_9 = libcrux_ml_kem_vector_portable_serialize_serialize_10_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)4U, (size_t)8U,
                                  int16_t *));
  uint8_t_x5 r10_14 = libcrux_ml_kem_vector_portable_serialize_serialize_10_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)8U, (size_t)12U,
                                  int16_t *));
  uint8_t_x5 r15_19 = libcrux_ml_kem_vector_portable_serialize_serialize_10_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)12U, (size_t)16U,
                                  int16_t *));
  ret[0U] = r0_4.fst;
  ret[1U] = r0_4.snd;
  ret[2U] = r0_4.thd;
  ret[3U] = r0_4.f3;
  ret[4U] = r0_4.f4;
  ret[5U] = r5_9.fst;
  ret[6U] = r5_9.snd;
  ret[7U] = r5_9.thd;
  ret[8U] = r5_9.f3;
  ret[9U] = r5_9.f4;
  ret[10U] = r10_14.fst;
  ret[11U] = r10_14.snd;
  ret[12U] = r10_14.thd;
  ret[13U] = r10_14.f3;
  ret[14U] = r10_14.f4;
  ret[15U] = r15_19.fst;
  ret[16U] = r15_19.snd;
  ret[17U] = r15_19.thd;
  ret[18U] = r15_19.f3;
  ret[19U] = r15_19.f4;
}

static inline void libcrux_ml_kem_vector_portable_serialize_10(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[20U]) {
  uint8_t ret0[20U];
  libcrux_ml_kem_vector_portable_serialize_serialize_10(a, ret0);
  libcrux_secrets_int_public_integers_declassify_d8_57(ret0, ret);
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline void libcrux_ml_kem_vector_portable_serialize_10_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[20U]) {
  libcrux_ml_kem_vector_portable_serialize_10(a, ret);
}

static KRML_MUSTINLINE int16_t_x8
libcrux_ml_kem_vector_portable_serialize_deserialize_10_int(
    Eurydice_slice bytes) {
  int16_t r0 = libcrux_secrets_int_as_i16_f5(
      (libcrux_secrets_int_as_i16_59(
           Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *)) &
       (int16_t)3)
          << 8U |
      (libcrux_secrets_int_as_i16_59(
           Eurydice_slice_index(bytes, (size_t)0U, uint8_t, uint8_t *)) &
       (int16_t)255));
  int16_t r1 = libcrux_secrets_int_as_i16_f5(
      (libcrux_secrets_int_as_i16_59(
           Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *)) &
       (int16_t)15)
          << 6U |
      libcrux_secrets_int_as_i16_59(
          Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *)) >>
          2U);
  int16_t r2 = libcrux_secrets_int_as_i16_f5(
      (libcrux_secrets_int_as_i16_59(
           Eurydice_slice_index(bytes, (size_t)3U, uint8_t, uint8_t *)) &
       (int16_t)63)
          << 4U |
      libcrux_secrets_int_as_i16_59(
          Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *)) >>
          4U);
  int16_t r3 = libcrux_secrets_int_as_i16_f5(
      libcrux_secrets_int_as_i16_59(
          Eurydice_slice_index(bytes, (size_t)4U, uint8_t, uint8_t *))
          << 2U |
      libcrux_secrets_int_as_i16_59(
          Eurydice_slice_index(bytes, (size_t)3U, uint8_t, uint8_t *)) >>
          6U);
  int16_t r4 = libcrux_secrets_int_as_i16_f5(
      (libcrux_secrets_int_as_i16_59(
           Eurydice_slice_index(bytes, (size_t)6U, uint8_t, uint8_t *)) &
       (int16_t)3)
          << 8U |
      (libcrux_secrets_int_as_i16_59(
           Eurydice_slice_index(bytes, (size_t)5U, uint8_t, uint8_t *)) &
       (int16_t)255));
  int16_t r5 = libcrux_secrets_int_as_i16_f5(
      (libcrux_secrets_int_as_i16_59(
           Eurydice_slice_index(bytes, (size_t)7U, uint8_t, uint8_t *)) &
       (int16_t)15)
          << 6U |
      libcrux_secrets_int_as_i16_59(
          Eurydice_slice_index(bytes, (size_t)6U, uint8_t, uint8_t *)) >>
          2U);
  int16_t r6 = libcrux_secrets_int_as_i16_f5(
      (libcrux_secrets_int_as_i16_59(
           Eurydice_slice_index(bytes, (size_t)8U, uint8_t, uint8_t *)) &
       (int16_t)63)
          << 4U |
      libcrux_secrets_int_as_i16_59(
          Eurydice_slice_index(bytes, (size_t)7U, uint8_t, uint8_t *)) >>
          4U);
  int16_t r7 = libcrux_secrets_int_as_i16_f5(
      libcrux_secrets_int_as_i16_59(
          Eurydice_slice_index(bytes, (size_t)9U, uint8_t, uint8_t *))
          << 2U |
      libcrux_secrets_int_as_i16_59(
          Eurydice_slice_index(bytes, (size_t)8U, uint8_t, uint8_t *)) >>
          6U);
  return (KRML_CLITERAL(int16_t_x8){.fst = r0,
                                    .snd = r1,
                                    .thd = r2,
                                    .f3 = r3,
                                    .f4 = r4,
                                    .f5 = r5,
                                    .f6 = r6,
                                    .f7 = r7});
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_10(Eurydice_slice bytes) {
  int16_t_x8 v0_7 = libcrux_ml_kem_vector_portable_serialize_deserialize_10_int(
      Eurydice_slice_subslice3(bytes, (size_t)0U, (size_t)10U, uint8_t *));
  int16_t_x8 v8_15 =
      libcrux_ml_kem_vector_portable_serialize_deserialize_10_int(
          Eurydice_slice_subslice3(bytes, (size_t)10U, (size_t)20U, uint8_t *));
  return (
      KRML_CLITERAL(libcrux_ml_kem_vector_portable_vector_type_PortableVector){
          .elements = {v0_7.fst, v0_7.snd, v0_7.thd, v0_7.f3, v0_7.f4, v0_7.f5,
                       v0_7.f6, v0_7.f7, v8_15.fst, v8_15.snd, v8_15.thd,
                       v8_15.f3, v8_15.f4, v8_15.f5, v8_15.f6, v8_15.f7}});
}

static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_10(Eurydice_slice a) {
  return libcrux_ml_kem_vector_portable_serialize_deserialize_10(
      libcrux_secrets_int_classify_public_classify_ref_9b_90(a));
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_10_b8(Eurydice_slice a) {
  return libcrux_ml_kem_vector_portable_deserialize_10(a);
}

typedef struct uint8_t_x3_s {
  uint8_t fst;
  uint8_t snd;
  uint8_t thd;
} uint8_t_x3;

static KRML_MUSTINLINE uint8_t_x3
libcrux_ml_kem_vector_portable_serialize_serialize_12_int(Eurydice_slice v) {
  uint8_t r0 = libcrux_secrets_int_as_u8_f5(
      Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *) & (int16_t)255);
  uint8_t r1 = libcrux_secrets_int_as_u8_f5(
      Eurydice_slice_index(v, (size_t)0U, int16_t, int16_t *) >> 8U |
      (Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *) & (int16_t)15)
          << 4U);
  uint8_t r2 = libcrux_secrets_int_as_u8_f5(
      Eurydice_slice_index(v, (size_t)1U, int16_t, int16_t *) >> 4U &
      (int16_t)255);
  return (KRML_CLITERAL(uint8_t_x3){.fst = r0, .snd = r1, .thd = r2});
}

static KRML_MUSTINLINE void
libcrux_ml_kem_vector_portable_serialize_serialize_12(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector v,
    uint8_t ret[24U]) {
  uint8_t_x3 r0_2 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)0U, (size_t)2U,
                                  int16_t *));
  uint8_t_x3 r3_5 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)2U, (size_t)4U,
                                  int16_t *));
  uint8_t_x3 r6_8 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)4U, (size_t)6U,
                                  int16_t *));
  uint8_t_x3 r9_11 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)6U, (size_t)8U,
                                  int16_t *));
  uint8_t_x3 r12_14 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)8U, (size_t)10U,
                                  int16_t *));
  uint8_t_x3 r15_17 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)10U, (size_t)12U,
                                  int16_t *));
  uint8_t_x3 r18_20 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)12U, (size_t)14U,
                                  int16_t *));
  uint8_t_x3 r21_23 = libcrux_ml_kem_vector_portable_serialize_serialize_12_int(
      Eurydice_array_to_subslice3(v.elements, (size_t)14U, (size_t)16U,
                                  int16_t *));
  ret[0U] = r0_2.fst;
  ret[1U] = r0_2.snd;
  ret[2U] = r0_2.thd;
  ret[3U] = r3_5.fst;
  ret[4U] = r3_5.snd;
  ret[5U] = r3_5.thd;
  ret[6U] = r6_8.fst;
  ret[7U] = r6_8.snd;
  ret[8U] = r6_8.thd;
  ret[9U] = r9_11.fst;
  ret[10U] = r9_11.snd;
  ret[11U] = r9_11.thd;
  ret[12U] = r12_14.fst;
  ret[13U] = r12_14.snd;
  ret[14U] = r12_14.thd;
  ret[15U] = r15_17.fst;
  ret[16U] = r15_17.snd;
  ret[17U] = r15_17.thd;
  ret[18U] = r18_20.fst;
  ret[19U] = r18_20.snd;
  ret[20U] = r18_20.thd;
  ret[21U] = r21_23.fst;
  ret[22U] = r21_23.snd;
  ret[23U] = r21_23.thd;
}

static inline void libcrux_ml_kem_vector_portable_serialize_12(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[24U]) {
  uint8_t ret0[24U];
  libcrux_ml_kem_vector_portable_serialize_serialize_12(a, ret0);
  libcrux_secrets_int_public_integers_declassify_d8_d2(ret0, ret);
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline void libcrux_ml_kem_vector_portable_serialize_12_b8(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
    uint8_t ret[24U]) {
  libcrux_ml_kem_vector_portable_serialize_12(a, ret);
}

typedef struct int16_t_x2_s {
  int16_t fst;
  int16_t snd;
} int16_t_x2;

static KRML_MUSTINLINE int16_t_x2
libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
    Eurydice_slice bytes) {
  int16_t byte0 = libcrux_secrets_int_as_i16_59(
      Eurydice_slice_index(bytes, (size_t)0U, uint8_t, uint8_t *));
  int16_t byte1 = libcrux_secrets_int_as_i16_59(
      Eurydice_slice_index(bytes, (size_t)1U, uint8_t, uint8_t *));
  int16_t byte2 = libcrux_secrets_int_as_i16_59(
      Eurydice_slice_index(bytes, (size_t)2U, uint8_t, uint8_t *));
  int16_t r0 = (byte1 & (int16_t)15) << 8U | (byte0 & (int16_t)255);
  int16_t r1 = byte2 << 4U | (byte1 >> 4U & (int16_t)15);
  return (KRML_CLITERAL(int16_t_x2){.fst = r0, .snd = r1});
}

static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_serialize_deserialize_12(Eurydice_slice bytes) {
  int16_t_x2 v0_1 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
      Eurydice_slice_subslice3(bytes, (size_t)0U, (size_t)3U, uint8_t *));
  int16_t_x2 v2_3 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
      Eurydice_slice_subslice3(bytes, (size_t)3U, (size_t)6U, uint8_t *));
  int16_t_x2 v4_5 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
      Eurydice_slice_subslice3(bytes, (size_t)6U, (size_t)9U, uint8_t *));
  int16_t_x2 v6_7 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
      Eurydice_slice_subslice3(bytes, (size_t)9U, (size_t)12U, uint8_t *));
  int16_t_x2 v8_9 = libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
      Eurydice_slice_subslice3(bytes, (size_t)12U, (size_t)15U, uint8_t *));
  int16_t_x2 v10_11 =
      libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
          Eurydice_slice_subslice3(bytes, (size_t)15U, (size_t)18U, uint8_t *));
  int16_t_x2 v12_13 =
      libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
          Eurydice_slice_subslice3(bytes, (size_t)18U, (size_t)21U, uint8_t *));
  int16_t_x2 v14_15 =
      libcrux_ml_kem_vector_portable_serialize_deserialize_12_int(
          Eurydice_slice_subslice3(bytes, (size_t)21U, (size_t)24U, uint8_t *));
  return (
      KRML_CLITERAL(libcrux_ml_kem_vector_portable_vector_type_PortableVector){
          .elements = {v0_1.fst, v0_1.snd, v2_3.fst, v2_3.snd, v4_5.fst,
                       v4_5.snd, v6_7.fst, v6_7.snd, v8_9.fst, v8_9.snd,
                       v10_11.fst, v10_11.snd, v12_13.fst, v12_13.snd,
                       v14_15.fst, v14_15.snd}});
}

static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_12(Eurydice_slice a) {
  return libcrux_ml_kem_vector_portable_serialize_deserialize_12(
      libcrux_secrets_int_classify_public_classify_ref_9b_90(a));
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_deserialize_12_b8(Eurydice_slice a) {
  return libcrux_ml_kem_vector_portable_deserialize_12(a);
}

static KRML_MUSTINLINE size_t
libcrux_ml_kem_vector_portable_sampling_rej_sample(Eurydice_slice a,
                                                   Eurydice_slice result) {
  size_t sampled = (size_t)0U;
  for (size_t i = (size_t)0U; i < Eurydice_slice_len(a, uint8_t) / (size_t)3U;
       i++) {
    size_t i0 = i;
    int16_t b1 = (int16_t)Eurydice_slice_index(a, i0 * (size_t)3U + (size_t)0U,
                                               uint8_t, uint8_t *);
    int16_t b2 = (int16_t)Eurydice_slice_index(a, i0 * (size_t)3U + (size_t)1U,
                                               uint8_t, uint8_t *);
    int16_t b3 = (int16_t)Eurydice_slice_index(a, i0 * (size_t)3U + (size_t)2U,
                                               uint8_t, uint8_t *);
    int16_t d1 = (b2 & (int16_t)15) << 8U | b1;
    int16_t d2 = b3 << 4U | b2 >> 4U;
    if (d1 < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS) {
      if (sampled < (size_t)16U) {
        Eurydice_slice_index(result, sampled, int16_t, int16_t *) = d1;
        sampled++;
      }
    }
    if (d2 < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS) {
      if (sampled < (size_t)16U) {
        Eurydice_slice_index(result, sampled, int16_t, int16_t *) = d2;
        sampled++;
      }
    }
  }
  return sampled;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
static inline size_t libcrux_ml_kem_vector_portable_rej_sample_b8(
    Eurydice_slice a, Eurydice_slice out) {
  return libcrux_ml_kem_vector_portable_sampling_rej_sample(a, out);
}

#define LIBCRUX_ML_KEM_MLKEM768_VECTOR_U_COMPRESSION_FACTOR ((size_t)10U)

#define LIBCRUX_ML_KEM_MLKEM768_C1_BLOCK_SIZE              \
  (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT * \
   LIBCRUX_ML_KEM_MLKEM768_VECTOR_U_COMPRESSION_FACTOR / (size_t)8U)

#define LIBCRUX_ML_KEM_MLKEM768_RANK ((size_t)3U)

#define LIBCRUX_ML_KEM_MLKEM768_C1_SIZE \
  (LIBCRUX_ML_KEM_MLKEM768_C1_BLOCK_SIZE * LIBCRUX_ML_KEM_MLKEM768_RANK)

#define LIBCRUX_ML_KEM_MLKEM768_VECTOR_V_COMPRESSION_FACTOR ((size_t)4U)

#define LIBCRUX_ML_KEM_MLKEM768_C2_SIZE                    \
  (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT * \
   LIBCRUX_ML_KEM_MLKEM768_VECTOR_V_COMPRESSION_FACTOR / (size_t)8U)

#define LIBCRUX_ML_KEM_MLKEM768_CPA_PKE_CIPHERTEXT_SIZE \
  (LIBCRUX_ML_KEM_MLKEM768_C1_SIZE + LIBCRUX_ML_KEM_MLKEM768_C2_SIZE)

#define LIBCRUX_ML_KEM_MLKEM768_T_AS_NTT_ENCODED_SIZE      \
  (LIBCRUX_ML_KEM_MLKEM768_RANK *                          \
   LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT * \
   LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_COEFFICIENT / (size_t)8U)

#define LIBCRUX_ML_KEM_MLKEM768_CPA_PKE_PUBLIC_KEY_SIZE \
  (LIBCRUX_ML_KEM_MLKEM768_T_AS_NTT_ENCODED_SIZE + (size_t)32U)

#define LIBCRUX_ML_KEM_MLKEM768_CPA_PKE_SECRET_KEY_SIZE    \
  (LIBCRUX_ML_KEM_MLKEM768_RANK *                          \
   LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT * \
   LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_COEFFICIENT / (size_t)8U)

#define LIBCRUX_ML_KEM_MLKEM768_ETA1 ((size_t)2U)

#define LIBCRUX_ML_KEM_MLKEM768_ETA1_RANDOMNESS_SIZE \
  (LIBCRUX_ML_KEM_MLKEM768_ETA1 * (size_t)64U)

#define LIBCRUX_ML_KEM_MLKEM768_ETA2 ((size_t)2U)

#define LIBCRUX_ML_KEM_MLKEM768_ETA2_RANDOMNESS_SIZE \
  (LIBCRUX_ML_KEM_MLKEM768_ETA2 * (size_t)64U)

#define LIBCRUX_ML_KEM_MLKEM768_IMPLICIT_REJECTION_HASH_INPUT_SIZE \
  (LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE +                   \
   LIBCRUX_ML_KEM_MLKEM768_CPA_PKE_CIPHERTEXT_SIZE)

typedef libcrux_ml_kem_types_MlKemPrivateKey_d9
    libcrux_ml_kem_mlkem768_MlKem768PrivateKey;

typedef libcrux_ml_kem_types_MlKemPublicKey_30
    libcrux_ml_kem_mlkem768_MlKem768PublicKey;

#define LIBCRUX_ML_KEM_MLKEM768_RANKED_BYTES_PER_RING_ELEMENT \
  (LIBCRUX_ML_KEM_MLKEM768_RANK *                             \
   LIBCRUX_ML_KEM_CONSTANTS_BITS_PER_RING_ELEMENT / (size_t)8U)

#define LIBCRUX_ML_KEM_MLKEM768_SECRET_KEY_SIZE      \
  (LIBCRUX_ML_KEM_MLKEM768_CPA_PKE_SECRET_KEY_SIZE + \
   LIBCRUX_ML_KEM_MLKEM768_CPA_PKE_PUBLIC_KEY_SIZE + \
   LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE +          \
   LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE)

/**
A monomorphic instance of libcrux_ml_kem.polynomial.PolynomialRingElement
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector

*/
typedef struct libcrux_ml_kem_polynomial_PolynomialRingElement_1d_s {
  libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficients[16U];
} libcrux_ml_kem_polynomial_PolynomialRingElement_1d;

/**
A monomorphic instance of
libcrux_ml_kem.ind_cpa.unpacked.IndCpaPrivateKeyUnpacked with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- $3size_t
*/
typedef struct libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0_s {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d secret_as_ntt[3U];
} libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0;

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.ZERO_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_ZERO_d6_ea(void) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d lit;
  libcrux_ml_kem_vector_portable_vector_type_PortableVector
      repeat_expression[16U];
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    repeat_expression[i] = libcrux_ml_kem_vector_portable_ZERO_b8();
  }
  memcpy(lit.coefficients, repeat_expression,
         (size_t)16U *
             sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector));
  return lit;
}

/**
This function found in impl {core::ops::function::FnMut<(usize),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]> for libcrux_ml_kem::ind_cpa::decrypt::closure<Vector, K,
CIPHERTEXT_SIZE, VECTOR_U_ENCODED_SIZE, U_COMPRESSION_FACTOR,
V_COMPRESSION_FACTOR>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt.call_mut_0b
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- VECTOR_U_ENCODED_SIZE= 960
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_ind_cpa_decrypt_call_mut_0b_42(void **_, size_t tupled_args) {
  return libcrux_ml_kem_polynomial_ZERO_d6_ea();
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_to_uncompressed_ring_element with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_serialize_deserialize_to_uncompressed_ring_element_ea(
    Eurydice_slice serialized) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d re =
      libcrux_ml_kem_polynomial_ZERO_d6_ea();
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(serialized, uint8_t) / (size_t)24U; i++) {
    size_t i0 = i;
    Eurydice_slice bytes =
        Eurydice_slice_subslice3(serialized, i0 * (size_t)24U,
                                 i0 * (size_t)24U + (size_t)24U, uint8_t *);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_deserialize_12_b8(bytes);
    re.coefficients[i0] = uu____0;
  }
  return re;
}

/**
 Call [`deserialize_to_uncompressed_ring_element`] for each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.deserialize_vector
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_deserialize_vector_1b(
    Eurydice_slice secret_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *secret_as_ntt) {
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0 =
        libcrux_ml_kem_serialize_deserialize_to_uncompressed_ring_element_ea(
            Eurydice_slice_subslice3(
                secret_key,
                i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
                (i0 + (size_t)1U) *
                    LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
                uint8_t *));
    secret_as_ntt[i0] = uu____0;
  }
}

/**
This function found in impl {core::ops::function::FnMut<(usize),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]> for
libcrux_ml_kem::ind_cpa::deserialize_then_decompress_u::closure<Vector, K,
CIPHERTEXT_SIZE, U_COMPRESSION_FACTOR>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cpa.deserialize_then_decompress_u.call_mut_35 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- U_COMPRESSION_FACTOR= 10
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_ind_cpa_deserialize_then_decompress_u_call_mut_35_6c(
    void **_, size_t tupled_args) {
  return libcrux_ml_kem_polynomial_ZERO_d6_ea();
}

/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.compress.decompress_ciphertext_coefficient with
const generics
- COEFFICIENT_BITS= 10
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_decompress_ciphertext_coefficient_ef(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    int32_t decompressed =
        libcrux_secrets_int_as_i32_f5(a.elements[i0]) *
        libcrux_secrets_int_as_i32_f5(
            libcrux_secrets_int_public_integers_classify_27_39(
                LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS));
    decompressed = (decompressed << 1U) + ((int32_t)1 << (uint32_t)(int32_t)10);
    decompressed = decompressed >> (uint32_t)((int32_t)10 + (int32_t)1);
    a.elements[i0] = libcrux_secrets_int_as_i16_36(decompressed);
  }
  return a;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.decompress_ciphertext_coefficient_b8 with const
generics
- COEFFICIENT_BITS= 10
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_decompress_ciphertext_coefficient_b8_ef(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  return libcrux_ml_kem_vector_portable_compress_decompress_ciphertext_coefficient_ef(
      a);
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_10 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_serialize_deserialize_then_decompress_10_ea(
    Eurydice_slice serialized) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d re =
      libcrux_ml_kem_polynomial_ZERO_d6_ea();
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(serialized, uint8_t) / (size_t)20U; i++) {
    size_t i0 = i;
    Eurydice_slice bytes =
        Eurydice_slice_subslice3(serialized, i0 * (size_t)20U,
                                 i0 * (size_t)20U + (size_t)20U, uint8_t *);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
        libcrux_ml_kem_vector_portable_deserialize_10_b8(bytes);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_decompress_ciphertext_coefficient_b8_ef(
            coefficient);
    re.coefficients[i0] = uu____0;
  }
  return re;
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_ring_element_u with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 10
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_serialize_deserialize_then_decompress_ring_element_u_0a(
    Eurydice_slice serialized) {
  return libcrux_ml_kem_serialize_deserialize_then_decompress_10_ea(serialized);
}

typedef struct libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2_s {
  libcrux_ml_kem_vector_portable_vector_type_PortableVector fst;
  libcrux_ml_kem_vector_portable_vector_type_PortableVector snd;
} libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2;

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_layer_int_vec_step
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE
    libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2
    libcrux_ml_kem_ntt_ntt_layer_int_vec_step_ea(
        libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
        libcrux_ml_kem_vector_portable_vector_type_PortableVector b,
        int16_t zeta_r) {
  libcrux_ml_kem_vector_portable_vector_type_PortableVector t =
      libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_b8(b,
                                                                        zeta_r);
  b = libcrux_ml_kem_vector_portable_sub_b8(a, &t);
  a = libcrux_ml_kem_vector_portable_add_b8(a, &t);
  return (KRML_CLITERAL(
      libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2){.fst = a,
                                                                    .snd = b});
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_4_plus
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_ntt_ntt_at_layer_4_plus_ea(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re,
    size_t layer, size_t _initial_coefficient_bound) {
  size_t step = (size_t)1U << (uint32_t)layer;
  for (size_t i0 = (size_t)0U; i0 < (size_t)128U >> (uint32_t)layer; i0++) {
    size_t round = i0;
    zeta_i[0U] = zeta_i[0U] + (size_t)1U;
    size_t offset = round * step * (size_t)2U;
    size_t offset_vec = offset / (size_t)16U;
    size_t step_vec = step / (size_t)16U;
    for (size_t i = offset_vec; i < offset_vec + step_vec; i++) {
      size_t j = i;
      libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2 uu____0 =
          libcrux_ml_kem_ntt_ntt_layer_int_vec_step_ea(
              re->coefficients[j], re->coefficients[j + step_vec],
              libcrux_ml_kem_polynomial_zeta(zeta_i[0U]));
      libcrux_ml_kem_vector_portable_vector_type_PortableVector x = uu____0.fst;
      libcrux_ml_kem_vector_portable_vector_type_PortableVector y = uu____0.snd;
      re->coefficients[j] = x;
      re->coefficients[j + step_vec] = y;
    }
  }
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_3
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_ntt_ntt_at_layer_3_ea(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re,
    size_t _initial_coefficient_bound) {
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    size_t round = i;
    zeta_i[0U] = zeta_i[0U] + (size_t)1U;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_ntt_layer_3_step_b8(
            re->coefficients[round],
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U]));
    re->coefficients[round] = uu____0;
  }
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_2
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_ntt_ntt_at_layer_2_ea(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re,
    size_t _initial_coefficient_bound) {
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    size_t round = i;
    zeta_i[0U] = zeta_i[0U] + (size_t)1U;
    re->coefficients[round] =
        libcrux_ml_kem_vector_portable_ntt_layer_2_step_b8(
            re->coefficients[round], libcrux_ml_kem_polynomial_zeta(zeta_i[0U]),
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U] + (size_t)1U));
    zeta_i[0U] = zeta_i[0U] + (size_t)1U;
  }
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_1
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_ntt_ntt_at_layer_1_ea(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re,
    size_t _initial_coefficient_bound) {
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    size_t round = i;
    zeta_i[0U] = zeta_i[0U] + (size_t)1U;
    re->coefficients[round] =
        libcrux_ml_kem_vector_portable_ntt_layer_1_step_b8(
            re->coefficients[round], libcrux_ml_kem_polynomial_zeta(zeta_i[0U]),
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U] + (size_t)1U),
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U] + (size_t)2U),
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U] + (size_t)3U));
    zeta_i[0U] = zeta_i[0U] + (size_t)3U;
  }
}

/**
A monomorphic instance of libcrux_ml_kem.polynomial.poly_barrett_reduce
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_polynomial_poly_barrett_reduce_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *myself) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_barrett_reduce_b8(
            myself->coefficients[i0]);
    myself->coefficients[i0] = uu____0;
  }
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.poly_barrett_reduce_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_polynomial_poly_barrett_reduce_d6_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *self) {
  libcrux_ml_kem_polynomial_poly_barrett_reduce_ea(self);
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_vector_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- VECTOR_U_COMPRESSION_FACTOR= 10
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ntt_ntt_vector_u_0a(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re) {
  size_t zeta_i = (size_t)0U;
  libcrux_ml_kem_ntt_ntt_at_layer_4_plus_ea(&zeta_i, re, (size_t)7U,
                                            (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_4_plus_ea(&zeta_i, re, (size_t)6U,
                                            (size_t)2U * (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_4_plus_ea(&zeta_i, re, (size_t)5U,
                                            (size_t)3U * (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_4_plus_ea(&zeta_i, re, (size_t)4U,
                                            (size_t)4U * (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_3_ea(&zeta_i, re, (size_t)5U * (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_2_ea(&zeta_i, re, (size_t)6U * (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_1_ea(&zeta_i, re, (size_t)7U * (size_t)3328U);
  libcrux_ml_kem_polynomial_poly_barrett_reduce_d6_ea(re);
}

/**
 Call [`deserialize_then_decompress_ring_element_u`] on each ring element
 in the `ciphertext`.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.deserialize_then_decompress_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- U_COMPRESSION_FACTOR= 10
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cpa_deserialize_then_decompress_u_6c(
    uint8_t *ciphertext,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret[3U]) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d u_as_ntt[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    u_as_ntt[i] =
        libcrux_ml_kem_ind_cpa_deserialize_then_decompress_u_call_mut_35_6c(
            &lvalue, i);
  }
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(
               Eurydice_array_to_slice((size_t)1088U, ciphertext, uint8_t),
               uint8_t) /
               (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                (size_t)10U / (size_t)8U);
       i++) {
    size_t i0 = i;
    Eurydice_slice u_bytes = Eurydice_array_to_subslice3(
        ciphertext,
        i0 * (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
              (size_t)10U / (size_t)8U),
        i0 * (LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
              (size_t)10U / (size_t)8U) +
            LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT *
                (size_t)10U / (size_t)8U,
        uint8_t *);
    u_as_ntt[i0] =
        libcrux_ml_kem_serialize_deserialize_then_decompress_ring_element_u_0a(
            u_bytes);
    libcrux_ml_kem_ntt_ntt_vector_u_0a(&u_as_ntt[i0]);
  }
  memcpy(
      ret, u_as_ntt,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
}

/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.compress.decompress_ciphertext_coefficient with
const generics
- COEFFICIENT_BITS= 4
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_decompress_ciphertext_coefficient_d1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    int32_t decompressed =
        libcrux_secrets_int_as_i32_f5(a.elements[i0]) *
        libcrux_secrets_int_as_i32_f5(
            libcrux_secrets_int_public_integers_classify_27_39(
                LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_MODULUS));
    decompressed = (decompressed << 1U) + ((int32_t)1 << (uint32_t)(int32_t)4);
    decompressed = decompressed >> (uint32_t)((int32_t)4 + (int32_t)1);
    a.elements[i0] = libcrux_secrets_int_as_i16_36(decompressed);
  }
  return a;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
/**
A monomorphic instance of
libcrux_ml_kem.vector.portable.decompress_ciphertext_coefficient_b8 with const
generics
- COEFFICIENT_BITS= 4
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_decompress_ciphertext_coefficient_b8_d1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  return libcrux_ml_kem_vector_portable_compress_decompress_ciphertext_coefficient_d1(
      a);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.deserialize_then_decompress_4
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_serialize_deserialize_then_decompress_4_ea(
    Eurydice_slice serialized) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d re =
      libcrux_ml_kem_polynomial_ZERO_d6_ea();
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(serialized, uint8_t) / (size_t)8U; i++) {
    size_t i0 = i;
    Eurydice_slice bytes = Eurydice_slice_subslice3(
        serialized, i0 * (size_t)8U, i0 * (size_t)8U + (size_t)8U, uint8_t *);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
        libcrux_ml_kem_vector_portable_deserialize_4_b8(bytes);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_decompress_ciphertext_coefficient_b8_d1(
            coefficient);
    re.coefficients[i0] = uu____0;
  }
  return re;
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_ring_element_v with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- COMPRESSION_FACTOR= 4
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_serialize_deserialize_then_decompress_ring_element_v_89(
    Eurydice_slice serialized) {
  return libcrux_ml_kem_serialize_deserialize_then_decompress_4_ea(serialized);
}

/**
A monomorphic instance of libcrux_ml_kem.polynomial.ZERO
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_ZERO_ea(void) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d lit;
  libcrux_ml_kem_vector_portable_vector_type_PortableVector
      repeat_expression[16U];
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    repeat_expression[i] = libcrux_ml_kem_vector_portable_ZERO_b8();
  }
  memcpy(lit.coefficients, repeat_expression,
         (size_t)16U *
             sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector));
  return lit;
}

/**
 Given two `KyberPolynomialRingElement`s in their NTT representations,
 compute their product. Given two polynomials in the NTT domain `f^` and `ĵ`,
 the `iᵗʰ` coefficient of the product `k̂` is determined by the calculation:

 ```plaintext
 ĥ[2·i] + ĥ[2·i + 1]X = (f^[2·i] + f^[2·i + 1]X)·(ĝ[2·i] + ĝ[2·i + 1]X) mod (X²
 - ζ^(2·BitRev₇(i) + 1))
 ```

 This function almost implements <strong>Algorithm 10</strong> of the
 NIST FIPS 203 standard, which is reproduced below:

 ```plaintext
 Input: Two arrays fˆ ∈ ℤ₂₅₆ and ĝ ∈ ℤ₂₅₆.
 Output: An array ĥ ∈ ℤq.

 for(i ← 0; i < 128; i++)
     (ĥ[2i], ĥ[2i+1]) ← BaseCaseMultiply(fˆ[2i], fˆ[2i+1], ĝ[2i], ĝ[2i+1],
 ζ^(2·BitRev₇(i) + 1)) end for return ĥ
 ```
 We say "almost" because the coefficients of the ring element output by
 this function are in the Montgomery domain.

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.ntt_multiply
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_ntt_multiply_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *myself,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *rhs) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d out =
      libcrux_ml_kem_polynomial_ZERO_ea();
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_ntt_multiply_b8(
            &myself->coefficients[i0], &rhs->coefficients[i0],
            libcrux_ml_kem_polynomial_zeta((size_t)64U + (size_t)4U * i0),
            libcrux_ml_kem_polynomial_zeta((size_t)64U + (size_t)4U * i0 +
                                           (size_t)1U),
            libcrux_ml_kem_polynomial_zeta((size_t)64U + (size_t)4U * i0 +
                                           (size_t)2U),
            libcrux_ml_kem_polynomial_zeta((size_t)64U + (size_t)4U * i0 +
                                           (size_t)3U));
    out.coefficients[i0] = uu____0;
  }
  return out;
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.ntt_multiply_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_ntt_multiply_d6_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *rhs) {
  return libcrux_ml_kem_polynomial_ntt_multiply_ea(self, rhs);
}

/**
 Given two polynomial ring elements `lhs` and `rhs`, compute the pointwise
 sum of their constituent coefficients.
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_to_ring_element
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_polynomial_add_to_ring_element_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *myself,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *rhs) {
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(
               Eurydice_array_to_slice(
                   (size_t)16U, myself->coefficients,
                   libcrux_ml_kem_vector_portable_vector_type_PortableVector),
               libcrux_ml_kem_vector_portable_vector_type_PortableVector);
       i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_add_b8(myself->coefficients[i0],
                                              &rhs->coefficients[i0]);
    myself->coefficients[i0] = uu____0;
  }
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_to_ring_element_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_polynomial_add_to_ring_element_d6_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *rhs) {
  libcrux_ml_kem_polynomial_add_to_ring_element_1b(self, rhs);
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_at_layer_1
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_1_ea(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re) {
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    size_t round = i;
    zeta_i[0U] = zeta_i[0U] - (size_t)1U;
    re->coefficients[round] =
        libcrux_ml_kem_vector_portable_inv_ntt_layer_1_step_b8(
            re->coefficients[round], libcrux_ml_kem_polynomial_zeta(zeta_i[0U]),
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U] - (size_t)1U),
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U] - (size_t)2U),
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U] - (size_t)3U));
    zeta_i[0U] = zeta_i[0U] - (size_t)3U;
  }
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_at_layer_2
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_2_ea(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re) {
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    size_t round = i;
    zeta_i[0U] = zeta_i[0U] - (size_t)1U;
    re->coefficients[round] =
        libcrux_ml_kem_vector_portable_inv_ntt_layer_2_step_b8(
            re->coefficients[round], libcrux_ml_kem_polynomial_zeta(zeta_i[0U]),
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U] - (size_t)1U));
    zeta_i[0U] = zeta_i[0U] - (size_t)1U;
  }
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_at_layer_3
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_3_ea(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re) {
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    size_t round = i;
    zeta_i[0U] = zeta_i[0U] - (size_t)1U;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_inv_ntt_layer_3_step_b8(
            re->coefficients[round],
            libcrux_ml_kem_polynomial_zeta(zeta_i[0U]));
    re->coefficients[round] = uu____0;
  }
}

/**
A monomorphic instance of
libcrux_ml_kem.invert_ntt.inv_ntt_layer_int_vec_step_reduce with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE
    libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2
    libcrux_ml_kem_invert_ntt_inv_ntt_layer_int_vec_step_reduce_ea(
        libcrux_ml_kem_vector_portable_vector_type_PortableVector a,
        libcrux_ml_kem_vector_portable_vector_type_PortableVector b,
        int16_t zeta_r) {
  libcrux_ml_kem_vector_portable_vector_type_PortableVector a_minus_b =
      libcrux_ml_kem_vector_portable_sub_b8(b, &a);
  a = libcrux_ml_kem_vector_portable_barrett_reduce_b8(
      libcrux_ml_kem_vector_portable_add_b8(a, &b));
  b = libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_b8(
      a_minus_b, zeta_r);
  return (KRML_CLITERAL(
      libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2){.fst = a,
                                                                    .snd = b});
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_at_layer_4_plus
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_4_plus_ea(
    size_t *zeta_i, libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re,
    size_t layer) {
  size_t step = (size_t)1U << (uint32_t)layer;
  for (size_t i0 = (size_t)0U; i0 < (size_t)128U >> (uint32_t)layer; i0++) {
    size_t round = i0;
    zeta_i[0U] = zeta_i[0U] - (size_t)1U;
    size_t offset = round * step * (size_t)2U;
    size_t offset_vec =
        offset / LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR;
    size_t step_vec =
        step / LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR;
    for (size_t i = offset_vec; i < offset_vec + step_vec; i++) {
      size_t j = i;
      libcrux_ml_kem_vector_portable_vector_type_PortableVector_x2 uu____0 =
          libcrux_ml_kem_invert_ntt_inv_ntt_layer_int_vec_step_reduce_ea(
              re->coefficients[j], re->coefficients[j + step_vec],
              libcrux_ml_kem_polynomial_zeta(zeta_i[0U]));
      libcrux_ml_kem_vector_portable_vector_type_PortableVector x = uu____0.fst;
      libcrux_ml_kem_vector_portable_vector_type_PortableVector y = uu____0.snd;
      re->coefficients[j] = x;
      re->coefficients[j + step_vec] = y;
    }
  }
}

/**
A monomorphic instance of libcrux_ml_kem.invert_ntt.invert_ntt_montgomery
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_invert_ntt_invert_ntt_montgomery_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re) {
  size_t zeta_i =
      LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT / (size_t)2U;
  libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_1_ea(&zeta_i, re);
  libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_2_ea(&zeta_i, re);
  libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_3_ea(&zeta_i, re);
  libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_4_plus_ea(&zeta_i, re,
                                                          (size_t)4U);
  libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_4_plus_ea(&zeta_i, re,
                                                          (size_t)5U);
  libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_4_plus_ea(&zeta_i, re,
                                                          (size_t)6U);
  libcrux_ml_kem_invert_ntt_invert_ntt_at_layer_4_plus_ea(&zeta_i, re,
                                                          (size_t)7U);
  libcrux_ml_kem_polynomial_poly_barrett_reduce_d6_ea(re);
}

/**
A monomorphic instance of libcrux_ml_kem.polynomial.subtract_reduce
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_subtract_reduce_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *myself,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d b) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector
        coefficient_normal_form =
            libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_b8(
                b.coefficients[i0], (int16_t)1441);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector diff =
        libcrux_ml_kem_vector_portable_sub_b8(myself->coefficients[i0],
                                              &coefficient_normal_form);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector red =
        libcrux_ml_kem_vector_portable_barrett_reduce_b8(diff);
    b.coefficients[i0] = red;
  }
  return b;
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.subtract_reduce_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_subtract_reduce_d6_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d b) {
  return libcrux_ml_kem_polynomial_subtract_reduce_ea(self, b);
}

/**
 The following functions compute various expressions involving
 vectors and matrices. The computation of these expressions has been
 abstracted away into these functions in order to save on loop iterations.
 Compute v − InverseNTT(sᵀ ◦ NTT(u))
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_message
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_matrix_compute_message_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *v,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *secret_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *u_as_ntt) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d result =
      libcrux_ml_kem_polynomial_ZERO_d6_ea();
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d product =
        libcrux_ml_kem_polynomial_ntt_multiply_d6_ea(&secret_as_ntt[i0],
                                                     &u_as_ntt[i0]);
    libcrux_ml_kem_polynomial_add_to_ring_element_d6_1b(&result, &product);
  }
  libcrux_ml_kem_invert_ntt_invert_ntt_montgomery_1b(&result);
  return libcrux_ml_kem_polynomial_subtract_reduce_d6_ea(v, result);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.to_unsigned_field_modulus
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_serialize_to_unsigned_field_modulus_ea(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  return libcrux_ml_kem_vector_portable_to_unsigned_representative_b8(a);
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.compress_then_serialize_message with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE void
libcrux_ml_kem_serialize_compress_then_serialize_message_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d re, uint8_t ret[32U]) {
  uint8_t serialized[32U] = {0U};
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
        libcrux_ml_kem_serialize_to_unsigned_field_modulus_ea(
            re.coefficients[i0]);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector
        coefficient_compressed =
            libcrux_ml_kem_vector_portable_compress_1_b8(coefficient);
    uint8_t bytes[2U];
    libcrux_ml_kem_vector_portable_serialize_1_b8(coefficient_compressed,
                                                  bytes);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice3(serialized, (size_t)2U * i0,
                                    (size_t)2U * i0 + (size_t)2U, uint8_t *),
        Eurydice_array_to_slice((size_t)2U, bytes, uint8_t), uint8_t);
  }
  memcpy(ret, serialized, (size_t)32U * sizeof(uint8_t));
}

/**
 This function implements <strong>Algorithm 14</strong> of the
 NIST FIPS 203 specification; this is the Kyber CPA-PKE decryption algorithm.

 Algorithm 14 is reproduced below:

 ```plaintext
 Input: decryption key dkₚₖₑ ∈ 𝔹^{384k}.
 Input: ciphertext c ∈ 𝔹^{32(dᵤk + dᵥ)}.
 Output: message m ∈ 𝔹^{32}.

 c₁ ← c[0 : 32dᵤk]
 c₂ ← c[32dᵤk : 32(dᵤk + dᵥ)]
 u ← Decompress_{dᵤ}(ByteDecode_{dᵤ}(c₁))
 v ← Decompress_{dᵥ}(ByteDecode_{dᵥ}(c₂))
 ŝ ← ByteDecode₁₂(dkₚₖₑ)
 w ← v - NTT-¹(ŝᵀ ◦ NTT(u))
 m ← ByteEncode₁(Compress₁(w))
 return m
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt_unpacked
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- VECTOR_U_ENCODED_SIZE= 960
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_decrypt_unpacked_42(
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0 *secret_key,
    uint8_t *ciphertext, uint8_t ret[32U]) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d u_as_ntt[3U];
  libcrux_ml_kem_ind_cpa_deserialize_then_decompress_u_6c(ciphertext, u_as_ntt);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d v =
      libcrux_ml_kem_serialize_deserialize_then_decompress_ring_element_v_89(
          Eurydice_array_to_subslice_from((size_t)1088U, ciphertext,
                                          (size_t)960U, uint8_t, size_t,
                                          uint8_t[]));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d message =
      libcrux_ml_kem_matrix_compute_message_1b(&v, secret_key->secret_as_ntt,
                                               u_as_ntt);
  uint8_t ret0[32U];
  libcrux_ml_kem_serialize_compress_then_serialize_message_ea(message, ret0);
  memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.decrypt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- VECTOR_U_ENCODED_SIZE= 960
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_decrypt_42(
    Eurydice_slice secret_key, uint8_t *ciphertext, uint8_t ret[32U]) {
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0
      secret_key_unpacked;
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret0[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    ret0[i] = libcrux_ml_kem_ind_cpa_decrypt_call_mut_0b_42(&lvalue, i);
  }
  memcpy(
      secret_key_unpacked.secret_as_ntt, ret0,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  libcrux_ml_kem_ind_cpa_deserialize_vector_1b(
      secret_key, secret_key_unpacked.secret_as_ntt);
  uint8_t ret1[32U];
  libcrux_ml_kem_ind_cpa_decrypt_unpacked_42(&secret_key_unpacked, ciphertext,
                                             ret1);
  memcpy(ret, ret1, (size_t)32U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.G_4a
with const generics
- K= 3
*/
static inline void libcrux_ml_kem_hash_functions_portable_G_4a_e0(
    Eurydice_slice input, uint8_t ret[64U]) {
  libcrux_ml_kem_hash_functions_portable_G(input, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF
with const generics
- LEN= 32
*/
static inline void libcrux_ml_kem_hash_functions_portable_PRF_9e(
    Eurydice_slice input, uint8_t ret[32U]) {
  uint8_t digest[32U] = {0U};
  libcrux_sha3_portable_shake256(
      Eurydice_array_to_slice((size_t)32U, digest, uint8_t), input);
  memcpy(ret, digest, (size_t)32U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF_4a
with const generics
- K= 3
- LEN= 32
*/
static inline void libcrux_ml_kem_hash_functions_portable_PRF_4a_41(
    Eurydice_slice input, uint8_t ret[32U]) {
  libcrux_ml_kem_hash_functions_portable_PRF_9e(input, ret);
}

/**
A monomorphic instance of
libcrux_ml_kem.ind_cpa.unpacked.IndCpaPublicKeyUnpacked with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- $3size_t
*/
typedef struct libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0_s {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d t_as_ntt[3U];
  uint8_t seed_for_A[32U];
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d A[3U][3U];
} libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0;

/**
This function found in impl {core::default::Default for
libcrux_ml_kem::ind_cpa::unpacked::IndCpaPublicKeyUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.unpacked.default_8b
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static inline libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0
libcrux_ml_kem_ind_cpa_unpacked_default_8b_1b(void) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    uu____0[i] = libcrux_ml_kem_polynomial_ZERO_d6_ea();
  }
  uint8_t uu____1[32U] = {0U};
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0 lit;
  memcpy(
      lit.t_as_ntt, uu____0,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  memcpy(lit.seed_for_A, uu____1, (size_t)32U * sizeof(uint8_t));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d repeat_expression0[3U][3U];
  for (size_t i0 = (size_t)0U; i0 < (size_t)3U; i0++) {
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d repeat_expression[3U];
    for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
      repeat_expression[i] = libcrux_ml_kem_polynomial_ZERO_d6_ea();
    }
    memcpy(repeat_expression0[i0], repeat_expression,
           (size_t)3U *
               sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  }
  memcpy(lit.A, repeat_expression0,
         (size_t)3U *
             sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]));
  return lit;
}

/**
 Only use with public values.

 This MUST NOT be used with secret inputs, like its caller
 `deserialize_ring_elements_reduced`.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_to_reduced_ring_element with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_serialize_deserialize_to_reduced_ring_element_ea(
    Eurydice_slice serialized) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d re =
      libcrux_ml_kem_polynomial_ZERO_d6_ea();
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(serialized, uint8_t) / (size_t)24U; i++) {
    size_t i0 = i;
    Eurydice_slice bytes =
        Eurydice_slice_subslice3(serialized, i0 * (size_t)24U,
                                 i0 * (size_t)24U + (size_t)24U, uint8_t *);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
        libcrux_ml_kem_vector_portable_deserialize_12_b8(bytes);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_cond_subtract_3329_b8(coefficient);
    re.coefficients[i0] = uu____0;
  }
  return re;
}

/**
 See [deserialize_ring_elements_reduced_out].
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_serialize_deserialize_ring_elements_reduced_1b(
    Eurydice_slice public_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *deserialized_pk) {
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(public_key, uint8_t) /
               LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT;
       i++) {
    size_t i0 = i;
    Eurydice_slice ring_element = Eurydice_slice_subslice3(
        public_key, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
        i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT +
            LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
        uint8_t *);
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0 =
        libcrux_ml_kem_serialize_deserialize_to_reduced_ring_element_ea(
            ring_element);
    deserialized_pk[i0] = uu____0;
  }
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PortableHash
with const generics
- $3size_t
*/
typedef struct libcrux_ml_kem_hash_functions_portable_PortableHash_88_s {
  libcrux_sha3_generic_keccak_KeccakState_17 shake128_state[3U];
} libcrux_ml_kem_hash_functions_portable_PortableHash_88;

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_init_absorb_final with const
generics
- K= 3
*/
static inline libcrux_ml_kem_hash_functions_portable_PortableHash_88
libcrux_ml_kem_hash_functions_portable_shake128_init_absorb_final_e0(
    uint8_t (*input)[34U]) {
  libcrux_ml_kem_hash_functions_portable_PortableHash_88 shake128_state;
  libcrux_sha3_generic_keccak_KeccakState_17 repeat_expression[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    repeat_expression[i] = libcrux_sha3_portable_incremental_shake128_init();
  }
  memcpy(shake128_state.shake128_state, repeat_expression,
         (size_t)3U * sizeof(libcrux_sha3_generic_keccak_KeccakState_17));
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_sha3_portable_incremental_shake128_absorb_final(
        &shake128_state.shake128_state[i0],
        Eurydice_array_to_slice((size_t)34U, input[i0], uint8_t));
  }
  return shake128_state;
}

/**
This function found in impl {libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_init_absorb_final_4a with const
generics
- K= 3
*/
static inline libcrux_ml_kem_hash_functions_portable_PortableHash_88
libcrux_ml_kem_hash_functions_portable_shake128_init_absorb_final_4a_e0(
    uint8_t (*input)[34U]) {
  return libcrux_ml_kem_hash_functions_portable_shake128_init_absorb_final_e0(
      input);
}

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_first_three_blocks with
const generics
- K= 3
*/
static inline void
libcrux_ml_kem_hash_functions_portable_shake128_squeeze_first_three_blocks_e0(
    libcrux_ml_kem_hash_functions_portable_PortableHash_88 *st,
    uint8_t ret[3U][504U]) {
  uint8_t out[3U][504U] = {{0U}};
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_sha3_portable_incremental_shake128_squeeze_first_three_blocks(
        &st->shake128_state[i0],
        Eurydice_array_to_slice((size_t)504U, out[i0], uint8_t));
  }
  memcpy(ret, out, (size_t)3U * sizeof(uint8_t[504U]));
}

/**
This function found in impl {libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_first_three_blocks_4a
with const generics
- K= 3
*/
static inline void
libcrux_ml_kem_hash_functions_portable_shake128_squeeze_first_three_blocks_4a_e0(
    libcrux_ml_kem_hash_functions_portable_PortableHash_88 *self,
    uint8_t ret[3U][504U]) {
  libcrux_ml_kem_hash_functions_portable_shake128_squeeze_first_three_blocks_e0(
      self, ret);
}

/**
 If `bytes` contains a set of uniformly random bytes, this function
 uniformly samples a ring element `â` that is treated as being the NTT
 representation of the corresponding polynomial `a`.

 Since rejection sampling is used, it is possible the supplied bytes are
 not enough to sample the element, in which case an `Err` is returned and the
 caller must try again with a fresh set of bytes.

 This function <strong>partially</strong> implements <strong>Algorithm
 6</strong> of the NIST FIPS 203 standard, We say "partially" because this
 implementation only accepts a finite set of bytes as input and returns an error
 if the set is not enough; Algorithm 6 of the FIPS 203 standard on the other
 hand samples from an infinite stream of bytes until the ring element is filled.
 Algorithm 6 is reproduced below:

 ```plaintext
 Input: byte stream B ∈ 𝔹*.
 Output: array â ∈ ℤ₂₅₆.

 i ← 0
 j ← 0
 while j < 256 do
     d₁ ← B[i] + 256·(B[i+1] mod 16)
     d₂ ← ⌊B[i+1]/16⌋ + 16·B[i+2]
     if d₁ < q then
         â[j] ← d₁
         j ← j + 1
     end if
     if d₂ < q and j < 256 then
         â[j] ← d₂
         j ← j + 1
     end if
     i ← i + 3
 end while
 return â
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_uniform_distribution_next with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- N= 504
*/
static KRML_MUSTINLINE bool
libcrux_ml_kem_sampling_sample_from_uniform_distribution_next_89(
    uint8_t (*randomness)[504U], size_t *sampled_coefficients,
    int16_t (*out)[272U]) {
  for (size_t i0 = (size_t)0U; i0 < (size_t)3U; i0++) {
    size_t i1 = i0;
    for (size_t i = (size_t)0U; i < (size_t)504U / (size_t)24U; i++) {
      size_t r = i;
      if (sampled_coefficients[i1] <
          LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
        size_t sampled = libcrux_ml_kem_vector_portable_rej_sample_b8(
            Eurydice_array_to_subslice3(randomness[i1], r * (size_t)24U,
                                        r * (size_t)24U + (size_t)24U,
                                        uint8_t *),
            Eurydice_array_to_subslice3(out[i1], sampled_coefficients[i1],
                                        sampled_coefficients[i1] + (size_t)16U,
                                        int16_t *));
        size_t uu____0 = i1;
        sampled_coefficients[uu____0] = sampled_coefficients[uu____0] + sampled;
      }
    }
  }
  bool done = true;
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    if (sampled_coefficients[i0] >=
        LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
      sampled_coefficients[i0] =
          LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT;
    } else {
      done = false;
    }
  }
  return done;
}

/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_next_block with const
generics
- K= 3
*/
static inline void
libcrux_ml_kem_hash_functions_portable_shake128_squeeze_next_block_e0(
    libcrux_ml_kem_hash_functions_portable_PortableHash_88 *st,
    uint8_t ret[3U][168U]) {
  uint8_t out[3U][168U] = {{0U}};
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_sha3_portable_incremental_shake128_squeeze_next_block(
        &st->shake128_state[i0],
        Eurydice_array_to_slice((size_t)168U, out[i0], uint8_t));
  }
  memcpy(ret, out, (size_t)3U * sizeof(uint8_t[168U]));
}

/**
This function found in impl {libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>}
*/
/**
A monomorphic instance of
libcrux_ml_kem.hash_functions.portable.shake128_squeeze_next_block_4a with const
generics
- K= 3
*/
static inline void
libcrux_ml_kem_hash_functions_portable_shake128_squeeze_next_block_4a_e0(
    libcrux_ml_kem_hash_functions_portable_PortableHash_88 *self,
    uint8_t ret[3U][168U]) {
  libcrux_ml_kem_hash_functions_portable_shake128_squeeze_next_block_e0(self,
                                                                        ret);
}

/**
 If `bytes` contains a set of uniformly random bytes, this function
 uniformly samples a ring element `â` that is treated as being the NTT
 representation of the corresponding polynomial `a`.

 Since rejection sampling is used, it is possible the supplied bytes are
 not enough to sample the element, in which case an `Err` is returned and the
 caller must try again with a fresh set of bytes.

 This function <strong>partially</strong> implements <strong>Algorithm
 6</strong> of the NIST FIPS 203 standard, We say "partially" because this
 implementation only accepts a finite set of bytes as input and returns an error
 if the set is not enough; Algorithm 6 of the FIPS 203 standard on the other
 hand samples from an infinite stream of bytes until the ring element is filled.
 Algorithm 6 is reproduced below:

 ```plaintext
 Input: byte stream B ∈ 𝔹*.
 Output: array â ∈ ℤ₂₅₆.

 i ← 0
 j ← 0
 while j < 256 do
     d₁ ← B[i] + 256·(B[i+1] mod 16)
     d₂ ← ⌊B[i+1]/16⌋ + 16·B[i+2]
     if d₁ < q then
         â[j] ← d₁
         j ← j + 1
     end if
     if d₂ < q and j < 256 then
         â[j] ← d₂
         j ← j + 1
     end if
     i ← i + 3
 end while
 return â
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_uniform_distribution_next with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- N= 168
*/
static KRML_MUSTINLINE bool
libcrux_ml_kem_sampling_sample_from_uniform_distribution_next_890(
    uint8_t (*randomness)[168U], size_t *sampled_coefficients,
    int16_t (*out)[272U]) {
  for (size_t i0 = (size_t)0U; i0 < (size_t)3U; i0++) {
    size_t i1 = i0;
    for (size_t i = (size_t)0U; i < (size_t)168U / (size_t)24U; i++) {
      size_t r = i;
      if (sampled_coefficients[i1] <
          LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
        size_t sampled = libcrux_ml_kem_vector_portable_rej_sample_b8(
            Eurydice_array_to_subslice3(randomness[i1], r * (size_t)24U,
                                        r * (size_t)24U + (size_t)24U,
                                        uint8_t *),
            Eurydice_array_to_subslice3(out[i1], sampled_coefficients[i1],
                                        sampled_coefficients[i1] + (size_t)16U,
                                        int16_t *));
        size_t uu____0 = i1;
        sampled_coefficients[uu____0] = sampled_coefficients[uu____0] + sampled;
      }
    }
  }
  bool done = true;
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    if (sampled_coefficients[i0] >=
        LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT) {
      sampled_coefficients[i0] =
          LIBCRUX_ML_KEM_CONSTANTS_COEFFICIENTS_IN_RING_ELEMENT;
    } else {
      done = false;
    }
  }
  return done;
}

/**
A monomorphic instance of libcrux_ml_kem.polynomial.from_i16_array
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_from_i16_array_ea(Eurydice_slice a) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d result =
      libcrux_ml_kem_polynomial_ZERO_ea();
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_from_i16_array_b8(
            Eurydice_slice_subslice3(a, i0 * (size_t)16U,
                                     (i0 + (size_t)1U) * (size_t)16U,
                                     int16_t *));
    result.coefficients[i0] = uu____0;
  }
  return result;
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.from_i16_array_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_from_i16_array_d6_ea(Eurydice_slice a) {
  return libcrux_ml_kem_polynomial_from_i16_array_ea(a);
}

/**
This function found in impl {core::ops::function::FnMut<(@Array<i16, 272usize>),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@2]> for libcrux_ml_kem::sampling::sample_from_xof::closure<Vector,
Hasher, K>[TraitClause@0, TraitClause@1, TraitClause@2, TraitClause@3]}
*/
/**
A monomorphic instance of libcrux_ml_kem.sampling.sample_from_xof.call_mut_e7
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_sampling_sample_from_xof_call_mut_e7_2b(
    void **_, int16_t tupled_args[272U]) {
  int16_t s[272U];
  memcpy(s, tupled_args, (size_t)272U * sizeof(int16_t));
  return libcrux_ml_kem_polynomial_from_i16_array_d6_ea(
      Eurydice_array_to_subslice3(s, (size_t)0U, (size_t)256U, int16_t *));
}

/**
A monomorphic instance of libcrux_ml_kem.sampling.sample_from_xof
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_sampling_sample_from_xof_2b(
    uint8_t (*seeds)[34U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret[3U]) {
  size_t sampled_coefficients[3U] = {0U};
  int16_t out[3U][272U] = {{0U}};
  libcrux_ml_kem_hash_functions_portable_PortableHash_88 xof_state =
      libcrux_ml_kem_hash_functions_portable_shake128_init_absorb_final_4a_e0(
          seeds);
  uint8_t randomness0[3U][504U];
  libcrux_ml_kem_hash_functions_portable_shake128_squeeze_first_three_blocks_4a_e0(
      &xof_state, randomness0);
  bool done = libcrux_ml_kem_sampling_sample_from_uniform_distribution_next_89(
      randomness0, sampled_coefficients, out);
  while (true) {
    if (done) {
      break;
    } else {
      uint8_t randomness[3U][168U];
      libcrux_ml_kem_hash_functions_portable_shake128_squeeze_next_block_4a_e0(
          &xof_state, randomness);
      done = libcrux_ml_kem_sampling_sample_from_uniform_distribution_next_890(
          randomness, sampled_coefficients, out);
    }
  }
  /* Passing arrays by value in Rust generates a copy in C */
  int16_t copy_of_out[3U][272U];
  memcpy(copy_of_out, out, (size_t)3U * sizeof(int16_t[272U]));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret0[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    ret0[i] = libcrux_ml_kem_sampling_sample_from_xof_call_mut_e7_2b(
        &lvalue, copy_of_out[i]);
  }
  memcpy(
      ret, ret0,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
}

/**
A monomorphic instance of libcrux_ml_kem.matrix.sample_matrix_A
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_matrix_sample_matrix_A_2b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d (*A_transpose)[3U],
    uint8_t *seed, bool transpose) {
  for (size_t i0 = (size_t)0U; i0 < (size_t)3U; i0++) {
    size_t i1 = i0;
    uint8_t seeds[3U][34U];
    for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
      core_array__core__clone__Clone_for__Array_T__N___clone(
          (size_t)34U, seed, seeds[i], uint8_t, void *);
    }
    for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
      size_t j = i;
      seeds[j][32U] = (uint8_t)i1;
      seeds[j][33U] = (uint8_t)j;
    }
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d sampled[3U];
    libcrux_ml_kem_sampling_sample_from_xof_2b(seeds, sampled);
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)3U, sampled,
                     libcrux_ml_kem_polynomial_PolynomialRingElement_1d),
                 libcrux_ml_kem_polynomial_PolynomialRingElement_1d);
         i++) {
      size_t j = i;
      libcrux_ml_kem_polynomial_PolynomialRingElement_1d sample = sampled[j];
      if (transpose) {
        A_transpose[j][i1] = sample;
      } else {
        A_transpose[i1][j] = sample;
      }
    }
  }
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.build_unpacked_public_key_mut
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- T_AS_NTT_ENCODED_SIZE= 1152
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cpa_build_unpacked_public_key_mut_3f(
    Eurydice_slice public_key,
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0
        *unpacked_public_key) {
  Eurydice_slice uu____0 = Eurydice_slice_subslice_to(
      public_key, (size_t)1152U, uint8_t, size_t, uint8_t[]);
  libcrux_ml_kem_serialize_deserialize_ring_elements_reduced_1b(
      uu____0, unpacked_public_key->t_as_ntt);
  Eurydice_slice seed = Eurydice_slice_subslice_from(
      public_key, (size_t)1152U, uint8_t, size_t, uint8_t[]);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d(*uu____1)[3U] =
      unpacked_public_key->A;
  uint8_t ret[34U];
  libcrux_ml_kem_utils_into_padded_array_b6(seed, ret);
  libcrux_ml_kem_matrix_sample_matrix_A_2b(uu____1, ret, false);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.build_unpacked_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- T_AS_NTT_ENCODED_SIZE= 1152
*/
static KRML_MUSTINLINE
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0
    libcrux_ml_kem_ind_cpa_build_unpacked_public_key_3f(
        Eurydice_slice public_key) {
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0
      unpacked_public_key = libcrux_ml_kem_ind_cpa_unpacked_default_8b_1b();
  libcrux_ml_kem_ind_cpa_build_unpacked_public_key_mut_3f(public_key,
                                                          &unpacked_public_key);
  return unpacked_public_key;
}

/**
A monomorphic instance of K.
with types libcrux_ml_kem_polynomial_PolynomialRingElement
libcrux_ml_kem_vector_portable_vector_type_PortableVector[3size_t],
libcrux_ml_kem_polynomial_PolynomialRingElement
libcrux_ml_kem_vector_portable_vector_type_PortableVector

*/
typedef struct tuple_ed_s {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d fst[3U];
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d snd;
} tuple_ed;

/**
This function found in impl {core::ops::function::FnMut<(usize),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@2]> for libcrux_ml_kem::ind_cpa::encrypt_c1::closure<Vector, Hasher,
K, C1_LEN, U_COMPRESSION_FACTOR, BLOCK_LEN, ETA1, ETA1_RANDOMNESS_SIZE, ETA2,
ETA2_RANDOMNESS_SIZE>[TraitClause@0, TraitClause@1, TraitClause@2,
TraitClause@3]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt_c1.call_mut_f1
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- C1_LEN= 960
- U_COMPRESSION_FACTOR= 10
- BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_ind_cpa_encrypt_c1_call_mut_f1_85(void **_, size_t tupled_args) {
  return libcrux_ml_kem_polynomial_ZERO_d6_ea();
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN
with const generics
- K= 3
- LEN= 128
*/
static inline void libcrux_ml_kem_hash_functions_portable_PRFxN_41(
    uint8_t (*input)[33U], uint8_t ret[3U][128U]) {
  uint8_t out[3U][128U] = {{0U}};
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_sha3_portable_shake256(
        Eurydice_array_to_slice((size_t)128U, out[i0], uint8_t),
        Eurydice_array_to_slice((size_t)33U, input[i0], uint8_t));
  }
  memcpy(ret, out, (size_t)3U * sizeof(uint8_t[128U]));
}

/**
This function found in impl {libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRFxN_4a
with const generics
- K= 3
- LEN= 128
*/
static inline void libcrux_ml_kem_hash_functions_portable_PRFxN_4a_41(
    uint8_t (*input)[33U], uint8_t ret[3U][128U]) {
  libcrux_ml_kem_hash_functions_portable_PRFxN_41(input, ret);
}

/**
 Given a series of uniformly random bytes in `randomness`, for some number
 `eta`, the `sample_from_binomial_distribution_{eta}` functions sample a ring
 element from a binomial distribution centered at 0 that uses two sets of `eta`
 coin flips. If, for example, `eta = ETA`, each ring coefficient is a value `v`
 such such that `v ∈ {-ETA, -ETA + 1, ..., 0, ..., ETA + 1, ETA}` and:

 ```plaintext
 - If v < 0, Pr[v] = Pr[-v]
 - If v >= 0, Pr[v] = BINOMIAL_COEFFICIENT(2 * ETA; ETA - v) / 2 ^ (2 * ETA)
 ```

 The values `v < 0` are mapped to the appropriate `KyberFieldElement`.

 The expected value is:

 ```plaintext
 E[X] = (-ETA)Pr[-ETA] + (-(ETA - 1))Pr[-(ETA - 1)] + ... + (ETA - 1)Pr[ETA - 1]
 + (ETA)Pr[ETA] = 0 since Pr[-v] = Pr[v] when v < 0.
 ```

 And the variance is:

 ```plaintext
 Var(X) = E[(X - E[X])^2]
        = E[X^2]
        = sum_(v=-ETA to ETA)v^2 * (BINOMIAL_COEFFICIENT(2 * ETA; ETA - v) /
 2^(2 * ETA)) = ETA / 2
 ```

 This function implements <strong>Algorithm 7</strong> of the NIST FIPS 203
 standard, which is reproduced below:

 ```plaintext
 Input: byte array B ∈ 𝔹^{64η}.
 Output: array f ∈ ℤ₂₅₆.

 b ← BytesToBits(B)
 for (i ← 0; i < 256; i++)
     x ← ∑(j=0 to η - 1) b[2iη + j]
     y ← ∑(j=0 to η - 1) b[2iη + η + j]
     f[i] ← x−y mod q
 end for
 return f
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_binomial_distribution_2 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_sampling_sample_from_binomial_distribution_2_ea(
    Eurydice_slice randomness) {
  int16_t sampled_i16s[256U] = {0U};
  for (size_t i0 = (size_t)0U;
       i0 < Eurydice_slice_len(randomness, uint8_t) / (size_t)4U; i0++) {
    size_t chunk_number = i0;
    Eurydice_slice byte_chunk = Eurydice_slice_subslice3(
        randomness, chunk_number * (size_t)4U,
        chunk_number * (size_t)4U + (size_t)4U, uint8_t *);
    uint32_t random_bits_as_u32 =
        (((uint32_t)Eurydice_slice_index(byte_chunk, (size_t)0U, uint8_t,
                                         uint8_t *) |
          (uint32_t)Eurydice_slice_index(byte_chunk, (size_t)1U, uint8_t,
                                         uint8_t *)
              << 8U) |
         (uint32_t)Eurydice_slice_index(byte_chunk, (size_t)2U, uint8_t,
                                        uint8_t *)
             << 16U) |
        (uint32_t)Eurydice_slice_index(byte_chunk, (size_t)3U, uint8_t,
                                       uint8_t *)
            << 24U;
    uint32_t even_bits = random_bits_as_u32 & 1431655765U;
    uint32_t odd_bits = random_bits_as_u32 >> 1U & 1431655765U;
    uint32_t coin_toss_outcomes = even_bits + odd_bits;
    for (uint32_t i = 0U; i < 32U / 4U; i++) {
      uint32_t outcome_set = i;
      uint32_t outcome_set0 = outcome_set * 4U;
      int16_t outcome_1 =
          (int16_t)(coin_toss_outcomes >> (uint32_t)outcome_set0 & 3U);
      int16_t outcome_2 =
          (int16_t)(coin_toss_outcomes >> (uint32_t)(outcome_set0 + 2U) & 3U);
      size_t offset = (size_t)(outcome_set0 >> 2U);
      sampled_i16s[(size_t)8U * chunk_number + offset] = outcome_1 - outcome_2;
    }
  }
  return libcrux_ml_kem_polynomial_from_i16_array_d6_ea(
      Eurydice_array_to_slice((size_t)256U, sampled_i16s, int16_t));
}

/**
A monomorphic instance of
libcrux_ml_kem.sampling.sample_from_binomial_distribution with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- ETA= 2
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_sampling_sample_from_binomial_distribution_a0(
    Eurydice_slice randomness) {
  return libcrux_ml_kem_sampling_sample_from_binomial_distribution_2_ea(
      randomness);
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_at_layer_7
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_ntt_ntt_at_layer_7_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re) {
  size_t step = LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT / (size_t)2U;
  for (size_t i = (size_t)0U; i < step; i++) {
    size_t j = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector t =
        libcrux_ml_kem_vector_portable_multiply_by_constant_b8(
            re->coefficients[j + step], (int16_t)-1600);
    re->coefficients[j + step] =
        libcrux_ml_kem_vector_portable_sub_b8(re->coefficients[j], &t);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____1 =
        libcrux_ml_kem_vector_portable_add_b8(re->coefficients[j], &t);
    re->coefficients[j] = uu____1;
  }
}

/**
A monomorphic instance of libcrux_ml_kem.ntt.ntt_binomially_sampled_ring_element
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ntt_ntt_binomially_sampled_ring_element_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re) {
  libcrux_ml_kem_ntt_ntt_at_layer_7_ea(re);
  size_t zeta_i = (size_t)1U;
  libcrux_ml_kem_ntt_ntt_at_layer_4_plus_ea(&zeta_i, re, (size_t)6U,
                                            (size_t)11207U);
  libcrux_ml_kem_ntt_ntt_at_layer_4_plus_ea(&zeta_i, re, (size_t)5U,
                                            (size_t)11207U + (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_4_plus_ea(
      &zeta_i, re, (size_t)4U, (size_t)11207U + (size_t)2U * (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_3_ea(
      &zeta_i, re, (size_t)11207U + (size_t)3U * (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_2_ea(
      &zeta_i, re, (size_t)11207U + (size_t)4U * (size_t)3328U);
  libcrux_ml_kem_ntt_ntt_at_layer_1_ea(
      &zeta_i, re, (size_t)11207U + (size_t)5U * (size_t)3328U);
  libcrux_ml_kem_polynomial_poly_barrett_reduce_d6_ea(re);
}

/**
 Sample a vector of ring elements from a centered binomial distribution and
 convert them into their NTT representations.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.sample_vector_cbd_then_ntt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- ETA= 2
- ETA_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE uint8_t
libcrux_ml_kem_ind_cpa_sample_vector_cbd_then_ntt_3b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re_as_ntt,
    uint8_t *prf_input, uint8_t domain_separator) {
  uint8_t prf_inputs[3U][33U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    core_array__core__clone__Clone_for__Array_T__N___clone(
        (size_t)33U, prf_input, prf_inputs[i], uint8_t, void *);
  }
  domain_separator =
      libcrux_ml_kem_utils_prf_input_inc_e0(prf_inputs, domain_separator);
  uint8_t prf_outputs[3U][128U];
  libcrux_ml_kem_hash_functions_portable_PRFxN_4a_41(prf_inputs, prf_outputs);
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    re_as_ntt[i0] =
        libcrux_ml_kem_sampling_sample_from_binomial_distribution_a0(
            Eurydice_array_to_slice((size_t)128U, prf_outputs[i0], uint8_t));
    libcrux_ml_kem_ntt_ntt_binomially_sampled_ring_element_ea(&re_as_ntt[i0]);
  }
  return domain_separator;
}

/**
This function found in impl {core::ops::function::FnMut<(usize),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@2]> for libcrux_ml_kem::ind_cpa::encrypt_c1::closure#1<Vector,
Hasher, K, C1_LEN, U_COMPRESSION_FACTOR, BLOCK_LEN, ETA1, ETA1_RANDOMNESS_SIZE,
ETA2, ETA2_RANDOMNESS_SIZE>[TraitClause@0, TraitClause@1, TraitClause@2,
TraitClause@3]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt_c1.call_mut_dd
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- C1_LEN= 960
- U_COMPRESSION_FACTOR= 10
- BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_ind_cpa_encrypt_c1_call_mut_dd_85(void **_, size_t tupled_args) {
  return libcrux_ml_kem_polynomial_ZERO_d6_ea();
}

/**
 Sample a vector of ring elements from a centered binomial distribution.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.sample_ring_element_cbd
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- ETA2_RANDOMNESS_SIZE= 128
- ETA2= 2
*/
static KRML_MUSTINLINE uint8_t
libcrux_ml_kem_ind_cpa_sample_ring_element_cbd_3b(
    uint8_t *prf_input, uint8_t domain_separator,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error_1) {
  uint8_t prf_inputs[3U][33U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    core_array__core__clone__Clone_for__Array_T__N___clone(
        (size_t)33U, prf_input, prf_inputs[i], uint8_t, void *);
  }
  domain_separator =
      libcrux_ml_kem_utils_prf_input_inc_e0(prf_inputs, domain_separator);
  uint8_t prf_outputs[3U][128U];
  libcrux_ml_kem_hash_functions_portable_PRFxN_4a_41(prf_inputs, prf_outputs);
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0 =
        libcrux_ml_kem_sampling_sample_from_binomial_distribution_a0(
            Eurydice_array_to_slice((size_t)128U, prf_outputs[i0], uint8_t));
    error_1[i0] = uu____0;
  }
  return domain_separator;
}

/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF
with const generics
- LEN= 128
*/
static inline void libcrux_ml_kem_hash_functions_portable_PRF_a6(
    Eurydice_slice input, uint8_t ret[128U]) {
  uint8_t digest[128U] = {0U};
  libcrux_sha3_portable_shake256(
      Eurydice_array_to_slice((size_t)128U, digest, uint8_t), input);
  memcpy(ret, digest, (size_t)128U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.PRF_4a
with const generics
- K= 3
- LEN= 128
*/
static inline void libcrux_ml_kem_hash_functions_portable_PRF_4a_410(
    Eurydice_slice input, uint8_t ret[128U]) {
  libcrux_ml_kem_hash_functions_portable_PRF_a6(input, ret);
}

/**
This function found in impl {core::ops::function::FnMut<(usize),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]> for libcrux_ml_kem::matrix::compute_vector_u::closure<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_vector_u.call_mut_a8
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_matrix_compute_vector_u_call_mut_a8_1b(void **_,
                                                      size_t tupled_args) {
  return libcrux_ml_kem_polynomial_ZERO_d6_ea();
}

/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_error_reduce
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_polynomial_add_error_reduce_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *myself,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t j = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector
        coefficient_normal_form =
            libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_b8(
                myself->coefficients[j], (int16_t)1441);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector sum =
        libcrux_ml_kem_vector_portable_add_b8(coefficient_normal_form,
                                              &error->coefficients[j]);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector red =
        libcrux_ml_kem_vector_portable_barrett_reduce_b8(sum);
    myself->coefficients[j] = red;
  }
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_error_reduce_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void libcrux_ml_kem_polynomial_add_error_reduce_d6_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error) {
  libcrux_ml_kem_polynomial_add_error_reduce_ea(self, error);
}

/**
 Compute u := InvertNTT(Aᵀ ◦ r̂) + e₁
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_vector_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_matrix_compute_vector_u_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d (*a_as_ntt)[3U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error_1,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret[3U]) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d result[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    result[i] =
        libcrux_ml_kem_matrix_compute_vector_u_call_mut_a8_1b(&lvalue, i);
  }
  for (size_t i0 = (size_t)0U;
       i0 < Eurydice_slice_len(
                Eurydice_array_to_slice(
                    (size_t)3U, a_as_ntt,
                    libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]),
                libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]);
       i0++) {
    size_t i1 = i0;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *row = a_as_ntt[i1];
    for (size_t i = (size_t)0U;
         i < Eurydice_slice_len(
                 Eurydice_array_to_slice(
                     (size_t)3U, row,
                     libcrux_ml_kem_polynomial_PolynomialRingElement_1d),
                 libcrux_ml_kem_polynomial_PolynomialRingElement_1d);
         i++) {
      size_t j = i;
      libcrux_ml_kem_polynomial_PolynomialRingElement_1d *a_element = &row[j];
      libcrux_ml_kem_polynomial_PolynomialRingElement_1d product =
          libcrux_ml_kem_polynomial_ntt_multiply_d6_ea(a_element, &r_as_ntt[j]);
      libcrux_ml_kem_polynomial_add_to_ring_element_d6_1b(&result[i1],
                                                          &product);
    }
    libcrux_ml_kem_invert_ntt_invert_ntt_montgomery_1b(&result[i1]);
    libcrux_ml_kem_polynomial_add_error_reduce_d6_ea(&result[i1], &error_1[i1]);
  }
  memcpy(
      ret, result,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
}

/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress.compress
with const generics
- COEFFICIENT_BITS= 10
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_compress_ef(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    int16_t uu____0 = libcrux_secrets_int_as_i16_f5(
        libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(
            (uint8_t)(int32_t)10,
            libcrux_secrets_int_as_u16_f5(a.elements[i0])));
    a.elements[i0] = uu____0;
  }
  return a;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress_b8
with const generics
- COEFFICIENT_BITS= 10
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_b8_ef(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  return libcrux_ml_kem_vector_portable_compress_compress_ef(a);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.compress_then_serialize_10
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- OUT_LEN= 320
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_serialize_compress_then_serialize_10_ff(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re, uint8_t ret[320U]) {
  uint8_t serialized[320U] = {0U};
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
        libcrux_ml_kem_vector_portable_compress_b8_ef(
            libcrux_ml_kem_serialize_to_unsigned_field_modulus_ea(
                re->coefficients[i0]));
    uint8_t bytes[20U];
    libcrux_ml_kem_vector_portable_serialize_10_b8(coefficient, bytes);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice3(serialized, (size_t)20U * i0,
                                    (size_t)20U * i0 + (size_t)20U, uint8_t *),
        Eurydice_array_to_slice((size_t)20U, bytes, uint8_t), uint8_t);
  }
  memcpy(ret, serialized, (size_t)320U * sizeof(uint8_t));
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.compress_then_serialize_ring_element_u with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- COMPRESSION_FACTOR= 10
- OUT_LEN= 320
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_serialize_compress_then_serialize_ring_element_u_fe(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re, uint8_t ret[320U]) {
  uint8_t uu____0[320U];
  libcrux_ml_kem_serialize_compress_then_serialize_10_ff(re, uu____0);
  memcpy(ret, uu____0, (size_t)320U * sizeof(uint8_t));
}

/**
 Call [`compress_then_serialize_ring_element_u`] on each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.compress_then_serialize_u
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- OUT_LEN= 960
- COMPRESSION_FACTOR= 10
- BLOCK_LEN= 320
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_compress_then_serialize_u_43(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d input[3U],
    Eurydice_slice out) {
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(
               Eurydice_array_to_slice(
                   (size_t)3U, input,
                   libcrux_ml_kem_polynomial_PolynomialRingElement_1d),
               libcrux_ml_kem_polynomial_PolynomialRingElement_1d);
       i++) {
    size_t i0 = i;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d re = input[i0];
    Eurydice_slice uu____0 = Eurydice_slice_subslice3(
        out, i0 * ((size_t)960U / (size_t)3U),
        (i0 + (size_t)1U) * ((size_t)960U / (size_t)3U), uint8_t *);
    uint8_t ret[320U];
    libcrux_ml_kem_serialize_compress_then_serialize_ring_element_u_fe(&re,
                                                                       ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)320U, ret, uint8_t), uint8_t);
  }
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt_c1
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- C1_LEN= 960
- U_COMPRESSION_FACTOR= 10
- BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE tuple_ed libcrux_ml_kem_ind_cpa_encrypt_c1_85(
    Eurydice_slice randomness,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d (*matrix)[3U],
    Eurydice_slice ciphertext) {
  uint8_t prf_input[33U];
  libcrux_ml_kem_utils_into_padded_array_c8(randomness, prf_input);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d r_as_ntt[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    r_as_ntt[i] = libcrux_ml_kem_ind_cpa_encrypt_c1_call_mut_f1_85(&lvalue, i);
  }
  uint8_t domain_separator0 =
      libcrux_ml_kem_ind_cpa_sample_vector_cbd_then_ntt_3b(r_as_ntt, prf_input,
                                                           0U);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d error_1[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    error_1[i] = libcrux_ml_kem_ind_cpa_encrypt_c1_call_mut_dd_85(&lvalue, i);
  }
  uint8_t domain_separator = libcrux_ml_kem_ind_cpa_sample_ring_element_cbd_3b(
      prf_input, domain_separator0, error_1);
  prf_input[32U] = domain_separator;
  uint8_t prf_output[128U];
  libcrux_ml_kem_hash_functions_portable_PRF_4a_410(
      Eurydice_array_to_slice((size_t)33U, prf_input, uint8_t), prf_output);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d error_2 =
      libcrux_ml_kem_sampling_sample_from_binomial_distribution_a0(
          Eurydice_array_to_slice((size_t)128U, prf_output, uint8_t));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d u[3U];
  libcrux_ml_kem_matrix_compute_vector_u_1b(matrix, r_as_ntt, error_1, u);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0[3U];
  memcpy(
      uu____0, u,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  libcrux_ml_kem_ind_cpa_compress_then_serialize_u_43(uu____0, ciphertext);
  /* Passing arrays by value in Rust generates a copy in C */
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d copy_of_r_as_ntt[3U];
  memcpy(
      copy_of_r_as_ntt, r_as_ntt,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  tuple_ed lit;
  memcpy(
      lit.fst, copy_of_r_as_ntt,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  lit.snd = error_2;
  return lit;
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_then_decompress_message with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_serialize_deserialize_then_decompress_message_ea(
    uint8_t *serialized) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d re =
      libcrux_ml_kem_polynomial_ZERO_d6_ea();
  for (size_t i = (size_t)0U; i < (size_t)16U; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector
        coefficient_compressed =
            libcrux_ml_kem_vector_portable_deserialize_1_b8(
                Eurydice_array_to_subslice3(serialized, (size_t)2U * i0,
                                            (size_t)2U * i0 + (size_t)2U,
                                            uint8_t *));
    libcrux_ml_kem_vector_portable_vector_type_PortableVector uu____0 =
        libcrux_ml_kem_vector_portable_decompress_1_b8(coefficient_compressed);
    re.coefficients[i0] = uu____0;
  }
  return re;
}

/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_message_error_reduce
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_add_message_error_reduce_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *myself,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *message,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d result) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector
        coefficient_normal_form =
            libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_b8(
                result.coefficients[i0], (int16_t)1441);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector sum1 =
        libcrux_ml_kem_vector_portable_add_b8(myself->coefficients[i0],
                                              &message->coefficients[i0]);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector sum2 =
        libcrux_ml_kem_vector_portable_add_b8(coefficient_normal_form, &sum1);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector red =
        libcrux_ml_kem_vector_portable_barrett_reduce_b8(sum2);
    result.coefficients[i0] = red;
  }
  return result;
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_message_error_reduce_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_add_message_error_reduce_d6_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *message,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d result) {
  return libcrux_ml_kem_polynomial_add_message_error_reduce_ea(self, message,
                                                               result);
}

/**
 Compute InverseNTT(tᵀ ◦ r̂) + e₂ + message
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_ring_element_v
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_matrix_compute_ring_element_v_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *t_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error_2,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *message) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d result =
      libcrux_ml_kem_polynomial_ZERO_d6_ea();
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d product =
        libcrux_ml_kem_polynomial_ntt_multiply_d6_ea(&t_as_ntt[i0],
                                                     &r_as_ntt[i0]);
    libcrux_ml_kem_polynomial_add_to_ring_element_d6_1b(&result, &product);
  }
  libcrux_ml_kem_invert_ntt_invert_ntt_montgomery_1b(&result);
  return libcrux_ml_kem_polynomial_add_message_error_reduce_d6_ea(
      error_2, message, result);
}

/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress.compress
with const generics
- COEFFICIENT_BITS= 4
*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_compress_d1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_VECTOR_TRAITS_FIELD_ELEMENTS_IN_VECTOR; i++) {
    size_t i0 = i;
    int16_t uu____0 = libcrux_secrets_int_as_i16_f5(
        libcrux_ml_kem_vector_portable_compress_compress_ciphertext_coefficient(
            (uint8_t)(int32_t)4,
            libcrux_secrets_int_as_u16_f5(a.elements[i0])));
    a.elements[i0] = uu____0;
  }
  return a;
}

/**
This function found in impl {libcrux_ml_kem::vector::traits::Operations for
libcrux_ml_kem::vector::portable::vector_type::PortableVector}
*/
/**
A monomorphic instance of libcrux_ml_kem.vector.portable.compress_b8
with const generics
- COEFFICIENT_BITS= 4
*/
static inline libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_vector_portable_compress_b8_d1(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector a) {
  return libcrux_ml_kem_vector_portable_compress_compress_d1(a);
}

/**
A monomorphic instance of libcrux_ml_kem.serialize.compress_then_serialize_4
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
libcrux_ml_kem_serialize_compress_then_serialize_4_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d re,
    Eurydice_slice serialized) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
        libcrux_ml_kem_vector_portable_compress_b8_d1(
            libcrux_ml_kem_serialize_to_unsigned_field_modulus_ea(
                re.coefficients[i0]));
    uint8_t bytes[8U];
    libcrux_ml_kem_vector_portable_serialize_4_b8(coefficient, bytes);
    Eurydice_slice_copy(
        Eurydice_slice_subslice3(serialized, (size_t)8U * i0,
                                 (size_t)8U * i0 + (size_t)8U, uint8_t *),
        Eurydice_array_to_slice((size_t)8U, bytes, uint8_t), uint8_t);
  }
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.compress_then_serialize_ring_element_v with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- COMPRESSION_FACTOR= 4
- OUT_LEN= 128
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_serialize_compress_then_serialize_ring_element_v_6c(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d re, Eurydice_slice out) {
  libcrux_ml_kem_serialize_compress_then_serialize_4_ea(re, out);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt_c2
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- V_COMPRESSION_FACTOR= 4
- C2_LEN= 128
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_encrypt_c2_6c(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *t_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *r_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error_2,
    uint8_t *message, Eurydice_slice ciphertext) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d message_as_ring_element =
      libcrux_ml_kem_serialize_deserialize_then_decompress_message_ea(message);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d v =
      libcrux_ml_kem_matrix_compute_ring_element_v_1b(
          t_as_ntt, r_as_ntt, error_2, &message_as_ring_element);
  libcrux_ml_kem_serialize_compress_then_serialize_ring_element_v_6c(
      v, ciphertext);
}

/**
 This function implements <strong>Algorithm 13</strong> of the
 NIST FIPS 203 specification; this is the Kyber CPA-PKE encryption algorithm.

 Algorithm 13 is reproduced below:

 ```plaintext
 Input: encryption key ekₚₖₑ ∈ 𝔹^{384k+32}.
 Input: message m ∈ 𝔹^{32}.
 Input: encryption randomness r ∈ 𝔹^{32}.
 Output: ciphertext c ∈ 𝔹^{32(dᵤk + dᵥ)}.

 N ← 0
 t̂ ← ByteDecode₁₂(ekₚₖₑ[0:384k])
 ρ ← ekₚₖₑ[384k: 384k + 32]
 for (i ← 0; i < k; i++)
     for(j ← 0; j < k; j++)
         Â[i,j] ← SampleNTT(XOF(ρ, i, j))
     end for
 end for
 for(i ← 0; i < k; i++)
     r[i] ← SamplePolyCBD_{η₁}(PRF_{η₁}(r,N))
     N ← N + 1
 end for
 for(i ← 0; i < k; i++)
     e₁[i] ← SamplePolyCBD_{η₂}(PRF_{η₂}(r,N))
     N ← N + 1
 end for
 e₂ ← SamplePolyCBD_{η₂}(PRF_{η₂}(r,N))
 r̂ ← NTT(r)
 u ← NTT-¹(Âᵀ ◦ r̂) + e₁
 μ ← Decompress₁(ByteDecode₁(m)))
 v ← NTT-¹(t̂ᵀ ◦ rˆ) + e₂ + μ
 c₁ ← ByteEncode_{dᵤ}(Compress_{dᵤ}(u))
 c₂ ← ByteEncode_{dᵥ}(Compress_{dᵥ}(v))
 return c ← (c₁ ‖ c₂)
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt_unpacked
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_LEN= 960
- C2_LEN= 128
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
- BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_encrypt_unpacked_2a(
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0 *public_key,
    uint8_t *message, Eurydice_slice randomness, uint8_t ret[1088U]) {
  uint8_t ciphertext[1088U] = {0U};
  tuple_ed uu____0 = libcrux_ml_kem_ind_cpa_encrypt_c1_85(
      randomness, public_key->A,
      Eurydice_array_to_subslice3(ciphertext, (size_t)0U, (size_t)960U,
                                  uint8_t *));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d r_as_ntt[3U];
  memcpy(
      r_as_ntt, uu____0.fst,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d error_2 = uu____0.snd;
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d *uu____1 =
      public_key->t_as_ntt;
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d *uu____2 = r_as_ntt;
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d *uu____3 = &error_2;
  uint8_t *uu____4 = message;
  libcrux_ml_kem_ind_cpa_encrypt_c2_6c(
      uu____1, uu____2, uu____3, uu____4,
      Eurydice_array_to_subslice_from((size_t)1088U, ciphertext, (size_t)960U,
                                      uint8_t, size_t, uint8_t[]));
  memcpy(ret, ciphertext, (size_t)1088U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.encrypt
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_LEN= 960
- C2_LEN= 128
- U_COMPRESSION_FACTOR= 10
- V_COMPRESSION_FACTOR= 4
- BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_encrypt_2a(
    Eurydice_slice public_key, uint8_t *message, Eurydice_slice randomness,
    uint8_t ret[1088U]) {
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0
      unpacked_public_key =
          libcrux_ml_kem_ind_cpa_build_unpacked_public_key_3f(public_key);
  uint8_t ret0[1088U];
  libcrux_ml_kem_ind_cpa_encrypt_unpacked_2a(&unpacked_public_key, message,
                                             randomness, ret0);
  memcpy(ret, ret0, (size_t)1088U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.kdf_39
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
*/
static KRML_MUSTINLINE void libcrux_ml_kem_variant_kdf_39_d6(
    Eurydice_slice shared_secret, uint8_t *_, uint8_t ret[32U]) {
  uint8_t out[32U] = {0U};
  Eurydice_slice_copy(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                      shared_secret, uint8_t);
  memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
 This code verifies on some machines, runs out of memory on others
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.decapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CPA_SECRET_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- CIPHERTEXT_SIZE= 1088
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- C1_BLOCK_SIZE= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
- IMPLICIT_REJECTION_HASH_INPUT_SIZE= 1120
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cca_decapsulate_62(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext, uint8_t ret[32U]) {
  Eurydice_slice_uint8_t_x4 uu____0 =
      libcrux_ml_kem_types_unpack_private_key_b4(
          Eurydice_array_to_slice((size_t)2400U, private_key->value, uint8_t));
  Eurydice_slice ind_cpa_secret_key = uu____0.fst;
  Eurydice_slice ind_cpa_public_key = uu____0.snd;
  Eurydice_slice ind_cpa_public_key_hash = uu____0.thd;
  Eurydice_slice implicit_rejection_value = uu____0.f3;
  uint8_t decrypted[32U];
  libcrux_ml_kem_ind_cpa_decrypt_42(ind_cpa_secret_key, ciphertext->value,
                                    decrypted);
  uint8_t to_hash0[64U];
  libcrux_ml_kem_utils_into_padded_array_24(
      Eurydice_array_to_slice((size_t)32U, decrypted, uint8_t), to_hash0);
  Eurydice_slice_copy(
      Eurydice_array_to_subslice_from(
          (size_t)64U, to_hash0, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
          uint8_t, size_t, uint8_t[]),
      ind_cpa_public_key_hash, uint8_t);
  uint8_t hashed[64U];
  libcrux_ml_kem_hash_functions_portable_G_4a_e0(
      Eurydice_array_to_slice((size_t)64U, to_hash0, uint8_t), hashed);
  Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
      Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
      LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
      Eurydice_slice_uint8_t_x2);
  Eurydice_slice shared_secret0 = uu____1.fst;
  Eurydice_slice pseudorandomness = uu____1.snd;
  uint8_t to_hash[1120U];
  libcrux_ml_kem_utils_into_padded_array_15(implicit_rejection_value, to_hash);
  Eurydice_slice uu____2 = Eurydice_array_to_subslice_from(
      (size_t)1120U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
      uint8_t, size_t, uint8_t[]);
  Eurydice_slice_copy(uu____2, libcrux_ml_kem_types_as_ref_d3_80(ciphertext),
                      uint8_t);
  uint8_t implicit_rejection_shared_secret0[32U];
  libcrux_ml_kem_hash_functions_portable_PRF_4a_41(
      Eurydice_array_to_slice((size_t)1120U, to_hash, uint8_t),
      implicit_rejection_shared_secret0);
  uint8_t expected_ciphertext[1088U];
  libcrux_ml_kem_ind_cpa_encrypt_2a(ind_cpa_public_key, decrypted,
                                    pseudorandomness, expected_ciphertext);
  uint8_t implicit_rejection_shared_secret[32U];
  libcrux_ml_kem_variant_kdf_39_d6(
      Eurydice_array_to_slice((size_t)32U, implicit_rejection_shared_secret0,
                              uint8_t),
      libcrux_ml_kem_types_as_slice_a9_80(ciphertext),
      implicit_rejection_shared_secret);
  uint8_t shared_secret[32U];
  libcrux_ml_kem_variant_kdf_39_d6(
      shared_secret0, libcrux_ml_kem_types_as_slice_a9_80(ciphertext),
      shared_secret);
  uint8_t ret0[32U];
  libcrux_ml_kem_constant_time_ops_compare_ciphertexts_select_shared_secret_in_constant_time(
      libcrux_ml_kem_types_as_ref_d3_80(ciphertext),
      Eurydice_array_to_slice((size_t)1088U, expected_ciphertext, uint8_t),
      Eurydice_array_to_slice((size_t)32U, shared_secret, uint8_t),
      Eurydice_array_to_slice((size_t)32U, implicit_rejection_shared_secret,
                              uint8_t),
      ret0);
  memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
 Portable decapsulate
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.decapsulate with const generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CPA_SECRET_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- CIPHERTEXT_SIZE= 1088
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- C1_BLOCK_SIZE= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
- IMPLICIT_REJECTION_HASH_INPUT_SIZE= 1120
*/
static inline void
libcrux_ml_kem_ind_cca_instantiations_portable_decapsulate_35(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext, uint8_t ret[32U]) {
  libcrux_ml_kem_ind_cca_decapsulate_62(private_key, ciphertext, ret);
}

/**
 Decapsulate ML-KEM 768

 Generates an [`MlKemSharedSecret`].
 The input is a reference to an [`MlKem768PrivateKey`] and an
 [`MlKem768Ciphertext`].
*/
static inline void libcrux_ml_kem_mlkem768_portable_decapsulate(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext, uint8_t ret[32U]) {
  libcrux_ml_kem_ind_cca_instantiations_portable_decapsulate_35(
      private_key, ciphertext, ret);
}

/**
This function found in impl {libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.entropy_preprocess_39
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_variant_entropy_preprocess_39_9c(
    Eurydice_slice randomness, uint8_t ret[32U]) {
  uint8_t out[32U] = {0U};
  Eurydice_slice_copy(Eurydice_array_to_slice((size_t)32U, out, uint8_t),
                      randomness, uint8_t);
  memcpy(ret, out, (size_t)32U * sizeof(uint8_t));
}

/**
This function found in impl {libcrux_ml_kem::hash_functions::Hash<K> for
libcrux_ml_kem::hash_functions::portable::PortableHash<K>}
*/
/**
A monomorphic instance of libcrux_ml_kem.hash_functions.portable.H_4a
with const generics
- K= 3
*/
static inline void libcrux_ml_kem_hash_functions_portable_H_4a_e0(
    Eurydice_slice input, uint8_t ret[32U]) {
  libcrux_ml_kem_hash_functions_portable_H(input, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.encapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- PUBLIC_KEY_SIZE= 1184
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- C1_BLOCK_SIZE= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE tuple_c2 libcrux_ml_kem_ind_cca_encapsulate_ca(
    libcrux_ml_kem_types_MlKemPublicKey_30 *public_key, uint8_t *randomness) {
  uint8_t randomness0[32U];
  libcrux_ml_kem_variant_entropy_preprocess_39_9c(
      Eurydice_array_to_slice((size_t)32U, randomness, uint8_t), randomness0);
  uint8_t to_hash[64U];
  libcrux_ml_kem_utils_into_padded_array_24(
      Eurydice_array_to_slice((size_t)32U, randomness0, uint8_t), to_hash);
  Eurydice_slice uu____0 = Eurydice_array_to_subslice_from(
      (size_t)64U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE, uint8_t,
      size_t, uint8_t[]);
  uint8_t ret0[32U];
  libcrux_ml_kem_hash_functions_portable_H_4a_e0(
      Eurydice_array_to_slice((size_t)1184U,
                              libcrux_ml_kem_types_as_slice_e6_d0(public_key),
                              uint8_t),
      ret0);
  Eurydice_slice_copy(
      uu____0, Eurydice_array_to_slice((size_t)32U, ret0, uint8_t), uint8_t);
  uint8_t hashed[64U];
  libcrux_ml_kem_hash_functions_portable_G_4a_e0(
      Eurydice_array_to_slice((size_t)64U, to_hash, uint8_t), hashed);
  Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
      Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
      LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
      Eurydice_slice_uint8_t_x2);
  Eurydice_slice shared_secret = uu____1.fst;
  Eurydice_slice pseudorandomness = uu____1.snd;
  uint8_t ciphertext[1088U];
  libcrux_ml_kem_ind_cpa_encrypt_2a(
      Eurydice_array_to_slice((size_t)1184U,
                              libcrux_ml_kem_types_as_slice_e6_d0(public_key),
                              uint8_t),
      randomness0, pseudorandomness, ciphertext);
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_ciphertext[1088U];
  memcpy(copy_of_ciphertext, ciphertext, (size_t)1088U * sizeof(uint8_t));
  tuple_c2 lit;
  lit.fst = libcrux_ml_kem_types_from_e0_80(copy_of_ciphertext);
  uint8_t ret[32U];
  libcrux_ml_kem_variant_kdf_39_d6(shared_secret, ciphertext, ret);
  memcpy(lit.snd, ret, (size_t)32U * sizeof(uint8_t));
  return lit;
}

/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.encapsulate with const generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- PUBLIC_KEY_SIZE= 1184
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- C1_BLOCK_SIZE= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static inline tuple_c2
libcrux_ml_kem_ind_cca_instantiations_portable_encapsulate_cd(
    libcrux_ml_kem_types_MlKemPublicKey_30 *public_key, uint8_t *randomness) {
  return libcrux_ml_kem_ind_cca_encapsulate_ca(public_key, randomness);
}

/**
 Encapsulate ML-KEM 768

 Generates an ([`MlKem768Ciphertext`], [`MlKemSharedSecret`]) tuple.
 The input is a reference to an [`MlKem768PublicKey`] and [`SHARED_SECRET_SIZE`]
 bytes of `randomness`.
*/
static inline tuple_c2 libcrux_ml_kem_mlkem768_portable_encapsulate(
    libcrux_ml_kem_types_MlKemPublicKey_30 *public_key,
    uint8_t randomness[32U]) {
  return libcrux_ml_kem_ind_cca_instantiations_portable_encapsulate_cd(
      public_key, randomness);
}

/**
This function found in impl {core::default::Default for
libcrux_ml_kem::ind_cpa::unpacked::IndCpaPrivateKeyUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.unpacked.default_70
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static inline libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0
libcrux_ml_kem_ind_cpa_unpacked_default_70_1b(void) {
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0 lit;
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d repeat_expression[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    repeat_expression[i] = libcrux_ml_kem_polynomial_ZERO_d6_ea();
  }
  memcpy(
      lit.secret_as_ntt, repeat_expression,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  return lit;
}

/**
This function found in impl {libcrux_ml_kem::variant::Variant for
libcrux_ml_kem::variant::MlKem}
*/
/**
A monomorphic instance of libcrux_ml_kem.variant.cpa_keygen_seed_39
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_variant_cpa_keygen_seed_39_9c(
    Eurydice_slice key_generation_seed, uint8_t ret[64U]) {
  uint8_t seed[33U] = {0U};
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          seed, (size_t)0U,
          LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t *),
      key_generation_seed, uint8_t);
  seed[LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE] =
      (uint8_t)(size_t)3U;
  uint8_t ret0[64U];
  libcrux_ml_kem_hash_functions_portable_G_4a_e0(
      Eurydice_array_to_slice((size_t)33U, seed, uint8_t), ret0);
  memcpy(ret, ret0, (size_t)64U * sizeof(uint8_t));
}

/**
This function found in impl {core::ops::function::FnMut<(usize),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@3]> for
libcrux_ml_kem::ind_cpa::generate_keypair_unpacked::closure<Vector, Hasher,
Scheme, K, ETA1, ETA1_RANDOMNESS_SIZE>[TraitClause@0, TraitClause@1,
TraitClause@2, TraitClause@3, TraitClause@4, TraitClause@5]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cpa.generate_keypair_unpacked.call_mut_73 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_ind_cpa_generate_keypair_unpacked_call_mut_73_1c(
    void **_, size_t tupled_args) {
  return libcrux_ml_kem_polynomial_ZERO_d6_ea();
}

/**
A monomorphic instance of libcrux_ml_kem.polynomial.to_standard_domain
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE libcrux_ml_kem_vector_portable_vector_type_PortableVector
libcrux_ml_kem_polynomial_to_standard_domain_ea(
    libcrux_ml_kem_vector_portable_vector_type_PortableVector vector) {
  return libcrux_ml_kem_vector_portable_montgomery_multiply_by_constant_b8(
      vector,
      LIBCRUX_ML_KEM_VECTOR_TRAITS_MONTGOMERY_R_SQUARED_MOD_FIELD_MODULUS);
}

/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_standard_error_reduce
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
libcrux_ml_kem_polynomial_add_standard_error_reduce_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *myself,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error) {
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t j = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector
        coefficient_normal_form =
            libcrux_ml_kem_polynomial_to_standard_domain_ea(
                myself->coefficients[j]);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector sum =
        libcrux_ml_kem_vector_portable_add_b8(coefficient_normal_form,
                                              &error->coefficients[j]);
    libcrux_ml_kem_vector_portable_vector_type_PortableVector red =
        libcrux_ml_kem_vector_portable_barrett_reduce_b8(sum);
    myself->coefficients[j] = red;
  }
}

/**
This function found in impl
{libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.add_standard_error_reduce_d6
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static KRML_MUSTINLINE void
libcrux_ml_kem_polynomial_add_standard_error_reduce_d6_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *self,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error) {
  libcrux_ml_kem_polynomial_add_standard_error_reduce_ea(self, error);
}

/**
 Compute Â ◦ ŝ + ê
*/
/**
A monomorphic instance of libcrux_ml_kem.matrix.compute_As_plus_e
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_matrix_compute_As_plus_e_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *t_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d (*matrix_A)[3U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *s_as_ntt,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *error_as_ntt) {
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(
               Eurydice_array_to_slice(
                   (size_t)3U, matrix_A,
                   libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]),
               libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]);
       i++) {
    size_t i0 = i;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *row = matrix_A[i0];
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0 =
        libcrux_ml_kem_polynomial_ZERO_d6_ea();
    t_as_ntt[i0] = uu____0;
    for (size_t i1 = (size_t)0U;
         i1 < Eurydice_slice_len(
                  Eurydice_array_to_slice(
                      (size_t)3U, row,
                      libcrux_ml_kem_polynomial_PolynomialRingElement_1d),
                  libcrux_ml_kem_polynomial_PolynomialRingElement_1d);
         i1++) {
      size_t j = i1;
      libcrux_ml_kem_polynomial_PolynomialRingElement_1d *matrix_element =
          &row[j];
      libcrux_ml_kem_polynomial_PolynomialRingElement_1d product =
          libcrux_ml_kem_polynomial_ntt_multiply_d6_ea(matrix_element,
                                                       &s_as_ntt[j]);
      libcrux_ml_kem_polynomial_add_to_ring_element_d6_1b(&t_as_ntt[i0],
                                                          &product);
    }
    libcrux_ml_kem_polynomial_add_standard_error_reduce_d6_ea(
        &t_as_ntt[i0], &error_as_ntt[i0]);
  }
}

/**
 This function implements most of <strong>Algorithm 12</strong> of the
 NIST FIPS 203 specification; this is the Kyber CPA-PKE key generation
 algorithm.

 We say "most of" since Algorithm 12 samples the required randomness within
 the function itself, whereas this implementation expects it to be provided
 through the `key_generation_seed` parameter.

 Algorithm 12 is reproduced below:

 ```plaintext
 Output: encryption key ekₚₖₑ ∈ 𝔹^{384k+32}.
 Output: decryption key dkₚₖₑ ∈ 𝔹^{384k}.

 d ←$ B
 (ρ,σ) ← G(d)
 N ← 0
 for (i ← 0; i < k; i++)
     for(j ← 0; j < k; j++)
         Â[i,j] ← SampleNTT(XOF(ρ, i, j))
     end for
 end for
 for(i ← 0; i < k; i++)
     s[i] ← SamplePolyCBD_{η₁}(PRF_{η₁}(σ,N))
     N ← N + 1
 end for
 for(i ← 0; i < k; i++)
     e[i] ← SamplePolyCBD_{η₂}(PRF_{η₂}(σ,N))
     N ← N + 1
 end for
 ŝ ← NTT(s)
 ê ← NTT(e)
 t̂ ← Â◦ŝ + ê
 ekₚₖₑ ← ByteEncode₁₂(t̂) ‖ ρ
 dkₚₖₑ ← ByteEncode₁₂(ŝ)
 ```

 The NIST FIPS 203 standard can be found at
 <https://csrc.nist.gov/pubs/fips/203/ipd>.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.generate_keypair_unpacked
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_generate_keypair_unpacked_1c(
    Eurydice_slice key_generation_seed,
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0 *private_key,
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0 *public_key) {
  uint8_t hashed[64U];
  libcrux_ml_kem_variant_cpa_keygen_seed_39_9c(key_generation_seed, hashed);
  Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
      Eurydice_array_to_slice((size_t)64U, hashed, uint8_t), (size_t)32U,
      uint8_t, Eurydice_slice_uint8_t_x2);
  Eurydice_slice seed_for_A = uu____0.fst;
  Eurydice_slice seed_for_secret_and_error = uu____0.snd;
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d(*uu____1)[3U] =
      public_key->A;
  uint8_t ret[34U];
  libcrux_ml_kem_utils_into_padded_array_b6(seed_for_A, ret);
  libcrux_ml_kem_matrix_sample_matrix_A_2b(uu____1, ret, true);
  uint8_t prf_input[33U];
  libcrux_ml_kem_utils_into_padded_array_c8(seed_for_secret_and_error,
                                            prf_input);
  uint8_t domain_separator =
      libcrux_ml_kem_ind_cpa_sample_vector_cbd_then_ntt_3b(
          private_key->secret_as_ntt, prf_input, 0U);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d error_as_ntt[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    error_as_ntt[i] =
        libcrux_ml_kem_ind_cpa_generate_keypair_unpacked_call_mut_73_1c(&lvalue,
                                                                        i);
  }
  libcrux_ml_kem_ind_cpa_sample_vector_cbd_then_ntt_3b(error_as_ntt, prf_input,
                                                       domain_separator);
  libcrux_ml_kem_matrix_compute_As_plus_e_1b(
      public_key->t_as_ntt, public_key->A, private_key->secret_as_ntt,
      error_as_ntt);
  uint8_t uu____2[32U];
  Result_fb dst;
  Eurydice_slice_to_array2(&dst, seed_for_A, Eurydice_slice, uint8_t[32U],
                           TryFromSliceError);
  unwrap_26_b3(dst, uu____2);
  memcpy(public_key->seed_for_A, uu____2, (size_t)32U * sizeof(uint8_t));
}

/**
A monomorphic instance of
libcrux_ml_kem.serialize.serialize_uncompressed_ring_element with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics

*/
static KRML_MUSTINLINE void
libcrux_ml_kem_serialize_serialize_uncompressed_ring_element_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *re, uint8_t ret[384U]) {
  uint8_t serialized[384U] = {0U};
  for (size_t i = (size_t)0U;
       i < LIBCRUX_ML_KEM_POLYNOMIAL_VECTORS_IN_RING_ELEMENT; i++) {
    size_t i0 = i;
    libcrux_ml_kem_vector_portable_vector_type_PortableVector coefficient =
        libcrux_ml_kem_serialize_to_unsigned_field_modulus_ea(
            re->coefficients[i0]);
    uint8_t bytes[24U];
    libcrux_ml_kem_vector_portable_serialize_12_b8(coefficient, bytes);
    Eurydice_slice_copy(
        Eurydice_array_to_subslice3(serialized, (size_t)24U * i0,
                                    (size_t)24U * i0 + (size_t)24U, uint8_t *),
        Eurydice_array_to_slice((size_t)24U, bytes, uint8_t), uint8_t);
  }
  memcpy(ret, serialized, (size_t)384U * sizeof(uint8_t));
}

/**
 Call [`serialize_uncompressed_ring_element`] for each ring element.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_vector
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_serialize_vector_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *key,
    Eurydice_slice out) {
  for (size_t i = (size_t)0U;
       i < Eurydice_slice_len(
               Eurydice_array_to_slice(
                   (size_t)3U, key,
                   libcrux_ml_kem_polynomial_PolynomialRingElement_1d),
               libcrux_ml_kem_polynomial_PolynomialRingElement_1d);
       i++) {
    size_t i0 = i;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d re = key[i0];
    Eurydice_slice uu____0 = Eurydice_slice_subslice3(
        out, i0 * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
        (i0 + (size_t)1U) * LIBCRUX_ML_KEM_CONSTANTS_BYTES_PER_RING_ELEMENT,
        uint8_t *);
    uint8_t ret[384U];
    libcrux_ml_kem_serialize_serialize_uncompressed_ring_element_ea(&re, ret);
    Eurydice_slice_copy(
        uu____0, Eurydice_array_to_slice((size_t)384U, ret, uint8_t), uint8_t);
  }
}

/**
 Concatenate `t` and `ρ` into the public key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_public_key_mut
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_serialize_public_key_mut_89(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *t_as_ntt,
    Eurydice_slice seed_for_a, uint8_t *serialized) {
  libcrux_ml_kem_ind_cpa_serialize_vector_1b(
      t_as_ntt,
      Eurydice_array_to_subslice3(
          serialized, (size_t)0U,
          libcrux_ml_kem_constants_ranked_bytes_per_ring_element((size_t)3U),
          uint8_t *));
  Eurydice_slice_copy(
      Eurydice_array_to_subslice_from(
          (size_t)1184U, serialized,
          libcrux_ml_kem_constants_ranked_bytes_per_ring_element((size_t)3U),
          uint8_t, size_t, uint8_t[]),
      seed_for_a, uint8_t);
}

/**
 Concatenate `t` and `ρ` into the public key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cpa_serialize_public_key_89(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *t_as_ntt,
    Eurydice_slice seed_for_a, uint8_t ret[1184U]) {
  uint8_t public_key_serialized[1184U] = {0U};
  libcrux_ml_kem_ind_cpa_serialize_public_key_mut_89(t_as_ntt, seed_for_a,
                                                     public_key_serialized);
  memcpy(ret, public_key_serialized, (size_t)1184U * sizeof(uint8_t));
}

/**
 Serialize the secret key from the unpacked key pair generation.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.serialize_unpacked_secret_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- PRIVATE_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
*/
static inline libcrux_ml_kem_utils_extraction_helper_Keypair768
libcrux_ml_kem_ind_cpa_serialize_unpacked_secret_key_6c(
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0 *public_key,
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0 *private_key) {
  uint8_t public_key_serialized[1184U];
  libcrux_ml_kem_ind_cpa_serialize_public_key_89(
      public_key->t_as_ntt,
      Eurydice_array_to_slice((size_t)32U, public_key->seed_for_A, uint8_t),
      public_key_serialized);
  uint8_t secret_key_serialized[1152U] = {0U};
  libcrux_ml_kem_ind_cpa_serialize_vector_1b(
      private_key->secret_as_ntt,
      Eurydice_array_to_slice((size_t)1152U, secret_key_serialized, uint8_t));
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_secret_key_serialized[1152U];
  memcpy(copy_of_secret_key_serialized, secret_key_serialized,
         (size_t)1152U * sizeof(uint8_t));
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_public_key_serialized[1184U];
  memcpy(copy_of_public_key_serialized, public_key_serialized,
         (size_t)1184U * sizeof(uint8_t));
  libcrux_ml_kem_utils_extraction_helper_Keypair768 lit;
  memcpy(lit.fst, copy_of_secret_key_serialized,
         (size_t)1152U * sizeof(uint8_t));
  memcpy(lit.snd, copy_of_public_key_serialized,
         (size_t)1184U * sizeof(uint8_t));
  return lit;
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- PRIVATE_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE libcrux_ml_kem_utils_extraction_helper_Keypair768
libcrux_ml_kem_ind_cpa_generate_keypair_ea(Eurydice_slice key_generation_seed) {
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0 private_key =
      libcrux_ml_kem_ind_cpa_unpacked_default_70_1b();
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0 public_key =
      libcrux_ml_kem_ind_cpa_unpacked_default_8b_1b();
  libcrux_ml_kem_ind_cpa_generate_keypair_unpacked_1c(
      key_generation_seed, &private_key, &public_key);
  return libcrux_ml_kem_ind_cpa_serialize_unpacked_secret_key_6c(&public_key,
                                                                 &private_key);
}

/**
 Serialize the secret key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.serialize_kem_secret_key_mut
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
- SERIALIZED_KEY_LEN= 2400
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_serialize_kem_secret_key_mut_d6(
    Eurydice_slice private_key, Eurydice_slice public_key,
    Eurydice_slice implicit_rejection_value, uint8_t *serialized) {
  size_t pointer = (size_t)0U;
  uint8_t *uu____0 = serialized;
  size_t uu____1 = pointer;
  size_t uu____2 = pointer;
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          uu____0, uu____1, uu____2 + Eurydice_slice_len(private_key, uint8_t),
          uint8_t *),
      private_key, uint8_t);
  pointer = pointer + Eurydice_slice_len(private_key, uint8_t);
  uint8_t *uu____3 = serialized;
  size_t uu____4 = pointer;
  size_t uu____5 = pointer;
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          uu____3, uu____4, uu____5 + Eurydice_slice_len(public_key, uint8_t),
          uint8_t *),
      public_key, uint8_t);
  pointer = pointer + Eurydice_slice_len(public_key, uint8_t);
  Eurydice_slice uu____6 = Eurydice_array_to_subslice3(
      serialized, pointer, pointer + LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE,
      uint8_t *);
  uint8_t ret[32U];
  libcrux_ml_kem_hash_functions_portable_H_4a_e0(public_key, ret);
  Eurydice_slice_copy(
      uu____6, Eurydice_array_to_slice((size_t)32U, ret, uint8_t), uint8_t);
  pointer = pointer + LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE;
  uint8_t *uu____7 = serialized;
  size_t uu____8 = pointer;
  size_t uu____9 = pointer;
  Eurydice_slice_copy(
      Eurydice_array_to_subslice3(
          uu____7, uu____8,
          uu____9 + Eurydice_slice_len(implicit_rejection_value, uint8_t),
          uint8_t *),
      implicit_rejection_value, uint8_t);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.serialize_kem_secret_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
- SERIALIZED_KEY_LEN= 2400
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cca_serialize_kem_secret_key_d6(
    Eurydice_slice private_key, Eurydice_slice public_key,
    Eurydice_slice implicit_rejection_value, uint8_t ret[2400U]) {
  uint8_t out[2400U] = {0U};
  libcrux_ml_kem_ind_cca_serialize_kem_secret_key_mut_d6(
      private_key, public_key, implicit_rejection_value, out);
  memcpy(ret, out, (size_t)2400U * sizeof(uint8_t));
}

/**
 Packed API

 Generate a key pair.

 Depending on the `Vector` and `Hasher` used, this requires different hardware
 features
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- CPA_PRIVATE_KEY_SIZE= 1152
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE libcrux_ml_kem_mlkem768_MlKem768KeyPair
libcrux_ml_kem_ind_cca_generate_keypair_15(uint8_t *randomness) {
  Eurydice_slice ind_cpa_keypair_randomness = Eurydice_array_to_subslice3(
      randomness, (size_t)0U,
      LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t *);
  Eurydice_slice implicit_rejection_value = Eurydice_array_to_subslice_from(
      (size_t)64U, randomness,
      LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t,
      size_t, uint8_t[]);
  libcrux_ml_kem_utils_extraction_helper_Keypair768 uu____0 =
      libcrux_ml_kem_ind_cpa_generate_keypair_ea(ind_cpa_keypair_randomness);
  uint8_t ind_cpa_private_key[1152U];
  memcpy(ind_cpa_private_key, uu____0.fst, (size_t)1152U * sizeof(uint8_t));
  uint8_t public_key[1184U];
  memcpy(public_key, uu____0.snd, (size_t)1184U * sizeof(uint8_t));
  uint8_t secret_key_serialized[2400U];
  libcrux_ml_kem_ind_cca_serialize_kem_secret_key_d6(
      Eurydice_array_to_slice((size_t)1152U, ind_cpa_private_key, uint8_t),
      Eurydice_array_to_slice((size_t)1184U, public_key, uint8_t),
      implicit_rejection_value, secret_key_serialized);
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_secret_key_serialized[2400U];
  memcpy(copy_of_secret_key_serialized, secret_key_serialized,
         (size_t)2400U * sizeof(uint8_t));
  libcrux_ml_kem_types_MlKemPrivateKey_d9 private_key =
      libcrux_ml_kem_types_from_77_28(copy_of_secret_key_serialized);
  libcrux_ml_kem_types_MlKemPrivateKey_d9 uu____2 = private_key;
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_public_key[1184U];
  memcpy(copy_of_public_key, public_key, (size_t)1184U * sizeof(uint8_t));
  return libcrux_ml_kem_types_from_17_74(
      uu____2, libcrux_ml_kem_types_from_fd_d0(copy_of_public_key));
}

/**
 Portable generate key pair.
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.generate_keypair with const
generics
- K= 3
- CPA_PRIVATE_KEY_SIZE= 1152
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static inline libcrux_ml_kem_mlkem768_MlKem768KeyPair
libcrux_ml_kem_ind_cca_instantiations_portable_generate_keypair_ce(
    uint8_t *randomness) {
  return libcrux_ml_kem_ind_cca_generate_keypair_15(randomness);
}

/**
 Generate ML-KEM 768 Key Pair
*/
static inline libcrux_ml_kem_mlkem768_MlKem768KeyPair
libcrux_ml_kem_mlkem768_portable_generate_key_pair(uint8_t randomness[64U]) {
  return libcrux_ml_kem_ind_cca_instantiations_portable_generate_keypair_ce(
      randomness);
}

/**
 Validate an ML-KEM private key.

 This implements the Hash check in 7.3 3.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_private_key_only
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
- SECRET_KEY_SIZE= 2400
*/
static KRML_MUSTINLINE bool libcrux_ml_kem_ind_cca_validate_private_key_only_d6(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key) {
  uint8_t t[32U];
  libcrux_ml_kem_hash_functions_portable_H_4a_e0(
      Eurydice_array_to_subslice3(private_key->value, (size_t)384U * (size_t)3U,
                                  (size_t)768U * (size_t)3U + (size_t)32U,
                                  uint8_t *),
      t);
  Eurydice_slice expected = Eurydice_array_to_subslice3(
      private_key->value, (size_t)768U * (size_t)3U + (size_t)32U,
      (size_t)768U * (size_t)3U + (size_t)64U, uint8_t *);
  return Eurydice_array_eq_slice((size_t)32U, t, &expected, uint8_t, bool);
}

/**
 Validate an ML-KEM private key.

 This implements the Hash check in 7.3 3.
 Note that the size checks in 7.2 1 and 2 are covered by the `SECRET_KEY_SIZE`
 and `CIPHERTEXT_SIZE` in the `private_key` and `ciphertext` types.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_private_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CIPHERTEXT_SIZE= 1088
*/
static KRML_MUSTINLINE bool libcrux_ml_kem_ind_cca_validate_private_key_37(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *_ciphertext) {
  return libcrux_ml_kem_ind_cca_validate_private_key_only_d6(private_key);
}

/**
 Private key validation
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.validate_private_key with const
generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CIPHERTEXT_SIZE= 1088
*/
static KRML_MUSTINLINE bool
libcrux_ml_kem_ind_cca_instantiations_portable_validate_private_key_31(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext) {
  return libcrux_ml_kem_ind_cca_validate_private_key_37(private_key,
                                                        ciphertext);
}

/**
 Validate a private key.

 Returns `true` if valid, and `false` otherwise.
*/
static inline bool libcrux_ml_kem_mlkem768_portable_validate_private_key(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext) {
  return libcrux_ml_kem_ind_cca_instantiations_portable_validate_private_key_31(
      private_key, ciphertext);
}

/**
 Private key validation
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.validate_private_key_only with
const generics
- K= 3
- SECRET_KEY_SIZE= 2400
*/
static KRML_MUSTINLINE bool
libcrux_ml_kem_ind_cca_instantiations_portable_validate_private_key_only_41(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key) {
  return libcrux_ml_kem_ind_cca_validate_private_key_only_d6(private_key);
}

/**
 Validate the private key only.

 Returns `true` if valid, and `false` otherwise.
*/
static inline bool libcrux_ml_kem_mlkem768_portable_validate_private_key_only(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key) {
  return libcrux_ml_kem_ind_cca_instantiations_portable_validate_private_key_only_41(
      private_key);
}

/**
This function found in impl {core::ops::function::FnMut<(usize),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]> for
libcrux_ml_kem::serialize::deserialize_ring_elements_reduced_out::closure<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced_out.call_mut_0b with
types libcrux_ml_kem_vector_portable_vector_type_PortableVector with const
generics
- K= 3
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_serialize_deserialize_ring_elements_reduced_out_call_mut_0b_1b(
    void **_, size_t tupled_args) {
  return libcrux_ml_kem_polynomial_ZERO_d6_ea();
}

/**
 This function deserializes ring elements and reduces the result by the field
 modulus.

 This function MUST NOT be used on secret inputs.
*/
/**
A monomorphic instance of
libcrux_ml_kem.serialize.deserialize_ring_elements_reduced_out with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_serialize_deserialize_ring_elements_reduced_out_1b(
    Eurydice_slice public_key,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret[3U]) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d deserialized_pk[3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    deserialized_pk[i] =
        libcrux_ml_kem_serialize_deserialize_ring_elements_reduced_out_call_mut_0b_1b(
            &lvalue, i);
  }
  libcrux_ml_kem_serialize_deserialize_ring_elements_reduced_1b(
      public_key, deserialized_pk);
  memcpy(
      ret, deserialized_pk,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
}

/**
 Validate an ML-KEM public key.

 This implements the Modulus check in 7.2 2.
 Note that the size check in 7.2 1 is covered by the `PUBLIC_KEY_SIZE` in the
 `public_key` type.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.validate_public_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE bool libcrux_ml_kem_ind_cca_validate_public_key_89(
    uint8_t *public_key) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d deserialized_pk[3U];
  libcrux_ml_kem_serialize_deserialize_ring_elements_reduced_out_1b(
      Eurydice_array_to_subslice_to(
          (size_t)1184U, public_key,
          libcrux_ml_kem_constants_ranked_bytes_per_ring_element((size_t)3U),
          uint8_t, size_t, uint8_t[]),
      deserialized_pk);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d *uu____0 = deserialized_pk;
  uint8_t public_key_serialized[1184U];
  libcrux_ml_kem_ind_cpa_serialize_public_key_89(
      uu____0,
      Eurydice_array_to_subslice_from(
          (size_t)1184U, public_key,
          libcrux_ml_kem_constants_ranked_bytes_per_ring_element((size_t)3U),
          uint8_t, size_t, uint8_t[]),
      public_key_serialized);
  return Eurydice_array_eq((size_t)1184U, public_key, public_key_serialized,
                           uint8_t);
}

/**
 Public key validation
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.validate_public_key with const
generics
- K= 3
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE bool
libcrux_ml_kem_ind_cca_instantiations_portable_validate_public_key_41(
    uint8_t *public_key) {
  return libcrux_ml_kem_ind_cca_validate_public_key_89(public_key);
}

/**
 Validate a public key.

 Returns `true` if valid, and `false` otherwise.
*/
static inline bool libcrux_ml_kem_mlkem768_portable_validate_public_key(
    libcrux_ml_kem_types_MlKemPublicKey_30 *public_key) {
  return libcrux_ml_kem_ind_cca_instantiations_portable_validate_public_key_41(
      public_key->value);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.MlKemPublicKeyUnpacked
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- $3size_t
*/
typedef struct libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0_s {
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0 ind_cpa_public_key;
  uint8_t public_key_hash[32U];
} libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0;

typedef libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768PublicKeyUnpacked;

/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.unpacked.MlKemPrivateKeyUnpacked with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- $3size_t
*/
typedef struct libcrux_ml_kem_ind_cca_unpacked_MlKemPrivateKeyUnpacked_a0_s {
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPrivateKeyUnpacked_a0
      ind_cpa_private_key;
  uint8_t implicit_rejection_value[32U];
} libcrux_ml_kem_ind_cca_unpacked_MlKemPrivateKeyUnpacked_a0;

typedef struct
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked_s {
  libcrux_ml_kem_ind_cca_unpacked_MlKemPrivateKeyUnpacked_a0 private_key;
  libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 public_key;
} libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked;

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.decapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CPA_SECRET_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- CIPHERTEXT_SIZE= 1088
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- C1_BLOCK_SIZE= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
- IMPLICIT_REJECTION_HASH_INPUT_SIZE= 1120
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cca_unpacked_decapsulate_51(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *key_pair,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext, uint8_t ret[32U]) {
  uint8_t decrypted[32U];
  libcrux_ml_kem_ind_cpa_decrypt_unpacked_42(
      &key_pair->private_key.ind_cpa_private_key, ciphertext->value, decrypted);
  uint8_t to_hash0[64U];
  libcrux_ml_kem_utils_into_padded_array_24(
      Eurydice_array_to_slice((size_t)32U, decrypted, uint8_t), to_hash0);
  Eurydice_slice uu____0 = Eurydice_array_to_subslice_from(
      (size_t)64U, to_hash0, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
      uint8_t, size_t, uint8_t[]);
  Eurydice_slice_copy(
      uu____0,
      Eurydice_array_to_slice((size_t)32U, key_pair->public_key.public_key_hash,
                              uint8_t),
      uint8_t);
  uint8_t hashed[64U];
  libcrux_ml_kem_hash_functions_portable_G_4a_e0(
      Eurydice_array_to_slice((size_t)64U, to_hash0, uint8_t), hashed);
  Eurydice_slice_uint8_t_x2 uu____1 = Eurydice_slice_split_at(
      Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
      LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
      Eurydice_slice_uint8_t_x2);
  Eurydice_slice shared_secret = uu____1.fst;
  Eurydice_slice pseudorandomness = uu____1.snd;
  uint8_t to_hash[1120U];
  libcrux_ml_kem_utils_into_padded_array_15(
      Eurydice_array_to_slice(
          (size_t)32U, key_pair->private_key.implicit_rejection_value, uint8_t),
      to_hash);
  Eurydice_slice uu____2 = Eurydice_array_to_subslice_from(
      (size_t)1120U, to_hash, LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE,
      uint8_t, size_t, uint8_t[]);
  Eurydice_slice_copy(uu____2, libcrux_ml_kem_types_as_ref_d3_80(ciphertext),
                      uint8_t);
  uint8_t implicit_rejection_shared_secret[32U];
  libcrux_ml_kem_hash_functions_portable_PRF_4a_41(
      Eurydice_array_to_slice((size_t)1120U, to_hash, uint8_t),
      implicit_rejection_shared_secret);
  uint8_t expected_ciphertext[1088U];
  libcrux_ml_kem_ind_cpa_encrypt_unpacked_2a(
      &key_pair->public_key.ind_cpa_public_key, decrypted, pseudorandomness,
      expected_ciphertext);
  uint8_t selector =
      libcrux_ml_kem_constant_time_ops_compare_ciphertexts_in_constant_time(
          libcrux_ml_kem_types_as_ref_d3_80(ciphertext),
          Eurydice_array_to_slice((size_t)1088U, expected_ciphertext, uint8_t));
  uint8_t ret0[32U];
  libcrux_ml_kem_constant_time_ops_select_shared_secret_in_constant_time(
      shared_secret,
      Eurydice_array_to_slice((size_t)32U, implicit_rejection_shared_secret,
                              uint8_t),
      selector, ret0);
  memcpy(ret, ret0, (size_t)32U * sizeof(uint8_t));
}

/**
 Unpacked decapsulate
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.unpacked.decapsulate with const
generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CPA_SECRET_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- CIPHERTEXT_SIZE= 1088
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- C1_BLOCK_SIZE= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
- IMPLICIT_REJECTION_HASH_INPUT_SIZE= 1120
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_decapsulate_35(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *key_pair,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext, uint8_t ret[32U]) {
  libcrux_ml_kem_ind_cca_unpacked_decapsulate_51(key_pair, ciphertext, ret);
}

/**
 Decapsulate ML-KEM 768 (unpacked)

 Generates an [`MlKemSharedSecret`].
 The input is a reference to an unpacked key pair of type
 [`MlKem768KeyPairUnpacked`] and an [`MlKem768Ciphertext`].
*/
static inline void libcrux_ml_kem_mlkem768_portable_unpacked_decapsulate(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
        *private_key,
    libcrux_ml_kem_mlkem768_MlKem768Ciphertext *ciphertext, uint8_t ret[32U]) {
  libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_decapsulate_35(
      private_key, ciphertext, ret);
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.encaps_prepare
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]]
with const generics
- K= 3
*/
static inline void libcrux_ml_kem_ind_cca_unpacked_encaps_prepare_9c(
    Eurydice_slice randomness, Eurydice_slice pk_hash, uint8_t ret[64U]) {
  uint8_t to_hash[64U];
  libcrux_ml_kem_utils_into_padded_array_24(randomness, to_hash);
  Eurydice_slice_copy(
      Eurydice_array_to_subslice_from((size_t)64U, to_hash,
                                      LIBCRUX_ML_KEM_CONSTANTS_H_DIGEST_SIZE,
                                      uint8_t, size_t, uint8_t[]),
      pk_hash, uint8_t);
  uint8_t ret0[64U];
  libcrux_ml_kem_hash_functions_portable_G_4a_e0(
      Eurydice_array_to_slice((size_t)64U, to_hash, uint8_t), ret0);
  memcpy(ret, ret0, (size_t)64U * sizeof(uint8_t));
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.encapsulate
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]] with const
generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- PUBLIC_KEY_SIZE= 1184
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- VECTOR_U_BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE tuple_c2 libcrux_ml_kem_ind_cca_unpacked_encapsulate_0c(
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *public_key,
    uint8_t *randomness) {
  uint8_t hashed[64U];
  libcrux_ml_kem_ind_cca_unpacked_encaps_prepare_9c(
      Eurydice_array_to_slice((size_t)32U, randomness, uint8_t),
      Eurydice_array_to_slice((size_t)32U, public_key->public_key_hash,
                              uint8_t),
      hashed);
  Eurydice_slice_uint8_t_x2 uu____0 = Eurydice_slice_split_at(
      Eurydice_array_to_slice((size_t)64U, hashed, uint8_t),
      LIBCRUX_ML_KEM_CONSTANTS_SHARED_SECRET_SIZE, uint8_t,
      Eurydice_slice_uint8_t_x2);
  Eurydice_slice shared_secret = uu____0.fst;
  Eurydice_slice pseudorandomness = uu____0.snd;
  uint8_t ciphertext[1088U];
  libcrux_ml_kem_ind_cpa_encrypt_unpacked_2a(&public_key->ind_cpa_public_key,
                                             randomness, pseudorandomness,
                                             ciphertext);
  uint8_t shared_secret_array[32U] = {0U};
  Eurydice_slice_copy(
      Eurydice_array_to_slice((size_t)32U, shared_secret_array, uint8_t),
      shared_secret, uint8_t);
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_ciphertext[1088U];
  memcpy(copy_of_ciphertext, ciphertext, (size_t)1088U * sizeof(uint8_t));
  libcrux_ml_kem_mlkem768_MlKem768Ciphertext uu____2 =
      libcrux_ml_kem_types_from_e0_80(copy_of_ciphertext);
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_shared_secret_array[32U];
  memcpy(copy_of_shared_secret_array, shared_secret_array,
         (size_t)32U * sizeof(uint8_t));
  tuple_c2 lit;
  lit.fst = uu____2;
  memcpy(lit.snd, copy_of_shared_secret_array, (size_t)32U * sizeof(uint8_t));
  return lit;
}

/**
 Unpacked encapsulate
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.unpacked.encapsulate with const
generics
- K= 3
- CIPHERTEXT_SIZE= 1088
- PUBLIC_KEY_SIZE= 1184
- T_AS_NTT_ENCODED_SIZE= 1152
- C1_SIZE= 960
- C2_SIZE= 128
- VECTOR_U_COMPRESSION_FACTOR= 10
- VECTOR_V_COMPRESSION_FACTOR= 4
- VECTOR_U_BLOCK_LEN= 320
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
- ETA2= 2
- ETA2_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE tuple_c2
libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_encapsulate_cd(
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *public_key,
    uint8_t *randomness) {
  return libcrux_ml_kem_ind_cca_unpacked_encapsulate_0c(public_key, randomness);
}

/**
 Encapsulate ML-KEM 768 (unpacked)

 Generates an ([`MlKem768Ciphertext`], [`MlKemSharedSecret`]) tuple.
 The input is a reference to an unpacked public key of type
 [`MlKem768PublicKeyUnpacked`], the SHA3-256 hash of this public key, and
 [`SHARED_SECRET_SIZE`] bytes of `randomness`.
*/
static inline tuple_c2 libcrux_ml_kem_mlkem768_portable_unpacked_encapsulate(
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *public_key,
    uint8_t randomness[32U]) {
  return libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_encapsulate_cd(
      public_key, randomness);
}

/**
This function found in impl {core::ops::function::FnMut<(usize),
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1]> for
libcrux_ml_kem::ind_cca::unpacked::transpose_a::closure::closure<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.unpacked.transpose_a.closure.call_mut_b4 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_ind_cca_unpacked_transpose_a_closure_call_mut_b4_1b(
    void **_, size_t tupled_args) {
  return libcrux_ml_kem_polynomial_ZERO_d6_ea();
}

/**
This function found in impl {core::ops::function::FnMut<(usize),
@Array<libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@1], K>> for
libcrux_ml_kem::ind_cca::unpacked::transpose_a::closure<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.unpacked.transpose_a.call_mut_7b with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
*/
static inline void libcrux_ml_kem_ind_cca_unpacked_transpose_a_call_mut_7b_1b(
    void **_, size_t tupled_args,
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret[3U]) {
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    ret[i] = libcrux_ml_kem_ind_cca_unpacked_transpose_a_closure_call_mut_b4_1b(
        &lvalue, i);
  }
}

/**
This function found in impl {core::clone::Clone for
libcrux_ml_kem::polynomial::PolynomialRingElement<Vector>[TraitClause@0,
TraitClause@2]}
*/
/**
A monomorphic instance of libcrux_ml_kem.polynomial.clone_c1
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics

*/
static inline libcrux_ml_kem_polynomial_PolynomialRingElement_1d
libcrux_ml_kem_polynomial_clone_c1_ea(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d *self) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d lit;
  libcrux_ml_kem_vector_portable_vector_type_PortableVector ret[16U];
  core_array__core__clone__Clone_for__Array_T__N___clone(
      (size_t)16U, self->coefficients, ret,
      libcrux_ml_kem_vector_portable_vector_type_PortableVector, void *);
  memcpy(lit.coefficients, ret,
         (size_t)16U *
             sizeof(libcrux_ml_kem_vector_portable_vector_type_PortableVector));
  return lit;
}

/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.transpose_a
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static inline void libcrux_ml_kem_ind_cca_unpacked_transpose_a_1b(
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d ind_cpa_a[3U][3U],
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret[3U][3U]) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d A[3U][3U];
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    /* original Rust expression is not an lvalue in C */
    void *lvalue = (void *)0U;
    libcrux_ml_kem_ind_cca_unpacked_transpose_a_call_mut_7b_1b(&lvalue, i,
                                                               A[i]);
  }
  for (size_t i = (size_t)0U; i < (size_t)3U; i++) {
    size_t i0 = i;
    libcrux_ml_kem_polynomial_PolynomialRingElement_1d _a_i[3U][3U];
    memcpy(_a_i, A,
           (size_t)3U *
               sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]));
    for (size_t i1 = (size_t)0U; i1 < (size_t)3U; i1++) {
      size_t j = i1;
      libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0 =
          libcrux_ml_kem_polynomial_clone_c1_ea(&ind_cpa_a[j][i0]);
      A[i0][j] = uu____0;
    }
  }
  memcpy(ret, A,
         (size_t)3U *
             sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]));
}

/**
 Generate Unpacked Keys
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.generate_keypair
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector,
libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_variant_MlKem with const generics
- K= 3
- CPA_PRIVATE_KEY_SIZE= 1152
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE void libcrux_ml_kem_ind_cca_unpacked_generate_keypair_15(
    uint8_t randomness[64U],
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *out) {
  Eurydice_slice ind_cpa_keypair_randomness = Eurydice_array_to_subslice3(
      randomness, (size_t)0U,
      LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t *);
  Eurydice_slice implicit_rejection_value = Eurydice_array_to_subslice_from(
      (size_t)64U, randomness,
      LIBCRUX_ML_KEM_CONSTANTS_CPA_PKE_KEY_GENERATION_SEED_SIZE, uint8_t,
      size_t, uint8_t[]);
  libcrux_ml_kem_ind_cpa_generate_keypair_unpacked_1c(
      ind_cpa_keypair_randomness, &out->private_key.ind_cpa_private_key,
      &out->public_key.ind_cpa_public_key);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0[3U][3U];
  memcpy(uu____0, out->public_key.ind_cpa_public_key.A,
         (size_t)3U *
             sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d A[3U][3U];
  libcrux_ml_kem_ind_cca_unpacked_transpose_a_1b(uu____0, A);
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____1[3U][3U];
  memcpy(uu____1, A,
         (size_t)3U *
             sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]));
  memcpy(out->public_key.ind_cpa_public_key.A, uu____1,
         (size_t)3U *
             sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]));
  uint8_t pk_serialized[1184U];
  libcrux_ml_kem_ind_cpa_serialize_public_key_89(
      out->public_key.ind_cpa_public_key.t_as_ntt,
      Eurydice_array_to_slice(
          (size_t)32U, out->public_key.ind_cpa_public_key.seed_for_A, uint8_t),
      pk_serialized);
  uint8_t uu____2[32U];
  libcrux_ml_kem_hash_functions_portable_H_4a_e0(
      Eurydice_array_to_slice((size_t)1184U, pk_serialized, uint8_t), uu____2);
  memcpy(out->public_key.public_key_hash, uu____2,
         (size_t)32U * sizeof(uint8_t));
  uint8_t uu____3[32U];
  Result_fb dst;
  Eurydice_slice_to_array2(&dst, implicit_rejection_value, Eurydice_slice,
                           uint8_t[32U], TryFromSliceError);
  unwrap_26_b3(dst, uu____3);
  memcpy(out->private_key.implicit_rejection_value, uu____3,
         (size_t)32U * sizeof(uint8_t));
}

/**
 Generate a key pair
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.unpacked.generate_keypair with
const generics
- K= 3
- CPA_PRIVATE_KEY_SIZE= 1152
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
- ETA1= 2
- ETA1_RANDOMNESS_SIZE= 128
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_generate_keypair_ce(
    uint8_t randomness[64U],
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *out) {
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_randomness[64U];
  memcpy(copy_of_randomness, randomness, (size_t)64U * sizeof(uint8_t));
  libcrux_ml_kem_ind_cca_unpacked_generate_keypair_15(copy_of_randomness, out);
}

/**
 Generate ML-KEM 768 Key Pair in "unpacked" form.
*/
static inline void
libcrux_ml_kem_mlkem768_portable_unpacked_generate_key_pair_mut(
    uint8_t randomness[64U],
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
        *key_pair) {
  /* Passing arrays by value in Rust generates a copy in C */
  uint8_t copy_of_randomness[64U];
  memcpy(copy_of_randomness, randomness, (size_t)64U * sizeof(uint8_t));
  libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_generate_keypair_ce(
      copy_of_randomness, key_pair);
}

/**
This function found in impl {core::default::Default for
libcrux_ml_kem::ind_cca::unpacked::MlKemPublicKeyUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.default_30
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0
libcrux_ml_kem_ind_cca_unpacked_default_30_1b(void) {
  return (
      KRML_CLITERAL(libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0){
          .ind_cpa_public_key = libcrux_ml_kem_ind_cpa_unpacked_default_8b_1b(),
          .public_key_hash = {0U}});
}

/**
This function found in impl {core::default::Default for
libcrux_ml_kem::ind_cca::unpacked::MlKemKeyPairUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.default_7b
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
    libcrux_ml_kem_ind_cca_unpacked_default_7b_1b(void) {
  libcrux_ml_kem_ind_cca_unpacked_MlKemPrivateKeyUnpacked_a0 uu____0 = {
      .ind_cpa_private_key = libcrux_ml_kem_ind_cpa_unpacked_default_70_1b(),
      .implicit_rejection_value = {0U}};
  return (KRML_CLITERAL(
      libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked){
      .private_key = uu____0,
      .public_key = libcrux_ml_kem_ind_cca_unpacked_default_30_1b()});
}

/**
 Generate ML-KEM 768 Key Pair in "unpacked" form.
*/
static inline libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
libcrux_ml_kem_mlkem768_portable_unpacked_generate_key_pair(
    uint8_t randomness[64U]) {
  libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked key_pair =
      libcrux_ml_kem_ind_cca_unpacked_default_7b_1b();
  uint8_t uu____0[64U];
  memcpy(uu____0, randomness, (size_t)64U * sizeof(uint8_t));
  libcrux_ml_kem_mlkem768_portable_unpacked_generate_key_pair_mut(uu____0,
                                                                  &key_pair);
  return key_pair;
}

/**
 Create a new, empty unpacked key.
*/
static inline libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
libcrux_ml_kem_mlkem768_portable_unpacked_init_key_pair(void) {
  return libcrux_ml_kem_ind_cca_unpacked_default_7b_1b();
}

/**
 Create a new, empty unpacked public key.
*/
static inline libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0
libcrux_ml_kem_mlkem768_portable_unpacked_init_public_key(void) {
  return libcrux_ml_kem_ind_cca_unpacked_default_30_1b();
}

/**
 Take a serialized private key and generate an unpacked key pair from it.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.keys_from_private_key
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CPA_SECRET_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- T_AS_NTT_ENCODED_SIZE= 1152
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_unpacked_keys_from_private_key_42(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
        *key_pair) {
  Eurydice_slice_uint8_t_x4 uu____0 =
      libcrux_ml_kem_types_unpack_private_key_b4(
          Eurydice_array_to_slice((size_t)2400U, private_key->value, uint8_t));
  Eurydice_slice ind_cpa_secret_key = uu____0.fst;
  Eurydice_slice ind_cpa_public_key = uu____0.snd;
  Eurydice_slice ind_cpa_public_key_hash = uu____0.thd;
  Eurydice_slice implicit_rejection_value = uu____0.f3;
  libcrux_ml_kem_ind_cpa_deserialize_vector_1b(
      ind_cpa_secret_key,
      key_pair->private_key.ind_cpa_private_key.secret_as_ntt);
  libcrux_ml_kem_ind_cpa_build_unpacked_public_key_mut_3f(
      ind_cpa_public_key, &key_pair->public_key.ind_cpa_public_key);
  Eurydice_slice_copy(
      Eurydice_array_to_slice((size_t)32U, key_pair->public_key.public_key_hash,
                              uint8_t),
      ind_cpa_public_key_hash, uint8_t);
  Eurydice_slice_copy(
      Eurydice_array_to_slice(
          (size_t)32U, key_pair->private_key.implicit_rejection_value, uint8_t),
      implicit_rejection_value, uint8_t);
  Eurydice_slice_copy(
      Eurydice_array_to_slice(
          (size_t)32U, key_pair->public_key.ind_cpa_public_key.seed_for_A,
          uint8_t),
      Eurydice_slice_subslice_from(ind_cpa_public_key, (size_t)1152U, uint8_t,
                                   size_t, uint8_t[]),
      uint8_t);
}

/**
 Take a serialized private key and generate an unpacked key pair from it.
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.unpacked.keypair_from_private_key
with const generics
- K= 3
- SECRET_KEY_SIZE= 2400
- CPA_SECRET_KEY_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
- T_AS_NTT_ENCODED_SIZE= 1152
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_keypair_from_private_key_fd(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
        *key_pair) {
  libcrux_ml_kem_ind_cca_unpacked_keys_from_private_key_42(private_key,
                                                           key_pair);
}

/**
 Get an unpacked key from a private key.
*/
static inline void
libcrux_ml_kem_mlkem768_portable_unpacked_key_pair_from_private_mut(
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *private_key,
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
        *key_pair) {
  libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_keypair_from_private_key_fd(
      private_key, key_pair);
}

/**
This function found in impl
{libcrux_ml_kem::ind_cca::unpacked::MlKemKeyPairUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.unpacked.serialized_private_key_mut_11 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- CPA_PRIVATE_KEY_SIZE= 1152
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_unpacked_serialized_private_key_mut_11_43(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *self,
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *serialized) {
  libcrux_ml_kem_utils_extraction_helper_Keypair768 uu____0 =
      libcrux_ml_kem_ind_cpa_serialize_unpacked_secret_key_6c(
          &self->public_key.ind_cpa_public_key,
          &self->private_key.ind_cpa_private_key);
  uint8_t ind_cpa_private_key[1152U];
  memcpy(ind_cpa_private_key, uu____0.fst, (size_t)1152U * sizeof(uint8_t));
  uint8_t ind_cpa_public_key[1184U];
  memcpy(ind_cpa_public_key, uu____0.snd, (size_t)1184U * sizeof(uint8_t));
  libcrux_ml_kem_ind_cca_serialize_kem_secret_key_mut_d6(
      Eurydice_array_to_slice((size_t)1152U, ind_cpa_private_key, uint8_t),
      Eurydice_array_to_slice((size_t)1184U, ind_cpa_public_key, uint8_t),
      Eurydice_array_to_slice(
          (size_t)32U, self->private_key.implicit_rejection_value, uint8_t),
      serialized->value);
}

/**
This function found in impl
{libcrux_ml_kem::ind_cca::unpacked::MlKemKeyPairUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.unpacked.serialized_private_key_11 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- CPA_PRIVATE_KEY_SIZE= 1152
- PRIVATE_KEY_SIZE= 2400
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE libcrux_ml_kem_types_MlKemPrivateKey_d9
libcrux_ml_kem_ind_cca_unpacked_serialized_private_key_11_43(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *self) {
  libcrux_ml_kem_types_MlKemPrivateKey_d9 sk =
      libcrux_ml_kem_types_default_d3_28();
  libcrux_ml_kem_ind_cca_unpacked_serialized_private_key_mut_11_43(self, &sk);
  return sk;
}

/**
 Get the serialized private key.
*/
static inline libcrux_ml_kem_types_MlKemPrivateKey_d9
libcrux_ml_kem_mlkem768_portable_unpacked_key_pair_serialized_private_key(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
        *key_pair) {
  return libcrux_ml_kem_ind_cca_unpacked_serialized_private_key_11_43(key_pair);
}

/**
 Get the serialized private key.
*/
static inline void
libcrux_ml_kem_mlkem768_portable_unpacked_key_pair_serialized_private_key_mut(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *key_pair,
    libcrux_ml_kem_types_MlKemPrivateKey_d9 *serialized) {
  libcrux_ml_kem_ind_cca_unpacked_serialized_private_key_mut_11_43(key_pair,
                                                                   serialized);
}

/**
This function found in impl
{libcrux_ml_kem::ind_cca::unpacked::MlKemPublicKeyUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.serialized_dd
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE libcrux_ml_kem_types_MlKemPublicKey_30
libcrux_ml_kem_ind_cca_unpacked_serialized_dd_89(
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *self) {
  uint8_t ret[1184U];
  libcrux_ml_kem_ind_cpa_serialize_public_key_89(
      self->ind_cpa_public_key.t_as_ntt,
      Eurydice_array_to_slice((size_t)32U, self->ind_cpa_public_key.seed_for_A,
                              uint8_t),
      ret);
  return libcrux_ml_kem_types_from_fd_d0(ret);
}

/**
This function found in impl
{libcrux_ml_kem::ind_cca::unpacked::MlKemKeyPairUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.unpacked.serialized_public_key_11 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE libcrux_ml_kem_types_MlKemPublicKey_30
libcrux_ml_kem_ind_cca_unpacked_serialized_public_key_11_89(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *self) {
  return libcrux_ml_kem_ind_cca_unpacked_serialized_dd_89(&self->public_key);
}

/**
 Get the serialized public key.
*/
static inline libcrux_ml_kem_types_MlKemPublicKey_30
libcrux_ml_kem_mlkem768_portable_unpacked_key_pair_serialized_public_key(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked
        *key_pair) {
  return libcrux_ml_kem_ind_cca_unpacked_serialized_public_key_11_89(key_pair);
}

/**
This function found in impl
{libcrux_ml_kem::ind_cca::unpacked::MlKemPublicKeyUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.serialized_mut_dd
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_unpacked_serialized_mut_dd_89(
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *self,
    libcrux_ml_kem_types_MlKemPublicKey_30 *serialized) {
  libcrux_ml_kem_ind_cpa_serialize_public_key_mut_89(
      self->ind_cpa_public_key.t_as_ntt,
      Eurydice_array_to_slice((size_t)32U, self->ind_cpa_public_key.seed_for_A,
                              uint8_t),
      serialized->value);
}

/**
This function found in impl
{libcrux_ml_kem::ind_cca::unpacked::MlKemKeyPairUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.unpacked.serialized_public_key_mut_11 with types
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_unpacked_serialized_public_key_mut_11_89(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *self,
    libcrux_ml_kem_types_MlKemPublicKey_30 *serialized) {
  libcrux_ml_kem_ind_cca_unpacked_serialized_mut_dd_89(&self->public_key,
                                                       serialized);
}

/**
 Get the serialized public key.
*/
static inline void
libcrux_ml_kem_mlkem768_portable_unpacked_key_pair_serialized_public_key_mut(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *key_pair,
    libcrux_ml_kem_types_MlKemPublicKey_30 *serialized) {
  libcrux_ml_kem_ind_cca_unpacked_serialized_public_key_mut_11_89(key_pair,
                                                                  serialized);
}

/**
This function found in impl {core::clone::Clone for
libcrux_ml_kem::ind_cpa::unpacked::IndCpaPublicKeyUnpacked<Vector,
K>[TraitClause@0, TraitClause@2]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cpa.unpacked.clone_91
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static inline libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0
libcrux_ml_kem_ind_cpa_unpacked_clone_91_1b(
    libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0 *self) {
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d uu____0[3U];
  core_array__core__clone__Clone_for__Array_T__N___clone(
      (size_t)3U, self->t_as_ntt, uu____0,
      libcrux_ml_kem_polynomial_PolynomialRingElement_1d, void *);
  uint8_t uu____1[32U];
  core_array__core__clone__Clone_for__Array_T__N___clone(
      (size_t)32U, self->seed_for_A, uu____1, uint8_t, void *);
  libcrux_ml_kem_ind_cpa_unpacked_IndCpaPublicKeyUnpacked_a0 lit;
  memcpy(
      lit.t_as_ntt, uu____0,
      (size_t)3U * sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d));
  memcpy(lit.seed_for_A, uu____1, (size_t)32U * sizeof(uint8_t));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d ret[3U][3U];
  core_array__core__clone__Clone_for__Array_T__N___clone(
      (size_t)3U, self->A, ret,
      libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U], void *);
  memcpy(lit.A, ret,
         (size_t)3U *
             sizeof(libcrux_ml_kem_polynomial_PolynomialRingElement_1d[3U]));
  return lit;
}

/**
This function found in impl {core::clone::Clone for
libcrux_ml_kem::ind_cca::unpacked::MlKemPublicKeyUnpacked<Vector,
K>[TraitClause@0, TraitClause@2]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.clone_d7
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static inline libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0
libcrux_ml_kem_ind_cca_unpacked_clone_d7_1b(
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *self) {
  libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 lit;
  lit.ind_cpa_public_key =
      libcrux_ml_kem_ind_cpa_unpacked_clone_91_1b(&self->ind_cpa_public_key);
  uint8_t ret[32U];
  core_array__core__clone__Clone_for__Array_T__N___clone(
      (size_t)32U, self->public_key_hash, ret, uint8_t, void *);
  memcpy(lit.public_key_hash, ret, (size_t)32U * sizeof(uint8_t));
  return lit;
}

/**
This function found in impl
{libcrux_ml_kem::ind_cca::unpacked::MlKemKeyPairUnpacked<Vector,
K>[TraitClause@0, TraitClause@1]}
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.public_key_11
with types libcrux_ml_kem_vector_portable_vector_type_PortableVector
with const generics
- K= 3
*/
static KRML_MUSTINLINE libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *
libcrux_ml_kem_ind_cca_unpacked_public_key_11_1b(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *self) {
  return &self->public_key;
}

/**
 Get the unpacked public key.
*/
static inline void libcrux_ml_kem_mlkem768_portable_unpacked_public_key(
    libcrux_ml_kem_mlkem768_portable_unpacked_MlKem768KeyPairUnpacked *key_pair,
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *pk) {
  libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 uu____0 =
      libcrux_ml_kem_ind_cca_unpacked_clone_d7_1b(
          libcrux_ml_kem_ind_cca_unpacked_public_key_11_1b(key_pair));
  pk[0U] = uu____0;
}

/**
 Get the serialized public key.
*/
static inline void
libcrux_ml_kem_mlkem768_portable_unpacked_serialized_public_key(
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0 *public_key,
    libcrux_ml_kem_types_MlKemPublicKey_30 *serialized) {
  libcrux_ml_kem_ind_cca_unpacked_serialized_mut_dd_89(public_key, serialized);
}

/**
 Generate an unpacked key from a serialized key.
*/
/**
A monomorphic instance of libcrux_ml_kem.ind_cca.unpacked.unpack_public_key
with types libcrux_ml_kem_hash_functions_portable_PortableHash[[$3size_t]],
libcrux_ml_kem_vector_portable_vector_type_PortableVector with const generics
- K= 3
- T_AS_NTT_ENCODED_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_unpacked_unpack_public_key_0a(
    libcrux_ml_kem_types_MlKemPublicKey_30 *public_key,
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0
        *unpacked_public_key) {
  Eurydice_slice uu____0 =
      Eurydice_array_to_subslice_to((size_t)1184U, public_key->value,
                                    (size_t)1152U, uint8_t, size_t, uint8_t[]);
  libcrux_ml_kem_serialize_deserialize_ring_elements_reduced_1b(
      uu____0, unpacked_public_key->ind_cpa_public_key.t_as_ntt);
  uint8_t uu____1[32U];
  libcrux_ml_kem_utils_into_padded_array_9e(
      Eurydice_array_to_subslice_from((size_t)1184U, public_key->value,
                                      (size_t)1152U, uint8_t, size_t,
                                      uint8_t[]),
      uu____1);
  memcpy(unpacked_public_key->ind_cpa_public_key.seed_for_A, uu____1,
         (size_t)32U * sizeof(uint8_t));
  libcrux_ml_kem_polynomial_PolynomialRingElement_1d(*uu____2)[3U] =
      unpacked_public_key->ind_cpa_public_key.A;
  uint8_t ret[34U];
  libcrux_ml_kem_utils_into_padded_array_b6(
      Eurydice_array_to_subslice_from((size_t)1184U, public_key->value,
                                      (size_t)1152U, uint8_t, size_t,
                                      uint8_t[]),
      ret);
  libcrux_ml_kem_matrix_sample_matrix_A_2b(uu____2, ret, false);
  uint8_t uu____3[32U];
  libcrux_ml_kem_hash_functions_portable_H_4a_e0(
      Eurydice_array_to_slice((size_t)1184U,
                              libcrux_ml_kem_types_as_slice_e6_d0(public_key),
                              uint8_t),
      uu____3);
  memcpy(unpacked_public_key->public_key_hash, uu____3,
         (size_t)32U * sizeof(uint8_t));
}

/**
 Get the unpacked public key.
*/
/**
A monomorphic instance of
libcrux_ml_kem.ind_cca.instantiations.portable.unpacked.unpack_public_key with
const generics
- K= 3
- T_AS_NTT_ENCODED_SIZE= 1152
- PUBLIC_KEY_SIZE= 1184
*/
static KRML_MUSTINLINE void
libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_unpack_public_key_31(
    libcrux_ml_kem_types_MlKemPublicKey_30 *public_key,
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0
        *unpacked_public_key) {
  libcrux_ml_kem_ind_cca_unpacked_unpack_public_key_0a(public_key,
                                                       unpacked_public_key);
}

/**
 Get the unpacked public key.
*/
static inline void
libcrux_ml_kem_mlkem768_portable_unpacked_unpacked_public_key(
    libcrux_ml_kem_types_MlKemPublicKey_30 *public_key,
    libcrux_ml_kem_ind_cca_unpacked_MlKemPublicKeyUnpacked_a0
        *unpacked_public_key) {
  libcrux_ml_kem_ind_cca_instantiations_portable_unpacked_unpack_public_key_31(
      public_key, unpacked_public_key);
}

#if defined(__cplusplus)
}
#endif

#define libcrux_mlkem768_portable_H_DEFINED
#endif /* libcrux_mlkem768_portable_H */


/* rename some types to be a bit more ergonomic */
#define libcrux_mlkem768_keypair libcrux_ml_kem_mlkem768_MlKem768KeyPair_s
#define libcrux_mlkem768_pk libcrux_ml_kem_types_MlKemPublicKey_30_s
#define libcrux_mlkem768_sk libcrux_ml_kem_types_MlKemPrivateKey_d9_s
#define libcrux_mlkem768_ciphertext libcrux_ml_kem_mlkem768_MlKem768Ciphertext_s
#define libcrux_mlkem768_enc_result tuple_c2_s
/* defines for PRNG inputs */
#define LIBCRUX_ML_KEM_KEY_PAIR_PRNG_LEN 64U
#define LIBCRUX_ML_KEM_ENC_PRNG_LEN 32
