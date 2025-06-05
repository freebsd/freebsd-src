#ifndef __WorkItem
#define __WorkItem

#include <list>
#include "windows.h"

extern "C" {
    #include "ccs_pipe.h"
    }

class WorkItem {
private:
          k5_ipc_stream    _buf;
          WIN_PIPE*       _pipe;
    const long            _rpcmsg;
    const long            _sst;
public:
    WorkItem(   k5_ipc_stream buf,
                WIN_PIPE*     pipe,
                const long    type,
                const long    serverStartTime);
    WorkItem(   const         WorkItem&);
    WorkItem();
    ~WorkItem();

    const k5_ipc_stream payload()       const   {return _buf;}
    const k5_ipc_stream take_payload();
          WIN_PIPE*     take_pipe();
          WIN_PIPE*     pipe()          const   {return _pipe;}
    const long          type()          const   {return _rpcmsg;}
    const long          sst()           const   {return _sst;}
    char*               print(char* buf);
    };

class WorkList {
private:
    std::list <WorkItem*>   wl;
    CRITICAL_SECTION        cs;
    HANDLE                  hEvent;
public:
    WorkList();
    ~WorkList();
    int initialize();
    int cleanup();
    void wait();
    int add(WorkItem*);
    int remove(WorkItem**);
    bool isEmpty() {return wl.empty();}
    };

#endif  // __WorkItem
