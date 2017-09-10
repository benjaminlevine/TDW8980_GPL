#ifndef	_XT_MULTI_URL_H_
#define	_XT_MULTI_URL_H_


#define MULTIURL_VERSION			"0.0.1"

#define XT_MULTIURL_MAX_URL		10
#define	XT_MULTIURL_STRLEN		31

#define XT_MULTIURL		1 << 1

struct xt_multiurl_info
{
	int		url_count;
	char	urls[XT_MULTIURL_MAX_URL][XT_MULTIURL_STRLEN];
};


#endif
