//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___BIT_COUNTR_H
#define _LIBCPP___BIT_COUNTR_H

#include <__config>
#include <__type_traits/integer_traits.h>
#include <limits>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_PUSH_MACROS
#include <__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

// A constexpr implementation for C++11 and later (using clang extensions for constexpr support)
// Precondition: __t != 0 (the caller __countr_zero handles __t == 0 as a special case)
template <class _Tp>
[[__nodiscard__]] _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR int __countr_zero_impl(_Tp __t) _NOEXCEPT {
  _LIBCPP_ASSERT_INTERNAL(__t != 0, "__countr_zero_impl called with zero value");
  static_assert(__is_unsigned_integer_v<_Tp>, "__countr_zero_impl only works with unsigned types");
  if _LIBCPP_CONSTEXPR (sizeof(_Tp) <= sizeof(unsigned int)) {
    return __builtin_ctz(static_cast<unsigned int>(__t));
  } else if _LIBCPP_CONSTEXPR (sizeof(_Tp) <= sizeof(unsigned long)) {
    return __builtin_ctzl(static_cast<unsigned long>(__t));
  } else if _LIBCPP_CONSTEXPR (sizeof(_Tp) <= sizeof(unsigned long long)) {
    return __builtin_ctzll(static_cast<unsigned long long>(__t));
  } else {
#if _LIBCPP_STD_VER == 11
    unsigned long long __ull       = static_cast<unsigned long long>(__t);
    const unsigned int __ulldigits = numeric_limits<unsigned long long>::digits;
    return __ull == 0ull ? __ulldigits + std::__countr_zero_impl<_Tp>(__t >> __ulldigits) : __builtin_ctzll(__ull);
#else
    int __ret                      = 0;
    const unsigned int __ulldigits = numeric_limits<unsigned long long>::digits;
    while (static_cast<unsigned long long>(__t) == 0uLL) {
      __ret += __ulldigits;
      __t >>= __ulldigits;
    }
    return __ret + __builtin_ctzll(static_cast<unsigned long long>(__t));
#endif
  }
}

template <class _Tp>
[[__nodiscard__]] _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR int __countr_zero(_Tp __t) _NOEXCEPT {
  static_assert(__is_unsigned_integer_v<_Tp>, "__countr_zero only works with unsigned types");
#if __has_builtin(__builtin_ctzg) // TODO (LLVM 21): This can be dropped once we only support Clang >= 19.
  return __builtin_ctzg(__t, numeric_limits<_Tp>::digits);
#else
  return __t != 0 ? std::__countr_zero_impl(__t) : numeric_limits<_Tp>::digits;
#endif
}

#if _LIBCPP_STD_VER >= 20

template <__unsigned_integer _Tp>
[[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr int countr_zero(_Tp __t) noexcept {
  return std::__countr_zero(__t);
}

template <__unsigned_integer _Tp>
[[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr int countr_one(_Tp __t) noexcept {
  return __t != numeric_limits<_Tp>::max() ? std::countr_zero(static_cast<_Tp>(~__t)) : numeric_limits<_Tp>::digits;
}

#endif // _LIBCPP_STD_VER >= 20

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP___BIT_COUNTR_H
