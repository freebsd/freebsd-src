#ifndef __LOADFUNCS_H__
#define __LOADFUNCS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>

typedef struct _FUNC_INFO {
    void** func_ptr_var;
    char* func_name;
} FUNC_INFO;

#define DECL_FUNC_PTR(x) FP_##x p##x
#define MAKE_FUNC_INFO(x) { (void**) &p##x, #x }
#define END_FUNC_INFO { 0, 0 }
#define TYPEDEF_FUNC(ret, call, name, args) typedef ret (call *FP_##name) args

void
UnloadFuncs(
    FUNC_INFO fi[],
    HINSTANCE h
    );

int
LoadFuncs(
    const char* dll_name,
    FUNC_INFO fi[],
    HINSTANCE* ph,  // [out, optional] - DLL handle
    int* pindex,    // [out, optional] - index of last func loaded (-1 if none)
    int cleanup,    // cleanup function pointers and unload on error
    int go_on,      // continue loading even if some functions cannot be loaded
    int silent      // do not pop-up a system dialog if DLL cannot be loaded
    );

#ifdef __cplusplus
}
#endif

#endif /* __LOADFUNCS_H__ */
