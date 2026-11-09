/****************************** Module Header ******************************\
* Module Name:  ServiceInstaller.cpp
* Project:      RokuPowerService (derived from CppWindowsService)
* Copyright (c) Microsoft Corporation.
* 
* Derivative work created by Kris Farrin, 2018
* 
* The file implements functions that install and uninstall the service.
* 
* Function to retreive the Service Description added for RokuPowerService.
* Added functionality to the install service function to set a description.
* Added restart service function.
* 
* This source is subject to the Microsoft Limited Public License v1.1.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma region "Includes"
#include <stdio.h>
#include <windows.h>
#include "ServiceInstaller.h"
#pragma endregion

// RAII helper class to safely manage SC_HANDLE resources and prevent handle leaks
class ScHandleWrapper
{
private:
    SC_HANDLE m_handle;
public:
    explicit ScHandleWrapper(SC_HANDLE h = NULL) : m_handle(h) {}
    ~ScHandleWrapper() { if (m_handle) CloseServiceHandle(m_handle); }
    void reset(SC_HANDLE h = NULL) { if (m_handle) CloseServiceHandle(m_handle); m_handle = h; }
    SC_HANDLE get() const { return m_handle; }
    operator SC_HANDLE() const { return m_handle; }

    // Non-copyable
    ScHandleWrapper(const ScHandleWrapper&) = delete;
    ScHandleWrapper& operator=(const ScHandleWrapper&) = delete;
};


//
//   FUNCTION: InstallService
//
//   PURPOSE: Install the current application as a service to the local 
//   service control manager database.
//
//   PARAMETERS:
//   * pszServiceName - the name of the service to be installed
//   * pszDisplayName - the display name of the service
//   * dwStartType - the service start option. This parameter can be one of 
//     the following values: SERVICE_AUTO_START, SERVICE_BOOT_START, 
//     SERVICE_DEMAND_START, SERVICE_DISABLED, SERVICE_SYSTEM_START.
//   * pszDependencies - a pointer to a double null-terminated array of null-
//     separated names of services or load ordering groups that the system 
//     must start before this service.
//   * pszAccount - the name of the account under which the service runs.
//   * pszPassword - the password to the account name.
//
//   NOTE: If the function fails to install the service, it prints the error 
//   in the standard output stream for users to diagnose the problem.
//
void InstallService(PWSTR pszServiceName, 
                    PWSTR pszDisplayName, 
                    DWORD dwStartType,
                    PWSTR pszDependencies, 
                    PWSTR pszAccount, 
                    PWSTR pszPassword, PWSTR szDescription, SERVICE_FAILURE_ACTIONS* servFailActions)
{
    wchar_t szPath[MAX_PATH];

    if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)) == 0)
    {
        wprintf(L"GetModuleFileName failed w/err 0x%08lx\n", GetLastError());
        return;
    }

    // Open the local default service control manager database using RAII
    ScHandleWrapper schSCManager(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!schSCManager)
    {
		DWORD lastErr = GetLastError();
		if (0x5 == lastErr)
		{
			wprintf(L"Permission denied. (Try running as administrator.)  Error 0x%08lx\n", lastErr);
		}
		else wprintf(L"OpenSCManager failed w/err 0x%08lx\n", lastErr);
        return;
    }

#ifdef _DEBUG
	tryAgain:
#endif

    // Install the service into SCM by calling CreateService
    ScHandleWrapper schService(CreateService(
        schSCManager,                   // SCManager database
        pszServiceName,                 // Name of service
        pszDisplayName,                 // Name to display
        SERVICE_ALL_ACCESS,             // Desired access
        SERVICE_WIN32_OWN_PROCESS,      // Service type
        dwStartType,                    // Service start type
        SERVICE_ERROR_NORMAL,           // Error control type
        szPath,                         // Service's binary
        NULL,                           // No load ordering group
        NULL,                           // No tag identifier
        pszDependencies,                // Dependencies
        pszAccount,                     // Service running account
        pszPassword                     // Password of the account
    ));

    if (!schService)
    {
		DWORD lastErr = GetLastError();
		if (0x431 == lastErr)
		{
#ifdef _DEBUG
			UninstallService(pszServiceName);
			goto tryAgain;
#else
			wprintf(L"%s already installed.\n", pszServiceName);
#endif // _DEBUG
		}
		else
		{
			wprintf(L"CreateService failed w/err 0x%08lx\n", lastErr);
		}
		return;
    }

	if (NULL != szDescription)
	{
		SERVICE_DESCRIPTION sd;
		sd.lpDescription = szDescription;

		if (!ChangeServiceConfig2(
			schService,                 // handle to service
			SERVICE_CONFIG_DESCRIPTION, // change: description
			&sd))                      // new description
		{
			if (!DeleteService(schService)) wprintf(L"Error creating service, failed to remove service.  Error 0x%08lx\n", GetLastError());
		}
	}

	if (NULL != servFailActions)
	{
		if (!ChangeServiceConfig2(
			schService,					// handle to service
			SERVICE_CONFIG_FAILURE_ACTIONS,
			servFailActions))
		{
			wprintf(L"Error creating service failure actions; non-critical: service creation continues.  Error 0x%08lx\n", GetLastError());
		}
	}

    wprintf(L"%s is installed.\n", pszServiceName);

	if (!StartService(
		schService,  // handle to service 
		0,           // number of arguments 
		NULL))      // no arguments 
	{
		wprintf(L"StartService failed (%d)\n", GetLastError());
	}
	else wprintf(L"Starting service %s...\n", pszServiceName);
}


//
//   FUNCTION: UninstallService
//
//   PURPOSE: Stop and remove the service from the local service control 
//   manager database.
//
//   PARAMETERS: 
//   * pszServiceName - the name of the service to be removed.
//
//   NOTE: If the function fails to uninstall the service, it prints the 
//   error in the standard output stream for users to diagnose the problem.
//
void UninstallService(PWSTR pszServiceName)
{
    SERVICE_STATUS ssSvcStatus = {};

    // Open the local default service control manager database
    ScHandleWrapper schSCManager(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
    if (!schSCManager)
	{
		wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        return;
    }

    // Open the service with delete, stop, and query status permissions
    ScHandleWrapper schService(OpenService(schSCManager, pszServiceName, SERVICE_STOP | 
        SERVICE_QUERY_STATUS | DELETE));
    if (!schService)
    {
		DWORD lastErr = GetLastError();
		switch (lastErr)
		{
		case 0x424:
			wprintf(L"%s not installed.  Error 0x%08lx\n", pszServiceName, lastErr);
			break;
		case 0x5:
			wprintf(L"Permission denied. (Try running as administrator.)  Error 0x%08lx\n", lastErr);
			break;
		default:
        	wprintf(L"OpenService failed w/err 0x%08lx\n", lastErr);
			break;
		}
        return;
    }

    // Try to stop the service
    if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus))
    {
        wprintf(L"Stopping %s.", pszServiceName);
        Sleep(1000);

        while (QueryServiceStatus(schService, &ssSvcStatus))
        {
            if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING)
            {
                wprintf(L".");
                Sleep(1000);
            }
            else break;
        }

        if (ssSvcStatus.dwCurrentState == SERVICE_STOPPED)
        {
            wprintf(L"\n%s is stopped.\n", pszServiceName);
        }
        else
        {
            wprintf(L"\n%s failed to stop.\n", pszServiceName);
        }
    }

    // Now remove the service by calling DeleteService.
    if (!DeleteService(schService))
    {
        wprintf(L"DeleteService failed w/err 0x%08lx\n", GetLastError());
        return;
    }

    wprintf(L"%s is removed.\n", pszServiceName);
}



//
//   FUNCTION: RestartService
//
//   PURPOSE: Stop and restart the service from the local service control 
//   manager database.
//
//   PARAMETERS: 
//   * pszServiceName - the name of the service to be restarted.
//
//   Added by Kris Farrin for RokuPowerService
//
void RestartService(PWSTR pszServiceName)
{
	SERVICE_STATUS ssSvcStatus = {};

	// Open the local default service control manager database
	ScHandleWrapper schSCManager(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
	if (!schSCManager)
	{
		wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
		return;
	}

	// Open the service with delete, stop, and query status permissions
	ScHandleWrapper schService(OpenService(schSCManager, pszServiceName, SERVICE_STOP |
		SERVICE_QUERY_STATUS | SERVICE_START));
	if (!schService)
	{
		DWORD lastErr = GetLastError();
		switch (lastErr)
		{
		case 0x424:
			wprintf(L"%s not installed.  Error 0x%08lx\n", pszServiceName, lastErr);
			break;
		case 0x5:
			wprintf(L"Permission denied. (Try running as administrator.)  Error 0x%08lx\n", lastErr);
			break;
		default:
			wprintf(L"OpenService failed w/err 0x%08lx\n", lastErr);
			break;
		}
		return;
	}

	// Try to stop the service
	if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus))
	{
		wprintf(L"Stopping %s.", pszServiceName);
		Sleep(1000);

		while (QueryServiceStatus(schService, &ssSvcStatus))
		{
			if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING)
			{
				wprintf(L".");
				Sleep(1000);
			}
			else break;
		}

		if (ssSvcStatus.dwCurrentState == SERVICE_STOPPED)
		{
			wprintf(L"\n%s is stopped.\n", pszServiceName);
		}
		else
		{
			wprintf(L"\n%s failed to stop.\n", pszServiceName);
			return;
		}
	}

	// Try to start the service
	if (!StartService(schService, 0, NULL))
	{
		wprintf(L"StartService failed (%d)\n", GetLastError());
	}
	else wprintf(L"Starting service %s...\n", pszServiceName);
}



//
//   FUNCTION: GetServiceDescription
//
//   PURPOSE: Retreive the service description of a service.
//
//   PARAMETERS: 
//   * pszServiceName - the name of the service to be queried.
//
//   Added by Kris Farrin for RokuPowerService
wchar_t* GetServiceDescription(PWSTR pszServiceName) {

	LPSERVICE_DESCRIPTION lpsd = NULL;
	DWORD dwBytesNeeded = 0, cbBufSize = 0, dwError = 0;
	wchar_t* pszDescriptionResult = NULL;

	// Open the local default service control manager database using RAII
	ScHandleWrapper schSCManager(OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT));
	if (!schSCManager)
	{
		wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
		return NULL;
	}

	ScHandleWrapper schService(OpenService(schSCManager, pszServiceName, SERVICE_QUERY_CONFIG));
	if (!schService)
	{
		wprintf(L"OpenService failed w/err 0x%08lx\n", GetLastError());
		return NULL;
	}

	if (!QueryServiceConfig2(
		schService,
		SERVICE_CONFIG_DESCRIPTION,
		NULL,
		0,
		&dwBytesNeeded))
	{
		dwError = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == dwError)
		{
			cbBufSize = dwBytesNeeded;
			lpsd = (LPSERVICE_DESCRIPTION)LocalAlloc(LMEM_FIXED, cbBufSize);
			if (lpsd == NULL)
			{
				wprintf(L"LocalAlloc failed (%lu)\n", GetLastError());
				return NULL;
			}
		}
		else
		{
			wprintf(L"QueryServiceConfig2 failed (%lu)\n", dwError);
			return NULL;
		}
	}

	if (!QueryServiceConfig2(
		schService,
		SERVICE_CONFIG_DESCRIPTION,
		(LPBYTE)lpsd,
		cbBufSize,
		&dwBytesNeeded))
	{
		wprintf(L"QueryServiceConfig2 failed (%lu)\n", GetLastError());
		if (lpsd != NULL) LocalFree(lpsd);
		return NULL;
	}

	if (lpsd != NULL)
	{
		if (lpsd->lpDescription != NULL)
		{
			size_t len = wcslen(lpsd->lpDescription) + 1;
			pszDescriptionResult = new wchar_t[len];
			wcscpy_s(pszDescriptionResult, len, lpsd->lpDescription);
		}
		LocalFree(lpsd);
	}

	return pszDescriptionResult;
}