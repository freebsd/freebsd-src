Initial credentials
===================

Software that performs tasks such as logging users into a computer
when they type their Kerberos password needs to get initial
credentials (usually ticket granting tickets) from Kerberos.  Such
software shares some behavior with the :ref:`kinit(1)` program.

Whenever a program grants access to a resource (such as a local login
session on a desktop computer) based on a user successfully getting
initial Kerberos credentials, it must verify those credentials against
a secure shared secret (e.g., a host keytab) to ensure that the user
credentials actually originate from a legitimate KDC.  Failure to
perform this verification is a critical vulnerability, because a
malicious user can execute the "Zanarotti attack": the user constructs
a fake response that appears to come from the legitimate KDC, but
whose contents come from an attacker-controlled KDC.

Some applications read a Kerberos password over the network (ideally
over a secure channel), which they then verify against the KDC.  While
this technique may be the only practical way to integrate Kerberos
into some existing legacy systems, its use is contrary to the original
design goals of Kerberos.

The function :c:func:`krb5_get_init_creds_password` will get initial
credentials for a client using a password.  An application that needs
to verify the credentials can call :c:func:`krb5_verify_init_creds`.
Here is an example of code to obtain and verify TGT credentials, given
strings *princname* and *password* for the client principal name and
password::

    krb5_error_code ret;
    krb5_creds creds;
    krb5_principal client_princ = NULL;

    memset(&creds, 0, sizeof(creds));
    ret = krb5_parse_name(context, princname, &client_princ);
    if (ret)
        goto cleanup;
    ret = krb5_get_init_creds_password(context, &creds, client_princ,
                                       password, NULL, NULL, 0, NULL, NULL);
    if (ret)
        goto cleanup;
    ret = krb5_verify_init_creds(context, &creds, NULL, NULL, NULL, NULL);

    cleanup:
    krb5_free_principal(context, client_princ);
    krb5_free_cred_contents(context, &creds);
    return ret;

Options for get_init_creds
--------------------------

The function :c:func:`krb5_get_init_creds_password` takes an options
parameter (which can be a null pointer).  Use the function
:c:func:`krb5_get_init_creds_opt_alloc` to allocate an options
structure, and :c:func:`krb5_get_init_creds_opt_free` to free it.  For
example::

    krb5_error_code ret;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_creds creds;

    memset(&creds, 0, sizeof(creds));
    ret = krb5_get_init_creds_opt_alloc(context, &opt);
    if (ret)
        goto cleanup;
    krb5_get_init_creds_opt_set_tkt_life(opt, 24 * 60 * 60);
    ret = krb5_get_init_creds_password(context, &creds, client_princ,
                                       password, NULL, NULL, 0, NULL, opt);
    if (ret)
        goto cleanup;

    cleanup:
    krb5_get_init_creds_opt_free(context, opt);
    krb5_free_cred_contents(context, &creds);
    return ret;

Getting anonymous credentials
-----------------------------

As of release 1.8, it is possible to obtain fully anonymous or
partially anonymous (realm-exposed) credentials, if the KDC supports
it.  The MIT KDC supports issuing fully anonymous credentials as of
release 1.8 if configured appropriately (see :ref:`anonymous_pkinit`),
but does not support issuing realm-exposed anonymous credentials at
this time.

To obtain fully anonymous credentials, call
:c:func:`krb5_get_init_creds_opt_set_anonymous` on the options
structure to set the anonymous flag, and specify a client principal
with the KDC's realm and a single empty data component (the principal
obtained by parsing ``@``\ *realmname*).  Authentication will take
place using anonymous PKINIT; if successful, the client principal of
the resulting tickets will be
``WELLKNOWN/ANONYMOUS@WELLKNOWN:ANONYMOUS``.  Here is an example::

    krb5_get_init_creds_opt_set_anonymous(opt, 1);
    ret = krb5_build_principal(context, &client_princ, strlen(myrealm),
                               myrealm, "", (char *)NULL);
    if (ret)
        goto cleanup;
    ret = krb5_get_init_creds_password(context, &creds, client_princ,
                                       password, NULL, NULL, 0, NULL, opt);
    if (ret)
        goto cleanup;

To obtain realm-exposed anonymous credentials, set the anonymous flag
on the options structure as above, but specify a normal client
principal in order to prove membership in the realm.  Authentication
will take place as it normally does; if successful, the client
principal of the resulting tickets will be ``WELLKNOWN/ANONYMOUS@``\
*realmname*.

User interaction
----------------

Authenticating a user usually requires the entry of secret
information, such as a password.  A password can be supplied directly
to :c:func:`krb5_get_init_creds_password` via the *password*
parameter, or the application can supply prompter and/or responder
callbacks instead.  If callbacks are used, the user can also be
queried for other secret information such as a PIN, informed of
impending password expiration, or prompted to change a password which
has expired.

Prompter callback
~~~~~~~~~~~~~~~~~

A prompter callback can be specified via the *prompter* and *data*
parameters to :c:func:`krb5_get_init_creds_password`.  The prompter
will be invoked each time the krb5 library has a question to ask or
information to present.  When the prompter callback is invoked, the
*banner* argument (if not null) is intended to be displayed to the
user, and the questions to be answered are specified in the *prompts*
array.  Each prompt contains a text question in the *prompt* field, a
*hidden* bit to indicate whether the answer should be hidden from
display, and a storage area for the answer in the *reply* field.  The
callback should fill in each question's ``reply->data`` with the
answer, up to a maximum number of ``reply->length`` bytes, and then
reset ``reply->length`` to the length of the answer.

A prompter callback can call :c:func:`krb5_get_prompt_types` to get an
array of type constants corresponding to the prompts, to get
programmatic information about the semantic meaning of the questions.
:c:func:`krb5_get_prompt_types` may return a null pointer if no prompt
type information is available.

Text-based applications can use a built-in text prompter
implementation by supplying :c:func:`krb5_prompter_posix` as the
*prompter* parameter and a null pointer as the *data* parameter.  For
example::

    ret = krb5_get_init_creds_password(context, &creds, client_princ,
                                       NULL, krb5_prompter_posix, NULL, 0,
                                       NULL, NULL);

Responder callback
~~~~~~~~~~~~~~~~~~

A responder callback can be specified through the init_creds options
using the :c:func:`krb5_get_init_creds_opt_set_responder` function.
Responder callbacks can present a more sophisticated user interface
for authentication secrets.  The responder callback is usually invoked
only once per authentication, with a list of questions produced by all
of the allowed preauthentication mechanisms.

When the responder callback is invoked, the *rctx* argument can be
accessed to obtain the list of questions and to answer them.  The
:c:func:`krb5_responder_list_questions` function retrieves an array of
question types.  For each question type, the
:c:func:`krb5_responder_get_challenge` function retrieves additional
information about the question, if applicable, and the
:c:func:`krb5_responder_set_answer` function sets the answer.

Responder question types, challenges, and answers are UTF-8 strings.
The question type is a well-known string; the meaning of the challenge
and answer depend on the question type.  If an application does not
understand a question type, it cannot interpret the challenge or
provide an answer.  Failing to answer a question typically results in
the prompter callback being used as a fallback.

Password question
#################

The :c:macro:`KRB5_RESPONDER_QUESTION_PASSWORD` (or ``"password"``)
question type requests the user's password.  This question does not
have a challenge, and the response is simply the password string.

One-time password question
##########################

The :c:macro:`KRB5_RESPONDER_QUESTION_OTP` (or ``"otp"``) question
type requests a choice among one-time password tokens and the PIN and
value for the chosen token.  The challenge and answer are JSON-encoded
strings, but an application can use convenience functions to avoid
doing any JSON processing itself.

The :c:func:`krb5_responder_otp_get_challenge` function decodes the
challenge into a krb5_responder_otp_challenge structure.  The
:c:func:`krb5_responder_otp_set_answer` function selects one of the
token information elements from the challenge and supplies the value
and pin for that token.

PKINIT password or PIN question
###############################

The :c:macro:`KRB5_RESPONDER_QUESTION_PKINIT` (or ``"pkinit"``) question
type requests PINs for hardware devices and/or passwords for encrypted
credentials which are stored on disk, potentially also supplying
information about the state of the hardware devices.  The challenge and
answer are JSON-encoded strings, but an application can use convenience
functions to avoid doing any JSON processing itself.

The :c:func:`krb5_responder_pkinit_get_challenge` function decodes the
challenges into a krb5_responder_pkinit_challenge structure.  The
:c:func:`krb5_responder_pkinit_set_answer` function can be used to
supply the PIN or password for a particular client credential, and can
be called multiple times.

Example
#######

Here is an example of using a responder callback::

    static krb5_error_code
    my_responder(krb5_context context, void *data,
                 krb5_responder_context rctx)
    {
        krb5_error_code ret;
        krb5_responder_otp_challenge *chl;

        if (krb5_responder_get_challenge(context, rctx,
                                         KRB5_RESPONDER_QUESTION_PASSWORD)) {
            ret = krb5_responder_set_answer(context, rctx,
                                            KRB5_RESPONDER_QUESTION_PASSWORD,
                                            "open sesame");
            if (ret)
                return ret;
        }
        ret = krb5_responder_otp_get_challenge(context, rctx, &chl);
        if (ret == 0 && chl != NULL) {
            ret = krb5_responder_otp_set_answer(context, rctx, 0, "1234",
                                                NULL);
            krb5_responder_otp_challenge_free(context, rctx, chl);
            if (ret)
                return ret;
        }
        return 0;
    }

    static krb5_error_code
    get_creds(krb5_context context, krb5_principal client_princ)
    {
        krb5_error_code ret;
        krb5_get_init_creds_opt *opt = NULL;
        krb5_creds creds;

        memset(&creds, 0, sizeof(creds));
        ret = krb5_get_init_creds_opt_alloc(context, &opt);
        if (ret)
            goto cleanup;
        ret = krb5_get_init_creds_opt_set_responder(context, opt, my_responder,
                                                    NULL);
        if (ret)
            goto cleanup;
        ret = krb5_get_init_creds_password(context, &creds, client_princ,
                                           NULL, NULL, NULL, 0, NULL, opt);

    cleanup:
        krb5_get_init_creds_opt_free(context, opt);
        krb5_free_cred_contents(context, &creds);
        return ret;
    }

Verifying initial credentials
-----------------------------

Use the function :c:func:`krb5_verify_init_creds` to verify initial
credentials.  It takes an options structure (which can be a null
pointer).  Use :c:func:`krb5_verify_init_creds_opt_init` to initialize
the caller-allocated options structure, and
:c:func:`krb5_verify_init_creds_opt_set_ap_req_nofail` to set the
"nofail" option.  For example::

    krb5_verify_init_creds_opt vopt;

    krb5_verify_init_creds_opt_init(&vopt);
    krb5_verify_init_creds_opt_set_ap_req_nofail(&vopt, 1);
    ret = krb5_verify_init_creds(context, &creds, NULL, NULL, NULL, &vopt);

The confusingly named "nofail" option, when set, means that the
verification must actually succeed in order for
:c:func:`krb5_verify_init_creds` to indicate success.  The default
state of this option (cleared) means that if there is no key material
available to verify the user credentials, the verification will
succeed anyway.  (The default can be changed by a configuration file
setting.)

This accommodates a use case where a large number of unkeyed shared
desktop workstations need to allow users to log in using Kerberos.
The security risks from this practice are mitigated by the absence of
valuable state on the shared workstations---any valuable resources
that the users would access reside on networked servers.
