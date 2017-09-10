/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		os_log.c
 * brief		
 * details	
 *
 * author	wangwenhao
 * version	
 * date		16Jun11
 *
 * history \arg	1.0, 16Jun11, wangwenhao, create file
 */

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include <os_lib.h>
#include <os_msg.h>
#include <os_log.h>
 
/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           EXTERN_PROTOTYPES                                    */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_PROTOTYPES                                     */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_FUNCTIONS                                      */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           PUBLIC_FUNCTIONS                                     */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           GLOBAL_FUNCTIONS                                     */
/**************************************************************************************************/
void cmmlog(LOG_SEVERITY severity, LOG_MODULE module, const char *format, ...)
{
	CMSG_FD msgFd;
	CMSG_BUFF msgBuf;
	LOG_MSG *pLogMsg = (LOG_MSG *)msgBuf.content;
	va_list args;

	if (0 != msg_init(&msgFd))
	{
		return;
	}
	
	if (0 != msg_connSrv(CMSG_ID_LOG, &msgFd))
	{
		goto tail;
	}

	msgBuf.type = CMSG_LOG;

	pLogMsg->severity = severity;
	pLogMsg->module = module;

	va_start(args, format);
	vsnprintf(pLogMsg->content, LOG_CONTENT_LEN, format, args);
	va_end(args);

	DEBUG_PRINT("Send log, module %d, severity %d\n", module, severity);
	msg_send(&msgFd, &msgBuf);

tail:
	msg_cleanup(&msgFd);
}

void cmmlog_logRemoute(unsigned int ip, unsigned short port, const char *content, unsigned int len)
{
	int sock;
	struct sockaddr_in addr;
	int sockOpt;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
	{
		printf("Get socket failed, when do remote log\n");
		return;
	}

	addr.sin_family = AF_INET;
	/* this may have already changed to network byte-order by inet_addr */
	addr.sin_addr.s_addr = ip;
	addr.sin_port = htons(port); 

	sockOpt = 1;
	if (0 != ioctl(sock, FIONBIO, (int)&sockOpt))
	{
		printf("log ioctl data nonblock fail\n");
	}

	sendto(sock, (void *)content, len, 0, (struct sockaddr *)&addr, sizeof(addr));

	close(sock);
}

