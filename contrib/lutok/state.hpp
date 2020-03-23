// Copyright 2011 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file state.hpp
/// Provides the state wrapper class for the Lua C state.

#if !defined(LUTOK_STATE_HPP)
#define LUTOK_STATE_HPP

#include <string>

#if defined(_LIBCPP_VERSION) || __cplusplus >= 201103L
#include <memory>
#else
#include <tr1/memory>
#endif

namespace lutok {


class debug;
class state;


/// The type of a C++ function that can be bound into Lua.
///
/// Functions of this type are free to raise exceptions.  These will not
/// propagate into the Lua C API.  However, any such exceptions will be reported
/// as a Lua error and their type will be lost.
typedef int (*cxx_function)(state&);


/// Stack index constant pointing to the registry table.
extern const int registry_index;


/// A RAII model for the Lua state.
///
/// This class holds the state of the Lua interpreter during its existence and
/// provides wrappers around several Lua library functions that operate on such
/// state.
///
/// These wrapper functions differ from the C versions in that they use the
/// implicit state hold by the class, they use C++ types where appropriate and
/// they use exceptions to report errors.
///
/// The wrappers intend to be as lightweight as possible but, in some
/// situations, they are pretty complex because they need to do extra work to
/// capture the errors reported by the Lua C API.  We prefer having fine-grained
/// error control rather than efficiency, so this is OK.
class state {
    struct impl;

    /// Pointer to the shared internal implementation.
#if defined(_LIBCPP_VERSION) || __cplusplus >= 201103L
    std::shared_ptr< impl > _pimpl;
#else
    std::tr1::shared_ptr< impl > _pimpl;
#endif

    void* new_userdata_voidp(const size_t);
    void* to_userdata_voidp(const int);

    friend class state_c_gate;
    explicit state(void*);
    void* raw_state(void);

public:
    state(void);
    ~state(void);

    void close(void);
    void get_global(const std::string&);
    void get_global_table(void);
    bool get_metafield(const int, const std::string&);
    bool get_metatable(const int);
    void get_table(const int);
    int get_top(void);
    void insert(const int);
    bool is_boolean(const int);
    bool is_function(const int);
    bool is_nil(const int);
    bool is_number(const int);
    bool is_string(const int);
    bool is_table(const int);
    bool is_userdata(const int);
    void load_file(const std::string&);
    void load_string(const std::string&);
    void new_table(void);
    template< typename Type > Type* new_userdata(void);
    bool next(const int);
    void open_all(void);
    void open_base(void);
    void open_string(void);
    void open_table(void);
    void pcall(const int, const int, const int);
    void pop(const int);
    void push_boolean(const bool);
    void push_cxx_closure(cxx_function, const int);
    void push_cxx_function(cxx_function);
    void push_integer(const int);
    void push_nil(void);
    void push_string(const std::string&);
    void push_value(const int);
    void raw_get(const int);
    void raw_set(const int);
    void set_global(const std::string&);
    void set_metatable(const int);
    void set_table(const int);
    bool to_boolean(const int);
    long to_integer(const int);
    template< typename Type > Type* to_userdata(const int);
    std::string to_string(const int);
    int upvalue_index(const int);
};


}  // namespace lutok

#endif  // !defined(LUTOK_STATE_HPP)
