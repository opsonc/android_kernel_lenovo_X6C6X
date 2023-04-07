#include <linux/types.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/config.h>
#endif
#if defined(CONFIG_SMP) && ! defined(__SMP__)
#define __SMP__
#endif
#if defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
#define MODVERSIONS
#endif

#ifdef MODVERSIONS
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include <linux/modversions.h>
#else
#include <config/modversions.h>
#endif
#endif


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/skbuff.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/udp.h>
#include <linux/mm.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/ip6_checksum.h>

#include "btree2.h"
#include "wjfw.h"

#define	SPIN_LOCK
#define	SPIN_LOCK_SAVE
#define VERSION_STRING	"VER 1.6.8"

#define PROT_IPV4	0x0008
#define PROT_IPV6	0xDD86
#define BROADCAST_MASK	0xFF000000
#define LOOP_MASK		0x0000007F
#define INVALID_POS		-1
#define UDP_HEADER_LEN	8

#ifdef SPIN_LOCK
#ifdef SPIN_LOCK_SAVE
#define LOCK_IP_TREE		spin_lock_irqsave(&g_ip_tree_lock, g_ip_tree_lock_flags)
#define UNLOCK_IP_TREE		spin_unlock_irqrestore(&g_ip_tree_lock, g_ip_tree_lock_flags)
#define LOCK_UID_TREE		spin_lock_irqsave(&g_uid_tree_lock, g_uid_tree_lock_flags)
#define UNLOCK_UID_TREE		spin_unlock_irqrestore(&g_uid_tree_lock, g_uid_tree_lock_flags)
#define LOCK_HOST_TREE		spin_lock_irqsave(&g_host_tree_lock, g_host_tree_lock_flags)
#define UNLOCK_HOST_TREE	spin_unlock_irqrestore(&g_host_tree_lock, g_host_tree_lock_flags)
#define LOCK_DCACHE_LIST	spin_lock_irqsave(&g_dcache_list_lock, g_dcache_list_lock_flags)
#define UNLOCK_DCACHE_LIST	spin_unlock_irqrestore(&g_dcache_list_lock, g_dcache_list_lock_flags)
#define LOCK_DNSIP_TREE		spin_lock_irqsave(&g_dnsip_tree_lock, g_dnsip_tree_lock_flags)
#define UNLOCK_DNSIP_TREE	spin_unlock_irqrestore(&g_dnsip_tree_lock, g_dnsip_tree_lock_flags)
#ifdef __MY_THREAD__
#define LOCK_SKB_LIST		spin_lock_irqsave(&g_skb_delay_lock, g_skb_delay_list_lock_flags)
#define UNLOCK_SKB_LIST		spin_unlock_irqrestore(&g_skb_delay_lock, g_skb_delay_list_lock_flags)
#endif
#else
#define LOCK_IP_TREE		spin_lock_bh(&g_ip_tree_lock)
#define UNLOCK_IP_TREE		spin_unlock_bh(&g_ip_tree_lock)
#define LOCK_UID_TREE		spin_lock_bh(&g_uid_tree_lock)
#define UNLOCK_UID_TREE		spin_unlock_bh(&g_uid_tree_lock)
#define LOCK_HOST_TREE		spin_lock_bh(&g_host_tree_lock)
#define UNLOCK_HOST_TREE	spin_unlock_bh(&g_host_tree_lock)
#define LOCK_DCACHE_LIST	spin_lock_bh(&g_dcache_list_lock)
#define UNLOCK_DCACHE_LIST	spin_unlock_bh(&g_dcache_list_lock)
#define LOCK_DNSIP_TREE		spin_lock_bh(&g_dnsip_tree_lock)
#define UNLOCK_DNSIP_TREE	spin_unlock_bh(&g_dnsip_tree_lock)
#endif
#else
#define LOCK_IP_TREE		mutex_lock(&g_ip_tree_lock)
#define UNLOCK_IP_TREE		mutex_unlock(&g_ip_tree_lock)
#define LOCK_UID_TREE		mutex_lock(&g_uid_tree_lock)
#define UNLOCK_UID_TREE		mutex_unlock(&g_uid_tree_lock)
#define LOCK_HOST_TREE		mutex_lock(&g_host_tree_lock)
#define UNLOCK_HOST_TREE	mutex_unlock(&g_host_tree_lock)
#define LOCK_DCACHE_LIST	mutex_lock(&g_dcache_list_lock)
#define UNLOCK_DCACHE_LIST	mutex_unlock(&g_dcache_list_lock)
#define LOCK_DNSIP_TREE		mutex_lock(&g_dnsip_tree_lock)
#define UNLOCK_DNSIP_TREE	mutex_unlock(&g_dnsip_tree_lock)
#endif


struct SDnsCacheItem
{
	SHost Host;
	int32_t nHostType[MAX_CACHE_HOSTID_CNT];
	int32_t nCount;
	union UAnswer Answers[MAX_CACHE_HOSTID_CNT];
	struct SDnsCacheItem *pNext;
};

struct SIpNode
{
	SIpAddr		IpBegin;
	SIpAddr		IpEnd;
	FLAG_FILTER	FlagFilter;
	SFilterPort	FilterPort;
	struct SIpNode *pNext;
};

typedef SFilterUid SUidNode;


static struct nf_hook_ops nfho_in4;
static struct nf_hook_ops nfho_out4;
static struct nf_hook_ops nfho_in6;
static struct nf_hook_ops nfho_out6;


static BTREE *g_ip_tree = NULL;
static BTREE *g_uid_tree = NULL;
static BTREE *g_host_tree = NULL;
static struct SDnsCacheItem *g_dcache_list = NULL;
#ifdef __ALLOW_DIRECT_IP__
static BTREE *g_dnsip_tree = NULL;
#endif

#ifdef SPIN_LOCK
static spinlock_t g_ip_tree_lock;
static spinlock_t g_uid_tree_lock;
static spinlock_t g_host_tree_lock;
static spinlock_t g_dcache_list_lock;
#ifdef SPIN_LOCK_SAVE
static unsigned long g_ip_tree_lock_flags;
static unsigned long g_uid_tree_lock_flags;
static unsigned long g_host_tree_lock_flags;
static unsigned long g_dcache_list_lock_flags;
#endif
#ifdef __ALLOW_DIRECT_IP__
static spinlock_t g_dnsip_tree_lock;
#endif
#else
static struct mutex g_ip_tree_lock;
static struct mutex g_uid_tree_lock;
static struct mutex g_host_tree_lock;
static struct mutex g_dcache_list_lock;
#ifdef __ALLOW_DIRECT_IP__
static struct mutex g_dnsip_tree_lock;
#endif
#endif
static SFilterPort g_FilterPort;
static FLAG_FILTER g_DefaultFilter = FLAG_FILTER_BLACK;


struct netlink_kernel_cfg nl_cfg;
static struct sock *g_socket = NULL;
static int g_nPid = 0;
static int32_t g_nEnabled = 0;
static int g_nInUse = 0;

#ifdef __MY_THREAD__
struct SSkbDelay
{
	struct sk_buff *skb;
	unsigned long time;
	struct SSkbDelay *pNext;
	char dev_name[IFNAMSIZ];
};
static spinlock_t g_skb_delay_lock;
static unsigned long g_skb_delay_list_lock_flags;
static struct SSkbDelay *g_skb_delay_list = NULL;
struct task_struct *g_pThdTask = NULL;
#endif

static void init_uid(void)
{
	g_uid_tree = btree_new();
}

static void init_ip_host(void)
{

	g_ip_tree = btree_new();
	g_host_tree = btree_new();

	g_FilterPort.wlCnt = 0;
	g_FilterPort.blCnt = 0;
}

static void init_data(void)
{
	BTREE_ORDER = 15;
	BTREE_ORDER_HALF = BTREE_ORDER >> 1;

	init_uid();
	init_ip_host();
}

static void Ip_Free(void *a)
{
	struct SIpNode *pNode = (struct SIpNode *) a;

	while (pNode)
	{
		struct SIpNode *pNext = (struct SIpNode *)pNode->pNext;
		FREE(pNode);
		pNode = pNext;
	}
}

static void Uid_Free(void *a)
{
	if (a)
	{
		FREE(a);
	}
}

static void FilterHost_Free(void *a)
{
	SFilterHost *pNode = (SFilterHost *)a;

	while (pNode)
	{
		SFilterHost *pNext = (SFilterHost *)pNode->pNext;
		FREE(pNode);
		pNode = pNext;
	}
}

static inline void print_ipv6(const TYPE_IP_V6 *pV6)
{
	const unsigned char *p = (const char *)pV6;

	printk(KERN_ERR "0x%02X%02X_%02X%02X_%02X%02X_%02X%02X_%02X%02X_%02X%02X_%02X%02X_%02X%02X ",
			p[15], p[14], p[13], p[12], p[11], p[10], p[9], p[8], p[7], p[6], p[5], p[4], p[3], p[2], p[1], p[0]);
}

static void release_uid(void)
{
	btree_free(g_uid_tree, Uid_Free);
	g_uid_tree = NULL;
}

static void release_ip_host(void)
{
	g_FilterPort.wlCnt = 0;
	g_FilterPort.blCnt = 0;

	btree_free(g_ip_tree, Ip_Free);
	g_ip_tree = NULL;
	btree_free(g_host_tree, FilterHost_Free);
	g_host_tree = NULL;
}

static void release_data(void)
{
	g_DefaultFilter = FLAG_FILTER_BLACK;
	g_nEnabled = 0;

	release_uid();
	release_ip_host();
}

inline void SleepJiffies(int nJiffies)
{

	do {
		while (nJiffies > 0)
		{
			nJiffies = schedule_timeout(nJiffies);
		}
	} while(0);
}

inline bool MergePort(SPort *pPortA, const SPort *pPortB)
{
	bool bRet = false;

	if (pPortA->Begin > pPortB->Begin)
	{
		pPortA->Begin = pPortB->Begin;
		bRet = true;
	}
	if (pPortA->End < pPortB->End)
	{
		pPortA->End = pPortB->End;
		bRet = true;
	}

	return bRet;
}

inline void MergePorts(SPort *pPorts, int16_t *pnCnt)
{
	if (*pnCnt <= 1)
	{
		return;
	}

	while (1)
	{
		int i, j;
		bool bChanged = false;
		for (i = 0; i < *pnCnt; ++ i)
		{
			for (j = 0; j < *pnCnt; ++ j)
			{
				if (i == j)
				{
					continue;
				}
				if (((pPorts[i].Begin >= pPorts[j].Begin) && (pPorts[i].Begin <= pPorts[j].End))
					|| ((pPorts[i].End >= pPorts[j].Begin) && (pPorts[i].End <= pPorts[j].End))
					)
				{
					int k;

					MergePort(&pPorts[i], &pPorts[j]);
					for (k = j; k < (*pnCnt - 1); ++ k)
					{
						pPorts[k].Begin = pPorts[k + 1].Begin;
						pPorts[k].End = pPorts[k + 1].End;
					}
					-- (*pnCnt);
					bChanged = true;
				}
			}
		}
		if (!bChanged)
		{
			break;
		}
	}
}

inline bool String2FilterPort(const char* sStr, bool bBL, SFilterPort *pFilterPort)
{
	bool bRet = false;
	const char *p = sStr;
	SPort Port;
	bool bPhase2 = false;

	if (bBL)
	{
		pFilterPort->blCnt = 0;
	}
	else
	{
		pFilterPort->wlCnt = 0;
	}
	Port.Begin = 0;
	Port.End = 0;
	while (true)
	{
		switch (*p)
		{
			case ',':
			case 0:
				if (Port.Begin != 0)
				{
					if (!bPhase2)
					{
						Port.End = Port.Begin;
					}
					if (bBL)
					{
						if (pFilterPort->blCnt >= MAX_ITEM_COUNT)
						{
							return true;
						}
						MEMCPY(&pFilterPort->BL[pFilterPort->blCnt], &Port, sizeof(Port));
						pFilterPort->blCnt ++;
					}
					else
					{
						if (pFilterPort->wlCnt >= MAX_ITEM_COUNT)
						{
							return true;
						}
						MEMCPY(&pFilterPort->WL[pFilterPort->wlCnt], &Port, sizeof(Port));
						pFilterPort->wlCnt ++;
					}
					Port.Begin = 0;
					Port.End = 0;
					bRet = true;
					bPhase2 = false;
				}
				break;
			case '-':
				bPhase2 = true;
				break;
			default:
				if (bPhase2)
				{
					Port.End = (Port.End * 10) + (*p - 0x30);
				}
				else
				{
					Port.Begin = (Port.Begin * 10) + (*p - 0x30);
				}
				break;
		}
		if (*p)
		{
			p ++;
		}
		else
		{
			break;
		}
	}

	if (bRet)
	{
		MergePorts(pFilterPort->BL, &pFilterPort->blCnt);
		MergePorts(pFilterPort->WL, &pFilterPort->wlCnt);
	}

	return bRet;
}

inline FLAG_FILTER FindPort(const SFilterPort *pFilterPort, int nPort)
{
	int i;

	for (i = 0; i < pFilterPort->blCnt; ++ i)
	{
		if ((pFilterPort->BL[i].Begin <= nPort) && (pFilterPort->BL[i].End >= nPort))
		{
			return FLAG_FILTER_BLACK;
		}
	}

	for (i = 0; i < pFilterPort->wlCnt; ++ i)
	{
		if ((pFilterPort->WL[i].Begin <= nPort) && (pFilterPort->WL[i].End >= nPort))
		{
			return FLAG_FILTER_WHITE;
		}
	}

	return FLAG_FILTER_DEFAULT;
}

static int Ip_Cmp(const void *a, const void *b)
{
	const struct SIpNode *pNodeX = (struct SIpNode *) a;
	const struct SIpNode *pNodeY = (struct SIpNode *) b;

	if (pNodeX->IpBegin.nType == IPADDR_TYPE_V4)
	{
		if (pNodeY->IpBegin.nType == IPADDR_TYPE_V4)
		{
			if ((pNodeX->IpBegin.uIpV4 >= pNodeY->IpBegin.uIpV4) && (pNodeX->IpEnd.uIpV4 <= pNodeY->IpEnd.uIpV4))
				return 0;
			else if ((pNodeX->IpBegin.uIpV4 <= pNodeY->IpBegin.uIpV4) && (pNodeX->IpEnd.uIpV4 >= pNodeY->IpEnd.uIpV4))
				return 0;
			else if ((pNodeX->IpBegin.uIpV4 >= pNodeY->IpBegin.uIpV4) && (pNodeX->IpBegin.uIpV4 <= pNodeY->IpEnd.uIpV4))
				return 0;
			else if ((pNodeX->IpEnd.uIpV4 >= pNodeY->IpBegin.uIpV4) && (pNodeX->IpEnd.uIpV4 <= pNodeY->IpEnd.uIpV4))
				return 0;
			else if (pNodeX->IpBegin.uIpV4 < pNodeY->IpBegin.uIpV4)
				return -1;

			else	return 1;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		if (pNodeY->IpBegin.nType == IPADDR_TYPE_V4)
		{
			return 1;
		}
		else
		{
			if (u128_ge(&pNodeX->IpBegin.uIpV6, &pNodeY->IpBegin.uIpV6) && u128_le(&pNodeX->IpEnd.uIpV6, &pNodeY->IpEnd.uIpV6))
				return 0;
			else if (u128_le(&pNodeX->IpBegin.uIpV6, &pNodeY->IpBegin.uIpV6) && u128_ge(&pNodeX->IpEnd.uIpV6, &pNodeY->IpEnd.uIpV6))
				return 0;
			else if (u128_ge(&pNodeX->IpBegin.uIpV6, &pNodeY->IpBegin.uIpV6) && u128_le(&pNodeX->IpBegin.uIpV6, &pNodeY->IpEnd.uIpV6))
				return 0;
			else if (u128_ge(&pNodeX->IpEnd.uIpV6, &pNodeY->IpBegin.uIpV6) && u128_le(&pNodeX->IpEnd.uIpV6, &pNodeY->IpEnd.uIpV6))
				return 0;
			else if (u128_lt(&pNodeX->IpBegin.uIpV6, &pNodeY->IpBegin.uIpV6))
				return -1;

			else	return 1;
		}
	}
}

inline bool Ip_Equals(const void *a, const void *b)
{
	const struct SIpNode *pNodeX = (struct SIpNode *) a;
	const struct SIpNode *pNodeY = (struct SIpNode *) b;

	if (pNodeX->IpBegin.nType == IPADDR_TYPE_V4)
	{
		if (pNodeY->IpBegin.nType == IPADDR_TYPE_V4)
		{
			if ((pNodeX->IpBegin.uIpV4 == pNodeY->IpBegin.uIpV4) && (pNodeX->IpEnd.uIpV4 == pNodeY->IpEnd.uIpV4))
				return true;
		}
	}
	else
	{
		if (pNodeY->IpBegin.nType == IPADDR_TYPE_V6)
		{
			if (u128_eq(&pNodeX->IpBegin.uIpV6, &pNodeY->IpBegin.uIpV6) && u128_eq(&pNodeX->IpEnd.uIpV6, &pNodeY->IpEnd.uIpV6))
				return true;
		}
	}

	return false;
}

static int Uid_Cmp(const void *a, const void *b)
{
	const SUidNode *pNodeX = (SUidNode *) a;
	const SUidNode *pNodeY = (SUidNode *) b;

	if (pNodeX->uUid < pNodeY->uUid)
		return -1;
	else if (pNodeX->uUid == pNodeY->uUid)
		return 0;

	else	return 1;
}

static int FilterHost_Cmp(const void *a, const void *b)
{
	const SFilterHost *pNodeX = (SFilterHost *)a;
	const SFilterHost *pNodeY = (SFilterHost *)b;

       	PRINTK(KERN_ALERT "FilterHost_Cmp, {%s} {%s}\n", pNodeX->Host.sName, pNodeY->Host.sName);
	return Domain_Cmp(pNodeX->Host.sName, pNodeX->Host.nLen, pNodeY->Host.sName, pNodeY->Host.nLen);
}

inline int Host_Cmp(const SHost *pHostA, const SHost *pHostB)
{
	int nLen1 = pHostA->nLen;
	int nLen2 = pHostB->nLen;
	const char *sStr1 = pHostA->sName;
	const char *sStr2 = pHostB->sName;

	-- nLen1;
	-- nLen2;
	while ((nLen1 >= 0) && (nLen2 >= 0))
	{
		if (*(sStr1 + nLen1) == *(sStr2 + nLen2))
		{
			-- nLen1;
			-- nLen2;
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
		else
		{
			return -1;
		}
	}
	else
	{
		return 1;
	}
}

inline bool Answer_Equals(const union UAnswer *pAnsA, const union UAnswer *pAnsB, int32_t nHostType)
{
	if (nHostType == HOST_TYPE_IP)
	{
		if ((pAnsA->IpAddr.nType == IPADDR_TYPE_V4) && (pAnsB->IpAddr.nType == IPADDR_TYPE_V4))
		{
			return (pAnsA->IpAddr.uIpV4 == pAnsB->IpAddr.uIpV4);
		}
		else if ((pAnsA->IpAddr.nType == IPADDR_TYPE_V6) && (pAnsB->IpAddr.nType == IPADDR_TYPE_V6))
		{
			return ((pAnsA->IpAddr.uIpV6.LOWER == pAnsB->IpAddr.uIpV6.LOWER)
				&& (pAnsA->IpAddr.uIpV6.UPPER == pAnsB->IpAddr.uIpV6.UPPER));
		}
	}
	else
	{
#ifdef __HANDLE_CNAME__
		return (Host_Cmp(&pAnsA->RedirHost, &pAnsB->RedirHost) == 0);
#endif
	}

	return false;
}

#ifdef __ALLOW_DIRECT_IP__
static int DnsIp_Cmp(const void *a, const void *b)
{
	const TYPE_IP_V4 *pX = (TYPE_IP_V4 *) a;
	const TYPE_IP_V4 *pY = (TYPE_IP_V4 *) b;

	if ((*pX) < (*pY))
		return -1;
	if ((*pX) > (*pY))
		return 1;

	return 0;
}
#endif

inline void *bt_insert(PROC_CMP pCmp, BTREE **btr, void *pNode)
{
	BTREE_POS pos;

	BTREE *p = btree_add(pCmp, *btr, pNode, &pos);
	if (p)
	{
		if ((*btr)->parent != NULL)
			*btr = (*btr)->parent;

		return pNode;
	}

	return NULL;
}

inline void *bt_find(PROC_CMP pCmp, BTREE *btr, const void *pNode)
{
	BTREE_POS pos;

	BTREE *p = btree_find(pCmp, btr, (void *)pNode, &pos);
	if (p && (pos != INVALID_POS))
	{
		return p->key[pos];
	}

	return NULL;
}

inline struct SIpNode *AddNewIpNodeToTree(const SIpAddr *pIpBegin, const SIpAddr *pIpEnd, int nMemFlag)
{
	struct SIpNode *pNode = MALLOC(sizeof(struct SIpNode), nMemFlag);

	if (pNode)
	{
		MEMCPY(&pNode->IpBegin, pIpBegin, sizeof(*pIpBegin));
		MEMCPY(&pNode->IpEnd, pIpEnd, sizeof(*pIpEnd));
		if (!bt_insert(Ip_Cmp, &g_ip_tree, pNode))
		{
			FREE(pNode);
		}
	}

	return pNode;
}

inline bool MergeIp(struct SIpNode *pNodeA, const struct SIpNode *pNodeB)
{
	bool bRet = false;

	if (pNodeA->IpBegin.nType == IPADDR_TYPE_V4)
	{
        	PRINTK(KERN_ALERT "MergeIp, {%08X, %08X} {%08X, %08X}\n", pNodeA->IpBegin.uIpV4, pNodeA->IpEnd.uIpV4, pNodeB->IpBegin.uIpV4, pNodeB->IpEnd.uIpV4);
		if (pNodeA->IpBegin.uIpV4 > pNodeB->IpBegin.uIpV4)
		{
			pNodeA->IpBegin.uIpV4 = pNodeB->IpBegin.uIpV4;
			bRet = true;
		}
		if (pNodeA->IpEnd.uIpV4 < pNodeB->IpEnd.uIpV4)
		{
			pNodeA->IpEnd.uIpV4 = pNodeB->IpEnd.uIpV4;
			bRet = true;
		}
	}
	else
	{
        	PRINTK(KERN_ALERT "MergeIp ");
#ifdef __DEBUG__
		print_ipv6(&pNodeA->IpBegin.uIpV6);
		print_ipv6(&pNodeA->IpEnd.uIpV6);
		print_ipv6(&pNodeB->IpBegin.uIpV6);
		print_ipv6(&pNodeB->IpEnd.uIpV6);
#endif
		if (u128_gt(&pNodeA->IpBegin.uIpV6, &pNodeB->IpBegin.uIpV6))
		{
			MEMCPY(&pNodeA->IpBegin.uIpV6, &pNodeB->IpBegin.uIpV6, sizeof(TYPE_IP_V6));
			bRet = true;
		}
		if (u128_lt(&pNodeA->IpEnd.uIpV6, &pNodeB->IpEnd.uIpV6))
		{
			MEMCPY(&pNodeA->IpEnd.uIpV6, &pNodeB->IpEnd.uIpV6, sizeof(TYPE_IP_V6));
			bRet = true;
		}
	}

	return bRet;
}

inline bool MergePorts2(SPort *pPortsA, int16_t *pnCntA, const SPort *pPortsB, int16_t nCntB)
{
	bool bRet = false;
	int i, j;
	bool bMerged = false;

	if (nCntB <= 0)
	{
		return false;
	}

	for (i = 0; i < nCntB; ++ i)
	{
		bool bFound = false;
		for (j = 0; j < *pnCntA; ++ j)
		{
			if (((pPortsB[i].Begin >= pPortsA[j].Begin) && (pPortsB[i].Begin <= pPortsA[j].End))
				|| ((pPortsB[i].End >= pPortsA[j].Begin) && (pPortsB[i].End <= pPortsA[j].End))
				)
			{
				if (MergePort(&pPortsA[j], &pPortsB[i]))
				{
					bRet = true;
					bMerged = true;
					bFound = true;
					break;
				}
			}
		}
		if (!bFound)
		{
			if (*pnCntA < MAX_ITEM_COUNT)
			{
				pPortsA[*pnCntA].Begin = pPortsB[i].Begin;
				pPortsA[*pnCntA].End = pPortsB[i].End;
				++ (*pnCntA);
				bRet = true;
			}
		}
	}

	if (bMerged)
	{
		MergePorts(pPortsA, pnCntA);
	}

	return bRet;
}

inline bool Ports_Equal(const SPort *pPortsA, int nCntA, const SPort *pPortsB, int nCntB)
{
	int i, j;

	if (nCntA != nCntB)
	{
		return false;
	}

	for (i = 0; i < nCntA; ++ i)
	{
		bool bFound = false;
		for (j = 0; j < nCntB; ++ j)
		{
			if ((pPortsA[i].Begin == pPortsB[j].Begin) && (pPortsA[i].End == pPortsB[j].End))
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			return false;
		}
	}

	return true;
}

inline void MergeIpList(struct SIpNode *pNode)
{
	struct SIpNode *pLast, *pTmp1, *pTmp2;
	int nCnt = 0;

	if (pNode->pNext)
	{
		pLast = pNode;
		pTmp1 = pNode->pNext;
		while (pTmp1)
		{
			bool bRemoved = false;

			pTmp2 = pNode->pNext;
			while (pTmp2)
			{
				if (pTmp1 != pTmp2)
				{
					if (Ip_Cmp(pTmp1, pTmp2) == 0)
					{
						if (Ports_Equal(pTmp1->FilterPort.BL, pTmp1->FilterPort.blCnt, pTmp2->FilterPort.BL, pTmp2->FilterPort.blCnt)
							&& Ports_Equal(pTmp1->FilterPort.WL, pTmp1->FilterPort.wlCnt, pTmp2->FilterPort.WL, pTmp2->FilterPort.wlCnt)
							&& (pTmp1->FlagFilter == pTmp2->FlagFilter))
						{
							MergeIp(pTmp2, pTmp1);
							bRemoved = true;
							break;
						}
						else if (Ip_Equals(pTmp1, pTmp2) && (pTmp1->FlagFilter == pTmp2->FlagFilter))
						{
							MergePorts2(pTmp2->FilterPort.BL, &pTmp2->FilterPort.blCnt, pTmp1->FilterPort.BL, pTmp1->FilterPort.blCnt);
							MergePorts2(pTmp2->FilterPort.WL, &pTmp2->FilterPort.wlCnt, pTmp1->FilterPort.WL, pTmp1->FilterPort.wlCnt);
							bRemoved = true;
							break;
						}
					}
				}
				pTmp2 = pTmp2->pNext;
			}

			if (bRemoved)
			{
				pLast->pNext = pTmp1->pNext;
				FREE(pTmp1);
				pTmp1 = pLast->pNext;
			}
			else
			{
				pLast = pTmp1;
				pTmp1 = pTmp1->pNext;
				++ nCnt;
			}
		}
		if (nCnt == 1)
		{
			pTmp1 = pNode->pNext;
			if (pTmp1)
			{
				MEMCPY(&pNode->IpBegin, &pTmp1->IpBegin, sizeof(pTmp1->IpBegin));
				MEMCPY(&pNode->IpEnd, &pTmp1->IpEnd, sizeof(pTmp1->IpEnd));
				pNode->FlagFilter = pTmp1->FlagFilter;
				if (pTmp1->FilterPort.blCnt > 0)
				{
					pNode->FilterPort.blCnt = pTmp1->FilterPort.blCnt;
					MEMCPY(&pNode->FilterPort.BL[0], &pTmp1->FilterPort.BL[0], sizeof(SPort) * pTmp1->FilterPort.blCnt);
				}
				if (pTmp1->FilterPort.wlCnt > 0)
				{
					pNode->FilterPort.wlCnt = pTmp1->FilterPort.wlCnt;
					MEMCPY(&pNode->FilterPort.WL[0], &pTmp1->FilterPort.WL[0], sizeof(SPort) * pTmp1->FilterPort.wlCnt);
				}
				FREE(pTmp1);
				pNode->pNext = NULL;
			}
		}
	}
}

inline struct SIpNode *AddIpFilter(const SIpAddr *pIpBegin, const SIpAddr *pIpEnd, FLAG_FILTER FlagFilter, const SFilterPort *pFP, int nMemFlag)
{
	BTREE_POS pos;
	BTREE *pTree;
	struct SIpNode *pNode = NULL, *pTmp = NULL, *pNext = NULL;
	struct SIpNode *pTmpNode = MALLOC(sizeof(struct SIpNode), nMemFlag);

	if (!pTmpNode)
	{
		printk(KERN_ERR "Malloc for SIpNode failed\n");
		return NULL;
	}

	MEMCPY(&pTmpNode->IpBegin, pIpBegin, sizeof(*pIpBegin));
	MEMCPY(&pTmpNode->IpEnd, pIpEnd, sizeof(*pIpEnd));

	pTree = btree_find(Ip_Cmp, g_ip_tree, pTmpNode, &pos);
	if (pTree && (pos != INVALID_POS))
	{
		pNode = pTree->key[pos];
		if (pNode->pNext)
		{
			bool bToBeAdd = false;
			PRINTK(KERN_ERR "Dbrach 1\n");

			if (MergeIp(pNode, pTmpNode))
			{
				btree_delete(pTree, pos, Ip_Cmp);
				bToBeAdd = true;
				do
				{
					pTree = btree_find(Ip_Cmp, g_ip_tree, pNode, &pos);
					if (pTree && (pos != INVALID_POS))
					{
						pTmp = pTree->key[pos];
						MergeIp(pNode, pTmp);
						if (pTmp->pNext)
						{
							pNext = pTmp->pNext;
							FREE(pTmp);
							pTmp = pNext;
							while (pTmp)
							{
								pNext = pTmp->pNext;
								pTmp->pNext = pNode->pNext;
								pNode->pNext = pTmp;
								pTmp = pNext;
							}
						}
						else
						{
							pTmp->pNext = pNode->pNext;
							pNode->pNext = pTmp;
						}
						btree_delete(pTree, pos, Ip_Cmp);
					}
					else
					{
						break;
					}
				}
				while (1);
			}

			{
				bool bMerged = false;
				pTmp = pNode->pNext;
				while (pTmp)
				{
					if ((Ip_Cmp(pTmp, pTmpNode) == 0)
						&& Ports_Equal(pTmp->FilterPort.BL, pTmp->FilterPort.blCnt, pFP->BL, pFP->blCnt)
						&& Ports_Equal(pTmp->FilterPort.WL, pTmp->FilterPort.wlCnt, pFP->WL, pFP->wlCnt)
						&& (pTmp->FlagFilter == FlagFilter))
					{
						MergeIp(pTmp, pTmpNode);
						bMerged = true;
						break;
					}
					else if (Ip_Equals(pTmp, pTmpNode) && (pTmp->FlagFilter == FlagFilter))
					{
						MergePorts2(pTmp->FilterPort.BL, &pTmp->FilterPort.blCnt, pFP->BL, pFP->blCnt);
						MergePorts2(pTmp->FilterPort.WL, &pTmp->FilterPort.wlCnt, pFP->WL, pFP->wlCnt);
						bMerged = true;
						break;
					}
					pTmp = pTmp->pNext;
				}
				if (!bMerged)
				{
					pTmp = MALLOC(sizeof(struct SIpNode), nMemFlag);
					if (pTmp)
					{
						MEMCPY(&pTmp->IpBegin, pIpBegin, sizeof(*pIpBegin));
						MEMCPY(&pTmp->IpEnd, pIpEnd, sizeof(*pIpEnd));
						pTmp->FlagFilter = FlagFilter;
						if (pFP)
						{
							if (pFP->blCnt > 0)
							{
								pTmp->FilterPort.blCnt = pFP->blCnt;
								MEMCPY(&pTmp->FilterPort.BL[0], &pFP->BL[0], sizeof(SPort) * pFP->blCnt);
							}
							if (pFP->wlCnt > 0)
							{
								pTmp->FilterPort.wlCnt = pFP->wlCnt;
								MEMCPY(&pTmp->FilterPort.WL[0], &pFP->WL[0], sizeof(SPort) * pFP->wlCnt);
							}
						}
						pTmp->pNext = pNode->pNext;
						pNode->pNext = pTmp;
					}
				}
			}

			MergeIpList(pNode);

			if (bToBeAdd)
			{
				if (!bt_insert(Ip_Cmp, &g_ip_tree, pNode))
				{
					Ip_Free(pNode);
				}
			}
		}
		else
		{
			if (Ports_Equal(pNode->FilterPort.BL, pNode->FilterPort.blCnt, pFP->BL, pFP->blCnt)
				&& Ports_Equal(pNode->FilterPort.WL, pNode->FilterPort.wlCnt, pFP->WL, pFP->wlCnt)
				&& (pNode->FlagFilter == FlagFilter))
			{
				PRINTK(KERN_ERR "Abrach 1\n");
				if (MergeIp(pNode, pTmpNode))
				{
					bool bHead = false;

					PRINTK(KERN_ERR "Abrach 11\n");
					btree_delete(pTree, pos, Ip_Cmp);
					do
					{
						pTree = btree_find(Ip_Cmp, g_ip_tree, pNode, &pos);
						if (pTree && (pos != INVALID_POS))
						{
							pTmp = pTree->key[pos];
							if (pTmp->pNext)
							{
								if (!bHead)
								{
									MergeIp(pTmp, pNode);
									pNode->pNext = pTmp->pNext;
									pTmp->pNext = pNode;
									pNode = pTmp;
									bHead = true;
								}
								else
								{
									MergeIp(pNode, pTmp);
									pNext = pTmp->pNext;
									FREE(pTmp);
									pTmp = pNext;
									while (pTmp)
									{
										pNext = pTmp->pNext;
										pTmp->pNext = pNode->pNext;
										pNode->pNext = pTmp;
										pTmp = pNext;
									}
								}
							}
							else
							{
								if (!bHead)
								{
									pNode->pNext = pTmp;
									pTmp = MALLOC(sizeof(struct SIpNode), nMemFlag);
									if (pTmp)
									{
										MEMCPY(&pTmp->IpBegin, &pNode->IpBegin, sizeof(pNode->IpBegin));
										MEMCPY(&pTmp->IpEnd, &pNode->IpEnd, sizeof(pNode->IpEnd));
										pTmp->pNext = pNode;
										MergeIp(pTmp, pNode->pNext);
										pNode = pTmp;
										bHead = true;
									}
								}
								else
								{
									MergeIp(pNode, pTmp);
									pTmp->pNext = pNode->pNext;
									pNode->pNext = pTmp;
								}
							}
							btree_delete(pTree, pos, Ip_Cmp);
						}
						else
						{
							break;
						}
					}
					while (1);

					MergeIpList(pNode);

					if (!bt_insert(Ip_Cmp, &g_ip_tree, pNode))
					{
						Ip_Free(pNode);
					}
				}
			}
			else if (Ip_Equals(pNode, pTmpNode) && (pNode->FlagFilter == FlagFilter))
			{
				PRINTK(KERN_ERR "Abrach 2\n");
				MergePorts2(pNode->FilterPort.BL, &pNode->FilterPort.blCnt, pFP->BL, pFP->blCnt);
				MergePorts2(pNode->FilterPort.WL, &pNode->FilterPort.wlCnt, pFP->WL, pFP->wlCnt);
			}
			else
			{
				PRINTK(KERN_ERR "Abrach 3\n");
				btree_delete(pTree, pos, Ip_Cmp);
				pTmp = MALLOC(sizeof(struct SIpNode), nMemFlag);
				if (pTmp)
				{
					MEMCPY(&pTmp->IpBegin, pIpBegin, sizeof(*pIpBegin));
					MEMCPY(&pTmp->IpEnd, pIpEnd, sizeof(*pIpEnd));
					pTmp->pNext = pNode;
					MergeIp(pTmp, pNode);
					pNode = pTmp;
					pTmp = MALLOC(sizeof(struct SIpNode), nMemFlag);
					if (pTmp)
					{
						MEMCPY(&pTmp->IpBegin, pIpBegin, sizeof(*pIpBegin));
						MEMCPY(&pTmp->IpEnd, pIpEnd, sizeof(*pIpEnd));
						pTmp->FlagFilter = FlagFilter;
						if (pFP)
						{
							if (pFP->blCnt > 0)
							{
								pTmp->FilterPort.blCnt = pFP->blCnt;
								MEMCPY(&pTmp->FilterPort.BL[0], &pFP->BL[0], sizeof(SPort) * pFP->blCnt);
							}
							if (pFP->wlCnt > 0)
							{
								pTmp->FilterPort.wlCnt = pFP->wlCnt;
								MEMCPY(&pTmp->FilterPort.WL[0], &pFP->WL[0], sizeof(SPort) * pFP->wlCnt);
							}
						}
						pTmp->pNext = pNode->pNext;
						pNode->pNext = pTmp;

						do
						{
							pTree = btree_find(Ip_Cmp, g_ip_tree, pNode, &pos);
							if (pTree && (pos != INVALID_POS))
							{
								pTmp = pTree->key[pos];
								MergeIp(pNode, pTmp);
								if (pTmp->pNext)
								{
									pNext = pTmp->pNext;
									FREE(pTmp);
									pTmp = pNext;
									while (pTmp)
									{
										pNext = pTmp->pNext;
										pTmp->pNext = pNode->pNext;
										pNode->pNext = pTmp;
										pTmp = pNext;
									}
								}
								else
								{
									pTmp->pNext = pNode->pNext;
									pNode->pNext = pTmp;
								}
								btree_delete(pTree, pos, Ip_Cmp);
							}
							else
							{
								break;
							}
						}
						while (1);

						MergeIpList(pNode);

						if (!bt_insert(Ip_Cmp, &g_ip_tree, pNode))
						{
							Ip_Free(pNode);
						}
					}
				}
			}
		}
	}
	else
	{
		pNode = AddNewIpNodeToTree(&pTmpNode->IpBegin, &pTmpNode->IpEnd, nMemFlag);
		if (pNode)
		{
			pNode->FlagFilter = FlagFilter;
			if (pFP)
			{
				if (pFP->blCnt > 0)
				{
					pNode->FilterPort.blCnt = pFP->blCnt;
					MEMCPY(&pNode->FilterPort.BL[0], &pFP->BL[0], sizeof(SPort) * pFP->blCnt);
				}
				if (pFP->wlCnt > 0)
				{
					pNode->FilterPort.wlCnt = pFP->wlCnt;
					MEMCPY(&pNode->FilterPort.WL[0], &pFP->WL[0], sizeof(SPort) * pFP->wlCnt);
				}
			}
		}
		else
		{
			printk(KERN_ERR "AddNewIpNodeToTree failed, ip is 0x%x\n", (int)pTmpNode->IpBegin.uIpV4);
		}
	}
	FREE(pTmpNode);
	PRINTK(KERN_ALERT "AddIpFilter, 0x%X, 0x%x, %s\n", pIpBegin->uIpV4, pIpEnd->uIpV4, FlagFilter == FLAG_FILTER_WHITE ? "white":
									(FlagFilter == FLAG_FILTER_BLACK ? "Black" : "Default"));

	return pNode;
}

inline bool IsBiggerHost(const SHost *pHostA, const SHost *pHostB)
{
       	PRINTK(KERN_ALERT "IsBiggerHost, {%s} {%s}\n", pHostA->sName, pHostB->sName);
	if (pHostA->sName[0] == '*')
	{
		if (pHostB->sName[0] == '*')
		{
			if (pHostA->nLen < pHostB->nLen)
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

inline void MergeHostList(SFilterHost *pNode)
{
	SFilterHost *pLast1, *pLast2, *pTmp1, *pTmp2;

	if (pNode->pNext)
	{
		pLast1 = NULL;
		pTmp1 = pNode;
		while (pTmp1)
		{
			bool bRemoved1 = false;

			pLast2 = NULL;
			pTmp2 = pNode;
			while (pTmp2)
			{
				bool bRemoved2 = false;
				if (pTmp1 != pTmp2)
				{
					if (FilterHost_Cmp(pTmp1, pTmp2) == 0)
					{
						if (Ports_Equal(pTmp1->FilterPort.BL, pTmp1->FilterPort.blCnt, pTmp2->FilterPort.BL, pTmp2->FilterPort.blCnt)
							&& Ports_Equal(pTmp1->FilterPort.WL, pTmp1->FilterPort.wlCnt, pTmp2->FilterPort.WL, pTmp2->FilterPort.wlCnt)
							&& (pTmp1->FlagFilter == pTmp2->FlagFilter))
						{
							if (pTmp2 == pNode)
							{
								if (IsBiggerHost(&pTmp1->Host, &pTmp2->Host))
								{
									pTmp2->Host.nLen = pTmp1->Host.nLen;
									MEMCPY(pTmp2->Host.sName, pTmp1->Host.sName, pTmp1->Host.nLen);
								}
								bRemoved1 = true;
								break;
							}
							else
							{
								if (IsBiggerHost(&pTmp2->Host, &pTmp1->Host))
								{
									pTmp1->Host.nLen = pTmp2->Host.nLen;
									MEMCPY(pTmp1->Host.sName, pTmp2->Host.sName, pTmp2->Host.nLen);
								}
								bRemoved2 = true;
							}
						}
						else if ((Host_Cmp(&pTmp1->Host, &pTmp2->Host) == 0) && (pTmp1->FlagFilter == pTmp2->FlagFilter))
						{
							if (pTmp2 == pNode)
							{
								MergePorts2(pTmp2->FilterPort.BL, &pTmp2->FilterPort.blCnt, pTmp1->FilterPort.BL, pTmp1->FilterPort.blCnt);
								MergePorts2(pTmp2->FilterPort.WL, &pTmp2->FilterPort.wlCnt, pTmp1->FilterPort.WL, pTmp1->FilterPort.wlCnt);
								bRemoved1 = true;
								break;
							}
							else
							{
								MergePorts2(pTmp1->FilterPort.BL, &pTmp1->FilterPort.blCnt, pTmp2->FilterPort.BL, pTmp2->FilterPort.blCnt);
								MergePorts2(pTmp1->FilterPort.WL, &pTmp1->FilterPort.wlCnt, pTmp2->FilterPort.WL, pTmp2->FilterPort.wlCnt);
								bRemoved2 = true;
							}
						}
					}
				}
				if (bRemoved2)
				{
					pLast2->pNext = pTmp2->pNext;
					FREE(pTmp2);
					pTmp2 = (SFilterHost *)pLast2->pNext;
				}
				else
				{
					pLast2 = pTmp2;
					pTmp2 = (SFilterHost *)pTmp2->pNext;
				}
			}

			if (bRemoved1)
			{
				pLast1->pNext = pTmp1->pNext;
				FREE(pTmp1);
				pTmp1 = (SFilterHost *)pLast1->pNext;
			}
			else
			{
				pLast1 = pTmp1;
				pTmp1 = (SFilterHost *)pTmp1->pNext;
			}
		}
	}
}

inline SFilterHost *AddHostFilter(const SHost *pHost, FLAG_FILTER FlagFilter, const SFilterPort *pFP, int nMemFlag)
{
	BTREE_POS pos;
	BTREE *pTree;
	SFilterHost *pNode, *pTmp, *pNext;
	SFilterHost *pTmpNode = (SFilterHost *)pHost;


	pTree = btree_find(FilterHost_Cmp, g_host_tree, pTmpNode, &pos);
	if (pTree && (pos != INVALID_POS))
	{
		pNode = pTree->key[pos];
		if (pNode->pNext)
		{
			bool bToBeAdd = false;
			PRINTK(KERN_ERR "Cbrach 1\n");

			if (IsBiggerHost(pHost, &pNode->Host))
			{
				btree_delete(pTree, pos, FilterHost_Cmp);
				bToBeAdd = true;
				do
				{
					pTree = btree_find(FilterHost_Cmp, g_host_tree, pTmpNode, &pos);
					if (pTree && (pos != INVALID_POS))
					{
						pTmp = pTree->key[pos];
						while (pTmp)
						{
							pNext = (SFilterHost *)pTmp->pNext;
							pTmp->pNext = pNode->pNext;
							pNode->pNext = (struct SFilterHost *)pTmp;
							pTmp = pNext;
						}
						btree_delete(pTree, pos, FilterHost_Cmp);
					}
					else
					{
						break;
					}
				}
				while (1);
			}

			{
				bool bMerged = false;
				pTmp = pNode;
				while (pTmp)
				{
					if ((FilterHost_Cmp(pTmp, pTmpNode) == 0)
						&& Ports_Equal(pTmp->FilterPort.BL, pTmp->FilterPort.blCnt, pFP->BL, pFP->blCnt)
						&& Ports_Equal(pTmp->FilterPort.WL, pTmp->FilterPort.wlCnt, pFP->WL, pFP->wlCnt)
						&& (pTmp->FlagFilter == FlagFilter))
					{
						if (IsBiggerHost(pHost, &pTmp->Host))
						{
							pTmp->Host.nLen = pHost->nLen;
							MEMCPY(pTmp->Host.sName, pHost->sName, pHost->nLen);
						}
						bMerged = true;
						break;
					}
					else if ((Host_Cmp(&pTmp->Host, pHost) == 0) && (pTmp->FlagFilter == FlagFilter))
					{
						MergePorts2(pTmp->FilterPort.BL, &pTmp->FilterPort.blCnt, pFP->BL, pFP->blCnt);
						MergePorts2(pTmp->FilterPort.WL, &pTmp->FilterPort.wlCnt, pFP->WL, pFP->wlCnt);
						bMerged = true;
						break;
					}
					pTmp = (SFilterHost *)pTmp->pNext;
				}
				if (!bMerged)
				{
					pTmp = MALLOC(sizeof(SFilterHost), nMemFlag);
					if (pTmp)
					{
						pTmp->Host.nLen = pHost->nLen;
						MEMCPY(pTmp->Host.sName, pHost->sName, pHost->nLen);
						pTmp->FlagFilter = FlagFilter;
						if (pFP)
						{
							if (pFP->blCnt > 0)
							{
								pTmp->FilterPort.blCnt = pFP->blCnt;
								MEMCPY(&pTmp->FilterPort.BL[0], &pFP->BL[0], sizeof(SPort) * pFP->blCnt);
							}
							if (pFP->wlCnt > 0)
							{
								pTmp->FilterPort.wlCnt = pFP->wlCnt;
								MEMCPY(&pTmp->FilterPort.WL[0], &pFP->WL[0], sizeof(SPort) * pFP->wlCnt);
							}
						}
						if (bToBeAdd)
						{
							pTmp->pNext = (struct SFilterHost *)pNode;
							pNode = pTmp;
						}
						else
						{
							pTmp->pNext = pNode->pNext;
							pNode->pNext = (struct SFilterHost *)pTmp;
						}
					}
				}
			}

			MergeHostList(pNode);

			if (bToBeAdd)
			{
				if (!bt_insert(FilterHost_Cmp, &g_host_tree, pNode))
				{
					FilterHost_Free(pNode);
				}
			}
		}
		else
		{
			if (Ports_Equal(pNode->FilterPort.BL, pNode->FilterPort.blCnt, pFP->BL, pFP->blCnt)
				&& Ports_Equal(pNode->FilterPort.WL, pNode->FilterPort.wlCnt, pFP->WL, pFP->wlCnt)
				&& (pNode->FlagFilter == FlagFilter))
			{
				PRINTK(KERN_ERR "Bbrach 1\n");
				if (IsBiggerHost(pHost, &pNode->Host))
				{
					PRINTK(KERN_ERR "Bbrach 11\n");
					btree_delete(pTree, pos, FilterHost_Cmp);
					pNode->Host.nLen = pHost->nLen;
					MEMCPY(pNode->Host.sName, pHost->sName, pHost->nLen);
					do
					{
						pTree = btree_find(FilterHost_Cmp, g_host_tree, pNode, &pos);
						if (pTree && (pos != INVALID_POS))
						{
							pTmp = pTree->key[pos];
							while (pTmp)
							{
								pNext = (SFilterHost *)pTmp->pNext;
								pTmp->pNext = pNode->pNext;
								pNode->pNext = (struct SFilterHost *)pTmp;
								pTmp = pNext;
							}
							btree_delete(pTree, pos, FilterHost_Cmp);
						}
						else
						{
							break;
						}
					}
					while (1);

					MergeHostList(pNode);

					if (!bt_insert(FilterHost_Cmp, &g_host_tree, pNode))
					{
						FilterHost_Free(pNode);
					}
				}
			}
			else if ((Host_Cmp(pHost, &pNode->Host) == 0) && (pNode->FlagFilter == FlagFilter))
			{
				PRINTK(KERN_ERR "Bbrach 2\n");
				MergePorts2(pNode->FilterPort.BL, &pNode->FilterPort.blCnt, pFP->BL, pFP->blCnt);
				MergePorts2(pNode->FilterPort.WL, &pNode->FilterPort.wlCnt, pFP->WL, pFP->wlCnt);
			}
			else
			{
				PRINTK(KERN_ERR "Bbrach 3\n");
				btree_delete(pTree, pos, FilterHost_Cmp);
				pTmp = MALLOC(sizeof(SFilterHost), nMemFlag);
				if (pTmp)
				{
					pTmp->Host.nLen = pHost->nLen;
					MEMCPY(pTmp->Host.sName, pHost->sName, pHost->nLen);
					pTmp->FlagFilter = FlagFilter;
					if (pFP)
					{
						if (pFP->blCnt > 0)
						{
							pTmp->FilterPort.blCnt = pFP->blCnt;
							MEMCPY(&pTmp->FilterPort.BL[0], &pFP->BL[0], sizeof(SPort) * pFP->blCnt);
						}
						if (pFP->wlCnt > 0)
						{
							pTmp->FilterPort.wlCnt = pFP->wlCnt;
							MEMCPY(&pTmp->FilterPort.WL[0], &pFP->WL[0], sizeof(SPort) * pFP->wlCnt);
						}
					}
					if (IsBiggerHost(pHost, &pNode->Host))
					{
						pTmp->pNext = (struct SFilterHost *)pNode;
						pNode = pTmp;
					}
					else
					{
						pTmp->pNext = pNode->pNext;
						pNode->pNext = (struct SFilterHost *)pTmp;
					}

					do
					{
						pTree = btree_find(FilterHost_Cmp, g_host_tree, pNode, &pos);
						if (pTree && (pos != INVALID_POS))
						{
							pTmp = pTree->key[pos];
							while (pTmp)
							{
								pNext = (SFilterHost *)pTmp->pNext;
								pTmp->pNext = pNode->pNext;
								pNode->pNext = (struct SFilterHost *)pTmp;
								pTmp = pNext;
							}
							btree_delete(pTree, pos, FilterHost_Cmp);
						}
						else
						{
							break;
						}
					}
					while (1);

					MergeHostList(pNode);

					if (!bt_insert(FilterHost_Cmp, &g_host_tree, pNode))
					{
						FilterHost_Free(pNode);
					}
				}
			}
		}
	}
	else
	{
		pNode = MALLOC(sizeof(SFilterHost), nMemFlag);
		if (pNode)
		{
			pNode->Host.nLen = pHost->nLen;
			MEMCPY(pNode->Host.sName, pHost->sName, pHost->nLen);
#ifdef __DEBUG__
			if (pHost->nLen < MAX_NAME_LENGTH)
			{
				pNode->Host.sName[pHost->nLen] = 0;
			}
#endif
			pNode->FlagFilter = FlagFilter;
			if (pFP)
			{
				if (pFP->blCnt > 0)
				{
					pNode->FilterPort.blCnt = pFP->blCnt;
					MEMCPY(&pNode->FilterPort.BL[0], &pFP->BL[0], sizeof(SPort) * pFP->blCnt);
				}
				if (pFP->wlCnt > 0)
				{
					pNode->FilterPort.wlCnt = pFP->wlCnt;
					MEMCPY(&pNode->FilterPort.WL[0], &pFP->WL[0], sizeof(SPort) * pFP->wlCnt);
				}
			}
			if (!bt_insert(FilterHost_Cmp, &g_host_tree, pNode))
			{
				FilterHost_Free(pNode);
			}
		}
		else
		{
			printk(KERN_ERR "MALLOC FilterHost failed, host is %s\n", pHost->sName);
		}
	}
	PRINTK(KERN_ALERT "AddHostFilter, %s, %s\n", pHost->sName, FlagFilter == FLAG_FILTER_WHITE ? "white":
									(FlagFilter == FLAG_FILTER_BLACK ? "Black" : "Default"));

	return pNode;
}

void FreeDCacheList(void)
{
	struct SDnsCacheItem *pItem = g_dcache_list;

	while (pItem)
	{
		struct SDnsCacheItem *pNext = pItem->pNext;
		FREE(pItem);
		pItem = pNext;
	}
	g_dcache_list = NULL;
}

inline struct SDnsCacheItem *Find_DnsCacheItem(const SHost *pHost)
{
	struct SDnsCacheItem *pTmp = g_dcache_list;

	while (pTmp)
	{
		if (Host_Cmp(&pTmp->Host, pHost) == 0)
		{
			return pTmp;
		}
		pTmp = pTmp->pNext;
	}

	return NULL;
}

inline void AddDnsCache(const SDnsResp *pDnsResp)
{
	int i, j;
	struct SDnsCacheItem *pItemTmp = NULL;
	SFilterHost *pNode = NULL, *pTmp = NULL;
	SFilterHost *pTmpNode = (SFilterHost *)pDnsResp;
#ifdef __DEBUG__
	static int nCnt = 0;
#endif

	LOCK_DCACHE_LIST;
	pItemTmp = Find_DnsCacheItem(&pDnsResp->Host);
	if (!pItemTmp)
	{
		pItemTmp = MALLOC(sizeof(struct SDnsCacheItem), ALLOCMEM_MIDD_FLAG);
		if (!pItemTmp)
		{
			printk(KERN_ERR "AddDnsCache: MALLOC SDnsCacheItem failed!");

			UNLOCK_DCACHE_LIST;
			return;
		}
		pItemTmp->Host.nLen = pDnsResp->Host.nLen;
		MEMCPY(pItemTmp->Host.sName, pDnsResp->Host.sName, pDnsResp->Host.nLen);
		pItemTmp->nCount = 0;
		if (!g_dcache_list)
		{
			pItemTmp->pNext = NULL;
			g_dcache_list = pItemTmp;
		}
		else
		{
			pItemTmp->pNext = g_dcache_list;
			g_dcache_list = pItemTmp;
		}
#ifdef __DEBUG__
		++ nCnt;
		PRINTK(KERN_ERR "AddDnsCache: %d items cached\n", nCnt);
#endif
	}

	LOCK_HOST_TREE;

	pNode = bt_find(FilterHost_Cmp, g_host_tree, pTmpNode);

	for (i = 0; i < pDnsResp->nCount; ++ i)
	{
		bool bToAdd = true;
		for (j = 0; j < pItemTmp->nCount; ++ j)
		{
			if (pDnsResp->nHostType[i] == pItemTmp->nHostType[j])
			{
				if (Answer_Equals(&pDnsResp->Answers[i], &pItemTmp->Answers[j], pDnsResp->nHostType[i]))
				{
					bToAdd = false;
					break;
				}
			}
		}
		if (bToAdd)
		{
			if (pItemTmp->nCount < MAX_CACHE_HOSTID_CNT)
			{
				pItemTmp->nHostType[pItemTmp->nCount] = pDnsResp->nHostType[i];
				if (pDnsResp->nHostType[i] == HOST_TYPE_IP)
				{
					pItemTmp->Answers[pItemTmp->nCount].IpAddr.nType = pDnsResp->Answers[i].IpAddr.nType;
					if (pDnsResp->Answers[i].IpAddr.nType == IPADDR_TYPE_V4)
					{
						pItemTmp->Answers[pItemTmp->nCount].IpAddr.uIpV4 = pDnsResp->Answers[i].IpAddr.uIpV4;
					}
					else
					{
						MEMCPY(&pItemTmp->Answers[pItemTmp->nCount].IpAddr.uIpV6, &pDnsResp->Answers[i].IpAddr.uIpV6, sizeof(TYPE_IP_V6));
					}
					++ pItemTmp->nCount;
				}
#ifdef __HANDLE_CNAME__
				else
				{
					pItemTmp->Answers[pItemTmp->nCount].RedirHost.nLen = pDnsResp->Answers[i].RedirHost.nLen;
					MEMCPY(pItemTmp->Answers[pItemTmp->nCount].RedirHost.sName, pDnsResp->Answers[i].RedirHost.sName, pDnsResp->Answers[i].RedirHost.nLen);
					++ pItemTmp->nCount;
				}
#endif
			}
			else
			{
				printk(KERN_ERR "!!! DNS Resp items are full for host %s\n", pDnsResp->Host.sName);
			}
		}

		pTmp = pNode;
		while (pTmp)
		{
			if (Domain_Cmp(pDnsResp->Host.sName, pDnsResp->Host.nLen, pTmp->Host.sName, pTmp->Host.nLen) == 0)
			{
				if ((pTmp->FlagFilter == FLAG_FILTER_WHITE)
					|| (pTmp->FlagFilter == FLAG_FILTER_BLACK)
					|| (pTmp->FilterPort.wlCnt > 0)
					|| (pTmp->FilterPort.blCnt > 0))
				{
					if (pItemTmp->nHostType[i] == HOST_TYPE_IP)
					{
						LOCK_IP_TREE;
						AddIpFilter(&pDnsResp->Answers[i].IpAddr, &pDnsResp->Answers[i].IpAddr,
								pTmp->FlagFilter, &pTmp->FilterPort, ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);
						UNLOCK_IP_TREE;
					}
#ifdef __HANDLE_CNAME__
					else
					{
						AddHostFilter(&pDnsResp->Answers[i].RedirHost, pTmp->FlagFilter, &pTmp->FilterPort, ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);
					}
#endif
				}
			}
			pTmp = (SFilterHost *)pTmp->pNext;
		}
	}
	UNLOCK_HOST_TREE;
	UNLOCK_DCACHE_LIST;
}

inline int SendNLMsg(int nMsgType, char *pMsg, int nLen, int nMemFlag)
{
	struct sk_buff *skb_1;
	struct nlmsghdr *nlh;
	int len = NLMSG_SPACE(nLen);

	if ((!pMsg) || (!g_socket) || (!g_nPid))
	{
		printk(KERN_ERR "SendNLMsg: invalid parameter!\n");
		return -1;
	}
	skb_1 = alloc_skb(len, nMemFlag);

	if (!skb_1)
	{
		printk(KERN_ERR "SendNLMsg:alloc_skb_1 error\n");
		return -2;
	}
	nlh = nlmsg_put(skb_1, 0, 0, nMsgType, nLen, 0);

	NETLINK_CB(skb_1).creds.pid = 0;
	NETLINK_CB(skb_1).dst_group = 0;

	MEMCPY(NLMSG_DATA(nlh), pMsg, nLen);

	len = netlink_unicast(g_socket, skb_1, g_nPid, MSG_DONTWAIT);
	if (len <= 0)
	{
		printk(KERN_ERR "netlink_unicast failed on SendNLMsg! Return value is %d\n", len);
	}

	return len;
}

inline void HandleDnsCache(SFilterHost *pFH)
{
	int i;
	struct SDnsCacheItem *pTmp1 = g_dcache_list;

	while (pTmp1)
	{
		if (Domain_Cmp(pTmp1->Host.sName, pTmp1->Host.nLen, pFH->Host.sName, pFH->Host.nLen) == 0)
		{
			for (i = 0; i < pTmp1->nCount; ++ i)
			{
				if (pTmp1->nHostType[i] == HOST_TYPE_IP)
				{
					LOCK_IP_TREE;
					AddIpFilter(&pTmp1->Answers[i].IpAddr, &pTmp1->Answers[i].IpAddr, pFH->FlagFilter, &pFH->FilterPort, ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);
					UNLOCK_IP_TREE;
				}
				else
				{
#ifdef __HANDLE_CNAME__
					LOCK_HOST_TREE;
					AddHostFilter(&pTmp1->Answers[i].RedirHost, pFH->FlagFilter, &pFH->FilterPort, ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);
					UNLOCK_HOST_TREE;
#endif

				}
			}
		}
		pTmp1 = pTmp1->pNext;
	}
}

#ifdef __DROP_ONDNSREQ__
inline FLAG_FILTER HandleDnsReq(const unsigned char *pData, int nLen)
{
	int i;
	int nReqCnt;
	FLAG_FILTER nRet = FLAG_FILTER_DEFAULT;
	const unsigned char *pEnd = pData + nLen;

	if (nLen < 12)
	{
		return nRet;
	}

	pData += 2;
	if (*pData & 0x80)
	{
		return nRet;
	}
	PRINTK(KERN_ALERT "Enter HandleDnsReq\n");
	pData += 2;
	nReqCnt = (*pData << 8) | *(pData + 1);
	if (nReqCnt > 0)
	{
		pData += 8;
		for (i = 0; i < nReqCnt; ++ i)
		{
			SFilterHost fh;
			int nCnt = 0;
			short nType;

			fh.Host.nLen = 0;
			while (*pData && (*pData < 192) && (pData < pEnd))
			{
				if (fh.Host.nLen == 0)
				{
					if (nCnt > 0)
					{
						*(fh.Host.sName + nCnt) = '.';
						++ nCnt;
					}

					MEMCPY(fh.Host.sName + nCnt, pData + 1, *pData);
					nCnt += *pData;
				}

				pData += (*pData + 1);
			}
			*pData ? pData += 2 : ++ pData;

			nType = (*pData << 8) | *(pData + 1);
			pData += 4;
			if (fh.Host.nLen == 0)
			{
				if ((nType == 0x01))
				{
					SFilterHost *pNode;

					*(fh.Host.sName + nCnt) = 0;
					fh.Host.nLen = nCnt;
					PRINTK(KERN_ALERT "DNS Req, host: %s, Name len:%d\n", fh.Host.sName, fh.Host.nLen);

					LOCK_HOST_TREE;
					pNode = bt_find(FilterHost_Cmp, g_host_tree, &fh);
					if (pNode)
					{
						nRet = pNode->FlagFilter;
						UNLOCK_HOST_TREE;
						PRINTK(KERN_ALERT "DNS Req, host: %s, flag: %s\n", fh.Host.sName,
							(nRet == FLAG_FILTER_BLACK) ? "BLACK" :
							((nRet == FLAG_FILTER_WHITE) ? "WHITE" : "DEFAULT"));

						return nRet;
					}
					else
					{
						UNLOCK_HOST_TREE;
					}
				}
			}
		}
	}
	PRINTK(KERN_ALERT ", set as default\n");

	return nRet;
}
#endif

inline void HandleDnsResp(const unsigned char *pData, int nLen, SDnsResp *pDnsResp)
{
	int i;
#ifdef __HANDLE_CNAME__
	const unsigned char *pOriData = pData;
#endif
	const unsigned char *pEnd = pData + nLen;
	int nReqCnt, nAnsCnt;

	pDnsResp->nCount = 0;
	if (nLen < 12)
	{
		PRINTK(KERN_ALERT "!!! DNS header length is error (%d)\n", nLen);
		return;
	}

	pData += 2;
	if (!(*pData & 0x80))
	{
		PRINTK(KERN_ALERT "!!! This is a DNS query\n");
		return;
	}
	pData += 2;
	nReqCnt = (*pData << 8) | *(pData + 1);
	if (nReqCnt < 1)
	{
		PRINTK(KERN_ALERT "!!! DNS Req cnt is less than 1 (%d)\n", nReqCnt);
		return;
	}
#ifdef __DEBUG__
	if (nReqCnt > 1)
	{
		PRINTK(KERN_ALERT "!!! Query reuest is more than 1 (%d)\n", nReqCnt);
	}
#endif
	pData += 2;
	nAnsCnt = (*pData << 8) | *(pData + 1);
	if (nAnsCnt < 1)
	{
		PRINTK(KERN_ALERT "!!! DNS ans cnt is less than 1 (%d)\n", nAnsCnt);
		return;
	}
	pData += 6;

	pDnsResp->Host.nLen = 0;
	for (i = 0; i < nReqCnt; ++ i)
	{
		int nCnt = 0;
		short nType;

		while (*pData && (*pData < 192) && (pData < pEnd))
		{
			if (pDnsResp->Host.nLen == 0)
			{
				if (nCnt > 0)
				{
					if (nCnt >= (MAX_NAME_LENGTH - 1))
					{
						PRINTK(KERN_ALERT "Domain name is too long(1), give up\n");
						return;
					}
					*(pDnsResp->Host.sName + nCnt) = '.';
					++ nCnt;
				}

				if ((nCnt + *pData) >= MAX_NAME_LENGTH)
				{
					PRINTK(KERN_ALERT "Domain name is too long(2), give up\n");
					return;
				}
				MEMCPY(pDnsResp->Host.sName + nCnt, pData + 1, *pData);
				nCnt += *pData;
			}

			pData += (*pData + 1);
		}
		*pData ? pData += 2 : ++ pData;

		nType = (*pData << 8) | *(pData + 1);
		pData += 4;
		if (pDnsResp->Host.nLen == 0)
		{
			if (((nType == 0x01) || (nType == 0x1C)))
			{
				*(pDnsResp->Host.sName + nCnt) = 0;
				pDnsResp->Host.nLen = nCnt;
			}
		}
	}
	if (pDnsResp->Host.nLen == 0)
	{
		PRINTK(KERN_ALERT "DnsResp no query found, nReqCnt: %d\n", nReqCnt);

		return;
	}
	PRINTK(KERN_ALERT "Query host resp: %s, addr count : %d\n", pDnsResp->Host.sName, nAnsCnt);

	for (i = 0; i < nAnsCnt; ++ i)
	{
		short nType,  nLength;

		while (*pData && (*pData < 192) && (pData < pEnd))
		{
			pData += (*pData + 1);
		}
		*pData ? pData += 2 : ++ pData;
		if ((pData + 10) >= pEnd)
		{
			PRINTK(KERN_ALERT "!!! Data length is not enough\n");
			return;
		}

		nType = (*pData << 8 ) | *(pData + 1);
		pData += 8;
		nLength = (*pData << 8 ) | *(pData + 1);
		pData += 2;
		PRINTK(KERN_ALERT "No.%d Type: %d, Length: %d\n", i + 1, nType, nLength);
		switch (nType)
		{
		case 1:
			pDnsResp->nHostType[pDnsResp->nCount] = HOST_TYPE_IP;
			if (nLength == sizeof(TYPE_IP_V4))
			{
				pDnsResp->Answers[pDnsResp->nCount].IpAddr.nType = IPADDR_TYPE_V4;
				MEMCPY(&pDnsResp->Answers[pDnsResp->nCount].IpAddr.uIpV4, pData, sizeof(TYPE_IP_V4));
				pDnsResp->Answers[pDnsResp->nCount].IpAddr.uIpV4 = ntohl(pDnsResp->Answers[pDnsResp->nCount].IpAddr.uIpV4);
				pData += sizeof(TYPE_IP_V4);
				PRINTK(KERN_ALERT "No.%d Ip addr is: 0x%08X for %s\n", pDnsResp->nCount + 1, (unsigned int)pDnsResp->Answers[pDnsResp->nCount].IpAddr.uIpV4, pDnsResp->Host.sName);
#ifdef __ALLOW_DIRECT_IP__
				{
					TYPE_IP_V4 *pDnsIp = MALLOC(sizeof(TYPE_IP_V4), ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);
					if (pDnsIp)
					{
						*pDnsIp = pDnsResp->IpAddrs[pDnsResp->nCount].uIpV4;
						LOCK_DNSIP_TREE;
						if (!bt_insert(DnsIp_Cmp, &g_dnsip_tree, pDnsIp))
						{
							FREE(pDnsIp);
						}
						UNLOCK_DNSIP_TREE;
					}
				}
#endif
				++ pDnsResp->nCount;
				if (pDnsResp->nCount >= MAX_CACHE_HOSTID_CNT)
				{
					PRINTK(KERN_ALERT "!!! Answer count is too much (%d)\n", MAX_CACHE_HOSTID_CNT);
					return;
				}
			}
			else
			{
				PRINTK(KERN_ALERT "Not a ip v4 address, nLength = %d\n", nLength);
				pData += nLength;
			}
			break;
		case 0x1C:
			pDnsResp->nHostType[pDnsResp->nCount] = HOST_TYPE_IP;
			if (nLength == sizeof(TYPE_IP_V6))
			{
				pDnsResp->Answers[pDnsResp->nCount].IpAddr.nType = IPADDR_TYPE_V6;
				MEMCPY_INVT(&pDnsResp->Answers[pDnsResp->nCount].IpAddr.uIpV6, pData, sizeof(TYPE_IP_V6));
				pData += sizeof(TYPE_IP_V6);
				PRINTK(KERN_ALERT "Found a IP V6 address for %s\n", pDnsResp->Host.sName);
#ifdef __DEBUG__
				print_ipv6(&pDnsResp->Answers[pDnsResp->nCount].IpAddr.uIpV6);
#endif 
				++ pDnsResp->nCount;
				if (pDnsResp->nCount >= MAX_CACHE_HOSTID_CNT)
				{
					PRINTK(KERN_ALERT "!!! Answer count is too much (%d)\n", MAX_CACHE_HOSTID_CNT);
					return;
				}
			}
			else
			{
				PRINTK(KERN_ALERT "Not a ip v6 address, nLength = %d\n", nLength);
				pData += nLength;
			}
			break;
		case 5:	
			{
				const unsigned char *pThisEnd = pData + nLength;
#ifdef __HANDLE_CNAME__
				int nCount = 0;

				pDnsResp->nHostType[pDnsResp->nCount] = HOST_TYPE_DOMAIN;
				while (*pData  && (pData < pEnd))
				{
					if (((*pData) & 192) == 192)
					{
						int nOffset = (*pData) & (~192);
						pData ++;
						nOffset <<= 8;
						nOffset += (*pData);
						pData = pOriData + nOffset;

						continue;
					}

					if (nCount > 0)
					{
						if (nCount >= (MAX_NAME_LENGTH - 1))
						{
							PRINTK(KERN_ALERT "CName is too long(1), give up\n");
						}
						else
						{
							*(pDnsResp->Answers[pDnsResp->nCount].RedirHost.sName + nCount) = '.';
						}
						++ nCount;
					}

					if ((nCount + *pData) >= MAX_NAME_LENGTH)
					{
						PRINTK(KERN_ALERT "CName is too long(2), give up\n");
					}
					else
					{
						MEMCPY(pDnsResp->Answers[pDnsResp->nCount].RedirHost.sName + nCount, pData + 1, *pData);
					}
					nCount += *pData;
					pData += (*pData + 1);
				}
				if (nCount < MAX_NAME_LENGTH)
				{
					pDnsResp->Answers[pDnsResp->nCount].RedirHost.nLen = nCount;
					pDnsResp->Answers[pDnsResp->nCount].RedirHost.sName[nCount] = 0;
					PRINTK(KERN_ALERT "Found CName resp: %s for %s\n", pDnsResp->Answers[pDnsResp->nCount].RedirHost.sName, pDnsResp->Host.sName);
					++ pDnsResp->nCount;
					if (pDnsResp->nCount >= MAX_CACHE_HOSTID_CNT)
					{
						PRINTK(KERN_ALERT "!!! Answer count is too much (%d)\n", MAX_CACHE_HOSTID_CNT);
						return;
					}
				}
#endif 
				pData = pThisEnd;
			}
			break;
		default:
			if ((pData + nLength) < pEnd)
			{
				pData += nLength;
			}
			else
			{
				PRINTK(KERN_ALERT "!!! Data length is not enough\n");
				return;
			}
			break;
		}
	}
}

#ifdef __MY_THREAD__
#define SKB_DELAY_TIME	(HZ/500)	
int ThreadTask(void *pData)
{
	while (!kthread_should_stop())
	{
		struct SSkbDelay *pskb, *plast = NULL;

		LOCK_SKB_LIST;
		pskb = g_skb_delay_list;
		while (pskb)
		{
			if ((jiffies < pskb->time) || (jiffies > (pskb->time + SKB_DELAY_TIME)))	
			{
				struct SSkbDelay *ptmp = pskb;
#if 1
				struct net_device *dev = NULL;

				dev = dev_get_by_name(&init_net, pskb->dev_name);
				if (!dev)
				{
					printk("dev_get_by_name %s fail\n", pskb->dev_name);
					kfree_skb(pskb->skb);
				}
				else
				{
					int ret_val;

					pskb->skb->dev = dev;
					pskb->skb->protocol = eth_type_trans(pskb->skb, dev);
					ret_val = netif_rx(pskb->skb);	
					if (likely(ret_val != NET_RX_DROP))
					{
						dev->stats.rx_bytes += pskb->skb->len;
						dev->stats.rx_packets ++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,113)
						dev->last_rx = jiffies;
#endif 
					}
					else	
					{
						dev->stats.rx_errors ++;
						dev->stats.rx_dropped ++;
						printk("netif_rx_ni fail, ret val: %d\n", ret_val);
					}
					dev_put(dev);
				}
#else
				kfree_skb(pskb->skb);
#endif 
				if (plast)
				{
					plast->pNext = pskb->pNext;
					pskb = plast->pNext;
				}
				else
				{
					pskb = pskb->pNext;
				}
				if (g_skb_delay_list == ptmp)
				{
					g_skb_delay_list = ptmp->pNext;
				}
				FREE(ptmp);
			}
			else
			{
				plast = pskb;
				pskb = plast->pNext;
			}
		}
		UNLOCK_SKB_LIST;

		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(SKB_DELAY_TIME);
	}

	return 0;
}
#endif	

void NL_receive_data(struct sk_buff *__skb)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	skb = skb_get(__skb);
	nlh = nlmsg_hdr(skb);
	if ((skb->len == NLMSG_SPACE(0))	
		&& (nlh->nlmsg_type == 0))
	{
		g_nPid = nlh->nlmsg_pid;
		PRINTK(KERN_ALERT "User space process id received: %d\n", g_nPid);
	}
	else
	{
		const char *pData = NLMSG_DATA(nlh);

		switch (nlh->nlmsg_type)	
		{
		case PACKFILTER_IOCTL_SET_FILTERIP:
			{
				int64_t nValue = 0;
				int64_t i;
				SFilterIp fi;

				MEMCPY(&nValue, pData, sizeof(nValue));	
				pData += sizeof(nValue);
				for (i = 0; i < nValue; ++ i)
				{
					MEMCPY(&fi, pData, sizeof(fi));
					pData += sizeof(fi);
					LOCK_IP_TREE;
					AddIpFilter(&fi.IpBegin, &fi.IpEnd, fi.FlagFilter, &fi.FilterPort, ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);
					UNLOCK_IP_TREE;
#ifdef __DEBUG__
					if (fi.IpBegin.nType == IPADDR_TYPE_V4)
					{
#ifdef CONFIG_64BIT	
						PRINTK(KERN_ALERT "AddFilterIp, sizeof(SFilterIp): %lu, 0x%08X, %s\n", sizeof(fi), (unsigned int)fi.IpBegin.uIpV4, fi.FlagFilter == FLAG_FILTER_WHITE ? "white":
#else
						PRINTK(KERN_ALERT "AddFilterIp, sizeof(SFilterIp): %u, 0x%08X, %s\n", sizeof(fi), (unsigned int)fi.IpBegin.uIpV4, fi.FlagFilter == FLAG_FILTER_WHITE ? "white":
#endif 
												(fi.FlagFilter == FLAG_FILTER_BLACK ? "Black" : "Default"));
					}
					else
					{
						PRINTK(KERN_ALERT "AddFilterIp, %s\n", fi.FlagFilter == FLAG_FILTER_WHITE ? "white":
												(fi.FlagFilter == FLAG_FILTER_BLACK ? "Black" : "Default"));
						print_ipv6(&fi.IpBegin.uIpV6);
					}
#endif 
				}
			}
			break;
		case PACKFILTER_IOCTL_SET_FILTERHOST:
			{
				SFilterHost *pNodeTmp = MALLOC(sizeof(SFilterHost), ALLOCMEM_MIDD_FLAG);

				if (pNodeTmp)
				{
					MEMCPY(pNodeTmp, pData, sizeof(*pNodeTmp));
					if ((pNodeTmp->Host.sName[0] == '*') && (pNodeTmp->Host.nLen == 1)) 
					{
						if ((pNodeTmp->FilterPort.blCnt <= 0) && (pNodeTmp->FilterPort.wlCnt <= 0))	
						{
							g_DefaultFilter = pNodeTmp->FlagFilter;
							PRINTK(KERN_ALERT "Set Default Filter, %s\n", g_DefaultFilter == FLAG_FILTER_WHITE ? "white":
											(g_DefaultFilter == FLAG_FILTER_BLACK ? "Black" : "Default"));
						}
						else
						{
							if (pNodeTmp->FilterPort.blCnt > 0)
							{
								g_FilterPort.blCnt = pNodeTmp->FilterPort.blCnt;
								MEMCPY(&g_FilterPort.BL[0], &pNodeTmp->FilterPort.BL[0], sizeof(SPort) * g_FilterPort.blCnt);
							}
							if (pNodeTmp->FilterPort.wlCnt > 0)
							{
								g_FilterPort.wlCnt = pNodeTmp->FilterPort.wlCnt;
								MEMCPY(&g_FilterPort.WL[0], &pNodeTmp->FilterPort.WL[0], sizeof(SPort) * g_FilterPort.wlCnt);
							}
							PRINTK(KERN_ALERT "Set Default Port Filter, BL: %d, WL: %d\n", g_FilterPort.blCnt, g_FilterPort.wlCnt);
						}
					}
					else
					{
						LOCK_DCACHE_LIST;
						HandleDnsCache(pNodeTmp);	
						UNLOCK_DCACHE_LIST;

						LOCK_HOST_TREE;
						AddHostFilter(&pNodeTmp->Host, pNodeTmp->FlagFilter, &pNodeTmp->FilterPort, ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);
						UNLOCK_HOST_TREE;
#ifdef CONFIG_64BIT	
						PRINTK(KERN_ALERT "AddFilterHost, sizeof(SFilterHost): %lu, %s, %s\n", sizeof(*pNodeTmp), pNodeTmp->Host.sName, pNodeTmp->FlagFilter == FLAG_FILTER_WHITE ? "white":
#else
						PRINTK(KERN_ALERT "AddFilterHost, sizeof(SFilterHost): %u, %s, %s\n", sizeof(*pNodeTmp), pNodeTmp->Host.sName, pNodeTmp->FlagFilter == FLAG_FILTER_WHITE ? "white":
#endif 
												(pNodeTmp->FlagFilter == FLAG_FILTER_BLACK ? "Black" : "Default"));
					}
					FREE(pNodeTmp);
				}
				else
				{
					printk(KERN_ERR "Malloc memory failed for PACKFILTER_IOCTL_SET_FILTERHOST\n");
				}
			}
			break;
		case PACKFILTER_IOCTL_SET_DEFAULTFILTER:
			MEMCPY(&g_DefaultFilter, pData, sizeof(g_DefaultFilter));
			PRINTK(KERN_ALERT "Set Default Filter, %s\n", g_DefaultFilter == FLAG_FILTER_WHITE ? "white":
											(g_DefaultFilter == FLAG_FILTER_BLACK ? "Black" : "Default"));
			break;
		case PACKFILTER_IOCTL_SET_ENABLED:
			MEMCPY(&g_nEnabled, pData, sizeof(g_nEnabled));
			PRINTK(KERN_ALERT "Filter Enable: %s\n", g_nEnabled ? "on": "off");
			break;
		case PACKFILTER_IOCTL_SET_REINIT:
			LOCK_IP_TREE;
			LOCK_UID_TREE;
			LOCK_HOST_TREE;
			release_data();
			init_data();
			UNLOCK_IP_TREE;
			UNLOCK_UID_TREE;
			UNLOCK_HOST_TREE;
			PRINTK(KERN_ALERT "Filter Reinited\n");
			break;
		case PACKFILTER_IOCTL_SET_FILTERUID:
			{
				SUidNode *pNode2 = NULL;
				SUidNode *pNodeTmp = MALLOC(sizeof(SUidNode), ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);
				g_nInUse = 1;

				if (pNodeTmp)
				{
					MEMCPY(pNodeTmp, pData, sizeof(*pNodeTmp));
					LOCK_UID_TREE;
					pNode2 = bt_find(Uid_Cmp, g_uid_tree, pNodeTmp);
					if (pNode2)	
					{
						PRINTK(KERN_ALERT "UID: %d 's rule changed\n", pNodeTmp->uUid);
						pNode2->FlagFilter = pNodeTmp->FlagFilter;
						FREE(pNodeTmp);
					}
					else	
					{
						if (!bt_insert(Uid_Cmp, &g_uid_tree, pNodeTmp))
						{
							printk(KERN_ERR "bt_insert failed, for user uid: %u\n", pNodeTmp->uUid);
							FREE(pNodeTmp);
						}
					}
					UNLOCK_UID_TREE;
					PRINTK(KERN_ALERT "AddFilterUid, %d, %s\n", pNodeTmp->uUid, pNodeTmp->FlagFilter == FLAG_FILTER_WHITE ? "white":
												(pNodeTmp->FlagFilter == FLAG_FILTER_BLACK ? "Black" : "Default"));
				}
				else
				{
					printk(KERN_ERR "Malloc memory failed for PACKFILTER_IOCTL_SET_FILTERUID\n");
				}
			}
			break;
		case PACKFILTER_IOCTL_SET_CLRUID:	
			LOCK_UID_TREE;
			release_uid();
			init_uid();
			UNLOCK_UID_TREE;
			PRINTK(KERN_ALERT "Filter UID cleared\n");
			break;
		case PACKFILTER_IOCTL_SET_CLRIPHOST:	
			LOCK_IP_TREE;
			LOCK_HOST_TREE;
			release_ip_host();
			init_ip_host();
			UNLOCK_IP_TREE;
			UNLOCK_HOST_TREE;
			PRINTK(KERN_ALERT "Filter IP and HOST cleared\n");
			break;
		default:
			printk(KERN_ERR "WJFirewall: Unknown command type (0x%X)\n", nlh->nlmsg_type);
			break;
		}
	}
	kfree_skb(skb);
}

inline void AddNewToQueue(SIpAddr *pIpAddr)
{
	SendNLMsg(PACKFILTER_IOCTL_GET_NEWIP, (char *)pIpAddr, sizeof(SIpAddr), ALLOCMEM_HIGH_FLAG);
}

inline FLAG_FILTER IpFilter(const struct SIpNode *pToFind, int nPort, bool bValidPort)
{
	const struct SIpNode *pNode;
	FLAG_FILTER nIpFF = FLAG_FILTER_DEFAULT;
	
	LOCK_IP_TREE;
	pNode = bt_find(Ip_Cmp, g_ip_tree, pToFind);
	if (pNode)	
	{
		if (pNode->pNext)	
		{
			pNode = pNode->pNext;
		}

		while (pNode)
		{
			if (Ip_Cmp(pToFind, pNode) == 0)
			{
				FLAG_FILTER flg;

				nIpFF |= pNode->FlagFilter;
				if (bValidPort)
				{
					flg = FindPort(&pNode->FilterPort, nPort);
					if (flg != FLAG_FILTER_DEFAULT)
					{
						nIpFF |= flg;
					}
				}
				if ((nIpFF & FLAG_FILTER_BLACK) == FLAG_FILTER_BLACK)	
				{
					break;
				}
			}
			pNode = pNode->pNext;
		}
	}
	UNLOCK_IP_TREE;

	if ((nIpFF & FLAG_FILTER_BLACK) == FLAG_FILTER_BLACK)	
	{
		return FLAG_FILTER_BLACK;
	}
	else if ((nIpFF & FLAG_FILTER_WHITE) == FLAG_FILTER_WHITE)
	{
		return FLAG_FILTER_WHITE;
	}

	return FLAG_FILTER_DEFAULT;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
inline FLAG_FILTER ProcessFilter(struct sk_buff *skb, const struct nf_hook_state *state, __kernel_uid32_t *pUid)
#else
inline FLAG_FILTER ProcessFilter(struct sk_buff *skb, __kernel_uid32_t *pUid)
#endif
{
	struct sock *sk = NULL;
	FLAG_FILTER nUidFF = FLAG_FILTER_DEFAULT;
	*pUid = 0xFFFFFFFF;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)	
	if (virt_addr_valid(state))
	{
		sk = state->sk;
		PRINTK("state valid\n");
	}
	else if (virt_addr_valid(skb))
	{
		sk = skb->sk;
		PRINTK("skb valid\n");
	}
	else
	{
		printk(KERN_ERR "state(0x%p) && skb (0x%p) is invalid\n", state, skb);
	}
#else
	if (virt_addr_valid(skb))
	{
		sk = skb->sk;
		PRINTK("skb valid\n");
	}
	else
	{
		printk(KERN_ERR "skb (0x%p) is invalid\n", skb);
	}
#endif
	if (virt_addr_valid(sk))
	{
		if (virt_addr_valid(sk->sk_socket))
		{
			if (virt_addr_valid(sk->sk_socket->file))
			{
				if (virt_addr_valid(sk->sk_socket->file->f_cred))
				{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,10,0)
					*pUid = sk->sk_socket->file->f_cred->uid;
#else
					*pUid = sk->sk_socket->file->f_cred->uid.val;
#endif	
				}
				else
				{
					PRINTK(KERN_ALERT "socket file cred(0x%p) is invalid\n", sk->sk_socket->file->f_cred);
				}
			}
			else
			{
				PRINTK(KERN_ALERT "socket file(0x%p) is invalid\n", sk->sk_socket->file);
			}

		}
		else
		{
			PRINTK(KERN_ALERT "socket(0x%p) is invalid\n", sk->sk_socket);
		}
	}
	else
	{
		PRINTK(KERN_ALERT "sk(0x%p) is invalid\n", sk);
	}

	if (*pUid != 0xFFFFFFFF)
	{
		SUidNode *pUidNode = NULL;
		SUidNode tmpUidNode;

		tmpUidNode.uUid = *pUid;
		PRINTK("UID: %d\n", *pUid);

		LOCK_UID_TREE;
		pUidNode = bt_find(Uid_Cmp, g_uid_tree, &tmpUidNode);
		if (!IS_ERR_OR_NULL(pUidNode))      
		{
			nUidFF = pUidNode->FlagFilter;
			UNLOCK_UID_TREE;
			PRINTK(KERN_ALERT "ProcessFilter: uid %d found! Flag is %d\n", *pUid, nUidFF);
		}
		else if (*pUid < 1000)	
		{
			*pUid = 0;

			UNLOCK_UID_TREE;
			nUidFF = FLAG_FILTER_WHITE;
			PRINTK(KERN_ALERT "ProcessFilter: uid %d is for system! Flag is White\n", *pUid);
		}
		else
		{
			nUidFF = FLAG_FILTER_DEFAULT;
			UNLOCK_UID_TREE;
		}
	}
	else
	{
		nUidFF = FLAG_FILTER_WHITE;
	}

	return nUidFF;
}



bool GenPakRefuse(struct sk_buff *oskb, int tcphoff)
{
	struct sk_buff *nskb;
	struct iphdr *iph;
	struct ipv6hdr *iphv6;
	struct tcphdr *tph;
	struct dst_entry *dst = skb_dst(oskb);
	struct net_device *dst_dev;
	unsigned char *rdptr;
	uint16_t uPort;
	uint32_t tcp_len = 0;
	uint32_t pay_load = sizeof(struct tcphdr) + tcp_len;
	uint32_t ip_tot_len = tcphoff + pay_load;
	uint32_t tot_len = ETH_HLEN + ip_tot_len;

	if (!dst)
	{
		printk("oskb->dst is null\n");
		return false;
	}
	dst_dev = dst->dev;
	if (!dst_dev)
	{
		printk("oskb->dst->dev==NULL\n");
		return false;
	}

	if (skb_is_nonlinear(oskb))
	{
		printk("###### nonlinear skb found\n");
		if (0 != skb_linearize(oskb))
		{
			printk(KERN_ERR "GenPakRefuse: skb_linearize failed!\n");

			return false;
		}
	}

	
	nskb = dev_alloc_skb(tot_len > 60 ? tot_len + 4 : 64);
	if (nskb != NULL)
	{
		struct ethhdr *eth;

		skb_reserve(nskb, 2);
		rdptr = (u8 *)skb_put(nskb, tot_len > 60 ? tot_len : 60);
		eth = (struct ethhdr *)rdptr;
		MEMCPY(eth->h_dest, dst_dev->dev_addr, ETH_ALEN);
		memset(eth->h_source, 0, ETH_ALEN);
		eth->h_proto = oskb->protocol;
		MEMCPY(rdptr + ETH_HLEN, oskb->data, ip_tot_len);
		if (tot_len < 60)
		{
			memset(rdptr + tot_len, 0, 60 - tot_len);
		}
	}
	else
	{
		printk("dev_alloc_skb fail\n");
		return false;
	}

	iph = (struct iphdr *)(rdptr + ETH_HLEN);
	iphv6 = (struct ipv6hdr *)(rdptr + ETH_HLEN);
	tph = (struct tcphdr *)(rdptr + ETH_HLEN + tcphoff);
	tcp_len = ip_tot_len - tcphoff;

	tph->window = 0;
	tph->syn = 0;   
	tph->rst = 1;   
	tph->psh = 0; 	
	tph->ack = 1;   
	tph->urg = 0; 
	tph->urg_ptr = 0;
	tph->doff = tcp_len >> 2;	
	tph->ack_seq = htonl(ntohl(tph->seq) + 1);	
	tph->seq = 0;	

	uPort = tph->source;
	tph->source = tph->dest;
	tph->dest = uPort;

	tph->check = 0;
	if (oskb->protocol == PROT_IPV4)	
	{
		TYPE_IP_V4 uIpV4;

		uIpV4 = iph->saddr;
		iph->saddr = iph->daddr;
		iph->daddr = uIpV4;

		iph->tot_len = htons(ip_tot_len);	
		nskb->csum = csum_partial((char *)tph, tcp_len, 0);      
		tph->check = csum_tcpudp_magic(iph->saddr, iph->daddr, tcp_len, IPPROTO_TCP, nskb->csum);
		iph->check = 0;
		ip_send_check(iph);
	}
	else	
	{
		TYPE_IP_V6 uIpV6;

		MEMCPY(&uIpV6, &iphv6->saddr, sizeof(iphv6->saddr));
		MEMCPY(&iphv6->saddr, &iphv6->daddr, sizeof(iphv6->saddr));
		MEMCPY(&iphv6->daddr, &uIpV6, sizeof(iphv6->daddr));

		iphv6->payload_len = htons(pay_load);	
		nskb->csum = csum_partial((char *)tph, tcp_len, 0);      
		tph->check = csum_ipv6_magic(&iphv6->saddr, &iphv6->daddr, tcp_len, IPPROTO_TCP, nskb->csum);        
	}
	nskb->ip_summed = CHECKSUM_UNNECESSARY;
	if (tph->check == 0)
		tph->check = 0xFFFF;

	nskb->mac_len = ETH_HLEN;
	skb_shinfo(nskb)->nr_frags = 0;
	skb_shinfo(nskb)->frag_list = NULL;

	{
#ifdef __MY_THREAD__
		struct SSkbDelay *pskb_delay = MALLOC(sizeof(struct SSkbDelay), ALLOCMEM_MIDD_FLAG);
		if (pskb_delay)
		{
			pskb_delay->skb = nskb;
			pskb_delay->time = jiffies;
			strcpy(pskb_delay->dev_name, dst_dev->name);
			LOCK_SKB_LIST;
			if (g_skb_delay_list == NULL)
			{
				pskb_delay->pNext = NULL;
				g_skb_delay_list = pskb_delay;
			}
			else
			{
				pskb_delay->pNext = g_skb_delay_list;
			}
			UNLOCK_SKB_LIST;
		}
		else
		{
			printk(KERN_ERR "Malloc memory failed for pskb_delay\n");
			kfree_skb(nskb);

			return false;
		}
#else
		struct net_device *dev = NULL;

		dev = dev_get_by_name(&init_net, dst_dev->name);
		if (!dev)
		{
			printk("dev_get_by_name %s fail\n", dst_dev->name);
			kfree_skb(nskb);

			return false;
		}
		else
		{
			int ret_val;

			nskb->dev = dev;
			nskb->protocol = eth_type_trans(nskb, dev);
			ret_val = netif_rx(nskb);	
			if (likely(ret_val != NET_RX_DROP))
			{
				dev->stats.rx_bytes += tot_len;
				dev->stats.rx_packets ++;
				dev->last_rx = jiffies;
			}
			else	
			{
				dev->stats.rx_errors ++;
				dev->stats.rx_dropped ++;
				dev_put(dev);
				printk("netif_rx_ni fail, ret val: %d\n", ret_val);

				return false;
			}
			dev_put(dev);
		}
#endif 
	}

	return true;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
static unsigned int hook_func_in(void *priv,
                               struct sk_buff *skb,
                               const struct nf_hook_state *state)
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(3,10,0)
static unsigned int hook_func_in(unsigned int hooknum,
				struct sk_buff *skb,
				const struct net_device *in,
				const struct net_device *out,
				int (*okfn)(struct sk_buff *))
#else
static unsigned int hook_func_in(const struct nf_hook_ops *ops,
				struct sk_buff *skb,
				const struct net_device *in,
				const struct net_device *out,
				int (*okfn)(struct sk_buff *))
#endif	
{
	const struct iphdr *iph;
	const struct ipv6hdr *iphv6;
	unsigned char *pUdp;

	if (IS_ERR_OR_NULL(skb))
	{
		printk(KERN_ERR "hook_func_in, skb is null");
		return NF_ACCEPT;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	if (IS_ERR_OR_NULL(state))
	{
		printk(KERN_ERR "hook_func_in, state is null");
	}
#endif

	if (skb->protocol == PROT_IPV4)	
	{
		iph = ip_hdr(skb);
		if (((iph->daddr & LOOP_MASK) == LOOP_MASK)	
			|| ((iph->daddr & BROADCAST_MASK) == BROADCAST_MASK))	
		{
			return NF_ACCEPT;
		}

		if (iph->protocol != IPPROTO_UDP)
		{
			return NF_ACCEPT;
		}
		pUdp = skb->data + (iph->ihl << 2);
	}
	else if (skb->protocol == PROT_IPV6)	
	{
		int udphoff;
		u_int8_t nexthdr;
		__be16 frag_off;

		iphv6 = ipv6_hdr(skb);

		if (ipv6_addr_any(&iphv6->saddr) || ipv6_addr_any(&iphv6->daddr))
		{
			return NF_ACCEPT;
		}


		if (ipv6_addr_equal(&iphv6->saddr, &iphv6->daddr))
		{
			return NF_ACCEPT;
		}


		if (ipv6_addr_loopback(&iphv6->saddr) || ipv6_addr_loopback(&iphv6->daddr))
		{
			return NF_ACCEPT;
		}


		if (iphv6->nexthdr != NEXTHDR_UDP)
		{
			return NF_ACCEPT;
		}

		nexthdr = iphv6->nexthdr;
		udphoff = ipv6_skip_exthdr(skb, sizeof(*iphv6), &nexthdr, &frag_off);
		pUdp = skb->data + udphoff;
		PRINTK(KERN_ALERT "IP V6 UDP packet IN found!\n");
	}
	else
	{
		return NF_ACCEPT;
	}
	
	{
		unsigned short uPort = *pUdp;	

		uPort <<= 8;
		uPort += *(pUdp + 1);

		PRINTK(KERN_ALERT "IN UDP found! source port is %d\n", uPort);
		if (uPort == 53)	
		{
			short nLen = *(pUdp + 4);
			nLen <<= 8;
			nLen += *(pUdp + 5);
			if (nLen > UDP_HEADER_LEN)
			{
				SDnsResp *pDnsResp;

				pDnsResp = MALLOC(sizeof(SDnsResp), ALLOCMEM_MIDD_FLAG | ALLOCMEM_ZERO_FLAG);	
				if (pDnsResp)
				{
					HandleDnsResp(pUdp + UDP_HEADER_LEN, nLen - UDP_HEADER_LEN, pDnsResp);
					if (pDnsResp->nCount > 0)
					{
						AddDnsCache(pDnsResp);
					}
					FREE(pDnsResp);
				}
				else
				{
					printk(KERN_ERR "Malloc memory failed for pDnsResp\n");
				}
			}
		}
	}

	return NF_ACCEPT;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
static unsigned int hook_func_out(void *priv,
                               struct sk_buff *skb,
                               const struct nf_hook_state *state)
#elif LINUX_VERSION_CODE <= KERNEL_VERSION(3,10,0)
static unsigned int hook_func_out(unsigned int hooknum,
				struct sk_buff *skb,
				const struct net_device *in,
				const struct net_device *out,
				int (*okfn)(struct sk_buff *))
#else
static unsigned int hook_func_out(const struct nf_hook_ops *ops,
				struct sk_buff *skb,
				const struct net_device *in,
				const struct net_device *out,
				int (*okfn)(struct sk_buff *))
#endif	
{
	const struct iphdr *iph = ip_hdr(skb);
	const struct ipv6hdr *iphv6 = ipv6_hdr(skb);

	if (!g_nEnabled)	
	{
		return NF_ACCEPT;
	}

	if (IS_ERR_OR_NULL(skb))
	{
		printk(KERN_ERR "hook_func_out, skb is null");
		return NF_ACCEPT;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
	if (IS_ERR_OR_NULL(state))
	{
		printk(KERN_ERR "hook_func_out, state is null");
	}
#endif

	if (skb->protocol == PROT_IPV4)	
	{
		if (((iph->daddr & LOOP_MASK) == LOOP_MASK)	
			|| ((iph->daddr & BROADCAST_MASK) == BROADCAST_MASK))	
		{
			return NF_ACCEPT;
		}
	}
	else if (skb->protocol == PROT_IPV6)
	{

		if (ipv6_addr_any(&iphv6->saddr) || ipv6_addr_any(&iphv6->daddr))
		{
			return NF_ACCEPT;
		}


		if (ipv6_addr_equal(&iphv6->saddr, &iphv6->daddr))
		{
			return NF_ACCEPT;
		}


		if (ipv6_addr_loopback(&iphv6->saddr) || ipv6_addr_loopback(&iphv6->daddr))
		{
			return NF_ACCEPT;
		}

		PRINTK(KERN_ALERT "IP V6 packet OUT found!\n");
	}
	else
	{
		return NF_ACCEPT;
	}

	{
		unsigned int uRet = NF_ACCEPT;
		FLAG_FILTER nIpFF = FLAG_FILTER_DEFAULT;
		struct SIpNode tmpNode;
		__kernel_uid32_t uUid = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)
		FLAG_FILTER nUidFF = ProcessFilter(skb, state, &uUid);
#ifdef __DEBUG__
		if (virt_addr_valid(state))
		{
			if (skb->sk != state->sk)
			{
				PRINTK(KERN_ERR "skb->sk: 0x%p, state->sk: 0x%p\n", skb->sk, state->sk);
			}
		}
#endif 
#else
		FLAG_FILTER nUidFF = ProcessFilter(skb, &uUid);
#endif
	

		if (skb->protocol == PROT_IPV4)	
		{
			tmpNode.IpBegin.nType = IPADDR_TYPE_V4;
			tmpNode.IpEnd.nType = IPADDR_TYPE_V4;
			tmpNode.IpBegin.uIpV4 = ntohl(iph->daddr);
			tmpNode.IpEnd.uIpV4 = ntohl(iph->daddr);
		}
		else
		{
			tmpNode.IpBegin.nType = IPADDR_TYPE_V6;
			tmpNode.IpEnd.nType = IPADDR_TYPE_V6;
			MEMCPY_INVT(&tmpNode.IpBegin.uIpV6, &iphv6->daddr, sizeof(iphv6->daddr));
			MEMCPY_INVT(&tmpNode.IpEnd.uIpV6, &iphv6->daddr, sizeof(iphv6->daddr));
#ifdef __DEBUG__
			print_ipv6(&tmpNode.IpBegin.uIpV6);
#endif 
		}
		if (((skb->protocol == PROT_IPV4) && (iph->protocol == IPPROTO_TCP)) 
			|| ((skb->protocol == PROT_IPV6) && (iphv6->nexthdr == NEXTHDR_TCP)))	
		{
			unsigned char *pTcp;
			int tcphoff;
			uint16_t uPort;
			int32_t nConfirmNo = 0;	
			unsigned char uFlag;

			if (skb->protocol == PROT_IPV4)	
			{
				tcphoff = iph->ihl << 2;
			}
			else
			{
				u_int8_t nexthdr;
				__be16 frag_off;

				nexthdr = iphv6->nexthdr;
				tcphoff = ipv6_skip_exthdr(skb, sizeof(*iphv6), &nexthdr, &frag_off);
			}
			pTcp = skb->data + tcphoff;
			uPort = *(pTcp + 2);
			uFlag = *(pTcp + 13) & 0x3F;
			MEMCPY(&nConfirmNo, pTcp + 8, sizeof(nConfirmNo));

#ifndef __BLOCK_ALL__
			if ((uFlag & 0x02) == 0x02)	
#endif 
			{
				uPort <<= 8;
				uPort += *(pTcp + 3);


				nIpFF = IpFilter(&tmpNode, uPort, true);
				if (nIpFF == FLAG_FILTER_DEFAULT)
				{
					FLAG_FILTER flg2 = FindPort(&g_FilterPort, uPort);
					if (flg2 != FLAG_FILTER_DEFAULT)
					{
						nIpFF = flg2;
					}
				}

				if ((nIpFF == FLAG_FILTER_BLACK) || (nUidFF == FLAG_FILTER_BLACK)
#ifndef __ALLOW_DIRECT_IP__
					|| ((nIpFF == FLAG_FILTER_DEFAULT) && (nUidFF == FLAG_FILTER_DEFAULT)
					&& (g_DefaultFilter == FLAG_FILTER_BLACK)) 
#endif	
					)
				{
					if (skb->protocol == PROT_IPV4)	
					{
						printk(KERN_ERR "WantJoin: TCP [%u] out... to %u.%u.%u.%u, drop it\n", uUid,
							(tmpNode.IpBegin.uIpV4 >> 24) & 0xFF, (tmpNode.IpBegin.uIpV4 >> 16) & 0xFF, 
							(tmpNode.IpBegin.uIpV4 >> 8) & 0xFF, tmpNode.IpBegin.uIpV4 & 0xFF);
					}
					else
					{
						print_ipv6(&tmpNode.IpBegin.uIpV6);
						printk(KERN_ERR " WantJoin: TCP [%u] out... dropp it\n", uUid);
					}
					GenPakRefuse(skb, tcphoff);
					uRet = NF_DROP;
				}
#ifdef __DEBUG__
				else
				{
					if (skb->protocol == PROT_IPV4)	
					{
						printk(KERN_ERR "WantJoin: TCP [%u] out... to %u.%u.%u.%u\n", uUid,
							(tmpNode.IpBegin.uIpV4 >> 24) & 0xFF, (tmpNode.IpBegin.uIpV4 >> 16) & 0xFF, 
							(tmpNode.IpBegin.uIpV4 >> 8) & 0xFF, tmpNode.IpBegin.uIpV4 & 0xFF);
					}
					else
					{
						print_ipv6(&tmpNode.IpBegin.uIpV6);
						printk(KERN_ERR " WantJoin: TCP [%u] out...\n", uUid);
					}
				}
#endif 
			}
		}
		else if (((skb->protocol == PROT_IPV4) && (iph->protocol == IPPROTO_UDP)) 
			|| ((skb->protocol == PROT_IPV6) && (iphv6->nexthdr == NEXTHDR_UDP)))	
		{
			unsigned char *pUdp;
			unsigned short uPort;	

			if (skb->protocol == PROT_IPV4)	
			{
				pUdp = skb->data + (iph->ihl << 2);
			}
			else
			{
				int udphoff;
				u_int8_t nexthdr;
				__be16 frag_off;

				nexthdr = iphv6->nexthdr;
				udphoff = ipv6_skip_exthdr(skb, sizeof(*iphv6), &nexthdr, &frag_off);
				pUdp = skb->data + udphoff;
			}

			uPort = *(pUdp + 2);
			uPort <<= 8;
			uPort += *(pUdp + 3);
			if ((uPort != 53)	
				&& (uPort != 68) && (uPort != 67))	
			{
				nIpFF = IpFilter(&tmpNode, uPort, false);
				if ((nUidFF == FLAG_FILTER_BLACK) || (nIpFF == FLAG_FILTER_BLACK)
#ifndef __ALLOW_DIRECT_IP__
						|| ((nUidFF == FLAG_FILTER_DEFAULT) && (nIpFF == FLAG_FILTER_DEFAULT)
						&& (g_DefaultFilter == FLAG_FILTER_BLACK)) 
#endif	
					)
				{
					uRet = NF_DROP;
					if (skb->protocol == PROT_IPV4)	
					{
						printk(KERN_ERR "WantJoin: UDP [%u] out... to %u.%u.%u.%u, drop it\n", uUid,
							(tmpNode.IpBegin.uIpV4 >> 24) & 0xFF, (tmpNode.IpBegin.uIpV4 >> 16) & 0xFF,
							(tmpNode.IpBegin.uIpV4 >> 8) & 0xFF, tmpNode.IpBegin.uIpV4 & 0xFF);
					}
					else
					{
						print_ipv6(&tmpNode.IpBegin.uIpV6);
						printk(KERN_ERR " WantJoin: UDP [%u] out... drop it\n", uUid);
					}
				}
			}
#ifdef __DROP_ONDNSREQ__
			else if (uPort == 53)	
			{
				short nLen = *(pUdp + 4);
				nLen <<= 8;
				nLen += *(pUdp + 5);
				if (nLen > UDP_HEADER_LEN)
				{

					FLAG_FILTER nHostFF = HandleDnsReq(pUdp + UDP_HEADER_LEN, nLen - UDP_HEADER_LEN);
					PRINTK(KERN_ALERT "nIpFF:%d, nHostFF:%d, g_DefaultFilter:%d\n",
							nIpFF, nHostFF, g_DefaultFilter);
					if ((nUidFF == FLAG_FILTER_BLACK) || (nIpFF == FLAG_FILTER_BLACK)
							|| (nHostFF == FLAG_FILTER_BLACK)
							|| ((nUidFF == FLAG_FILTER_DEFAULT) 
							&& (nIpFF == FLAG_FILTER_DEFAULT) && (nHostFF == FLAG_FILTER_DEFAULT)
							&& (g_DefaultFilter == FLAG_FILTER_BLACK))) 
					{
						uRet = NF_DROP;
						if (skb->protocol == PROT_IPV4)	
						{
							printk(KERN_ERR "WantJoin: DNS [%u] Req... to %u.%u.%u.%u, drop it\n", uUid,
								(tmpNode.IpBegin.uIpV4 >> 24) & 0xFF, (tmpNode.IpBegin.uIpV4 >> 16) & 0xFF,
								(tmpNode.IpBegin.uIpV4 >> 8) & 0xFF, tmpNode.IpBegin.uIpV4 & 0xFF);
						}
						else
						{
							print_ipv6(&tmpNode.IpBegin.uIpV6);
							printk(KERN_ERR " WantJoin: DNS [%u] Req... drop it\n", uUid);
						}
					}
				}
			}
#endif	
		}
#ifdef __DEBUG__
		if (uRet == NF_ACCEPT)
		{
		}
#endif 

		return uRet;
	}
}

inline void ClearUp(void)
{
#ifdef __MY_THREAD__
	if (g_pThdTask)
	{
		kthread_stop(g_pThdTask);
		g_pThdTask = NULL;
	}
	LOCK_SKB_LIST;
	while (g_skb_delay_list)
	{
		struct SSkbDelay *ptmp = g_skb_delay_list->pNext;

		kfree_skb(g_skb_delay_list->skb);
		FREE(g_skb_delay_list);
		g_skb_delay_list = ptmp;
	}
	g_skb_delay_list = NULL;
	UNLOCK_SKB_LIST;
#endif	

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,113)
	nf_unregister_hook(&nfho_in4);
	nf_unregister_hook(&nfho_out4);
	nf_unregister_hook(&nfho_in6);
	nf_unregister_hook(&nfho_out6);
#else
	nf_unregister_net_hook(&init_net, &nfho_in4);
	nf_unregister_net_hook(&init_net, &nfho_out4);
	nf_unregister_net_hook(&init_net, &nfho_in6);
	nf_unregister_net_hook(&init_net, &nfho_out6);
#endif 

	LOCK_IP_TREE;
	LOCK_UID_TREE;
	LOCK_HOST_TREE;
#ifdef __ALLOW_DIRECT_IP__
	LOCK_DNSIP_TREE;
#endif 
	release_data();
	g_nPid = 0;
#ifdef __ALLOW_DIRECT_IP__
	UNLOCK_DNSIP_TREE;
#endif 
	UNLOCK_IP_TREE;
	UNLOCK_UID_TREE;
	UNLOCK_HOST_TREE;

#ifdef __ALLOW_DIRECT_IP__
	LOCK_DNSIP_TREE;
	btree_free(g_dnsip_tree);
	g_dnsip_tree = NULL;
	UNLOCK_DNSIP_TREE;
#endif 

	LOCK_DCACHE_LIST;
	FreeDCacheList();
	g_dcache_list = NULL;
	UNLOCK_DCACHE_LIST;

	if (g_socket != NULL)
	{
		sock_release(g_socket->sk_socket);
		g_socket = NULL;
	}
	g_nInUse = 0;
}


static int __init fi_init_module(void)
{
#ifdef __MY_THREAD__
	int err = 0;
	spin_lock_init(&g_skb_delay_lock);
#endif

	g_nInUse = 0;
	g_nPid = 0;
	init_data();
	g_dcache_list = NULL;
#ifdef __ALLOW_DIRECT_IP__
	g_dnsip_tree = btree_new();
#endif

#ifdef SPIN_LOCK
	spin_lock_init(&g_ip_tree_lock);
	spin_lock_init(&g_uid_tree_lock);
	spin_lock_init(&g_host_tree_lock);
	spin_lock_init(&g_dcache_list_lock);
#ifdef __ALLOW_DIRECT_IP__
	spin_lock_init(&g_dnsip_tree_lock);
#endif
#else
	mutex_init(&g_ip_tree_lock);
	mutex_init(&g_uid_tree_lock);
	mutex_init(&g_host_tree_lock);
	mutex_init(&g_dcache_list_lock);
#ifdef __ALLOW_DIRECT_IP__
	mutex_init(&g_dnsip_tree_lock);
#endif
#endif



	nfho_in4.hook     = hook_func_in;
	nfho_in4.hooknum  = NF_INET_LOCAL_IN;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
	nfho_in4.dev = NULL;
#else
	nfho_in4.owner = THIS_MODULE;
#endif
	nfho_in4.pf       = PF_INET;
	nfho_in4.priority = NF_IP_PRI_FIRST;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,113)
	nf_register_hook(&nfho_in4);
#else
	nf_register_net_hook(&init_net, &nfho_in4);
#endif



	nfho_out4.hook     = hook_func_out;
	nfho_out4.hooknum  = NF_INET_POST_ROUTING;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
	nfho_in4.dev = NULL;
#else
	nfho_out4.owner = THIS_MODULE;
#endif	
	nfho_out4.pf       = PF_INET;
	nfho_out4.priority = NF_IP_PRI_FIRST;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,113)
	nf_register_hook(&nfho_out4);
#else
	nf_register_net_hook(&init_net, &nfho_out4);
#endif 

	MEMCPY(&nfho_in6, &nfho_in4, sizeof(nfho_in4));
	nfho_in6.pf       = PF_INET6;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,113)
	nf_register_hook(&nfho_in6);
#else
	nf_register_net_hook(&init_net, &nfho_in6);
#endif

	MEMCPY(&nfho_out6, &nfho_out4, sizeof(nfho_out4));
	nfho_out6.pf       = PF_INET6;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,113)
	nf_register_hook(&nfho_out6);
#else
	nf_register_net_hook(&init_net, &nfho_out6);
#endif


	nl_cfg.input = NL_receive_data;
	nl_cfg.groups = 1;
        nl_cfg.cb_mutex = NULL;

	g_socket = netlink_kernel_create(&init_net, NETLINK_PACKFILTER, &nl_cfg);
	if (!g_socket)
	{
		printk(KERN_ERR "netlink_kernel_create failed!\n");
	}

#ifdef __MY_THREAD__
	g_pThdTask = kthread_create(ThreadTask, NULL, "WJFirewallTask");
	if (IS_ERR(g_pThdTask))
	{
		err = PTR_ERR(g_pThdTask);
		printk(KERN_ERR "Unable to start kernel thread g_pThdTask, err is %d.\n", err);
		g_pThdTask = NULL;

		ClearUp();

		return err;
	}
	wake_up_process(g_pThdTask);
#endif

	printk(KERN_ERR "BeiJing KangShuo Information Tech. co. ltd\n");
	printk(KERN_ERR "KangShuo FireWall [%s] install into kernel!\n", VERSION_STRING);

	return 0;
}


static void __exit fi_cleanup_module(void)
{
	ClearUp();

	printk(KERN_ERR "WantJoin FireWall removed from kernel!\n");
}


module_init(fi_init_module);
module_exit(fi_cleanup_module);

MODULE_AUTHOR("ivan liu <ljxp@263.net>");
MODULE_DESCRIPTION("High performance firewall for LENOVO customing");
#ifdef __DEBUG__
MODULE_LICENSE("GPL V2");
#else
MODULE_LICENSE(" ");
#endif

