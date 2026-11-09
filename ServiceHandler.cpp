/****************************** Module Header ******************************\
* Module Name:  ServiceHandler.cpp
* Project:      RokuPowerService (derived from CppWindowsService)
* Copyright (c) Microsoft Corporation.
*
* Derivative work created by Kris Farrin, 2018
* Changes noted in comments (search for RokuPowerService)
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

#pragma region Includes
#include "ServiceHandler.h"
#include "ServiceInstaller.h"
#include "ThreadPool.h"
#include "debugOutput.h"
#pragma endregion

CServiceHandler::CServiceHandler(PWSTR pszServiceName, 
                               BOOL fCanStop, 
                               BOOL fCanShutdown, 
                               BOOL fCanPauseContinue)
: CServiceBase(pszServiceName, fCanStop, fCanShutdown, fCanPauseContinue)
{
    m_fStopping = FALSE;
	
	//Added by Kris Farrin for RokuPowerService
	m_fPaused = FALSE;		//initialize Service Paused flag
	m_hPowerEvents = NULL;	//initialize handle to register for power events
		
	m_tNullTime = std::chrono::system_clock::now();	//initialize a time when starting the service to use as a "NULL" value

    // Create a manual-reset event that is not signaled at first to indicate 
    // the stopped signal of the service.
    m_hStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_hStoppedEvent == NULL)
    {
        throw GetLastError();
    }
}


CServiceHandler::~CServiceHandler(void)
{
    if (m_hStoppedEvent)
    {
        CloseHandle(m_hStoppedEvent);
        m_hStoppedEvent = NULL;
    }
}


//
//   FUNCTION: CServiceHandler::OnStart(DWORD, LPWSTR *)
//
//   PURPOSE: The function is executed when a Start command is sent to the 
//   service by the SCM or when the operating system starts (for a service 
//   that starts automatically). It specifies actions to take when the 
//   service starts. In this code sample, OnStart logs a service-start 
//   message to the Application log, and queues the main service function for 
//   execution in a thread pool worker thread.
//
//	 Modified by Kris Farrin for the RokuPowerService project to return a
//   BOOL value to allow the OnStart function to cancel the service start
//	 procedure if there was an error during intialization.  Added implementation
//   of RokuPowerService intialization.
//
//   PARAMETERS:
//   * dwArgc   - number of command line arguments
//   * lpszArgv - array of command line arguments
//
//   NOTE: A service application is designed to be long running. Therefore, 
//   it usually polls or monitors something in the system. The monitoring is 
//   set up in the OnStart method. However, OnStart does not actually do the 
//   monitoring. The OnStart method must return to the operating system after 
//   the service's operation has begun. It must not loop forever or block. To 
//   set up a simple monitoring mechanism, one general solution is to create 
//   a timer in OnStart. The timer would then raise events in your code 
//   periodically, at which time your service could do its monitoring. The 
//   other solution is to spawn a new thread to perform the main service 
//   functions, which is demonstrated in this code sample.
//
BOOL CServiceHandler::OnStart(DWORD dwArgc, LPWSTR *lpszArgv)
{
    // Log a service start message to the Application log.
    WriteEventLogEntry(L"RokuPowerService is starting.", EVENTLOG_INFORMATION_TYPE);

	if (!m_WakeRoku.WSAStarted)
	{
		WriteEventLogEntry(L"Could Not start WSA.  Stopping Service.\n", EVENTLOG_INFORMATION_TYPE);
		return FALSE;
	}

	debugOutput(L"Getting Service Description:\n");

	//Parse Service Description
	wchar_t* serviceDesc = GetServiceDescription(this->m_name);
	if (serviceDesc == NULL)
	{
		debugOutput(L"Error: Service description could not be retrieved.\n");
		return FALSE;
	}

	indentedDebugOutput(serviceDesc);
	debugOutput(L"\n");
	wchar_t* DeviceIP;
	wchar_t* SubnetMask;
	wchar_t* DeviceMAC;
	wchar_t* DelayTime;
	wchar_t* context;
	
	wcstok_s(serviceDesc, L"\n", &context);
	wcstok_s(NULL, L"\n", &context);

	DeviceIP = wcstok_s(NULL, L"\n", &context);
	if (DeviceIP == NULL || 15 < lstrlenW(DeviceIP)) {
		debugOutput(L"Error Parsing IP\n");
		delete[] serviceDesc;
		return FALSE;
	}
	debugOutput(L"Device IP: ", DeviceIP, L"\n");
	
	SubnetMask = wcstok_s(NULL, L"\n", &context);
	if (SubnetMask == NULL || 15 < lstrlenW(SubnetMask)) {
		debugOutput(L"Error Parsing Subnet Mask\n");
		delete[] serviceDesc;
		return FALSE;
	}
	debugOutput(L"Subnet Mask: ", SubnetMask, L"\n");
	
	DeviceMAC = wcstok_s(NULL, L"\n", &context);
	if (DeviceMAC == NULL || 17 != lstrlenW(DeviceMAC)) {
		debugOutput(L"Error Parsing MAC Address\n");
		delete[] serviceDesc;
		return FALSE;
	}
	debugOutput(L"Device MAC: ", DeviceMAC, L"\n");

	DelayTime = wcstok_s(NULL, L"\n", &context);
	if (NULL == DelayTime) {
		debugOutput(L"Error parsing off time value\n");
		delete[] serviceDesc;
		return FALSE;
	} else {
		m_iPowerOffDelay = std::stoi(DelayTime);
	}

	delete[] serviceDesc;

	if (!m_WakeRoku.Init(DeviceIP, SubnetMask, DeviceMAC))	{
		WriteEventLogEntry(L"Failed to initialize variables.  Stopping Service.", EVENTLOG_INFORMATION_TYPE);
		return FALSE;
	}


	//register with the OS to get message notifications regarding power events
	m_hPowerEvents = RegisterPowerSettingNotification(this->m_statusHandle, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_SERVICE_HANDLE);

	if (NULL == m_hPowerEvents)	{
		WriteEventLogEntry(L"Failed to register for power events.  Stopping Service.", EVENTLOG_INFORMATION_TYPE);
		return FALSE;
	} else {
		debugOutput(L"Registered for Power Events\n");
	}

	
	if (m_iPowerOffDelay > 0) {		//if there is a delayed time off, start the timer thread
		debugOutput(L"Starting worker thread\n");
		// Queue the main service function for execution in a worker thread
		CThreadPool::QueueUserWorkItem(&CServiceHandler::ServiceWorkerThread, this);
	} else {						//if the delay functionality is disabled
		SetEvent(m_hStoppedEvent);	//set the event that the worker thread is stopped, since we never start it
	}

	debugOutput(L"\n\n");

	return TRUE;
}


//
//   FUNCTION: CServiceHandler::ServiceWorkerThread(void)
//
//   PURPOSE: The method performs the main function of the service. It runs 
//   on a thread pool worker thread.
//
//   In the RokuPowerService project, this function is used as a timer to send
//	 power off events at a specific time.  We are also polling once a second to check
//   the flag indicating that the service should stop.
//
void CServiceHandler::ServiceWorkerThread(void)
{
	//initialize timepoint slightly less than m_iPowerOffDelay minutes, since we can't have a power off event in any less time
	std::chrono::system_clock::time_point NextTime = std::chrono::system_clock::now() + std::chrono::minutes(m_iPowerOffDelay) - std::chrono::seconds(10);

	// Only loop while the service is not stopping.
	while (!m_fStopping)
	{
		//if the service is paused or we're more than a second away from the next possible power off event
		debugOutput(L"\n");
		debugOutputTimestamp();
		if (m_fPaused || (NextTime > std::chrono::system_clock::now() + std::chrono::seconds(1))) {
			debugOutput(L"More than a second away, sleeping 1 second\n");
			Sleep(1000);		//sleep for a second
			continue;			//then restart the loop
			//this allows us to wait until the next possible power off event time
			//while still checking the m_fStopping and m_fPaused flags once a second
		}
		
		//we're only here if we're less than a second from the next possible event time
		debugOutput(L"Less than a second from NextTime; Checking off time\n");
		
		{
			std::lock_guard<std::mutex> guard(m_TPMutex);		//set lock guard
			if (m_tOffTime == m_tNullTime)						//if the off time is set to our "null" time...
			{
				debugOutput(L"Off time null, waiting ", m_iPowerOffDelay, L" minutes to check again\n");
				//set the next possible power off event time to m_iPowerOffDelay minutes from now
				NextTime = std::chrono::system_clock::now() + std::chrono::minutes(m_iPowerOffDelay);
				continue;		//restart our loop to keep checking m_fStopping every second
			}

			//if the off time is more than a second away
			if (m_tOffTime > std::chrono::system_clock::now() + std::chrono::seconds(1))
			{
				debugOutput(L"More than a second away, setting NextTime to off time\n");
				NextTime = m_tOffTime;		//set our next possible event time to the off time value
				continue;					//restart our loop to keep checking m_fStopping every second
			}
		}
		
		debugOutput(L"Offtime hasn't changed, sleeping until off time\n");
		//probably unnecessary since a second is frankly irrelevant on our practical timescale, but why not
		std::this_thread::sleep_until(NextTime);

		//there is some period of time before our off time that is < 1 second in length in which a monitor on event will not cancel the timed monitor off event.
		//by leaving the sleep (debug log info) and call to turn the roku off inside the mutex lock's scope, even if this occurs, the monitor on event handler
		//will be blocked from sending the monitor on command until we are done sending the monitor off command
		
		debugOutput(L"\n");
		debugOutputTimestamp();
		//debugOutput(L"\n");

		debugOutput(L"Power Off Timer Event\n");

		switch (m_WakeRoku.PowerOff()) {  //call routine to turn off the Roku and switch on output
		case 2:
			WriteEventLogEntry(L"Unable to connect to Roku via TCP.", EVENTLOG_ERROR_TYPE);
			break;
		case 1:
			WriteEventLogEntry(L"HTTP error connecting to Roku.", EVENTLOG_ERROR_TYPE);
			break;
		case 0:
			debugOutput(L"HTTP PowerOff keypress sent successfully\n");
			break;
		}

		debugOutput(L"Event complete, resetting timer and waiting ", m_iPowerOffDelay, L" minutes to check again\n");
		
		{
			std::lock_guard<std::mutex> guard(m_TPMutex);
			m_tOffTime = m_tNullTime;
			NextTime = m_tOffTime + std::chrono::minutes(m_iPowerOffDelay);
		}
	}

    // Signal the stopped event.
    SetEvent(m_hStoppedEvent);
}


//
//   FUNCTION: CServiceHandler::OnStop(void)
//
//   PURPOSE: The function is executed when a Stop command is sent to the 
//   service by SCM. It specifies actions to take when a service stops 
//   running. In this code sample, OnStop logs a service-stop message to the 
//   Application log, and waits for the finish of the main service function.
//
//   COMMENTS:
//   Be sure to periodically call ReportServiceStatus() with 
//   SERVICE_STOP_PENDING if the procedure is going to take long time. 
//
void CServiceHandler::OnStop()
{
    // Log a service stop message to the Application log.
    WriteEventLogEntry(L"RokuPowerService is stopping.", EVENTLOG_INFORMATION_TYPE);

	//Unregister for Power Setting Notifications if we successfully registered on start.
	if (NULL != m_hPowerEvents)
	{
		UnregisterPowerSettingNotification(m_hPowerEvents);
	}

    // Indicate that the service is stopping and wait for the finish of the 
    // main service function (ServiceWorkerThread).
    m_fStopping = TRUE;
    if (WaitForSingleObject(m_hStoppedEvent, INFINITE) != WAIT_OBJECT_0)
    {
        throw GetLastError();
    }
}


//
//   FUNCTION: CServiceHandler::OnPause()
//
//   PURPOSE: Sets a flag indicating that the service is paused.
//
//	 Modified by Kris Farrin for RokuPowerService.  Added flag to indicate that
//	 the service is paused.  Added implementation to pause RokuPowerService.
//
void CServiceHandler::OnPause()
{
	m_fPaused = TRUE;

	//pausing the service will cancel and timed off events which have not yet occurred
	std::lock_guard<std::mutex> guard(m_TPMutex);		//set lock guard
	m_tOffTime = m_tNullTime;							//set off time to our "NULL" value
}


//
//   FUNCTION: CServiceHandler::OnContinue()
//
//   PURPOSE: Sets a flag indicating that the service is no longer paused.
//
//	 Modified by Kris Farrin for RokuPowerService.  Added flag to indicate that
//	 the service is unpaused.  Added implementation to unpause RokuPowerService.
//
void CServiceHandler::OnContinue()
{
	m_fPaused = FALSE;
}


//
//   FUNCTION: CServiceHandler::VirtualHandler(DWORD)
//
//   PURPOSE: Determines if the event message is a relevant message to turn the
//   display power on, and if so sends a PowerOn keypress message via HTTP to
//   the RokuTV.
//   Added by Kris Farrin as part of the RokuPowerService project
//
DWORD WINAPI CServiceHandler::VirtualHandler(DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	if (!m_fPaused) //pass on control if the service is paused
	{
		if (SERVICE_CONTROL_POWEREVENT == dwCtrl)  //is the message a power event?
		{
			debugOutput(L"\n");
			debugOutputTimestamp();
			//debugOutput(L"\n");

			debugOutput(L"\tPower Event\n");
			if (PBT_POWERSETTINGCHANGE == dwEventType)  //is the event type a power setting change?
			{
				debugOutput(L"\tPower Setting Change\n");
				POWERBROADCAST_SETTING* pps = (POWERBROADCAST_SETTING*)lpEventData;  //get the data passed with the event

				if (sizeof(DWORD) == pps->DataLength && pps->PowerSetting == GUID_CONSOLE_DISPLAY_STATE)  //is it a change to the console display state?
				{
					debugOutput(L"\tConsole Display State\n");
					DWORD nMonitorState = *pps->Data;
					
					if (1 == nMonitorState)
					{
						//monitor is commanded on
						debugOutput(L"\tMonitor On\n");

						{
							std::lock_guard<std::mutex> guard(m_TPMutex);	//set lock guard
							if (m_tOffTime != m_tNullTime) {				//if there is currently a delayed off event set
								debugOutput(L"\tResetting Power Off Timer\n");
								m_tOffTime = m_tNullTime;					//set offtime to our "NULL" time to cancel it
							}
						}
						
						//call routine to wake the Roku and switch on output
						switch (m_WakeRoku.PowerOn()) {
							case 2:		//there was a socket error
								WriteEventLogEntry(L"Unable to connect to Roku via TCP.", EVENTLOG_ERROR_TYPE);
								break;
							case 1:		//there was an http error
								WriteEventLogEntry(L"HTTP error connecting to Roku.", EVENTLOG_ERROR_TYPE);
								break;
							case 0:		//successful
								debugOutput(L"\tHTTP PowerOn keypress sent successfully\n");
								break;
						}
					}
					else if (0 == nMonitorState)
					{
						//monitor is commanded off
						debugOutput(L"\tMonitor Off\n");

						switch (m_iPowerOffDelay) {
							case -1:			//if off time delay is -1, off commands are disabled
								break;				//do nothing
							case 0: {			//if off time delay is 0, turn monitor off immediately
								switch (m_WakeRoku.PowerOff()) {  //call routine to turn off the Roku
									case WR_SOCKET_ERROR:		//there was a socket error
										WriteEventLogEntry(L"Unable to connect to Roku via TCP.", EVENTLOG_ERROR_TYPE);
										break;
									case WR_HTTP_ERROR:		//there was an http error
										WriteEventLogEntry(L"HTTP error connecting to Roku.", EVENTLOG_ERROR_TYPE);
										break;
									case WR_SUCCESS:		//successful
										debugOutput(L"\tHTTP PowerOff keypress sent successfully\n");
										break;
								}
								break;
							}
							default: {
								debugOutput(L"\tPower Off Timer set for ", m_iPowerOffDelay, " minutes\n");
								std::lock_guard<std::mutex> guard(m_TPMutex);
								m_tOffTime = std::chrono::system_clock::now() + std::chrono::minutes(m_iPowerOffDelay);
							}
						}
					}
				}
			}
		}
	}
	return NO_ERROR;
}