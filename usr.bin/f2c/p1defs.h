#define P1_UNKNOWN 0
#define P1_COMMENT 1		/* Fortan comment string */
#define P1_EOF 2		/* End of file dummy token */
#define P1_SET_LINE 3		/* Reset the line counter */
#define P1_FILENAME 4		/* Name of current input file */
#define P1_NAME_POINTER 5	/* Pointer to hash table entry */
#define P1_CONST 6		/* Some constant value */
#define P1_EXPR 7		/* Followed by opcode */

/* The next two tokens could be grouped together, since they always come
   from an Addr structure */

#define P1_IDENT 8		/* Char string identifier in addrp->user
				   field */
#define P1_EXTERN 9		/* Pointer to external symbol entry */

#define P1_HEAD 10		/* Function header info */
#define P1_LIST 11		/* A list of data (e.g. arguments) will
				   follow the tag, type, and count */
#define P1_LITERAL 12		/* Hold the index into the literal pool */
#define P1_LABEL 13		/* label value */
#define P1_ASGOTO 14		/* Store the hash table pointer of
				   variable used in assigned goto */
#define P1_GOTO 15		/* Store the statement number */
#define P1_IF 16		/* store the condition as an expression */
#define P1_ELSE 17		/* No data */
#define P1_ELIF 18		/* store the condition as an expression */
#define P1_ENDIF 19		/* Marks the end of a block IF */
#define P1_ENDELSE 20		/* Marks the end of a block ELSE */
#define P1_ADDR 21		/* Addr data; used for arrays, common and
				   equiv addressing, NOT for names, idents
				   or externs */
#define P1_SUBR_RET 22		/* Subroutine return; the return expression
				   follows */
#define P1_COMP_GOTO 23		/* Computed goto; has expr, label list */
#define P1_FOR 24		/* C FOR loop; three expressions follow */
#define P1_ENDFOR 25		/* End of C FOR loop */
#define P1_FORTRAN 26		/* original Fortran source */
#define P1_CHARP 27		/* user.Charp field -- for long names */
#define P1_WHILE1START 28	/* start of DO WHILE */
#define P1_WHILE2START 29	/* rest of DO WHILE */
#define P1_PROCODE 30		/* invoke procode() -- to adjust params */
#define P1_ELSEIFSTART 31	/* handle extra code for abs, min, max
				   in else if() */

#define P1_FILENAME_MAX	256	/* max filename length to retain (for -g) */
#define P1_STMTBUFSIZE 1400



#define COMMENT_BUFFER_SIZE 255	/* max number of chars in each comment */
#define CONSTANT_STR_MAX 1000	/* max number of chars in string constant */

void	p1_asgoto Argdcl((Addrp));
void	p1_comment Argdcl((char*));
void	p1_elif Argdcl((tagptr));
void	p1_else Argdcl((void));
void	p1_endif Argdcl((void));
void	p1_expr Argdcl((tagptr));
void	p1_for Argdcl((tagptr, tagptr, tagptr));
void	p1_goto Argdcl((long int));
void	p1_head Argdcl((int, char*));
void	p1_if Argdcl((tagptr));
void	p1_label Argdcl((long int));
void	p1_line_number Argdcl((long int));
void	p1_subr_ret Argdcl((tagptr));
void	p1comp_goto Argdcl((tagptr, int, struct Labelblock**));
void	p1else_end Argdcl((void));
void	p1for_end Argdcl((void));
void	p1put Argdcl((int));
void	p1puts Argdcl((int, char*));

/* The pass 1 intermediate file has the following format:

	<ascii-integer-rep> [ : [ <sp> [ <data> ]]] \n

   e.g.   1: This is a comment

   This format is destined to change in the future, but for now a readable
   form is more desirable than a compact form.

   NOTES ABOUT THE P1 FORMAT
   ----------------------------------------------------------------------

	P1_COMMENT:  The comment string (in   <data>)   may be at most
		COMMENT_BUFFER_SIZE bytes long.  It must contain no newlines
		or null characters.  A side effect of the way comments are
		read in   lex.c   is that no '\377' chars may be in a
		comment either.

	P1_SET_LINE:  <data>  holds the line number in the current source file.

	P1_INC_LINE:  Increment the source line number;   <data>   is empty.

	P1_NAME_POINTER:  <data>   holds the integer representation of a
			  pointer into a hash table entry.

	P1_CONST:  the first field in   <data>   is a type tag (one of the
		   TYxxxx   macros), the next field holds the constant
		   value

	P1_EXPR:  <data>   holds the opcode number of the expression,
		  followed by the type of the expression (required for
		  OPCONV).  Next is the value of   vleng.
		  The type of operation represented by the
		  opcode determines how many of the following data items
		  are part of this expression.

	P1_IDENT:  <data>   holds the type, then storage, then the
		   char string identifier in the   addrp->user   field.

	P1_EXTERN:  <data>   holds an offset into the external symbol
		    table entry

	P1_HEAD:  the first field in   <data>  is the procedure class, the
		  second is the name of the procedure

	P1_LIST:  the first field in   <data>   is the tag, the second the
		  type of the list, the third the number of elements in
		  the list

	P1_LITERAL:  <data>   holds the   litnum   of a value in the
		     literal pool.

	P1_LABEL:  <data>   holds the statement number of the current
		   line

	P1_ASGOTO:  <data>   holds the hash table pointer of the variable

	P1_GOTO:  <data>   holds the statement number to jump to

	P1_IF:  <data>   is empty, the following expression is the IF
	        condition.

	P1_ELSE:  <data>   is empty.

	P1_ELIF:  <data>   is empty, the following expression is the IF
		  condition.

	P1_ENDIF:  <data>   is empty.

	P1_ENDELSE:  <data>   is empty.

	P1_ADDR:   <data>   holds a direct copy of the structure.  The
		  next expression is a copy of    vleng,   and the next a
		  copy of    memoffset.

	P1_SUBR_RET:  The next token is an expression for the return value.

	P1_COMP_GOTO:  The next token is an integer expression, the
		       following one a list of labels.

	P1_FOR:  The next three expressions are the Init, Test, and
	         Increment expressions of a C FOR loop.

	P1_ENDFOR:  Marks the end of the body of a FOR loop

*/
