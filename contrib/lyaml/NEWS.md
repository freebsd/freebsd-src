# lyaml NEWS - User visible changes

## Noteworthy changes in release 6.2.8 (2022-10-22) [stable]

### Bug fixes

  - `luke` no longer crashes in `std.normalize` require loops
    occasionally in Lua 5.4.

  - lyaml emitter no longer leaks at least six bytes for every
    map, sequence and scalar emitted.


## Noteworthy changes in release 6.2.7 (2020-11-27) [stable]

### Bug fixes

  - Don't skip YAML entries from mixed key Lua tables.


## Noteworthy changes in release 6.2.6 (2020-08-28) [stable]

### Bug fixes

  - `luke` really propagates `LDFLAGS` to module compilation
    commands.


## Noteworthy changes in release 6.2.5 (2020-04-15) [stable]

### Bug fixes

  - `luke` really propagates `YAML_BINDIR`, `YAML_DIR`,
    `YAML_INCDIR` and `YAML_LIBDIR` to checksymbol test in lukefile
    given the change to `external_dependencies` layout in 6.1.2.


## Noteworthy changes in release 6.2.4 (2019-07-20) [stable]

### Bug fixes

  - `luke` works with upgraded bootstrap luarocks version of
    `require`.


## Noteworthy changes in release 6.2.3 (2018-09-16) [stable]

### New Features

  - Initial support for Lua 5.4.


## Noteworthy changes in release 6.2.2 (2018-03-28) [stable]

### Bug fixes

  - Remove spurious dependency on `std.normalize` and `std._debug`
    libraries.


## Noteworthy changes in release 6.2.1 (2018-02-20) [stable]

### Bug fixes

  - `spec/spec_helper.lua` now looks in the correct objdir
    for object modules built by luke, instead of adding unused
    paths from old Autotools objdirs.  So now specl is properly
    running examples against the not yet installed lyaml objects.


## Noteworthy changes in release 6.2 (2017-11-26) [stable]

### Bug fixes

  - `luke` uses the correct spelling of LIBFLAG to match luarocks now.

  - `luke` no longer throws spurious `cp: file exists` errors.

  - `luke` works on luajit again.


## Noteworthy changes in release 6.1.3 (2017-05-29) [stable]

### Bug fixes

  - `luke` no longer bombs out with a nil concat error.


## Noteworthy changes in release 6.1.2 (2017-04-30) [stable]

### Bug fixes

  - `luke` now propagates `LUA_DIR`, `YAML_INCDIR` and `YAML_LIBDIR`
    correctly.


## Noteworthy changes in release 6.1.1 (2017-01-22) [stable]

### New Features

  - Builds and installs with `luke` instead of Autotools.


## Noteworthy changes in release 6.1 (2016-10-08) [stable]

### Bug fixes

  - `lyaml.load` now correctly reads implicit null scalars in a YAML
    document as an `lyaml.null` reference, identical to the "~"
    shorthand syntax, according to [the specification][nullspec].

    ```yaml
    empty:
    canonical: ~
    english: null
    ~: null key
    ```


## Noteworthy changes in release 6.0 (2015-07-27) [stable]

### New Features

  - `lyaml.load` now correctly reads a !!bool tagged scalar from a
    YAML document, or an implicit bool value, according to
    [the specification][boolspec].

    ```yaml
    %TAG ! tag:yaml.org,2002:
    ---
    truthy:
      - !bool Y
      - !bool y
      - !bool True
      - !bool "on"
    falsey:
      - !bool n
      - !bool OFF
      - !bool garbage
    ```

  - `lyaml.load` now correctly reads a !!float tagged scalar from a
    YAML document, or an implicit float value, according to
    [the specification][floatspec].

  - `lyaml.load` now correctly reads a !!int tagged scalar from a
    YAML document, or an implicit integer value, according to
    [the specification][intspec].

  - `lyaml.load` now supports the !!merge key type according to
    [the specification][mergespec].

    ```yaml
    - &MERGE { x: 1, y: 2 }
    - &OVERRIDE { x: 0, z: 1 }
    -
      << : [&MERGE, &OVERRIDE]
      z: 3
    ```

    The anchored tables remain in the document too, so this results in
    the following Lua table:

    ```lua
    {                           -- START_STREAM
      {                         -- START_DOCUMENT
        { x = 1, y = 2 },       -- MERGE
        { x = 0, z = 1 },       -- OVERRIDE
        { x = 1, y = 2, z = 3}, -- <<<
      }                         -- END_DOCUMENT
    }                           -- END_STREAM
    ```

### Bug fixes

  - Multi-line strings were previously being dumped using single quotes
    which caused the dumped YAML to break.

    For example, { foo = "a\nmultiline\nstring" } would get dumped as:

    ```yaml
    foo: 'a

    multiline

    string'
    ```

    Note the extra line-breaks in between each line. This also causes
    YAML parsing to fail (since the blank lines didn't have the expected
    indentation).

    This patch fixes the dump to use the YAML literal syntax for any
    multi-line strings so the same example gets dumped as:

    ```yaml
    foo: |-
      a
      multiline
      string
    ```

  - `lyaml.load` now correctly reads the !!null tag in a YAML
    document as an `lyaml.null` reference, identical to the "~"
    shorthand syntax, according to [the specification][nullspec].

### Incompatible Changes

  - `lyaml.load` now takes a table of options as an optional second
    argument, not a simple boolean to determine whether all documents
    should be returned from the stream.  For now, a `true` second
    argument will be converted to the modern equivalent:

    ```lua
    lyaml.load (document, { all = true })
    ```

  - `lyaml.dump` now takes a table of options as an optional second
    argument, not an initial table of anchors.  For now, a second
    argument without any new API keys will be converted to the modern
    equivalent:

    ```lua
    lyaml.dump (t, { anchors = arg2 })
    ```

[boolspec]:  http://yaml.org/type/bool.html
[floatspec]: http://yaml.org/type/float.html
[intspec]:   http://yaml.org/type/int.html
[mergespec]: http://yaml.org/type/merge.html
[nullspec]:  http://yaml.org/type/null.html


## Noteworthy changes in release 5.1.4 (2015-01-01) [stable]

  - This release is functionally identical to the last.


## Noteworthy changes in release 5.1.3 (2015-01-01) [stable]

  - This release is functionally identical to the last.


## Noteworthy changes in release 5.1.2 (2014-12-27) [stable]

### Bugs Fixed

  - No more spurious .travis.yml is out of date warnings during
    `luarocks install lyaml`.


## Noteworthy changes in release 5.1.1 (2014-12-19) [stable]

### Bugs Fixed

  - When using `sudo make install` instead of LuaRocks, `lyaml.so`
    is now correctly installed to `$luaexecdir`.


## Noteworthy changes in release 5.1.0 (2014-12-17) [stable]

### New Features

  - Lua 5.3.0 compatibility.


## Noteworthy changes in release 5 (2014-09-25) [beta]

### Build

  - Significantly reduced pointer mismatch warnings from modern GNU
    compilers.

### New Features

  - `lyaml.dump` now takes a second argument containing a table of
    potential anchor values in `ANCHOR_NAME = { "match", "elements" }`
    pairs format.  The first time any are matched in the table being
    dumped, they are preceded by `&ANCHOR_NAME` in the output YAML
    document; subsequent matches are not written out in full, but
    shortened to the appropriate `*ANCHOR_NAME` alias.

### Bugs Fixed

  - `yaml.emitter` no longer emits numbers in SINGLE_QUOTE style by
    default.

  - `yaml.emitter ().emit` returns error strings correctly for invalid
    STREAM_START encoding, and MAPPING_START, SEQUENCE_START & SCALAR
    style fields.


## Noteworthy changes in release 4 (2013-09-11) [beta]

### New Features

  - New yaml.emitter API returns an object with an emit method for
    adding events using yaml_*_event_initialize() calls.

  - New yaml.parser API returns a Lua iterator that fetches the next
    event using yaml_parser_parse().

  - New yaml.scanner API returns a Lua iterator that fetches the next
    token using yaml_parser_scan().

  - Beginnings of Specl specs, starting with a reasonably comprehensive
    specifications for the new APIs above.

  - C implementation of lyaml.dump has moved to Lua implementation as
    yaml.dump.

  - C implementation of lyaml.load has moved to Lua implementation as
    yaml.load.

  - The new Lua implementation of lyaml.load () handles multi-document
    streams, and returns a table of documents when the new second
    argument is `true`.


## Noteworthy changes in release 3 (2013-04-27) [beta]

  - This release is functionally identical to the last.

### New Features

  - lyaml builds are now made against Lua 5.1, Lua 5.2 and luajit 2.0.0
    automatically, with every commit.

  - move to a cleaner, automated release system.


## Noteworthy changes in release 2 (2013-03-18) [beta]

  - This release is functionally identical to the last.

  - Use correct MIT license attribution, relicensing build files to match
    Andrew Danforth''s MIT licensed lyaml.c too.


## Noteworthy changes in release 1 (2013-03-17) [beta]

### New Features

  - A binding for libYAML, by Andrew Danforth:  Updated for Lua 5.1 and
    5.2, and packaged as a luarock.

  - I spun this out of Specl (http://github.com/gvvaughan/specl) so that
    other projects may use it, and to simplify the Specl build.

### Known Issues

  - There's not really any documentation, sorry.  Contributions welcome!
