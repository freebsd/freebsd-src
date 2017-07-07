/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/t_path.c - Path manipulation tests */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <k5-platform.h>

/* For testing purposes, use a different symbol for Windows path semantics. */
#ifdef _WIN32
#define WINDOWS_PATHS
#endif

/*
 * The ultimate arbiter of these tests is the dirname, basename, and isabs
 * methods of the Python posixpath and ntpath modules.
 */

struct {
    const char *path;
    const char *posix_dirname;
    const char *posix_basename;
    const char *win_dirname;
    const char *win_basename;
} split_tests[] = {
    { "",          "",      "",          "",       ""  },
    { "a/b/c",     "a/b",   "c",         "a/b",    "c" },
    { "a/b/",      "a/b",   "",          "a/b",    ""  },
    { "a\\b\\c",   "",      "a\\b\\c",   "a\\b",   "c" },
    { "a\\b\\",    "",      "a\\b\\",    "a\\b",   ""  },
    { "a/b\\c",    "a",     "b\\c",      "a/b",    "c" },
    { "a//b",      "a",     "b",         "a",      "b" },
    { "a/\\/b",    "a/\\",  "b",         "a",      "b" },
    { "a//b/c",    "a//b",  "c",         "a//b",   "c" },

    { "/",         "/",     "",          "/",      ""  },
    { "\\",        "",      "\\",        "\\",     ""  },
    { "/a/b/c",    "/a/b",  "c",         "/a/b",   "c" },
    { "\\a/b/c",   "\\a/b", "c",         "\\a/b",  "c" },
    { "/a",        "/",     "a",         "/",      "a" },
    { "//a",       "//",    "a",         "//",     "a" },
    { "\\//\\a",   "\\",    "\\a",       "\\//\\", "a" },

    { "/:",        "/",     ":",         "/:",     ""  },
    { "c:\\",      "",      "c:\\",      "c:\\",   ""  },
    { "c:/",       "c:",    "",          "c:/",    ""  },
    { "c:/\\a",    "c:",    "\\a",       "c:/\\",  "a" },
    { "c:a",       "",      "c:a",       "c:",     "a" },
};

struct {
    const char *path1;
    const char *path2;
    const char *posix_result;
    const char *win_result;
} join_tests[] = {
    { "",     "",     "",         ""      },
    { "",     "a",    "a",        "a"     },
    { "",     "/a",   "/a",       "/a"    },
    { "",     "c:",   "c:",       "c:"    },

    { "a",    "",     "a/",       "a\\"   },
    { "a/",   "",     "a/",       "a/"    },
    { "a\\",  "",     "a\\/",     "a\\"   },
    { "a/\\", "",     "a/\\/",    "a/\\"  },

    { "a",    "b",    "a/b",      "a\\b"  },
    { "a",    "/b",   "/b",       "/b"    },
    { "a",    "c:",   "a/c:",     "a\\c:" },
    { "a",    "c:/",  "a/c:/",    "c:/"   },
    { "a",    "c:/a", "a/c:/a",   "c:/a"  },
    { "a",    "/:",   "/:",       "a/:"   },
    { "a/",   "b",    "a/b",      "a/b"   },
    { "a/",   "",     "a/",       "a/"    },
    { "a\\",  "b",    "a\\/b",    "a\\b"  },

    { "a//",  "b",    "a//b",     "a//b"  },
    { "a/\\", "b",    "a/\\/b",   "a/\\b" },
};

struct {
    const char *path;
    int posix_result;
    int win_result;
} isabs_tests[] = {
    { "",      0, 0 },
    { "/",     1, 1 },
    { "/a",    1, 1 },
    { "a/b",   0, 0 },
    { "\\",    0, 1 },
    { "\\a",   0, 1 },
    { "c:",    0, 0 },
    { "/:",    1, 0 },
    { "\\:",   0, 0 },
    { "c:/a",  0, 1 },
    { "c:\\a", 0, 1 },
    { "c:a",   0, 0 },
    { "c:a/b", 0, 0 },
    { "/:a/b", 1, 0 },
};

int
main(void)
{
    char *dirname, *basename, *joined;
    const char *edirname, *ebasename, *ejoined, *ipath, *path1, *path2;
    int result, eresult, status = 0;
    size_t i;

    for (i = 0; i < sizeof(split_tests) / sizeof(*split_tests); i++) {
        ipath = split_tests[i].path;
#ifdef WINDOWS_PATHS
        edirname = split_tests[i].win_dirname;
        ebasename = split_tests[i].win_basename;
#else
        edirname = split_tests[i].posix_dirname;
        ebasename = split_tests[i].posix_basename;
#endif
        assert(k5_path_split(ipath, NULL, NULL) == 0);
        assert(k5_path_split(ipath, &dirname, NULL) == 0);
        free(dirname);
        assert(k5_path_split(ipath, NULL, &basename) == 0);
        free(basename);
        assert(k5_path_split(ipath, &dirname, &basename) == 0);
        if (strcmp(dirname, edirname) != 0) {
            fprintf(stderr, "Split test %d: dirname %s != expected %s\n",
                    (int)i, dirname, edirname);
            status = 1;
        }
        if (strcmp(basename, ebasename) != 0) {
            fprintf(stderr, "Split test %d: basename %s != expected %s\n",
                    (int)i, basename, ebasename);
            status = 1;
        }
        free(dirname);
        free(basename);
    }

    for (i = 0; i < sizeof(join_tests) / sizeof(*join_tests); i++) {
        path1 = join_tests[i].path1;
        path2 = join_tests[i].path2;
#ifdef WINDOWS_PATHS
        ejoined = join_tests[i].win_result;
#else
        ejoined = join_tests[i].posix_result;
#endif
        assert(k5_path_join(path1, path2, &joined) == 0);
        if (strcmp(joined, ejoined) != 0) {
            fprintf(stderr, "Join test %d: %s != expected %s\n",
                    (int)i, joined, ejoined);
            status = 1;
        }
        free(joined);
    }

    for (i = 0; i < sizeof(isabs_tests) / sizeof(*isabs_tests); i++) {
#ifdef WINDOWS_PATHS
        eresult = isabs_tests[i].win_result;
#else
        eresult = isabs_tests[i].posix_result;
#endif
        result = k5_path_isabs(isabs_tests[i].path);
        if (result != eresult) {
            fprintf(stderr, "isabs test %d: %d != expected %d\n",
                    (int)i, result, eresult);
            status = 1;
        }
    }

    return status;
}
