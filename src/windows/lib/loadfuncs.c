#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "loadfuncs.h"

//
// UnloadFuncs:
//
// This function will reset all the function pointers of a function loaded
// by LaodFuncs and will free the DLL instance provided.
//

void
UnloadFuncs(
    FUNC_INFO fi[],
    HINSTANCE h
    )
{
    int n;
    if (fi)
        for (n = 0; fi[n].func_ptr_var; n++)
            *(fi[n].func_ptr_var) = NULL;
    if (h) FreeLibrary(h);
}


//
// LoadFuncs:
//
// This function try to load the functions for a DLL.  It returns 0 on failure
// and non-zero on success.  The parameters are descibed below.
//

int
LoadFuncs(
    const char* dll_name,
    FUNC_INFO fi[],
    HINSTANCE* ph,  // [out, optional] - DLL handle
    int* pindex,    // [out, optional] - index of last func loaded (-1 if none)
    int cleanup,    // cleanup function pointers and unload on error
    int go_on,      // continue loading even if some functions cannot be loaded
    int silent      // do not pop-up a system dialog if DLL cannot be loaded

    )
{
    HINSTANCE h;
    int i, n, last_i;
    int error = 0;
    UINT em;

    if (ph) *ph = 0;
    if (pindex) *pindex = -1;

    for (n = 0; fi[n].func_ptr_var; n++)
	*(fi[n].func_ptr_var) = NULL;

    if (silent)
	em = SetErrorMode(SEM_FAILCRITICALERRORS);
    h = LoadLibrary(dll_name);
    if (silent)
        SetErrorMode(em);

    if (!h)
        return 0;

    last_i = -1;
    for (i = 0; (go_on || !error) && (i < n); i++)
    {
	void* p = (void*)GetProcAddress(h, fi[i].func_name);
	if (!p)
	    error = 1;
        else
        {
            last_i = i;
	    *(fi[i].func_ptr_var) = p;
        }
    }
    if (pindex) *pindex = last_i;
    if (error && cleanup && !go_on) {
	for (i = 0; i < n; i++) {
	    *(fi[i].func_ptr_var) = NULL;
	}
	FreeLibrary(h);
	return 0;
    }
    if (ph) *ph = h;
    if (error) return 0;
    return 1;
}
