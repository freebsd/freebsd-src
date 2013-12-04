/* -*- buffer-read-only: t -*- vi: set ro:
 *
 * Prototypes for autoopts
 * Generated Fri Apr 29 15:44:06 PDT 2011
 */
#ifndef AUTOOPTS_PROTO_H_GUARD
#define AUTOOPTS_PROTO_H_GUARD 1

#ifndef LOCAL
#  define LOCAL extern
#  define REDEF_LOCAL 1
#else
#  undef  REDEF_LOCAL
#endif
/*
 *  Extracted from autoopts.c
 */
LOCAL void *
ao_malloc(size_t sz);

LOCAL void *
ao_realloc(void *p, size_t sz);

LOCAL char *
ao_strdup(char const *str);

LOCAL tSuccess
handle_opt(tOptions* pOpts, tOptState* pOptState);

LOCAL tSuccess
longOptionFind(tOptions* pOpts, char* pzOptName, tOptState* pOptState);

LOCAL tSuccess
shortOptionFind(tOptions* pOpts, uint_t optValue, tOptState* pOptState);

LOCAL tSuccess
doImmediateOpts(tOptions* pOpts);

LOCAL tSuccess
doRegularOpts(tOptions* pOpts);

/*
 *  Extracted from configfile.c
 */
LOCAL void
internalFileLoad(tOptions* pOpts);

LOCAL char*
parseAttributes(
    tOptions*           pOpts,
    char*               pzText,
    tOptionLoadMode*    pMode,
    tOptionValue*       pType );

LOCAL tSuccess
validateOptionsStruct(tOptions* pOpts, char const* pzProgram);

/*
 *  Extracted from environment.c
 */
LOCAL void
doPrognameEnv(tOptions* pOpts, teEnvPresetType type);

LOCAL void
doEnvPresets(tOptions* pOpts, teEnvPresetType type);

/*
 *  Extracted from load.c
 */
LOCAL void
mungeString(char* pzTxt, tOptionLoadMode mode);

LOCAL void
loadOptionLine(
    tOptions*   pOpts,
    tOptState*  pOS,
    char*       pzLine,
    tDirection  direction,
    tOptionLoadMode   load_mode );

/*
 *  Extracted from nested.c
 */
LOCAL void
unload_arg_list(tArgList* pAL);

LOCAL tOptionValue*
optionLoadNested(char const* pzTxt, char const* pzName, size_t nameLen);

LOCAL int
get_special_char(char const ** ppz, int * ct);

LOCAL void
emit_special_char(FILE * fp, int ch);

/*
 *  Extracted from sort.c
 */
LOCAL void
optionSort(tOptions* pOpts);

/*
 *  Extracted from stack.c
 */
LOCAL void
addArgListEntry(void** ppAL, void* entry);

#ifdef REDEF_LOCAL
#  undef LOCAL
#  define LOCAL
#endif
#endif /* AUTOOPTS_PROTO_H_GUARD */
