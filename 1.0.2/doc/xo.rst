
.. index:: --libxo, xo

The "xo" Utility
================

The `xo` utility allows command line access to the functionality of
the libxo library.  Using `xo`, shell scripts can emit XML, JSON, and
HTML using the same commands that emit text output.

The style of output can be selected using a specific option: "-X" for
XML, "-J" for JSON, "-H" for HTML, or "-T" for TEXT, which is the
default.  The "--style <style>" option can also be used.  The standard
set of "--libxo" options are available (see :ref:`options`), as well
as the `LIBXO_OPTIONS`_ environment variable.

.. _`LIBXO_OPTIONS`: :ref:`libxo-options`

The `xo` utility accepts a format string suitable for `xo_emit` and
a set of zero or more arguments used to supply data for that string::

    xo "The {k:name} weighs {:weight/%d} pounds.\n" fish 6

  TEXT:
    The fish weighs 6 pounds.
  XML:
    <name>fish</name>
    <weight>6</weight>
  JSON:
    "name": "fish",
    "weight": 6
  HTML:
    <div class="line">
      <div class="text">The </div>
      <div class="data" data-tag="name">fish</div>
      <div class="text"> weighs </div>
      <div class="data" data-tag="weight">6</div>
      <div class="text"> pounds.</div>
    </div>

The `--wrap $path` option can be used to wrap emitted content in a
specific hierarchy.  The path is a set of hierarchical names separated
by the '/' character::

    xo --wrap top/a/b/c '{:tag}' value

  XML:
    <top>
      <a>
        <b>
          <c>
            <tag>value</tag>
          </c>
        </b>
      </a>
    </top>
  JSON:
    "top": {
      "a": {
        "b": {
          "c": {
            "tag": "value"
          }
        }
      }
    }

The `--open $path` and `--close $path` can be used to emit
hierarchical information without the matching close and open
tag.  This allows a shell script to emit open tags, data, and
then close tags.  The `--depth` option may be used to set the
depth for indentation.  The `--leading-xpath` may be used to
prepend data to the XPath values used for HTML output style::

  EXAMPLE;
    #!/bin/sh
    xo --open top/data
    xo --depth 2 '{:tag}' value
    xo --close top/data
  XML:
    <top>
      <data>
        <tag>value</tag>
      </data>
    </top>
  JSON:
    "top": {
      "data": {
        "tag": "value"
      }
    }

When making partial lines of output (where the format string does not
include a newline), use the `--continuation` option to let secondary
invocations know they are adding data to an existing line.

When emitting a series of objects, use the `--not-first` option to
ensure that any details from the previous object (e.g. commas in JSON)
are handled correctly.

Use the `--top-wrap` option to ensure any top-level object details are
handled correctly, e.g. wrap the entire output in a top-level set of
braces for JSON output.

  EXAMPLE;
    #!/bin/sh
    xo --top-wrap --open top/data
    xo --depth 2 'First {:tag} ' value1
    xo --depth 2 --continuation 'and then {:tag}\n' value2
    xo --top-wrap --close top/data
  TEXT:
    First value1 and then value2
  HTML:
    <div class="line">
      <div class="text">First </div>
      <div class="data" data-tag="tag">value1</div>
      <div class="text"> </div>
      <div class="text">and then </div>
      <div class="data" data-tag="tag">value2</div>
    </div>
  XML:
    <top>
      <data>
        <tag>value1</tag>
        <tag>value2</tag>
      </data>
    </top>
  JSON:
    {
      "top": {
        "data": {
        "tag": "value1",
        "tag": "value2"
        }
      }
    } 

Lists and Instances
-------------------

A "*list*" is set of one or more instances that appear under the same
parent.  The instances contain details about a specific object.  One
can think of instances as objects or records.  A call is needed to
open and close the list, while a distinct call is needed to open and
close each instance of the list.

Use the `--open-list` and `--open-instances` to open lists and
instances.  Use the `--close-list` and `--close-instances` to close
them.  Each of these options take a `name` parameter, providing the
name of the list and instance.

In the following example, a list named "machine" is created with three
instances:

    opts="--json"
    xo $opts --open-list machine
    NF=
    for name in red green blue; do
        xo $opts --depth 1 $NF --open-instance machine
        xo $opts --depth 2 "Machine {k:name} has {:memory}\n" $name 55
        xo $opts --depth 1 --close-instance machine
        NF=--not-first
    done
    xo $opts $NF --close-list machine

The normal `libxo` functions use a state machine to help these
transitions, but since each `xo` command is invoked independent of the
previous calls, the state must be passed in explicitly via these
command line options.

Command Line Options
--------------------

::

  Usage: xo [options] format [fields]
    --close <path>        Close tags for the given path
    --close-instance <name> Close an open instance name
    --close-list <name>   Close an open list name
    --continuation OR -C  Output belongs on same line as previous output
    --depth <num>         Set the depth for pretty printing
    --help                Display this help text
    --html OR -H          Generate HTML output
    --json OR -J          Generate JSON output
    --leading-xpath <path> Add a prefix to generated XPaths (HTML)
    --not-first           Indicate this object is not the first (JSON)
    --open <path>         Open tags for the given path
    --open-instance <name> Open an instance given by name
    --open-list <name>   Open a list given by name
    --option <opts> -or -O <opts>  Give formatting options
    --pretty OR -p        Make 'pretty' output (add indent, newlines)
    --style <style>       Generate given style (xml, json, text, html)
    --text OR -T          Generate text output (the default style)
    --top-wrap            Generate a top-level object wrapper (JSON)
    --version             Display version information
    --warn OR -W          Display warnings in text on stderr
    --warn-xml            Display warnings in xml on stdout
    --wrap <path>         Wrap output in a set of containers
    --xml OR -X           Generate XML output
    --xpath               Add XPath data to HTML output);

Example
-------

::

  % xo 'The {:product} is {:status}\n' stereo "in route"
  The stereo is in route
  % ./xo/xo -p -X 'The {:product} is {:status}\n' stereo "in route"
  <product>stereo</product>
  <status>in route</status>
