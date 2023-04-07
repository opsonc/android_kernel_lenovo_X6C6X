
#ifndef __PACKFILTER_h_Include__
#define __PACKFILTER_h_Include__

#define MAX_NAME_LENGTH			128
#define MAX_ITEM_COUNT			28
#define MAX_ITEM_COUNT2			152
#ifdef __HANDLE_CNAME__
#define MAX_CACHE_HOSTID_CNT		24
#else
#define MAX_CACHE_HOSTID_CNT		64
#endif
#define DEV_PACKFILTER_NAME		"wjfirewall"
#define DEV_PF_FULLNAME			"/dev/wjfirewall"
#define NETLINK_PACKFILTER		29
#define ALLOCMEM_HIGH_FLAG		GFP_ATOMIC
#define ALLOCMEM_MIDD_FLAG		GFP_ATOMIC
#define ALLOCMEM_ZERO_FLAG		__GFP_ZERO

#ifdef __DEBUG__
#define PRINTK			printk
#else
#define PRINTK(...)
#endif

#define MALLOC(size, flag)	kmalloc(size, flag)
#define FREE(p)				kfree(p)

#pragma pack (push, 8)

typedef uint32_t TYPE_IP_V4;
typedef struct
{
	uint64_t LOWER;
	uint64_t UPPER;
} TYPE_IP_V6;

typedef char TYPE_NAME[MAX_NAME_LENGTH];
typedef uint32_t FLAG_FILTER;

#define FLAG_FILTER_DEFAULT	0x00
#define	FLAG_FILTER_WHITE	0x01
#define FLAG_FILTER_BLACK	0x02


typedef uint32_t IPADDR_TYPE;

#define IPADDR_TYPE_V4	0x00
#define IPADDR_TYPE_V6	0x01

#define HOST_TYPE_NONE	0x00
#define HOST_TYPE_IP	0x01
#define HOST_TYPE_DOMAIN	0x02

typedef struct
{
	union
	{
		TYPE_IP_V6 uIpV6;
		TYPE_IP_V4 uIpV4;
	};
	IPADDR_TYPE nType;
} SIpAddr;

typedef struct
{
	int32_t nLen;
	TYPE_NAME sName;
} SHost;

typedef struct
{
	uint16_t	Begin;
	uint16_t	End;
} SPort;

typedef struct
{
	SPort	BL[MAX_ITEM_COUNT];
	SPort	WL[MAX_ITEM_COUNT];
	int16_t	blCnt;
	int16_t	wlCnt;
} SFilterPort;

typedef struct
{
	SIpAddr		IpBegin;
        SIpAddr		IpEnd;
	FLAG_FILTER	FlagFilter;
	SFilterPort	FilterPort;
} SFilterIp;

typedef struct
{
	__kernel_uid32_t uUid;
	FLAG_FILTER FlagFilter;
} SFilterUid;

union UAnswer
{
	SIpAddr IpAddr;
#ifdef __HANDLE_CNAME__
	SHost RedirHost;
#endif
};

typedef struct
{
	SHost Host;
	int32_t nHostType[MAX_CACHE_HOSTID_CNT];
	int32_t nCount;
	union UAnswer Answers[MAX_CACHE_HOSTID_CNT];
} SDnsResp;

typedef struct
{
	SHost OriHost;
	SHost Host;
	int32_t nCount;
	SIpAddr Items[MAX_ITEM_COUNT2];
	int32_t nDotCnt;
} SDnsResp2;

#ifdef __cplusplus
struct SFilterHost
#else
typedef struct
#endif
{
	SHost Host;
	FLAG_FILTER	FlagFilter;
	SFilterPort	FilterPort;
#ifdef __cplusplus
	SFilterHost *pNext;
};
#else
	struct SFilterHost *pNext;
} SFilterHost;
#endif


#if 0
#define PACKFILTER_IOCTL_BASE	'y'
#define PACKFILTER_IOCTL_SET_FILTERIP 	_IOW(PACKFILTER_IOCTL_BASE, 0x80, SFilterItem)
#define PACKFILTER_IOCTL_SET_FILTERHOST _IOW(PACKFILTER_IOCTL_BASE, 0x81, SFilterItem)
#define PACKFILTER_IOCTL_GET_NEWIP 		_IOR(PACKFILTER_IOCTL_BASE, 0x82, SHostId)
#define PACKFILTER_IOCTL_GET_IPSTATRSU	_IOWR(PACKFILTER_IOCTL_BASE, 0x83, SNodeStatRsu)
#define PACKFILTER_IOCTL_SET_DEFAULTFILTER	_IOW(PACKFILTER_IOCTL_BASE, 0x84, FLAG_FILTER)
#define PACKFILTER_IOCTL_GET_DNS_REQ_PACK	_IOWR(PACKFILTER_IOCTL_BASE, 0x85, void *)
#define PACKFILTER_IOCTL_GET_DNS_RES_PACK	_IOWR(PACKFILTER_IOCTL_BASE, 0x86, void *)
#define PACKFILTER_IOCTL_SET_ENABLED		_IOW(PACKFILTER_IOCTL_BASE, 0x87, int)
#define PACKFILTER_IOCTL_SET_REINIT			_IOW(PACKFILTER_IOCTL_BASE, 0x88, )
#define PACKFILTER_IOCTL_SET_REINIT			_IOW(PACKFILTER_IOCTL_BASE, 0x89, SFilterUid)
#else
#define PACKFILTER_IOCTL_SET_FILTERIP	 	0x80
#define PACKFILTER_IOCTL_SET_FILTERHOST 	0x81
#define PACKFILTER_IOCTL_GET_NEWIP	 	0x82
#define PACKFILTER_IOCTL_SET_DEFAULTFILTER	0x84
#define PACKFILTER_IOCTL_GET_DNS_REQ_PACK	0x85
#define PACKFILTER_IOCTL_GET_DNS_RES_PACK	0x86
#define PACKFILTER_IOCTL_SET_ENABLED		0x87
#define PACKFILTER_IOCTL_SET_REINIT		0x88
#define PACKFILTER_IOCTL_SET_FILTERUID	 	0x89
#define PACKFILTER_IOCTL_SET_CLRUID	 	0x8C
#define PACKFILTER_IOCTL_SET_CLRIPHOST	 	0x8D
#endif

static inline void MEMCPY(void *pDest, const void *pSrc, int nLen)
{
	int i;

	for (i = 0; i < nLen; ++ i)
	{
		*((unsigned char *)pDest + i) = *((unsigned char *)pSrc + i);
	}
}

static inline void MEMCPY_INVT(void *pDest, const void *pSrc, int nLen)
{
	if (nLen > 0)
	{
		int i;
		unsigned char *pS = ((unsigned char *)pSrc) + (nLen - 1);

		for (i = 0; i < nLen; ++ i)
		{
			*((unsigned char *)pDest + i) = *pS;
			--pS;
		}
	}
}

static inline bool u128_eq(const TYPE_IP_V6 *pA, const TYPE_IP_V6 *pB)
{
	return ((pA->UPPER == pB->UPPER) && (pA->LOWER == pB->LOWER));
}

static inline bool u128_ne(const TYPE_IP_V6 *pA, const TYPE_IP_V6 *pB)
{
	return ((pA->UPPER != pB->UPPER) | (pA->LOWER != pB->LOWER));
}

static inline bool u128_gt(const TYPE_IP_V6 *pA, const TYPE_IP_V6 *pB)
{
	if (pA->UPPER == pB->UPPER)
	{
		return (pA->LOWER > pB->LOWER);
	}
	return (pA->UPPER > pB->UPPER);
}

static inline bool u128_lt(const TYPE_IP_V6 *pA, const TYPE_IP_V6 *pB)
{
	if (pA->UPPER == pB->UPPER)
	{
		return (pA->LOWER < pB->LOWER);
	}
	return (pA->UPPER < pB->UPPER);
}

static inline bool u128_ge(const TYPE_IP_V6 *pA, const TYPE_IP_V6 *pB)
{
	return (u128_gt(pA, pB) || u128_eq(pA, pB));
}

static inline bool u128_le(const TYPE_IP_V6 *pA, const TYPE_IP_V6 *pB)
{
	return (u128_lt(pA, pB) || u128_eq(pA, pB));
}


static inline int Domain_Cmp(const char* sStr1, int nLen1, const char* sStr2, int nLen2)
{
	-- nLen1;
	-- nLen2;
	while ((nLen1 >= 0) && (nLen2 >= 0))
	{
		if (*(sStr1 + nLen1) == *(sStr2 + nLen2))
		{
			-- nLen1;
			-- nLen2;
		}
		else if ((*(sStr1 + nLen1) == '*') || (*(sStr2 + nLen2) == '*'))
		{
			return 0;
		}
		else if (*(sStr1 + nLen1) > *(sStr2 + nLen2))
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}

	if (nLen1 < 0)
	{
		if (nLen2 < 0)
		{
			return 0;
		}
		else if ((*(sStr2 + nLen2) == '*')
			|| ((nLen2 == 1) && (*sStr2 == '*') && (*(sStr2 + 1) == '.')))
		{
			return 0;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		if ((*(sStr1 + nLen1) == '*')
			|| ((nLen1 == 1) && (*sStr1 == '*') && (*(sStr1 + 1) == '.')))
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
}


#pragma pack (pop)

#endif
