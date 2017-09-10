#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <syslog.h>
#include <upnp.h>
#include "globals.h"
#include "config.h"
#include "pmlist.h"
#include "gatedevice.h"
#include "util.h"

#if HAVE_LIBIPTC
#include "iptc.h"
#endif

struct portMap* pmlist_NewNode(int enabled, long int duration, char *remoteHost,
			       char *externalPort, char *internalPort,
			       char *protocol, char *internalClient, char *desc)
{
	struct portMap* temp = (struct portMap*) malloc(sizeof(struct portMap));

	temp->m_PortMappingEnabled = enabled;
	
	if (remoteHost && strlen(remoteHost) < sizeof(temp->m_RemoteHost)) strcpy(temp->m_RemoteHost, remoteHost);
		else strcpy(temp->m_RemoteHost, "");
	if (strlen(externalPort) < sizeof(temp->m_ExternalPort)) strcpy(temp->m_ExternalPort, externalPort);
		else strcpy(temp->m_ExternalPort, "");
	if (strlen(internalPort) < sizeof(temp->m_InternalPort)) strcpy(temp->m_InternalPort, internalPort);
		else strcpy(temp->m_InternalPort, "");
	if (strlen(protocol) < sizeof(temp->m_PortMappingProtocol)) strcpy(temp->m_PortMappingProtocol, protocol);
		else strcpy(temp->m_PortMappingProtocol, "");
	if (strlen(internalClient) < sizeof(temp->m_InternalClient)) strcpy(temp->m_InternalClient, internalClient);
		else strcpy(temp->m_InternalClient, "");
	if (strlen(desc) < sizeof(temp->m_PortMappingDescription)) strcpy(temp->m_PortMappingDescription, desc);
		else strcpy(temp->m_PortMappingDescription, "");
	temp->m_PortMappingLeaseDuration = duration;

	temp->next = NULL;
	temp->prev = NULL;
	
	return temp;
}
	
struct portMap* pmlist_Find(char *externalPort, char *proto, char *internalClient)
{
	struct portMap* temp;
	
	temp = pmlist_Head;
	if (temp == NULL)
		return NULL;
	
	do 
	{
		if ( (strcmp(temp->m_ExternalPort, externalPort) == 0) &&
				(strcmp(temp->m_PortMappingProtocol, proto) == 0) &&
				(strcmp(temp->m_InternalClient, internalClient) == 0) )
			return temp; // We found a match, return pointer to it
		else
			temp = temp->next;
	} while (temp != NULL);
	
	// If we made it here, we didn't find it, so return NULL
	return NULL;
}

struct portMap* pmlist_FindByIndex(int index)
{
	int i=0;
	struct portMap* temp;

	temp = pmlist_Head;
	if (temp == NULL)
		return NULL;
	do
	{
		if (i == index)
			return temp;
		else
		{
			temp = temp->next;	
			i++;
		}
	} while (temp != NULL);

	return NULL;
}	

struct portMap* pmlist_FindSpecific(char *externalPort, char *protocol)
{
	struct portMap* temp;
	
	temp = pmlist_Head;
	if (temp == NULL)
		return NULL;
	
	do
	{
		if ( (strcmp(temp->m_ExternalPort, externalPort) == 0) &&
				(strcmp(temp->m_PortMappingProtocol, protocol) == 0))
			return temp;
		else
			temp = temp->next;
	} while (temp != NULL);

	return NULL;
}

int pmlist_IsEmtpy(void)
{
	if (pmlist_Head)
		return 0;
	else
		return 1;
}

int pmlist_Size(void)
{
	struct portMap* temp;
	int size = 0;
	
	temp = pmlist_Head;
	if (temp == NULL)
		return 0;
	
	while (temp->next)
	{
		size++;
		temp = temp->next;
	}
	size++;
	return size;
}	

int pmlist_FreeList(void)
{
  struct portMap *temp, *next;

  temp = pmlist_Head;
  while(temp) {
    CancelMappingExpiration(temp->expirationEventId);
    pmlist_DeletePortMapping(temp->m_PortMappingEnabled, temp->m_PortMappingProtocol, temp->m_ExternalPort,
			     temp->m_InternalClient, temp->m_InternalPort);
    next = temp->next;
    free(temp);
    temp = next;
  }
  pmlist_Head = pmlist_Tail = NULL;
  return 1;
}
		
/* 
 * fn		int pmlist_PushBack(struct portMap* item)
 * brief	新添加一个端口映射时会调用该api添加到链表尾部.	
 * details	最终会调用该函数添加端口映射到链表和到iptables.	
 *
 * param[in]	item  端口映射条目.
 *
 * return		int
 * retval		0/1
 *				0	失败
 *				1	成功
 *
 * note	written by  09Nov11, LI CHENGLONG	
 */
int pmlist_PushBack(struct portMap* item)
{
	int action_succeeded = 0;

	if (pmlist_Tail) // We have a list, place on the end
	{
		pmlist_Tail->next = item;
		item->prev = pmlist_Tail;
		item->next = NULL;
		pmlist_Tail = item;
		action_succeeded = 1;
	}
	else // We obviously have no list, because we have no tail :D
	{
		pmlist_Head = pmlist_Tail = pmlist_Current = item;
		item->prev = NULL;
		item->next = NULL;
 		action_succeeded = 1;
		trace(3, "appended %d %s %s %s %s %ld", item->m_PortMappingEnabled, 
				    item->m_PortMappingProtocol, item->m_ExternalPort, item->m_InternalClient, item->m_InternalPort,
				    item->m_PortMappingLeaseDuration);
	}
	if (action_succeeded == 1)
	{
		pmlist_AddPortMapping(item->m_PortMappingEnabled, item->m_PortMappingProtocol,
				      item->m_ExternalPort, item->m_InternalClient, item->m_InternalPort);
		return 1;
	}
	else
		return 0;
}

		
/* 
 * fn		int pmlist_Delete(struct portMap* item)
 * brief	从链表中删除一个端口映射.	
 * details	从链表和iptables中删除一个端口映射.		
 *
 * param[in]	item 待删除的端口映射条目.
 *
 * return		
 * retval		
 *
 * note	written by  09Nov11, LI CHENGLONG	
 */
int pmlist_Delete(struct portMap* item)
{
	struct portMap *temp;
	int action_succeeded = 0;

	temp = pmlist_Find(item->m_ExternalPort, item->m_PortMappingProtocol, item->m_InternalClient);
	if (temp) // We found the item to delete
	{
	  CancelMappingExpiration(temp->expirationEventId);
		pmlist_DeletePortMapping(item->m_PortMappingEnabled, item->m_PortMappingProtocol, item->m_ExternalPort, 
				item->m_InternalClient, item->m_InternalPort);
		if (temp == pmlist_Head) // We are the head of the list
		{
			if (temp->next == NULL) // We're the only node in the list
			{
				pmlist_Head = pmlist_Tail = pmlist_Current = NULL;
				free (temp);
				action_succeeded = 1;
			}
			else // we have a next, so change head to point to it
			{
				pmlist_Head = temp->next;
				pmlist_Head->prev = NULL;
				free (temp);
				action_succeeded = 1;	
			}
		}
		else if (temp == pmlist_Tail) // We are the Tail, but not the Head so we have prev
		{
			pmlist_Tail = pmlist_Tail->prev;
			free (pmlist_Tail->next);
			pmlist_Tail->next = NULL;
			action_succeeded = 1;
		}
		else // We exist and we are between two nodes
		{
			temp->prev->next = temp->next;
			temp->next->prev = temp->prev;
			pmlist_Current = temp->next; // We put current to the right after a extraction
			free (temp);	
			action_succeeded = 1;
		}
	}
	else  // We're deleting something that's not there, so return 0
		action_succeeded = 0;

	return action_succeeded;
}

int pmlist_AddPortMapping (int enabled, char *protocol, char *externalPort, char *internalClient, char *internalPort)
{

	if (g_vars.extInterfaceName[0] == '\0')
	{
		fprintf(stderr, "extInterfaceName not available\n\n");
	}

    if (enabled)
    {
#if HAVE_LIBIPTC
	char *buffer = malloc(strlen(internalClient) + strlen(internalPort) + 2);
	if (buffer == NULL) {
		fprintf(stderr, "failed to malloc memory\n");
		return 0;
	}

	strcpy(buffer, internalClient);
	strcat(buffer, ":");
	strcat(buffer, internalPort);

	if (g_vars.forwardRules)
		iptc_add_rule("filter", g_vars.forwardChainName, protocol, NULL, NULL, NULL, internalClient, NULL, internalPort, "ACCEPT", NULL, FALSE);

	iptc_add_rule("nat", g_vars.preroutingChainName, protocol, g_vars.extInterfaceName, NULL, NULL, NULL, NULL, externalPort, "DNAT", buffer, TRUE);
	free(buffer);
#else
	char command[COMMAND_LEN];
	int status;
	
	{
	  char dest[DEST_LEN];
	/* g_vars.extInterfaceName,
	 如果extInterfaceName 为空 则 是使所有接口都生效. is defined by command line  by Li Chenglong*/
	  /*important!, Added by LI CHENGLONG , 2011-Nov-09.*/
	/* changed "-I" to "-A", g_vars.preroutingChainName to "PREROUTING_UPNP"
	 * g_vars.forwardChainName to "FORWARD_UPNP"
	 * use system call instead of fork
	 * yangxv, 2011.11.20
	 */
	  char *args[] = {"iptables", "-t", "nat", "-A", g_vars.preroutingChainName, "-i", g_vars.extInterfaceName, "-p", protocol, "--dport", externalPort, "-j", "DNAT", "--to", dest, NULL};
 	  /* Ended by LI CHENGLONG , 2011-Nov-09.*/

	  snprintf(dest, DEST_LEN, "%s:%s", internalClient, internalPort);
	  snprintf(command, COMMAND_LEN, "%s -t nat -A %s -i %s -p %s --dport %s -j DNAT --to %s:%s", g_vars.iptables, "PREROUTING_UPNP"/*g_vars.preroutingChainName*/, g_vars.extInterfaceName, protocol, externalPort, internalClient, internalPort);
	  trace(3, "%s", command);

	  system(command);

	  #if 0
	  if (!fork()) {
	    int rc = execv(g_vars.iptables, args);
	    exit(rc);
	  } else {
	    wait(&status);		
	  }
	  #endif
	}

	if (g_vars.forwardRules)
	{
	  char *args[] = {"iptables", "-A", g_vars.forwardChainName, "-p", protocol, "-d", internalClient, "--dport", internalPort, "-j", "ACCEPT", NULL};
	  
	  snprintf(command, COMMAND_LEN, "%s -A %s -p %s -d %s --dport %s -j ACCEPT", g_vars.iptables, "FORWARD_UPNP"/*g_vars.forwardChainName*/, protocol, internalClient, internalPort);
	  trace(3, "%s", command);

	  system(command);

	#if 0
	  if (!fork()) {
	    int rc = execv(g_vars.iptables, args);
	    exit(rc);
	  } else {
	    wait(&status);		
	  }
	#endif
	}
#endif
    }
    return 1;
}


/* 
 * fn		int pmlist_DeletePortMapping(int enabled, 
 *										 char *protocol,
 *										 char *externalPort,
 *										 char *internalClient,
 *										 char *internalPort);
 * brief		从iptables中删除端口映射.
 * details		从iptables中删除端口映射.
 *
 * param[in]	enabled					是否启用
 * param[in]	protocol				端口映射协议
 * param[in]	externalPort			外部端口号
 * param[in]	internalClient			内部客户端ip
 * param[in]	internalPort			内部端口号
 *
 * return		int
 * retval		
 *
 * note	written by  09Nov11, LI CHENGLONG	
 */
int pmlist_DeletePortMapping(int enabled, char *protocol, char *externalPort, char *internalClient, char *internalPort)
{

	if (g_vars.extInterfaceName[0] == '\0')
	{
		fprintf(stderr, "extInterfaceName not available\n");
	}
	
    if (enabled)
    {
#if HAVE_LIBIPTC
	char *buffer = malloc(strlen(internalClient) + strlen(internalPort) + 2);
	strcpy(buffer, internalClient);
	strcat(buffer, ":");
	strcat(buffer, internalPort);

	if (g_vars.forwardRules)
	    iptc_delete_rule("filter", g_vars.forwardChainName, protocol, NULL, NULL, NULL, internalClient, NULL, internalPort, "ACCEPT", NULL);

	iptc_delete_rule("nat", g_vars.preroutingChainName, protocol, g_vars.extInterfaceName, NULL, NULL, NULL, NULL, externalPort, "DNAT", buffer);
	free(buffer);
#else
	char command[COMMAND_LEN];
	int status;
	
	{
	  char dest[DEST_LEN];
	  char *args[] = {"iptables", "-t", "nat", "-D", g_vars.preroutingChainName, "-i", g_vars.extInterfaceName, "-p", protocol, "--dport", externalPort, "-j", "DNAT", "--to", dest, NULL};

	  snprintf(dest, DEST_LEN, "%s:%s", internalClient, internalPort);
	  snprintf(command, COMMAND_LEN, "%s -t nat -D %s -i %s -p %s --dport %s -j DNAT --to %s:%s",
		  g_vars.iptables, "PREROUTING_UPNP"/*g_vars.preroutingChainName*/, g_vars.extInterfaceName, protocol, externalPort, internalClient, internalPort);
	  trace(3, "%s", command);
	  
	 system(command);

#if 0
	  if (!fork()) {
	    int rc = execv(g_vars.iptables, args);
	    exit(rc);
	  } else {
	    wait(&status);		
	  }
#endif
	}

	if (g_vars.forwardRules)
	{
	  char *args[] = {"iptables", "-D", g_vars.forwardChainName, "-p", protocol, "-d", internalClient, "--dport", internalPort, "-j", "ACCEPT", NULL};
	  
	  snprintf(command, COMMAND_LEN, "%s -D %s -p %s -d %s --dport %s -j ACCEPT", g_vars.iptables, "FORWARD_UPNP"/*g_vars.forwardChainName*/, protocol, internalClient, internalPort);
	  trace(3, "%s", command);

	  system(command);

#if 0
	  if (!fork()) {
	    int rc = execv(g_vars.iptables, args);
	    exit(rc);
	  } else {
	    wait(&status);		
	  }
#endif
	}
#endif
    }
    return 1;
}

