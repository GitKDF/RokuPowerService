/****************************** Module Header ******************************\
* Module Name:  WakeRoku.cpp
* Project:      RokuPowerService
* Created by Kris Farrin, 2018
*
* Provides a minimal HTTP connection specific to sending a POST request to
* turn on a Roku.
\***************************************************************************/

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <atomic>

#pragma comment(lib,"ws2_32.lib")

#define WOL_TIMEOUT	20
//DIAL specification recommends trying WOL packets for twice the timeout value indicated
//in the SSDP response from the client, which my Roku TV specifies at 10 seconds

#define WR_SUCCESS			0
#define WR_HTTP_ERROR		1
#define WR_SOCKET_ERROR		2


class WakeRoku
{
public:

	WakeRoku();
	~WakeRoku();

	BOOL Init(wchar_t* wDestIP, wchar_t* wSubnetMask, wchar_t* wMACAddress);
	int SendHTTPString(std::string &sendBuffer, BOOL UseWOL = false, int MaxRetriesOn202 = 5);
	int PowerOn();
	int PowerOff();
	BOOL WSAStarted;

private:

	static BOOL CreateBroadcastSocket(SOCKET &conn);
	void WakeOnLAN();

	static int WCHAR_TO_CHAR(wchar_t* in_char);
	static void WCHAR_TO_CHAR(wchar_t* in_char, char* out_char, int size_needed);
	static int WCHAR_TO_HEX(wchar_t* hexChar);

	WSADATA wsaData;
	HANDLE m_WOLThreadStoppedEvent;
	HANDLE m_HTTPThreadStoppedEvent;

	struct sockaddr_in m_broadcastServer;
	struct sockaddr_in m_destinationServer;
	std::string m_powerOnString;
	std::string m_powerOffString;

	static const int magicPacketLen = 102;
	char m_magicPacket[magicPacketLen + 1];

	std::atomic<bool> m_threadFlag;

};
