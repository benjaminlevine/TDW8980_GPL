#ifndef _RELAY_H_
#define _RELAY_H_

#include "dhcps.h"

void process_relay_request(struct iface_config_t *iface, char *pkt, int len);
void process_relay_response(struct iface_config_t *iface);

#endif

