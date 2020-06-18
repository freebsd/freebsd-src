This is flex, the fast lexical analyzer generator.

flex is a tool for generating scanners: programs which recognize
lexical patterns in text.

The flex codebase is kept in
[Git on GitHub.](https://github.com/westes/flex)

Use GitHub's [issues](https://github.com/westes/flex/issues) and
[pull request](https://github.com/westes/flex) features to file bugs
and submit patches.

There are several mailing lists available as well:

* flex-announce@lists.sourceforge.net - where posts will be made
  announcing new releases of flex.
* flex-help@lists.sourceforge.net - where you can post questions about
  using flex
* flex-devel@lists.sourceforge.net - where you can discuss development
  of flex itself

Find information on subscribing to the mailing lists at:

http://sourceforge.net/mail/?group_id=97492

The flex distribution contains the following files which may be of
interest:

* README - This file.
* NEWS - current version number and list of user-visible changes.
* INSTALL - basic installation information.
* ABOUT-NLS - description of internationalization support in flex.
* COPYING - flex's copyright and license.
* doc/ - user documentation.
* examples/ - containing examples of some possible flex scanners and a
	      few other things. See the file examples/README for more
              details.
* tests/ - regression tests. See TESTS/README for details.
* po/ - internationalization support files.

You need the following tools to build flex from the maintainer's
repository:

* compiler suite - flex is built with gcc
* bash, or a good Bourne-style shell
* m4 - m4 -p needs to work; GNU m4 and a few others are suitable
* GNU bison;  to generate parse.c from parse.y
* autoconf; for handling the build system
* automake; for Makefile generation
* gettext; for i18n support
* help2man; to generate the flex man page
* tar, gzip, lzip, etc.; for packaging of the source distribution
* GNU texinfo; to build and test the flex manual. Note that if you want
  to build the dvi/ps/pdf versions of the documentation you will need
  texi2dvi and related programs, along with a sufficiently powerful
  implementation of TeX to process them. See your operating system
  documentation for how to achieve this. The printable versions of the
  manual are not built unless specifically requested, but the targets
  are included by automake.
* GNU indent; for indenting the flex source the way we want it done

In cases where the versions of the above tools matter, the file
configure.ac will specify the minimum required versions.

Once you have all the necessary tools installed, life becomes
simple. To prepare the flex tree for building, run the script:

```bash
./autogen.sh
```

in the top level of the flex source tree.

This script calls the various tools needed to get flex ready for the
GNU-style configure script to be able to work.

From this point on, building flex follows the usual  routine:

```bash
configure && make && make install
```

This file is part of flex.

This code is derived from software contributed to Berkeley by
Vern Paxson.

The United States Government has rights in this work pursuant
to contract no. DE-AC03-76SF00098 between the United States
Department of Energy and the University of California.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

Neither the name of the University nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.
