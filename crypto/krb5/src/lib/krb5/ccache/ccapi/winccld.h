/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * winccld.h -- the dynamic loaded version of the ccache DLL
 */


#ifndef KRB5_WINCCLD_H_
#define KRB5_WINCCLD_H_

#include <CredentialsCache.h>

typedef CCACHE_API cc_int32 (*FP_cc_initialize) (
    cc_context_t*           outContext,
    cc_int32                inVersion,
    cc_int32*               outSupportedVersion,
    char const**            outVendor);

#ifdef KRB5_WINCCLD_C_
typedef struct _FUNC_INFO {
    void** func_ptr_var;
    char* func_name;
} FUNC_INFO;

#define DECL_FUNC_PTR(x) FP_##x p##x
#define MAKE_FUNC_INFO(x) { (void**) &p##x, #x }
#define END_FUNC_INFO { 0, 0 }
#else
#define DECL_FUNC_PTR(x) extern FP_##x p##x
#endif

DECL_FUNC_PTR(cc_initialize);

#ifdef KRB5_WINCCLD_C_
FUNC_INFO krbcc_fi[] = {
    MAKE_FUNC_INFO(cc_initialize),
    END_FUNC_INFO
};
#undef MAKE_FUNC_INFO
#undef END_FUNC_INFO
#else

#define cc_initialize pcc_initialize
#endif

#undef DECL_FUNC_PTR

#endif /* KRB5_WINCCLD_H_ */
