/* $FreeBSD$ */
#undef __P
#if defined (__STDC__) && __STDC__
#define __P(args) args
#else
#define __P(args) ()
#endif

void
  parse_long_options __P ((int _argc, char **_argv, const char *_command_name,
			   const char *_version_string, void (*_usage) (int)));
