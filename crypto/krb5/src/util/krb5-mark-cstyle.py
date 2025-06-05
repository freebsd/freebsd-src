from optparse import OptionParser
import os
import re
import sys

styles = {
    "bsd":
        "/* -*- mode: c; c-file-style: \"bsd\"; indent-tabs-mode: t -*- */\n",
    "krb5":
        "/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */\n"
    }

def dofile(fname, style):
    changed = False
    newname = fname + ".new"
    infile = open(fname)
    outfile = open(newname, "w")
    first = next(infile)
    if (first != style):
        changed = True
        outfile.write(style)
        if re.match(r"""\s*/\*\s*-\*-.*-\*-\s*\*/""", first):
            # Replace first line if it was already a local variables line.
            pass
        else:
            outfile.write(first)

        # Simply copy remaining lines.
        for line in infile:
            outfile.write(line)

    infile.close()
    outfile.close()

    if changed:
        os.rename(newname, fname)
    else:
        os.remove(newname)

parser = OptionParser()
parser.add_option("--cstyle", action="store", dest="style",
                  choices=("bsd", "krb5"), default="krb5")
(options, args) = parser.parse_args()

for fname in args:
    print(fname)
    dofile(fname, styles[options.style])
