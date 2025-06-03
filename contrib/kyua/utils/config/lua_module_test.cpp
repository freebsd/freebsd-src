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

#include "utils/config/lua_module.hpp"

#include <atf-c++.hpp>

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.ipp>

#include "utils/config/tree.ipp"
#include "utils/defs.hpp"

namespace config = utils::config;


namespace {


/// Non-native type to use as a leaf node.
struct custom_type {
    /// The value recorded in the object.
    int value;

    /// Constructs a new object.
    ///
    /// \param value_ The value to store in the object.
    explicit custom_type(const int value_) :
        value(value_)
    {
    }
};


/// Custom implementation of a node type for testing purposes.
class custom_node : public config::typed_leaf_node< custom_type > {
public:
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::unique_ptr< custom_node > new_node(new custom_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Pushes the node's value onto the Lua stack.
    ///
    /// \param state The Lua state onto which to push the value.
    void
    push_lua(lutok::state& state) const
    {
        state.push_integer(value().value * 5);
    }

    /// Sets the value of the node from an entry in the Lua stack.
    ///
    /// \param state The Lua state from which to get the value.
    /// \param value_index The stack index in which the value resides.
    void
    set_lua(lutok::state& state, const int value_index)
    {
        ATF_REQUIRE(state.is_number(value_index));
        set(custom_type(state.to_integer(value_index) * 2));
    }

    /// Sets the value of the node from a raw string representation.
    ///
    /// \post The test case is marked as failed, as this function is not
    /// supposed to be invoked by the lua_module code.
    void
    set_string(const std::string& /* raw_value */)
    {
        ATF_FAIL("Should not be used");
    }

    /// Converts the contents of the node to a string.
    ///
    /// \post The test case is marked as failed, as this function is not
    /// supposed to be invoked by the lua_module code.
    ///
    /// \return Nothing.
    std::string
    to_string(void) const
    {
        ATF_FAIL("Should not be used");
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(top__valid_types);
ATF_TEST_CASE_BODY(top__valid_types)
{
    config::tree tree;
    tree.define< config::bool_node >("top_boolean");
    tree.define< config::int_node >("top_integer");
    tree.define< config::string_node >("top_string");

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state,
                         "top_boolean = true\n"
                         "top_integer = 12345\n"
                         "top_string = 'a foo'\n",
                         0, 0, 0);
    }

    ATF_REQUIRE_EQ(true, tree.lookup< config::bool_node >("top_boolean"));
    ATF_REQUIRE_EQ(12345, tree.lookup< config::int_node >("top_integer"));
    ATF_REQUIRE_EQ("a foo", tree.lookup< config::string_node >("top_string"));
}


ATF_TEST_CASE_WITHOUT_HEAD(top__invalid_types);
ATF_TEST_CASE_BODY(top__invalid_types)
{
    config::tree tree;
    tree.define< config::bool_node >("top_boolean");
    tree.define< config::int_node >("top_integer");

    {
        lutok::state state;
        config::redirect(state, tree);
        ATF_REQUIRE_THROW_RE(
            lutok::error,
            "Invalid value for property 'top_boolean': Not a boolean",
            lutok::do_string(state,
                             "top_boolean = true\n"
                             "top_integer = 8\n"
                             "top_boolean = 'foo'\n",
                             0, 0, 0));
    }

    ATF_REQUIRE_EQ(true, tree.lookup< config::bool_node >("top_boolean"));
    ATF_REQUIRE_EQ(8, tree.lookup< config::int_node >("top_integer"));
}


ATF_TEST_CASE_WITHOUT_HEAD(top__reuse);
ATF_TEST_CASE_BODY(top__reuse)
{
    config::tree tree;
    tree.define< config::int_node >("first");
    tree.define< config::int_node >("second");

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state, "first = 100; second = first * 2", 0, 0, 0);
    }

    ATF_REQUIRE_EQ(100, tree.lookup< config::int_node >("first"));
    ATF_REQUIRE_EQ(200, tree.lookup< config::int_node >("second"));
}


ATF_TEST_CASE_WITHOUT_HEAD(top__reset);
ATF_TEST_CASE_BODY(top__reset)
{
    config::tree tree;
    tree.define< config::int_node >("first");

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state, "first = 100; first = 200", 0, 0, 0);
    }

    ATF_REQUIRE_EQ(200, tree.lookup< config::int_node >("first"));
}


ATF_TEST_CASE_WITHOUT_HEAD(top__already_set_on_entry);
ATF_TEST_CASE_BODY(top__already_set_on_entry)
{
    config::tree tree;
    tree.define< config::int_node >("first");
    tree.set< config::int_node >("first", 100);

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state, "first = first * 15", 0, 0, 0);
    }

    ATF_REQUIRE_EQ(1500, tree.lookup< config::int_node >("first"));
}


ATF_TEST_CASE_WITHOUT_HEAD(subtree__valid_types);
ATF_TEST_CASE_BODY(subtree__valid_types)
{
    config::tree tree;
    tree.define< config::bool_node >("root.boolean");
    tree.define< config::int_node >("root.a.integer");
    tree.define< config::string_node >("root.string");

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state,
                         "root.boolean = true\n"
                         "root.a.integer = 12345\n"
                         "root.string = 'a foo'\n",
                         0, 0, 0);
    }

    ATF_REQUIRE_EQ(true, tree.lookup< config::bool_node >("root.boolean"));
    ATF_REQUIRE_EQ(12345, tree.lookup< config::int_node >("root.a.integer"));
    ATF_REQUIRE_EQ("a foo", tree.lookup< config::string_node >("root.string"));
}


ATF_TEST_CASE_WITHOUT_HEAD(subtree__reuse);
ATF_TEST_CASE_BODY(subtree__reuse)
{
    config::tree tree;
    tree.define< config::int_node >("a.first");
    tree.define< config::int_node >("a.second");

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state, "a.first = 100; a.second = a.first * 2",
                         0, 0, 0);
    }

    ATF_REQUIRE_EQ(100, tree.lookup< config::int_node >("a.first"));
    ATF_REQUIRE_EQ(200, tree.lookup< config::int_node >("a.second"));
}


ATF_TEST_CASE_WITHOUT_HEAD(subtree__reset);
ATF_TEST_CASE_BODY(subtree__reset)
{
    config::tree tree;
    tree.define< config::int_node >("a.first");

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state, "a.first = 100; a.first = 200", 0, 0, 0);
    }

    ATF_REQUIRE_EQ(200, tree.lookup< config::int_node >("a.first"));
}


ATF_TEST_CASE_WITHOUT_HEAD(subtree__already_set_on_entry);
ATF_TEST_CASE_BODY(subtree__already_set_on_entry)
{
    config::tree tree;
    tree.define< config::int_node >("a.first");
    tree.set< config::int_node >("a.first", 100);

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state, "a.first = a.first * 15", 0, 0, 0);
    }

    ATF_REQUIRE_EQ(1500, tree.lookup< config::int_node >("a.first"));
}


ATF_TEST_CASE_WITHOUT_HEAD(subtree__override_inner);
ATF_TEST_CASE_BODY(subtree__override_inner)
{
    config::tree tree;
    tree.define_dynamic("root");

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state, "root.test = 'a'", 0, 0, 0);
        ATF_REQUIRE_THROW_RE(lutok::error, "Invalid value for property 'root'",
                             lutok::do_string(state, "root = 'b'", 0, 0, 0));
        // Ensure that the previous assignment to 'root' did not cause any
        // inconsistencies in the environment that would prevent a new
        // assignment from working.
        lutok::do_string(state, "root.test2 = 'c'", 0, 0, 0);
    }

    ATF_REQUIRE_EQ("a", tree.lookup< config::string_node >("root.test"));
    ATF_REQUIRE_EQ("c", tree.lookup< config::string_node >("root.test2"));
}


ATF_TEST_CASE_WITHOUT_HEAD(dynamic_subtree__strings);
ATF_TEST_CASE_BODY(dynamic_subtree__strings)
{
    config::tree tree;
    tree.define_dynamic("root");

    lutok::state state;
    config::redirect(state, tree);
    lutok::do_string(state,
                     "root.key1 = 1234\n"
                     "root.a.b.key2 = 'foo bar'\n",
                     0, 0, 0);

    ATF_REQUIRE_EQ("1234", tree.lookup< config::string_node >("root.key1"));
    ATF_REQUIRE_EQ("foo bar",
                   tree.lookup< config::string_node >("root.a.b.key2"));
}


ATF_TEST_CASE_WITHOUT_HEAD(dynamic_subtree__invalid_types);
ATF_TEST_CASE_BODY(dynamic_subtree__invalid_types)
{
    config::tree tree;
    tree.define_dynamic("root");

    lutok::state state;
    config::redirect(state, tree);
    ATF_REQUIRE_THROW_RE(lutok::error,
                         "Invalid value for property 'root.boolean': "
                         "Not a string",
                         lutok::do_string(state, "root.boolean = true",
                                          0, 0, 0));
    ATF_REQUIRE_THROW_RE(lutok::error,
                         "Invalid value for property 'root.table': "
                         "Not a string",
                         lutok::do_string(state, "root.table = {}",
                                          0, 0, 0));
    ATF_REQUIRE(!tree.is_set("root.boolean"));
    ATF_REQUIRE(!tree.is_set("root.table"));
}


ATF_TEST_CASE_WITHOUT_HEAD(locals);
ATF_TEST_CASE_BODY(locals)
{
    config::tree tree;
    tree.define< config::int_node >("the_key");

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state,
                         "local function generate()\n"
                         "    return 15\n"
                         "end\n"
                         "local test_var = 20\n"
                         "the_key = generate() + test_var\n",
                         0, 0, 0);
    }

    ATF_REQUIRE_EQ(35, tree.lookup< config::int_node >("the_key"));
}


ATF_TEST_CASE_WITHOUT_HEAD(custom_node);
ATF_TEST_CASE_BODY(custom_node)
{
    config::tree tree;
    tree.define< custom_node >("key1");
    tree.define< custom_node >("key2");
    tree.set< custom_node >("key2", custom_type(10));

    {
        lutok::state state;
        config::redirect(state, tree);
        lutok::do_string(state, "key1 = 512\n", 0, 0, 0);
        lutok::do_string(state, "key2 = key2 * 2\n", 0, 0, 0);
    }

    ATF_REQUIRE_EQ(1024, tree.lookup< custom_node >("key1").value);
    ATF_REQUIRE_EQ(200, tree.lookup< custom_node >("key2").value);
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_key);
ATF_TEST_CASE_BODY(invalid_key)
{
    config::tree tree;

    lutok::state state;
    config::redirect(state, tree);
    ATF_REQUIRE_THROW_RE(lutok::error, "Empty component in key 'root.'",
                         lutok::do_string(state, "root['']['a'] = 12345\n",
                                          0, 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(unknown_key);
ATF_TEST_CASE_BODY(unknown_key)
{
    config::tree tree;
    tree.define< config::bool_node >("static.bool");

    lutok::state state;
    config::redirect(state, tree);
    ATF_REQUIRE_THROW_RE(lutok::error,
                         "Unknown configuration property 'static.int'",
                         lutok::do_string(state,
                                          "static.int = 12345\n",
                                          0, 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(value_error);
ATF_TEST_CASE_BODY(value_error)
{
    config::tree tree;
    tree.define< config::bool_node >("a.b");

    lutok::state state;
    config::redirect(state, tree);
    ATF_REQUIRE_THROW_RE(lutok::error,
                         "Invalid value for property 'a.b': Not a boolean",
                         lutok::do_string(state, "a.b = 12345\n", 0, 0, 0));
    ATF_REQUIRE_THROW_RE(lutok::error,
                         "Invalid value for property 'a': ",
                         lutok::do_string(state, "a = 1\n", 0, 0, 0));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, top__valid_types);
    ATF_ADD_TEST_CASE(tcs, top__invalid_types);
    ATF_ADD_TEST_CASE(tcs, top__reuse);
    ATF_ADD_TEST_CASE(tcs, top__reset);
    ATF_ADD_TEST_CASE(tcs, top__already_set_on_entry);

    ATF_ADD_TEST_CASE(tcs, subtree__valid_types);
    ATF_ADD_TEST_CASE(tcs, subtree__reuse);
    ATF_ADD_TEST_CASE(tcs, subtree__reset);
    ATF_ADD_TEST_CASE(tcs, subtree__already_set_on_entry);
    ATF_ADD_TEST_CASE(tcs, subtree__override_inner);

    ATF_ADD_TEST_CASE(tcs, dynamic_subtree__strings);
    ATF_ADD_TEST_CASE(tcs, dynamic_subtree__invalid_types);

    ATF_ADD_TEST_CASE(tcs, locals);
    ATF_ADD_TEST_CASE(tcs, custom_node);

    ATF_ADD_TEST_CASE(tcs, invalid_key);
    ATF_ADD_TEST_CASE(tcs, unknown_key);
    ATF_ADD_TEST_CASE(tcs, value_error);
}
