/*
 * Display a tree menu from file
 *
 * filename	- file with like find(1) output
 * FS		- fields separator
 * title	- title of dialog box
 * prompt	- prompt text into dialog box
 * height	- height of dialog box
 * width	- width of dialog box
 * menu_height	- height of menu box
 * result	- pointer to char array
 *
 * return values:
 * -1		- ESC pressed
 * 0		- Ok, result set (must be freed later)
 * 1		- Cancel
 */
 
int dialog_ftree(unsigned char *filename, unsigned char FS,
		unsigned char *title, unsigned char *prompt, 
			int height, int width, int menu_height, 
					unsigned char **result);

/*
 * Display a tree menu from array
 *
 * names	- array with like find(1) output
 * size		- size of array
 * FS		- fields separator
 * title	- title of dialog box
 * prompt	- prompt text into dialog box
 * height	- height of dialog box
 * width	- width of dialog box
 * menu_height	- height of menu box
 * result	- pointer to char array
 *
 * return values:
 * -1		- ESC pressed
 * 0		- Ok, result set (must be freed later)
 * 1		- Cancel
 */
 
int dialog_tree(unsigned char **names, int size, unsigned char FS,
		unsigned char *title, unsigned char *prompt, 
			int height, int width, int menu_height, 
					unsigned char **result);
