'''
  Copyright 2011 by the Massachusetts
  Institute of Technology.  All Rights Reserved.

  Export of this software from the United States of America may
  require a specific license from the United States Government.
  It is the responsibility of any person or organization contemplating
  export to obtain such a license before exporting.

  WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
  distribute this software and its documentation for any purpose and
  without fee is hereby granted, provided that the above copyright
  notice appear in all copies and that both that copyright notice and
  this permission notice appear in supporting documentation, and that
  the name of M.I.T. not be used in advertising or publicity pertaining
  to distribution of the software without specific, written prior
  permission.  Furthermore if you modify this software you must label
  your software as modified software and not distribute it in such a
  fashion that it might be confused with the original M.I.T. software.
  M.I.T. makes no representations about the suitability of
  this software for any purpose.  It is provided "as is" without express
  or implied warranty.
'''
import sys
import os
import re
from optparse import OptionParser


from doxybuilder_types import *
from doxybuilder_funcs import *


def processOptions():
    usage = "\n\t\t%prog -t type -i in_dir -o out_dir"
    description = "Description:\n\tProcess doxygen output for c-types and/or functions"
    parser = OptionParser(usage=usage, description=description)

    parser.add_option("-t", "--type",  type="string", dest="action_type", help="process typedef and/or function. Possible choices: typedef, func, all. Default: all.", default="all")
    parser.add_option("-i", "--in",  type="string", dest="in_dir", help="input directory")
    parser.add_option("-o", "--out",  type="string", dest= "out_dir", help="output directory. Note:  The subdirectory ./types will be created for typedef")

    (options, args) = parser.parse_args()
    action = options.action_type
    in_dir = options.in_dir
    out_dir = options.out_dir


    if in_dir is None or out_dir is None:
       parser.error("Input and output directories are required")

    if action == "all" or action == "typedef":
        builder = DoxyBuilderTypes(in_dir, out_dir)
        builder.run_all()

    if action == "all" or action == "func" or action == "function":
        builder = DoxyBuilderFuncs(in_dir, out_dir)
        builder.run_all()


if __name__ == '__main__':
    parser = processOptions()


