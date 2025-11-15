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

#include "model/metadata.hpp"

#include <memory>

#include "engine/execenv/execenv.hpp"
#include "model/exceptions.hpp"
#include "model/types.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/config/nodes.ipp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.hpp"
#include "utils/units.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace text = utils::text;
namespace units = utils::units;

using utils::optional;


namespace {


/// Global instance of defaults.
///
/// This exists so that the getters in metadata can return references instead
/// of object copies.  Use get_defaults() to query.
static optional< config::tree > defaults;


/// A leaf node that holds a bytes quantity.
class bytes_node : public config::native_leaf_node< units::bytes > {
public:
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::unique_ptr< bytes_node > new_node(new bytes_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Pushes the node's value onto the Lua stack.
    void
    push_lua(lutok::state& /* state */) const
    {
        UNREACHABLE;
    }

    /// Sets the value of the node from an entry in the Lua stack.
    void
    set_lua(lutok::state& /* state */, const int /* index */)
    {
        UNREACHABLE;
    }
};


/// A leaf node that holds a time delta.
class delta_node : public config::typed_leaf_node< datetime::delta > {
public:
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::unique_ptr< delta_node > new_node(new delta_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Sets the value of the node from a raw string representation.
    ///
    /// \param raw_value The value to set the node to.
    ///
    /// \throw value_error If the value is invalid.
    void
    set_string(const std::string& raw_value)
    {
        unsigned int seconds;
        try {
            seconds = text::to_type< unsigned int >(raw_value);
        } catch (const text::error& e) {
            throw config::value_error(F("Invalid time delta %s") % raw_value);
        }
        set(datetime::delta(seconds, 0));
    }

    /// Converts the contents of the node to a string.
    ///
    /// \pre The node must have a value.
    ///
    /// \return A string representation of the value held by the node.
    std::string
    to_string(void) const
    {
        return F("%s") % value().seconds;
    }

    /// Pushes the node's value onto the Lua stack.
    void
    push_lua(lutok::state& /* state */) const
    {
        UNREACHABLE;
    }

    /// Sets the value of the node from an entry in the Lua stack.
    void
    set_lua(lutok::state& /* state */, const int /* index */)
    {
        UNREACHABLE;
    }
};


/// A leaf node that holds a "required user" property.
///
/// This node is just a string, but it provides validation of the only allowed
/// values.
class user_node : public config::string_node {
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::unique_ptr< user_node > new_node(new user_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Checks a given user textual representation for validity.
    ///
    /// \param user The value to validate.
    ///
    /// \throw config::value_error If the value is not valid.
    void
    validate(const value_type& user) const
    {
        if (!user.empty() && user != "root" && user != "unprivileged")
            throw config::value_error("Invalid required user value");
    }
};


/// A leaf node that holds a set of paths.
///
/// This node type is used to represent the value of the required files and
/// required programs, for example, and these do not allow relative paths.  We
/// check this here.
class paths_set_node : public config::base_set_node< fs::path > {
    /// Copies the node.
    ///
    /// \return A dynamically-allocated node.
    virtual base_node*
    deep_copy(void) const
    {
        std::unique_ptr< paths_set_node > new_node(new paths_set_node());
        new_node->_value = _value;
        return new_node.release();
    }

    /// Converts a single path to the native type.
    ///
    /// \param raw_value The value to parse.
    ///
    /// \return The parsed value.
    ///
    /// \throw config::value_error If the value is invalid.
    fs::path
    parse_one(const std::string& raw_value) const
    {
        try {
            return fs::path(raw_value);
        } catch (const fs::error& e) {
            throw config::value_error(e.what());
        }
    }

    /// Checks a collection of paths for validity.
    ///
    /// \param paths The value to validate.
    ///
    /// \throw config::value_error If the value is not valid.
    void
    validate(const value_type& paths) const
    {
        for (value_type::const_iterator iter = paths.begin();
             iter != paths.end(); ++iter) {
            const fs::path& path = *iter;
            if (!path.is_absolute() && path.ncomponents() > 1)
                throw config::value_error(F("Relative path '%s' not allowed") %
                                          *iter);
        }
    }
};


/// Initializes a tree to hold test case requirements.
///
/// \param [in,out] tree The tree to initialize.
static void
init_tree(config::tree& tree)
{
    tree.define< config::strings_set_node >("allowed_architectures");
    tree.define< config::strings_set_node >("allowed_platforms");
    tree.define_dynamic("custom");
    tree.define< config::string_node >("description");
    tree.define< config::string_node >("execenv");
    tree.define< config::string_node >("execenv_jail_params");
    tree.define< config::bool_node >("has_cleanup");
    tree.define< config::bool_node >("is_exclusive");
    tree.define< config::strings_set_node >("required_configs");
    tree.define< bytes_node >("required_disk_space");
    tree.define< paths_set_node >("required_files");
    tree.define< bytes_node >("required_memory");
    tree.define< config::strings_set_node >("required_kmods");
    tree.define< paths_set_node >("required_programs");
    tree.define< user_node >("required_user");
    tree.define< delta_node >("timeout");
}


/// Sets default values on a tree object.
///
/// \param [in,out] tree The tree to configure.
static void
set_defaults(config::tree& tree)
{
    tree.set< config::strings_set_node >("allowed_architectures",
                                         model::strings_set());
    tree.set< config::strings_set_node >("allowed_platforms",
                                         model::strings_set());
    tree.set< config::string_node >("description", "");
    tree.set< config::string_node >("execenv", "");
    tree.set< config::string_node >("execenv_jail_params", "");
    tree.set< config::bool_node >("has_cleanup", false);
    tree.set< config::bool_node >("is_exclusive", false);
    tree.set< config::strings_set_node >("required_configs",
                                         model::strings_set());
    tree.set< bytes_node >("required_disk_space", units::bytes(0));
    tree.set< paths_set_node >("required_files", model::paths_set());
    tree.set< bytes_node >("required_memory", units::bytes(0));
    tree.set< config::strings_set_node >("required_kmods", model::strings_set());
    tree.set< paths_set_node >("required_programs", model::paths_set());
    tree.set< user_node >("required_user", "");
    // TODO(jmmv): We shouldn't be setting a default timeout like this.  See
    // Issue 5 for details.
    tree.set< delta_node >("timeout", datetime::delta(300, 0));
}


/// Queries the global defaults tree object with lazy initialization.
///
/// \return A metadata tree.  This object is statically allocated so it is
/// acceptable to obtain references to it and its members.
const config::tree&
get_defaults(void)
{
    if (!defaults) {
        config::tree props;
        init_tree(props);
        set_defaults(props);
        defaults = props;
    }
    return defaults.get();
}


/// Looks up a value in a tree with error rewriting.
///
/// \tparam NodeType The type of the node.
/// \param tree The tree in which to insert the value.
/// \param key The key to set.
///
/// \return A read-write reference to the value in the node.
///
/// \throw model::error If the key is not known or if the value is not valid.
template< class NodeType >
typename NodeType::value_type&
lookup_rw(config::tree& tree, const std::string& key)
{
    try {
        return tree.lookup_rw< NodeType >(key);
    } catch (const config::unknown_key_error& e) {
        throw model::error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw model::error(F("Invalid value for metadata property %s: %s") %
                            key % e.what());
    }
}


/// Sets a value in a tree with error rewriting.
///
/// \tparam NodeType The type of the node.
/// \param tree The tree in which to insert the value.
/// \param key The key to set.
/// \param value The value to set the node to.
///
/// \throw model::error If the key is not known or if the value is not valid.
template< class NodeType >
void
set(config::tree& tree, const std::string& key,
    const typename NodeType::value_type& value)
{
    try {
        tree.set< NodeType >(key, value);
    } catch (const config::unknown_key_error& e) {
        throw model::error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw model::error(F("Invalid value for metadata property %s: %s") %
                            key % e.what());
    }
}


}  // anonymous namespace


/// Internal implementation of the metadata class.
struct model::metadata::impl : utils::noncopyable {
    /// Metadata properties.
    config::tree props;

    /// Constructor.
    ///
    /// \param props_ Metadata properties of the test.
    impl(const utils::config::tree& props_) :
        props(props_)
    {
    }

    /// Equality comparator.
    ///
    /// \param other The other object to compare this one to.
    ///
    /// \return True if this object and other are equal; false otherwise.
    bool
    operator==(const impl& other) const
    {
        return (get_defaults().combine(props) ==
                get_defaults().combine(other.props));
    }
};


/// Constructor.
///
/// \param props Metadata properties of the test.
model::metadata::metadata(const utils::config::tree& props) :
    _pimpl(new impl(props))
{
}


/// Destructor.
model::metadata::~metadata(void)
{
}


/// Applies a set of overrides to this metadata object.
///
/// \param overrides The overrides to apply.  Any values explicitly set in this
///     other object will override any possible values set in this object.
///
/// \return A new metadata object with the combination.
model::metadata
model::metadata::apply_overrides(const metadata& overrides) const
{
    return metadata(_pimpl->props.combine(overrides._pimpl->props));
}


/// Returns the architectures allowed by the test.
///
/// \return Set of architectures, or empty if this does not apply.
const model::strings_set&
model::metadata::allowed_architectures(void) const
{
    if (_pimpl->props.is_set("allowed_architectures")) {
        return _pimpl->props.lookup< config::strings_set_node >(
            "allowed_architectures");
    } else {
        return get_defaults().lookup< config::strings_set_node >(
            "allowed_architectures");
    }
}


/// Returns the platforms allowed by the test.
///
/// \return Set of platforms, or empty if this does not apply.
const model::strings_set&
model::metadata::allowed_platforms(void) const
{
    if (_pimpl->props.is_set("allowed_platforms")) {
        return _pimpl->props.lookup< config::strings_set_node >(
            "allowed_platforms");
    } else {
        return get_defaults().lookup< config::strings_set_node >(
            "allowed_platforms");
    }
}


/// Returns all the user-defined metadata properties.
///
/// \return A key/value map of properties.
model::properties_map
model::metadata::custom(void) const
{
    return _pimpl->props.all_properties("custom", true);
}


/// Returns the description of the test.
///
/// \return Textual description; may be empty.
const std::string&
model::metadata::description(void) const
{
    if (_pimpl->props.is_set("description")) {
        return _pimpl->props.lookup< config::string_node >("description");
    } else {
        return get_defaults().lookup< config::string_node >("description");
    }
}


/// Returns execution environment name.
///
/// \return Name of configured execution environment.
const std::string&
model::metadata::execenv(void) const
{
    if (_pimpl->props.is_set("execenv")) {
        return _pimpl->props.lookup< config::string_node >("execenv");
    } else {
        return get_defaults().lookup< config::string_node >("execenv");
    }
}


/// Returns execenv jail(8) parameters string to run a test with.
///
/// \return String of jail parameters.
const std::string&
model::metadata::execenv_jail_params(void) const
{
    if (_pimpl->props.is_set("execenv_jail_params")) {
        return _pimpl->props.lookup< config::string_node >(
            "execenv_jail_params");
    } else {
        return get_defaults().lookup< config::string_node >(
            "execenv_jail_params");
    }
}


/// Returns whether the test has a cleanup part or not.
///
/// \return True if there is a cleanup part; false otherwise.
bool
model::metadata::has_cleanup(void) const
{
    if (_pimpl->props.is_set("has_cleanup")) {
        return _pimpl->props.lookup< config::bool_node >("has_cleanup");
    } else {
        return get_defaults().lookup< config::bool_node >("has_cleanup");
    }
}


/// Returns whether the test has a specific execenv apart from default one.
///
/// \return True if there is a non-host execenv configured; false otherwise.
bool
model::metadata::has_execenv(void) const
{
    const std::string& name = execenv();
    return !name.empty() && name != engine::execenv::default_execenv_name;
}


/// Returns whether the test is exclusive or not.
///
/// \return True if the test has to be run on its own, not concurrently with any
/// other tests; false otherwise.
bool
model::metadata::is_exclusive(void) const
{
    if (_pimpl->props.is_set("is_exclusive")) {
        return _pimpl->props.lookup< config::bool_node >("is_exclusive");
    } else {
        return get_defaults().lookup< config::bool_node >("is_exclusive");
    }
}


/// Returns the list of configuration variables needed by the test.
///
/// \return Set of configuration variables.
const model::strings_set&
model::metadata::required_configs(void) const
{
    if (_pimpl->props.is_set("required_configs")) {
        return _pimpl->props.lookup< config::strings_set_node >(
            "required_configs");
    } else {
        return get_defaults().lookup< config::strings_set_node >(
            "required_configs");
    }
}


/// Returns the amount of free disk space required by the test.
///
/// \return Number of bytes, or 0 if this does not apply.
const units::bytes&
model::metadata::required_disk_space(void) const
{
    if (_pimpl->props.is_set("required_disk_space")) {
        return _pimpl->props.lookup< bytes_node >("required_disk_space");
    } else {
        return get_defaults().lookup< bytes_node >("required_disk_space");
    }
}


/// Returns the list of files needed by the test.
///
/// \return Set of paths.
const model::paths_set&
model::metadata::required_files(void) const
{
    if (_pimpl->props.is_set("required_files")) {
        return _pimpl->props.lookup< paths_set_node >("required_files");
    } else {
        return get_defaults().lookup< paths_set_node >("required_files");
    }
}


/// Returns the amount of memory required by the test.
///
/// \return Number of bytes, or 0 if this does not apply.
const units::bytes&
model::metadata::required_memory(void) const
{
    if (_pimpl->props.is_set("required_memory")) {
        return _pimpl->props.lookup< bytes_node >("required_memory");
    } else {
        return get_defaults().lookup< bytes_node >("required_memory");
    }
}


/// Returns the list of kernel modules needed by the test.
///
/// \return Set of kernel module names.
const model::strings_set&
model::metadata::required_kmods(void) const
{
    if (_pimpl->props.is_set("required_kmods")) {
        return _pimpl->props.lookup< config::strings_set_node >(
            "required_kmods");
    } else {
        return get_defaults().lookup< config::strings_set_node >(
            "required_kmods");
    }
}


/// Returns the list of programs needed by the test.
///
/// \return Set of paths.
const model::paths_set&
model::metadata::required_programs(void) const
{
    if (_pimpl->props.is_set("required_programs")) {
        return _pimpl->props.lookup< paths_set_node >("required_programs");
    } else {
        return get_defaults().lookup< paths_set_node >("required_programs");
    }
}


/// Returns the user required by the test.
///
/// \return One of unprivileged, root or empty.
const std::string&
model::metadata::required_user(void) const
{
    if (_pimpl->props.is_set("required_user")) {
        return _pimpl->props.lookup< user_node >("required_user");
    } else {
        return get_defaults().lookup< user_node >("required_user");
    }
}


/// Returns the timeout of the test.
///
/// \return A time delta; should be compared to default_timeout to see if it has
/// been overriden.
const datetime::delta&
model::metadata::timeout(void) const
{
    if (_pimpl->props.is_set("timeout")) {
        return _pimpl->props.lookup< delta_node >("timeout");
    } else {
        return get_defaults().lookup< delta_node >("timeout");
    }
}


/// Externalizes the metadata to a set of key/value textual pairs.
///
/// \return A key/value representation of the metadata.
model::properties_map
model::metadata::to_properties(void) const
{
    const config::tree fully_specified = get_defaults().combine(_pimpl->props);
    return fully_specified.all_properties();
}


/// Equality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are equal; false otherwise.
bool
model::metadata::operator==(const metadata& other) const
{
    return _pimpl == other._pimpl || *_pimpl == *other._pimpl;
}


/// Inequality comparator.
///
/// \param other The other object to compare this one to.
///
/// \return True if this object and other are different; false otherwise.
bool
model::metadata::operator!=(const metadata& other) const
{
    return !(*this == other);
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
std::ostream&
model::operator<<(std::ostream& output, const metadata& object)
{
    output << "metadata{";

    bool first = true;
    const model::properties_map props = object.to_properties();
    for (model::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter) {
        if (!first)
            output << ", ";
        output << F("%s=%s") % (*iter).first %
            text::quote((*iter).second, '\'');
        first = false;
    }

    output << "}";
    return output;
}


/// Internal implementation of the metadata_builder class.
struct model::metadata_builder::impl : utils::noncopyable {
    /// Collection of requirements.
    config::tree props;

    /// Whether we have created a metadata object or not.
    bool built;

    /// Constructor.
    impl(void) :
        built(false)
    {
        init_tree(props);
    }

    /// Constructor.
    ///
    /// \param base The base model to construct a copy from.
    impl(const model::metadata& base) :
        props(base._pimpl->props.deep_copy()),
        built(false)
    {
    }
};


/// Constructor.
model::metadata_builder::metadata_builder(void) :
    _pimpl(new impl())
{
}


/// Constructor.
model::metadata_builder::metadata_builder(const model::metadata& base) :
    _pimpl(new impl(base))
{
}


/// Destructor.
model::metadata_builder::~metadata_builder(void)
{
}


/// Accumulates an additional allowed architecture.
///
/// \param arch The architecture.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::add_allowed_architecture(const std::string& arch)
{
    if (!_pimpl->props.is_set("allowed_architectures")) {
        _pimpl->props.set< config::strings_set_node >(
            "allowed_architectures",
            get_defaults().lookup< config::strings_set_node >(
                "allowed_architectures"));
    }
    lookup_rw< config::strings_set_node >(
        _pimpl->props, "allowed_architectures").insert(arch);
    return *this;
}


/// Accumulates an additional allowed platform.
///
/// \param platform The platform.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::add_allowed_platform(const std::string& platform)
{
    if (!_pimpl->props.is_set("allowed_platforms")) {
        _pimpl->props.set< config::strings_set_node >(
            "allowed_platforms",
            get_defaults().lookup< config::strings_set_node >(
                "allowed_platforms"));
    }
    lookup_rw< config::strings_set_node >(
        _pimpl->props, "allowed_platforms").insert(platform);
    return *this;
}


/// Accumulates a single user-defined property.
///
/// \param key Name of the property to define.
/// \param value Value of the property.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::add_custom(const std::string& key,
                                     const std::string& value)
{
    _pimpl->props.set_string(F("custom.%s") % key, value);
    return *this;
}


/// Accumulates an additional required configuration variable.
///
/// \param var The name of the configuration variable.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::add_required_config(const std::string& var)
{
    if (!_pimpl->props.is_set("required_configs")) {
        _pimpl->props.set< config::strings_set_node >(
            "required_configs",
            get_defaults().lookup< config::strings_set_node >(
                "required_configs"));
    }
    lookup_rw< config::strings_set_node >(
        _pimpl->props, "required_configs").insert(var);
    return *this;
}


/// Accumulates an additional required file.
///
/// \param path The path to the file.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::add_required_file(const fs::path& path)
{
    if (!_pimpl->props.is_set("required_files")) {
        _pimpl->props.set< paths_set_node >(
            "required_files",
            get_defaults().lookup< paths_set_node >("required_files"));
    }
    lookup_rw< paths_set_node >(_pimpl->props, "required_files").insert(path);
    return *this;
}


/// Accumulates an additional required program.
///
/// \param path The path to the program.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::add_required_program(const fs::path& path)
{
    if (!_pimpl->props.is_set("required_programs")) {
        _pimpl->props.set< paths_set_node >(
            "required_programs",
            get_defaults().lookup< paths_set_node >("required_programs"));
    }
    lookup_rw< paths_set_node >(_pimpl->props,
                                "required_programs").insert(path);
    return *this;
}


/// Sets the architectures allowed by the test.
///
/// \param as Set of architectures.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_allowed_architectures(
    const model::strings_set& as)
{
    set< config::strings_set_node >(_pimpl->props, "allowed_architectures", as);
    return *this;
}


/// Sets the platforms allowed by the test.
///
/// \return ps Set of platforms.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_allowed_platforms(const model::strings_set& ps)
{
    set< config::strings_set_node >(_pimpl->props, "allowed_platforms", ps);
    return *this;
}


/// Sets the user-defined properties.
///
/// \param props The custom properties to set.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_custom(const model::properties_map& props)
{
    for (model::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter)
        _pimpl->props.set_string(F("custom.%s") % (*iter).first,
                                 (*iter).second);
    return *this;
}


/// Sets the description of the test.
///
/// \param description Textual description of the test.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_description(const std::string& description)
{
    set< config::string_node >(_pimpl->props, "description", description);
    return *this;
}


/// Sets execution environment name.
///
/// \param name Execution environment name.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_execenv(const std::string& name)
{
    set< config::string_node >(_pimpl->props, "execenv", name);
    return *this;
}


/// Sets execenv jail(8) parameters string to run the test with.
///
/// \param params String of jail parameters.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_execenv_jail_params(const std::string& params)
{
    set< config::string_node >(_pimpl->props, "execenv_jail_params", params);
    return *this;
}


/// Sets whether the test has a cleanup part or not.
///
/// \param cleanup True if the test has a cleanup part; false otherwise.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_has_cleanup(const bool cleanup)
{
    set< config::bool_node >(_pimpl->props, "has_cleanup", cleanup);
    return *this;
}


/// Sets whether the test is exclusive or not.
///
/// \param exclusive True if the test is exclusive; false otherwise.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_is_exclusive(const bool exclusive)
{
    set< config::bool_node >(_pimpl->props, "is_exclusive", exclusive);
    return *this;
}


/// Sets the list of configuration variables needed by the test.
///
/// \param vars Set of configuration variables.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_required_configs(const model::strings_set& vars)
{
    set< config::strings_set_node >(_pimpl->props, "required_configs", vars);
    return *this;
}


/// Sets the amount of free disk space required by the test.
///
/// \param bytes Number of bytes.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_required_disk_space(const units::bytes& bytes)
{
    set< bytes_node >(_pimpl->props, "required_disk_space", bytes);
    return *this;
}


/// Sets the list of files needed by the test.
///
/// \param files Set of paths.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_required_files(const model::paths_set& files)
{
    set< paths_set_node >(_pimpl->props, "required_files", files);
    return *this;
}


/// Sets the amount of memory required by the test.
///
/// \param bytes Number of bytes.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_required_memory(const units::bytes& bytes)
{
    set< bytes_node >(_pimpl->props, "required_memory", bytes);
    return *this;
}


/// Sets the list of programs needed by the test.
///
/// \param progs Set of paths.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_required_programs(const model::paths_set& progs)
{
    set< paths_set_node >(_pimpl->props, "required_programs", progs);
    return *this;
}


/// Sets the user required by the test.
///
/// \param user One of unprivileged, root or empty.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_required_user(const std::string& user)
{
    set< user_node >(_pimpl->props, "required_user", user);
    return *this;
}


/// Sets a metadata property by name from its textual representation.
///
/// \param key The property to set.
/// \param value The value to set the property to.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid or the key does not exist.
model::metadata_builder&
model::metadata_builder::set_string(const std::string& key,
                                    const std::string& value)
{
    try {
        _pimpl->props.set_string(key, value);
    } catch (const config::unknown_key_error& e) {
        throw model::format_error(F("Unknown metadata property %s") % key);
    } catch (const config::value_error& e) {
        throw model::format_error(
            F("Invalid value for metadata property %s: %s") % key % e.what());
    }
    return *this;
}


/// Sets the timeout of the test.
///
/// \param timeout The timeout to set.
///
/// \return A reference to this builder.
///
/// \throw model::error If the value is invalid.
model::metadata_builder&
model::metadata_builder::set_timeout(const datetime::delta& timeout)
{
    set< delta_node >(_pimpl->props, "timeout", timeout);
    return *this;
}


/// Creates a new metadata object.
///
/// \pre This has not yet been called.  We only support calling this function
/// once due to the way the internal tree works: we pass around references, not
/// deep copies, so if we allowed a second build, we'd encourage reusing the
/// same builder to construct different metadata objects, and this could have
/// unintended consequences.
///
/// \return The constructed metadata object.
model::metadata
model::metadata_builder::build(void) const
{
    PRE(!_pimpl->built);
    _pimpl->built = true;

    return metadata(_pimpl->props);
}
