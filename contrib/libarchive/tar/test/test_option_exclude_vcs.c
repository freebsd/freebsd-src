/*-
 * Copyright (c) 2019 Martin Matuska
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

DEFINE_TEST(test_option_exclude_vcs)
{
	assertUmask(0);
	assertMakeDir("in", 0755);
	assertChdir("in");
	assertMakeFile("file", 0644, "");
	assertMakeDir("dir", 0755);
	assertMakeDir("CVS", 0755);
	assertMakeFile("CVS/fileattr", 0644, "");
	assertMakeFile(".cvsignore", 0644, "");
	assertMakeDir("RCS", 0755);
	assertMakeFile("RCS/somefile", 0655, "");
	assertMakeDir("SCCS", 0755);
	assertMakeFile("SCCS/somefile", 0655, "");
	assertMakeDir(".svn", 0755);
	assertMakeFile(".svn/format", 0655, "");
	assertMakeDir(".git", 0755);
	assertMakeFile(".git/config", 0655, "");
	assertMakeFile(".gitignore", 0644, "");
	assertMakeFile(".gitattributes", 0644, "");
	assertMakeFile(".gitmodules", 0644, "");
	assertMakeDir(".arch-ids", 0755);
	assertMakeFile(".arch-ids/somefile", 0644, "");
	assertMakeDir("{arch}", 0755);
	assertMakeFile("{arch}/somefile", 0644, "");
	assertMakeFile("=RELEASE-ID", 0644, "");
	assertMakeFile("=meta-update", 0644, "");
	assertMakeFile("=update", 0644, "");
	assertMakeDir(".bzr", 0755);
	assertMakeDir(".bzr/checkout", 0755);
	assertMakeFile(".bzrignore", 0644, "");
	assertMakeFile(".bzrtags", 0644, "");
	assertMakeDir(".hg", 0755);
	assertMakeFile(".hg/dirstate", 0644, "");
	assertMakeFile(".hgignore", 0644, "");
	assertMakeFile(".hgtags", 0644, "");
	assertMakeDir("_darcs", 0755);
	assertMakeFile("_darcs/format", 0644, "");
	assertChdir("..");
	
	assertEqualInt(0, systemf("%s -c -C in -f included.tar .", testprog));
	assertEqualInt(0,
	    systemf("%s -c --exclude-vcs -C in -f excluded.tar .", testprog));

	/* No flags, archive with vcs files */
	assertMakeDir("vcs-noexclude", 0755);
	assertEqualInt(0, systemf("%s -x -C vcs-noexclude -f included.tar",
	    testprog));
	assertChdir("vcs-noexclude");
	assertFileExists("file");
	assertIsDir("dir", 0755);
	assertIsDir("CVS", 0755);
	assertFileExists("CVS/fileattr");
	assertFileExists(".cvsignore");
	assertIsDir("RCS", 0755);
	assertFileExists("RCS/somefile");
	assertIsDir("SCCS", 0755);
	assertFileExists("SCCS/somefile");
	assertIsDir(".svn", 0755);
	assertFileExists(".svn/format");
	assertIsDir(".git", 0755);
	assertFileExists(".git/config");
	assertFileExists(".gitignore");
	assertFileExists(".gitattributes");
	assertFileExists(".gitmodules");
	assertIsDir(".arch-ids", 0755);
	assertFileExists(".arch-ids/somefile");
	assertIsDir("{arch}", 0755);
	assertFileExists("{arch}/somefile");
	assertFileExists("=RELEASE-ID");
	assertFileExists("=meta-update");
	assertFileExists("=update");
	assertIsDir(".bzr", 0755);
	assertIsDir(".bzr/checkout", 0755);
	assertFileExists(".bzrignore");
	assertFileExists(".bzrtags");
	assertIsDir(".hg", 0755);
	assertFileExists(".hg/dirstate");
	assertFileExists(".hgignore");
	assertFileExists(".hgtags");
	assertIsDir("_darcs", 0755);
	assertFileExists("_darcs/format");
	assertChdir("..");

	/* --exclude-vcs, archive with vcs files */
	assertMakeDir("vcs-exclude", 0755);
	assertEqualInt(0,
	    systemf("%s -x --exclude-vcs -C vcs-exclude -f included.tar", testprog));
	assertChdir("vcs-exclude");
	assertFileExists("file");
	assertIsDir("dir", 0755);
	assertFileNotExists("CVS");
	assertFileNotExists("CVS/fileattr");
	assertFileNotExists(".cvsignore");
	assertFileNotExists("RCS");
	assertFileNotExists("RCS/somefile");
	assertFileNotExists("SCCS");
	assertFileNotExists("SCCS/somefile");
	assertFileNotExists(".svn");
	assertFileNotExists(".svn/format");
	assertFileNotExists(".git");
	assertFileNotExists(".git/config");
	assertFileNotExists(".gitignore");
	assertFileNotExists(".gitattributes");
	assertFileNotExists(".gitmodules");
	assertFileNotExists(".arch-ids");
	assertFileNotExists(".arch-ids/somefile");
	assertFileNotExists("{arch}");
	assertFileNotExists("{arch}/somefile");
	assertFileNotExists("=RELEASE-ID");
	assertFileNotExists("=meta-update");
	assertFileNotExists("=update");
	assertFileNotExists(".bzr");
	assertFileNotExists(".bzr/checkout");
	assertFileNotExists(".bzrignore");
	assertFileNotExists(".bzrtags");
	assertFileNotExists(".hg");
	assertFileNotExists(".hg/dirstate");
	assertFileNotExists(".hgignore");
	assertFileNotExists(".hgtags");
	assertFileNotExists("_darcs");
	assertFileNotExists("_darcs/format");
	assertChdir("..");

	/* --exclude-vcs, archive without vcs files */
	assertMakeDir("novcs-exclude", 0755);
	assertEqualInt(0,
	    systemf("%s -x --exclude-vcs -C novcs-exclude -f excluded.tar",
	    testprog));
	assertChdir("novcs-exclude");
	assertFileExists("file");
	assertIsDir("dir", 0755);
	assertFileNotExists("CVS");
	assertFileNotExists("CVS/fileattr");
	assertFileNotExists(".cvsignore");
	assertFileNotExists("RCS");
	assertFileNotExists("RCS/somefile");
	assertFileNotExists("SCCS");
	assertFileNotExists("SCCS/somefile");
	assertFileNotExists(".svn");
	assertFileNotExists(".svn/format");
	assertFileNotExists(".git");
	assertFileNotExists(".git/config");
	assertFileNotExists(".gitignore");
	assertFileNotExists(".gitattributes");
	assertFileNotExists(".gitmodules");
	assertFileNotExists(".arch-ids");
	assertFileNotExists(".arch-ids/somefile");
	assertFileNotExists("{arch}");
	assertFileNotExists("{arch}/somefile");
	assertFileNotExists("=RELEASE-ID");
	assertFileNotExists("=meta-update");
	assertFileNotExists("=update");
	assertFileNotExists(".bzr");
	assertFileNotExists(".bzr/checkout");
	assertFileNotExists(".bzrignore");
	assertFileNotExists(".bzrtags");
	assertFileNotExists(".hg");
	assertFileNotExists(".hg/dirstate");
	assertFileNotExists(".hgignore");
	assertFileNotExists(".hgtags");
	assertFileNotExists("_darcs");
	assertFileNotExists("_darcs/format");
	assertChdir("..");

	/* No flags, archive without vcs files */
	assertMakeDir("novcs-noexclude", 0755);
	assertEqualInt(0,
	    systemf("%s -x -C novcs-noexclude -f excluded.tar", testprog));
	assertChdir("novcs-noexclude");
	assertFileExists("file");
	assertIsDir("dir", 0755);
	assertFileNotExists("CVS");
	assertFileNotExists("CVS/fileattr");
	assertFileNotExists(".cvsignore");
	assertFileNotExists("RCS");
	assertFileNotExists("RCS/somefile");
	assertFileNotExists("SCCS");
	assertFileNotExists("SCCS/somefile");
	assertFileNotExists(".svn");
	assertFileNotExists(".svn/format");
	assertFileNotExists(".git");
	assertFileNotExists(".git/config");
	assertFileNotExists(".gitignore");
	assertFileNotExists(".gitattributes");
	assertFileNotExists(".gitmodules");
	assertFileNotExists(".arch-ids");
	assertFileNotExists(".arch-ids/somefile");
	assertFileNotExists("{arch}");
	assertFileNotExists("{arch}/somefile");
	assertFileNotExists("=RELEASE-ID");
	assertFileNotExists("=meta-update");
	assertFileNotExists("=update");
	assertFileNotExists(".bzr");
	assertFileNotExists(".bzr/checkout");
	assertFileNotExists(".bzrignore");
	assertFileNotExists(".bzrtags");
	assertFileNotExists(".hg");
	assertFileNotExists(".hg/dirstate");
	assertFileNotExists(".hgignore");
	assertFileNotExists(".hgtags");
	assertFileNotExists("_darcs");
	assertFileNotExists("_darcs/format");
}
