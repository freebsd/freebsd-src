#define FORM_NO_KEYS 12
#define FORM_LEFT 0
#define FORM_RIGHT 1
#define FORM_UP 2
#define FORM_DOWN 3
#define FORM_NEXT 4
#define FORM_EXIT 5
#define FORM_FIELD_HOME 6
#define FORM_FIELD_END 7
#define FORM_FIELD_LEFT 8
#define FORM_FIELD_RIGHT 9
#define FORM_FIELD_BACKSPACE 10
#define FORM_FIELD_DELETE 11

/* Attribute values */
#define FORM_DEFAULT_ATTR 0
#define FORM_SELECTED_ATTR 0

/* Field types */
#define FORM_FTYPE_INPUT 0
#define FORM_FTYPE_MENU 1
#define FORM_FTYPE_BUTTON 2
#define FORM_FTYPE_TEXT 3

#define MAX_FIELD_SIZE 80

struct form {
	int x;
	int y;
	int height;
	int width;
	struct field *fields;
};

struct input_field {
	int y_prompt;
	int x_prompt;
	int prompt_width;
	int prompt_attr;
	char *prompt;
	int y_field;
	int x_field;
	int field_width;
	int max_field_width;
	int field_attr;
	char *field;
};

struct text_field {
	int y;
	int x;
	int attr;
	char *text;
};

struct field {
	int field_id;
	int type;
	union {
		struct input_field input;
		struct text_field text;
#ifdef notyet
		struct menu_field menu
		struct button_field button;
#endif
	} entry;
	struct field *link;
	struct field *next;
	struct field *up;
	struct field *down;
	struct field *left;
	struct field *right;
};

extern unsigned int keymap[FORM_NO_KEYS];

int init_forms();
int edit_line(WINDOW *window, struct field *);
void edit_form(struct form *);
void refresh_form(WINDOW *, struct form *);
struct field *find_link(int);
