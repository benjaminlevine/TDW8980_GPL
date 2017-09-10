/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		libxt_weektime.c
 * brief		
 * details	
 *
 * author	wangwenhao
 * version	
 * date		25Oct11
 *
 * history \arg	1.0, 25Oct11, wangwenhao, create file
 */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
	 
#include <linux/netfilter/xt_weektime.h>

/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/
#define HALFHOURS_IN_HALFDAY 24

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           EXTERN_PROTOTYPES                                    */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           LOCAL_PROTOTYPES                                     */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/
static struct option opts[] = {
    { "MONAM",   1, 0, 0 },
	{ "MONPM",   1, 0, 1 },
	{ "TUEAM",   1, 0, 2 },
	{ "TUEPM",   1, 0, 3 },
	{ "WEDPM",   1, 0, 4 },
	{ "WEDAM",   1, 0, 5 },
	{ "THUPM",   1, 0, 6 },
	{ "THUAM",   1, 0, 7 },
	{ "FRIAM",   1, 0, 8 },
	{ "FRIPM",   1, 0, 9 },
	{ "SATPM",   1, 0, 10 },
	{ "SATAM",   1, 0, 11 },
	{ "SUNPM",   1, 0, 12 },
	{ "SUNAM",   1, 0, 13 },
    {0}
};


/**************************************************************************************************/
/*                                           LOCAL_FUNCTIONS                                      */
/**************************************************************************************************/
static void help(void)
{
    printf(
    "weektime v%s set time segments match in a week by half-hours. Options:\n"
    "  --MONAM, --MONPM, --TUEAM, --TUEPM, --WEDPM, --WEDAM, --THUPM, --THUAM,\n"
    "  --FRIAM, --FRIPM, --SATPM, --SATAM, --SUNPM, --SUNAM\n"
    "  All with hex-value of 24bit-used unsigned int. Each bit mean a half-hour\n"
    "starts with 00:00PM.\n"
    , WEEKTIME_VERSION);
}

static void init(struct xt_entry_match *m)
{
	struct xt_weektime_info *info = (void *)m->data;
	
	memset(info, 0, sizeof(struct xt_weektime_info));
}

static int parse(int c, char **argv, int invert, unsigned int *flags,
                      const void *entry, struct xt_entry_match **match)
{
	struct xt_weektime_info *info = (void *)(*match)->data;
	long tmpBits;

	if (c < 0 || c > 13)
	{
		return 0;
	}

	if (*flags & (1 << c))
	{
		exit_error(PARAMETER_PROBLEM,
					"Cannot specify --%s twice", opts[(unsigned int) c].name);
	}

	if (invert)
	{
		exit_error(PARAMETER_PROBLEM,
					"Unexpected \"!\" with --%s", opts[(unsigned int) c].name);
	}

	tmpBits = strtol(optarg, NULL, 0);
	if (tmpBits <= 0 || tmpBits >= (1 << HALFHOURS_IN_HALFDAY))
	{
		exit_error(PARAMETER_PROBLEM,
					"Value with --%s must be 1-24bits 0 or 1, higher 8 bits invalid with 0",
					opts[(unsigned int) c].name);
	}

	info->timeBits[(unsigned int) c] = (unsigned int)tmpBits;
	return 1;
}

static void print(const void *ip, const struct xt_entry_match *match,
                       int numeric)
{
	printf("WEEKTIME ");
}

static void save(const void *ip, const struct xt_entry_match *match)
{
	int index;
	const struct xt_weektime_info *info = (const void *)match->data;

	for (index = 0; index < WEEKTIME_DAY_HALFS; index++)
	{
		printf("--%s 0x%x ", opts[index].name, info->timeBits[index]);
	}
}

static struct xtables_match weektime_match = {
	.name          = "weektime",
	.family        = AF_INET,
	.version       = IPTABLES_VERSION,
	.size          = XT_ALIGN(sizeof(struct xt_weektime_info)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_weektime_info)),
	.help          = help,
	.init          = init,
	.parse         = parse,
	.print         = print,
	.save          = save,
	.extra_opts    = opts,
};

static struct xtables_match weektime_match6 = {
	.name          = "weektime",
	.family        = AF_INET6,
	.version       = IPTABLES_VERSION,
	.size          = XT_ALIGN(sizeof(struct xt_weektime_info)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_weektime_info)),
	.help          = help,
	.init          = init,
	.parse         = parse,
	.print         = print,
	.save          = save,
	.extra_opts    = opts,
};


/**************************************************************************************************/
/*                                           PUBLIC_FUNCTIONS                                     */
/**************************************************************************************************/
void _init(void)
{
	xtables_register_match(&weektime_match);
	xtables_register_match(&weektime_match6);
}

/**************************************************************************************************/
/*                                           GLOBAL_FUNCTIONS                                     */
/**************************************************************************************************/

