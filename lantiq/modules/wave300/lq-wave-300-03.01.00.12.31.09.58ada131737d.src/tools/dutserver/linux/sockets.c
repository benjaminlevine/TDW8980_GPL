/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/// Sockets Handling for the linux server
/// Can be also compiled in Windows (for easier debugging)

#include <stdio.h>
#include "sockets.h"
#include "driver_api.h"
#include "dutserver.h"

#ifdef WIN32
    #include <Winsock2.h>
    #include <process.h>
    #define MT_THREAD_RET_T void
    #define MT_THREAD_RET
  #define  socketClose(fd) closesocket(fd)
    #define MT_SLEEP(delay_in_mSec)   Sleep(delay_in_mSec)
  typedef int NAME_LEN_TYPE;
#else /* !WIN32 --> linux */
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <memory.h>
  #include <stdlib.h>
  #include <signal.h>
  #include <pthread.h>
    #define MT_SLEEP(delay_in_mSec)   usleep((delay_in_mSec)*1000)
    #define MT_THREAD_RET_T void*
    #define MT_THREAD_RET NULL
  #define  socketClose(fd) close(fd)
  #define INVALID_SOCKET -1
  #define _getpid getpid
  typedef unsigned int NAME_LEN_TYPE;
#endif /* WIN32 */

#define MT_SERVER_PORT_STREAM  22222  // DUT server/client port
#define MT_TCP_NODELAY  0x01
#define MT_BUFFERSIZE  512
#define mt_printf printf        // Remove this define to stop seeing prints
#define mt_error  printf        // Remove this define to stop seeing errors prints
#define COMM_PROTOCOL_VERSION 1      // Protocol version (part of the message)

#define MSG_ID_RESPOND_BIT 0x80
enum comm_msgIDs
{
  RESET_MAC_MSG_ID    = 0x01,
  UPLOAD_PROGMODEL_MSG_ID  = 0x02,
  C100_MSG_ID        = 0x03
};

static int listenSocket = INVALID_SOCKET;
static int lNewMsgSocket = INVALID_SOCKET;
static int lCurrentMsgSocket = INVALID_SOCKET;
static unsigned int numThreads = 0; // the number of threads in the system
static int bIsBigEndian = 0;

int toEndSocketsServer = 0; // When set to 1, signals main application to end

static inline uint32 RevUInt32(uint32 val)
{
  if (bIsBigEndian)
  {
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) |
      ((val & 0xFF0000) >> 8) |(val >> 24);
  }
  return val; // WE ARE ALREADY IN A LITTLE-ENDIAN CPU !
}

/*****************************************************************************
*   Function Name:  CompareIP
*   Description:   
*           Compare IP address
*   Return Value:   MT_RET_OK - same string
                    MT_RET_FAIL - different string 
*   Algorithm:  Gets the sa_data[] address (of the sockaddr struct). It holds the
                port number (two first bytes) and the IP address (the successive
                four bytes) of the host that is connected to current task. It starts
                bytes compare from the third bytes on (the IP address).
*****************************************************************************/
static MT_RET CompareIP(const MT_UBYTE* string1, const MT_UBYTE* string2, MT_UINT16 len)
{
    MT_UINT16  i;

    /* Advance to point at the IP address */
    string1 += 2;
    string2 += 2;
  
    for(i = 0; i < (len-2); i++)
    {
        if(*(string1++) != *(string2++)) {
            break; /* Exit from loop is not equal */
    }

    /* Only look at first 4 bytes of IP. */
    if (i == 3) {
      i = (len-2);
      break;
    }
    }

    if(i == (len-2))
    {
        return MT_RET_OK;
    }
    else
    {
        return MT_RET_FAIL;
    }
}

/** Main thread function for the listen socket
Create listen socket, bind it to address and listen for connect request.
Non blocking is set in order to continues monitoring connection.
In a loop test for connect request:
In case connect was requested it creates task for the connect session.
At the BCL_RxMsgSocket(), in case there is an active message socket task, the IP of the host
connected is test:
If from the same host as the active message socket task
connection might disconnected without informing to target, therefore close old, open new.
Else
connection from other board is not allow when another one is active, therefore close the new.
**/
static MT_THREAD_RET_T listenSocketThread(void* unusedArg)
{
  long lMsgSocketTemp;
  int nOptVal = 1;
  long lResultStatus;
  int i;
  struct sockaddr_in myaddr_in, their_addr;    /* for local socket address */
  int count = 0;
  
  NAME_LEN_TYPE nHostNameLength;
  NAME_LEN_TYPE addrlen;
  struct sockaddr hostAddrNew, hostAddrOld;    /* for local socket address */
  
  memset(hostAddrNew.sa_data, 0, 14);
  memset(hostAddrOld.sa_data, 0, 14);
  
  numThreads++;
  mt_printf("listen socket thread = %d\n",_getpid());
  
  /* Clear out address structures */
  memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
  memset((char *)&their_addr, 0, sizeof(struct sockaddr_in));
  
  /* Create the socket */
  listenSocket = socket(AF_INET,SOCK_STREAM,0);
  myaddr_in.sin_port = htons(MT_SERVER_PORT_STREAM);
  
  if (listenSocket == INVALID_SOCKET)
  {
    mt_error("BCL Server Error --> can't create Socket!\n");
    toEndSocketsServer = 1;
    numThreads--;
    return MT_THREAD_RET;
  }
  
#ifndef WIN32
  /* Reuse the socket address, so bind doesn't fail*/
  setsockopt (listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&nOptVal, sizeof(int));
#endif /*WIN32*/
  
  /* Setup for bind */
  myaddr_in.sin_family = AF_INET;
  myaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
  for (i=0; i<8; i++)
    myaddr_in.sin_zero[i] = 0;
  
  /* Bind the socket */
  lResultStatus = bind(listenSocket,(struct sockaddr *)&myaddr_in, sizeof(myaddr_in));
  if (lResultStatus == -1)
  {
    mt_error("Server Error --> can't bind Socket!\n");
    socketClose(listenSocket);
    numThreads--;
    toEndSocketsServer = 1;
    return MT_THREAD_RET;
  }           
  
  addrlen =  sizeof (struct sockaddr_in);
  
  /* Wait for data */
  mt_printf("Your connection was done. Waiting for client connection\n");
  lResultStatus = listen(listenSocket,5);
  if (lResultStatus == -1) 
  {
    mt_error("Server Error --> can't listen on Socket!\n");
    socketClose(listenSocket);
    numThreads--;
    toEndSocketsServer = 1;
    return MT_THREAD_RET;
  }  
  /* Non blocking socket.
  The accept() and recv() are not blocking, but imediatelly get the status and continue in the function flow */
  /* ioctl (BCL_nListenSocket, FIONBIO, &nOptVal); */
  
  while(!toEndSocketsServer)
  {
  /*************************************
  * In a loop test for connect request.
  * In case connect was requested it creates task for the connect session.
  * At the BCL_RxMsgSocket(), in case there is an active message socket task, the IP of the host
  * connected is test:
  * If from the same host as the active message socket task -
  *  connection might disconnected without informing to target, therefore close old, open new.
  * Else
  *  connection from other board is not allow when another one is active, therefore close the new.
    */
    lMsgSocketTemp = accept(listenSocket,(struct sockaddr *)&their_addr,&addrlen);
    
    if (lMsgSocketTemp != INVALID_SOCKET)
    {
      //mt_printf("Comparing Their address:\n" );
      //BCL_IpCompare((MT_UBYTE *)hostAddrNew.sa_data, (MT_UBYTE *)(their_addr->sa_data), /*13*/sizeof (hostAddrNew.sa_data));
      if (lNewMsgSocket != INVALID_SOCKET)
      {
        nHostNameLength = sizeof (hostAddrNew);
        getpeername(lMsgSocketTemp, &hostAddrNew, &nHostNameLength);
        
        /* Linux Bugfix: if the old socket was closed, the old address gets NULLs, so kill the old thread. */
        if ((CompareIP((MT_UBYTE *)hostAddrNew.sa_data, (MT_UBYTE *)hostAddrOld.sa_data, /*13*/sizeof (hostAddrNew.sa_data))) == MT_RET_OK )
        {           
          long lMsgSocketPrev = lNewMsgSocket;
          mt_printf ("Socket -> Server closing old connection %ld\n",lMsgSocketPrev);
          lNewMsgSocket = INVALID_SOCKET; /* replace the socket to use */
          /* close old socket */
          socketClose (lMsgSocketPrev);
        }
        else
        {
          count = 0;
          mt_printf ("Socket -> Server closing new connection \n");
          socketClose (lMsgSocketTemp);
          lMsgSocketTemp = INVALID_SOCKET;
        }
      }
      
      /* if we got here, we already closed old socket, just set the new one */
      if (lMsgSocketTemp != INVALID_SOCKET)
      {
        char* IP;
        
        nHostNameLength = sizeof (hostAddrNew);
        getpeername(lMsgSocketTemp, &hostAddrOld, &nHostNameLength);
        
        IP = (char *)hostAddrOld.sa_data + 2;
        mt_printf("\n%d.%d.%d.%d - ", IP[0],IP[1],IP[2],IP[3]);
        mt_printf("open new socket %ld\n",lMsgSocketTemp);
        /*** Set socket options ***/
        
        /* Force small messages to be sent with no delay */
        setsockopt (lMsgSocketTemp, IPPROTO_TCP, MT_TCP_NODELAY, (char*)&nOptVal, sizeof(int));
        
        /* Close connection in case connection lost (default - 2 hours quiet to send keepalive message) */
        setsockopt (lMsgSocketTemp, SOL_SOCKET, SO_KEEPALIVE, (char*)&nOptVal, sizeof(int));
        /*ioctl (lMsgSocketTemp, FIONBIO, &nOptVal);*/
        
        /* set the active socket for the threads to use */
        lNewMsgSocket = lMsgSocketTemp;
        lMsgSocketTemp = INVALID_SOCKET;
      }
      
    }      
    /* Delay 10mSec */
    MT_SLEEP(10);
  }
  numThreads--;
  return MT_THREAD_RET;
}

static void sendRespondMsg(int msgID, const char* data, int dataLength)
{
  char* outBuf = malloc(dataLength + 8);
  outBuf[0] = 'M';
  outBuf[1] = 'T';
  outBuf[2] = COMM_PROTOCOL_VERSION;
  outBuf[3] = (MT_UBYTE)(msgID | MSG_ID_RESPOND_BIT);
  *((MT_UINT32*)(outBuf + 4)) = RevUInt32(dataLength); // Variable data length is 0
  if (data && dataLength > 0) memcpy(outBuf + 8, data, dataLength);
  send(lCurrentMsgSocket, outBuf, dataLength + 8, 0);
  free(outBuf);
}

/// Parse the data part of the upload progmodel message, and pass the command to the driver API
///
static void parseUploadProgmodelPacket(const char* data, int length)
{
  char progNames[2][256];
  unsigned int namesLen[2];

  char* newLinePtr = strchr(data, '\n');
  if (newLinePtr == NULL) 
  {
    mt_error("Invalid data in the reset MAC message.\n");
    return;
  }
  namesLen[0] = newLinePtr - data;
  namesLen[1] = length - namesLen[0] - 1;
  if (namesLen[0] >= 255 || namesLen[1] >= 255)
  {
    mt_error("Filenames of the prog models are too long (>=255 bytes).\n");
    return;
  }
  memcpy(progNames[0], data, namesLen[0]);
  progNames[0][namesLen[0]] = '\0';
  memcpy(progNames[1], newLinePtr + 1, namesLen[1]); // in case packet wasn't terminated with NULL character
  progNames[1][namesLen[1]] = '\0';

  driver_upload_progmodel(progNames[0], progNames[1]);
  sendRespondMsg(UPLOAD_PROGMODEL_MSG_ID, 0, 0);
}

/// Parse the data part of the C100 message, pass it to the driver API and send the respond C100 message back.
///
static void parseC100Packet(const char* data, int length)
{
  umi_c100 sendMsg;
  char recvMsg[RECV_C100_MSG_MAX_SIZE];
  if (length < sizeof(umi_c100) - UMI_C100_DATA_SIZE)
  {
    mt_error("Invalid C100 packet received from DUT client - too short.\n");
    return;
  }
  if (length > sizeof(sendMsg))
  {
    mt_error("Invalid C100 packet received from DUT client - too long.\n");
    return;
  }

  memcpy(&sendMsg, data, length);
  if (0 != driver_send_c100_command(&sendMsg, recvMsg))
  {
    mt_error("Failed to send C100 message");
    memset(recvMsg, 0, sizeof(recvMsg));
  }
  sendRespondMsg(C100_MSG_ID, recvMsg, RECV_C100_MSG_MAX_SIZE);
}

/** Parse the received packet and call the needed API function

  The assumption is that the packet is received as a whole. 
  If not, skip this packet (should never happen in our packet sizes)

**/
static void parsePacket(const char* inBuf, int packetLength)
{
  int dataLength, totalCalcedLength;
  int msgID;
  
  // Check the validity of the received packet
  if (packetLength < 8) { mt_error("Wrong packet received. Packet length = %d\n", packetLength); return; }
  if (inBuf[0] != 'M' || inBuf[1] != 'T') { mt_error("Wrong packet received. MT signature not found\n"); return; }
  dataLength = RevUInt32(*((MT_UINT32*)(inBuf + 4)));
  totalCalcedLength = dataLength + 8;
  if (totalCalcedLength != dataLength + 8) { mt_error("Received %d bytes when packet is only %d bytes.\n", packetLength, totalCalcedLength); return; }

  // OK, we have a good packet. Let's parse it
  msgID = (int)inBuf[3];
  if (msgID == RESET_MAC_MSG_ID)
  {
    // Restart the MAC (currently by restarting the driver)
    driver_restart((int)inBuf[8]); // first byte is wlan index
    sendRespondMsg(RESET_MAC_MSG_ID, 0, 0);
  }
  else if (msgID == UPLOAD_PROGMODEL_MSG_ID)
  {
    parseUploadProgmodelPacket(inBuf+8, dataLength);
  }
  else if (msgID == C100_MSG_ID)
  {
    parseC100Packet(inBuf+8, dataLength);
  }
  else
  {
    mt_error("Received msg %d. Don't know how to handle this message.\n", msgID);
  }
}

/*****************************************************************************
*   Function Name:  commSocketThread 
*   Description:   
*           Enables Socket comunication
*   Algorithm: 
            Clear all buffers.
            Test if there is additional (old) message socket task (lNewMsgSocket = INVALID_SOCKET if no task is active).
            If there is old active task:
                Get the IP of the old and the new message socket tasks.
                If new and old IP are the same:
                    Close the old task as it is not valid any more (probably was closed with no target notification).
                Else:
                    Other PC is not allow to connect in case PC is already connected. Close the new task and socket.
               lNewMsgSocket (and local lMsgSocketOld) is updated with the active task.
               Due to real time problem and IP constrains (IP sequence can change) lMsgSocketOld is used in this function
               (local variable) in addition to the golbal lNewMsgSocket variable.
               Get the task ID for all the operations required it and for future task delete when needed.
               Handle Rx data flow. Calls parsePacket when data is received
*****************************************************************************/
MT_THREAD_RET_T commSocketThread (void* unusedArg)
{
  long lResultStatus;
  char inBuf[MT_BUFFERSIZE];
  
  int testInt = 0x12345678;
  MT_UBYTE* pTestByte = (MT_UBYTE*)&testInt;
  bIsBigEndian = (*pTestByte == 0x12);

  numThreads++;
  mt_printf("comm socket thread = %d, %s-endian CPU\n",
      _getpid(), bIsBigEndian ? "big" : "little");

  while(!toEndSocketsServer)
  {  
    if (lCurrentMsgSocket == INVALID_SOCKET)
    {
      MT_SLEEP(10);
    }
    
    lResultStatus = -2;
    while (lResultStatus == -2 && lCurrentMsgSocket == lNewMsgSocket && lCurrentMsgSocket != INVALID_SOCKET && !toEndSocketsServer) 
    {
      lResultStatus = recv(lCurrentMsgSocket, inBuf, MT_BUFFERSIZE, 0);
      
      if (lResultStatus == 0)
      {
        mt_printf ("Socket -> Client closed connection \n");
        /* it may be that the msgsocket was closed from the outside */
        socketClose (lCurrentMsgSocket);
        /* if this is also the last socket, clear it */
        if (lCurrentMsgSocket == lNewMsgSocket) 
          lNewMsgSocket = INVALID_SOCKET;
        /* set current to invalid */
        lCurrentMsgSocket = INVALID_SOCKET;

        driver_stop();

        if (disconnect_cmd) {
          mt_printf("Executing system command: %s\n", disconnect_cmd);
          system(disconnect_cmd);
        }
      }
      else if (lResultStatus != -1) 
      {
        // OK, received packet
        parsePacket(inBuf, lResultStatus);
      }
      
    }
  
    /* check if the socket was replaced */
    if (lCurrentMsgSocket != lNewMsgSocket)
    {
      mt_printf("Replacing socket %d with %d\n",lCurrentMsgSocket,lNewMsgSocket);
      /* try to close old socket */
      if (lCurrentMsgSocket != INVALID_SOCKET)
      {
        socketClose(lCurrentMsgSocket);
        mt_printf("closed old socket %d\n",lCurrentMsgSocket);
      }
      
      /* replace current socket */
      lCurrentMsgSocket = lNewMsgSocket;
    }
    
  }
  
  numThreads--;
  return MT_THREAD_RET;
}


/// Starts the sockets server thread
int StartSocketsServer()
{
#ifdef WIN32
    WORD wVersionRequested = MAKEWORD( 2, 0 );
    WSADATA wsaData;
    int err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) 
    {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        mt_error("Winsock couldn't be initialized");
        return MT_RET_FAIL;
    }
    // Begin windows socket server threads
    _beginthread(listenSocketThread, 0, (void*)0);
    _beginthread(commSocketThread, 0, (void*)0);
#else // LINUX
    /* Disable broken pipe signals - this allows error handling, 
       instead of killing the program on broken pipe*/
    pthread_t listenSocketThreadID;
    pthread_t commSocketThreadID;
    signal(SIGPIPE, SIG_IGN);    
    pthread_create(&listenSocketThreadID, NULL, listenSocketThread, (void*)0);
    pthread_create(&commSocketThreadID, NULL, commSocketThread, (void*)0);
#endif
  return MT_RET_OK;
}

void EndSocketsServer()
{
  toEndSocketsServer = 1;
  socketClose(listenSocket);
  socketClose(lCurrentMsgSocket);
  socketClose(lNewMsgSocket);
  MT_SLEEP(100);
  while (numThreads)
  {
    mt_printf("Waiting for threads to die...\n");
    MT_SLEEP(250);
  }
  mt_printf("Socket server ended successfully.\n");
}
