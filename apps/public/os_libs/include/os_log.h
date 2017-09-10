/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		os_log.h
 * brief		
 * details	
 *
 * author	wangwenhao
 * version	
 * date		16Jun11
 *
 * history \arg	1.0, 16Jun11, wangwenhao, create file
 */
#ifndef __OS_LOG_H__
#define __OS_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */


/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/
#define LOG_CONTENT_LEN 128

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/
typedef enum
{
	LOG_EMERG = 0,
	LOG_ALERT = 1,
	LOG_CRIT = 2,
	LOG_ERROR = 3,
	LOG_WARN = 4,
	LOG_NOTICE = 5,
	LOG_INFORM = 6,
	LOG_DEBUG = 7
} LOG_SEVERITY;

typedef enum
{
	LOG_USER = 1,
	LOG_LOCAL0 = 16,
	LOG_LOCAL1 = 17,
	LOG_LOCAL2 = 18,
	LOG_LOCAL3 = 19,
	LOG_LOCAL4 = 20,
	LOG_LOCAL5 = 21,
	LOG_LOCAL6 = 22,
	LOG_LOCAL7 = 23
} LOG_FACILITY;

typedef enum
{
	LOG_SYSTEM = 0,
	LOG_INTERNET = 1,
	LOG_DHCPD = 2,
	LOG_HTTPD = 3,
	LOG_CMM_PPP = 4,
	LOG_OTHER = 5,
	LOG_DHCPC = 6,
	LOG_DSL	= 7,
	LOG_IGMP = 8,
	LOG_MOBILE = 9,
	LOG_VOIP = 10,
	LOG_MODULE_MAX
} LOG_MODULE;

typedef struct _LOG_MSG
{
	LOG_SEVERITY severity;
	LOG_MODULE module;
	char content[LOG_CONTENT_LEN];
} LOG_MSG;
/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           FUNCTIONS                                            */
/**************************************************************************************************/
void cmmlog(LOG_SEVERITY severity, LOG_MODULE module, const char *format, ...);

void cmmlog_logRemoute(unsigned int ip, unsigned short port, const char *content, unsigned int len);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif	/* __OS_LOG_H__ */
