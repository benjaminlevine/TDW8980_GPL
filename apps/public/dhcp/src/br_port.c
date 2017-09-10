#include "dhcps.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <netinet/if_ether.h>
#include <linux/if_bridge.h>
#include <net/if.h>

#define DHCP_MAC_ADDR_LEN  (6)

/************************************************************ 
  Function:        getPortNameFromMacAddr
  Description:     provide a mac address, search it in the bridge fdb table
					, and find out which interface the packet came from
  Input:           
		const char* ifName 				the name of bridge 
		const unsigned char* macAddr	the mac address

  Output:          
		char *portName					the port name if mac is found
  Return:          
		-1			if error occurs
		0			if succeed
************************************************************/ 
int getPortNameFromMacAddr(const char *ifName, const unsigned char *macAddr, char *portName)
{
   int sfd;
   int portid = -1;
   int i;
   int n=-1;
   int retries=0;
   int ret = 0;

   struct ifreq ifr;
   struct __fdb_entry fe[128];
   int ifindices[1024];
   unsigned long args0[4] = { BRCTL_GET_FDB_ENTRIES,
                              (unsigned long) fe,
                              sizeof(fe)/sizeof(struct __fdb_entry), 0 };
   unsigned long args1[4] = { BRCTL_GET_PORT_LIST,
                              (unsigned long) ifindices,
                              sizeof(ifindices)/sizeof(int), 0 };


   if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   {
       perror("socket create");
	   return -1;
   }

   memset(ifr.ifr_name, 0, IFNAMSIZ);
   strncpy(ifr.ifr_name, ifName, IFNAMSIZ);
   ifr.ifr_data = (char *) args0;

   while (n < 0 && retries < 10)
   {
      n = ioctl(sfd, SIOCDEVPRIVATE, &ifr);
      if (n < 0)
      {
         LOG_INFO("DHCPS: showmacs error %d n=%d EAGAIN=%d\n", retries, n, errno == EAGAIN ? 1:0);

         if (errno == EAGAIN)
         {
            sleep(0);
            retries++;
         }
         else
         {
            break;
         }
      }
   }

   if (n < 0)
   {
      LOG_INFO("DHCPS: showmacs failed, n=%d retries=%d\n", n, retries);
      close(sfd);
      return -1;
   }
   else
   {
      LOG_INFO("DHCPS: got %d mac addresses from kernel\n", n);
   }

   for (i = 0; i < n; i++) {
      if (memcmp(macAddr, fe[i].mac_addr, DHCP_MAC_ADDR_LEN) == 0)
      {
         portid = fe[i].port_no;
         LOG_INFO("DHCPS: found port id = %d\n", portid);
         break;
      }
   }

   if (portid == -1)
   {
      close(sfd);
	  LOG_INFO("DHCPS: portid == -1\n");
      return -1;
   }


   memset(ifindices, 0, sizeof(ifindices));
   strncpy(ifr.ifr_name, ifName, IFNAMSIZ);
   ifr.ifr_data = (char *) &args1;

   n = ioctl(sfd, SIOCDEVPRIVATE, &ifr);

   close(sfd);

   if (n < 0) {
      LOG_INFO("DHCPS: list ports for bridge: br0 failed: %s\n",
                   strerror(errno));
      return -1;
   }

   if (ifindices[portid] == 0)
   {
      LOG_INFO("DHCPS: ifindices[portid] is zero!, portid=%d\n", portid);
      return -1;
   }

   if (!if_indextoname(ifindices[portid], portName))
   {
      LOG_INFO("DHCPS: if_indextoname failed, ifindices[portid]=%d\n", ifindices[portid]);
      return -1;
   }

   if (strlen(portName) == 0)
   {
      LOG_INFO("DHCPS: empty portName?\n");
      ret = -1;
   }
   else
   {
      LOG_INFO("DHCPS: PortName=%s\n", portName);
   }

   return ret;
}
