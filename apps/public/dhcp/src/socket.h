/* socket.h */
#include "dhcps.h"

#ifndef _SOCKET_H
#define _SOCKET_H

int listen_socket(uint32_t ip, int port, char *inf);
int listen_socket_on_ip(uint32_t ip, int port);

#endif
