/* dhcps.h */
#ifndef _DHCPS_H
#define _DHCPS_H

#include <netinet/ip.h>
#include <netinet/udp.h>

#include "version.h"
#include "common.h"

struct option_set {
	uint8_t *data;
	struct option_set *next;
};

struct static_lease {
	uint8_t mac[6];
	uint32_t ip;
};

#include "leases.h"
#include "static_leases.h"

/* the period of time the client is allowed to use that address */
#define LEASE_TIME              (60*60*24*10) /* 10 days of seconds */
#define LEASES_FILE		"/var/tmp/udhcpd.leases"

/* where to find the DHCP server configuration file */
#define DHCPD_CONF_FILE         "/var/tmp/dconf/udhcpd.conf"

#define MAX_PORT_NAME_LEN 16
#define MAX_VENDOR_ID_LEN 128
#define MAX_FILE_NAME_LEN 256
#define MAX_BOOTP_SRV_NAME_LEN 16

#define MAX_CLIENT_NUM 253
#define MAX_COND_SERVER_POOLS 8 
#define MAX_STATIC_LEASES_NUM 32

#define MAX_OPTION_VALUE_LEN 128

#ifdef DHCP_COND_SRV_POOL
typedef enum
{
    PC = 1,
    CAMERA,
    HGW,
    STB,
    PHONE,
    UNKNOWN
}DEVICE_TYPE;

/* Struct to store conditional server pool config. */
struct cond_server_pool_t
{
    uint32_t start;
    uint32_t end;
    uint32_t gateway;
    char vendor_id[MAX_VENDOR_ID_LEN];
    DEVICE_TYPE deviceTpye;
    uint32_t optionCode;
    char optionStr[MAX_OPTION_VALUE_LEN];
    uint32_t dns[2];
};
#endif

#ifdef DHCP_RELAY
struct relay_skt_t
{
    int skt;
    char ifName[MAX_INTF_NAME];
    uint32_t ip;
};
#endif

/* Struct to store public server config.*/
struct server_config_t {
	unsigned long decline_time; 	/* how long an address is reserved if a client returns a
				    	             * decline message */
	unsigned long conflict_time; 	/* how long an arp conflict offender is leased for */
	unsigned long offer_time; 	/* how long an offered address is reserved */
	unsigned long min_lease; 	/* minimum lease a client can request*/
	char lease_file[MAX_FILE_NAME_LEN];
};

/* Struct to store private server config of each lan.*/
struct iface_config_t {
    int in_use;         /* This struct is in use if set 1.*/
    int fd;             /* Berkeley Packet Filter Device in vxworks OS or skt in linux OS */
    uint32_t server;		/* Our IP, in network order */
    uint32_t netmask;       /* Our netmask */
	uint32_t start;		/* Start address of leases, network order */
	uint32_t end;			/* End of leases, network order */
	struct option_set *options;	/* List of DHCP options loaded from the config file */
	char ifName[MAX_INTF_NAME];		/* The name of the interface to use */
	int ifindex;			/* Index number of the interface to use */
	uint8_t arp[6];		/* Our arp address */
	unsigned long lease;		/* lease time in seconds (host order) */
	uint32_t siaddr;		/* next server bootp option */
	char sname[MAX_BOOTP_SRV_NAME_LEN];			/* bootp server name */
	char boot_file[MAX_FILE_NAME_LEN];		/* bootp boot file option */
#ifdef DHCP_RELAY
    uint32_t dhcps_remote; /* upper level dhcp server's IP address, network order. */
    int relay_skt;         /* skt for relay, listen on ip.*/
    char relay_ifName[MAX_INTF_NAME];
#endif
#ifdef DHCP_COND_SRV_POOL
    struct cond_server_pool_t cond_server_pools[MAX_COND_SERVER_POOLS]; /* conditional poos */
#endif    
};

extern struct server_config_t server_config;
extern struct iface_config_t iface_configs[MAX_GROUP_NUM];
extern struct dhcpOfferedAddr leases[MAX_CLIENT_NUM];
extern struct static_lease static_leases[MAX_STATIC_LEASES_NUM];
extern struct iface_config_t* cur_iface_config;
#ifdef DHCP_RELAY
extern struct relay_skt_t relay_skts[MAX_WAN_NUM];
#endif

struct dhcpOfferedAddr *leases_shm;

#endif
