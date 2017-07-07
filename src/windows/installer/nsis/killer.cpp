/*
      Process Killer for NSIS script
      
      Rob Murawski
      
      Released under terms of IBM Open Source agreement for OpenAFS
      
      */


#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <vdmdbg.h>

char strProcessName[256];

typedef BOOL (CALLBACK *PROCENUMPROC)(DWORD, WORD, LPSTR, LPARAM);

typedef struct {
   DWORD          dwPID;
   PROCENUMPROC   lpProc;
   DWORD          lParam;
   BOOL           bEnd;
} EnumInfoStruct;

BOOL WINAPI EnumProcs(PROCENUMPROC lpProc, LPARAM lParam);

BOOL WINAPI Enum16(DWORD dwThreadId, WORD hMod16, WORD hTask16,
      PSZ pszModName, PSZ pszFileName, LPARAM lpUserDefined);

// 
// The EnumProcs function takes a pointer to a callback function
// that will be called once per process with the process filename 
// and process ID.
// 
// lpProc -- Address of callback routine.
// 
// lParam -- A user-defined LPARAM value to be passed to
//           the callback routine.
// 
// Callback function definition:
// BOOL CALLBACK Proc(DWORD dw, WORD w, LPCSTR lpstr, LPARAM lParam);
// 
BOOL WINAPI EnumProcs(PROCENUMPROC lpProc, LPARAM lParam) {

   OSVERSIONINFO  osver;
   HINSTANCE      hInstLib  = NULL;
   HINSTANCE      hInstLib2 = NULL;
   HANDLE         hSnapShot = NULL;
   LPDWORD        lpdwPIDs  = NULL;
   PROCESSENTRY32 procentry;
   BOOL           bFlag;
   DWORD          dwSize;
   DWORD          dwSize2;
   DWORD          dwIndex;
   HMODULE        hMod;
   HANDLE         hProcess;
   char           szFileName[MAX_PATH];
   EnumInfoStruct sInfo;

   // ToolHelp Function Pointers.
   HANDLE (WINAPI *lpfCreateToolhelp32Snapshot)(DWORD, DWORD);
   BOOL (WINAPI *lpfProcess32First)(HANDLE, LPPROCESSENTRY32);
   BOOL (WINAPI *lpfProcess32Next)(HANDLE, LPPROCESSENTRY32);

   // PSAPI Function Pointers.
   BOOL (WINAPI *lpfEnumProcesses)(DWORD *, DWORD, DWORD *);
   BOOL (WINAPI *lpfEnumProcessModules)(HANDLE, HMODULE *, DWORD, 
         LPDWORD);
   DWORD (WINAPI *lpfGetModuleBaseName)(HANDLE, HMODULE, LPTSTR, DWORD);

   // VDMDBG Function Pointers.
   INT (WINAPI *lpfVDMEnumTaskWOWEx)(DWORD, TASKENUMPROCEX, LPARAM);

   // Retrieve the OS version
   osver.dwOSVersionInfoSize = sizeof(osver);
   if (!GetVersionEx(&osver))
      return FALSE;
   
   // If Windows NT 4.0
   if (osver.dwPlatformId == VER_PLATFORM_WIN32_NT
         && osver.dwMajorVersion == 4) {

      __try {

         // Get the procedure addresses explicitly. We do
         // this so we don't have to worry about modules
         // failing to load under OSes other than Windows NT 4.0 
         // because references to PSAPI.DLL can't be resolved.
         hInstLib = LoadLibraryA("PSAPI.DLL");
         if (hInstLib == NULL)
            __leave;

         hInstLib2 = LoadLibraryA("VDMDBG.DLL");
         if (hInstLib2 == NULL)
            __leave;

         // Get procedure addresses.
         lpfEnumProcesses = (BOOL (WINAPI *)(DWORD *, DWORD, DWORD*))
               GetProcAddress(hInstLib, "EnumProcesses");

         lpfEnumProcessModules = (BOOL (WINAPI *)(HANDLE, HMODULE *,
               DWORD, LPDWORD)) GetProcAddress(hInstLib,
               "EnumProcessModules");

         lpfGetModuleBaseName = (DWORD (WINAPI *)(HANDLE, HMODULE,
               LPTSTR, DWORD)) GetProcAddress(hInstLib,
               "GetModuleBaseNameA");

         lpfVDMEnumTaskWOWEx = (INT (WINAPI *)(DWORD, TASKENUMPROCEX,
               LPARAM)) GetProcAddress(hInstLib2, "VDMEnumTaskWOWEx");
         
         if (lpfEnumProcesses == NULL 
               || lpfEnumProcessModules == NULL 
               || lpfGetModuleBaseName == NULL 
               || lpfVDMEnumTaskWOWEx == NULL)
            __leave;

         // 
         // Call the PSAPI function EnumProcesses to get all of the
         // ProcID's currently in the system.
         // 
         // NOTE: In the documentation, the third parameter of
         // EnumProcesses is named cbNeeded, which implies that you
         // can call the function once to find out how much space to
         // allocate for a buffer and again to fill the buffer.
         // This is not the case. The cbNeeded parameter returns
         // the number of PIDs returned, so if your buffer size is
         // zero cbNeeded returns zero.
         // 
         // NOTE: The "HeapAlloc" loop here ensures that we
         // actually allocate a buffer large enough for all the
         // PIDs in the system.
         // 
         dwSize2 = 256 * sizeof(DWORD);
         do {

            if (lpdwPIDs) {
               HeapFree(GetProcessHeap(), 0, lpdwPIDs);
               dwSize2 *= 2;
            }

            lpdwPIDs = (LPDWORD) HeapAlloc(GetProcessHeap(), 0, 
                  dwSize2);
            if (lpdwPIDs == NULL)
               __leave;
            
            if (!lpfEnumProcesses(lpdwPIDs, dwSize2, &dwSize))
               __leave;

         } while (dwSize == dwSize2);

         // How many ProcID's did we get?
         dwSize /= sizeof(DWORD);

         // Loop through each ProcID.
         for (dwIndex = 0; dwIndex < dwSize; dwIndex++) {

            szFileName[0] = 0;
            
            // Open the process (if we can... security does not
            // permit every process in the system to be opened).
            hProcess = OpenProcess(
                  PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                  FALSE, lpdwPIDs[dwIndex]);
            if (hProcess != NULL) {

               // Here we call EnumProcessModules to get only the
               // first module in the process. This will be the 
               // EXE module for which we will retrieve the name.
               if (lpfEnumProcessModules(hProcess, &hMod,
                     sizeof(hMod), &dwSize2)) {

                  // Get the module name
                  if (!lpfGetModuleBaseName(hProcess, hMod,
                        szFileName, sizeof(szFileName)))
                     szFileName[0] = 0;
               }
               CloseHandle(hProcess);
            }
            // Regardless of OpenProcess success or failure, we
            // still call the enum func with the ProcID.
            if (!lpProc(lpdwPIDs[dwIndex], 0, szFileName, lParam))
               break;

            // Did we just bump into an NTVDM?
            if (_stricmp(szFileName, "NTVDM.EXE") == 0) {

               // Fill in some info for the 16-bit enum proc.
               sInfo.dwPID = lpdwPIDs[dwIndex];
               sInfo.lpProc = lpProc;
               sInfo.lParam = (DWORD) lParam;
               sInfo.bEnd = FALSE;

               // Enum the 16-bit stuff.
               lpfVDMEnumTaskWOWEx(lpdwPIDs[dwIndex],
                  (TASKENUMPROCEX) Enum16, (LPARAM) &sInfo);

               // Did our main enum func say quit?
               if (sInfo.bEnd)
                  break;
            }
         }

      } __finally {

         if (hInstLib)
            FreeLibrary(hInstLib);

         if (hInstLib2)
            FreeLibrary(hInstLib2);

         if (lpdwPIDs)
            HeapFree(GetProcessHeap(), 0, lpdwPIDs);
      }

   // If any OS other than Windows NT 4.0.
   } else if (osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS
         || (osver.dwPlatformId == VER_PLATFORM_WIN32_NT
         && osver.dwMajorVersion > 4)) {

      __try {

         hInstLib = LoadLibraryA("Kernel32.DLL");
         if (hInstLib == NULL)
            __leave;

         // If NT-based OS, load VDMDBG.DLL.
         if (osver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
            hInstLib2 = LoadLibraryA("VDMDBG.DLL");
            if (hInstLib2 == NULL)
               __leave;
         }

         // Get procedure addresses. We are linking to 
         // these functions explicitly, because a module using
         // this code would fail to load under Windows NT,
         // which does not have the Toolhelp32
         // functions in KERNEL32.DLL.
         lpfCreateToolhelp32Snapshot =
               (HANDLE (WINAPI *)(DWORD,DWORD))
               GetProcAddress(hInstLib, "CreateToolhelp32Snapshot");

         lpfProcess32First =
               (BOOL (WINAPI *)(HANDLE,LPPROCESSENTRY32))
               GetProcAddress(hInstLib, "Process32First");

         lpfProcess32Next =
               (BOOL (WINAPI *)(HANDLE,LPPROCESSENTRY32))
               GetProcAddress(hInstLib, "Process32Next");

         if (lpfProcess32Next == NULL
               || lpfProcess32First == NULL
               || lpfCreateToolhelp32Snapshot == NULL)
            __leave;

         if (osver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
            lpfVDMEnumTaskWOWEx = (INT (WINAPI *)(DWORD, TASKENUMPROCEX,
                  LPARAM)) GetProcAddress(hInstLib2, "VDMEnumTaskWOWEx");
            if (lpfVDMEnumTaskWOWEx == NULL)
               __leave;
         }

         // Get a handle to a Toolhelp snapshot of all processes.
         hSnapShot = lpfCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
         if (hSnapShot == INVALID_HANDLE_VALUE) {
            FreeLibrary(hInstLib);
            return FALSE;
         }

         // Get the first process' information.
         procentry.dwSize = sizeof(PROCESSENTRY32);
         bFlag = lpfProcess32First(hSnapShot, &procentry);

         // While there are processes, keep looping.
         while (bFlag) {
            
            // Call the enum func with the filename and ProcID.
            if (lpProc(procentry.th32ProcessID, 0,
                  procentry.szExeFile, lParam)) {

               // Did we just bump into an NTVDM?
               if (_stricmp(procentry.szExeFile, "NTVDM.EXE") == 0) {

                  // Fill in some info for the 16-bit enum proc.
                  sInfo.dwPID = procentry.th32ProcessID;
                  sInfo.lpProc = lpProc;
                  sInfo.lParam = (DWORD) lParam;
                  sInfo.bEnd = FALSE;

                  // Enum the 16-bit stuff.
                  lpfVDMEnumTaskWOWEx(procentry.th32ProcessID,
                     (TASKENUMPROCEX) Enum16, (LPARAM) &sInfo);

                  // Did our main enum func say quit?
                  if (sInfo.bEnd)
                     break;
               }

               procentry.dwSize = sizeof(PROCESSENTRY32);
               bFlag = lpfProcess32Next(hSnapShot, &procentry);

            } else
               bFlag = FALSE;
         }

      } __finally {

         if (hInstLib)
            FreeLibrary(hInstLib);

         if (hInstLib2)
            FreeLibrary(hInstLib2);
      }

   } else
      return FALSE;

   // Free the library.
   FreeLibrary(hInstLib);

   return TRUE;
}


BOOL WINAPI Enum16(DWORD dwThreadId, WORD hMod16, WORD hTask16,
      PSZ pszModName, PSZ pszFileName, LPARAM lpUserDefined) {

   BOOL bRet;

   EnumInfoStruct *psInfo = (EnumInfoStruct *)lpUserDefined;

   bRet = psInfo->lpProc(psInfo->dwPID, hTask16, pszFileName,
      psInfo->lParam);

   if (!bRet) 
      psInfo->bEnd = TRUE;

   return !bRet;
} 


BOOL CALLBACK MyProcessEnumerator(DWORD dwPID, WORD wTask, 
      LPCSTR szProcess, LPARAM lParam) {

   /*if (wTask == 0)
      printf("%5u   %s\n", dwPID, szProcess);
   else
      printf("  %5u %s\n", wTask, szProcess);*/
   
   if(stricmp(szProcess,strProcessName)==0)
   {
      HANDLE hProcess=OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPID);
      if(hProcess!=NULL)
         TerminateProcess(hProcess,0);
      CloseHandle(hProcess);
   }

   return TRUE;
}


void main(int argc, char *argv[])
{
   if(argc<2)
   {
      printf("Please specify the process name to kill\n");
      
      return;
   }

   if(strlen((argv[1]))<255)
      strcpy(strProcessName,(argv[1]));
   else
      return;
  
   EnumProcs((PROCENUMPROC) MyProcessEnumerator, 0);
  
}
