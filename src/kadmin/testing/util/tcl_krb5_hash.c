/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * All of the TCL krb5 functions which return (or place into output
 * variables) structures or pointers to structures that can't be
 * represented as tcl native types, do so by returning a handle for
 * the appropriate structure.  The handle is a string of the form
 * "type$id", where "type" is the type of datum represented by the
 * handle and "id" is a unique identifier for it.  This handle can
 * then be used later by the caller to refer to the object, and
 * internally to retrieve the actually datum from the appropriate hash
 * table.
 *
 * The functions in this file do four things:
 *
 * 1) Given a pointer to a datum and a string representing the type of
 * datum to which the pointer refers, create a new handle for the
 * datum, store the datum in the hash table using the new handle as
 * its key, and return the new handle.
 *
 * 2) Given a handle, locate and return the appropriate hash table
 * datum.
 *
 * 3) Given a handle, look through a table of types and unparse
 * functions to figure out what function to call to get a string
 * representation of the datum, call it with the appropriate pointer
 * (obtained from the hash table) as an argument, and return the
 * resulting string as the unparsed form of the datum.
 *
 * 4) Given a handle, remove that handle and its associated datum from
 * the hash table (but don't free it -- it's assumed to have already
 * been freed by the caller).
 */

#if HAVE_TCL_H
#include <tcl.h>
#elif HAVE_TCL_TCL_H
#include <tcl/tcl.h>
#endif
#include <assert.h>

#define SEP_STR "$"

static char *memory_error = "out of memory";

/*
 * Right now, we're only using one hash table.  However, at some point
 * in the future, we might decide to use a separate hash table for
 * every type.  Therefore, I'm putting this function in as an
 * abstraction so it's the only thing we'll have to change if we
 * decide to do that.
 *
 * Also, this function allows us to put in just one place the code for
 * checking to make sure that the hash table exists and initializing
 * it if it doesn't.
 */

static TclHashTable *get_hash_table(Tcl_Interp *interp,
                                    char *type)
{
    static Tcl_HashTable *hash_table = 0;

    if (! hash_table) {
        if (! (hash_table = malloc(sizeof(*hash_table)))) {
            Tcl_SetResult(interp, memory_error, TCL_STATIC);
            return 0;
        }
        Tcl_InitHashTable(hash_table, TCL_STRING_KEYS);
    }
    return hash_table;
}

#define MAX_ID 999999999
#define ID_BUF_SIZE 10

static Tcl_HashEntry *get_new_handle(Tcl_Interp *interp,
                                     char *type)
{
    static unsigned long int id_counter = 0;
    Tcl_DString *handle;
    char int_buf[ID_BUF_SIZE];

    if (! (handle = malloc(sizeof(*handle)))) {
        Tcl_SetResult(interp, memory_error, TCL_STATIC);
        return 0;
    }
    Tcl_DStringInit(handle);

    assert(id_counter <= MAX_ID);

    sprintf(int_buf, "%d", id_counter++);

    Tcl_DStringAppend(handle, type, -1);
    Tcl_DStringAppend(handle, SEP_STR, -1);
    Tcl_DStringAppend(handle, int_buf, -1);

    return handle;
}


Tcl_DString *tcl_krb5_create_object(Tcl_Interp *interp,
                                    char *type,
                                    ClientData datum)
{
    Tcl_HashTable *table;
    Tcl_DString *handle;
    Tcl_HashEntry *entry;
    int entry_created = 0;

    if (! (table = get_hash_table(interp, type))) {
        return 0;
    }

    if (! (handle = get_new_handle(interp, type))) {
        return 0;
    }

    if (! (entry = Tcl_CreateHashEntry(table, handle, &entry_created))) {
        Tcl_SetResult(interp, "error creating hash entry", TCL_STATIC);
        Tcl_DStringFree(handle);
        return TCL_ERROR;
    }

    assert(entry_created);

    Tcl_SetHashValue(entry, datum);

    return handle;
}

ClientData tcl_krb5_get_object(Tcl_Interp *interp,
                               char *handle)
{
    char *myhandle, *id_ptr;
    Tcl_HashTable *table;
    Tcl_HashEntry *entry;

    if (! (myhandle = strdup(handle))) {
        Tcl_SetResult(interp, memory_error, TCL_STATIC);
        return 0;
    }

    if (! (id_ptr = index(myhandle, *SEP_STR))) {
        free(myhandle);
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "malformatted handle \"", handle,
                         "\"", 0);
        return 0;
    }

    *id_ptr = '\0';

    if (! (table = get_hash_table(interp, myhandle))) {
        free(myhandle);
        return 0;
    }

    free(myhandle);

    if (! (entry = Tcl_FindHashEntry(table, handle))) {
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "no object corresponding to handle \"",
                         handle, "\"", 0);
        return 0;
    }

    return(Tcl_GetHashValue(entry));
}
