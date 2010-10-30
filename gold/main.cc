// main.cc -- gold main function.

#include "gold.h"

#include "options.h"
#include "dirsearch.h"
#include "workqueue.h"
#include "object.h"
#include "symtab.h"
#include "layout.h"

using namespace gold;

int
main(int argc, char** argv)
{
#if defined (HAVE_SETLOCALE) && defined (HAVE_LC_MESSAGES)
  setlocale (LC_MESSAGES, "");
#endif
#if defined (HAVE_SETLOCALE)
  setlocale (LC_CTYPE, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  program_name = argv[0];

  // Handle the command line options.
  Command_line command_line;
  command_line.process(argc - 1, argv + 1);

  // The work queue.
  Workqueue workqueue(command_line.options());

  // The list of input objects.
  Input_objects input_objects;

  // The symbol table.
  Symbol_table symtab;

  // The layout object.
  Layout layout(command_line.options());

  // Get the search path from the -L options.
  Dirsearch search_path;
  search_path.add(&workqueue, command_line.options().search_path());

  // Queue up the first set of tasks.
  queue_initial_tasks(command_line.options(), search_path,
		      command_line, &workqueue, &input_objects,
		      &symtab, &layout);

  // Run the main task processing loop.
  workqueue.process();

  gold_exit(true);
}
