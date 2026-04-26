//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___BIT_POPCOUNT_H
#define _LIBCPP___BIT_POPCOUNT_H

#include <__config>
#include <__type_traits/integer_traits.h>
#include <limits>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_PUSH_MACROS
#include <__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _Tp>
[[__nodiscard__]] _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR int __popcount_impl(_Tp __t) _NOEXCEPT {
  if _LIBCPP_CONSTEXPR (sizeof(_Tp) <= sizeof(unsigned int)) {
    return __builtin_popcount(static_cast<unsigned int>(__t));
  } else if _LIBCPP_CONSTEXPR (sizeof(_Tp) <= sizeof(unsigned long)) {
    return __builtin_popcountl(static_cast<unsigned long>(__t));
  } else if _LIBCPP_CONSTEXPR (sizeof(_Tp) <= sizeof(unsigned long long)) {
    return __builtin_popcountll(static_cast<unsigned long long>(__t));
  } else {
#if _LIBCPP_STD_VER == 11
    return __t != 0 ? __builtin_popcountll(static_cast<unsigned long long>(__t)) +
                          std::__popcount_impl<_Tp>(__t >> numeric_limits<unsigned long long>::digits)
                    : 0;
#else
    int __ret = 0;
    while (__t != 0) {
      __ret += __builtin_popcountll(static_cast<unsigned long long>(__t));
      __t >>= std::numeric_limits<unsigned long long>::digits;
    }
    return __ret;
#endif
  }
}

template <class _Tp>
[[__nodiscard__]] _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR int __popcount(_Tp __t) _NOEXCEPT {
  static_assert(__is_unsigned_integer_v<_Tp>, "__popcount only works with unsigned types");
#if __has_builtin(__builtin_popcountg) // TODO (LLVM 21): This can be dropped once we only support Clang >= 19.
  return __builtin_popcountg(__t);
#else
  return std::__popcount_impl(__t);
#endif
}

#if _LIBCPP_STD_VER >= 20

template <__unsigned_integer _Tp>
[[nodiscard]] _LIBCPP_HIDE_FROM_ABI constexpr int popcount(_Tp __t) noexcept {
  return std::__popcount(__t);
}

#endif

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP___BIT_POPCOUNT_H
