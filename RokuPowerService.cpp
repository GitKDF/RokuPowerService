/****************************** Module Header ******************************\
* Module Name:  RokuPowerService.cpp
* Project:      RokuPowerService (derived from CppWindowsService)
* Copyright (c) Microsoft Corporation.
* 
* Derivative work created by Kris Farrin, 2018
*
* The file defines the entry point of the application. According to the 
* arguments in the command line, the function installs or uninstalls or 
* starts the service by calling into different routines.
* 
* This source is adapted from code subject to the Microsoft Limited Public License v1.1.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
*
* Original source from MSDN:'A basic Windows service in C++ (CppWindowsService)'
* https://code.msdn.microsoft.com/windowsapps/CppWindowsService-cacf4948
* 
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

// RokuPowerService -install 172.16.1.40 255.255.0.0 1c:1e:e3:f8:b7:3d
// RokuPowerService -install 192.168.86.34 255.255.255.0 1c:1e:e3:f8:b7:3d


#pragma region Includes
//enable logging in release build
#ifndef _DEBUG
#define _DEBUG
#endif

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "ServiceInstaller.h"
#include "ServiceHandler.h"
#include "debugOutput.h"
// ... rest of your includes#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include "ServiceInstaller.h"
#include "ServiceHandler.h"
#include "debugOutput.h"

#ifdef _DEBUG
	std::wofstream out;
	#include <Wtsapi32.h>
	#include <Shlwapi.h>
	#pragma comment( lib, "Wtsapi32.lib" )
	#pragma comment( lib, "Shlwapi.lib" )
#endif
#pragma endregion

// 
// Settings of the service
// 

// Internal name of the service
#define SERVICE_NAME             L"RokuPowerService"

// Displayed name of the service
#define SERVICE_DISPLAY_NAME     L"Roku Power Service"

// Displayed name of the service
#define SERVICE_DESCRIPTION      L"Allows a Roku TV used as a monitor to be powered on by the Windows Console Display State\nConfiguration Settings:\n"

// Current Version
#define CURRENT_VERSION			 L"5.0"

// Service start options.
#define SERVICE_START_TYPE       SERVICE_AUTO_START

// List of service dependencies - "dep1\0dep2\0\0"
#define SERVICE_DEPENDENCIES     L""

// The name of the account under which the service should run
#define SERVICE_ACCOUNT          L"NT AUTHORITY\\System"

// The password to the service account name
#define SERVICE_PASSWORD         NULL

// Default setting if no power off delay specified
#define DEFAULT_POWER_OFF_DELAY	14

#define PARAMETER_TEXT			 L"\n\
\t\t    RokuPowerService " CURRENT_VERSION "\n\
\t\t----====================----\n\
\n\
\tParameters:\n\
\t-----------\n\
\t-install <RokuIP> <Subnet Mask> <MAC Address> [Delay Time]\n\
\t-restart\n\
\t-remove\n\
\t-help\n\
\n\
\t<RokuIP>      is the IPv4 Address (xxx.xxx.xxx.xxx) of the RokuTV.\n\
\t              (Must be fixed in your router's DHCP settings.)\n\
\t<Subnet Mask> is the Subnet Mask for your Local Network.\n\
\t<MAC Address> is the MAC Address of the TV (Wired or Wireless,\n\
\t              whichever you are using) in the form XX:XX:XX:XX:XX:XX\n\
\t[Delay Time]  is an optional delay time to turn the TV Off.\n\
\t              Range is -1 to 99.  Set to -1 to disable,\n\
\t              0 to turn off immediately, or 1-99 for delay in minutes.\n\
\t              Default if omitted is 14 minutes.\n\
\n\
\tNote that service installation and removal\n\
\toperations must be run as administrator.\n"
        
#define HELP_TEXT				 L"\n\
    Details\n\
 --=========--\n\
Your Roku must be set up to have a fixed IP address in your router's DHCP\n\
settings.  Most modern routers allow assignment of a hardware MAC address to\n\
a specific IP address.  The MAC address of your Roku can be found under\n\
Settings -> System -> About; if your TV has both wireless and wired network\n\
connections, use the MAC address of the one you are connecting to your\n\
network, for both the DHCP assignment and RokuPowerService setup.\n\
\n\
To find your Subnet Mask, run IPConfig from a command prompt and note the\n\
Subnet Mask for your local network adapter.  This will typically be\n\
255.255.255.0 for 192.168.x.x networks, or 255.255.0.0 for 172.16.x.x networks.\n\
In the worst case, defaulting to 255.0.0.0 should generally work.\n\
\n\
Default delay time is 14 minutes to turn monitor off just as the message\n\
that it is powering down in 60 seconds comes on the screen if you have the\n\
\"Reduce Power after 15 minutes\" option enabled on the TV.  Entering 0\n\
will turn the TV off immediately when the PC sends a monitor off event.\n\
Entering -1 will disable off commands completely.\n\
\n\
     About\n\
 --=========--\n\
This will install a service which will send PowerOn/Off commands to your\n\
RokuTV when a VGA/DVI monitor would change power states via VESA DPMS\n\
commands.  Eventually display adapter manufacturers may implement CEC to\n\
control HDMI devices when they see that TVs being used as monitors is\n\
becoming more common with the introduction of fantastic, low latency,\n\
4k 60hz 4:4:4 displays at prices lower than many smaller, lower resolution\n\
dedicated monitors.\n\
\n\
To quickly enable or disable this power signaling from your PC (e.g. if using\n\
the TV for non-computer purposes) you can set up two shortcuts on your PC,\n\
which will need to be set to run as administrator:\n\
\t Disable: sc pause RokuPowerService\n\
\t Enable:  sc continue RokuPowerService\n\
\n\
   Changelog\n\
 --=========--\n\
Version 1.0: Basic functionality established by registering for and\n\
             interpreting power events and using external curl.exe utility\n\
             to send PowerOn signal to TV.\n\
\n\
Version 2.0: Added internal HTTP/TCP Socket functionality.\n\
\n\
Version 3.0: Added Wake-On-LAN support to allow disabling of 'Fast TV start'\n\
             and enabling of 'Turn off after 4 hours' on the TV.  Time from\n\
             pressing a key to lock screen displayed is about 8 seconds from\n\
             a full sleep state.\n\
\n\
Version 3.1: Added loop to send up to 4 PowerOn keypresses if the response is\n\
             'HTTP 202 Accepted'.  This happens occasionally when the TV is\n\
             powering up from a sleep state, and although the HTTP request is\n\
             accepted, the TV would occasionally never turn on with just one.\n\
\n\
Version 4.0: General bug fixes / improved error handling / code refactoring.\n\
             Added option to turn Roku TV Off when monitor is commanded off,\n\
             with an optional delay time.\n\
\n\
Version 5.0: Added interactive setup assistant when run directly without\n\
             arguments, featuring automatic self-copy to Program Files\n\
             and robust IP/MAC input validation. Modernized codebase\n\
             with C++17 standards, RAII handle safety, thread-safe synchronization,\n\
             and native Windows IP parsing APIs."

//
//  FUNCTION: wmain(int, wchar_t *[])
//
//  PURPOSE: entrypoint for the application.
// 
//  PARAMETERS:
//    argc - number of command line arguments
//    argv - array of command line arguments
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//    wmain() either performs the command line task, or run the service.
//
int wmain(int argc, wchar_t *argv[])
{
	// if we're compiling a debug version, enable verbose debug output to a logfile in the same
	// directory as our executable.  This is handy to let the thing run for a while and review
	// the log later.  because debugging a service is a pain, and
	//I was able to use this to get things running fine.  I also spent a few years using a
	//proto-C scripting language that had no debugging functionality, and setting a global
	//boolean and using a debugOutput subroutine was the only option, so I'm used to it.
	#ifdef _DEBUG
		wchar_t workingPath[MAX_PATH];  //WCHAR filename buffer
		GetModuleFileName(NULL, workingPath, MAX_PATH);  //get the binary's filename and path
		std::wstring::size_type pos = std::wstring(workingPath).find_last_of(L"\\/");  //split the path from filename
		std::wstring tmpFilePath = std::wstring(workingPath).substr(0, pos) + L"\\DebugOutput.txt";  //build new filename and path

		out = std::wofstream(tmpFilePath);  //new stream to our DebugOutput.txt file
		std::wcout.rdbuf(out.rdbuf()); //redirect std::cout to output text file
	#endif

    if ((argc > 1) && ((*argv[1] == L'-' || (*argv[1] == L'/'))))
    {
        if (_wcsicmp(L"install", argv[1] + 1) == 0)
        {
			// Install the service when the command is 
			// "-install" or "/install".
			if ((5 == argc) || (6 == argc))  //ensure there is a serial number
			{
				WakeRoku wrCheckParameters;
				if (wrCheckParameters.Init(argv[2], argv[3], argv[4]))	//validate user provided IP, SNMask, and Mac
				{
					std::wstring wstr;			//build string with Description followed by IP, SNMask, and Mac separated by newlines
					wstr = SERVICE_DESCRIPTION;
					wstr += argv[2];
					wstr += L"\n";
					wstr += argv[3];
					wstr += L"\n";
					wstr += argv[4];
					wstr += L"\n";

					if (6 == argc)
					{
						int timeOutValue = 1;
						int onesDigit;
						int tensDigit;
						int length = lstrlen(argv[5]);
						int offset = 0;

						if (L'-' == argv[5][0]) {
							if ((length == 2) && (argv[5][1] == L'1')) {
								timeOutValue = -1;
								length = 0;			//set so that following switch will simply break
							} else {
								length = -1;		//this will cause us to fall through the following switch to the default and show the error message
							}
						}
						switch (length) {
						case 0:
							break;
						case 1:
							onesDigit = argv[5][offset] - L'0';
							if ((onesDigit >= 0) && (onesDigit <= 9))
							{
								timeOutValue *= onesDigit;
								break;
							}
						case 2:
							tensDigit = argv[5][offset] - L'0';
							if ((tensDigit >= 0) && (tensDigit <= 9))
							{
								onesDigit = argv[5][offset + 1] - L'0';
								if ((onesDigit >= 0) && (onesDigit <= 9)) {
									timeOutValue *= ((tensDigit * 10) + onesDigit);
									break;
								}
							}
						default:
							wprintf(L"\nError: Could not parse off time value\n");
							debugOutput(L"Error parsing off time value\n");
							goto invalidInput;
						}
						wstr += std::to_wstring(timeOutValue);
					}
					else
					{
						wstr += std::to_wstring(DEFAULT_POWER_OFF_DELAY);
					}

					SERVICE_FAILURE_ACTIONS servFailActions;
					SC_ACTION failActions[3];

					failActions[0].Type = SC_ACTION_RESTART; //Failure action: Restart Service
					failActions[0].Delay = 60000; //number of seconds to wait before performing failure action, in milliseconds = 2minutes
					failActions[1].Type = SC_ACTION_RESTART;
					failActions[1].Delay = 60000;
					failActions[2].Type = SC_ACTION_NONE;
					failActions[2].Delay = 60000;

					servFailActions.dwResetPeriod = 86400; // Reset Failures Counter, in Seconds = 1day
					servFailActions.lpCommand = NULL; //Command to perform due to service failure, not used
					servFailActions.lpRebootMsg = NULL; //Message during rebooting computer due to service failure, not used
					servFailActions.cActions = 3; // Number of failure action to manage
					servFailActions.lpsaActions = failActions;

					InstallService(
						SERVICE_NAME,               // Name of service
						SERVICE_DISPLAY_NAME,       // Name to display
						SERVICE_START_TYPE,         // Service start type
						SERVICE_DEPENDENCIES,       // Dependencies
						SERVICE_ACCOUNT,            // Service running account
						SERVICE_PASSWORD,           // Password of the account
						&wstr[0],					
						&servFailActions
						);
				}
				else
				{
					goto invalidInput;
				}
			}
			else
			{
				goto invalidInput;
			}
        }
        else if (_wcsicmp(L"remove", argv[1] + 1) == 0)
        {
            // Uninstall the service when the command is 
            // "-remove" or "/remove".
            UninstallService(SERVICE_NAME);
        }
        else if (_wcsicmp(L"restart", argv[1] + 1) == 0)
        {
            // Uninstall the service when the command is 
            // "-restart" or "/restart".
            RestartService(SERVICE_NAME);
        }
		else if ((_wcsicmp(L"help", argv[1] + 1) == 0) || (_wcsicmp(L"?", argv[1] + 1) == 0))
        {
			wprintf(HELP_TEXT);
		}
    }
    else
    {
		wprintf(PARAMETER_TEXT);

		debugOutput(L"Starting Service\n");

		#ifdef _DEBUG
		{
			WCHAR title[MAX_PATH + 40];
			WCHAR* pszFileName;
			GetModuleFileName(NULL, title, MAX_PATH);
			pszFileName = PathFindFileName(title);
			swprintf_s(title, MAX_PATH + 40, L"%s in startup - 60 seconds to take action", pszFileName);

			DWORD PID = GetCurrentProcessId();
			wchar_t message[200];
			swprintf_s(message, 200, L"To debug, first attach to the process with Visual "
				L"Studio, then click OK. If you don't want to debug, "
				L"just click OK without attaching\n\nProcess ID: %lu", PID);

			DWORD consoleSession = ::WTSGetActiveConsoleSessionId();
			DWORD response;
			BOOL ret = ::WTSSendMessage(WTS_CURRENT_SERVER_HANDLE,
				consoleSession,
				title, sizeof(title),
				message, sizeof(message),
				MB_OK,
				60,
				&response,
				TRUE);
		}
		#endif


		CServiceHandler service(SERVICE_NAME, TRUE, TRUE, TRUE);
        if (!CServiceBase::Run(service))
        {
            DWORD err = GetLastError();
            // Error 1063 (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) means the app 
            // was launched interactively by a user, or run with no arguments.
            if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT || argc == 1)
            {
                wprintf(L"\n==================================================\n");
                wprintf(L"        RokuPowerService - Interactive Setup        \n");
                wprintf(L"==================================================\n\n");
                wprintf(L"This application runs as a background Windows Service.\n");
                wprintf(L"It was launched directly rather than by the Service Manager.\n\n");
                wprintf(L"Would you like to install this service now? (Y/N): ");

                wchar_t input[16];
                if (_getws_s(input) && (input[0] == L'y' || input[0] == L'Y'))
                {
                    wchar_t ip[64], subnet[64], mac[64], delay[16] = L"14";

                    // Validation loop: keeps asking until valid IP, Subnet, and MAC are entered
                    while (true)
                    {
                        wprintf(L"\nEnter Roku IPv4 Address (e.g., 192.168.1.50): ");
                        _getws_s(ip);

                        wprintf(L"Enter Subnet Mask (e.g., 255.255.255.0): ");
                        _getws_s(subnet);

                        wprintf(L"Enter MAC Address (e.g., 1c:1e:e3:f8:b7:3d): ");
                        _getws_s(mac);

                        wprintf(L"Enter Power-Off Delay in minutes (-1 disable, 0 immediate, default 14): ");
                        _getws_s(delay);
                        if (wcslen(delay) == 0) wcscpy_s(delay, L"14");

                        // Test validation using WakeRoku::Init
                        WakeRoku validator;
                        if (validator.Init(ip, subnet, mac))
                        {
                            break; // Inputs are valid, proceed out of loop
                        }
                        wprintf(L"\n[Error] Invalid IP, Subnet Mask, or MAC Address format. Please try again.\n");
                    }

                    // Auto-copy to Program Files for a permanent installation home
                    wchar_t destDir[MAX_PATH];
                    ExpandEnvironmentStringsW(L"%ProgramFiles%\\RokuPowerService", destDir, MAX_PATH);
                    CreateDirectoryW(destDir, NULL);

                    wchar_t currentPath[MAX_PATH];
                    GetModuleFileNameW(NULL, currentPath, MAX_PATH);

                    wchar_t destPath[MAX_PATH];
                    swprintf_s(destPath, MAX_PATH, L"%s\\RokuPowerService.exe", destDir);

                    if (_wcsicmp(currentPath, destPath) != 0)
                    {
                        if (CopyFileW(currentPath, destPath, FALSE))
                        {
                            wprintf(L"\n[Success] Copied executable to: %s\n", destPath);
                        }
                        else
                        {
                            wprintf(L"\n[Note] Could not copy to Program Files (Administrator rights required). Running from current location.\n");
                        }
                    }

                    // Build configuration description string
                    std::wstring wstr = SERVICE_DESCRIPTION;
                    wstr += ip;     wstr += L"\n";
                    wstr += subnet; wstr += L"\n";
                    wstr += mac;    wstr += L"\n";
                    wstr += delay;  wstr += L"\n";

                    SERVICE_FAILURE_ACTIONS servFailActions;
                    SC_ACTION failActions[3];

                    failActions[0].Type = SC_ACTION_RESTART;
                    failActions[0].Delay = 60000;
                    failActions[1].Type = SC_ACTION_RESTART;
                    failActions[1].Delay = 60000;
                    failActions[2].Type = SC_ACTION_NONE;
                    failActions[2].Delay = 60000;

                    servFailActions.dwResetPeriod = 86400;
                    servFailActions.lpCommand = NULL;
                    servFailActions.lpRebootMsg = NULL;
                    servFailActions.cActions = 3;
                    servFailActions.lpsaActions = failActions;

                    InstallService(
                        SERVICE_NAME,
                        SERVICE_DISPLAY_NAME,
                        SERVICE_START_TYPE,
                        SERVICE_DEPENDENCIES,
                        SERVICE_ACCOUNT,
                        SERVICE_PASSWORD,
                        &wstr[0],
                        &servFailActions
                    );
                }
                else
                {
                    wprintf(L"\nInstallation cancelled.\n");
                }
            }
            else
            {
			#ifdef _DEBUG
				wchar_t errBuf[50];
				swprintf_s(errBuf, 50, L"Service failed to run w/err 0x%08lx\n", GetLastError());
				std::wcout << errBuf << std::endl;
			#endif // _DEBUG
            }
        }
    }
	return 0;

  invalidInput:
	wprintf(PARAMETER_TEXT);
    return 0;
}
