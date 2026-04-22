
libpkgconf `argvsplit` module
=============================

This is a lowlevel module which provides parsing of strings into argument vectors,
similar to what a shell would do.

.. c:function:: void pkgconf_argv_free(char **argv)

   Frees an argument vector.

   :param char** argv: The argument vector to free.
   :return: nothing

.. c:function:: int pkgconf_argv_split(const char *src, int *argc, char ***argv)

   Splits a string into an argument vector.

   :param char*   src: The string to split.
   :param int*    argc: A pointer to an integer to store the argument count.
   :param char*** argv: A pointer to a pointer for an argument vector.
   :return: 0 on success, -1 on error.
   :rtype: int
