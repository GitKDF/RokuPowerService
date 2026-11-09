/****************************** Module Header ******************************\
* Module Name:  WakeRoku.cpp
* Project:      RokuPowerService
* 
* Created by Kris Farrin, 2018
*
* Provides a minimal HTTP connection specific to sending a POST request to
* turn on a Roku.
*
* This code is loosely adapted from:
*	Code in the following codeproject post by enhzflep on July 24, 2012:
*		https://www.codeproject.com/Answers/427446/calling-a-website-from-cplusplus#answer2
*		and is licensed under the The Code Project Open License (CPOL) 1.02
* 
*	
\***************************************************************************/

#pragma region Includes
#include "WakeRoku.h"
#include "debugOutput.h"
#include "ThreadPool.h"
#include <thread>
#include <chrono>
#include <Ws2tcpip.h> // Required for InetPtonW
#pragma endregion


WakeRoku::WakeRoku()
{
	m_threadFlag = FALSE;

	// Start winsock version 2.2, which is required for modern IP helper functions like InetPtonW
	WSAStarted = (0 == (WSAStartup(MAKEWORD(2,2), &wsaData)));

	m_WOLThreadStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_WOLThreadStoppedEvent == NULL)
	{
		throw GetLastError();
	}
	SetEvent(m_WOLThreadStoppedEvent);

	m_HTTPThreadStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_HTTPThreadStoppedEvent == NULL)
	{
		throw GetLastError();
	}
	SetEvent(m_HTTPThreadStoppedEvent);
}


WakeRoku::~WakeRoku()
{
	if (WSAStarted) WSACleanup();

	if (m_WOLThreadStoppedEvent)
	{
		CloseHandle(m_WOLThreadStoppedEvent);
		m_WOLThreadStoppedEvent = NULL;
	}

	if (m_HTTPThreadStoppedEvent)
	{
		CloseHandle(m_HTTPThreadStoppedEvent);
		m_HTTPThreadStoppedEvent = NULL;
	}
}


BOOL WakeRoku::Init(wchar_t* wDestIP, wchar_t* wSubnetMask, wchar_t* wMACAddress)
{
	debugOutput(L"Initializing variables\n");

	const int IPBufSize = 16;

	// Setup server struct for HTTP to Roku and validate destination IP using InetPtonW
	m_destinationServer.sin_family = AF_INET;
	m_destinationServer.sin_port = htons(8060);

	if (InetPtonW(AF_INET, wDestIP, &m_destinationServer.sin_addr) != 1)
	{
		wprintf(L"\nError: Invalid IPv4 Address format\n\n");
		debugOutput(L"Invalid IPv4 Address format\n");
		return FALSE;
	}

	unsigned long destIP = m_destinationServer.sin_addr.s_addr;

	// Initialize char array with Destination IP for HTTP header string formatting
	char charDestIP[IPBufSize];
	WCHAR_TO_CHAR(wDestIP, charDestIP, IPBufSize);


	// Initialize char array with SubnetMask
	char charSubnetMask[IPBufSize];
	WCHAR_TO_CHAR(wSubnetMask, charSubnetMask, IPBufSize);

	// Convert Subnet Mask from char to binary representation using InetPtonW for consistency and safety
	struct in_addr subnetAddr;
	if (InetPtonW(AF_INET, wSubnetMask, &subnetAddr) != 1)
	{
		wprintf(L"\nError: Invalid Subnet Mask format\n\n");
		debugOutput(L"Invalid Subnet Mask format\n");
		return FALSE;
	}
	unsigned long subnetMask = subnetAddr.s_addr;

	// IP bitwise OR'd to the negation of the subnet mask to get the subnet broadcast address
	unsigned long broadcastIP = destIP | ~subnetMask;
	unsigned char* charBroadcastIP = (unsigned char*)&broadcastIP;
	debugOutput(L"WOL Broadcast IP: ", (int)charBroadcastIP[0], L".", (int)charBroadcastIP[1], L".", (int)charBroadcastIP[2], L".", (int)charBroadcastIP[3], L"\n");

	// Setup server struct for WOL Broadcast
	m_broadcastServer.sin_addr.s_addr = broadcastIP;
	m_broadcastServer.sin_family = AF_INET;
	m_broadcastServer.sin_port = htons(9);


	// Build HTTP string to send
	m_powerOnString = "POST /keypress/PowerOn HTTP/1.1\r\n" \
		"Host: " + std::string(charDestIP) + ":8060\r\n" \
		"User-Agent: RokuPowerService\r\n" \
		"Accept : */*\r\n\r\n";
	m_powerOffString = "POST /keypress/PowerOff HTTP/1.1\r\n" \
		"Host: " + std::string(charDestIP) + ":8060\r\n" \
		"User-Agent: RokuPowerService\r\n" \
		"Accept : */*\r\n\r\n";
	debugOutput(L"Power On HTTP POST Buffer:\n");
	indentedDebugOutput(m_powerOnString.c_str());


	// Load MAC Address
	int i;
	char m_MACAddress[6];
	for (i = 0; i < 6; i++)  //6 hexadecimal character pairs make 6 bytes worth of data
	{
		if ((i < 5) && (L':' != wMACAddress[(i * 3) + 2]))  //check to make sure every third character is a :
		{
			wprintf(L"\nError: Could not parse MAC Address\n");
			debugOutput(L"Error parsing MAC Address\n");
			return FALSE;
		}
		int hexValue = WCHAR_TO_HEX(wMACAddress + i * 3);  //get character 0,2,4...10 and call function to return it as integer value
		if (-1 == hexValue)		//check if there was an error in mapping the character to a value
		{
			wprintf(L"\nError: Could not parse MAC Address\n");
			debugOutput(L"Error parsing MAC Address\n");
			return FALSE;
		}
		m_MACAddress[i] = (hexValue << 4);  //4 bit shift to the left to move the value to the high order bits in the byte
		hexValue = WCHAR_TO_HEX(wMACAddress + i * 3 + 1);  //get character 1,3,5...11 and call function to return it as integer value
		if (-1 == hexValue)
		{
			wprintf(L"\nError: Could not parse MAC Address\n");
			debugOutput(L"Error parsing MAC Address\n");
			return FALSE;
		}
		m_MACAddress[i] += hexValue;  //add it to the existing byte value to set the low order bits
	}
	
	
	// Build Magic Packet
	memset(m_magicPacket, -1, 6); //6 bytes of 0xff
	for (i = 1; i <= 16; i++)	//6 bytes of the mac address repeated 16 times to complete the 102 byte packet
	{
		memcpy(m_magicPacket + (i * 6), m_MACAddress, 6);
	}
	m_magicPacket[magicPacketLen] = '\0';

	debugOutput(L"MagicPaket: ", m_magicPacket, L"\n");

	return TRUE;
}


int WakeRoku::PowerOn()
{
	return this->SendHTTPString(m_powerOnString, TRUE);
}

int WakeRoku::PowerOff()
{
	return this->SendHTTPString(m_powerOffString, false, 1);
}

int WakeRoku::SendHTTPString(std::string &sendBuffer, BOOL UseWOL, int MaxRetriesOn202) {
	//wait for the HTTPThreadStopped event to be set in the possibility that there is another thread already sending a command
	//waiting as long as triple the WOL timout value should be more than plenty of time for the other thread to finish
	//this is unlikely to ever occur, but *could* if multiple power event messages were to be received in a short period
	if (WaitForSingleObject(m_HTTPThreadStoppedEvent, 3 * WOL_TIMEOUT * 1000) != WAIT_OBJECT_0)
	{
		throw GetLastError();
	}
	ResetEvent(m_HTTPThreadStoppedEvent);


	const int readBufferSize = 128;
	char readBuffer[readBufferSize];
	char* tmpResult = (char*)malloc(1); tmpResult[0] = '\0';
	long totalBytesRead, thisReadSize;
	int retryCounter = 1;
	SOCKET conn;
	int connectionStatus;
	int returnValue = WR_SOCKET_ERROR;			//set return value indicator to Socket error initially, to be changed later upon success
	m_threadFlag = FALSE;						//reset our multi thread flag

	if (MaxRetriesOn202 < 1) { return 2; }		//return if an invalid max retry value is passed

	if (UseWOL) {								//if WOL is requested
		ResetEvent(m_WOLThreadStoppedEvent);	//reset our thread stopped event
		CThreadPool::QueueUserWorkItem(&WakeRoku::WakeOnLAN, this);		//start the WOL routine in a new thread
	}

	//create socket and check result
	conn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (conn == INVALID_SOCKET) {
		debugOutput(L"Invalid Socket\n");
		m_threadFlag = TRUE;					//set flag to indicate WOL thread to stop if it is running
		goto Cleanup;							//goto cleanup section
	}
	debugOutput(L"\tAttempting to connect to Roku by HTTP\n");
	
	//set a timeout of double the WOL timout to keep trying to connect
	{	//create separate scope for timeout variable since this will be skipped if we jumped to the Cleanup label
		std::chrono::high_resolution_clock::time_point breakTime = std::chrono::high_resolution_clock::now() + std::chrono::seconds(WOL_TIMEOUT * 2);
		do {								//start loop to initiate socket connection
			connectionStatus = connect(conn, (struct sockaddr*)&m_destinationServer, sizeof(m_destinationServer));
			if (0 == connectionStatus)		//return value is 0 when the connection is successful
			{
				m_threadFlag = TRUE;		//set the threadFlag to indicate that we can stop sending WOL packets
				break;						//break out of our while loop
			}
			else closesocket(conn);		//we didn't connect, so close the socket and we'll try again...
		} while (std::chrono::high_resolution_clock::now() < breakTime);			//...as long as we haven't reached our timeout point
	}
	
	if (0 != connectionStatus)			//if we didn't connect, then we exited our loop from the connect loop timing out
	{
		debugOutput(L"\tUnable to connect to Roku by HTTP\n");
		m_threadFlag = TRUE;			//probably unneccessary since the WOL thread probably timed out long ago
		goto Cleanup;					//goto cleanup section
	}
	debugOutput(L"\tConnected to Roku by HTTP\n");

	//Send data
	debugOutput(L"\tSending HTTP POST\n");

	do {
		//added this loop in v3.1 since I've had once or twice that the box responded with 202 when coming alive from a WOL
		//yet it never turned on with one keypress sent, sending a few has been reliable, no harm since they all instruct power ON
		if (retryCounter > 1) debugOutput(L"\tResending HTTP Post due to '202 Accepted' response\n");
		send(conn, &sendBuffer[0], (int)sendBuffer.length(), 0);		//send buffer

		//Receive data
		//considered using a buffer here, but in trying to keep portable compiled exe size small, I'm trying to limit how much is #included in the release build
		totalBytesRead = 0;
		while (1)
		{
			memset(readBuffer, 0, readBufferSize);  //clear read buffer
			//read data into buffer and get number of bytes to read (smaller of remaining data or buffersize)
			thisReadSize = recv(conn, readBuffer, readBufferSize, 0);

			if (thisReadSize <= 0) break;  //leave while loop if no data was read means no more data

			//increase memory allocation by number of bytes read, plus 1 for a null terminator
			//which we'll overwrite if there's more data next iteration
			tmpResult = (char*)realloc(tmpResult, thisReadSize + totalBytesRead + 1);

			//copy data read into our buffer after the data from the previous iteration
			memcpy(tmpResult + totalBytesRead, readBuffer, thisReadSize);

			//append a null terminator on the end
			tmpResult[totalBytesRead + thisReadSize] = '\0';

			totalBytesRead += thisReadSize;
		}

		debugOutput(L"\tReceived HTTP Response:\n");
		indentedDebugOutput(tmpResult, 2);

	} while ((++retryCounter <= MaxRetriesOn202) && (NULL != strstr(tmpResult, "202 Accepted")));

	closesocket(conn);
	
	//boolean evaluation will set return value to 1 (WR_HTTP_ERROR) if received string did not contain 200 OK and did not contain 202 Accepted, or 0 (WR_SUCCESS) if it contained either
	returnValue = ((NULL == strstr(tmpResult, "200 OK")) && (NULL == strstr(tmpResult, "202 Accepted")));

	free(tmpResult);

Cleanup:
	
	if (UseWOL) {		//if we started a thread for WOL packets
		if (WaitForSingleObject(m_WOLThreadStoppedEvent, 2 * WOL_TIMEOUT * 1000) != WAIT_OBJECT_0)		//waiting as long as double the WOL timout value should be oodles of time for the other thread to finish
		{
			throw GetLastError();
		}
	}

	m_threadFlag = FALSE;		//reset our multi thread flag

	SetEvent(m_HTTPThreadStoppedEvent);

	return returnValue;
}


int WakeRoku::WCHAR_TO_HEX(wchar_t* hexChar)
{
	switch (hexChar[0])
	{
	case L'0': return 0;
	case L'1': return 1;
	case L'2': return 2;
	case L'3': return 3;
	case L'4': return 4;
	case L'5': return 5;
	case L'6': return 6;
	case L'7': return 7;
	case L'8': return 8;
	case L'9': return 9;
	case L'A': case L'a': return 10;
	case L'B': case L'b': return 11;
	case L'C': case L'c': return 12;
	case L'D': case L'd': return 13;
	case L'E': case L'e': return 14;
	case L'F': case L'f': return 15;
	}
	return -1;
}


BOOL WakeRoku::CreateBroadcastSocket(SOCKET &conn)
{
	conn = socket(AF_INET, SOCK_DGRAM, 0);
	if (conn == INVALID_SOCKET) {
		debugOutput(L"Invalid Socket\n");
		return FALSE;
	}

	char* broadcast[1];
	memset(broadcast, 1, 1);
	if (SOCKET_ERROR == setsockopt(conn, SOL_SOCKET, SO_BROADCAST, (char*)broadcast, sizeof(broadcast)))
	{
		debugOutput(L"Unable to enable broadcast on socket.", WSAGetLastError(), L"\n");
		return FALSE;
	}
	
	return TRUE;
}


void WakeRoku::WakeOnLAN()
{
	SOCKET conn;

	if (CreateBroadcastSocket(conn))
	{
		std::chrono::high_resolution_clock::time_point nextTime = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point stopTime = std::chrono::high_resolution_clock::now() + std::chrono::seconds(WOL_TIMEOUT);

		int sendCounter = 0;

		while (!m_threadFlag)
		{
			std::this_thread::sleep_until(nextTime);

			if (SOCKET_ERROR != sendto(conn, m_magicPacket, magicPacketLen, 0, (struct sockaddr *)&m_broadcastServer, sizeof(m_broadcastServer)))
			{
				sendCounter++;
			}
			else debugOutput(L"WOL Socket Error: ", WSAGetLastError(), L"\n");

			//DIAL specification indicates to send WOL packets every 50 milliseconds until the server responds
			nextTime += std::chrono::milliseconds(50);
			if (nextTime > stopTime)
			{
				debugOutput(L"Wake On Lan Timeout\n");
			}
		}

		if (0 != sendCounter) debugOutput(L"Sent ", sendCounter, L" Magic Packets\n");
	}

	closesocket(conn);

	SetEvent(m_WOLThreadStoppedEvent);
}



//change made this obsolete, but I'm saving it as it should optimize out anyway and I might want it later
//I don't think these are used per my comment a line up, but updating to avoid compiler warnings
int WakeRoku::WCHAR_TO_CHAR(wchar_t* in_char)
{
	int w_len = (int)wcslen(in_char);  //get length of wchar string as an int
	//get the buffer size needed (+1 for null terminator)
	return WideCharToMultiByte(CP_ACP, 0, in_char, w_len, NULL, 0, NULL, NULL) + 1; //line 395
}


void WakeRoku::WCHAR_TO_CHAR(wchar_t* in_char, char* out_char, int size_needed)
{
	memset(out_char, '\0', size_needed);  //clear buffer
	int w_len = (int)wcslen(in_char);  //get length of wchar string as an int
	WideCharToMultiByte(CP_ACP, 0, in_char, w_len, out_char, size_needed, NULL, NULL); //line 403
}
