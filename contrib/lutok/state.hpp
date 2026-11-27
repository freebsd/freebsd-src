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

#include <lua.hpp>
#include <memory>
#include <string>

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
    std::shared_ptr<impl> _pimpl;

    [[nodiscard]] void* new_userdata_voidp(size_t) const;
    [[nodiscard]] void* to_userdata_voidp(int) const;

    friend class state_c_gate;
    explicit state(void*);
    [[nodiscard]] void* raw_state(void) const;

public:
    state(void);
    ~state(void);

    void close(void) const;
    void get_global(const std::string&);
    void get_global_table(void) const;
    [[nodiscard]] bool get_metafield(int, const std::string&) const;
    [[nodiscard]] bool get_metatable(int) const;
    void get_table(int);
    [[nodiscard]] int get_top(void) const;
    void insert(int) const;
    [[nodiscard]] bool is_boolean(int) const;
    [[nodiscard]] bool is_function(int) const;
    [[nodiscard]] bool is_nil(int) const;
    [[nodiscard]] bool is_number(int) const;
    [[nodiscard]] bool is_string(int) const;
    [[nodiscard]] bool is_table(int) const;
    [[nodiscard]] bool is_userdata(int) const;
    void load_file(const std::string&);
    void load_string(const std::string&);
    void new_table(void) const;
    template <typename Type> Type* new_userdata(void);
    bool next(int);
    void open_all(void) const;
    void open_base(void);
    void open_string(void) const;
    void open_table(void) const;
    void pcall(int, int, int);
    void pop(int) const;
    void push_boolean(bool) const;
    void push_cxx_closure(cxx_function, int) const;
    void push_cxx_function(cxx_function) const;
    void push_integer(int) const;
    void push_nil(void) const;
    void push_string(const std::string&) const;
    void push_value(int) const;
    void raw_get(int) const;
    void raw_set(int) const;
    void set_global(const std::string&);
    void set_metatable(int) const;
    void set_table(int);
    [[nodiscard]] bool to_boolean(int) const;
    [[nodiscard]] lua_Integer to_integer(int) const;
    template <typename Type> Type* to_userdata(int);
    [[nodiscard]] std::string to_string(int) const;
    static int upvalue_index(int);
};

} // namespace lutok

#endif // !defined(LUTOK_STATE_HPP)
