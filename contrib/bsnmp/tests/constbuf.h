/**
 * Copyright (c) 2019-2020 Hartmut Brandt.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef constbuf_h_1578777513
#define constbuf_h_1578777513

#include <array>
#include <cassert>
#include <cstdint>

#if !defined(HAVE_EXPR_IN_ARRAY_SIZE) && (defined(__clang__) || (defined(__GNUC__) && (__GNUC__>= 9 || (__GNUC__ == 9 && __GNUC_MINOR__ >= 1))))
#define HAVE_EXPR_IN_ARRAY_SIZE 1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif

#ifndef HAVE_EXPR_IN_ARRAY_SIZE
#include <vector>
#endif

namespace test {
namespace detail {

enum class Constbuf_mode {
    BIN,
    COMMENT,
    HEX,
    CHECK,
    GOTO,
};

template<typename A>
constexpr bool
count_comment(A c, Constbuf_mode &mode)
{
    if (c == '\n')
        mode = Constbuf_mode::BIN;
    return false;
}

template<typename A>
constexpr bool
count_hex(A c, Constbuf_mode &mode, std::size_t &bits)
{
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F')) {
        if (bits % 4 != 0)
            throw "unaligned hex digit";
        bits += 4;
        return false;
    }
    if (c == ':')
        return false;
    mode = Constbuf_mode::BIN;
    return true;
}

template<typename A, typename U>
constexpr bool
handle_hex(A c, Constbuf_mode &mode, std::size_t &bit, U &n)
{
    if (c >= '0' && c <= '9') {
        n[bit / 8] |= ((c - '0') << 4) >> (bit % 8);
        bit += 4;
        return false;
    }
    if (c >= 'a' && c <= 'f') {
        n[bit / 8] |= ((c - 'a' + 10) << 4) >> (bit % 8);
        bit += 4;
        return false;
    }
    if (c >= 'A' && c <= 'F') {
        n[bit / 8] |= ((c - 'A' + 10) << 4) >> (bit % 8);
        bit += 4;
        return false;
    }
    if (c == ':')
        return false;
    mode = Constbuf_mode::BIN;
    return true;
}

template<typename A>
constexpr bool
count_check(A c, Constbuf_mode &mode, std::size_t &)
{
    if (c >= '0' && c <= '9')
        return false;
    mode = Constbuf_mode::BIN;
    return true;
}

template<typename A>
constexpr bool
handle_check(A c, Constbuf_mode &mode, std::size_t &bits, std::size_t &addr)
{
    if (c >= '0' && c <= '9') {
        addr = 10 * addr + c - '0';
        return false;
    }
    if (bits % 8 != 0 || bits / 8 != addr)
        throw "address check failed";
    mode = Constbuf_mode::BIN;
    return true;
}

template<typename A>
constexpr bool
count_goto(A c, Constbuf_mode &mode, std::size_t &bits, std::size_t &addr)
{
    if (c >= '0' && c <= '9') {
        addr = 10 * addr + c - '0';
        return false;
    }
    if (8 * addr < bits)
        throw "cannot go backwards";
    bits = 8 * addr;
    mode = Constbuf_mode::BIN;
    return true;
}

template<typename A>
constexpr bool
count_bin(A c, Constbuf_mode &mode, std::size_t &bits, std::size_t &addr)
{
    if (c == ' ' || c == '\t' || c == '\n')
        /* just ignore */
        return false;
    if (c == ';') {
        mode = Constbuf_mode::COMMENT;
        return false;
    }
    if (c == 'x' || c == 'X') {
        mode = Constbuf_mode::HEX;
        return false;
    }
    if (c == '!') {
        mode = Constbuf_mode::CHECK;
        return false;
    }
    if (c == '@') {
        mode = Constbuf_mode::GOTO;
        addr = 0;
        return false;
    }
    if (c == '0' || c == '1' || c == '.') {
        bits++;
        return false;
    }
    throw "bad character";
}

template<typename A, typename U>
constexpr bool
handle_bin(A c, Constbuf_mode &mode, std::size_t &bit, std::size_t &addr, U &n)
{
    if (c == ' ' || c == '\t' || c == '\n')
        /* just ignore */
        return false;
    if (c == ';') {
        mode = Constbuf_mode::COMMENT;
        return false;
    }
    if (c == 'x' || c == 'X') {
        mode = Constbuf_mode::HEX;
        return false;
    }
    if (c == '!') {
        mode = Constbuf_mode::CHECK;
        addr = 0;
        return false;
    }
    if (c == '@') {
        mode = Constbuf_mode::GOTO;
        addr = 0;
        return false;
    }
    if (c == '0' || c == '.') {
        bit++;
        return false;
    }
    if (c == '1') {
        n[bit / 8] |= 0x80 >> (bit % 8);
        bit++;
        return false;
    }
    throw "bad character";
}

/**
 * Count the bits in the test buffer. For a syntax see below.
 *
 * \tparam A    buffer base character type
 * \tparam a    characters
 *
 * \return number of bits required
 */
template<typename A, A ...a>
constexpr std::size_t
count_bits()
{
    std::size_t bits {0};
    std::size_t addr {0};
    auto mode = Constbuf_mode::BIN;

    for (auto c : {a...}) {
        for (bool again = true; again; again = false) {
            switch (mode) {
            case Constbuf_mode::COMMENT:
                again = count_comment(c, mode);
                break;
            case Constbuf_mode::CHECK:
                again = count_check(c, mode, bits);
                break;
            case Constbuf_mode::GOTO:
                again = count_goto(c, mode, bits, addr);
                break;
            case Constbuf_mode::HEX:
                again = count_hex(c, mode, bits);
                break;
            case Constbuf_mode::BIN:
                again = count_bin(c, mode, bits, addr);
                break;
            }
        }
    }
    return bits;
}

}

template<typename A, A ...a>
#ifdef HAVE_EXPR_IN_ARRAY_SIZE
constexpr auto
#else
auto
#endif
constbuf()
{
#ifdef HAVE_EXPR_IN_ARRAY_SIZE
    std::array<uint8_t, (detail::count_bits<A, a...>() + 7) / 8> n {};
#else
    std::vector<uint8_t> n((detail::count_bits<A, a...>() + 7) / 8);
#endif
    using namespace detail;

    std::size_t bit {0};
    std::size_t addr {0};
    auto mode = Constbuf_mode::BIN;

    for (auto c : {a...}) {
        for (bool again = true; again; again = false) {
            switch (mode) {
            case Constbuf_mode::COMMENT:
                again = count_comment(c, mode);
                break;
            case Constbuf_mode::CHECK:
                again = handle_check(c, mode, bit, addr);
                break;
            case Constbuf_mode::GOTO:
                again = count_goto(c, mode, bit, addr);
                break;
            case Constbuf_mode::HEX:
                again = handle_hex(c, mode, bit, n);
                break;
            case Constbuf_mode::BIN:
                again = handle_bin(c, mode, bit, addr, n);
                break;
            }
        }
    }
    return n;
}

inline namespace literals {
inline namespace cbuf_literals {

#ifdef HAVE_EXPR_IN_ARRAY_SIZE
template<typename A, A ...a>
constexpr auto
#else
template<typename A, A ...a>
auto
#endif
operator ""_cbuf()
{
	return test::constbuf<A, a...>();
}

} /* namespace cbuf_literals */
} /* namespace literals */

} /* namespace test */

#endif
