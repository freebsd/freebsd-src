/* -*- buffer-read-only: t -*- vi: set ro:
 *
 * Prototypes for autoopts
 * Generated Sat May  5 12:02:36 PDT 2007
 */
#ifndef AUTOOPTS_PROTO_H_GUARD
#define AUTOOPTS_PROTO_H_GUARD 1
#ifndef LOCAL
#  define LOCAL extern
#  define REDEF_LOCAL 1
#else
#  undef  REDEF_LOCAL
#endif
/*\n *  Extracted from autoopts.c\n */
LOCAL void *
ao_malloc( size_t sz );

LOCAL void *
ao_realloc( void *p, size_t sz );

LOCAL void
ao_free( void *p );

LOCAL char *
ao_strdup( char const *str );

LOCAL tSuccess
handleOption( tOptions* pOpts, tOptState* pOptState );

LOCAL tSuccess
longOptionFind( tOptions* pOpts, char* pzOptName, tOptState* pOptState );

LOCAL tSuccess
shortOptionFind( tOptions* pOpts, uint_t optValue, tOptState* pOptState );

LOCAL tSuccess
doImmediateOpts( tOptions* pOpts );

LOCAL tSuccess
doRegularOpts( tOptions* pOpts );

/*\n *  Extracted from configfile.c\n */
LOCAL void
internalFileLoad( tOptions* pOpts );

LOCAL char*
parseAttributes(
    tOptions*           pOpts,
    char*               pzText,
    tOptionLoadMode*    pMode,
    tOptionValue*       pType );

LOCAL tSuccess
validateOptionsStruct( tOptions* pOpts, char const* pzProgram );

/*\n *  Extracted from environment.c\n */
LOCAL void
doPrognameEnv( tOptions* pOpts, teEnvPresetType type );

LOCAL void
doEnvPresets( tOptions* pOpts, teEnvPresetType type );

/*\n *  Extracted from load.c\n */
LOCAL void
mungeString( char* pzTxt, tOptionLoadMode mode );

LOCAL void
loadOptionLine(
    tOptions*   pOpts,
    tOptState*  pOS,
    char*       pzLine,
    tDirection  direction,
    tOptionLoadMode   load_mode );

/*\n *  Extracted from nested.c\n */
LOCAL tOptionValue*
optionLoadNested(char const* pzTxt, char const* pzName, size_t nameLen);

/*\n *  Extracted from sort.c\n */
LOCAL void
optionSort( tOptions* pOpts );

/*\n *  Extracted from stack.c\n */
LOCAL void
addArgListEntry( void** ppAL, void* entry );

#ifdef REDEF_LOCAL
#  undef LOCAL
#  define LOCAL
#endif
#endif /* AUTOOPTS_PROTO_H_GUARD */
