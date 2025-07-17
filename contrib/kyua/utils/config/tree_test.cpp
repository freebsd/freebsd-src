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

#include "utils/config/tree.ipp"

#include <atf-c++.hpp>

#include "utils/config/nodes.ipp"
#include "utils/format/macros.hpp"
#include "utils/text/operations.ipp"

namespace config = utils::config;
namespace text = utils::text;


namespace {


/// Simple wrapper around an integer value without default constructors.
///
/// The purpose of this type is to have a simple class without default
/// constructors to validate that we can use it as a leaf of a tree.
class int_wrapper {
    /// The wrapped integer value.
    int _value;

public:
    /// Constructs a new wrapped integer.
    ///
    /// \param value_ The value to store in the object.
    explicit int_wrapper(int value_) :
        _value(value_)
    {
    }

    /// \return The integer value stored by the object.
    int
    value(void) const
    {
        return _value;
    }
};


/// Custom tree leaf type for an object without defualt constructors.
class wrapped_int_node : public config::typed_leaf_node< int_wrapper > {
public:
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::unique_ptr< wrapped_int_node > new_node(new wrapped_int_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Pushes the node's value onto the Lua stack.
    ///
    /// \param state The Lua state onto which to push the value.
    void
    push_lua(lutok::state& state) const
    {
        state.push_integer(
            config::typed_leaf_node< int_wrapper >::value().value());
    }

    /// Sets the value of the node from an entry in the Lua stack.
    ///
    /// \param state The Lua state from which to get the value.
    /// \param value_index The stack index in which the value resides.
    void
    set_lua(lutok::state& state, const int value_index)
    {
        ATF_REQUIRE(state.is_number(value_index));
        int_wrapper new_value(state.to_integer(value_index));
        config::typed_leaf_node< int_wrapper >::set(new_value);
    }

    /// Sets the value of the node from a raw string representation.
    ///
    /// \param raw_value The value to set the node to.
    void
    set_string(const std::string& raw_value)
    {
        int_wrapper new_value(text::to_type< int >(raw_value));
        config::typed_leaf_node< int_wrapper >::set(new_value);
    }

    /// Converts the contents of the node to a string.
    ///
    /// \return A string representation of the value held by the node.
    std::string
    to_string(void) const
    {
        return F("%s") %
            config::typed_leaf_node< int_wrapper >::value().value();
    }
};


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(define_set_lookup__one_level);
ATF_TEST_CASE_BODY(define_set_lookup__one_level)
{
    config::tree tree;

    tree.define< config::int_node >("var1");
    tree.define< config::string_node >("var2");
    tree.define< config::bool_node >("var3");

    tree.set< config::int_node >("var1", 42);
    tree.set< config::string_node >("var2", "hello");
    tree.set< config::bool_node >("var3", false);

    ATF_REQUIRE_EQ(42, tree.lookup< config::int_node >("var1"));
    ATF_REQUIRE_EQ("hello", tree.lookup< config::string_node >("var2"));
    ATF_REQUIRE(!tree.lookup< config::bool_node >("var3"));
}


ATF_TEST_CASE_WITHOUT_HEAD(define_set_lookup__multiple_levels);
ATF_TEST_CASE_BODY(define_set_lookup__multiple_levels)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar.1");
    tree.define< config::string_node >("foo.bar.2");
    tree.define< config::bool_node >("foo.3");
    tree.define_dynamic("sub.tree");

    tree.set< config::int_node >("foo.bar.1", 42);
    tree.set< config::string_node >("foo.bar.2", "hello");
    tree.set< config::bool_node >("foo.3", true);
    tree.set< config::string_node >("sub.tree.1", "bye");
    tree.set< config::int_node >("sub.tree.2", 4);
    tree.set< config::int_node >("sub.tree.3.4", 123);

    ATF_REQUIRE_EQ(42, tree.lookup< config::int_node >("foo.bar.1"));
    ATF_REQUIRE_EQ("hello", tree.lookup< config::string_node >("foo.bar.2"));
    ATF_REQUIRE(tree.lookup< config::bool_node >("foo.3"));
    ATF_REQUIRE_EQ(4, tree.lookup< config::int_node >("sub.tree.2"));
    ATF_REQUIRE_EQ(123, tree.lookup< config::int_node >("sub.tree.3.4"));
}


ATF_TEST_CASE_WITHOUT_HEAD(deep_copy__empty);
ATF_TEST_CASE_BODY(deep_copy__empty)
{
    config::tree tree1;
    config::tree tree2 = tree1.deep_copy();

    tree1.define< config::bool_node >("var1");
    // This would crash if the copy shared the internal data.
    tree2.define< config::int_node >("var1");
}


ATF_TEST_CASE_WITHOUT_HEAD(deep_copy__some);
ATF_TEST_CASE_BODY(deep_copy__some)
{
    config::tree tree1;
    tree1.define< config::bool_node >("this.is.a.var");
    tree1.set< config::bool_node >("this.is.a.var", true);
    tree1.define< config::int_node >("this.is.another.var");
    tree1.set< config::int_node >("this.is.another.var", 34);
    tree1.define< config::int_node >("and.another");
    tree1.set< config::int_node >("and.another", 123);

    config::tree tree2 = tree1.deep_copy();
    tree2.set< config::bool_node >("this.is.a.var", false);
    tree2.set< config::int_node >("this.is.another.var", 43);

    ATF_REQUIRE( tree1.lookup< config::bool_node >("this.is.a.var"));
    ATF_REQUIRE(!tree2.lookup< config::bool_node >("this.is.a.var"));

    ATF_REQUIRE_EQ(34, tree1.lookup< config::int_node >("this.is.another.var"));
    ATF_REQUIRE_EQ(43, tree2.lookup< config::int_node >("this.is.another.var"));

    ATF_REQUIRE_EQ(123, tree1.lookup< config::int_node >("and.another"));
    ATF_REQUIRE_EQ(123, tree2.lookup< config::int_node >("and.another"));
}


ATF_TEST_CASE_WITHOUT_HEAD(combine__empty);
ATF_TEST_CASE_BODY(combine__empty)
{
    const config::tree t1, t2;
    const config::tree combined = t1.combine(t2);

    const config::tree expected;
    ATF_REQUIRE(expected == combined);
}


static void
init_tree_for_combine_test(config::tree& tree)
{
    tree.define< config::int_node >("int-node");
    tree.define< config::string_node >("string-node");
    tree.define< config::int_node >("unused.node");
    tree.define< config::int_node >("deeper.int.node");
    tree.define_dynamic("deeper.dynamic");
}


ATF_TEST_CASE_WITHOUT_HEAD(combine__same_layout__no_overrides);
ATF_TEST_CASE_BODY(combine__same_layout__no_overrides)
{
    config::tree t1, t2;
    init_tree_for_combine_test(t1);
    init_tree_for_combine_test(t2);
    t1.set< config::int_node >("int-node", 3);
    t1.set< config::string_node >("string-node", "foo");
    t1.set< config::int_node >("deeper.int.node", 15);
    t1.set_string("deeper.dynamic.first", "value1");
    t1.set_string("deeper.dynamic.second", "value2");
    const config::tree combined = t1.combine(t2);

    ATF_REQUIRE(t1 == combined);
}


ATF_TEST_CASE_WITHOUT_HEAD(combine__same_layout__no_base);
ATF_TEST_CASE_BODY(combine__same_layout__no_base)
{
    config::tree t1, t2;
    init_tree_for_combine_test(t1);
    init_tree_for_combine_test(t2);
    t2.set< config::int_node >("int-node", 3);
    t2.set< config::string_node >("string-node", "foo");
    t2.set< config::int_node >("deeper.int.node", 15);
    t2.set_string("deeper.dynamic.first", "value1");
    t2.set_string("deeper.dynamic.second", "value2");
    const config::tree combined = t1.combine(t2);

    ATF_REQUIRE(t2 == combined);
}


ATF_TEST_CASE_WITHOUT_HEAD(combine__same_layout__mix);
ATF_TEST_CASE_BODY(combine__same_layout__mix)
{
    config::tree t1, t2;
    init_tree_for_combine_test(t1);
    init_tree_for_combine_test(t2);
    t1.set< config::int_node >("int-node", 3);
    t2.set< config::int_node >("int-node", 5);
    t1.set< config::string_node >("string-node", "foo");
    t2.set< config::int_node >("deeper.int.node", 15);
    t1.set_string("deeper.dynamic.first", "value1");
    t1.set_string("deeper.dynamic.second", "value2.1");
    t2.set_string("deeper.dynamic.second", "value2.2");
    t2.set_string("deeper.dynamic.third", "value3");
    const config::tree combined = t1.combine(t2);

    config::tree expected;
    init_tree_for_combine_test(expected);
    expected.set< config::int_node >("int-node", 5);
    expected.set< config::string_node >("string-node", "foo");
    expected.set< config::int_node >("deeper.int.node", 15);
    expected.set_string("deeper.dynamic.first", "value1");
    expected.set_string("deeper.dynamic.second", "value2.2");
    expected.set_string("deeper.dynamic.third", "value3");
    ATF_REQUIRE(expected == combined);
}


ATF_TEST_CASE_WITHOUT_HEAD(combine__different_layout);
ATF_TEST_CASE_BODY(combine__different_layout)
{
    config::tree t1;
    t1.define< config::int_node >("common.base1");
    t1.define< config::int_node >("common.base2");
    t1.define_dynamic("dynamic.base");
    t1.define< config::int_node >("unset.base");

    config::tree t2;
    t2.define< config::int_node >("common.base2");
    t2.define< config::int_node >("common.base3");
    t2.define_dynamic("dynamic.other");
    t2.define< config::int_node >("unset.other");

    t1.set< config::int_node >("common.base1", 1);
    t1.set< config::int_node >("common.base2", 2);
    t1.set_string("dynamic.base.first", "foo");
    t1.set_string("dynamic.base.second", "bar");

    t2.set< config::int_node >("common.base2", 4);
    t2.set< config::int_node >("common.base3", 3);
    t2.set_string("dynamic.other.first", "FOO");
    t2.set_string("dynamic.other.second", "BAR");

    config::tree combined = t1.combine(t2);

    config::tree expected;
    expected.define< config::int_node >("common.base1");
    expected.define< config::int_node >("common.base2");
    expected.define< config::int_node >("common.base3");
    expected.define_dynamic("dynamic.base");
    expected.define_dynamic("dynamic.other");
    expected.define< config::int_node >("unset.base");
    expected.define< config::int_node >("unset.other");

    expected.set< config::int_node >("common.base1", 1);
    expected.set< config::int_node >("common.base2", 4);
    expected.set< config::int_node >("common.base3", 3);
    expected.set_string("dynamic.base.first", "foo");
    expected.set_string("dynamic.base.second", "bar");
    expected.set_string("dynamic.other.first", "FOO");
    expected.set_string("dynamic.other.second", "BAR");

    ATF_REQUIRE(expected == combined);

    // The combined tree should have respected existing but unset nodes.  Check
    // that these calls do not crash.
    combined.set< config::int_node >("unset.base", 5);
    combined.set< config::int_node >("unset.other", 5);
}


ATF_TEST_CASE_WITHOUT_HEAD(combine__dynamic_wins);
ATF_TEST_CASE_BODY(combine__dynamic_wins)
{
    config::tree t1;
    t1.define< config::int_node >("inner.leaf1");
    t1.set< config::int_node >("inner.leaf1", 3);

    config::tree t2;
    t2.define_dynamic("inner");
    t2.set_string("inner.leaf2", "4");

    config::tree combined = t1.combine(t2);

    config::tree expected;
    expected.define_dynamic("inner");
    expected.set_string("inner.leaf1", "3");
    expected.set_string("inner.leaf2", "4");

    ATF_REQUIRE(expected == combined);

    // The combined inner node should have become dynamic so this call should
    // not fail.
    combined.set_string("inner.leaf3", "5");
}


ATF_TEST_CASE_WITHOUT_HEAD(combine__inner_leaf_mismatch);
ATF_TEST_CASE_BODY(combine__inner_leaf_mismatch)
{
    config::tree t1;
    t1.define< config::int_node >("top.foo.bar");

    config::tree t2;
    t2.define< config::int_node >("top.foo");

    ATF_REQUIRE_THROW_RE(config::bad_combination_error,
                         "'top.foo' is an inner node in the base tree but a "
                         "leaf node in the overrides tree",
                         t1.combine(t2));

    ATF_REQUIRE_THROW_RE(config::bad_combination_error,
                         "'top.foo' is a leaf node in the base tree but an "
                         "inner node in the overrides tree",
                         t2.combine(t1));
}


ATF_TEST_CASE_WITHOUT_HEAD(lookup__invalid_key);
ATF_TEST_CASE_BODY(lookup__invalid_key)
{
    config::tree tree;

    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.lookup< config::int_node >("."));
}


ATF_TEST_CASE_WITHOUT_HEAD(lookup__unknown_key);
ATF_TEST_CASE_BODY(lookup__unknown_key)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar");
    tree.define< config::int_node >("a.b.c");
    tree.define_dynamic("a.d");
    tree.set< config::int_node >("a.b.c", 123);
    tree.set< config::int_node >("a.d.100", 0);

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("abc"));

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("foo"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("foo.bar"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("foo.bar.baz"));

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.b"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.c"));
    (void)tree.lookup< config::int_node >("a.b.c");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.b.c.d"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.d"));
    (void)tree.lookup< config::int_node >("a.d.100");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.d.101"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.d.100.3"));
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.lookup< config::int_node >("a.d.e"));
}


ATF_TEST_CASE_WITHOUT_HEAD(is_set__one_level);
ATF_TEST_CASE_BODY(is_set__one_level)
{
    config::tree tree;

    tree.define< config::int_node >("var1");
    tree.define< config::string_node >("var2");
    tree.define< config::bool_node >("var3");

    tree.set< config::int_node >("var1", 42);
    tree.set< config::bool_node >("var3", false);

    ATF_REQUIRE( tree.is_set("var1"));
    ATF_REQUIRE(!tree.is_set("var2"));
    ATF_REQUIRE( tree.is_set("var3"));
}


ATF_TEST_CASE_WITHOUT_HEAD(is_set__multiple_levels);
ATF_TEST_CASE_BODY(is_set__multiple_levels)
{
    config::tree tree;

    tree.define< config::int_node >("a.b.var1");
    tree.define< config::string_node >("a.b.var2");
    tree.define< config::bool_node >("e.var3");

    tree.set< config::int_node >("a.b.var1", 42);
    tree.set< config::bool_node >("e.var3", false);

    ATF_REQUIRE(!tree.is_set("a"));
    ATF_REQUIRE(!tree.is_set("a.b"));
    ATF_REQUIRE( tree.is_set("a.b.var1"));
    ATF_REQUIRE(!tree.is_set("a.b.var1.trailing"));

    ATF_REQUIRE(!tree.is_set("a"));
    ATF_REQUIRE(!tree.is_set("a.b"));
    ATF_REQUIRE(!tree.is_set("a.b.var2"));
    ATF_REQUIRE(!tree.is_set("a.b.var2.trailing"));

    ATF_REQUIRE(!tree.is_set("e"));
    ATF_REQUIRE( tree.is_set("e.var3"));
    ATF_REQUIRE(!tree.is_set("e.var3.trailing"));
}


ATF_TEST_CASE_WITHOUT_HEAD(is_set__invalid_key);
ATF_TEST_CASE_BODY(is_set__invalid_key)
{
    config::tree tree;

    ATF_REQUIRE_THROW(config::invalid_key_error, tree.is_set(".abc"));
}


ATF_TEST_CASE_WITHOUT_HEAD(set__invalid_key);
ATF_TEST_CASE_BODY(set__invalid_key)
{
    config::tree tree;

    ATF_REQUIRE_THROW(config::invalid_key_error,
                      tree.set< config::int_node >("foo.", 54));
}


ATF_TEST_CASE_WITHOUT_HEAD(set__invalid_key_value);
ATF_TEST_CASE_BODY(set__invalid_key_value)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar");
    tree.define_dynamic("a.d");

    ATF_REQUIRE_THROW(config::invalid_key_value,
                      tree.set< config::int_node >("foo", 3));
    ATF_REQUIRE_THROW(config::invalid_key_value,
                      tree.set< config::int_node >("a", -10));
}


ATF_TEST_CASE_WITHOUT_HEAD(set__unknown_key);
ATF_TEST_CASE_BODY(set__unknown_key)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar");
    tree.define< config::int_node >("a.b.c");
    tree.define_dynamic("a.d");
    tree.set< config::int_node >("a.b.c", 123);
    tree.set< config::string_node >("a.d.3", "foo");

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("abc", 2));

    tree.set< config::int_node >("foo.bar", 15);
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("foo.bar.baz", 0));

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("a.c", 100));
    tree.set< config::int_node >("a.b.c", -3);
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("a.b.c.d", 82));
    tree.set< config::string_node >("a.d.3", "bar");
    tree.set< config::string_node >("a.d.4", "bar");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set< config::int_node >("a.d.4.5", 82));
    tree.set< config::int_node >("a.d.5.6", 82);
}


ATF_TEST_CASE_WITHOUT_HEAD(set__unknown_key_not_strict);
ATF_TEST_CASE_BODY(set__unknown_key_not_strict)
{
    config::tree tree(false);

    tree.define< config::int_node >("foo.bar");
    tree.define< config::int_node >("a.b.c");
    tree.define_dynamic("a.d");
    tree.set< config::int_node >("a.b.c", 123);
    tree.set< config::string_node >("a.d.3", "foo");

    tree.set< config::int_node >("abc", 2);
    ATF_REQUIRE(!tree.is_set("abc"));

    tree.set< config::int_node >("foo.bar", 15);
    tree.set< config::int_node >("foo.bar.baz", 0);
    ATF_REQUIRE(!tree.is_set("foo.bar.baz"));

    tree.set< config::int_node >("a.c", 100);
    ATF_REQUIRE(!tree.is_set("a.c"));
}


ATF_TEST_CASE_WITHOUT_HEAD(push_lua__ok);
ATF_TEST_CASE_BODY(push_lua__ok)
{
    config::tree tree;

    tree.define< config::int_node >("top.integer");
    tree.define< wrapped_int_node >("top.custom");
    tree.define_dynamic("dynamic");
    tree.set< config::int_node >("top.integer", 5);
    tree.set< wrapped_int_node >("top.custom", int_wrapper(10));
    tree.set_string("dynamic.first", "foo");

    lutok::state state;
    tree.push_lua("top.integer", state);
    tree.push_lua("top.custom", state);
    tree.push_lua("dynamic.first", state);
    ATF_REQUIRE(state.is_number(-3));
    ATF_REQUIRE_EQ(5, state.to_integer(-3));
    ATF_REQUIRE(state.is_number(-2));
    ATF_REQUIRE_EQ(10, state.to_integer(-2));
    ATF_REQUIRE(state.is_string(-1));
    ATF_REQUIRE_EQ("foo", state.to_string(-1));
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(set_lua__ok);
ATF_TEST_CASE_BODY(set_lua__ok)
{
    config::tree tree;

    tree.define< config::int_node >("top.integer");
    tree.define< wrapped_int_node >("top.custom");
    tree.define_dynamic("dynamic");

    {
        lutok::state state;
        state.push_integer(5);
        state.push_integer(10);
        state.push_string("foo");
        tree.set_lua("top.integer", state, -3);
        tree.set_lua("top.custom", state, -2);
        tree.set_lua("dynamic.first", state, -1);
        state.pop(3);
    }

    ATF_REQUIRE_EQ(5, tree.lookup< config::int_node >("top.integer"));
    ATF_REQUIRE_EQ(10, tree.lookup< wrapped_int_node >("top.custom").value());
    ATF_REQUIRE_EQ("foo", tree.lookup< config::string_node >("dynamic.first"));
}


ATF_TEST_CASE_WITHOUT_HEAD(lookup_rw);
ATF_TEST_CASE_BODY(lookup_rw)
{
    config::tree tree;

    tree.define< config::int_node >("var1");
    tree.define< config::bool_node >("var3");

    tree.set< config::int_node >("var1", 42);
    tree.set< config::bool_node >("var3", false);

    tree.lookup_rw< config::int_node >("var1") += 10;
    ATF_REQUIRE_EQ(52, tree.lookup< config::int_node >("var1"));
    ATF_REQUIRE(!tree.lookup< config::bool_node >("var3"));
}


ATF_TEST_CASE_WITHOUT_HEAD(lookup_string__ok);
ATF_TEST_CASE_BODY(lookup_string__ok)
{
    config::tree tree;

    tree.define< config::int_node >("var1");
    tree.define< config::string_node >("b.var2");
    tree.define< config::bool_node >("c.d.var3");

    tree.set< config::int_node >("var1", 42);
    tree.set< config::string_node >("b.var2", "hello");
    tree.set< config::bool_node >("c.d.var3", false);

    ATF_REQUIRE_EQ("42", tree.lookup_string("var1"));
    ATF_REQUIRE_EQ("hello", tree.lookup_string("b.var2"));
    ATF_REQUIRE_EQ("false", tree.lookup_string("c.d.var3"));
}


ATF_TEST_CASE_WITHOUT_HEAD(lookup_string__invalid_key);
ATF_TEST_CASE_BODY(lookup_string__invalid_key)
{
    config::tree tree;

    ATF_REQUIRE_THROW(config::invalid_key_error, tree.lookup_string(""));
}


ATF_TEST_CASE_WITHOUT_HEAD(lookup_string__unknown_key);
ATF_TEST_CASE_BODY(lookup_string__unknown_key)
{
    config::tree tree;

    tree.define< config::int_node >("a.b.c");

    ATF_REQUIRE_THROW(config::unknown_key_error, tree.lookup_string("a.b"));
    ATF_REQUIRE_THROW(config::unknown_key_error, tree.lookup_string("a.b.c.d"));
}


ATF_TEST_CASE_WITHOUT_HEAD(set_string__ok);
ATF_TEST_CASE_BODY(set_string__ok)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar.1");
    tree.define< config::string_node >("foo.bar.2");
    tree.define_dynamic("sub.tree");

    tree.set_string("foo.bar.1", "42");
    tree.set_string("foo.bar.2", "hello");
    tree.set_string("sub.tree.2", "15");
    tree.set_string("sub.tree.3.4", "bye");

    ATF_REQUIRE_EQ(42, tree.lookup< config::int_node >("foo.bar.1"));
    ATF_REQUIRE_EQ("hello", tree.lookup< config::string_node >("foo.bar.2"));
    ATF_REQUIRE_EQ("15", tree.lookup< config::string_node >("sub.tree.2"));
    ATF_REQUIRE_EQ("bye", tree.lookup< config::string_node >("sub.tree.3.4"));
}


ATF_TEST_CASE_WITHOUT_HEAD(set_string__invalid_key);
ATF_TEST_CASE_BODY(set_string__invalid_key)
{
    config::tree tree;

    ATF_REQUIRE_THROW(config::invalid_key_error, tree.set_string(".", "foo"));
}


ATF_TEST_CASE_WITHOUT_HEAD(set_string__invalid_key_value);
ATF_TEST_CASE_BODY(set_string__invalid_key_value)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar");

    ATF_REQUIRE_THROW(config::invalid_key_value,
                      tree.set_string("foo", "abc"));
    ATF_REQUIRE_THROW(config::invalid_key_value,
                      tree.set_string("foo.bar", " -3"));
    ATF_REQUIRE_THROW(config::invalid_key_value,
                      tree.set_string("foo.bar", "3 "));
}


ATF_TEST_CASE_WITHOUT_HEAD(set_string__unknown_key);
ATF_TEST_CASE_BODY(set_string__unknown_key)
{
    config::tree tree;

    tree.define< config::int_node >("foo.bar");
    tree.define< config::int_node >("a.b.c");
    tree.define_dynamic("a.d");
    tree.set_string("a.b.c", "123");
    tree.set_string("a.d.3", "foo");

    ATF_REQUIRE_THROW(config::unknown_key_error, tree.set_string("abc", "2"));

    tree.set_string("foo.bar", "15");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set_string("foo.bar.baz", "0"));

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set_string("a.c", "100"));
    tree.set_string("a.b.c", "-3");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set_string("a.b.c.d", "82"));
    tree.set_string("a.d.3", "bar");
    tree.set_string("a.d.4", "bar");
    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.set_string("a.d.4.5", "82"));
    tree.set_string("a.d.5.6", "82");
}


ATF_TEST_CASE_WITHOUT_HEAD(set_string__unknown_key_not_strict);
ATF_TEST_CASE_BODY(set_string__unknown_key_not_strict)
{
    config::tree tree(false);

    tree.define< config::int_node >("foo.bar");
    tree.define< config::int_node >("a.b.c");
    tree.define_dynamic("a.d");
    tree.set_string("a.b.c", "123");
    tree.set_string("a.d.3", "foo");

    tree.set_string("abc", "2");
    ATF_REQUIRE(!tree.is_set("abc"));

    tree.set_string("foo.bar", "15");
    tree.set_string("foo.bar.baz", "0");
    ATF_REQUIRE(!tree.is_set("foo.bar.baz"));

    tree.set_string("a.c", "100");
    ATF_REQUIRE(!tree.is_set("a.c"));
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__none);
ATF_TEST_CASE_BODY(all_properties__none)
{
    const config::tree tree;
    ATF_REQUIRE(tree.all_properties().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__all_set);
ATF_TEST_CASE_BODY(all_properties__all_set)
{
    config::tree tree;

    tree.define< config::int_node >("plain");
    tree.set< config::int_node >("plain", 1234);

    tree.define< config::int_node >("static.first");
    tree.set< config::int_node >("static.first", -3);
    tree.define< config::string_node >("static.second");
    tree.set< config::string_node >("static.second", "some text");

    tree.define_dynamic("dynamic");
    tree.set< config::string_node >("dynamic.first", "hello");
    tree.set< config::string_node >("dynamic.second", "bye");

    config::properties_map exp_properties;
    exp_properties["plain"] = "1234";
    exp_properties["static.first"] = "-3";
    exp_properties["static.second"] = "some text";
    exp_properties["dynamic.first"] = "hello";
    exp_properties["dynamic.second"] = "bye";

    const config::properties_map properties = tree.all_properties();
    ATF_REQUIRE(exp_properties == properties);
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__some_unset);
ATF_TEST_CASE_BODY(all_properties__some_unset)
{
    config::tree tree;

    tree.define< config::int_node >("static.first");
    tree.set< config::int_node >("static.first", -3);
    tree.define< config::string_node >("static.second");

    tree.define_dynamic("dynamic");

    config::properties_map exp_properties;
    exp_properties["static.first"] = "-3";

    const config::properties_map properties = tree.all_properties();
    ATF_REQUIRE(exp_properties == properties);
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__subtree__inner);
ATF_TEST_CASE_BODY(all_properties__subtree__inner)
{
    config::tree tree;

    tree.define< config::int_node >("root.a.b.c.first");
    tree.define< config::int_node >("root.a.b.c.second");
    tree.define< config::int_node >("root.a.d.first");

    tree.set< config::int_node >("root.a.b.c.first", 1);
    tree.set< config::int_node >("root.a.b.c.second", 2);
    tree.set< config::int_node >("root.a.d.first", 3);

    {
        config::properties_map exp_properties;
        exp_properties["root.a.b.c.first"] = "1";
        exp_properties["root.a.b.c.second"] = "2";
        exp_properties["root.a.d.first"] = "3";
        ATF_REQUIRE(exp_properties == tree.all_properties("root"));
        ATF_REQUIRE(exp_properties == tree.all_properties("root.a"));
    }

    {
        config::properties_map exp_properties;
        exp_properties["root.a.b.c.first"] = "1";
        exp_properties["root.a.b.c.second"] = "2";
        ATF_REQUIRE(exp_properties == tree.all_properties("root.a.b"));
        ATF_REQUIRE(exp_properties == tree.all_properties("root.a.b.c"));
    }

    {
        config::properties_map exp_properties;
        exp_properties["root.a.d.first"] = "3";
        ATF_REQUIRE(exp_properties == tree.all_properties("root.a.d"));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__subtree__leaf);
ATF_TEST_CASE_BODY(all_properties__subtree__leaf)
{
    config::tree tree;

    tree.define< config::int_node >("root.a.b.c.first");
    tree.set< config::int_node >("root.a.b.c.first", 1);
    ATF_REQUIRE_THROW_RE(config::value_error, "Cannot export.*leaf",
                         tree.all_properties("root.a.b.c.first"));
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__subtree__strip_key);
ATF_TEST_CASE_BODY(all_properties__subtree__strip_key)
{
    config::tree tree;

    tree.define< config::int_node >("root.a.b.c.first");
    tree.define< config::int_node >("root.a.b.c.second");
    tree.define< config::int_node >("root.a.d.first");

    tree.set< config::int_node >("root.a.b.c.first", 1);
    tree.set< config::int_node >("root.a.b.c.second", 2);
    tree.set< config::int_node >("root.a.d.first", 3);

    config::properties_map exp_properties;
    exp_properties["b.c.first"] = "1";
    exp_properties["b.c.second"] = "2";
    exp_properties["d.first"] = "3";
    ATF_REQUIRE(exp_properties == tree.all_properties("root.a", true));
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__subtree__invalid_key);
ATF_TEST_CASE_BODY(all_properties__subtree__invalid_key)
{
    config::tree tree;

    ATF_REQUIRE_THROW(config::invalid_key_error, tree.all_properties("."));
}


ATF_TEST_CASE_WITHOUT_HEAD(all_properties__subtree__unknown_key);
ATF_TEST_CASE_BODY(all_properties__subtree__unknown_key)
{
    config::tree tree;

    tree.define< config::int_node >("root.a.b.c.first");
    tree.set< config::int_node >("root.a.b.c.first", 1);
    tree.define< config::int_node >("root.a.b.c.unset");

    ATF_REQUIRE_THROW(config::unknown_key_error,
                      tree.all_properties("root.a.b.c.first.foo"));
    ATF_REQUIRE_THROW_RE(config::value_error, "Cannot export.*leaf",
                         tree.all_properties("root.a.b.c.unset"));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__empty);
ATF_TEST_CASE_BODY(operators_eq_and_ne__empty)
{
    config::tree t1;
    config::tree t2;
    ATF_REQUIRE(  t1 == t2);
    ATF_REQUIRE(!(t1 != t2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__shallow_copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__shallow_copy)
{
    config::tree t1;
    t1.define< config::int_node >("root.a.b.c.first");
    t1.set< config::int_node >("root.a.b.c.first", 1);
    config::tree t2 = t1;
    ATF_REQUIRE(  t1 == t2);
    ATF_REQUIRE(!(t1 != t2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__deep_copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__deep_copy)
{
    config::tree t1;
    t1.define< config::int_node >("root.a.b.c.first");
    t1.set< config::int_node >("root.a.b.c.first", 1);
    config::tree t2 = t1.deep_copy();
    ATF_REQUIRE(  t1 == t2);
    ATF_REQUIRE(!(t1 != t2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__some_contents);
ATF_TEST_CASE_BODY(operators_eq_and_ne__some_contents)
{
    config::tree t1, t2;

    t1.define< config::int_node >("root.a.b.c.first");
    t1.set< config::int_node >("root.a.b.c.first", 1);
    ATF_REQUIRE(!(t1 == t2));
    ATF_REQUIRE(  t1 != t2);

    t2.define< config::int_node >("root.a.b.c.first");
    t2.set< config::int_node >("root.a.b.c.first", 1);
    ATF_REQUIRE(  t1 == t2);
    ATF_REQUIRE(!(t1 != t2));

    t1.set< config::int_node >("root.a.b.c.first", 2);
    ATF_REQUIRE(!(t1 == t2));
    ATF_REQUIRE(  t1 != t2);

    t2.set< config::int_node >("root.a.b.c.first", 2);
    ATF_REQUIRE(  t1 == t2);
    ATF_REQUIRE(!(t1 != t2));

    t1.define< config::string_node >("another.key");
    t1.set< config::string_node >("another.key", "some text");
    ATF_REQUIRE(!(t1 == t2));
    ATF_REQUIRE(  t1 != t2);

    t2.define< config::string_node >("another.key");
    t2.set< config::string_node >("another.key", "some text");
    ATF_REQUIRE(  t1 == t2);
    ATF_REQUIRE(!(t1 != t2));
}


ATF_TEST_CASE_WITHOUT_HEAD(custom_leaf__no_default_ctor);
ATF_TEST_CASE_BODY(custom_leaf__no_default_ctor)
{
    config::tree tree;

    tree.define< wrapped_int_node >("test1");
    tree.define< wrapped_int_node >("test2");
    tree.set< wrapped_int_node >("test1", int_wrapper(5));
    tree.set< wrapped_int_node >("test2", int_wrapper(10));
    const int_wrapper& test1 = tree.lookup< wrapped_int_node >("test1");
    ATF_REQUIRE_EQ(5, test1.value());
    const int_wrapper& test2 = tree.lookup< wrapped_int_node >("test2");
    ATF_REQUIRE_EQ(10, test2.value());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, define_set_lookup__one_level);
    ATF_ADD_TEST_CASE(tcs, define_set_lookup__multiple_levels);

    ATF_ADD_TEST_CASE(tcs, deep_copy__empty);
    ATF_ADD_TEST_CASE(tcs, deep_copy__some);

    ATF_ADD_TEST_CASE(tcs, combine__empty);
    ATF_ADD_TEST_CASE(tcs, combine__same_layout__no_overrides);
    ATF_ADD_TEST_CASE(tcs, combine__same_layout__no_base);
    ATF_ADD_TEST_CASE(tcs, combine__same_layout__mix);
    ATF_ADD_TEST_CASE(tcs, combine__different_layout);
    ATF_ADD_TEST_CASE(tcs, combine__dynamic_wins);
    ATF_ADD_TEST_CASE(tcs, combine__inner_leaf_mismatch);

    ATF_ADD_TEST_CASE(tcs, lookup__invalid_key);
    ATF_ADD_TEST_CASE(tcs, lookup__unknown_key);

    ATF_ADD_TEST_CASE(tcs, is_set__one_level);
    ATF_ADD_TEST_CASE(tcs, is_set__multiple_levels);
    ATF_ADD_TEST_CASE(tcs, is_set__invalid_key);

    ATF_ADD_TEST_CASE(tcs, set__invalid_key);
    ATF_ADD_TEST_CASE(tcs, set__invalid_key_value);
    ATF_ADD_TEST_CASE(tcs, set__unknown_key);
    ATF_ADD_TEST_CASE(tcs, set__unknown_key_not_strict);

    ATF_ADD_TEST_CASE(tcs, push_lua__ok);
    ATF_ADD_TEST_CASE(tcs, set_lua__ok);

    ATF_ADD_TEST_CASE(tcs, lookup_rw);

    ATF_ADD_TEST_CASE(tcs, lookup_string__ok);
    ATF_ADD_TEST_CASE(tcs, lookup_string__invalid_key);
    ATF_ADD_TEST_CASE(tcs, lookup_string__unknown_key);

    ATF_ADD_TEST_CASE(tcs, set_string__ok);
    ATF_ADD_TEST_CASE(tcs, set_string__invalid_key);
    ATF_ADD_TEST_CASE(tcs, set_string__invalid_key_value);
    ATF_ADD_TEST_CASE(tcs, set_string__unknown_key);
    ATF_ADD_TEST_CASE(tcs, set_string__unknown_key_not_strict);

    ATF_ADD_TEST_CASE(tcs, all_properties__none);
    ATF_ADD_TEST_CASE(tcs, all_properties__all_set);
    ATF_ADD_TEST_CASE(tcs, all_properties__some_unset);
    ATF_ADD_TEST_CASE(tcs, all_properties__subtree__inner);
    ATF_ADD_TEST_CASE(tcs, all_properties__subtree__leaf);
    ATF_ADD_TEST_CASE(tcs, all_properties__subtree__strip_key);
    ATF_ADD_TEST_CASE(tcs, all_properties__subtree__invalid_key);
    ATF_ADD_TEST_CASE(tcs, all_properties__subtree__unknown_key);

    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__empty);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__shallow_copy);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__deep_copy);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__some_contents);

    ATF_ADD_TEST_CASE(tcs, custom_leaf__no_default_ctor);
}
