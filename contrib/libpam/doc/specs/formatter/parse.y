
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
    
#define MAXLINE  1000
#define INDENT_STRING "  "
#define PAPER_WIDTH   74

    int indent=0;
    int line=1;
    char *last_label=NULL;

    extern void yyerror(const char *x);
    extern char *get_label(const char *label);
    extern void set_label(const char *label, const char *target);
    char *new_counter(const char *key);

#include "lex.yy.c"

%}

%union {
    int def;
    char *string;
}

%token NEW_COUNTER LABEL HASH CHAR NEWLINE NO_INDENT RIGHT
%type <string> stuff text

%start doc

%%

doc:
| doc NEWLINE {
    printf("\n");
    ++line;
}
| doc stuff NEWLINE {
    if (strlen($2) > (PAPER_WIDTH-(indent ? strlen(INDENT_STRING):0))) {
	yyerror("line too long");
    }
    printf("%s%s\n", indent ? INDENT_STRING:"", $2);
    free($2);
    indent = 1;
    ++line;
}
| doc stuff RIGHT stuff NEWLINE {
    char fixed[PAPER_WIDTH+1];
    int len;

    len = PAPER_WIDTH-(strlen($2)+strlen($4));

    if (len >= 0) {
	memset(fixed, ' ', len);
	fixed[len] = '\0';
    } else {
	yyerror("line too wide");
	fixed[0] = '\0';
    }
    printf("%s%s%s\n", $2, fixed, $4);
    free($2);
    free($4);
    indent = 1;
    ++line;
}
| doc stuff RIGHT stuff RIGHT stuff NEWLINE {
    char fixed[PAPER_WIDTH+1];
    int len, l;

    len = PAPER_WIDTH-(strlen($2)+strlen($4));

    if (len < 0) {
	len = 0;
	yyerror("line too wide");
    }

    l = len/2;
    memset(fixed, ' ', l);
    fixed[l] = '\0';
    printf("%s%s%s", $2, fixed, $4);
    free($2);
    free($4);
    
    l = (len+1)/2;
    memset(fixed, ' ', l);
    fixed[l] = '\0';
    printf("%s%s\n", fixed, $6);
    free($6);

    indent = 1;
    ++line;
}
| doc stuff RIGHT stuff RIGHT stuff NEWLINE {
    char fixed[PAPER_WIDTH+1];
    int len, l;

    len = PAPER_WIDTH-(strlen($2)+strlen($4));

    if (len < 0) {
	len = 0;
	yyerror("line too wide");
    }

    l = len/2;
    memset(fixed, ' ', l);
    fixed[l] = '\0';
    printf("%s%s%s", $2, fixed, $4);
    free($2);
    free($4);
    
    l = (len+1)/2;
    memset(fixed, ' ', l);
    fixed[l] = '\0';
    printf("%s%s\n", fixed, $6);
    free($6);

    indent = 1;
    ++line;
}
;

stuff: {
    $$ = strdup("");
}
| stuff text {
    $$ = malloc(strlen($1)+strlen($2)+1);
    sprintf($$,"%s%s", $1, $2);
    free($1);
    free($2);
}
;

text: CHAR {
    $$ = strdup(yytext);
}
| text CHAR {
    $$ = malloc(strlen($1)+2);
    sprintf($$,"%s%s", $1, yytext);
    free($1);
}
| NO_INDENT {
    $$ = strdup("");
    indent = 0;
}
| HASH {
    $$ = strdup("#");
}
| LABEL {
    if (($$ = get_label(yytext)) == NULL) {
	set_label(yytext, last_label);
	$$ = strdup("");
    }
}
| NEW_COUNTER {
    $$ = new_counter(yytext);
}
;

%%

typedef struct node_s {
    struct node_s *left, *right;
    const char *key;
    char *value;
} *node_t;

node_t label_root = NULL;
node_t counter_root = NULL;

const char *find_key(node_t root, const char *key)
{
    while (root) {
	int cmp = strcmp(key, root->key);

	if (cmp > 0) {
	    root = root->right;
	} else if (cmp) {
	    root = root->left;
	} else {
	    return root->value;
	}
    }
    return NULL;
}

node_t set_key(node_t root, const char *key, const char *value)
{
    if (root) {
	int cmp = strcmp(key, root->key);
	if (cmp > 0) {
	    root->right = set_key(root->right, key, value);
	} else if (cmp) {
	    root->left = set_key(root->left, key, value);
	} else {
	    free(root->value);
	    root->value = strdup(value);
	}
    } else {
	root = malloc(sizeof(struct node_s));
	root->right = root->left = NULL;
	root->key = strdup(key);
	root->value = strdup(value);
    }
    return root;
}

void yyerror(const char *x)
{
    fprintf(stderr, "line %d: %s\n", line, x);
}

char *get_label(const char *label)
{
    const char *found = find_key(label_root, label);

    if (found) {
	return strdup(found);
    }
    return NULL;
}

void set_label(const char *label, const char *target)
{
    if (target == NULL) {
	yyerror("no hanging value for label");
	target = "<??>";
    }
    label_root = set_key(label_root, label, target);
}

char *new_counter(const char *key)
{
    int i=0, j, ndollars = 0;
    const char *old;
    char *new;

    if (key[i++] != '#') {
	yyerror("bad index");
	return strdup("<???>");
    }

    while (key[i] == '$') {
	++ndollars;
	++i;
    }

    key += i;
    old = find_key(counter_root, key);
    new = malloc(20*ndollars);

    if (old) {
	for (j=0; ndollars > 1 && old[j]; ) {
	    if (old[j++] == '.' && --ndollars <= 0) {
		break;
	    }
	}
	if (j) {
	    strncpy(new, old, j);
	}
	if (old[j]) {
	    i = atoi(old+j);
	} else {
	    new[j++] = '.';
	    i = 0;
	}
    } else {
	j=0;
	while (--ndollars > 0) {
	    new[j++] = '0';
	    new[j++] = '.';
	}
	i = 0;
    }
    new[j] = '\0';
    sprintf(new+j, "%d", ++i);

    counter_root = set_key(counter_root, key, new);
    
    if (last_label) {
	free(last_label);
    }
    last_label = strdup(new);

    return new;
}

main()
{
    yyparse();
}
