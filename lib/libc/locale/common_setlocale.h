#define categories _categories
extern char *_categories[_LC_LAST];

#define current_categories _current_categories
extern char _current_categories[_LC_LAST][32];

#define new_categories _new_categories
extern char _new_categories[_LC_LAST][32];

#define current_locale_string _current_locale_string
extern char _current_locale_string[_LC_LAST * 33];

#define currentlocale _currentlocale
extern char *_currentlocale __P((void));
