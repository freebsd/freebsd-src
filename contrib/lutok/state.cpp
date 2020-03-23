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

extern "C" {
#include <unistd.h>
}

#include <cassert>
#include <cstring>

#include "c_gate.hpp"
#include "exceptions.hpp"
#include "state.ipp"


namespace {


/// Wrapper around lua_getglobal to run in a protected environment.
///
/// \pre stack(-1) is the name of the global to get.
/// \post stack(-1) is the value of the global.
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_getglobal(lua_State* state)
{
    lua_getglobal(state, lua_tostring(state, -1));
    return 1;
}


/// Wrapper around lua_gettable to run in a protected environment.
///
/// \pre stack(-2) is the table to get the element from.
/// \pre stack(-1) is the table index.
/// \post stack(-1) is the value of stack(-2)[stack(-1)].
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_gettable(lua_State* state)
{
    lua_gettable(state, -2);
    return 1;
}


/// Wrapper around lua_next to run in a protected environment.
///
/// \pre stack(-2) is the table to get the next element from.
/// \pre stack(-1) is the last processed key.
/// \post stack(-1) is the value of next(stack(-2), stack(-1)).
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_next(lua_State* state)
{
    const int more = lua_next(state, -2) != 0;
    lua_pushboolean(state, more);
    return more ? 3 : 1;
}


/// Wrapper around lua_setglobal to run in a protected environment.
///
/// \pre stack(-2) is the name of the global to set.
/// \pre stack(-1) is the value to set the global to.
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_setglobal(lua_State* state)
{
    lua_setglobal(state, lua_tostring(state, -2));
    return 0;
}


/// Wrapper around lua_settable to run in a protected environment.
///
/// \pre stack(-3) is the table to set the element into.
/// \pre stack(-2) is the table index.
/// \pre stack(-1) is the value to set.
///
/// \param state The Lua C API state.
///
/// \return The number of return values pushed onto the stack.
static int
protected_settable(lua_State* state)
{
    lua_settable(state, -3);
    return 0;
}


/// Calls a C++ Lua function from a C calling environment.
///
/// Any errors reported by the C++ function are caught and reported to the
/// caller as Lua errors.
///
/// \param function The C++ function to call.
/// \param raw_state The raw Lua state.
///
/// \return The number of return values pushed onto the Lua stack by the
/// function.
static int
call_cxx_function_from_c(lutok::cxx_function function,
                         lua_State* raw_state) throw()
{
    char error_buf[1024];

    try {
        lutok::state state = lutok::state_c_gate::connect(raw_state);
        return function(state);
    } catch (const std::exception& e) {
        std::strncpy(error_buf, e.what(), sizeof(error_buf));
    } catch (...) {
        std::strncpy(error_buf, "Unhandled exception in Lua C++ hook",
                     sizeof(error_buf));
    }
    error_buf[sizeof(error_buf) - 1] = '\0';
    // We raise the Lua error from outside the try/catch context and we use
    // a stack-based buffer to hold the message to ensure that we do not leak
    // any C++ objects (and, as a likely result, memory) when Lua performs its
    // longjmp.
    return luaL_error(raw_state, "%s", error_buf);
}


/// Lua glue to call a C++ closure.
///
/// This Lua binding is actually a closure that we have constructed from the
/// state.push_cxx_closure() method.  The closure contains the same upvalues
/// provided by the user plus an extra upvalue that contains the address of the
/// C++ function we have to call.  All we do here is safely delegate the
/// execution to the wrapped C++ closure.
///
/// \param raw_state The Lua C API state.
///
/// \return The number of return values of the called closure.
static int
cxx_closure_trampoline(lua_State* raw_state)
{
    lutok::state state = lutok::state_c_gate::connect(raw_state);

    int nupvalues;
    {
        lua_Debug debug;
        lua_getstack(raw_state, 0, &debug);
        lua_getinfo(raw_state, "u", &debug);
        nupvalues = debug.nups;
    }

    lutok::cxx_function* function = state.to_userdata< lutok::cxx_function >(
        state.upvalue_index(nupvalues));
    return call_cxx_function_from_c(*function, raw_state);
}


/// Lua glue to call a C++ function.
///
/// This Lua binding is actually a closure that we have constructed from the
/// state.push_cxx_function() method.  The closure has a single upvalue that
/// contains the address of the C++ function we have to call.  All we do here is
/// safely delegate the execution to the wrapped C++ function.
///
/// \param raw_state The Lua C API state.
///
/// \return The number of return values of the called function.
static int
cxx_function_trampoline(lua_State* raw_state)
{
    lutok::state state = lutok::state_c_gate::connect(raw_state);
    lutok::cxx_function* function = state.to_userdata< lutok::cxx_function >(
        state.upvalue_index(1));
    return call_cxx_function_from_c(*function, raw_state);
}


}  // anonymous namespace


const int lutok::registry_index = LUA_REGISTRYINDEX;


/// Internal implementation for lutok::state.
struct lutok::state::impl {
    /// The Lua internal state.
    lua_State* lua_state;

    /// Whether we own the state or not (to decide if we close it).
    bool owned;

    /// Constructor.
    ///
    /// \param lua_ The Lua internal state.
    /// \param owned_ Whether we own the state or not.
    impl(lua_State* lua_, bool owned_) :
        lua_state(lua_),
        owned(owned_)
    {
    }
};


/// Initializes the Lua state.
///
/// You must share the same state object alongside the lifetime of your Lua
/// session.  As soon as the object is destroyed, the session is terminated.
lutok::state::state(void)
{
    lua_State* lua = luaL_newstate();
    if (lua == NULL)
        throw lutok::error("lua open failed");
    _pimpl.reset(new impl(lua, true));
}


/// Initializes the Lua state from an existing raw state.
///
/// Instances constructed using this method do NOT own the raw state.  This
/// means that, on exit, the state will not be destroyed.
///
/// \param raw_state_ The raw Lua state to wrap.
lutok::state::state(void* raw_state_) :
    _pimpl(new impl(reinterpret_cast< lua_State* >(raw_state_), false))
{
}


/// Destructor for the Lua state.
///
/// Closes the session unless it has already been closed by calling the
/// close() method.  It is recommended to explicitly close the session in the
/// code.
lutok::state::~state(void)
{
    if (_pimpl->owned && _pimpl->lua_state != NULL)
        close();
}


/// Terminates this Lua session.
///
/// It is recommended to call this instead of relying on the destructor to do
/// the cleanup, but it is not a requirement to use close().
///
/// \pre close() has not yet been called.
/// \pre The Lua stack is empty.  This is not truly necessary but ensures that
///     our code is consistent and clears the stack explicitly.
void
lutok::state::close(void)
{
    assert(_pimpl->lua_state != NULL);
    assert(lua_gettop(_pimpl->lua_state) == 0);
    lua_close(_pimpl->lua_state);
    _pimpl->lua_state = NULL;
}


/// Wrapper around lua_getglobal.
///
/// \param name The second parameter to lua_getglobal.
///
/// \throw api_error If lua_getglobal fails.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lutok::state::get_global(const std::string& name)
{
    lua_pushcfunction(_pimpl->lua_state, protected_getglobal);
    lua_pushstring(_pimpl->lua_state, name.c_str());
    if (lua_pcall(_pimpl->lua_state, 1, 1, 0) != 0)
        throw lutok::api_error::from_stack(*this, "lua_getglobal");
}


/// Pushes a reference to the global table onto the stack.
///
/// This is a wrapper around the incompatible differences between Lua 5.1 and
/// 5.2 to access to the globals table.
///
/// \post state(-1) Contains the reference to the globals table.
void
lutok::state::get_global_table(void)
{
#if LUA_VERSION_NUM >= 502
    lua_pushvalue(_pimpl->lua_state, registry_index);
    lua_pushinteger(_pimpl->lua_state, LUA_RIDX_GLOBALS);
    lua_gettable(_pimpl->lua_state, -2);
    lua_remove(_pimpl->lua_state, -2);
#else
    lua_pushvalue(_pimpl->lua_state, LUA_GLOBALSINDEX);
#endif
}


/// Wrapper around luaL_getmetafield.
///
/// \param index The second parameter to luaL_getmetafield.
/// \param name The third parameter to luaL_getmetafield.
///
/// \return The return value of luaL_getmetafield.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
bool
lutok::state::get_metafield(const int index, const std::string& name)
{
    return luaL_getmetafield(_pimpl->lua_state, index, name.c_str()) != 0;
}


/// Wrapper around lua_getmetatable.
///
/// \param index The second parameter to lua_getmetatable.
///
/// \return The return value of lua_getmetatable.
bool
lutok::state::get_metatable(const int index)
{
    return lua_getmetatable(_pimpl->lua_state, index) != 0;
}


/// Wrapper around lua_gettable.
///
/// \param index The second parameter to lua_gettable.
///
/// \throw api_error If lua_gettable fails.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lutok::state::get_table(const int index)
{
    assert(lua_gettop(_pimpl->lua_state) >= 2);
    lua_pushcfunction(_pimpl->lua_state, protected_gettable);
    lua_pushvalue(_pimpl->lua_state, index < 0 ? index - 1 : index);
    lua_pushvalue(_pimpl->lua_state, -3);
    if (lua_pcall(_pimpl->lua_state, 2, 1, 0) != 0)
        throw lutok::api_error::from_stack(*this, "lua_gettable");
    lua_remove(_pimpl->lua_state, -2);
}


/// Wrapper around lua_gettop.
///
/// \return The return value of lua_gettop.
int
lutok::state::get_top(void)
{
    return lua_gettop(_pimpl->lua_state);
}


/// Wrapper around lua_insert.
///
/// \param index The second parameter to lua_insert.
void
lutok::state::insert(const int index)
{
    lua_insert(_pimpl->lua_state, index);
}


/// Wrapper around lua_isboolean.
///
/// \param index The second parameter to lua_isboolean.
///
/// \return The return value of lua_isboolean.
bool
lutok::state::is_boolean(const int index)
{
    return lua_isboolean(_pimpl->lua_state, index);
}


/// Wrapper around lua_isfunction.
///
/// \param index The second parameter to lua_isfunction.
///
/// \return The return value of lua_isfunction.
bool
lutok::state::is_function(const int index)
{
    return lua_isfunction(_pimpl->lua_state, index);
}


/// Wrapper around lua_isnil.
///
/// \param index The second parameter to lua_isnil.
///
/// \return The return value of lua_isnil.
bool
lutok::state::is_nil(const int index)
{
    return lua_isnil(_pimpl->lua_state, index);
}


/// Wrapper around lua_isnumber.
///
/// \param index The second parameter to lua_isnumber.
///
/// \return The return value of lua_isnumber.
bool
lutok::state::is_number(const int index)
{
    return lua_isnumber(_pimpl->lua_state, index);
}


/// Wrapper around lua_isstring.
///
/// \param index The second parameter to lua_isstring.
///
/// \return The return value of lua_isstring.
bool
lutok::state::is_string(const int index)
{
    return lua_isstring(_pimpl->lua_state, index);
}


/// Wrapper around lua_istable.
///
/// \param index The second parameter to lua_istable.
///
/// \return The return value of lua_istable.
bool
lutok::state::is_table(const int index)
{
    return lua_istable(_pimpl->lua_state, index);
}


/// Wrapper around lua_isuserdata.
///
/// \param index The second parameter to lua_isuserdata.
///
/// \return The return value of lua_isuserdata.
bool
lutok::state::is_userdata(const int index)
{
    return lua_isuserdata(_pimpl->lua_state, index);
}


/// Wrapper around luaL_loadfile.
///
/// \param file The second parameter to luaL_loadfile.
///
/// \throw api_error If luaL_loadfile returns an error.
/// \throw file_not_found_error If the file cannot be accessed.
///
/// \warning Terminates execution if there is not enough memory.
void
lutok::state::load_file(const std::string& file)
{
    if (::access(file.c_str(), R_OK) == -1)
        throw lutok::file_not_found_error(file);
    if (luaL_loadfile(_pimpl->lua_state, file.c_str()) != 0)
        throw lutok::api_error::from_stack(*this, "luaL_loadfile");
}


/// Wrapper around luaL_loadstring.
///
/// \param str The second parameter to luaL_loadstring.
///
/// \throw api_error If luaL_loadstring returns an error.
///
/// \warning Terminates execution if there is not enough memory.
void
lutok::state::load_string(const std::string& str)
{
    if (luaL_loadstring(_pimpl->lua_state, str.c_str()) != 0)
        throw lutok::api_error::from_stack(*this, "luaL_loadstring");
}


/// Wrapper around lua_newtable.
///
/// \warning Terminates execution if there is not enough memory.
void
lutok::state::new_table(void)
{
    lua_newtable(_pimpl->lua_state);
}


/// Wrapper around lua_newuserdata.
///
/// This is internal.  The public type-safe interface of this method should be
/// used instead.
///
/// \param size The second parameter to lua_newuserdata.
///
/// \return The return value of lua_newuserdata.
///
/// \warning Terminates execution if there is not enough memory.
void*
lutok::state::new_userdata_voidp(const size_t size)
{
    return lua_newuserdata(_pimpl->lua_state, size);
}


/// Wrapper around lua_next.
///
/// \param index The second parameter to lua_next.
///
/// \return True if there are more elements to process; false otherwise.
///
/// \warning Terminates execution if there is not enough memory.
bool
lutok::state::next(const int index)
{
    assert(lua_istable(_pimpl->lua_state, index));
    assert(lua_gettop(_pimpl->lua_state) >= 1);
    lua_pushcfunction(_pimpl->lua_state, protected_next);
    lua_pushvalue(_pimpl->lua_state, index < 0 ? index - 1 : index);
    lua_pushvalue(_pimpl->lua_state, -3);
    if (lua_pcall(_pimpl->lua_state, 2, LUA_MULTRET, 0) != 0)
        throw lutok::api_error::from_stack(*this, "lua_next");
    const bool more = lua_toboolean(_pimpl->lua_state, -1);
    lua_pop(_pimpl->lua_state, 1);
    if (more)
        lua_remove(_pimpl->lua_state, -3);
    else
        lua_pop(_pimpl->lua_state, 1);
    return more;
}


/// Wrapper around luaL_openlibs.
///
/// \throw api_error If luaL_openlibs fails.
///
/// \warning Terminates execution if there is not enough memory.
void
lutok::state::open_all(void)
{
    luaL_openlibs(_pimpl->lua_state);
}


/// Wrapper around luaopen_base.
///
/// \throw api_error If luaopen_base fails.
///
/// \warning Terminates execution if there is not enough memory.
void
lutok::state::open_base(void)
{
    lua_pushcfunction(_pimpl->lua_state, luaopen_base);
    if (lua_pcall(_pimpl->lua_state, 0, 0, 0) != 0)
        throw lutok::api_error::from_stack(*this, "luaopen_base");
}


/// Wrapper around luaopen_string.
///
/// \throw api_error If luaopen_string fails.
///
/// \warning Terminates execution if there is not enough memory.
void
lutok::state::open_string(void)
{
#if LUA_VERSION_NUM >= 502
    luaL_requiref(_pimpl->lua_state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(_pimpl->lua_state, 1);
#else
    lua_pushcfunction(_pimpl->lua_state, luaopen_string);
    if (lua_pcall(_pimpl->lua_state, 0, 0, 0) != 0)
        throw lutok::api_error::from_stack(*this, "luaopen_string");
#endif
}


/// Wrapper around luaopen_table.
///
/// \throw api_error If luaopen_table fails.
///
/// \warning Terminates execution if there is not enough memory.
void
lutok::state::open_table(void)
{
#if LUA_VERSION_NUM >= 502
    luaL_requiref(_pimpl->lua_state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(_pimpl->lua_state, 1);
#else
    lua_pushcfunction(_pimpl->lua_state, luaopen_table);
    if (lua_pcall(_pimpl->lua_state, 0, 0, 0) != 0)
        throw lutok::api_error::from_stack(*this, "luaopen_table");
#endif
}


/// Wrapper around lua_pcall.
///
/// \param nargs The second parameter to lua_pcall.
/// \param nresults The third parameter to lua_pcall.
/// \param errfunc The fourth parameter to lua_pcall.
///
/// \throw api_error If lua_pcall returns an error.
void
lutok::state::pcall(const int nargs, const int nresults, const int errfunc)
{
    if (lua_pcall(_pimpl->lua_state, nargs, nresults, errfunc) != 0)
        throw lutok::api_error::from_stack(*this, "lua_pcall");
}


/// Wrapper around lua_pop.
///
/// \param count The second parameter to lua_pop.
void
lutok::state::pop(const int count)
{
    assert(count <= lua_gettop(_pimpl->lua_state));
    lua_pop(_pimpl->lua_state, count);
    assert(lua_gettop(_pimpl->lua_state) >= 0);
}


/// Wrapper around lua_pushboolean.
///
/// \param value The second parameter to lua_pushboolean.
void
lutok::state::push_boolean(const bool value)
{
    lua_pushboolean(_pimpl->lua_state, value ? 1 : 0);
}


/// Wrapper around lua_pushcclosure.
///
/// This is not a pure wrapper around lua_pushcclosure because this has to do
/// extra magic to allow passing C++ functions instead of plain C functions.
///
/// \param function The C++ function to be pushed as a closure.
/// \param nvalues The number of upvalues that the function receives.
void
lutok::state::push_cxx_closure(cxx_function function, const int nvalues)
{
    cxx_function *data = static_cast< cxx_function* >(
        lua_newuserdata(_pimpl->lua_state, sizeof(cxx_function)));
    *data = function;
    lua_pushcclosure(_pimpl->lua_state, cxx_closure_trampoline, nvalues + 1);
}


/// Wrapper around lua_pushcfunction.
///
/// This is not a pure wrapper around lua_pushcfunction because this has to do
/// extra magic to allow passing C++ functions instead of plain C functions.
///
/// \param function The C++ function to be pushed.
void
lutok::state::push_cxx_function(cxx_function function)
{
    cxx_function *data = static_cast< cxx_function* >(
        lua_newuserdata(_pimpl->lua_state, sizeof(cxx_function)));
    *data = function;
    lua_pushcclosure(_pimpl->lua_state, cxx_function_trampoline, 1);
}


/// Wrapper around lua_pushinteger.
///
/// \param value The second parameter to lua_pushinteger.
void
lutok::state::push_integer(const int value)
{
    lua_pushinteger(_pimpl->lua_state, value);
}


/// Wrapper around lua_pushnil.
void
lutok::state::push_nil(void)
{
    lua_pushnil(_pimpl->lua_state);
}


/// Wrapper around lua_pushstring.
///
/// \param str The second parameter to lua_pushstring.
///
/// \warning Terminates execution if there is not enough memory.
void
lutok::state::push_string(const std::string& str)
{
    lua_pushstring(_pimpl->lua_state, str.c_str());
}


/// Wrapper around lua_pushvalue.
///
/// \param index The second parameter to lua_pushvalue.
void
lutok::state::push_value(const int index)
{
    lua_pushvalue(_pimpl->lua_state, index);
}


/// Wrapper around lua_rawget.
///
/// \param index The second parameter to lua_rawget.
void
lutok::state::raw_get(const int index)
{
    lua_rawget(_pimpl->lua_state, index);
}


/// Wrapper around lua_rawset.
///
/// \param index The second parameter to lua_rawset.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lutok::state::raw_set(const int index)
{
    lua_rawset(_pimpl->lua_state, index);
}


/// Wrapper around lua_setglobal.
///
/// \param name The second parameter to lua_setglobal.
///
/// \throw api_error If lua_setglobal fails.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lutok::state::set_global(const std::string& name)
{
    lua_pushcfunction(_pimpl->lua_state, protected_setglobal);
    lua_pushstring(_pimpl->lua_state, name.c_str());
    lua_pushvalue(_pimpl->lua_state, -3);
    if (lua_pcall(_pimpl->lua_state, 2, 0, 0) != 0)
        throw lutok::api_error::from_stack(*this, "lua_setglobal");
    lua_pop(_pimpl->lua_state, 1);
}


/// Wrapper around lua_setmetatable.
///
/// \param index The second parameter to lua_setmetatable.
void
lutok::state::set_metatable(const int index)
{
    lua_setmetatable(_pimpl->lua_state, index);
}


/// Wrapper around lua_settable.
///
/// \param index The second parameter to lua_settable.
///
/// \throw api_error If lua_settable fails.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lutok::state::set_table(const int index)
{
    lua_pushcfunction(_pimpl->lua_state, protected_settable);
    lua_pushvalue(_pimpl->lua_state, index < 0 ? index - 1 : index);
    lua_pushvalue(_pimpl->lua_state, -4);
    lua_pushvalue(_pimpl->lua_state, -4);
    if (lua_pcall(_pimpl->lua_state, 3, 0, 0) != 0)
        throw lutok::api_error::from_stack(*this, "lua_settable");
    lua_pop(_pimpl->lua_state, 2);
}


/// Wrapper around lua_toboolean.
///
/// \param index The second parameter to lua_toboolean.
///
/// \return The return value of lua_toboolean.
bool
lutok::state::to_boolean(const int index)
{
    assert(is_boolean(index));
    return lua_toboolean(_pimpl->lua_state, index);
}


/// Wrapper around lua_tointeger.
///
/// \param index The second parameter to lua_tointeger.
///
/// \return The return value of lua_tointeger.
long
lutok::state::to_integer(const int index)
{
    assert(is_number(index));
    return lua_tointeger(_pimpl->lua_state, index);
}


/// Wrapper around lua_touserdata.
///
/// This is internal.  The public type-safe interface of this method should be
/// used instead.
///
/// \param index The second parameter to lua_touserdata.
///
/// \return The return value of lua_touserdata.
///
/// \warning Terminates execution if there is not enough memory.
void*
lutok::state::to_userdata_voidp(const int index)
{
    return lua_touserdata(_pimpl->lua_state, index);
}



/// Wrapper around lua_tostring.
///
/// \param index The second parameter to lua_tostring.
///
/// \return The return value of lua_tostring.
///
/// \warning Terminates execution if there is not enough memory.
std::string
lutok::state::to_string(const int index)
{
    assert(is_string(index));
    const char *raw_string = lua_tostring(_pimpl->lua_state, index);
    // Note that the creation of a string object below (explicit for clarity)
    // implies that the raw string is duplicated and, henceforth, the string is
    // safe even if the corresponding element is popped from the Lua stack.
    return std::string(raw_string);
}


/// Wrapper around lua_upvalueindex.
///
/// \param index The first parameter to lua_upvalueindex.
///
/// \return The return value of lua_upvalueindex.
int
lutok::state::upvalue_index(const int index)
{
    return lua_upvalueindex(index);
}


/// Gets the internal lua_State object.
///
/// \return The raw Lua state.  This is returned as a void pointer to prevent
/// including the lua.hpp header file from our public interface.  The only way
/// to call this method is by using the c_gate module, and c_gate takes care of
/// casting this object to the appropriate type.
void*
lutok::state::raw_state(void)
{
    return _pimpl->lua_state;
}
