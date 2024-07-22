/******************************************************************************
 * Author: liguoqiang
 * Date: 2023-12-27 08:53:33
 * LastEditors: liguoqiang
 * LastEditTime: 2024-04-18 10:46:24
 * Description: 
********************************************************************************/
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#endif

#include "utils.h"

const uint64 FILETIME_TO_TIMEVAL_SKEW=0x19db1ded53e8000;

#ifdef WIN32
static void system_timeval2(LPSYSTEMTIME st, struct timeval* tv)
{
	FILETIME ft;
	ULARGE_INTEGER ui;
	ZeroMemory(&ft, sizeof(FILETIME));
	ZeroMemory(&ui, sizeof(ULARGE_INTEGER));

	SystemTimeToFileTime(st, &ft);

	ui.LowPart = ft.dwLowDateTime;
	ui.HighPart = ft.dwHighDateTime;
	ui.QuadPart -= FILETIME_TO_TIMEVAL_SKEW;
	tv->tv_sec = (unsigned long)(ui.QuadPart / (10000 * 1000));
	tv->tv_usec = (unsigned long)(ui.QuadPart % (10000 * 1000)) / 10;
}
#endif

float getMSecOfTime()
{
	struct timeval tval;
	memset(&tval, 0, sizeof(struct timeval));
#ifdef WIN32
	SYSTEMTIME st;
	ZeroMemory(&st, sizeof(SYSTEMTIME));
	GetSystemTime(&st);
	system_timeval2(&st, &tval);
#else
	gettimeofday(&tval, 0);
#endif
	return (float)(tval.tv_sec * 1000 + tval.tv_usec / 1000);
}