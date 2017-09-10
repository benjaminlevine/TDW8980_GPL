/******************************************************************************
 *
 * Copyright (c) 2008-2008 TP-Link Technologies CO.,LTD.
 * All rights reserved.
 *
 * �ļ�����:	libipt_multiurl.c.h
 * ��    ��:	1.0
 * ժ    Ҫ:	Shared library add-on to iptables
 *
 * ��    ��:	ZJin <zhongjin@tp-link.net>
 * ��������:	10/23/2008
 *
 * �޸���ʷ:	yangxv, fit 2.6.22 kernel version, 2010.4.16
----------------------------
 *
 ******************************************************************************/

/* for example */
/*
	iptables -A FORWARD -m multiurl --urls www.baidu.com,sina,163 -p tcp --dport 80 -j ACCEPT

	netfilter will find the urls in every http-GET pkt, if found, means matched!
*/

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>

#include <linux/netfilter/xt_multiurl.h>
#include <linux/netfilter_ipv4/ipt_multiurl.h>

/* Function which prints out usage message. */
static void help(void)
{
    printf(
    "multiurl v%s options:\n"
    "  www.baidu.com,sina,gov            Match urls\n"
    , MULTIURL_VERSION);
}

static struct option opts[] = {
    { "urls",   1, 0, '1' },
    {0}
};

/* Initialize the match. */
static void init(struct ipt_entry_match *m)
{
	struct ipt_multiurl_info *info = (struct ipt_multiurl_info *)m->data;
	//*nfcache |= NFC_UNKNOWN;
	info->url_count = 0;
	//printf("here is init\n");
}

static void print_multiurl(const struct ipt_multiurl_info *info)
{
    int i;
    for (i = 0; i < info->url_count; i++)
	{
		printf("%s", info->urls[i]);
        if (i < info->url_count - 1) printf(",");
        if (i == info->url_count - 1) printf(" ");
    }
}

/*
	This function resolve the string "www.baidu.com,sina,163,xxx,sex", which multiurl point to it.
*/
static void parse_url(char *multiurl, struct ipt_multiurl_info *info)
{
    char *next, *prev;
    int count = 0;

    prev = multiurl;
    next = multiurl;

	//printf("debug: before pase\n");

    do
	{
        if ((next = strchr(next, ',')) != NULL)
        {			
            *next++ = '\0';
        }

		if (strlen(prev) >= IPT_MULTIURL_STRLEN)
			exit_error(PARAMETER_PROBLEM, "multiurl match: too long '%s'\n", prev);
		
		if (count >= IPT_MULTIURL_MAX_URL)
			exit_error(PARAMETER_PROBLEM, "multiurl match: too many url specified (MAX = %d)\n", IPT_MULTIURL_MAX_URL);
		
		strncpy(info->urls[count], prev, strlen(prev));

		//printf("multiurl_debug: url_%d, %s\n", count, info->urls[count]);
		
        count++;
        prev = next;

		if ((next + 1) == 0)
		{
			break;
		}
    } while (next);

    info->url_count = count;
}

/* Function which parses command options; returns true if it
   ate an option */
static int parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
      struct ipt_entry_match **match)
{
	struct ipt_multiurl_info *info = (struct ipt_multiurl_info *)(*match)->data;

    check_inverse(optarg, &invert, &optind, 0);
    if (invert) exit_error(PARAMETER_PROBLEM, "Sorry, you can't have an inverted comment");

	if (c == '1')
	{
		parse_url(argv[optind - 1], info);
	}
	else
	{
		return 0;
	}
	
	if (*flags)
		exit_error(PARAMETER_PROBLEM, "multiurl can only have one option");

	*flags = IPT_MULTIURL;

	return 1;
}

/* Final check; must specify something. */
static void final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM, "multiurl expection an option --urls");
}

/* Prints out the matchinfo. */
static void print(const void *ip,
      const struct ipt_entry_match *match,
      int numeric)
{

    const struct ipt_multiurl_info *info = (const struct ipt_multiurl_info *)match->data;

    printf("multiurl ");
	printf("--urls ");
	print_multiurl(info);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const void *ip, const struct ipt_entry_match *match)
{
	const struct ipt_multiurl_info *info = (const struct ipt_multiurl_info *)match->data;
	print_multiurl(info);
}

static struct xtables_match multiurl = { 
	.name		= "multiurl",
	.family		= AF_INET,
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct ipt_multiurl_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_multiurl_info)),
	.help		= help,
	.init		= init,
	.parse		= parse,
	.final_check= final_check,
	.print		= print,
	.save		= save,
	.extra_opts	= opts
};

void _init(void)
{
	xtables_register_match(&multiurl);
}

