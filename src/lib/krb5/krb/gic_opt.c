/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "k5-int.h"
#include "int-proto.h"
#include <krb5/clpreauth_plugin.h>

#define GIC_OPT_EXTENDED      0x80000000
#define GIC_OPT_SHALLOW_COPY  0x40000000

#define DEFAULT_FLAGS KRB5_GET_INIT_CREDS_OPT_CHG_PWD_PRMPT

#if defined(__MACH__) && defined(__APPLE__)
#include <TargetConditionals.h>
#endif

/* Match struct packing of krb5_get_init_creds_opt on macOS. */
#if TARGET_OS_MAC
#pragma pack(push,2)
#endif
struct extended_options {
    krb5_get_init_creds_opt opt;
    int num_preauth_data;
    krb5_gic_opt_pa_data *preauth_data;
    char *fast_ccache_name;
    krb5_ccache in_ccache;
    krb5_ccache out_ccache;
    krb5_flags fast_flags;
    krb5_expire_callback_func expire_cb;
    void *expire_data;
    krb5_responder_fn responder;
    void *responder_data;
    int pac_request;            /* -1 unset, 0 false, 1 true */
};
#if TARGET_OS_MAC
#pragma pack(pop)
#endif

void KRB5_CALLCONV
krb5_get_init_creds_opt_init(krb5_get_init_creds_opt *opt)
{
    opt->flags = DEFAULT_FLAGS;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_tkt_life(krb5_get_init_creds_opt *opt,
                                     krb5_deltat tkt_life)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_TKT_LIFE;
    opt->tkt_life = tkt_life;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_renew_life(krb5_get_init_creds_opt *opt,
                                       krb5_deltat renew_life)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE;
    opt->renew_life = renew_life;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_forwardable(krb5_get_init_creds_opt *opt,
                                        int forwardable)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_FORWARDABLE;
    opt->forwardable = forwardable;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_proxiable(krb5_get_init_creds_opt *opt,
                                      int proxiable)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_PROXIABLE;
    opt->proxiable = proxiable;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_canonicalize(krb5_get_init_creds_opt *opt,
                                         int canonicalize)
{
    if (canonicalize)
        opt->flags |= KRB5_GET_INIT_CREDS_OPT_CANONICALIZE;
    else
        opt->flags &= ~(KRB5_GET_INIT_CREDS_OPT_CANONICALIZE);
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_anonymous (krb5_get_init_creds_opt *opt,
                                       int anonymous)
{
    if (anonymous)
        opt->flags |= KRB5_GET_INIT_CREDS_OPT_ANONYMOUS;
    else opt->flags &= ~KRB5_GET_INIT_CREDS_OPT_ANONYMOUS;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_etype_list(krb5_get_init_creds_opt *opt, krb5_enctype *etype_list, int etype_list_length)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST;
    opt->etype_list = etype_list;
    opt->etype_list_length = etype_list_length;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_address_list(krb5_get_init_creds_opt *opt,
                                         krb5_address **addresses)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST;
    opt->address_list = addresses;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_preauth_list(krb5_get_init_creds_opt *opt,
                                         krb5_preauthtype *preauth_list,
                                         int preauth_list_length)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST;
    opt->preauth_list = preauth_list;
    opt->preauth_list_length = preauth_list_length;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_salt(krb5_get_init_creds_opt *opt, krb5_data *salt)
{
    opt->flags |= KRB5_GET_INIT_CREDS_OPT_SALT;
    opt->salt = salt;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_set_change_password_prompt(
    krb5_get_init_creds_opt *opt, int prompt)
{
    if (prompt)
        opt->flags |= KRB5_GET_INIT_CREDS_OPT_CHG_PWD_PRMPT;
    else
        opt->flags &= ~KRB5_GET_INIT_CREDS_OPT_CHG_PWD_PRMPT;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_alloc(krb5_context context,
                              krb5_get_init_creds_opt **opt)
{
    struct extended_options *opte;

    if (opt == NULL)
        return EINVAL;
    *opt = NULL;

    /* Return an extended structure cast as a krb5_get_init_creds_opt. */
    opte = calloc(1, sizeof(*opte));
    if (opte == NULL)
        return ENOMEM;
    opte->opt.flags = DEFAULT_FLAGS | GIC_OPT_EXTENDED;
    opte->pac_request = -1;
    *opt = (krb5_get_init_creds_opt *)opte;
    return 0;
}

void KRB5_CALLCONV
krb5_get_init_creds_opt_free(krb5_context context,
                             krb5_get_init_creds_opt *opt)
{
    struct extended_options *opte = (struct extended_options *)opt;
    int i;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return;
    assert(!(opt->flags & GIC_OPT_SHALLOW_COPY));
    for (i = 0; i < opte->num_preauth_data; i++) {
        free(opte->preauth_data[i].attr);
        free(opte->preauth_data[i].value);
    }
    free(opte->preauth_data);
    free(opte->fast_ccache_name);
    free(opte);
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_pa(krb5_context context,
                               krb5_get_init_creds_opt *opt,
                               const char *attr,
                               const char *value)
{
    struct extended_options *opte = (struct extended_options *)opt;
    krb5_gic_opt_pa_data *t, *pa;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    assert(!(opt->flags & GIC_OPT_SHALLOW_COPY));

    /* Allocate space for another option. */
    t = realloc(opte->preauth_data, (opte->num_preauth_data + 1) * sizeof(*t));
    if (t == NULL)
        return ENOMEM;
    opte->preauth_data = t;

    /* Copy the option into the new slot. */
    pa = &opte->preauth_data[opte->num_preauth_data];
    pa->attr = strdup(attr);
    if (pa->attr == NULL)
        return ENOMEM;
    pa->value = strdup(value);
    if (pa->value == NULL) {
        free(pa->attr);
        return ENOMEM;
    }
    opte->num_preauth_data++;

    /* Give preauth modules a chance to look at the option now. */
    return krb5_preauth_supply_preauth_data(context, opt, attr, value);
}

/*
 * This function allows a preauth plugin to obtain preauth
 * options.  The preauth_data returned from this function
 * should be freed by calling krb5_get_init_creds_opt_free_pa().
 *
 * The 'opt' pointer supplied to this function must have been
 * obtained using krb5_get_init_creds_opt_alloc()
 */
krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_get_pa(krb5_context context,
                               krb5_get_init_creds_opt *opt,
                               int *num_preauth_data,
                               krb5_gic_opt_pa_data **preauth_data)
{
    struct extended_options *opte = (struct extended_options *)opt;
    krb5_gic_opt_pa_data *p = NULL;
    int i;

    if (num_preauth_data == NULL || preauth_data == NULL)
        return EINVAL;
    *num_preauth_data = 0;
    *preauth_data = NULL;
    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;

    if (opte->num_preauth_data == 0)
        return 0;

    p = calloc(opte->num_preauth_data, sizeof(*p));
    if (p == NULL)
        return ENOMEM;

    for (i = 0; i < opte->num_preauth_data; i++) {
        p[i].attr = strdup(opte->preauth_data[i].attr);
        p[i].value = strdup(opte->preauth_data[i].value);
        if (p[i].attr == NULL || p[i].value == NULL)
            goto cleanup;
    }
    *num_preauth_data = i;
    *preauth_data = p;
    return 0;

cleanup:
    krb5_get_init_creds_opt_free_pa(context, opte->num_preauth_data, p);
    return ENOMEM;
}

/*
 * This function frees the preauth_data that was returned by
 * krb5_get_init_creds_opt_get_pa().
 */
void KRB5_CALLCONV
krb5_get_init_creds_opt_free_pa(krb5_context context, int num_preauth_data,
                                krb5_gic_opt_pa_data *preauth_data)
{
    int i;

    if (num_preauth_data <= 0 || preauth_data == NULL)
        return;

    for (i = 0; i < num_preauth_data; i++) {
        free(preauth_data[i].attr);
        free(preauth_data[i].value);
    }
    free(preauth_data);
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_fast_ccache_name(krb5_context context,
                                             krb5_get_init_creds_opt *opt,
                                             const char *ccache_name)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    assert(!(opt->flags & GIC_OPT_SHALLOW_COPY));
    free(opte->fast_ccache_name);
    opte->fast_ccache_name = strdup(ccache_name);
    if (opte->fast_ccache_name == NULL)
        return ENOMEM;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_fast_ccache(krb5_context context,
                                        krb5_get_init_creds_opt *opt,
                                        krb5_ccache ccache)
{
    krb5_error_code ret;
    char *name;

    ret = krb5_cc_get_full_name(context, ccache, &name);
    if (ret)
        return ret;
    ret = krb5_get_init_creds_opt_set_fast_ccache_name(context, opt, name);
    free(name);
    return ret;
}

const char *
k5_gic_opt_get_fast_ccache_name(krb5_get_init_creds_opt *opt)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return NULL;
    return opte->fast_ccache_name;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_in_ccache(krb5_context context,
                                      krb5_get_init_creds_opt *opt,
                                      krb5_ccache ccache)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    opte->in_ccache = ccache;
    return 0;
}

krb5_ccache
k5_gic_opt_get_in_ccache(krb5_get_init_creds_opt *opt)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return NULL;
    return opte->in_ccache;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_out_ccache(krb5_context context,
                                       krb5_get_init_creds_opt *opt,
                                       krb5_ccache ccache)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    opte->out_ccache = ccache;
    return 0;
}

krb5_ccache
k5_gic_opt_get_out_ccache(krb5_get_init_creds_opt *opt)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return NULL;
    return opte->out_ccache;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_fast_flags(krb5_context context,
                                       krb5_get_init_creds_opt *opt,
                                       krb5_flags flags)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    opte->fast_flags = flags;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_get_fast_flags(krb5_context context,
                                       krb5_get_init_creds_opt *opt,
                                       krb5_flags *out_flags)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (out_flags == NULL)
        return EINVAL;
    *out_flags = 0;
    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    *out_flags = opte->fast_flags;
    return 0;
}

krb5_flags
k5_gic_opt_get_fast_flags(krb5_get_init_creds_opt *opt)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return 0;
    return opte->fast_flags;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_expire_callback(krb5_context context,
                                            krb5_get_init_creds_opt *opt,
                                            krb5_expire_callback_func cb,
                                            void *data)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    opte->expire_cb = cb;
    opte->expire_data = data;
    return 0;
}

void
k5_gic_opt_get_expire_cb(krb5_get_init_creds_opt *opt,
                         krb5_expire_callback_func *cb_out, void **data_out)
{
    struct extended_options *opte = (struct extended_options *)opt;

    *cb_out = NULL;
    *data_out = NULL;
    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return;
    *cb_out = opte->expire_cb;
    *data_out = opte->expire_data;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_responder(krb5_context context,
                                      krb5_get_init_creds_opt *opt,
                                      krb5_responder_fn responder, void *data)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    opte->responder = responder;
    opte->responder_data = data;
    return 0;
}

void
k5_gic_opt_get_responder(krb5_get_init_creds_opt *opt,
                         krb5_responder_fn *responder_out, void **data_out)
{
    struct extended_options *opte = (struct extended_options *)opt;

    *responder_out = NULL;
    *data_out = NULL;
    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return;
    *responder_out = opte->responder;
    *data_out = opte->responder_data;
}

krb5_get_init_creds_opt *
k5_gic_opt_shallow_copy(krb5_get_init_creds_opt *opt)
{
    struct extended_options *opte;

    opte = calloc(1, sizeof(*opte));
    if (opt == NULL)
        opte->opt.flags = DEFAULT_FLAGS;
    else if (opt->flags & GIC_OPT_EXTENDED)
        *opte = *(struct extended_options *)opt;
    else
        opte->opt = *opt;
    opte->opt.flags |= GIC_OPT_SHALLOW_COPY;
    return (krb5_get_init_creds_opt *)opte;
}

krb5_error_code KRB5_CALLCONV
krb5_get_init_creds_opt_set_pac_request(krb5_context context,
                                        krb5_get_init_creds_opt *opt,
                                        krb5_boolean req_pac)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return EINVAL;
    opte->pac_request = !!req_pac;
    return 0;
}

int
k5_gic_opt_pac_request(krb5_get_init_creds_opt *opt)
{
    struct extended_options *opte = (struct extended_options *)opt;

    if (opt == NULL || !(opt->flags & GIC_OPT_EXTENDED))
        return -1;
    return opte->pac_request;
}
