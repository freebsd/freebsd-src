// Copyright 2012 The Kyua Authors.
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

#include "utils/config/nodes.ipp"

#include <atf-c++.hpp>

#include <lutok/state.ipp>

#include "utils/config/exceptions.hpp"
#include "utils/config/keys.hpp"
#include "utils/defs.hpp"

namespace config = utils::config;


namespace {


/// Typed leaf node that specializes the validate() method.
class validation_node : public config::int_node {
    /// Checks a given value for validity against a fake value.
    ///
    /// \param new_value The value to validate.
    ///
    /// \throw value_error If the value is not valid.
    void
    validate(const value_type& new_value) const
    {
        if (new_value == 12345)
            throw config::value_error("Custom validate method");
    }
};


/// Set node that specializes the validate() method.
class set_validation_node : public config::strings_set_node {
    /// Checks a given value for validity against a fake value.
    ///
    /// \param new_value The value to validate.
    ///
    /// \throw value_error If the value is not valid.
    void
    validate(const value_type& new_value) const
    {
        for (value_type::const_iterator iter = new_value.begin();
             iter != new_value.end(); ++iter)
            if (*iter == "throw")
                throw config::value_error("Custom validate method");
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__deep_copy);
ATF_TEST_CASE_BODY(bool_node__deep_copy)
{
    config::bool_node node;
    node.set(true);
    config::detail::base_node* raw_copy = node.deep_copy();
    config::bool_node* copy = static_cast< config::bool_node* >(raw_copy);
    ATF_REQUIRE(copy->value());
    copy->set(false);
    ATF_REQUIRE(node.value());
    ATF_REQUIRE(!copy->value());
    delete copy;
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__is_set_and_set);
ATF_TEST_CASE_BODY(bool_node__is_set_and_set)
{
    config::bool_node node;
    ATF_REQUIRE(!node.is_set());
    node.set(false);
    ATF_REQUIRE( node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__value_and_set);
ATF_TEST_CASE_BODY(bool_node__value_and_set)
{
    config::bool_node node;
    node.set(false);
    ATF_REQUIRE(!node.value());
    node.set(true);
    ATF_REQUIRE( node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__push_lua);
ATF_TEST_CASE_BODY(bool_node__push_lua)
{
    lutok::state state;

    config::bool_node node;
    node.set(true);
    node.push_lua(state);
    ATF_REQUIRE(state.is_boolean(-1));
    ATF_REQUIRE(state.to_boolean(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__set_lua__ok);
ATF_TEST_CASE_BODY(bool_node__set_lua__ok)
{
    lutok::state state;

    config::bool_node node;
    state.push_boolean(false);
    node.set_lua(state, -1);
    state.pop(1);
    ATF_REQUIRE(!node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__set_lua__invalid_value);
ATF_TEST_CASE_BODY(bool_node__set_lua__invalid_value)
{
    lutok::state state;

    config::bool_node node;
    state.push_string("foo bar");
    ATF_REQUIRE_THROW(config::value_error, node.set_lua(state, -1));
    state.pop(1);
    ATF_REQUIRE(!node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__set_string__ok);
ATF_TEST_CASE_BODY(bool_node__set_string__ok)
{
    config::bool_node node;
    node.set_string("false");
    ATF_REQUIRE(!node.value());
    node.set_string("true");
    ATF_REQUIRE( node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__set_string__invalid_value);
ATF_TEST_CASE_BODY(bool_node__set_string__invalid_value)
{
    config::bool_node node;
    ATF_REQUIRE_THROW(config::value_error, node.set_string("12345"));
    ATF_REQUIRE(!node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(bool_node__to_string);
ATF_TEST_CASE_BODY(bool_node__to_string)
{
    config::bool_node node;
    node.set(false);
    ATF_REQUIRE_EQ("false", node.to_string());
    node.set(true);
    ATF_REQUIRE_EQ("true", node.to_string());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__deep_copy);
ATF_TEST_CASE_BODY(int_node__deep_copy)
{
    config::int_node node;
    node.set(5);
    config::detail::base_node* raw_copy = node.deep_copy();
    config::int_node* copy = static_cast< config::int_node* >(raw_copy);
    ATF_REQUIRE_EQ(5, copy->value());
    copy->set(10);
    ATF_REQUIRE_EQ(5, node.value());
    ATF_REQUIRE_EQ(10, copy->value());
    delete copy;
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__is_set_and_set);
ATF_TEST_CASE_BODY(int_node__is_set_and_set)
{
    config::int_node node;
    ATF_REQUIRE(!node.is_set());
    node.set(20);
    ATF_REQUIRE( node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__value_and_set);
ATF_TEST_CASE_BODY(int_node__value_and_set)
{
    config::int_node node;
    node.set(20);
    ATF_REQUIRE_EQ(20, node.value());
    node.set(0);
    ATF_REQUIRE_EQ(0, node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__push_lua);
ATF_TEST_CASE_BODY(int_node__push_lua)
{
    lutok::state state;

    config::int_node node;
    node.set(754);
    node.push_lua(state);
    ATF_REQUIRE(state.is_number(-1));
    ATF_REQUIRE_EQ(754, state.to_integer(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__set_lua__ok);
ATF_TEST_CASE_BODY(int_node__set_lua__ok)
{
    lutok::state state;

    config::int_node node;
    state.push_integer(123);
    state.push_string("456");
    node.set_lua(state, -2);
    ATF_REQUIRE_EQ(123, node.value());
    node.set_lua(state, -1);
    ATF_REQUIRE_EQ(456, node.value());
    state.pop(2);
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__set_lua__invalid_value);
ATF_TEST_CASE_BODY(int_node__set_lua__invalid_value)
{
    lutok::state state;

    config::int_node node;
    state.push_boolean(true);
    ATF_REQUIRE_THROW(config::value_error, node.set_lua(state, -1));
    state.pop(1);
    ATF_REQUIRE(!node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__set_string__ok);
ATF_TEST_CASE_BODY(int_node__set_string__ok)
{
    config::int_node node;
    node.set_string("178");
    ATF_REQUIRE_EQ(178, node.value());
    node.set_string("-123");
    ATF_REQUIRE_EQ(-123, node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__set_string__invalid_value);
ATF_TEST_CASE_BODY(int_node__set_string__invalid_value)
{
    config::int_node node;
    ATF_REQUIRE_THROW(config::value_error, node.set_string(" 23"));
    ATF_REQUIRE(!node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(int_node__to_string);
ATF_TEST_CASE_BODY(int_node__to_string)
{
    config::int_node node;
    node.set(89);
    ATF_REQUIRE_EQ("89", node.to_string());
    node.set(-57);
    ATF_REQUIRE_EQ("-57", node.to_string());
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__deep_copy);
ATF_TEST_CASE_BODY(positive_int_node__deep_copy)
{
    config::positive_int_node node;
    node.set(5);
    config::detail::base_node* raw_copy = node.deep_copy();
    config::positive_int_node* copy = static_cast< config::positive_int_node* >(
        raw_copy);
    ATF_REQUIRE_EQ(5, copy->value());
    copy->set(10);
    ATF_REQUIRE_EQ(5, node.value());
    ATF_REQUIRE_EQ(10, copy->value());
    delete copy;
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__is_set_and_set);
ATF_TEST_CASE_BODY(positive_int_node__is_set_and_set)
{
    config::positive_int_node node;
    ATF_REQUIRE(!node.is_set());
    node.set(20);
    ATF_REQUIRE( node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__value_and_set);
ATF_TEST_CASE_BODY(positive_int_node__value_and_set)
{
    config::positive_int_node node;
    node.set(20);
    ATF_REQUIRE_EQ(20, node.value());
    node.set(1);
    ATF_REQUIRE_EQ(1, node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__push_lua);
ATF_TEST_CASE_BODY(positive_int_node__push_lua)
{
    lutok::state state;

    config::positive_int_node node;
    node.set(754);
    node.push_lua(state);
    ATF_REQUIRE(state.is_number(-1));
    ATF_REQUIRE_EQ(754, state.to_integer(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__set_lua__ok);
ATF_TEST_CASE_BODY(positive_int_node__set_lua__ok)
{
    lutok::state state;

    config::positive_int_node node;
    state.push_integer(123);
    state.push_string("456");
    node.set_lua(state, -2);
    ATF_REQUIRE_EQ(123, node.value());
    node.set_lua(state, -1);
    ATF_REQUIRE_EQ(456, node.value());
    state.pop(2);
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__set_lua__invalid_value);
ATF_TEST_CASE_BODY(positive_int_node__set_lua__invalid_value)
{
    lutok::state state;

    config::positive_int_node node;
    state.push_boolean(true);
    ATF_REQUIRE_THROW(config::value_error, node.set_lua(state, -1));
    state.pop(1);
    ATF_REQUIRE(!node.is_set());
    state.push_integer(0);
    ATF_REQUIRE_THROW(config::value_error, node.set_lua(state, -1));
    state.pop(1);
    ATF_REQUIRE(!node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__set_string__ok);
ATF_TEST_CASE_BODY(positive_int_node__set_string__ok)
{
    config::positive_int_node node;
    node.set_string("1");
    ATF_REQUIRE_EQ(1, node.value());
    node.set_string("178");
    ATF_REQUIRE_EQ(178, node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__set_string__invalid_value);
ATF_TEST_CASE_BODY(positive_int_node__set_string__invalid_value)
{
    config::positive_int_node node;
    ATF_REQUIRE_THROW(config::value_error, node.set_string(" 23"));
    ATF_REQUIRE(!node.is_set());
    ATF_REQUIRE_THROW(config::value_error, node.set_string("0"));
    ATF_REQUIRE(!node.is_set());
    ATF_REQUIRE_THROW(config::value_error, node.set_string("-5"));
    ATF_REQUIRE(!node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(positive_int_node__to_string);
ATF_TEST_CASE_BODY(positive_int_node__to_string)
{
    config::positive_int_node node;
    node.set(89);
    ATF_REQUIRE_EQ("89", node.to_string());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_node__deep_copy);
ATF_TEST_CASE_BODY(string_node__deep_copy)
{
    config::string_node node;
    node.set("first");
    config::detail::base_node* raw_copy = node.deep_copy();
    config::string_node* copy = static_cast< config::string_node* >(raw_copy);
    ATF_REQUIRE_EQ("first", copy->value());
    copy->set("second");
    ATF_REQUIRE_EQ("first", node.value());
    ATF_REQUIRE_EQ("second", copy->value());
    delete copy;
}


ATF_TEST_CASE_WITHOUT_HEAD(string_node__is_set_and_set);
ATF_TEST_CASE_BODY(string_node__is_set_and_set)
{
    config::string_node node;
    ATF_REQUIRE(!node.is_set());
    node.set("foo");
    ATF_REQUIRE( node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_node__value_and_set);
ATF_TEST_CASE_BODY(string_node__value_and_set)
{
    config::string_node node;
    node.set("foo");
    ATF_REQUIRE_EQ("foo", node.value());
    node.set("");
    ATF_REQUIRE_EQ("", node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_node__push_lua);
ATF_TEST_CASE_BODY(string_node__push_lua)
{
    lutok::state state;

    config::string_node node;
    node.set("some message");
    node.push_lua(state);
    ATF_REQUIRE(state.is_string(-1));
    ATF_REQUIRE_EQ("some message", state.to_string(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(string_node__set_lua__ok);
ATF_TEST_CASE_BODY(string_node__set_lua__ok)
{
    lutok::state state;

    config::string_node node;
    state.push_string("text 1");
    state.push_integer(231);
    node.set_lua(state, -2);
    ATF_REQUIRE_EQ("text 1", node.value());
    node.set_lua(state, -1);
    ATF_REQUIRE_EQ("231", node.value());
    state.pop(2);
}


ATF_TEST_CASE_WITHOUT_HEAD(string_node__set_lua__invalid_value);
ATF_TEST_CASE_BODY(string_node__set_lua__invalid_value)
{
    lutok::state state;

    config::bool_node node;
    state.new_table();
    ATF_REQUIRE_THROW(config::value_error, node.set_lua(state, -1));
    state.pop(1);
    ATF_REQUIRE(!node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_node__set_string);
ATF_TEST_CASE_BODY(string_node__set_string)
{
    config::string_node node;
    node.set_string("abcd efgh");
    ATF_REQUIRE_EQ("abcd efgh", node.value());
    node.set_string("  1234  ");
    ATF_REQUIRE_EQ("  1234  ", node.value());
}


ATF_TEST_CASE_WITHOUT_HEAD(string_node__to_string);
ATF_TEST_CASE_BODY(string_node__to_string)
{
    config::string_node node;
    node.set("");
    ATF_REQUIRE_EQ("", node.to_string());
    node.set("aaa");
    ATF_REQUIRE_EQ("aaa", node.to_string());
}


ATF_TEST_CASE_WITHOUT_HEAD(strings_set_node__deep_copy);
ATF_TEST_CASE_BODY(strings_set_node__deep_copy)
{
    std::set< std::string > value;
    config::strings_set_node node;
    value.insert("foo");
    node.set(value);
    config::detail::base_node* raw_copy = node.deep_copy();
    config::strings_set_node* copy =
        static_cast< config::strings_set_node* >(raw_copy);
    value.insert("bar");
    ATF_REQUIRE_EQ(1, copy->value().size());
    copy->set(value);
    ATF_REQUIRE_EQ(1, node.value().size());
    ATF_REQUIRE_EQ(2, copy->value().size());
    delete copy;
}


ATF_TEST_CASE_WITHOUT_HEAD(strings_set_node__is_set_and_set);
ATF_TEST_CASE_BODY(strings_set_node__is_set_and_set)
{
    std::set< std::string > value;
    value.insert("foo");

    config::strings_set_node node;
    ATF_REQUIRE(!node.is_set());
    node.set(value);
    ATF_REQUIRE( node.is_set());
}


ATF_TEST_CASE_WITHOUT_HEAD(strings_set_node__value_and_set);
ATF_TEST_CASE_BODY(strings_set_node__value_and_set)
{
    std::set< std::string > value;
    value.insert("first");

    config::strings_set_node node;
    node.set(value);
    ATF_REQUIRE(value == node.value());
    value.clear();
    node.set(value);
    value.insert("second");
    ATF_REQUIRE(node.value().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(strings_set_node__set_string);
ATF_TEST_CASE_BODY(strings_set_node__set_string)
{
    config::strings_set_node node;
    {
        std::set< std::string > expected;
        expected.insert("abcd");
        expected.insert("efgh");

        node.set_string("abcd efgh");
        ATF_REQUIRE(expected == node.value());
    }
    {
        std::set< std::string > expected;
        expected.insert("1234");

        node.set_string("  1234  ");
        ATF_REQUIRE(expected == node.value());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(strings_set_node__to_string);
ATF_TEST_CASE_BODY(strings_set_node__to_string)
{
    std::set< std::string > value;
    config::strings_set_node node;
    value.insert("second");
    value.insert("first");
    node.set(value);
    ATF_REQUIRE_EQ("first second", node.to_string());
}


ATF_TEST_CASE_WITHOUT_HEAD(typed_leaf_node__validate_set);
ATF_TEST_CASE_BODY(typed_leaf_node__validate_set)
{
    validation_node node;
    node.set(1234);
    ATF_REQUIRE_THROW_RE(config::value_error, "Custom validate method",
                         node.set(12345));
}


ATF_TEST_CASE_WITHOUT_HEAD(typed_leaf_node__validate_set_string);
ATF_TEST_CASE_BODY(typed_leaf_node__validate_set_string)
{
    validation_node node;
    node.set_string("1234");
    ATF_REQUIRE_THROW_RE(config::value_error, "Custom validate method",
                         node.set_string("12345"));
}


ATF_TEST_CASE_WITHOUT_HEAD(base_set_node__validate_set);
ATF_TEST_CASE_BODY(base_set_node__validate_set)
{
    set_validation_node node;
    set_validation_node::value_type values;
    values.insert("foo");
    values.insert("bar");
    node.set(values);
    values.insert("throw");
    values.insert("baz");
    ATF_REQUIRE_THROW_RE(config::value_error, "Custom validate method",
                         node.set(values));
}


ATF_TEST_CASE_WITHOUT_HEAD(base_set_node__validate_set_string);
ATF_TEST_CASE_BODY(base_set_node__validate_set_string)
{
    set_validation_node node;
    node.set_string("foo bar");
    ATF_REQUIRE_THROW_RE(config::value_error, "Custom validate method",
                         node.set_string("foo bar throw baz"));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, bool_node__deep_copy);
    ATF_ADD_TEST_CASE(tcs, bool_node__is_set_and_set);
    ATF_ADD_TEST_CASE(tcs, bool_node__value_and_set);
    ATF_ADD_TEST_CASE(tcs, bool_node__push_lua);
    ATF_ADD_TEST_CASE(tcs, bool_node__set_lua__ok);
    ATF_ADD_TEST_CASE(tcs, bool_node__set_lua__invalid_value);
    ATF_ADD_TEST_CASE(tcs, bool_node__set_string__ok);
    ATF_ADD_TEST_CASE(tcs, bool_node__set_string__invalid_value);
    ATF_ADD_TEST_CASE(tcs, bool_node__to_string);

    ATF_ADD_TEST_CASE(tcs, int_node__deep_copy);
    ATF_ADD_TEST_CASE(tcs, int_node__is_set_and_set);
    ATF_ADD_TEST_CASE(tcs, int_node__value_and_set);
    ATF_ADD_TEST_CASE(tcs, int_node__push_lua);
    ATF_ADD_TEST_CASE(tcs, int_node__set_lua__ok);
    ATF_ADD_TEST_CASE(tcs, int_node__set_lua__invalid_value);
    ATF_ADD_TEST_CASE(tcs, int_node__set_string__ok);
    ATF_ADD_TEST_CASE(tcs, int_node__set_string__invalid_value);
    ATF_ADD_TEST_CASE(tcs, int_node__to_string);

    ATF_ADD_TEST_CASE(tcs, positive_int_node__deep_copy);
    ATF_ADD_TEST_CASE(tcs, positive_int_node__is_set_and_set);
    ATF_ADD_TEST_CASE(tcs, positive_int_node__value_and_set);
    ATF_ADD_TEST_CASE(tcs, positive_int_node__push_lua);
    ATF_ADD_TEST_CASE(tcs, positive_int_node__set_lua__ok);
    ATF_ADD_TEST_CASE(tcs, positive_int_node__set_lua__invalid_value);
    ATF_ADD_TEST_CASE(tcs, positive_int_node__set_string__ok);
    ATF_ADD_TEST_CASE(tcs, positive_int_node__set_string__invalid_value);
    ATF_ADD_TEST_CASE(tcs, positive_int_node__to_string);

    ATF_ADD_TEST_CASE(tcs, string_node__deep_copy);
    ATF_ADD_TEST_CASE(tcs, string_node__is_set_and_set);
    ATF_ADD_TEST_CASE(tcs, string_node__value_and_set);
    ATF_ADD_TEST_CASE(tcs, string_node__push_lua);
    ATF_ADD_TEST_CASE(tcs, string_node__set_lua__ok);
    ATF_ADD_TEST_CASE(tcs, string_node__set_lua__invalid_value);
    ATF_ADD_TEST_CASE(tcs, string_node__set_string);
    ATF_ADD_TEST_CASE(tcs, string_node__to_string);

    ATF_ADD_TEST_CASE(tcs, strings_set_node__deep_copy);
    ATF_ADD_TEST_CASE(tcs, strings_set_node__is_set_and_set);
    ATF_ADD_TEST_CASE(tcs, strings_set_node__value_and_set);
    ATF_ADD_TEST_CASE(tcs, strings_set_node__set_string);
    ATF_ADD_TEST_CASE(tcs, strings_set_node__to_string);

    ATF_ADD_TEST_CASE(tcs, typed_leaf_node__validate_set);
    ATF_ADD_TEST_CASE(tcs, typed_leaf_node__validate_set_string);
    ATF_ADD_TEST_CASE(tcs, base_set_node__validate_set);
    ATF_ADD_TEST_CASE(tcs, base_set_node__validate_set_string);
}
