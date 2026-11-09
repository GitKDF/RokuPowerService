/****************************** Module Header ******************************\
* Module Name:  ServiceHandler.h
* Project:      RokuPowerService (derived from CppWindowsService)
* Copyright (c) Microsoft Corporation.
* Derivative work created by Kris Farrin, 2018
* 
* Provides a sample service class that derives from the service base class - 
* CServiceBase. The sample service logs the service start and stop 
* information to the Application event log, and shows how to run the main 
* function of the service in a thread pool worker thread.
* 
* This source is subject to the Microsoft Limited Public License v1.1.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#pragma once

#include "ServiceBase.h"
#include "WakeRoku.h"
#include <chrono>
#include <mutex>


class CServiceHandler : public CServiceBase
{
public:

    CServiceHandler(PWSTR pszServiceName, 
        BOOL fCanStop = TRUE, 
        BOOL fCanShutdown = TRUE, 
        BOOL fCanPauseContinue = FALSE);
    virtual ~CServiceHandler(void);

protected:

    virtual BOOL OnStart(DWORD dwArgc, LPWSTR *pszArgv);
    virtual void OnStop();
	virtual void OnPause();
	virtual void OnContinue();
	virtual DWORD WINAPI VirtualHandler(DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);

	void ServiceWorkerThread(void);
	
	HPOWERNOTIFY m_hPowerEvents;

private:

    HANDLE m_hStoppedEvent;
    std::atomic<bool> m_fStopping;
	std::atomic<bool> m_fPaused;

	int m_iPowerOffDelay;
	std::mutex m_TPMutex;
	std::chrono::system_clock::time_point m_tOffTime;
	std::chrono::system_clock::time_point m_tNullTime;

	WakeRoku m_WakeRoku;
};