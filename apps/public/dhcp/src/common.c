/* common.c
 *
 * Functions for debugging and logging as well as some other
 * simple helper functions.
 *
 * Russ Dill <Russ.Dill@asu.edu> 2001-2003
 * Rewritten by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"

#define DBG_INFO_LEN 256

static char *syslog_level_msg[] = {
	[EMERG]   = "EMERGENCY!",
	[ALERT]   = "ALERT!",
	[CRIT]    = "critical!",
	[WARNING] = "warning",
	[ERR]     = "error",
	[INFO]    = "info",
	[DEBUG]   = "debug"
};

void udhcp_logging(int level, const char *call, int line, const char *fmt, ...)
{
	int size = 0;
	char info[DBG_INFO_LEN] = {0};
	va_list args;

	va_start(args, fmt);
	size = vsnprintf(info, DBG_INFO_LEN - 1, fmt, args);
	va_end(args);

	printf("[ %s ] %03d: %s, %s\n", call, line, syslog_level_msg[level], info);

	return;
}

/* 
 * fn		int udhcp_addrNumToStr(unsigned int numAddr, char *pStrAddr)
 * brief	translate ip address from binary data into the standard numbers-and-dots notation	
 *
 * param[in]	numAddr	- binary data form ip address
 * param[out]	pStrAddr - return numbers-and-dots notation form ip address	
 *
 * return	-1 is returned if an error occurs, otherwise return value is 0	
 *
 * note		use inet_ntoa() is forbidden
 */
int udhcp_addrNumToStr(unsigned int numAddr, char *pStrAddr)
{
	int a = 0, b = 0, c = 0,d = 0;
	
	if (pStrAddr == NULL)
		return 0;
	
	a = numAddr >> 24;
	b = (numAddr << 8) >> 24;
	c = (numAddr << 16) >> 24;
	d = (numAddr << 24) >> 24;
	if (a > 255 || b > 255 || c > 255 || d > 255 ||
			a < 0 || b < 0 || c < 0 || d < 0)
	{
		return -1;
	}
	
    sprintf(pStrAddr, "%d.%d.%d.%d", a, b, c, d);
	
	return 0;
}
