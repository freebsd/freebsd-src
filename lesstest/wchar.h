int wchar_len(byte ch);
void store_wchar(byte** p, wchar ch);
wchar load_wchar(const byte** p);
wchar read_wchar(int fd);
int is_wide_char(wchar ch);
int is_composing_char(wchar ch);
int is_combining_char(wchar ch1, wchar ch2);
