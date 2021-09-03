/*
 * WPA Supplicant / main() function for Win32 service
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * The root of wpa_supplicant configuration in registry is
 * HKEY_LOCAL_MACHINE\\SOFTWARE\\%wpa_supplicant. This level includes global
 * parameters and a 'interfaces' subkey with all the interface configuration
 * (adapter to confname mapping). Each such mapping is a subkey that has
 * 'adapter' and 'config' values.
 *
 * This program can be run either as a normal command line application, e.g.,
 * for debugging, with 'wpasvc.exe app' or as a Windows service. Service need
 * to be registered with 'wpasvc.exe reg <full path to wpasvc.exe>'. After
 * this, it can be started like any other Windows service (e.g., 'net start
 * wpasvc') or it can be configured to start automatically through the Services
 * tool in administrative tasks. The service can be unregistered with
 * 'wpasvc.exe unreg'.
 */

#include "includes.h"
#include <windows.h>

#include "common.h"
#include "wpa_supplicant_i.h"
#include "eloop.h"

#ifndef WPASVC_NAME
#define WPASVC_NAME TEXT("wpasvc")
#endif
#ifndef WPASVC_DISPLAY_NAME
#define WPASVC_DISPLAY_NAME TEXT("wpa_supplicant service")
#endif
#ifndef WPASVC_DESCRIPTION
#define WPASVC_DESCRIPTION \
TEXT("Provides IEEE 802.1X and WPA/WPA2 supplicant functionality")
#endif

static HANDLE kill_svc;

static SERVICE_STATUS_HANDLE svc_status_handle;
static SERVICE_STATUS svc_status;


#ifndef WPA_KEY_ROOT
#define WPA_KEY_ROOT HKEY_LOCAL_MACHINE
#endif
#ifndef WPA_KEY_PREFIX
#define WPA_KEY_PREFIX TEXT("SOFTWARE\\wpa_supplicant")
#endif

#ifdef UNICODE
#define TSTR "%S"
#else /* UNICODE */
#define TSTR "%s"
#endif /* UNICODE */


static int read_interface(struct wpa_global *global, HKEY _hk,
			  const TCHAR *name)
{
	HKEY hk;
#define TBUFLEN 255
	TCHAR adapter[TBUFLEN], config[TBUFLEN], ctrl_interface[TBUFLEN];
	DWORD buflen, val;
	LONG ret;
	struct wpa_interface iface;
	int skip_on_error = 0;

	ret = RegOpenKeyEx(_hk, name, 0, KEY_QUERY_VALUE, &hk);
	if (ret != ERROR_SUCCESS) {
		printf("Could not open wpa_supplicant interface key\n");
		return -1;
	}

	os_memset(&iface, 0, sizeof(iface));
	iface.driver = "ndis";

	buflen = sizeof(ctrl_interface);
	ret = RegQueryValueEx(hk, TEXT("ctrl_interface"), NULL, NULL,
			      (LPBYTE) ctrl_interface, &buflen);
	if (ret == ERROR_SUCCESS) {
		ctrl_interface[TBUFLEN - 1] = TEXT('\0');
		wpa_unicode2ascii_inplace(ctrl_interface);
		printf("ctrl_interface[len=%d] '%s'\n",
		       (int) buflen, (char *) ctrl_interface);
		iface.ctrl_interface = (char *) ctrl_interface;
	}

	buflen = sizeof(adapter);
	ret = RegQueryValueEx(hk, TEXT("adapter"), NULL, NULL,
			      (LPBYTE) adapter, &buflen);
	if (ret == ERROR_SUCCESS) {
		adapter[TBUFLEN - 1] = TEXT('\0');
		wpa_unicode2ascii_inplace(adapter);
		printf("adapter[len=%d] '%s'\n",
		       (int) buflen, (char *) adapter);
		iface.ifname = (char *) adapter;
	}

	buflen = sizeof(config);
	ret = RegQueryValueEx(hk, TEXT("config"), NULL, NULL,
			      (LPBYTE) config, &buflen);
	if (ret == ERROR_SUCCESS) {
		config[sizeof(config) - 1] = '\0';
		wpa_unicode2ascii_inplace(config);
		printf("config[len=%d] '%s'\n",
		       (int) buflen, (char *) config);
		iface.confname = (char *) config;
	}

	buflen = sizeof(val);
	ret = RegQueryValueEx(hk, TEXT("skip_on_error"), NULL, NULL,
			      (LPBYTE) &val, &buflen);
	if (ret == ERROR_SUCCESS && buflen == sizeof(val))
		skip_on_error = val;

	RegCloseKey(hk);

	if (wpa_supplicant_add_iface(global, &iface, NULL) == NULL) {
		if (skip_on_error)
			wpa_printf(MSG_DEBUG, "Skipped interface '%s' due to "
				   "initialization failure", iface.ifname);
		else
			return -1;
	}

	return 0;
}


static int wpa_supplicant_thread(void)
{
	int exitcode;
	struct wpa_params params;
	struct wpa_global *global;
	HKEY hk, ihk;
	DWORD val, buflen, i;
	LONG ret;

	if (os_program_init())
		return -1;

	os_memset(&params, 0, sizeof(params));
	params.wpa_debug_level = MSG_INFO;

	ret = RegOpenKeyEx(WPA_KEY_ROOT, WPA_KEY_PREFIX,
			   0, KEY_QUERY_VALUE, &hk);
	if (ret != ERROR_SUCCESS) {
		printf("Could not open wpa_supplicant registry key\n");
		return -1;
	}

	buflen = sizeof(val);
	ret = RegQueryValueEx(hk, TEXT("debug_level"), NULL, NULL,
			      (LPBYTE) &val, &buflen);
	if (ret == ERROR_SUCCESS && buflen == sizeof(val)) {
		params.wpa_debug_level = val;
	}

	buflen = sizeof(val);
	ret = RegQueryValueEx(hk, TEXT("debug_show_keys"), NULL, NULL,
			      (LPBYTE) &val, &buflen);
	if (ret == ERROR_SUCCESS && buflen == sizeof(val)) {
		params.wpa_debug_show_keys = val;
	}

	buflen = sizeof(val);
	ret = RegQueryValueEx(hk, TEXT("debug_timestamp"), NULL, NULL,
			      (LPBYTE) &val, &buflen);
	if (ret == ERROR_SUCCESS && buflen == sizeof(val)) {
		params.wpa_debug_timestamp = val;
	}

	buflen = sizeof(val);
	ret = RegQueryValueEx(hk, TEXT("debug_use_file"), NULL, NULL,
			      (LPBYTE) &val, &buflen);
	if (ret == ERROR_SUCCESS && buflen == sizeof(val) && val) {
		params.wpa_debug_file_path = "\\Temp\\wpa_supplicant-log.txt";
	}

	exitcode = 0;
	global = wpa_supplicant_init(&params);
	if (global == NULL) {
		printf("Failed to initialize wpa_supplicant\n");
		exitcode = -1;
	}

	ret = RegOpenKeyEx(hk, TEXT("interfaces"), 0, KEY_ENUMERATE_SUB_KEYS,
			   &ihk);
	RegCloseKey(hk);
	if (ret != ERROR_SUCCESS) {
		printf("Could not open wpa_supplicant interfaces registry "
		       "key\n");
		return -1;
	}

	for (i = 0; ; i++) {
		TCHAR name[255];
		DWORD namelen;

		namelen = 255;
		ret = RegEnumKeyEx(ihk, i, name, &namelen, NULL, NULL, NULL,
				   NULL);

		if (ret == ERROR_NO_MORE_ITEMS)
			break;

		if (ret != ERROR_SUCCESS) {
			printf("RegEnumKeyEx failed: 0x%x\n",
			       (unsigned int) ret);
			break;
		}

		if (namelen >= 255)
			namelen = 255 - 1;
		name[namelen] = '\0';

		wpa_printf(MSG_DEBUG, "interface %d: %s\n", (int) i, name);
		if (read_interface(global, ihk, name) < 0)
			exitcode = -1;
	}

	RegCloseKey(ihk);

	if (exitcode == 0)
		exitcode = wpa_supplicant_run(global);

	wpa_supplicant_deinit(global);

	os_program_deinit();

	return exitcode;
}


static DWORD svc_thread(LPDWORD param)
{
	int ret = wpa_supplicant_thread();

	svc_status.dwCurrentState = SERVICE_STOPPED;
	svc_status.dwWaitHint = 0;
	if (!SetServiceStatus(svc_status_handle, &svc_status)) {
		printf("SetServiceStatus() failed: %d\n",
		       (int) GetLastError());
	}

	return ret;
}


static int register_service(const TCHAR *exe)
{
	SC_HANDLE svc, scm;
	SERVICE_DESCRIPTION sd;

	printf("Registering service: " TSTR "\n", WPASVC_NAME);

	scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
	if (!scm) {
		printf("OpenSCManager failed: %d\n", (int) GetLastError());
		return -1;
	}

	svc = CreateService(scm, WPASVC_NAME, WPASVC_DISPLAY_NAME,
			    SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
			    SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
			    exe, NULL, NULL, NULL, NULL, NULL);

	if (!svc) {
		printf("CreateService failed: %d\n\n", (int) GetLastError());
		CloseServiceHandle(scm);
		return -1;
	}

	os_memset(&sd, 0, sizeof(sd));
	sd.lpDescription = WPASVC_DESCRIPTION;
	if (!ChangeServiceConfig2(svc, SERVICE_CONFIG_DESCRIPTION, &sd)) {
		printf("ChangeServiceConfig2 failed: %d\n",
		       (int) GetLastError());
		/* This is not a fatal error, so continue anyway. */
	}

	CloseServiceHandle(svc);
	CloseServiceHandle(scm);

	printf("Service registered successfully.\n");

	return 0;
}


static int unregister_service(void)
{
	SC_HANDLE svc, scm;
	SERVICE_STATUS status;

	printf("Unregistering service: " TSTR "\n", WPASVC_NAME);

	scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
	if (!scm) {
		printf("OpenSCManager failed: %d\n", (int) GetLastError());
		return -1;
	}

	svc = OpenService(scm, WPASVC_NAME, SERVICE_ALL_ACCESS | DELETE);
	if (!svc) {
		printf("OpenService failed: %d\n\n", (int) GetLastError());
		CloseServiceHandle(scm);
		return -1;
	}

	if (QueryServiceStatus(svc, &status)) {
		if (status.dwCurrentState != SERVICE_STOPPED) {
			printf("Service currently active - stopping "
			       "service...\n");
			if (!ControlService(svc, SERVICE_CONTROL_STOP,
					    &status)) {
				printf("ControlService failed: %d\n",
				       (int) GetLastError());
			}
			Sleep(500);
		}
	}

	if (DeleteService(svc)) {
		printf("Service unregistered successfully.\n");
	} else {
		printf("DeleteService failed: %d\n", (int) GetLastError());
	}

	CloseServiceHandle(svc);
	CloseServiceHandle(scm);

	return 0;
}


static void WINAPI service_ctrl_handler(DWORD control_code)
{
	switch (control_code) {
	case SERVICE_CONTROL_INTERROGATE:
		break;
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		svc_status.dwCurrentState = SERVICE_STOP_PENDING;
		svc_status.dwWaitHint = 2000;
		eloop_terminate();
		SetEvent(kill_svc);
		break;
	}

	if (!SetServiceStatus(svc_status_handle, &svc_status)) {
		printf("SetServiceStatus() failed: %d\n",
		       (int) GetLastError());
	}
}


static void WINAPI service_start(DWORD argc, LPTSTR *argv)
{
	DWORD id;

	svc_status_handle = RegisterServiceCtrlHandler(WPASVC_NAME,
						       service_ctrl_handler);
	if (svc_status_handle == (SERVICE_STATUS_HANDLE) 0) {
		printf("RegisterServiceCtrlHandler failed: %d\n",
		       (int) GetLastError());
		return;
	}

	os_memset(&svc_status, 0, sizeof(svc_status));
	svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	svc_status.dwCurrentState = SERVICE_START_PENDING;
	svc_status.dwWaitHint = 1000;

	if (!SetServiceStatus(svc_status_handle, &svc_status)) {
		printf("SetServiceStatus() failed: %d\n",
		       (int) GetLastError());
		return;
	}

	kill_svc = CreateEvent(0, TRUE, FALSE, 0);
	if (!kill_svc) {
		printf("CreateEvent failed: %d\n", (int) GetLastError());
		return;
	}

	if (CreateThread(0, 0, (LPTHREAD_START_ROUTINE) svc_thread, 0, 0, &id)
	    == 0) {
		printf("CreateThread failed: %d\n", (int) GetLastError());
		return;
	}

	if (svc_status.dwCurrentState == SERVICE_START_PENDING) {
		svc_status.dwCurrentState = SERVICE_RUNNING;
		svc_status.dwWaitHint = 0;
		svc_status.dwControlsAccepted = SERVICE_ACCEPT_STOP |
			SERVICE_ACCEPT_SHUTDOWN;
	}

	if (!SetServiceStatus(svc_status_handle, &svc_status)) {
		printf("SetServiceStatus() failed: %d\n",
		       (int) GetLastError());
		return;
	}

	/* wait until service gets killed */
	WaitForSingleObject(kill_svc, INFINITE);
}


int main(int argc, char *argv[])
{
	SERVICE_TABLE_ENTRY dt[] = {
		{ WPASVC_NAME, service_start },
		{ NULL, NULL }
	};

	if (argc > 1) {
		if (os_strcmp(argv[1], "reg") == 0) {
			TCHAR *path;
			int ret;

			if (argc < 3) {
				path = os_malloc(MAX_PATH * sizeof(TCHAR));
				if (path == NULL)
					return -1;
				if (!GetModuleFileName(NULL, path, MAX_PATH)) {
					printf("GetModuleFileName failed: "
					       "%d\n", (int) GetLastError());
					os_free(path);
					return -1;
				}
			} else {
				path = wpa_strdup_tchar(argv[2]);
				if (path == NULL)
					return -1;
			}
			ret = register_service(path);
			os_free(path);
			return ret;
		} else if (os_strcmp(argv[1], "unreg") == 0) {
			return unregister_service();
		} else if (os_strcmp(argv[1], "app") == 0) {
			return wpa_supplicant_thread();
		}
	}

	if (!StartServiceCtrlDispatcher(dt)) {
		printf("StartServiceCtrlDispatcher failed: %d\n",
		       (int) GetLastError());
	}

	return 0;
}
