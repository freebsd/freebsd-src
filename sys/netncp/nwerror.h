/*
 * NetWare requestor error codes, they taken from NDK
 *
 * $FreeBSD: src/sys/netncp/nwerror.h,v 1.2 1999/12/12 05:50:07 bp Exp $
 */
#ifndef _NETNCP_NWERROR_H_
#define _NETNCP_NWERROR_H_

#ifndef SUCCESS
#define SUCCESS                        0
#endif

#define SHELL_ERROR                    0x8800
#define VLM_ERROR                      0x8800
#define ALREADY_ATTACHED               0x8800  /* 0  - Attach attempted to server with valid, existing connection */
#define INVALID_CONNECTION             0x8801  /* 1  - Request attempted with invalid or non-attached connection handle */
#define DRIVE_IN_USE                   0x8802  /* 2  - OS/2 only (NOT USED) */
#define CANT_ADD_CDS                   0x8803  /* 3  - Map drive attempted but unable to add new current directory structure */
#define DRIVE_CANNOT_MAP               0x8803
#define BAD_DRIVE_BASE                 0x8804  /* 4  - Map drive attempted with invalid path specification */
#define NET_READ_ERROR                 0x8805  /* 5  - Attempt to receive from the selected transport failed */
#define NET_RECV_ERROR                 0x8805  /* 5  */
#define UNKNOWN_NET_ERROR              0x8806  /* 6  - Network send attempted with an un-specific network error */
#define SERVER_INVALID_SLOT            0x8807  /* 7  - Server request attempted with invalid server connection slot */
#define BAD_SERVER_SLOT                0x8807  /* 7  */
#define NO_SERVER_SLOTS                0x8808  /* 8  - Attach attempted to server with no connection slots available */
#define NET_WRITE_ERROR                0x8809  /* 9  - Attempt to send on the selected transport failed */
#define CONNECTION_IN_ERROR_STATE      0x8809  /* Client-32 */
#define NET_SEND_ERROR                 0x8809  /* 9  */
#define SERVER_NO_ROUTE                0x880A  /* 10 - Attempted to find route to server where no route exists */
#define BAD_LOCAL_TARGET               0x880B  /* 11 - OS/2 only */
#define TOO_MANY_REQ_FRAGS             0x880C  /* 12 - Attempted request with too many request fragments specified */
#define CONNECT_LIST_OVERFLOW          0x880D  /* 13 */
#define BUFFER_OVERFLOW                0x880E  /* 14 - Attempt to receive more data than the reply buffer had room for */
#define MORE_DATA_ERROR                0x880E  /* Client-32 */
#define NO_CONN_TO_SERVER              0x880F  /* 15 */
#define NO_CONNECTION_TO_SERVER        0x880F  /* 15 - Attempt to get connection for a server not connected */
#define NO_ROUTER_FOUND                0x8810  /* 16 - OS/2 only */
#define BAD_FUNC_ERROR                 0x8811  /* 17 */
#define INVALID_SHELL_CALL             0x8811  /* 17 - Attempted function call to non- existent or illegal function */
#define SCAN_COMPLETE                  0x8812
#define LIP_RESIZE_ERROR               0x8812  /* Client-32 */
#define UNSUPPORTED_NAME_FORMAT_TYPE   0x8813
#define INVALID_DIR_HANDLE             0x8813  /* Client-32 */
#define HANDLE_ALREADY_LICENSED        0x8814
#define OUT_OF_CLIENT_MEMORY           0x8814  /* Client-32 */
#define HANDLE_ALREADY_UNLICENSED      0x8815
#define PATH_NOT_OURS                  0x8815  /* Client-32 */
#define INVALID_NCP_PACKET_LENGTH      0x8816
#define PATH_IS_PRINT_DEVICE           0x8816  /* Client-32 */
#define SETTING_UP_TIMEOUT             0x8817
#define PATH_IS_EXCLUDED_DEVICE        0x8817  /* Client-32 */
#define SETTING_SIGNALS                0x8818
#define PATH_IS_INVALID                0x8818  /* Client-32 */
#define SERVER_CONNECTION_LOST         0x8819
#define NOT_SAME_DEVICE                0x8819  /* Client-32 */
#define OUT_OF_HEAP_SPACE              0x881A
#define INVALID_SERVICE_REQUEST        0x881B
#define INVALID_SEARCH_HANDLE          0x881B  /* Client-32 */
#define INVALID_TASK_NUMBER            0x881C
#define INVALID_DEVICE_HANDLE          0x881C  /* Client-32 */
#define INVALID_MESSAGE_LENGTH         0x881D
#define INVALID_SEM_HANDLE             0x881D  /* Client-32 */
#define EA_SCAN_DONE                   0x881E
#define INVALID_CFG_HANDLE             0x881E  /* Client-32 */
#define BAD_CONNECTION_NUMBER          0x881F
#define INVALID_MOD_HANDLE             0x881F  /* Client-32 */
#define ASYN_FIRST_PASS                0x8820
#define INVALID_DEVICE_INDEX           0x8821
#define INVALID_CONN_HANDLE            0x8822
#define INVALID_QUEUE_ID               0x8823
#define INVALID_PDEVICE_HANDLE         0x8824
#define INVALID_JOB_HANDLE             0x8825
#define INVALID_ELEMENT_ID             0x8826
#define ALIAS_NOT_FOUND                0x8827
#define RESOURCE_SUSPENDED             0x8828
#define INVALID_QUEUE_SPECIFIED        0x8829
#define DEVICE_ALREADY_OPEN            0x882A
#define JOB_ALREADY_OPEN               0x882B
#define QUEUE_NAME_ID_MISMATCH         0x882C
#define JOB_ALREADY_STARTED            0x882D
#define SPECT_DAA_TYPE_NOT_SUPPORTED   0x882E
#define INVALID_ENVIR_HANDLE           0x882F
#define NOT_SAME_CONNECTION            0x8830  /* 48 - Internal server request attempted accross different server connections */
#define PRIMARY_CONNECTION_NOT_SET     0x8831  /* 49 - Attempt to retrieve default connection with no primary connection set */
#define NO_PRIMARY_SET                 0x8831  /* 49 */
#define KEYWORD_NOT_FOUND              0x8832  /* Client-32 */
#define PRINT_CAPTURE_NOT_IN_PROGRESS  0x8832  /* Client-32 */
#define NO_CAPTURE_SET                 0x8832  /* 50 */
#define NO_CAPTURE_IN_PROGRESS         0x8832  /* 50 - Capture information requested on port with no capture in progress */
#define BAD_BUFFER_LENGTH              0x8833  /* 51 */
#define INVALID_BUFFER_LENGTH          0x8833  /* 51 - Used to indicate length which caller requested on a GetDNC or SetDNC was too large */
#define NO_USER_NAME                   0x8834  /* 52 */
#define NO_NETWARE_PRINT_SPOOLER       0x8835  /* 53 - Capture requested without having the local print spooler installed */
#define INVALID_PARAMETER              0x8836  /* 54 - Attempted function with an invalid function parameter specified */
#define CONFIG_FILE_OPEN_FAILED        0x8837  /* 55 - OS/2 only */
#define NO_CONFIG_FILE                 0x8838  /* 56 - OS/2 only */
#define CONFIG_FILE_READ_FAILED        0x8839  /* 57 - OS/2 only */
#define CONFIG_LINE_TOO_LONG           0x883A  /* 58 - OS/2 only */
#define CONFIG_LINES_IGNORED           0x883B  /* 59 - OS/2 only */
#define NOT_MY_RESOURCE                0x883C  /* 60 - Attempted request made with a parameter using foriegn resource */
#define DAEMON_INSTALLED               0x883D  /* 61 - OS/2 only */
#define SPOOLER_INSTALLED              0x883E  /* 62 - Attempted load of print spooler with print spooler already installed */
#define CONN_TABLE_FULL                0x883F  /* 63 */
#define CONNECTION_TABLE_FULL          0x883F  /* 63 - Attempted to allocate a connection handle with no more local connection table entries */
#define CONFIG_SECTION_NOT_FOUND       0x8840  /* 64 - OS/2 only */
#define BAD_TRAN_TYPE                  0x8841  /* 65 */
#define INVALID_TRANSPORT_TYPE         0x8841  /* 65 - Attempted function on a connection with an invalid transport selected */
#define TDS_TAG_IN_USE                 0x8842  /* 66 - OS/2 only */
#define TDS_OUT_OF_MEMORY              0x8843  /* 67 - OS/2 only */
#define TDS_INVALID_TAG                0x8844  /* 68 - Attempted TDS function with invalid tag */
#define TDS_WRITE_TRUNCATED            0x8845  /* 69 - Attempted TDS write with buffer that exceeded buffer */
#define NO_CONNECTION_TO_DS            0x8846  /* Client-32 */
#define NO_DIRECTORY_SERVICE_CONNECTION 0x8846  /* 70 */
#define SERVICE_BUSY                   0x8846  /* 70 - Attempted request made to partially asynchronous function in busy state */
#define NO_SERVER_ERROR                0x8847  /* 71 - Attempted connect failed to find any servers responding */
#define BAD_VLM_ERROR                  0x8848  /* 72 - Attempted function call to non-existant or not-loaded overlay */
#define NETWORK_DRIVE_IN_USE           0x8849  /* 73 - Attempted map to network drive that was already mapped */
#define LOCAL_DRIVE_IN_USE             0x884A  /* 74 - Attempted map to local drive that was in use */
#define NO_DRIVES_AVAILABLE            0x884B  /* 75 - Attempted map to next available drive when none were available */
#define DEVICE_NOT_REDIRECTED          0x884C  /* 76 - The device is not redirected */
#define NO_MORE_SFT_ENTRIES            0x884D  /* 77 - Maximum number of files was reached */
#define UNLOAD_ERROR                   0x884E  /* 78 - Attempted unload failed */
#define IN_USE_ERROR                   0x884F  /* 79 - Attempted re-use of already in use connection entry */
#define TOO_MANY_REP_FRAGS             0x8850  /* 80 - Attempted request with too many reply fragments specified */
#define TABLE_FULL                     0x8851  /* 81 - Attempted to add a name into the name table after it was full */
#ifndef SOCKET_NOT_OPEN
#define SOCKET_NOT_OPEN                0x8852  /* 82 - Listen was posted on unopened socket */
#endif
#define MEM_MGR_ERROR                  0x8853  /* 83 - Attempted enhanced memory operation failed */
#define SFT3_ERROR                     0x8854  /* 84 - An SFT3 switch occured mid-transfer */
#define PREFERRED_NOT_FOUND            0x8855  /* 85 - the preferred directory server was not established but another directory server was returned */
#define DEVICE_NOT_RECOGNIZED          0x8856  /* 86 - used to determine if the device is not used by VISE so pass it on to the next redirector, if any. */
#define BAD_NET_TYPE                   0x8857  /* 87 - the network type (Bind/NDS) does not match the server version */
#define ERROR_OPENING_FILE             0x8858  /* 88 - generic open failure error, invalid path, access denied, etc.. */
#define NO_PREFERRED_SPECIFIED         0x8859  /* 89 - no preferred name specified */
#define ERROR_OPENING_SOCKET           0x885A  /* 90 - error opening a socket */
#define REQUESTER_FAILURE              0x885A  /* Client-32 */
#define RESOURCE_ACCESS_DENIED         0x885B  /* Client-32 */
#define SIGNATURE_LEVEL_CONFLICT       0x8861
#define NO_LOCK_FOUND                  0x8862  /* OS/2 - process lock on conn handle failed, process ID not recognized */
#define LOCK_TABLE_FULL                0x8863  /* OS/2 - process lock on conn handle failed, process lock table full */
#define INVALID_MATCH_DATA             0x8864
#define MATCH_FAILED                   0x8865
#define NO_MORE_ENTRIES                0x8866
#define INSUFFICIENT_RESOURCES         0x8867
#define STRING_TRANSLATION             0x8868
#define STRING_TRANSLATION_NEEDED      0x8868  /* Client-32 */
#define ACCESS_VIOLATION               0x8869
#define NOT_AUTHENTICATED              0x886A
#define INVALID_LEVEL                  0x886B
#define RESOURCE_LOCK_ERROR            0x886C
#define INVALID_NAME_FORMAT            0x886D
#define OBJECT_EXISTS                  0x886E
#define OBJECT_NOT_FOUND               0x886F
#define UNSUPPORTED_TRAN_TYPE          0x8870
#define INVALID_STRING_TYPE            0x8871
#define INVALID_OWNER                  0x8872
#define UNSUPPORTED_AUTHENTICATOR      0x8873
#define IO_PENDING                     0x8874
#define INVALID_DRIVE_NUM              0x8875
#define SHELL_FAILURE                  0x88FF
#define VLM_FAILURE                    0x88FF

#define  SVC_ALREADY_REGISTERED        0x8880 /* Client-32 */
#define  SVC_REGISTRY_FULL             0x8881 /* Client-32 */
#define  SVC_NOT_REGISTERED            0x8882 /* Client-32 */
#define  OUT_OF_RESOURCES              0x8883 /* Client-32 */
#define  RESOLVE_SVC_FAILED            0x8884 /* Client-32 */
#define  CONNECT_FAILED                0x8885 /* Client-32 */
#define  PROTOCOL_NOT_BOUND            0x8886 /* Client-32 */
#define  AUTHENTICATION_FAILED         0x8887 /* Client-32 */
#define  INVALID_AUTHEN_HANDLE         0x8888 /* Client-32 */
#define  AUTHEN_HANDLE_ALREADY_EXISTS  0x8889 /* Client-32 */

#define  DIFF_OBJECT_ALREADY_AUTHEN    0x8890 /* Client-32 */
#define  REQUEST_NOT_SERVICEABLE       0x8891 /* Client-32 */
#define  AUTO_RECONNECT_SO_REBUILD     0x8892 /* Client-32 */
#define  AUTO_RECONNECT_RETRY_REQUEST  0x8893 /* Client-32 */
#define  ASYNC_REQUEST_IN_USE          0x8894 /* Client-32 */
#define  ASYNC_REQUEST_CANCELED        0x8895 /* Client-32 */
#define  SESS_SVC_ALREADY_REGISTERED   0x8896 /* Client-32 */
#define  SESS_SVC_NOT_REGISTERED       0x8897 /* Client-32 */
#define  PREVIOUSLY_AUTHENTICATED      0x8899 /* Client-32 */
#define  RESOLVE_SVC_PARTIAL           0x889A /* Client-32 */
#define  NO_DEFAULT_SPECIFIED          0x889B /* Client-32 */
#define  HOOK_REQUEST_NOT_HANDLED      0x889C /* Client-32 */
#define  HOOK_REQUEST_BUSY             0x889D /* Client-32 */
#define  HOOK_REQUEST_QUEUED           0x889D /* Client-32 */
#define  AUTO_RECONNECT_SO_IGNORE      0x889E /* Client-32 */
#define  ASYNC_REQUEST_NOT_IN_USE      0x889F /* Client-32 */
#define  AUTO_RECONNECT_FAILURE        0x88A0 /* Client-32 */
#define  NET_ERROR_ABORT_APPLICATION   0x88A1 /* Client-32 */
#define  NET_ERROR_SUSPEND_APPLICATION 0x88A2 /* Client-32 */
#define  NET_ERROR_ABORTED_PROCESS_GROUP  0x88A3 /* Client-32 */
#define  NET_ERROR_PASSWORD_HAS_EXPIRED   0x88A5 /* Client-32 */
#define  NET_ERROR_NETWORK_INACTIVE       0x88A6 /* Client-32 */
#define  REPLY_TRUNCATED            0x88e6    /* 230 NLM */


/* Server Errors */

#define ERR_INSUFFICIENT_SPACE          0x8901  /* 001 */
#define ERR_NO_MORE_ENTRY           0x8914   /* 020 */
#define NLM_INVALID_CONNECTION         0x890a   /* 010 */
#define ERR_BUFFER_TOO_SMALL            0x8977  /* 119 */
#define ERR_VOLUME_FLAG_NOT_SET         0x8978  /* 120 the service requested, not avail. on the selected vol. */
#define ERR_NO_ITEMS_FOUND              0x8979  /* 121 */
#define ERR_CONN_ALREADY_TEMP           0x897a  /* 122 */
#define ERR_CONN_ALREADY_LOGGED_IN      0x897b  /* 123 */
#define ERR_CONN_NOT_AUTHENTICATED      0x897c  /* 124 */
#define ERR_CONN_NOT_LOGGED_IN          0x897d  /* 125 */
#define NCP_BOUNDARY_CHECK_FAILED       0x897e  /* 126 */
#define ERR_LOCK_WAITING                0x897f  /* 127 */
#define ERR_LOCK_FAIL                   0x8980  /* 128 */
#define FILE_IN_USE_ERROR               0x8980  /* 128 */
#define NO_MORE_FILE_HANDLES            0x8981  /* 129 */
#define NO_OPEN_PRIVILEGES              0x8982  /* 130 */
#define IO_ERROR_NETWORK_DISK           0x8983  /* 131 */
#define ERR_AUDITING_HARD_IO_ERROR      0x8983  /* 131 */
#define NO_CREATE_PRIVILEGES            0x8984  /* 132 */
#define ERR_AUDITING_NOT_SUPV           0x8984  /* 132 */
#define NO_CREATE_DELETE_PRIVILEGES     0x8985  /* 133 */
#define CREATE_FILE_EXISTS_READ_ONLY    0x8986  /* 134 */
#define WILD_CARDS_IN_CREATE_FILE_NAME  0x8987  /* 135 */
#define CREATE_FILENAME_ERROR           0x8987  /* 135 */
#define INVALID_FILE_HANDLE             0x8988  /* 136 */
#define NO_SEARCH_PRIVILEGES            0x8989  /* 137 */
#define NO_DELETE_PRIVILEGES            0x898A  /* 138 */
#define NO_RENAME_PRIVILEGES            0x898B  /* 139 */
#define NO_MODIFY_PRIVILEGES            0x898C  /* 140 */
#define SOME_FILES_AFFECTED_IN_USE      0x898D  /* 141 */
#define NO_FILES_AFFECTED_IN_USE        0x898E  /* 142 */
#define SOME_FILES_AFFECTED_READ_ONLY   0x898F  /* 143 */
#define NO_FILES_AFFECTED_READ_ONLY     0x8990  /* 144 */
#define SOME_FILES_RENAMED_NAME_EXISTS  0x8991  /* 145 */
#define NO_FILES_RENAMED_NAME_EXISTS    0x8992  /* 146 */
#define NO_READ_PRIVILEGES              0x8993  /* 147 */
#define NO_WRITE_PRIVILEGES_OR_READONLY 0x8994  /* 148 */
#define FILE_DETACHED                   0x8995  /* 149 */
#define SERVER_OUT_OF_MEMORY            0x8996  /* 150 */
#define ERR_TARGET_NOT_A_SUBDIRECTORY   0x8996  /* 150 can be changed later (note written by server people). */
#define NO_DISK_SPACE_FOR_SPOOL_FILE    0x8997  /* 151 */
#define ERR_AUDITING_NOT_ENABLED        0x8997  /* 151 */
#define VOLUME_DOES_NOT_EXIST           0x8998  /* 152 */
#define DIRECTORY_FULL                  0x8999  /* 153 */
#define RENAMING_ACROSS_VOLUMES         0x899A  /* 154 */
#define BAD_DIRECTORY_HANDLE            0x899B  /* 155 */
#define INVALID_PATH                    0x899C  /* 156 */
#define NO_MORE_TRUSTEES                0x899C  /* 156 */
#define NO_MORE_DIRECTORY_HANDLES       0x899D  /* 157 */
#define INVALID_FILENAME                0x899E  /* 158 */
#define DIRECTORY_ACTIVE                0x899F  /* 159 */
#define DIRECTORY_NOT_EMPTY             0x89A0  /* 160 */
#define DIRECTORY_IO_ERROR              0x89A1  /* 161 */
#define READ_FILE_WITH_RECORD_LOCKED    0x89A2  /* 162 */
#define ERR_TRANSACTION_RESTARTED       0x89A3  /* 163 */
#define ERR_RENAME_DIR_INVALID          0x89A4  /* 164 */
#define ERR_INVALID_OPENCREATE_MODE     0x89A5  /* 165 */
#define ERR_ALREADY_IN_USE              0x89A6  /* 166 */
#define ERR_AUDITING_ACTIVE             0x89A6  /* 166 */
#define ERR_INVALID_RESOURCE_TAG        0x89A7  /* 167 */
#define ERR_ACCESS_DENIED               0x89A8  /* 168 */
#define ERR_AUDITING_NO_RIGHTS          0x89A8  /* 168 */
#define INVALID_DATA_STREAM             0x89BE  /* 190 */
#define INVALID_NAME_SPACE              0x89BF  /* 191 */
#define NO_ACCOUNTING_PRIVILEGES        0x89C0  /* 192 */
#define LOGIN_DENIED_NO_ACCOUNT_BALANCE 0x89C1  /* 193 */
#define LOGIN_DENIED_NO_CREDIT          0x89C2  /* 194 */
#define ERR_AUDITING_RECORD_SIZE        0x89C2  /* 194 */
#define ERR_TOO_MANY_HOLDS              0x89C3  /* 195 */
#define ACCOUNTING_DISABLED             0x89C4  /* 196 */
#define INTRUDER_DETECTION_LOCK         0x89C5  /* 197 */
#define NO_CONSOLE_OPERATOR             0x89C6  /* 198 */
#define NO_CONSOLE_PRIVILEGES           0x89C6  /* 198 */
#define ERR_Q_IO_FAILURE                0x89D0  /* 208 */
#define ERR_NO_QUEUE                    0x89D1  /* 209 */
#define ERR_NO_Q_SERVER                 0x89D2  /* 210 */
#define ERR_NO_Q_RIGHTS                 0x89D3  /* 211 */
#define ERR_Q_FULL                      0x89D4  /* 212 */
#define ERR_NO_Q_JOB                    0x89D5  /* 213 */
#define ERR_NO_Q_JOB_RIGHTS             0x89D6  /* 214 */
#define ERR_Q_IN_SERVICE                0x89D7  /* 215 */
#define PASSWORD_NOT_UNIQUE             0x89D7  /* 215 */
#define ERR_Q_NOT_ACTIVE                0x89D8  /* 216 */
#define PASSWORD_TOO_SHORT              0x89D8  /* 216 */
#define ERR_Q_STN_NOT_SERVER            0x89D9  /* 217 */
#define LOGIN_DENIED_NO_CONNECTION      0x89D9  /* 217 */
#define ERR_MAXIMUM_LOGINS_EXCEEDED     0x89D9  /* 217 */
#define ERR_Q_HALTED                    0x89DA  /* 218 */
#define UNAUTHORIZED_LOGIN_TIME         0x89DA  /* 218 */
#define UNAUTHORIZED_LOGIN_STATION      0x89DB  /* 219 */
#define ERR_Q_MAX_SERVERS               0x89DB  /* 219 */
#define ACCOUNT_DISABLED                0x89DC  /* 220 */
#define PASSWORD_HAS_EXPIRED_NO_GRACE   0x89DE  /* 222 */
#define PASSWORD_HAS_EXPIRED            0x89DF  /* 223 */
#define E_NO_MORE_USERS                 0x89E7  /* 231 */
#define NOT_ITEM_PROPERTY               0x89E8  /* 232 */
#define WRITE_PROPERTY_TO_GROUP         0x89E8  /* 232 */
#define MEMBER_ALREADY_EXISTS           0x89E9  /* 233 */
#define NO_SUCH_MEMBER                  0x89EA  /* 234 */
#define NOT_GROUP_PROPERTY              0x89EB  /* 235 */
#define NO_SUCH_SEGMENT                 0x89EC  /* 236 */
#define PROPERTY_ALREADY_EXISTS         0x89ED  /* 237 */
#define OBJECT_ALREADY_EXISTS           0x89EE  /* 238 */
#define INVALID_NAME                    0x89EF  /* 239 */
#define WILD_CARD_NOT_ALLOWED           0x89F0  /* 240 */
#define INVALID_BINDERY_SECURITY        0x89F1  /* 241 */
#define NO_OBJECT_READ_PRIVILEGE        0x89F2  /* 242 */
#define NO_OBJECT_RENAME_PRIVILEGE      0x89F3  /* 243 */
#define NO_OBJECT_DELETE_PRIVILEGE      0x89F4  /* 244 */
#define NO_OBJECT_CREATE_PRIVILEGE      0x89F5  /* 245 */
#define NO_PROPERTY_DELETE_PRIVILEGE    0x89F6  /* 246 */
#define NO_PROPERTY_CREATE_PRIVILEGE    0x89F7  /* 247 */
#define NO_PROPERTY_WRITE_PRIVILEGE     0x89F8  /* 248 */
#define NO_FREE_CONNECTION_SLOTS        0x89F9  /* 249 */
#define NO_PROPERTY_READ_PRIVILEGE      0x89F9  /* 249 */
#define NO_MORE_SERVER_SLOTS            0x89FA  /* 250 */
#define TEMP_REMAP_ERROR                0x89FA  /* 250 */
#define INVALID_PARAMETERS              0x89FB  /* 251 */
#define NO_SUCH_PROPERTY                0x89FB  /* 251 */
#define ERR_NCP_NOT_SUPPORTED           0x89FB  /* 251 */
#define INTERNET_PACKET_REQT_CANCELED   0x89FC  /* 252 */
#define UNKNOWN_FILE_SERVER             0x89FC  /* 252 */
#define MESSAGE_QUEUE_FULL              0x89FC  /* 252 */
#define NO_SUCH_OBJECT                  0x89FC  /* 252 */
#define LOCK_COLLISION                  0x89FD  /* 253 */
#define BAD_STATION_NUMBER              0x89FD  /* 253 */
#define INVALID_PACKET_LENGTH           0x89FD  /* 253 */
#define UNKNOWN_REQUEST                 0x89FD  /* 253 */
#define BINDERY_LOCKED                  0x89FE  /* 254 */
#define TRUSTEE_NOT_FOUND               0x89FE  /* 254 */
#define DIRECTORY_LOCKED                0x89FE  /* 254 */
#define INVALID_SEMAPHORE_NAME_LENGTH   0x89FE  /* 254 */
#define PACKET_NOT_DELIVERABLE          0x89FE  /* 254 */
#define SERVER_BINDERY_LOCKED           0x89FE  /* 254 */
#define SOCKET_TABLE_FULL               0x89FE  /* 254 */
#define SPOOL_DIRECTORY_ERROR           0x89FE  /* 254 */
#define SUPERVISOR_HAS_DISABLED_LOGIN   0x89FE  /* 254 */
#define TIMEOUT_FAILURE                 0x89FE  /* 254 */
#define BAD_PRINTER_ERROR               0x89FF  /* 255 */
#define BAD_RECORD_OFFSET               0x89FF  /* 255 */
#define CLOSE_FCB_ERROR                 0x89FF  /* 255 */
#define FILE_EXTENSION_ERROR            0x89FF  /* 255 */
#define FILE_NAME_ERROR                 0x89FF  /* 255 */
#define HARDWARE_FAILURE                0x89FF  /* 255 */
#define INVALID_DRIVE_NUMBER            0x89FF  /* 255 */
#define DOS_INVALID_DRIVE               0x000F  /* 255 */
#define INVALID_INITIAL_SEMAPHORE_VALUE 0x89FF  /* 255 */
#define INVALID_SEMAPHORE_HANDLE        0x89FF  /* 255 */
#define IO_BOUND_ERROR                  0x89FF  /* 255 */
#define NO_FILES_FOUND_ERROR            0x89FF  /* 255 */
#define NO_RESPONSE_FROM_SERVER         0x89FF  /* 255 */
#define NO_SUCH_OBJECT_OR_BAD_PASSWORD  0x89FF  /* 255 */
#define PATH_NOT_LOCATABLE              0x89FF  /* 255 */
#define QUEUE_FULL_ERROR                0x89FF  /* 255 */
#define REQUEST_NOT_OUTSTANDING         0x89FF  /* 255 */
#ifndef SOCKET_ALREADY_OPEN
#define SOCKET_ALREADY_OPEN             0x89FF  /* 255 */
#endif
#define LOCK_ERROR                      0x89FF  /* 255 */
#ifndef FAILURE
#define FAILURE                         0x89FF  /* 255 Generic Failure */
#endif

/* #define NOT_SAME_LOCAL_DRIVE         0x89F6 */
/* #define TARGET_DRIVE_NOT_LOCAL       0x89F7 */
/* #define ALREADY_ATTACHED_TO_SERVER   0x89F8 */ /* 248 */
/* #define NOT_ATTACHED_TO_SERVER       0x89F8 */

/**** Network errors ****/
/* Decimal values at end of line are 32768 lower than actual */

#define NWE_ALREADY_ATTACHED            0x8800  /* 0  - Attach attempted to server with valid, existing connection */
#define NWE_CONN_INVALID                0x8801  /* 1  - Request attempted with invalid or non-attached connection handle */
#define NWE_DRIVE_IN_USE                0x8802  /* 2  - OS/2 only (NOT USED) */
#define NWE_DRIVE_CANNOT_MAP            0x8803  /* 3  - Map drive attempted but unable to add new current directory structure */
#define NWE_DRIVE_BAD_PATH              0x8804  /* 4  - Map drive attempted with invalid path specification */
#define NWE_NET_RECEIVE                 0x8805  /* 5  - Attempt to receive from the selected transport failed */
#define NWE_NET_UNKNOWN                 0x8806  /* 6  - Network send attempted with an un-specific network error */
#define NWE_SERVER_BAD_SLOT             0x8807  /* 7  - Server request attempted with invalid server connection slot */
#define NWE_SERVER_NO_SLOTS             0x8808  /* 8  - Attach attempted to server with no connection slots available */
#define NWE_NET_SEND                    0x8809  /* 9  - Attempt to send on the selected transport failed */
#define NWE_SERVER_NO_ROUTE             0x880A  /* 10 - Attempted to find route to server where no route exists */
#define NWE_BAD_LOCAL_TARGET            0x880B  /* 11 - OS/2 only */
#define NWE_REQ_TOO_MANY_REQ_FRAGS      0x880C  /* 12 - Attempted request with too many request fragments specified */
#define NWE_CONN_LIST_OVERFLOW          0x880D  /* 13 */
#define NWE_BUFFER_OVERFLOW             0x880E  /* 14 - Attempt to receive more data than the reply buffer had room for */
#define NWE_SERVER_NO_CONN              0x880F  /* 15 - Attempt to get connection for a server not connected */
#define NWE_NO_ROUTER_FOUND             0x8810  /* 16 - OS/2 only */
#define NWE_FUNCTION_INVALID            0x8811  /* 17 - Attempted function call to non- existent or illegal function */
#define NWE_SCAN_COMPLETE               0x8812
#define NWE_UNSUPPORTED_NAME_FORMAT_TYP 0x8813
#define NWE_HANDLE_ALREADY_LICENSED     0x8814
#define NWE_HANDLE_ALREADY_UNLICENSED   0x8815
#define NWE_INVALID_NCP_PACKET_LENGTH   0x8816
#define NWE_SETTING_UP_TIMEOUT          0x8817
#define NWE_SETTING_SIGNALS             0x8818
#define NWE_SERVER_CONNECTION_LOST      0x8819
#define NWE_OUT_OF_HEAP_SPACE           0x881A
#define NWE_INVALID_SERVICE_REQUEST     0x881B
#define NWE_INVALID_TASK_NUMBER         0x881C
#define NWE_INVALID_MESSAGE_LENGTH      0x881D
#define NWE_EA_SCAN_DONE                0x881E
#define NWE_BAD_CONNECTION_NUMBER       0x881F
#define NWE_MULT_TREES_NOT_SUPPORTED    0x8820  /* 32 - Attempt to open a connection to a DS tree other than the default tree */
#define NWE_CONN_NOT_SAME               0x8830  /* 48 - Internal server request attempted across different server connections */
#define NWE_CONN_PRIMARY_NOT_SET        0x8831  /* 49 - Attempt to retrieve default connection with no primary connection set */
#define NWE_PRN_CAPTURE_NOT_IN_PROGRESS 0x8832  /* 50 - Capture information requested on port with no capture in progress */
#define NWE_BUFFER_INVALID_LEN          0x8833  /* 51 - Used to indicate length which caller requested on a GetDNC or SetDNC was too large */
#define NWE_USER_NO_NAME                0x8834  /* 52 */
#define NWE_PRN_NO_LOCAL_SPOOLER        0x8835  /* 53 - Capture requested without having the local print spooler installed */
#define NWE_PARAM_INVALID               0x8836  /* 54 - Attempted function with an invalid function parameter specified */
#define NWE_CFG_OPEN_FAILED             0x8837  /* 55 - OS/2 only */
#define NWE_CFG_NO_FILE                 0x8838  /* 56 - OS/2 only */
#define NWE_CFG_READ_FAILED             0x8839  /* 57 - OS/2 only */
#define NWE_CFG_LINE_TOO_LONG           0x883A  /* 58 - OS/2 only */
#define NWE_CFG_LINES_IGNORED           0x883B  /* 59 - OS/2 only */
#define NWE_RESOURCE_NOT_OWNED          0x883C  /* 60 - Attempted request made with a parameter using foriegn resource */
#define NWE_DAEMON_INSTALLED            0x883D  /* 61 - OS/2 only */
#define NWE_PRN_SPOOLER_INSTALLED       0x883E  /* 62 - Attempted load of print spooler with print spooler already installed */
#define NWE_CONN_TABLE_FULL             0x883F  /* 63 - Attempted to allocate a connection handle with no more local connection table entries */
#define NWE_CFG_SECTION_NOT_FOUND       0x8840  /* 64 - OS/2 only */
#define NWE_TRAN_INVALID_TYPE           0x8841  /* 65 - Attempted function on a connection with an invalid transport selected */
#define NWE_TDS_TAG_IN_USE              0x8842  /* 66 - OS/2 only */
#define NWE_TDS_OUT_OF_MEMORY           0x8843  /* 67 - OS/2 only */
#define NWE_TDS_INVALID_TAG             0x8844  /* 68 - Attempted TDS function with invalid tag */
#define NWE_TDS_WRITE_TRUNCATED         0x8845  /* 69 - Attempted TDS write with buffer that exceeded buffer */
#define NWE_DS_NO_CONN                  0x8846  /* 70 */
#define NWE_SERVICE_BUSY                0x8846  /* 70 - Attempted request made to partially asynchronous function in busy state */
#define NWE_SERVER_NOT_FOUND            0x8847  /* 71 - Attempted connect failed to find any servers responding */
#define NWE_VLM_INVALID                 0x8848  /* 72 - Attempted function call to non-existant or not-loaded overlay */
#define NWE_DRIVE_ALREADY_MAPPED        0x8849  /* 73 - Attempted map to network drive that was already mapped */
#define NWE_DRIVE_LOCAL_IN_USE          0x884A  /* 74 - Attempted map to local drive that was in use */
#define NWE_DRIVE_NONE_AVAILABLE        0x884B  /* 75 - Attempted map to next available drive when none were available */
#define NWE_DEVICE_NOT_REDIRECTED       0x884C  /* 76 - The device is not redirected */
#define NWE_FILE_MAX_REACHED            0x884D  /* 77 - Maximum number of files was reached */
#define NWE_UNLOAD_FAILED               0x884E  /* 78 - Attempted unload failed */
#define NWE_CONN_IN_USE                 0x884F  /* 79 - Attempted re-use of already in use connection entry */
#define NWE_REQ_TOO_MANY_REP_FRAGS      0x8850  /* 80 - Attempted request with too many reply fragments specified */
#define NWE_NAME_TABLE_FULL             0x8851  /* 81 - Attempted to add a name into the name table after it was full */
#define NWE_SOCKET_NOT_OPEN             0x8852  /* 82 - Listen was posted on unopened socket */
#define NWE_MEMORY_MGR_ERROR            0x8853  /* 83 - Attempted enhanced memory operation failed */
#define NWE_SFT3_ERROR                  0x8854  /* 84 - An SFT3 switch occured mid-transfer */
#define NWE_DS_PREFERRED_NOT_FOUND      0x8855  /* 85 - the preferred directory server was not established but another directory server was returned */
#define NWE_DEVICE_NOT_RECOGNIZED       0x8856  /* 86 - used to determine if the device is not used by VISE so pass it on to the next redirector, if any. */
#define NWE_NET_INVALID_TYPE            0x8857  /* 87 - the network type (Bind/NDS) does not match the server version */
#define NWE_FILE_OPEN_FAILED            0x8858  /* 88 - generic open failure error, invalid path, access denied, etc.. */
#define NWE_DS_PREFERRED_NOT_SPECIFIED  0x8859  /* 89 - no preferred name specified */
#define NWE_SOCKET_OPEN_FAILED          0x885A  /* 90 - error opening a socket */
#define NWE_SIGNATURE_LEVEL_CONFLICT    0x8861
#define NWE_NO_LOCK_FOUND               0x8862  /* OS/2 - process lock on conn handle failed, process ID not recognized */
#define NWE_LOCK_TABLE_FULL             0x8863  /* OS/2 - process lock on conn handle failed, process lock table full */
#define NWE_INVALID_MATCH_DATA          0x8864
#define NWE_MATCH_FAILED                0x8865
#define NWE_NO_MORE_ENTRIES             0x8866
#define NWE_INSUFFICIENT_RESOURCES      0x8867
#define NWE_STRING_TRANSLATION          0x8868
#define NWE_ACCESS_VIOLATION            0x8869
#define NWE_NOT_AUTHENTICATED           0x886A
#define NWE_INVALID_LEVEL               0x886B
#define NWE_RESOURCE_LOCK               0x886C
#define NWE_INVALID_NAME_FORMAT         0x886D
#define NWE_OBJECT_EXISTS               0x886E
#define NWE_OBJECT_NOT_FOUND            0x886F
#define NWE_UNSUPPORTED_TRAN_TYPE       0x8870
#define NWE_INVALID_STRING_TYPE         0x8871
#define NWE_INVALID_OWNER               0x8872
#define NWE_UNSUPPORTED_AUTHENTICATOR   0x8873
#define NWE_IO_PENDING                  0x8874
#define NWE_INVALID_DRIVE_NUMBER        0x8875
#define NWE_REPLY_TRUNCATED            0x88e6    /* 230 NLM */
#define NWE_REQUESTER_FAILURE           0x88FF

/* Server Errors */

#define NWE_INSUFFICIENT_SPACE          0x8901  /* 001 */
#define NWE_BUFFER_TOO_SMALL            0x8977  /* 119 */
#define NWE_VOL_FLAG_NOT_SET            0x8978  /* 120 the service requested, not avail. on the selected vol. */
#define NWE_NO_ITEMS_FOUND              0x8979  /* 121 */
#define NWE_CONN_ALREADY_TEMP           0x897a  /* 122 */
#define NWE_CONN_ALREADY_LOGGED_IN      0x897b  /* 123 */
#define NWE_CONN_NOT_AUTHENTICATED      0x897c  /* 124 */
#define NWE_CONN_NOT_LOGGED_IN          0x897d  /* 125 */
#define NWE_NCP_BOUNDARY_CHECK_FAILED   0x897e  /* 126 */
#define NWE_LOCK_WAITING                0x897f  /* 127 */
#define NWE_LOCK_FAIL                   0x8980  /* 128 */
#define NWE_FILE_IN_USE                 0x8980  /* 128 */
#define NWE_FILE_NO_HANDLES             0x8981  /* 129 */
#define NWE_FILE_NO_OPEN_PRIV           0x8982  /* 130 */
#define NWE_DISK_IO_ERROR               0x8983  /* 131 */
#define NWE_AUDITING_HARD_IO_ERROR      0x8983  /* 131 */
#define NWE_FILE_NO_CREATE_PRIV         0x8984  /* 132 */
#define NWE_AUDITING_NOT_SUPV           0x8984  /* 132 */
#define NWE_FILE_NO_CREATE_DEL_PRIV     0x8985  /* 133 */
#define NWE_FILE_EXISTS_READ_ONLY       0x8986  /* 134 */
#define NWE_FILE_WILD_CARDS_IN_NAME     0x8987  /* 135 */
#define NWE_FILE_INVALID_HANDLE         0x8988  /* 136 */
#define NWE_FILE_NO_SRCH_PRIV           0x8989  /* 137 */
#define NWE_FILE_NO_DEL_PRIV            0x898A  /* 138 */
#define NWE_FILE_NO_RENAME_PRIV         0x898B  /* 139 */
#define NWE_FILE_NO_MOD_PRIV            0x898C  /* 140 */
#define NWE_FILE_SOME_IN_USE            0x898D  /* 141 */
#define NWE_FILE_NONE_IN_USE            0x898E  /* 142 */
#define NWE_FILE_SOME_READ_ONLY         0x898F  /* 143 */
#define NWE_FILE_NONE_READ_ONLY         0x8990  /* 144 */
#define NWE_FILE_SOME_RENAMED_EXIST     0x8991  /* 145 */
#define NWE_FILE_NONE_RENAMED_EXIST     0x8992  /* 146 */
#define NWE_FILE_NO_READ_PRIV           0x8993  /* 147 */
#define NWE_FILE_NO_WRITE_PRIV          0x8994  /* 148 */
#define NWE_FILE_READ_ONLY              0x8994  /* 148 */
#define NWE_FILE_DETACHED               0x8995  /* 149 */
#define NWE_SERVER_OUT_OF_MEMORY        0x8996  /* 150 */
#define NWE_DIR_TARGET_INVALID          0x8996  /* 150 */
#define NWE_DISK_NO_SPOOL_SPACE         0x8997  /* 151 */
#define NWE_AUDITING_NOT_ENABLED        0x8997  /* 151 */
#define NWE_VOL_INVALID                 0x8998  /* 152 */
#define NWE_DIR_FULL                    0x8999  /* 153 */
#define NWE_VOL_RENAMING_ACROSS         0x899A  /* 154 */
#define NWE_DIRHANDLE_INVALID           0x899B  /* 155 */
#define NWE_PATH_INVALID                0x899C  /* 156 */
#define NWE_TRUSTEES_NO_MORE            0x899C  /* 156 */
#define NWE_DIRHANDLE_NO_MORE           0x899D  /* 157 */
#define NWE_FILE_NAME_INVALID           0x899E  /* 158 */
#define NWE_DIR_ACTIVE                  0x899F  /* 159 */
#define NWE_DIR_NOT_EMPTY               0x89A0  /* 160 */
#define NWE_DIR_IO_ERROR                0x89A1  /* 161 */
#define NWE_FILE_IO_LOCKED              0x89A2  /* 162 */
#define NWE_TTS_RANSACTION_RESTARTED    0x89A3  /* 163 */
#define NWE_TTS_TRANSACTION_RESTARTED   0x89A3  /* 163 */
#define NWE_DIR_RENAME_INVALID          0x89A4  /* 164 */
#define NWE_FILE_OPENCREAT_MODE_INVALID 0x89A5  /* 165 */
#define NWE_ALREADY_IN_USE              0x89A6  /* 166 */
#define NWE_AUDITING_ACTIVE             0x89A6  /* 166 */
#define NWE_RESOURCE_TAG_INVALID        0x89A7  /* 167 */
#define NWE_ACCESS_DENIED               0x89A8  /* 168 */
#define NWE_AUDITING_NO_RIGHTS          0x89A8  /* 168 */
#define NWE_DATA_STREAM_INVALID         0x89BE  /* 190 */
#define NWE_NAME_SPACE_INVALID          0x89BF  /* 191 */
#define NWE_ACCTING_NO_PRIV             0x89C0  /* 192 */
#define NWE_ACCTING_NO_BALANCE          0x89C1  /* 193 */
#define NWE_ACCTING_NO_CREDIT           0x89C2  /* 194 */
#define NWE_AUDITING_RECORD_SIZE        0x89C2  /* 194 */
#define NWE_ACCTING_TOO_MANY_HOLDS      0x89C3  /* 195 */
#define NWE_ACCTING_DISABLED            0x89C4  /* 196 */
#define NWE_LOGIN_LOCKOUT               0x89C5  /* 197 */
#define NWE_CONSOLE_NO_PRIV             0x89C6  /* 198 */
#define NWE_Q_IO_FAILURE                0x89D0  /* 208 */
#define NWE_Q_NONE                      0x89D1  /* 209 */
#define NWE_Q_NO_SERVER                 0x89D2  /* 210 */
#define NWE_Q_NO_RIGHTS                 0x89D3  /* 211 */
#define NWE_Q_FULL                      0x89D4  /* 212 */
#define NWE_Q_NO_JOB                    0x89D5  /* 213 */
#define NWE_Q_NO_JOB_RIGHTS             0x89D6  /* 214 */
#define NWE_PASSWORD_UNENCRYPTED        0x89D6  /* 214 */
#define NWE_Q_IN_SERVICE                0x89D7  /* 215 */
#define NWE_PASSWORD_NOT_UNIQUE         0x89D7  /* 215 */
#define NWE_Q_NOT_ACTIVE                0x89D8  /* 216 */
#define NWE_PASSWORD_TOO_SHORT          0x89D8  /* 216 */
#define NWE_Q_STN_NOT_SERVER            0x89D9  /* 217 */
#define NWE_LOGIN_NO_CONN               0x89D9  /* 217 */
#define NWE_LOGIN_MAX_EXCEEDED          0x89D9  /* 217 */
#define NWE_Q_HALTED                    0x89DA  /* 218 */
#define NWE_LOGIN_UNAUTHORIZED_TIME     0x89DA  /* 218 */
#define NWE_LOGIN_UNAUTHORIZED_STATION  0x89DB  /* 219 */
#define NWE_Q_MAX_SERVERS               0x89DB  /* 219 */
#define NWE_ACCT_DISABLED               0x89DC  /* 220 */
#define NWE_PASSWORD_INVALID            0x89DE  /* 222 */
#define NWE_PASSWORD_EXPIRED            0x89DF  /* 223 */
#define NWE_LOGIN_NO_CONN_AVAIL         0x89E0  /* 224 */
#define NWE_E_NO_MORE_USERS             0x89E7  /* 231 */
#define NWE_BIND_NOT_ITEM_PROP          0x89E8  /* 232 */
#define NWE_BIND_WRITE_TO_GROUP_PROP    0x89E8  /* 232 */
#define NWE_BIND_MEMBER_ALREADY_EXISTS  0x89E9  /* 233 */
#define NWE_BIND_NO_SUCH_MEMBER         0x89EA  /* 234 */
#define NWE_BIND_NOT_GROUP_PROP         0x89EB  /* 235 */
#define NWE_BIND_NO_SUCH_SEGMENT        0x89EC  /* 236 */
#define NWE_BIND_PROP_ALREADY_EXISTS    0x89ED  /* 237 */
#define NWE_BIND_OBJ_ALREADY_EXISTS     0x89EE  /* 238 */
#define NWE_BIND_NAME_INVALID           0x89EF  /* 239 */
#define NWE_BIND_WILDCARD_INVALID       0x89F0  /* 240 */
#define NWE_BIND_SECURITY_INVALID       0x89F1  /* 241 */
#define NWE_BIND_OBJ_NO_READ_PRIV       0x89F2  /* 242 */
#define NWE_BIND_OBJ_NO_RENAME_PRIV     0x89F3  /* 243 */
#define NWE_BIND_OBJ_NO_DELETE_PRIV     0x89F4  /* 244 */
#define NWE_BIND_OBJ_NO_CREATE_PRIV     0x89F5  /* 245 */
#define NWE_BIND_PROP_NO_DELETE_PRIV    0x89F6  /* 246 */
#define NWE_BIND_PROP_NO_CREATE_PRIV    0x89F7  /* 247 */
#define NWE_BIND_PROP_NO_WRITE_PRIV     0x89F8  /* 248 */
#define NWE_BIND_PROP_NO_READ_PRIV      0x89F9  /* 249 */
#define NWE_NO_FREE_CONN_SLOTS          0x89F9  /* 249 */
#define NWE_NO_MORE_SERVER_SLOTS        0x89FA  /* 250 */
#define NWE_TEMP_REMAP_ERROR            0x89FA  /* 250 */
#define NWE_PARAMETERS_INVALID          0x89FB  /* 251 */
#define NWE_BIND_NO_SUCH_PROP           0x89FB  /* 251 */
#define NWE_NCP_NOT_SUPPORTED           0x89FB  /* 251 */
#define NWE_INET_PACKET_REQ_CANCELED    0x89FC  /* 252 */
#define NWE_SERVER_UNKNOWN              0x89FC  /* 252 */
#define NWE_MSG_Q_FULL                  0x89FC  /* 252 */
#define NWE_BIND_NO_SUCH_OBJ            0x89FC  /* 252 */
#define NWE_LOCK_COLLISION              0x89FD  /* 253 */
#define NWE_CONN_NUM_INVALID            0x89FD  /* 253 */
#define NWE_PACKET_LEN_INVALID          0x89FD  /* 253 */
#define NWE_UNKNOWN_REQ                 0x89FD  /* 253 */
#define NWE_BIND_LOCKED                 0x89FE  /* 254 */
#define NWE_TRUSTEE_NOT_FOUND           0x89FE  /* 254 */
#define NWE_DIR_LOCKED                  0x89FE  /* 254 */
#define NWE_SEM_INVALID_NAME_LEN        0x89FE  /* 254 */
#define NWE_PACKET_NOT_DELIVERABLE      0x89FE  /* 254 */
#define NWE_SOCKET_TABLE_FULL           0x89FE  /* 254 */
#define NWE_SPOOL_DIR_ERROR             0x89FE  /* 254 */
#define NWE_LOGIN_DISABLED_BY_SUPER     0x89FE  /* 254 */
#define NWE_TIMEOUT_FAILURE             0x89FE  /* 254 */
#define NWE_FILE_EXT                    0x89FF  /* 255 */
#define NWE_FILE_NAME                   0x89FF  /* 255 */
#define NWE_HARD_FAILURE                0x89FF  /* 255 */
#define NWE_FCB_CLOSE                   0x89FF  /* 255 */
#define NWE_IO_BOUND                    0x89FF  /* 255 */
#define NWE_BAD_SPOOL_PRINTER           0x89FF  /* 255 */
#define NWE_BAD_RECORD_OFFSET           0x89FF  /* 255 */
#define NWE_DRIVE_INVALID_NUM           0x89FF  /* 255 */
#define NWE_SEM_INVALID_INIT_VAL        0x89FF  /* 255 */
#define NWE_SEM_INVALID_HANDLE          0x89FF  /* 255 */
#define NWE_NO_FILES_FOUND_ERROR        0x89FF  /* 255 */
#define NWE_NO_RESPONSE_FROM_SERVER     0x89FF  /* 255 */
#define NWE_NO_OBJ_OR_BAD_PASSWORD      0x89FF  /* 255 */
#define NWE_PATH_NOT_LOCATABLE          0x89FF  /* 255 */
#define NWE_Q_FULL_ERROR                0x89FF  /* 255 */
#define NWE_REQ_NOT_OUTSTANDING         0x89FF  /* 255 */
#define NWE_SOCKET_ALREADY_OPEN         0x89FF  /* 255 */
#define NWE_LOCK_ERROR                  0x89FF  /* 255 */
#define NWE_FAILURE                     0x89FF  /* 255 Generic Failure */

#endif /* !_NWERROR_H_ */
