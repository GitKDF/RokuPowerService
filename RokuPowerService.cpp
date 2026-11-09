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
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
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
//  FUNCTION: wmain
