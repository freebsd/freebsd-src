/*
 * Copyright 2011 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Determines if a symbol is present in a given module.
 *
 * @param modname The name of the module.
 * @param symbname The name of the symbol.
 * @return Non-zero if found, zero if not found.
 */
int
module_symbol_is_present(const char *modname, const char *symbname);

/* Finds the file for a given symbol.
 *
 * If filename is non-null, the name of the file will be stored. This must
 * be freed with free().
 *
 * @param addr The address to resolve.
 * @param filename Where to store the name of the file.
 * @return 0 on error, non-zero on success.
 */
int
module_get_filename_for_symbol(void *addr, char **filename);

/* Closes a module.
 *
 * Does nothing if dll is NULL.
 *
 * @param dll The module to close.
 */
void
module_close(void *dll);


/* Loads a module and extracts the given symbol.
 *
 * This function loads the module specified by filename, but does not resolve
 * any of its symbol dependencies. Next is gets the symbol symbname and calls
 * shouldload(). If shouldload() returns non-zero, the module is reloaded
 * with full symbol resolution and stores the results in dll and symb.
 *
 * The job of shouldload() is to determine, based on the metadata in the
 * symbol fetched, if the module should be fully loaded. The shouldload()
 * callback MUST NOT attempt to call any functions in the module. This will
 * crash on WIN32.
 *
 * If an error occurs, an error string will be allocated and returned. If
 * allocation of this string fails, NULL will be returned. Since this is the
 * same as the non-error case, you should additionally check if dll or symb
 * is NULL.
 *
 * @param filename Path to the module
 * @param symbname Symbol name to load from the file and pass to shouldload()
 * @param shouldload Callback to determine whether to fullly load the module
 * @param misc Opaque pointer to pass to shouldload()
 * @param dll Where the module will be stored (can be NULL)
 * @param symb Where the symbol will be stored (can be NULL)
 * @return An error string.
 */
char *
module_load(const char *filename, const char *symbname,
            int (*shouldload)(void *symb, void *misc, char **err), void *misc,
            void **dll, void **symb);
