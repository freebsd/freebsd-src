#ifdef DEBUG

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <crtdbg.h>

void
OutputHeading(const char *explanation)
{
  _RPT1(_CRT_WARN,
	"\n\n%s:\n*********************************\n", explanation );
}

/*
 * The following macros set and clear, respectively, given bits
 * of the C runtime library debug flag, as specified by a bitmask.
 */
#define  SET_CRT_DEBUG_FIELD(a) \
            _CrtSetDbgFlag((a) | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG))
#define  CLEAR_CRT_DEBUG_FIELD(a) \
            _CrtSetDbgFlag(~(a) & _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG))

_CrtMemState s1;
_CrtMemState s2;
_CrtMemState s3;
static _CrtMemState *ss1 = NULL;
static _CrtMemState *ss2 = NULL;

void debug_init();

void
debug_check()
{
  _CrtMemState *temp;

  OutputHeading("Checking memory...");

  if (ss1 == NULL) {
    debug_init();
    ss1 = &s1;
    ss2 = &s2;
  }

  _CrtCheckMemory();

  /*   _CrtMemDumpAllObjectsSince( NULL ); */

  _CrtMemCheckpoint( &s2 );

  if ( _CrtMemDifference( &s3, &s1, &s2 ) )
    _CrtMemDumpStatistics( &s3 );

  /*   _CrtDumpMemoryLeaks(); */

  /*
   * swap the snapshots around
   */
  temp = ss1;
  ss1 = ss2;
  ss2 = temp;
}

void
debug_init()
{
  /* Send all reports to STDOUT */
   _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
   _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );
   _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
   _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDOUT );

   _CrtMemCheckpoint( &s1 );

   /*
    * Set the debug-heap flag so that freed blocks are kept on the
    * linked list, to catch any inadvertent use of freed memory
    */
   SET_CRT_DEBUG_FIELD( _CRTDBG_DELAY_FREE_MEM_DF );


   /*
    * Set the debug-heap flag so that memory leaks are reported when
    * the process terminates. Then, exit.
    */
   SET_CRT_DEBUG_FIELD( _CRTDBG_LEAK_CHECK_DF );
}
#endif /* DEBUG */
