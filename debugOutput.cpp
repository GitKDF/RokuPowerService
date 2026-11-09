/****************************** Module Header ******************************\
* Module Name:  debugOutput.cpp
* Project:      RokuPowerService
* Created by Kris Farrin, 2018
*
* Provides a way to output verbose debugging messages to the stdout stream
* (in this case redirected to a text file) that will be optimized out if
* compiling a RELEASE build.  This eliminates putting
*    #ifdef _DEBUG  /  std::wcout << arg << arg...  /  #endif
* all over the place.
* Also includes a recursive call to allow infinite arguments.
\***************************************************************************/


#include <iostream>
#include <ctime>
#include <cstddef>
#include "debugOutput.h"

void indentedDebugOutput(const char* outputBuf, int tabCount)
{
#if defined(_DEBUG) || defined(ENABLE_FILE_LOGGING)
	if (NULL == outputBuf) { return; }

	if ('\0' != outputBuf[0]) {
		int i;
		for (i = 1; i <= tabCount; i++) {
			std::wcout << '\t';
		}
	}

	while ('\0' != outputBuf[0])
	{
		//ignore any \r characters since pushing a \n character to the stream will output a \r\n
		if ( ('\r' == outputBuf[0]) && ('\n' == outputBuf[1]) ) {
			outputBuf++;
		}

		std::wcout << outputBuf[0];

		if ( ('\n' == outputBuf[0]) && ('\0' != outputBuf[1]) )
		{
			int i;
			for (i = 1; i <= tabCount; i++) {
				std::wcout << '\t';
			}
		}

		outputBuf++;
	}
	std::wcout.flush();
#endif
}


void indentedDebugOutput(const wchar_t* outputBuf, int tabCount)
{
#if defined(_DEBUG) || defined(ENABLE_FILE_LOGGING)
	if (NULL == outputBuf) { return; }

	if (L'\0' != outputBuf[0]) {
		int i;
		for (i = 1; i <= tabCount; i++) {
			std::wcout << L'\t';
		}
	}

	while (L'\0' != outputBuf[0])
	{
		//ignore any \r characters since pushing a \n character to the stream will output a \r\n
		if ( (L'\r' == outputBuf[0]) && (L'\n' == outputBuf[1]) ) {
			outputBuf++;
		}

		std::wcout << outputBuf[0];

		if ( (L'\n' == outputBuf[0]) && (L'\0' != outputBuf[1]) ) 
		{
			int i;
			for (i = 1; i <= tabCount; i++) {
				std::wcout << L'\t';
			}
		}

		outputBuf++;
	}
	std::wcout.flush();
#endif
}

void debugOutputTimestamp() {
#if defined(_DEBUG) || defined(ENABLE_FILE_LOGGING)
	time_t rawtime;
	struct tm timeInfo;
	char timeBuf[26];

	time(&rawtime);
	localtime_s(&timeInfo, &rawtime);
	asctime_s(timeBuf, sizeof(timeBuf), &timeInfo);
	debugOutput(timeBuf);
#endif
}
