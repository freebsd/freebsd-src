#if __STDC__
#define C_SYMBOL_NAME(name) _##name
#else
#define C_SYMBOL_NAME(name) _/**/name
#endif
