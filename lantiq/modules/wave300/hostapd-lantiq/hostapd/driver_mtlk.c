/*
 * hostapd / Driver interaction with Metalink WLANPlus 802.11n driver
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if_arp.h>
#include "wireless_copy.h"

#include <netinet/in.h>
#include <netpacket/packet.h>

#include "hostapd.h"
#include "driver.h"
#include "ieee802_1x.h"
#include "eloop.h"
#include "priv_netlink.h"
#include "sta_info.h"
#include "l2_packet/l2_packet.h"
#include "hostap_common.h"

#include "eapol_sm.h"
#include "wpa.h"
#include "radius/radius.h"
#include "ieee802_11.h"
#include "accounting.h"
#include "common.h"

enum ietypes {
	IE_WSC_BEACON     = 0,
	IE_WSC_PROBE_REQ  = 1,
	IE_WSC_PROBE_RESP = 2
};

struct mtlk_driver_data {
	struct hostapd_data *hapd;		/* back pointer */

	char	iface[IFNAMSIZ + 1];
	int     ifindex;
	struct l2_packet_data *sock_xmit;	/* raw packet xmit socket */
	struct l2_packet_data *sock_recv;	/* raw packet recv socket */
	int	ioctl_sock;			/* socket for ioctl() use */
	int	wext_sock;			/* socket for wireless events */
	int	we_version;
	u8	acct_mac[ETH_ALEN];
	struct hostap_sta_driver_data acct_data;
};

static const char *
ether_sprintf(const u8 *addr)
{
	static char buf[sizeof(MACSTR)];

	if (addr != NULL)
		snprintf(buf, sizeof(buf), MACSTR, MAC2STR(addr));
	else
		snprintf(buf, sizeof(buf), MACSTR, 0,0,0,0,0,0);
	return buf;
}

static int
mtlk_set_iface_flags(void *priv, int dev_up)
{
	struct mtlk_driver_data *drv = priv;
	struct ifreq ifr;

	wpa_printf(MSG_DEBUG, "%s: dev_up=%d", __func__, dev_up);

	if (drv->ioctl_sock < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", drv->iface);

	if (ioctl(drv->ioctl_sock, SIOCGIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		return -1;
	}

	if (dev_up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	if (ioctl(drv->ioctl_sock, SIOCSIFFLAGS, &ifr) != 0) {
		perror("ioctl[SIOCSIFFLAGS]");
		return -1;
	}

	if (dev_up) {
		memset(&ifr, 0, sizeof(ifr));
		snprintf(ifr.ifr_name, IFNAMSIZ, "%s", drv->iface);
		ifr.ifr_mtu = HOSTAPD_MTU;
		if (ioctl(drv->ioctl_sock, SIOCSIFMTU, &ifr) != 0) {
			perror("ioctl[SIOCSIFMTU]");
			printf("Setting MTU failed - trying to survive with "
			       "current value\n");
		}
	}

	return 0;
}

static int
mtlk_set_encryption(const char *ifname, void *priv, const char *alg,
	     const u8 *addr, int key_idx,
	     const u8 *key, size_t key_len, int txkey)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	struct iw_encode_ext *ext;
	int ret=0;

	wpa_printf(MSG_DEBUG,
		"%s: alg=%s addr=%s key_idx=%d txkey=%d",
		__func__, alg, ether_sprintf(addr), key_idx, txkey);

	ext = malloc(sizeof(*ext) + key_len);
	if (ext == NULL)
		return -1;
	memset(ext, 0, sizeof(*ext) + key_len);
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.encoding.flags = key_idx + 1;
	iwr.u.encoding.pointer = (caddr_t) ext;
	iwr.u.encoding.length = sizeof(*ext) + key_len;

	if (addr == NULL ||
	    memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0)
		ext->ext_flags |= IW_ENCODE_EXT_GROUP_KEY;
	if (txkey)
		ext->ext_flags |= IW_ENCODE_EXT_SET_TX_KEY;
	
	ext->addr.sa_family = ARPHRD_ETHER;
	if (addr)
		memcpy(ext->addr.sa_data, addr, ETH_ALEN);
	else
		memset(ext->addr.sa_data, 0xff, ETH_ALEN);
	if (key && key_len) {
		memcpy(ext + 1, key, key_len);
		ext->key_len = key_len;
	}

	if (strcmp(alg, "none") == 0)
		ext->alg = IW_ENCODE_ALG_NONE;
	else if (strcmp(alg, "WEP") == 0)
		ext->alg = IW_ENCODE_ALG_WEP;
	else if (strcmp(alg, "TKIP") == 0)
		ext->alg = IW_ENCODE_ALG_TKIP;
	else if (strcmp(alg, "CCMP") == 0)
		ext->alg = IW_ENCODE_ALG_CCMP;
	else {
		printf("%s: unknown/unsupported algorithm %s\n",
			__func__, alg);
		return -1;
	}

	if (ioctl(drv->ioctl_sock, SIOCSIWENCODEEXT, &iwr) < 0) {
		ret = errno == EOPNOTSUPP ? -2 : -1;
		perror("ioctl[SIOCSIWENCODEEXT]");
	}

	free(ext);
	return ret;
}


static int
mtlk_get_seqnum(const char *ifname, void *priv, const u8 *addr, int idx, u8 *seq)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	struct iw_encode_ext *ext;
	int ret=0;

	wpa_printf(MSG_DEBUG,
		"%s: addr=%s idx=%d", __func__, ether_sprintf(addr), idx);

	ext = malloc(sizeof(*ext));
	if (ext == NULL)
		return -1;
	memset(ext, 0, sizeof(*ext));
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.encoding.pointer = (caddr_t) ext;
	iwr.u.encoding.length = sizeof(*ext);

	if (addr == NULL ||
	    memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0)
		iwr.u.encoding.flags |= IW_ENCODE_EXT_GROUP_KEY;

	if (ioctl(drv->ioctl_sock, SIOCGIWENCODEEXT, &iwr) < 0) {
		ret = errno == EOPNOTSUPP ? -2 : -1;
		perror("ioctl[SIOCGIWENCODEEXT]");
		goto err;
	}

	memcpy(seq, ext->rx_seq, 6);
err:
	free(ext);
	return ret;
}


static int 
mtlk_flush(void *priv)
{
	return 0;		/* XXX */
}



static int
mtlk_sta_clear_stats(void *priv, const u8 *addr)
{
	return 0; /* FIX */
}


static int
mtlk_set_generic_elem(const char *ifname, void *priv, const u8 *ie, size_t ie_len)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) ie;
	iwr.u.data.length = ie_len;

	if (ioctl(drv->ioctl_sock, SIOCSIWGENIE, &iwr) < 0) {
		perror("ioctl[SIOCSIWGENIE]");
		ret = -1;
	}

	return ret;
}

static int
mtlk_mlme(struct mtlk_driver_data *drv,
	const u8 *addr, int cmd, int reason_code)
{
	struct iwreq iwr;
	struct iw_mlme mlme;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	memset(&mlme, 0, sizeof(mlme));
	mlme.cmd = cmd;
	mlme.reason_code = reason_code;
	mlme.addr.sa_family = ARPHRD_ETHER;
	memcpy(mlme.addr.sa_data, addr, ETH_ALEN);
	iwr.u.data.pointer = (caddr_t) &mlme;
	iwr.u.data.length = sizeof(mlme);

	if (ioctl(drv->ioctl_sock, SIOCSIWMLME, &iwr) < 0) {
		perror("ioctl[SIOCSIWMLME]");
		ret = -1;
	}

	return ret;
}

static int
mtlk_sta_deauth(void *priv, const u8 *addr, int reason_code)
{
	struct mtlk_driver_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return mtlk_mlme(drv, addr, IW_MLME_DEAUTH, reason_code);
}

static int
mtlk_sta_disassoc(void *priv, const u8 *addr, int reason_code)
{
	struct mtlk_driver_data *drv = priv;
	wpa_printf(MSG_DEBUG, "%s", __FUNCTION__);
	return mtlk_mlme(drv, addr, IW_MLME_DISASSOC, reason_code);
}

static int
mtlk_del_sta(struct mtlk_driver_data *drv, u8 *addr)
{
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		HOSTAPD_LEVEL_INFO, "disassociated");

	sta = ap_get_sta(hapd, addr);
	if (sta != NULL) {
		sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC);
		//wpa_sm_event(hapd, sta, WPA_DISASSOC);
        wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
		sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
		ap_free_sta(hapd, sta);
	}
	return 0;
}

static int
mtlk_process_wpa_ie(struct mtlk_driver_data *drv, struct sta_info *sta,
			u8 *rsnie)
{
	struct hostapd_data *hapd = drv->hapd;
	int ielen, res;

	ielen = rsnie[1] +2;

	if (sta->wpa_sm == NULL)
		sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth, sta->addr);
	if (sta->wpa_sm == NULL) {
		printf("Failed to initialize WPA state machine\n");
		return -1;
	}

	res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm, rsnie, ielen,
					NULL, 0);
	if (res != WPA_IE_OK) {
		printf("WPA/RSN information element rejected? (res %u)\n", res);
		return -1;
	}
	return 0;
}

static int
mtlk_new_sta(struct mtlk_driver_data *drv, u8 *addr, u8 *rsnie)
{
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;
	int new_assoc;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		HOSTAPD_LEVEL_INFO, "associated");

	sta = ap_get_sta(hapd, addr);
	if (sta) {
		accounting_sta_stop(hapd, sta);
	} else {
		sta = ap_sta_add(hapd, addr);
		if (sta == NULL)
			return -1;
	}

	if (memcmp(addr, drv->acct_mac, ETH_ALEN) == 0) {
		/* Cached accounting data is not valid anymore. */
		memset(drv->acct_mac, 0, ETH_ALEN);
		memset(&drv->acct_data, 0, sizeof(drv->acct_data));
	}

	if (hapd->conf->wpa && rsnie) {
		if (mtlk_process_wpa_ie(drv, sta, rsnie))
			return -1;
	}

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && !rsnie) {
		wpa_printf(MSG_DEBUG, "STA did not include WPA/RSN IE "
			   "in (Re)Association Request - possible WPS use");
		sta->flags |= WLAN_STA_MAYBE_WPS;
	}
#endif /* CONFIG_WPS */


	/*
	 * Now that the internal station state is setup
	 * kick the authenticator into action.
	 */
	new_assoc = (sta->flags & WLAN_STA_ASSOC) == 0;
	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC;
	wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);
	hostapd_new_assoc_sta(hapd, sta, !new_assoc);
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);
	return 0;
}

static int
mtlk_wireless_michaelmicfailure(struct mtlk_driver_data *drv,
				const char *ev, int len)
{
	const struct iw_michaelmicfailure *mic;
	u8 *addr;

	if (len < sizeof(*mic)) {
		wpa_printf(MSG_DEBUG,
			"Invalid MIC Failure data from driver");
		return -1;
	}

	mic = (const struct iw_michaelmicfailure *) ev;

	addr = (u8*) mic->src_addr.sa_data;
	wpa_printf(MSG_DEBUG,
		"Michael MIC failure wireless event: "
		"flags=0x%x src_addr=" MACSTR, mic->flags, MAC2STR(addr));

	ieee80211_michael_mic_failure(drv->hapd, addr, 1);

	return 0;
}

static void
mtlk_wireless_event_wireless_custom(struct mtlk_driver_data *drv,
				       char *custom)
{
	const char newsta_tag[] = "NEWSTA ";
	const char rsnie_tag[]  = "RSNIE_LEN ";

	wpa_printf(MSG_DEBUG, "Custom wireless event: '%s'",
		      custom);

	if (strncmp(custom, newsta_tag, strlen(newsta_tag)) == 0) {
		char *pos = custom;
		u8 addr[ETH_ALEN];
		u8 *rsnie, ielen;
		pos += strlen(newsta_tag);
		if (hwaddr_aton(pos, addr) != 0) {
			wpa_printf(MSG_DEBUG,
				"NEWSTA with invalid MAC address");
			return;
		}
		pos = strstr(pos, rsnie_tag);
		pos += strlen(rsnie_tag);
		ielen = atoi(pos);
		if (!ielen) {
			wpa_printf(MSG_DEBUG,
				"NEWSTA with zero RSNIE length?");
			return;
		}
		rsnie = malloc(ielen);
		if (!rsnie) {
			printf("ERROR: can't allocate buffer "
				"of %i bytes for RSNIE", ielen);
			return;
		}
		pos = strstr(pos, " : ");
		pos += 3;
		hexstr2bin(pos, rsnie, ielen);
		mtlk_new_sta(drv, addr, rsnie);
		free(rsnie);
	} else
	if (strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		char *pos;
		u8 addr[ETH_ALEN];
		pos = strstr(custom, "addr=");
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG,
				      "MLME-MICHAELMICFAILURE.indication "
				      "without sender address ignored");
			return;
		}
		pos += 5;
		if (hwaddr_aton(pos, addr) == 0) {
			ieee80211_michael_mic_failure(drv->hapd, addr, 1);
		} else {
			wpa_printf(MSG_DEBUG,
				      "MLME-MICHAELMICFAILURE.indication "
				      "with invalid MAC address");
		}
	} else if (strncmp(custom, "STA-TRAFFIC-STAT", 16) == 0) {
		char *key, *value;
		u32 val;
		key = custom;
		while ((key = strchr(key, '\n')) != NULL) {
			key++;
			value = strchr(key, '=');
			if (value == NULL)
				continue;
			*value++ = '\0';
			val = strtoul(value, NULL, 10);
			if (strcmp(key, "mac") == 0)
				hwaddr_aton(value, drv->acct_mac);
			else if (strcmp(key, "rx_packets") == 0)
				drv->acct_data.rx_packets = val;
			else if (strcmp(key, "tx_packets") == 0)
				drv->acct_data.tx_packets = val;
			else if (strcmp(key, "rx_bytes") == 0)
				drv->acct_data.rx_bytes = val;
			else if (strcmp(key, "tx_bytes") == 0)
				drv->acct_data.tx_bytes = val;
			key = value;
		}
	}
	else if (strncmp(custom, "PUSH-BUTTON.indication", 22) == 0) {
		/* Some atheros kernels send push button as a wireless event */
		/* PROBLEM! this event is received for ALL BSSs ...
		 * so all are enabled for WPS... ugh.
		 */
		 //printf("push button recieved\n");
		hostapd_wps_button_pushed(drv->hapd);
	} 
}

static void
mtlk_wireless_event_wireless(struct mtlk_driver_data *drv,
					    char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		wpa_printf(MSG_DEBUG, "Wireless event: "
			      "cmd=0x%x len=%d", iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		iwe->u.data.pointer = custom;
		if (drv->we_version > 18 &&
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
		     iwe->cmd == IWEVCUSTOM)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN,
			       sizeof(struct iw_event) - dlen);
		} else {
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case IWEVEXPIRED:
			mtlk_del_sta(drv, (u8 *) iwe->u.addr.sa_data);
			break;
		case IWEVREGISTERED:
			mtlk_new_sta(drv, (u8 *) iwe->u.addr.sa_data, NULL);
			break;
		case IWEVMICHAELMICFAILURE:
			mtlk_wireless_michaelmicfailure(drv, custom,
							iwe->u.data.length);
			break;
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end)
				return;
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;		/* XXX */
			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			mtlk_wireless_event_wireless_custom(drv, buf);
			free(buf);
			break;
		}

		pos += iwe->len;
	}
}


static void
mtlk_wireless_event_rtm_newlink(struct mtlk_driver_data *drv,
					       struct nlmsghdr *h, int len)
{
	struct ifinfomsg *ifi;
	int attrlen, nlmsg_len, rta_len;
	struct rtattr * attr;

	if (len < sizeof(*ifi))
		return;

	ifi = NLMSG_DATA(h);

	if (ifi->ifi_index != drv->ifindex)
		return;

	nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	attrlen = h->nlmsg_len - nlmsg_len;
	if (attrlen < 0)
		return;

	attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			mtlk_wireless_event_wireless(
				drv, ((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static void
mtlk_wireless_event_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	char buf[256];
	int left;
	struct sockaddr_nl from;
	socklen_t fromlen;
	struct nlmsghdr *h;
	struct mtlk_driver_data *drv = eloop_ctx;

	fromlen = sizeof(from);
	left = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
			(struct sockaddr *) &from, &fromlen);
	if (left < 0) {
		if (errno != EINTR && errno != EAGAIN)
			perror("recvfrom(netlink)");
		return;
	}

	h = (struct nlmsghdr *) buf;
	while (left >= sizeof(*h)) {
		int len, plen;

		len = h->nlmsg_len;
		plen = len - sizeof(*h);
		if (len > left || plen < 0) {
			/*
			printf("Malformed netlink message: "
			       "len=%d left=%d plen=%d\n",
			       len, left, plen);*/
			break;
		}

		switch (h->nlmsg_type) {
		case RTM_NEWLINK:
			mtlk_wireless_event_rtm_newlink(drv, h, plen);
			break;
		}

		len = NLMSG_ALIGN(len);
		left -= len;
		h = (struct nlmsghdr *) ((char *) h + len);
	}

	if (left > 0) {
		//printf("%d extra bytes in the end of netlink message\n", left);
	}
}


static int
mtlk_get_we_version(struct mtlk_driver_data *drv)
{
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	drv->we_version = 0;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = malloc(buflen);
	if (range == NULL)
		return -1;
	memset(range, 0, buflen);

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) range;
	iwr.u.data.length = buflen;

	minlen = ((char *) &range->enc_capa) - (char *) range +
		sizeof(range->enc_capa);

	if (ioctl(drv->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		perror("ioctl[SIOCGIWRANGE]");
		free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		wpa_printf(MSG_DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		drv->we_version = range->we_version_compiled;
	}

	free(range);
	return 0;
}


static int
mtlk_wireless_event_init(void *priv)
{
	struct mtlk_driver_data *drv = priv;
	int s;
	struct sockaddr_nl local;

	mtlk_get_we_version(drv);

	drv->wext_sock = -1;

	s = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (s < 0) {
		perror("socket(PF_NETLINK,SOCK_RAW,NETLINK_ROUTE)");
		return -1;
	}

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = RTMGRP_LINK;
	if (bind(s, (struct sockaddr *) &local, sizeof(local)) < 0) {
		perror("bind(netlink)");
		close(s);
		return -1;
	}

	eloop_register_read_sock(s, mtlk_wireless_event_receive, drv, NULL);
	drv->wext_sock = s;

	return 0;
}


static void
mtlk_wireless_event_deinit(void *priv)
{
	struct mtlk_driver_data *drv = priv;

	if (drv != NULL) {
		if (drv->wext_sock < 0)
			return;
		eloop_unregister_read_sock(drv->wext_sock);
		close(drv->wext_sock);
	}
}


static int
mtlk_send_eapol(void *priv, const u8 *addr, const u8 *data, size_t data_len,
        int encrypt, const u8 *own_addr)
{
	struct mtlk_driver_data *drv = priv;
	unsigned char buf[3000];
	unsigned char *bp = buf;
	struct l2_ethhdr *eth;
	size_t len;
	int status;

	/*
	 * Prepend the Ethernet header.  If the caller left us
	 * space at the front we could just insert it but since
	 * we don't know we copy to a local buffer.  Given the frequency
	 * and size of frames this probably doesn't matter.
	 */
	len = data_len + sizeof(struct l2_ethhdr);
	if (len > sizeof(buf)) {
		bp = malloc(len);
		if (bp == NULL) {
			printf("EAPOL frame discarded, cannot malloc temp "
			       "buffer of size %lu!\n", (unsigned long) len);
			return -1;
		}
	}
	eth = (struct l2_ethhdr *) bp;
	memcpy(eth->h_dest, addr, ETH_ALEN);
	memcpy(eth->h_source, drv->hapd->own_addr, ETH_ALEN);
	eth->h_proto = htons(ETH_P_EAPOL);
	memcpy(eth+1, data, data_len);

	status = l2_packet_send(drv->sock_xmit, addr, ETH_P_EAPOL, bp, len);

	if (bp != buf)
		free(bp);
	return status;
}

static void
handle_read(void *ctx, const u8 *src_addr, const u8 *buf, size_t len)
{
	struct mtlk_driver_data *drv = ctx;
	struct hostapd_data *hapd = drv->hapd;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, src_addr);
	if (!sta || !(sta->flags & WLAN_STA_ASSOC)) {
		printf("Data frame from not associated STA %s\n",
		       ether_sprintf(src_addr));
		/* XXX cannot happen */
		return;
	}
	ieee802_1x_receive(hapd, src_addr, buf + sizeof(struct l2_ethhdr),
			   len - sizeof(struct l2_ethhdr));
}

static void *
mtlk_init(struct hostapd_data *hapd)
{
	struct mtlk_driver_data *drv;
	struct ifreq ifr;
	struct iwreq iwr;

	drv = os_zalloc(sizeof(struct mtlk_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for mtlk driver data\n");
		goto bad;
	}

	drv->hapd = hapd;
	drv->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (drv->ioctl_sock < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		goto bad;
	}
	memcpy(drv->iface, hapd->conf->iface, sizeof(drv->iface));

	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", drv->iface);
	if (ioctl(drv->ioctl_sock, SIOCGIFINDEX, &ifr) != 0) {
		perror("ioctl(SIOCGIFINDEX)");
		goto bad;
	}
	drv->ifindex = ifr.ifr_ifindex;

	drv->sock_xmit = l2_packet_init(drv->iface, NULL, ETH_P_EAPOL,
					handle_read, drv, 1);
	if (drv->sock_xmit == NULL)
		goto bad;
	if (l2_packet_get_own_addr(drv->sock_xmit, hapd->own_addr))
		goto bad;
	if (hapd->conf->bridge[0] != '\0') {
		wpa_printf(MSG_DEBUG,
			"Configure bridge %s for EAPOL traffic.",
			hapd->conf->bridge);
		drv->sock_recv = l2_packet_init(hapd->conf->bridge, NULL,
						ETH_P_EAPOL, handle_read, drv,
						1);
		if (drv->sock_recv == NULL)
			goto bad;
	} else
		drv->sock_recv = drv->sock_xmit;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);

	iwr.u.mode = IW_MODE_MASTER;

	if (ioctl(drv->ioctl_sock, SIOCSIWMODE, &iwr) < 0) {
		perror("ioctl[SIOCSIWMODE]");
		printf("Could not set interface to master mode!\n");
		goto bad;
	}

	mtlk_set_iface_flags(drv, 0);	/* mark down during setup */

	return drv;
bad:
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv != NULL)
		free(drv);
	return NULL;
}


static void
mtlk_deinit(void *priv)
{
	struct mtlk_driver_data *drv = priv;

	drv->hapd->driver = NULL;

	(void) mtlk_set_iface_flags(drv, 0);
	if (drv->ioctl_sock >= 0)
		close(drv->ioctl_sock);
	if (drv->sock_recv != NULL && drv->sock_recv != drv->sock_xmit)
		l2_packet_deinit(drv->sock_recv);
	if (drv->sock_xmit != NULL)
		l2_packet_deinit(drv->sock_xmit);
	free(drv);
}

static int
mtlk_set_ssid(const char *ifname, void *priv, const u8 *buf, int len)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len + 1;

	if (ioctl(drv->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCSIWESSID]");
		printf("len=%d\n", len);
		return -1;
	}
	return 0;
}

static int
mtlk_get_ssid(const char *ifname, void *priv, u8 *buf, int len)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = 32;/*if len == 33, it will cause error, so set it to 32, edited by panxuzhou*/

	if (ioctl(drv->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		perror("ioctl[SIOCGIWESSID]");
		ret = -1;
	} else
		ret = iwr.u.essid.length;

	return ret;
}

static int
mtlk_commit(void *priv)
{
	return mtlk_set_iface_flags(priv, 1);
}

#ifdef CONFIG_WPS
static int
mtlk_set_wps_ie(void *priv, const u8 *ie, size_t ie_len, u16 ie_type)
{
	struct mtlk_driver_data *drv = priv;
	struct iwreq iwr;
	int ret = 0;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, drv->iface, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t) ie;
	iwr.u.data.length = ie_len;
	iwr.u.data.flags = ie_type;

	if (ioctl(drv->ioctl_sock, SIOCSIWGENIE, &iwr) < 0) {
		perror("ioctl[SIOCSIWGENIE]");
		ret = -1;
	}

	return ret;
}

static int
mtlk_set_wps_beacon_ie(const char *ifname, void *priv, const u8 *iebuf,
			size_t iebuflen)
{
	return mtlk_set_wps_ie(priv, iebuf, iebuflen, IE_WSC_BEACON);
}

static int
mtlk_set_wps_probe_resp_ie(const char *ifname, void *priv, const u8 *iebuf,
				size_t iebuflen)
{
	return mtlk_set_wps_ie(priv, iebuf, iebuflen, IE_WSC_PROBE_RESP);
}
#endif

const struct wpa_driver_ops wpa_driver_mtlk_ops = {
	.name			= "mtlk",
	.init			= mtlk_init,
	.deinit			= mtlk_deinit,
	.set_encryption		= mtlk_set_encryption,
	.get_seqnum		= mtlk_get_seqnum,
	.flush			= mtlk_flush,
	.set_generic_elem	= mtlk_set_generic_elem,
	.wireless_event_init	= mtlk_wireless_event_init,
	.wireless_event_deinit	= mtlk_wireless_event_deinit,
	.send_eapol		= mtlk_send_eapol,
	.sta_disassoc		= mtlk_sta_disassoc,
	.sta_deauth		= mtlk_sta_deauth,
	.set_ssid		= mtlk_set_ssid,
	.get_ssid		= mtlk_get_ssid,
	.sta_clear_stats        = mtlk_sta_clear_stats,
	.commit			= mtlk_commit,
#ifdef CONFIG_WPS
	.set_wps_beacon_ie      = mtlk_set_wps_beacon_ie,
	.set_wps_probe_resp_ie  = mtlk_set_wps_probe_resp_ie,
#endif
};
