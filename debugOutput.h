/****************************** Module Header ******************************\
* Module Name:  debugOutput.h
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

#pragma once

#ifdef _DEBUG
	#include <iostream>
	#include <fstream>
	#include <time.h>
#endif

template <typename T>
void debugOutput(T toStream)
{
	#ifdef _DEBUG
		std::wcout << toStream;
		std::wcout.flush();
	#endif
}

template<typename T, typename... Args>
void debugOutput(T toStream, Args... args)
{
	debugOutput(toStream);
	debugOutput(args...);
}

void indentedDebugOutput(const char* outputBuf, int tabCount = 1);
void indentedDebugOutput(const wchar_t* outputBuf, int tabCount = 1);
void debugOutputTimestamp();