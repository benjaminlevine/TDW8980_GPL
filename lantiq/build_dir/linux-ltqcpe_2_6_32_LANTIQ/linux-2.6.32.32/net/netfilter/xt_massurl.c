/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		xt_massurl.c
 * brief		
 * details	
 *
 * author	wangwenhao
 * version	
 * date		24Oct11
 *
 * history \arg	1.0, 24Oct11, wangwenhao, create file
 *         \arg 1.1, 25Nov11, zhulu, modified some function interfaces for kernel 2.6.30.9
 *
 * note		we use hash methed to do string match between "DNS domain/HTTP Host" and mass strings
 * 			configured by user
 *			if we use hash method CPU cost is:	l * (2 + n / x * (2 + c / (c - 1)))
 *			if we use mormal methoed then:		l * n * (c / (c - 1) + 1)
 *
 *			'l' means length of the HTTP host
 *			'n' means count of mass strings
 *			'c' means charactors used in HTTP host
 *			'x' means member count of hash array
 *
 *			so if 'n' gets large and 'x' gets big enough, hash method is much better than normal
 */
 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_massurl.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <asm/uaccess.h>
#include <net/netfilter/nf_conntrack_ecache.h>

#ifndef CONFIG_NF_CONNTRACK_MARK
# error "MASSURL need CONNTRACK_MARK selected"
#endif

/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/
#define	HOST_STR	"\r\nHost: "

#define URL_HASH_EXP 5
#define URL_HASH_SIZE (1 << URL_HASH_EXP)
#define URL_HASH_MASK (URL_HASH_SIZE - 1)

#define DNS_HEADER_LEN 12


#define HASH(a) ((unsigned char)(a) & URL_HASH_MASK)

#define U32_BIT_MASK 0x1f
#define BIT_ISSET(index, set) (set[index >> 5] & (1 << (index & U32_BIT_MASK)))

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/
struct hash_url {
	struct hlist_node node;
	char url[MASSURL_URL_LEN];
	unsigned char offset;
	unsigned short index;
};

typedef int (*CMP_FUNC)(const void *, const void *, size_t);

/**************************************************************************************************/
/*                                           EXTERN_PROTOTYPES                                    */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_PROTOTYPES                                     */
/**************************************************************************************************/
static bool match(const struct sk_buff *skb, const struct xt_match_param *par);

static bool checkentry(const struct xt_mtchk_param *par);
/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/


static DEFINE_RWLOCK(massurl_lock);

static struct xt_match xt_massurl_match[] = { 
	{
	.family			= AF_INET,
    .name           = "massurl",
    .match          = &match,
    .matchsize		= sizeof(struct xt_massurl_info),
    .checkentry     = &checkentry,
    .me             = THIS_MODULE,
	},
	{
	.family			= AF_INET6,
    .name           = "massurl",
    .match          = &match,
    .matchsize		= sizeof(struct xt_massurl_info),
    .checkentry     = &checkentry,
    .me             = THIS_MODULE,
	},
};

static struct hash_url urls[MASSURL_MAX_ENTRY];

static struct hlist_head urlHash[URL_HASH_SIZE];

unsigned short urlHashCount[URL_HASH_SIZE];

/**************************************************************************************************/
/*                                           LOCAL_FUNCTIONS                                      */
/**************************************************************************************************/
static unsigned char *url_strstr(const unsigned char *start,
								const unsigned char *end,
								const unsigned char *strCharSet)
{
	unsigned char *s_temp = (unsigned char *)start;        /*the s_temp point to the s*/

	int l1, l2;

	l2 = strlen(strCharSet);
	if (!l2)
		return (unsigned char *)start;

	l1 = end - s_temp;

	while (l1 >= l2)
	{
		l1--;
		if (!memcmp(s_temp, strCharSet, l2))
			return (unsigned char *)s_temp;
		s_temp++;
	}

	return NULL;
}

int cmpdns(const void *url, const void *dns, size_t len)
{
	const unsigned char *uUrl, *uDns;
	unsigned char tmpCount = 0;
	unsigned char count = 0;

	for (uUrl = url, uDns = dns; len > 0; len--, uUrl++, uDns++)
	{
		if (*uUrl == '.')
		{
			if (tmpCount != count && tmpCount != 0)
			{
				return 1;
			}

			tmpCount = *uDns;
			count = 0;
		}
		else
		{
			if (*uUrl != *uDns)
			{
				return 1;
			}
			count++;
		}
	}

	return 0;
}

static int url_strhash(const unsigned char *start,
				const unsigned char *end,
				unsigned int *pIndexBits,
				unsigned int type)
{
	const unsigned char *pIndex = start;
	const unsigned char *offStart;
	struct hash_url *pUrl;
	struct hlist_node *pNode;
	size_t len;

	read_lock(&massurl_lock);
	while (pIndex < end)
	{
		hlist_for_each_entry(pUrl, pNode, &urlHash[HASH(*pIndex)], node)
		{
			if (!BIT_ISSET(pUrl->index, pIndexBits))
			{
				continue;
			}
			
			offStart = pIndex - pUrl->offset;
			if (offStart < start)
			{
				continue;
			}

			len = strlen(pUrl->url);
			if (end - offStart < len)
			{
				continue;
			}
			
			if (MASSURL_TYPE_HTTP == type && 0 == memcmp((unsigned char *)pUrl->url, offStart, len))
			{
				read_unlock(&massurl_lock);
				/* printk("!!!!!!!!!!http packet caught!!!!!!!!!\n"); */
				return 1;
			}
			
			if (MASSURL_TYPE_DNS == type && 0 == cmpdns((unsigned char *)pUrl->url, offStart, len))
			{
				read_unlock(&massurl_lock);
				/* printk("!!!!!!!!!!dns packet caught!!!!!!!!!\n"); */
				return 1;
			}
		}

		pIndex++;
	}
	
	read_unlock(&massurl_lock);
	return 0;
}

static int setUrl(void __user *user, unsigned int userLen)
{
	struct massurl_url_info urlInfo;
	size_t len;
	struct hash_url *pUrl;
	char *pIndex;
	int hash;
	int minCount;
	int minHash;

	if (copy_from_user(&urlInfo, user, sizeof(urlInfo)) != 0)
	{
		return -EFAULT;
	}

	if (urlInfo.index >= MASSURL_MAX_ENTRY)
	{
		printk(KERN_WARNING "url index overflow\n");
		return -EINVAL;
	}

	pUrl = &urls[urlInfo.index];

	len = strlen(urlInfo.url);
	write_lock_bh(&massurl_lock);
	if (len == 0)
	{
/*
		if (pUrl->url[0] == '\0')
		{
			write_unlock_bh(&massurl_lock);
			printk(KERN_WARNING "can not del index empty\n");
			return -EINVAL;
		}
*/
		if (pUrl->url[0] != '\0')
		{
			urlHashCount[HASH(pUrl->url[pUrl->offset])]--;
			hlist_del(&pUrl->node);
			pUrl->url[0] = '\0';
			pUrl->offset = 0;
		}
	}
	else
	{
		if (pUrl->url[0] != '\0')
		{
			write_unlock_bh(&massurl_lock);
			printk(KERN_WARNING "can not add to already exist index\n");
			return -EINVAL;
		}
		
		strncpy(pUrl->url, urlInfo.url, MASSURL_URL_LEN);
		pUrl->url[MASSURL_URL_LEN - 1] = '\0';

		minCount = MASSURL_MAX_ENTRY + 1;
		minHash = HASH('.');	/* defult . for all . string */
		for (pIndex = pUrl->url; *pIndex != '\0'; pIndex++)
		{
			if (*pIndex == '.')
			{
				continue;
			}
			
			hash = HASH(*pIndex);

			if (urlHashCount[hash] < minCount)
			{
				minHash = hash;
				minCount = urlHashCount[hash];
				pUrl->offset = (unsigned char)(pIndex - pUrl->url);
			}
		}
		hlist_add_head(&pUrl->node, &urlHash[minHash]);
		urlHashCount[minHash]++;
		/*printk("###add url %s to hash %d\n", pUrl->url, minHash);*/
	}
	write_unlock_bh(&massurl_lock);

	return 0;
}

static int match_http(const struct sk_buff *skb, struct xt_massurl_info *info)
{
	const struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = (void *)iph + iph->ihl * 4;

	unsigned char *start;
	unsigned char *end;

	start = (unsigned char *)tcph + tcph->doff * 4;
	end = start + (iph->tot_len - iph->ihl * 4 - tcph->doff * 4);

	if (start >= end)
	{
		if (info->type == MASSURL_TYPE_HTTP)
		{
			return 2;
		}
		else
		{
			return 0;
		}
	}

	start = url_strstr(start, end, HOST_STR);
	if (start == NULL)
	{
		return 0;
	}

	start += 8;
	end = url_strstr(start, end, "\r\n");
	
	if (end == NULL)
	{
		return 0;
	}

	if (url_strhash(start, end, info->urlIndexBits, MASSURL_TYPE_HTTP))
	{
		return (info->type == MASSURL_TYPE_HTTP) ? 1 : 2;
	}
	return 0;
}

static int match_dns(const struct sk_buff *skb, struct xt_massurl_info *info)
{
	const struct iphdr *iph = ip_hdr(skb);
	const struct udphdr *udph = (void *)iph + iph->ihl * 4;
	
	return url_strhash((unsigned char *)udph + sizeof(struct udphdr) + DNS_HEADER_LEN,
		(unsigned char *)udph + udph->len,
		info->urlIndexBits,
		MASSURL_TYPE_DNS);
}

static bool match(const struct sk_buff *skb, const struct xt_match_param *par)
{
	struct xt_massurl_info *info = (struct xt_massurl_info *)(par->matchinfo);
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	int ret = 0;

	ct = nf_ct_get(skb, &ctinfo);
	if (ct == NULL)
	{
		printk("MASSURL: get conntrak failed\n");
		return 0;
	}
	ct->mark |= 0x40000000;

	if (info->type == MASSURL_TYPE_HTTP || info->type == MASSURL_TYPE_URL)
	{
		ret = match_http(skb, info);
	}
	else if (info->type == MASSURL_TYPE_DNS)
	{
		ret = match_dns(skb, info);
	}

	if (ret == 1)
	{
		/* printk("set connmark!!!!!!\n"); */
		ct->mark |= 0x80000000;
	}

	if (ret)
	{
		return 1;
	}

	return 0;
}


static bool checkentry(const struct xt_mtchk_param *par)
{
	struct xt_massurl_info *info = (struct xt_massurl_info *)(par->matchinfo);

	if (info->type < MASSURL_TYPE_HTTP && info->type > MASSURL_TYPE_DNS)
	{
		printk(KERN_WARNING "massurl: type can only be 'http' or 'dns'\n");
		return 0;
	}
	
	return 1;
}

static int url_list_show(char *buf, char **start, off_t off, int count,
                 int *eof, void *data)
{
	int n = 0;
	int index;
	int first = 0;
	struct hash_url *pUrl;
	struct hlist_node *pNode;

	n += sprintf(buf + n, "hash    count   index   offset  urls\n");
	for (index = 0; index < URL_HASH_SIZE; index++)
	{
		first = 1;

 		hlist_for_each_entry(pUrl, pNode, &urlHash[index], node)
 		{
			if (first)
			{
				n += sprintf(buf + n, "%-6d  %-6d  ", index, urlHashCount[index]);
				first = 0;
			}
			else
			{
				n += sprintf(buf + n, "                ");
			}
			n += sprintf(buf + n, "%-6d  %-6d  %s\n", pUrl->index, pUrl->offset, pUrl->url);
 		}
	}
	return n;
}

static int __init init(void)
{
	int index;
	for (index = 0; index < MASSURL_MAX_ENTRY; index++)
	{
		urls[index].index = index;
	}
	
	ipt_ctl_hook_url = setUrl;
	create_proc_read_entry("url_list", 0, NULL, url_list_show, NULL);

	return xt_register_matches(xt_massurl_match, ARRAY_SIZE(xt_massurl_match));
}

static void __exit fini(void)
{
	ipt_ctl_hook_url = NULL;
	xt_unregister_matches(xt_massurl_match, ARRAY_SIZE(xt_massurl_match));
}

/**************************************************************************************************/
/*                                           PUBLIC_FUNCTIONS                                     */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           GLOBAL_FUNCTIONS                                     */
/**************************************************************************************************/

module_init(init);
module_exit(fini);

MODULE_AUTHOR("Wang Wenhao <wangwenhao@tp-link.net>");
MODULE_DESCRIPTION("netfilter massurl match");
MODULE_LICENSE("GPL");


