#define ESC 27
#define TAB 9

struct field {
	int y;
	int x;
	int width;
	int maxlen;
	int next;
	int up;
	int down;
	int left;
	int right;
	char field[80];
};

int disp_fields(WINDOW *, struct field *, int);
int change_field(struct field, int);
int edit_line(WINDOW *, int, int, char *, int, int);
