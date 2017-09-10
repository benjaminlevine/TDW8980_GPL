#ifndef __BR_PORT_H__
#define __BR_PORT_H__

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
int getPortNameFromMacAddr(const char *ifName, const unsigned char *macAddr, char *portName);

#endif

