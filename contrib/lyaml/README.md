LYAML
=====

Copyright (C) 2013-2022 Gary V. Vaughan

[![License](https://img.shields.io/:license-mit-blue.svg)](https://mit-license.org)
[![workflow status](https://github.com/gvvaughan/lyaml/actions/workflows/spec.yml/badge.svg?branch=release-v6.2.8)](https://github.com/gvvaughan/lyaml/actions)
[![codecov.io](https://codecov.io/github/gvvaughan/lyaml/coverage.svg?branch=release-v6.2.8)](https://codecov.io/github/gvvaughan/lyaml?branch=release-v6.2.8)

[LibYAML] binding for [Lua], with a fast C implementation
for converting between [%YAML 1.1][yaml11] and [Lua] tables,
and a low-level [YAML] event parser for implementing more
intricate [YAML] document loading.

Usage
-----

### High Level API

These functions quickly convert back and forth between Lua tables
and [%YAML 1.1][yaml11] format strings.

```lua
local lyaml   = require "lyaml"
local t       = lyaml.load (YAML-STRING, [OPTS-TABLE])
local yamlstr = lyaml.dump (LUA-TABLE, [OPTS-TABLE])
local null    = lyaml.null ()
```

#### `lyaml.load`

`lyaml.load` accepts a YAML string for parsing. If the YAML string contains
multiple documents, only the first document will be returned by default. To
return multiple documents as a table, set `all = true` in the second
argument OPTS-TABLE.

```lua
lyaml.load("foo: bar")
--> { foo = "bar" }

lyaml.load("foo: bar", { all = true })
--> { { foo = "bar" } }

multi_doc_yaml = [[
---
one
...
---
two
...
]]

lyaml.load(multi_doc_yaml)
--> "one"

lyaml.load(multi_doc_yaml, { all = true })
--> { "one", "two" }
```

You can supply an alternative function for converting implicit plain
scalar values in the `implicit_scalar` field of the OPTS-TABLE argument;
otherwise a default is composed from the functions in the `lyaml.implicit`
module.

You can also supply an alternative table for coverting explicitly tagged
scalar values in the `explicit_scalar` field of the OPTS-TABLE argument;
otherwise all supported tags are parsed by default using the functions
from the `lyaml.explicit` module.

#### `lyaml.dump`

`lyaml.dump` accepts a table of values to dump. Each value in the table
represents a single YAML document. To dump a table of lua values this means
the table must be wrapped in another table (the outer table represents the
YAML documents, the inner table is the single document table to dump).

```lua
lyaml.dump({ { foo = "bar" } })
--> ---
--> foo: bar
--> ...

lyaml.dump({ "one", "two" })
--> --- one
--> ...
--> --- two
--> ...
```

If you need to round-trip load a dumped document, and you used a custom
function for converting implicit scalars, then you should pass that same
function in the `implicit_scalar` field of the OPTS-TABLE argument to
`lyaml.dump` so that it can quote strings that might otherwise be
implicitly converted on reload.

#### Nil Values

[Lua] tables treat `nil` valued keys as if they were not there,
where [YAML] explicitly supports `null` values (and keys!).  Lyaml
will retain [YAML] `null` values as `lyaml.null ()` by default,
though it is straight forward to wrap the low level APIs to use `nil`,
subject to the usual caveats of how nil values work in [Lua] tables.


### Low Level APIs

```lua
local emitter = require ("yaml").emitter ()

emitter.emit {type = "STREAM_START"}
for _, event in ipairs (event_list) do
  emitter.emit (event)
end
str = emitter.emit {type = "STREAM_END"}
```

The `yaml.emitter` function returns an emitter object that has a
single emit function, which you call with event tables, the last
`STREAM_END` event returns a string formatted as a [YAML 1.1][yaml11]
document.

```lua
local iter = require ("yaml").scanner (YAML-STRING)

for token_table in iter () do
  -- process token table
end
```

Each time the iterator returned by `scanner` is called, it returns
a table describing the next token of YAML-STRING.  See LibYAML's
[yaml.h] for details of the contents and semantics of the various
tokens produced by `yaml_parser_scan`, the underlying call made by
the iterator.

[LibYAML] implements a fast parser in C using `yaml_parser_scan`, which
is also bound to lyaml, and easier to use than the token API above:

```lua
local iter = require ("yaml").parser (YAML-STRING)

for event_table in iter () do
  -- process event table
end
```

Each time the iterator returned by `parser` is called, it returns
a table describing the next event from the "Parse" process of the
"Parse, Compose, Construct" processing model described in the
[YAML 1.1][yaml11] specification using [LibYAML].

Implementing the remaining "Compose" and "Construct" processes in
[Lua] is left as an exercise for the reader -- though, unlike the
high-level API, `lyaml.parser` exposes all details of the input
stream events, such as line and column numbers.


Installation
------------

There's no need to download an [lyaml] release, or clone the git repo,
unless you want to modify the code.  If you use [LuaRocks], you can
use it to install the latest release from its repository:

    luarocks --server=http://rocks.moonscript.org install lyaml

Or from the rockspec in a release tarball:

    luarocks make lyaml-?-1.rockspec

To install current git master from [GitHub][lyaml] (for testing):

    luarocks install http://raw.github.com/gvvaughan/lyaml/master/lyaml-git-1.rockspec

To install without [LuaRocks], clone the sources from the
[repository][lyaml], and then run the following commands:

```sh
cd lyaml
build-aux/luke LYAML_DIR=LIBYAML-INSTALL-PREFIX
sudo build-aux/luke PREFIX=LYAML-INSTALL-PREFIX install
specl -v1freport spec/*_spec.yaml
```

The dependencies are listed in the dependencies entry of the file
[rockspec][L15].


Bug reports and code contributions
----------------------------------

This library is maintained by its users.

Please make bug reports and suggestions as [GitHub Issues][issues].
Pull requests are especially appreciated.

But first, please check that your issue has not already been reported by
someone else, and that it is not already fixed by [master][lyaml] in
preparation for the next release (see  Installation section above for how
to temporarily install master with [LuaRocks][]).

There is no strict coding style, but please bear in mind the following
points when proposing changes:

0. Follow existing code. There are a lot of useful patterns and avoided
   traps there.

1. 3-character indentation using SPACES in Lua sources: It makes rogue
   TABs easier to see, and lines up nicely with 'if' and 'end' keywords.

2. Simple strings are easiest to type using single-quote delimiters,
   saving double-quotes for where a string contains apostrophes.

3. Save horizontal space by only using SPACEs where the parser requires
   them.

4. Use vertical space to separate out compound statements to help the
   coverage reports discover untested lines.

5. Prefer explicit string function calls over object methods, to mitigate
   issues with monkey-patching in caller environment.


[issues]:   http://github.com/gvvaughas/lyaml/issues
[libyaml]:  http://pyyaml.org/wiki/LibYAML
[lua]:      http://www.lua.org
[luarocks]: http://www.luarocks.org
[lyaml]:    http://github.com/gvvaughan/lyaml
[L15]:      http://github.com/gvvaughan/lyaml/blob/master/lyaml-git-1.rockspec#L15
[yaml.h]:   http://pyyaml.org/browser/libyaml/branches/stable/include/yaml.h
[yaml]:     http://yaml.org
[yaml11]:   http://yaml.org/spec/1.1/
