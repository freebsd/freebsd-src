/* manexamp.c -- The examples which appear in the documentation are here. */

#include <stdio.h>
#include <readline/readline.h>


/* **************************************************************** */
/*                                                                  */
*   			How to Emulate gets ()			    */
/*                                                                  */
/* **************************************************************** */

/* A static variable for holding the line. */
static char *line_read = (char *)NULL;

/* Read a string, and return a pointer to it.  Returns NULL on EOF. */
char *
do_gets ()
{
  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (line_read != (char *)NULL)
    {
      free (line_read);
      line_read = (char *)NULL;
    }

  /* Get a line from the user. */
  line_read = readline ("");

  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);

  return (line_read);
}


/* **************************************************************** */
/*                                                                  */
/*        Writing a Function to be Called by Readline.              */
/*                                                                  */
/* **************************************************************** */

/* Invert the case of the COUNT following characters. */
invert_case_line (count, key)
     int count, key;
{
  register int start, end;

  start = rl_point;

  if (count < 0)
    {
      direction = -1;
      count = -count;
    }
  else
    direction = 1;
      
  /* Find the end of the range to modify. */
  end = start + (count * direction);

  /* Force it to be within range. */
  if (end > rl_end)
    end = rl_end;
  else if (end < 0)
    end = -1;

  if (start > end)
    {
      int temp = start;
      start = end;
      end = temp;
    }

  if (start == end)
    return;

  /* Tell readline that we are modifying the line, so save the undo
     information. */
  rl_modifying (start, end);

  for (; start != end; start += direction)
    {
      if (uppercase_p (rl_line_buffer[start]))
	rl_line_buffer[start] = to_lower (rl_line_buffer[start]);
      else if (lowercase_p (rl_line_buffer[start]))
	rl_line_buffer[start] = to_upper (rl_line_buffer[start]);
    }

  /* Move point to on top of the last character changed. */
  rl_point = end - direction;
}


