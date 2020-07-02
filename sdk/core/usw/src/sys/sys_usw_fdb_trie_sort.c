#include "ctc_const.h"
#include "ctc_chip.h"
#include "ctc_linklist.h"
#include "sys_usw_chip.h"
#include "sys_usw_fdb_sort.h"
#include "sys_usw_chip_global.h"


struct sys_fdb_sort_listnode_s
{
    ctc_slistnode_t           head;
    uint32                    entry_cnt;
    ctc_l2_addr_t*            pfdb_entrys;
};
typedef struct sys_fdb_sort_listnode_s sys_fdb_sort_listnode_t;

typedef struct
{
    const ctc_l2_addr_t** nodes;
    uint32                    sorted_count;
    ctc_slist_t*              fdb_list;

}sys_fdb_sort_t;

struct sys_fdb_sort_master_s
{
    sys_fdb_sort_t          sort[2];
    uint8                      sort_flag;/** 0:means dump use sort[0],1:means dump use sort[1], 2: means first dump*/
    uint8                      deinit_flag;

    sal_task_t*                p_thread;
    uint32                     dump_time;  /**dump time,set by user, granularity: 10s*/

    sal_mutex_t *              l2_mutex;
};
typedef struct sys_fdb_sort_master_s sys_fdb_sort_master_t;

struct sys_fdb_sort_init_s
{
    sys_fdb_sort_get_entry_cb_t       p_fdb_get_entry_cb;
    sys_fdb_sort_get_trie_sort_cb_t   p_trie_sort_en;
};
typedef struct sys_fdb_sort_init_s sys_fdb_sort_init_t;

sys_fdb_sort_master_t* p_usw_fdb_sort_master[CTC_MAX_LOCAL_CHIP_NUM] = {NULL};
sys_fdb_sort_init_t g_usw_sys_fdb_sort_init[CTC_MAX_LOCAL_CHIP_NUM];

#define CONTINUE_IF_FLAG_NOT_MATCH(query_flag, flag)            \
    switch (query_flag)                                         \
    {                                                           \
    case CTC_L2_FDB_ENTRY_STATIC:                               \
        if (!CTC_FLAG_ISSET(flag, CTC_L2_FLAG_IS_STATIC))       \
        {                                                       \
            continue;                                           \
        }                                                       \
        break;                                                  \
                                                                \
    case  CTC_L2_FDB_ENTRY_DYNAMIC:                             \
        if (CTC_FLAG_ISSET(flag, CTC_L2_FLAG_IS_STATIC))        \
        {                                                       \
            continue;                                           \
        }                                                       \
        break;                                                  \
                                                                \
    case CTC_L2_FDB_ENTRY_LOCAL_DYNAMIC:                        \
        if (CTC_FLAG_ISSET(flag, CTC_L2_FLAG_REMOVE_DYNAMIC)    \
            || CTC_FLAG_ISSET(flag, CTC_L2_FLAG_IS_STATIC))     \
        {                                                       \
            continue;                                           \
        }                                                       \
        break;                                                  \
                                                                \
    case CTC_L2_FDB_ENTRY_ALL:                                  \
    default:                                                    \
        break;                                                  \
    }


#define SYS_L2_FDB_INIT_CHECK()        \
    {                              \
        if (p_usw_fdb_sort_master[lchip] == NULL)    \
        { return CTC_E_NOT_INIT; } \
    }

#define FDB_SORT_LOCK \
    if (p_usw_fdb_sort_master[lchip]->l2_mutex) sal_mutex_lock(p_usw_fdb_sort_master[lchip]->l2_mutex)

#define FDB_SORT_UNLOCK \
    if (p_usw_fdb_sort_master[lchip]->l2_mutex) sal_mutex_unlock(p_usw_fdb_sort_master[lchip]->l2_mutex)

#define  SYS_L2_FDB_DUMP_USE_DB0 0
#define  SYS_L2_FDB_DUMP_USE_DB1 1
#define  SYS_L2_FDB_FIRST_DUMP 2
#define  MAX_FDB_ENTRY_NUM 128 * 1024
#define  SYS_L2_FDB_COMPARE_FDB_FID(fid)   (fid == nodes[idx]->fid)
#define  SYS_L2_FDB_COMPARE_FDB_PORT(gport)  (gport== nodes[idx]->gport)
#define  SYS_L2_FDB_COMPARE_FDB_MAC(mac)   (!sal_memcmp(mac, nodes[idx]->mac, CTC_ETH_ADDR_LEN))

#define  SYS_L2_FDB_GET_LIST(fdb_list)\
    do {\
    fdb_list = (p_usw_fdb_sort_master[lchip]->sort_flag == SYS_L2_FDB_DUMP_USE_DB0) ?\
                    p_usw_fdb_sort_master[lchip]->sort[0].fdb_list : p_usw_fdb_sort_master[lchip]->sort[1].fdb_list;\
    }while(0)





int32
sys_usw_fdb_sort_register_fdb_entry_cb(uint8 lchip, sys_fdb_sort_get_entry_cb_t cb)
{
    g_usw_sys_fdb_sort_init[lchip].p_fdb_get_entry_cb = cb;

    return CTC_E_NONE;
}
#if 0
int32
sys_usw_fdb_sort_register_mac_limit_cb(uint8 lchip, sys_fdb_sort_mac_limit_cb_t cb)
{
    g_usw_sys_fdb_sort_init[lchip].p_fdb_mac_limit_cb = cb;

    return CTC_E_NONE;
}
#endif
int32
sys_usw_fdb_sort_register_trie_sort_cb(uint8 lchip, sys_fdb_sort_get_trie_sort_cb_t cb)
{
    g_usw_sys_fdb_sort_init[lchip].p_trie_sort_en = cb;

    return CTC_E_NONE;
}

STATIC int32
_sys_usw_fdb_sort_get_fdb_entry_by_all(uint8 lchip, const ctc_l2_addr_t **nodes, uint32 max_count,
                                            uint8 query_flag, ctc_l2_fdb_query_rst_t* pr)
{
    uint32 idx = pr->start_index;
    uint32 rst_count = 0;
    uint32 pr_count = pr->buffer_len/sizeof(ctc_l2_addr_t);
    ctc_l2_addr_t* pr_temp = pr->buffer;

    pr->is_end = 0;
    pr->next_query_index = pr->start_index + pr_count;
    if (pr->next_query_index >= max_count)
    {
        pr->next_query_index = max_count;
        pr->is_end = 1;
    }
    for (;idx < max_count; idx++)
    {
        if (nodes[idx]->mac[0] & 0x1)
        {
            continue;
        }
        CONTINUE_IF_FLAG_NOT_MATCH(query_flag,nodes[idx]->flag);
        sal_memcpy(pr_temp, nodes[idx], sizeof(ctc_l2_addr_t));
        pr_temp++;
        rst_count++;
        if (rst_count == pr_count)
        {
            pr->next_query_index = idx + 1;
            break;
        }
    }

    return rst_count;

}

STATIC int32
_sys_usw_fdb_sort_get_mcast_entry_by_all(uint8 lchip, const ctc_l2_addr_t **nodes, uint32 max_count,
                                    uint8 query_flag, ctc_l2_fdb_query_rst_t* pr)
{

    uint32 idx = pr->start_index;
    uint32 pr_count = pr->buffer_len/sizeof(ctc_l2_addr_t);
    ctc_l2_addr_t* pr_temp = pr->buffer;
    uint32 rst_count = 0;

    pr->is_end = 0;
    pr->next_query_index = pr->start_index + pr_count;
    if (pr->next_query_index >= max_count)
    {
        pr->next_query_index = max_count;
        pr->is_end = 1;
    }

    for (;idx < max_count; idx++)
    {
        if (nodes[idx]->mac[0] & 0x1)
        {
            sal_memcpy(pr_temp, nodes[idx], sizeof(ctc_l2_addr_t));
            pr_temp++;
            rst_count++;
        }
        if (rst_count == pr_count)
        {
            pr->next_query_index = idx + 1;
            break;
        }
    }

    return rst_count;
}

STATIC int32
_sys_usw_fdb_sort_get_entry_by_fid(uint8 lchip, const ctc_l2_addr_t **nodes, uint32 max_count,
                                            uint8 query_flag, uint16 fid, ctc_l2_fdb_query_rst_t* pr)
{
    uint32 idx = pr->start_index;
    uint32 pr_count = pr->buffer_len/sizeof(ctc_l2_addr_t);
    ctc_l2_addr_t* pr_temp = pr->buffer;
    uint32 rst_count = 0;
    uint32 cnt = 0;

    pr->is_end = 0;
    pr->next_query_index = pr->start_index + pr_count;
    if (pr->next_query_index >= max_count)
    {
        pr->next_query_index = max_count;
        pr->is_end = 1;
    }

    for (;idx < max_count; idx++)
    {
        if (SYS_L2_FDB_COMPARE_FDB_FID(fid))
        {
            CONTINUE_IF_FLAG_NOT_MATCH(query_flag, nodes[idx]->flag);
            sal_memcpy(pr_temp, nodes[idx], sizeof(ctc_l2_addr_t));
            pr_temp++;
            rst_count++;
        }

        /* the entry is sorted by fid+mac, if cnt > rst_count, it will get all entry by fid*/
        if (rst_count)
        {
            cnt++;
            if (cnt > rst_count)
            {
                pr->is_end = 1;
                break;
            }
        }
        if (rst_count == pr_count)
        {
            pr->next_query_index = idx + 1;
            break;
        }
    }

    return rst_count;
}

STATIC int32
_sys_usw_fdb_sort_get_entry_by_mac(uint8 lchip, const ctc_l2_addr_t **nodes, uint32 max_count,
                                            uint8 query_flag, mac_addr_t mac, ctc_l2_fdb_query_rst_t* pr)
{
    uint32 idx = pr->start_index;
    uint32 pr_count = pr->buffer_len/sizeof(ctc_l2_addr_t);
    ctc_l2_addr_t* pr_temp = pr->buffer;
    uint32 rst_count = 0;

    pr->is_end = 0;
    pr->next_query_index = pr->start_index + pr_count;
    if (pr->next_query_index >= max_count)
    {
        pr->next_query_index = max_count;
        pr->is_end = 1;
    }

    for (;idx < max_count; idx++)
    {
        if (SYS_L2_FDB_COMPARE_FDB_MAC(mac))
        {
            CONTINUE_IF_FLAG_NOT_MATCH(query_flag,nodes[idx]->flag);
            sal_memcpy(pr_temp, nodes[idx], sizeof(ctc_l2_addr_t));
            pr_temp++;
            rst_count++;
        }
        if (rst_count == pr_count)
        {
            pr->next_query_index = idx + 1;
            break;
        }
    }

    return rst_count;
}

STATIC int32
_sys_usw_fdb_sort_get_entry_by_port(uint8 lchip, const ctc_l2_addr_t **nodes, uint32 max_count,
                                            uint8 query_flag, uint16 gport, uint8 used_logic_port,
                                            ctc_l2_fdb_query_rst_t* pr)
{
    uint32 idx = pr->start_index;
    uint32 pr_count = pr->buffer_len/sizeof(ctc_l2_addr_t);
    ctc_l2_addr_t* pr_temp = pr->buffer;
    uint32 rst_count = 0;

    pr->is_end = 0;
    pr->next_query_index = pr->start_index + pr_count;
    if (pr->next_query_index >= max_count)
    {
        pr->next_query_index = max_count;
        pr->is_end = 1;
    }

    for (;idx < max_count; idx++)
    {
        if (SYS_L2_FDB_COMPARE_FDB_PORT(gport))
        {
            CONTINUE_IF_FLAG_NOT_MATCH(query_flag,nodes[idx]->flag);
            if (used_logic_port && !(nodes[idx]->is_logic_port))
            {
                continue;
            }
            sal_memcpy(pr_temp, nodes[idx], sizeof(ctc_l2_addr_t));
            pr_temp++;
            rst_count++;
        }
        if (rst_count == pr_count)
        {
            pr->next_query_index = idx + 1;
            break;
        }
    }

    return rst_count;
}

STATIC int32
_sys_usw_fdb_sort_get_entry_by_port_vlan(uint8 lchip, const ctc_l2_addr_t **nodes, uint32 max_count,
                                                    uint8 query_flag,uint16 gport, uint16 fid, uint8 used_logic_port,
                                                    ctc_l2_fdb_query_rst_t* pr)
{
    uint32 idx = pr->start_index;
    uint32 pr_count = pr->buffer_len/sizeof(ctc_l2_addr_t);
    ctc_l2_addr_t* pr_temp = pr->buffer;
    uint32 rst_count = 0;

    pr->is_end = 0;
    pr->next_query_index = pr->start_index + pr_count;
    if (pr->next_query_index >= max_count)
    {
        pr->next_query_index = max_count;
        pr->is_end = 1;
    }

    for (;idx < max_count; idx++)
    {
        if (SYS_L2_FDB_COMPARE_FDB_FID(fid) && SYS_L2_FDB_COMPARE_FDB_PORT(gport))
        {
            CONTINUE_IF_FLAG_NOT_MATCH(query_flag,nodes[idx]->flag);
            if (used_logic_port && !(nodes[idx]->is_logic_port))
            {
                continue;
            }
            sal_memcpy(pr_temp, nodes[idx], sizeof(ctc_l2_addr_t));
            pr_temp++;
            rst_count++;
        }
        if (rst_count == pr_count)
        {
            pr->next_query_index = idx + 1;
            break;
        }
    }

    return rst_count;
}

STATIC int32
_sys_usw_fdb_sort_get_entry_by_mac_fid(uint8 lchip, const ctc_l2_addr_t **nodes, uint32 max_count,
                                                uint8 query_flag, mac_addr_t mac, uint16 fid,
                                                ctc_l2_fdb_query_rst_t* pr)
{
    uint32 idx = 0;
    uint32 rst_count = 0;
    char str1[20]= {0};
    char str2[20]= {0};

    sal_sprintf(str1, "%.4u%.2x%.2x%.2x%.2x%.2x%.2x",
        fid,
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
    for(idx = 0; idx < max_count; idx++)
    {
        sal_sprintf(str2, "%.4u%.2x%.2x%.2x%.2x%.2x%.2x",
        nodes[idx]->fid,
        nodes[idx]->mac[0],
        nodes[idx]->mac[1],
        nodes[idx]->mac[2],
        nodes[idx]->mac[3],
        nodes[idx]->mac[4],
        nodes[idx]->mac[5]);
        if (!sal_memcmp(str1, str2, sal_strlen(str1)))
        {
            CONTINUE_IF_FLAG_NOT_MATCH(query_flag,nodes[idx]->flag);
            sal_memcpy(pr->buffer, nodes[idx], sizeof(ctc_l2_addr_t));
            rst_count = 1;
            pr->is_end = 1;
            break;
        }
    }

    return rst_count;
}

STATIC int32
_sys_usw_fdb_sort_get_entry(uint8 lchip, ctc_l2_fdb_query_t* pq, ctc_l2_fdb_query_rst_t* pr)
{
    uint8 flag = 0;
    uint8 sort_flag = 0;
    uint8 is_vport = 0;
    uint32 count = 0;
    uint32 rq_count = pr->buffer_len/sizeof(ctc_l2_addr_t);
    const ctc_l2_addr_t **nodes = NULL;

    if ((pq->query_flag == CTC_L2_FDB_ENTRY_MCAST) && (pq->query_type != CTC_L2_FDB_ENTRY_OP_ALL))
    {
        return CTC_E_NOT_SUPPORT;
    }
    if (!pr->start_index)
    {
        FDB_SORT_LOCK;
    }

    sort_flag = (p_usw_fdb_sort_master[lchip]->sort_flag == SYS_L2_FDB_DUMP_USE_DB0);
    nodes = sort_flag ? p_usw_fdb_sort_master[lchip]->sort[1].nodes : p_usw_fdb_sort_master[lchip]->sort[0].nodes;
    count = sort_flag ? p_usw_fdb_sort_master[lchip]->sort[1].sorted_count :p_usw_fdb_sort_master[lchip]->sort[0].sorted_count;
    pq->count = 0;
    flag     = pq->query_flag;
    is_vport = pq->use_logic_port;

    switch (pq->query_type)
    {
    case CTC_L2_FDB_ENTRY_OP_BY_VID:
        pq->count = _sys_usw_fdb_sort_get_entry_by_fid(lchip,nodes, count, flag, pq->fid, pr);
        break;

    case CTC_L2_FDB_ENTRY_OP_BY_PORT:
        pq->count = _sys_usw_fdb_sort_get_entry_by_port(lchip, nodes, count, flag, pq->gport, is_vport, pr);
        break;

    case CTC_L2_FDB_ENTRY_OP_BY_PORT_VLAN:
        pq->count = _sys_usw_fdb_sort_get_entry_by_port_vlan(lchip, nodes, count, flag, pq->gport, pq->fid, is_vport, pr);
        break;

    case CTC_L2_FDB_ENTRY_OP_BY_MAC_VLAN:
        pq->count = _sys_usw_fdb_sort_get_entry_by_mac_fid(lchip, nodes, count, flag, pq->mac, pq->fid, pr);
        break;

    case CTC_L2_FDB_ENTRY_OP_BY_MAC:
        pq->count = _sys_usw_fdb_sort_get_entry_by_mac(lchip, nodes, count, flag, pq->mac, pr);
        break;

    case CTC_L2_FDB_ENTRY_OP_ALL:
        if (pq->query_flag == CTC_L2_FDB_ENTRY_MCAST)
        {
            pq->count = _sys_usw_fdb_sort_get_mcast_entry_by_all(lchip, nodes, count, flag, pr);
        }
        else
        {
            pq->count = _sys_usw_fdb_sort_get_fdb_entry_by_all(lchip, nodes, count, flag, pr);
        }

        break;

    default:
        pq->count = 0;
        break;
    }

    /*cannot get enough entry means query end */
    if (rq_count > pq->count)
    {
        pr->is_end = TRUE;
    }
    if (pr->is_end)
    {
        FDB_SORT_UNLOCK;
    }
    return CTC_E_NONE;
}

STATIC int32
sys_usw_fdb_compare_data(const void *a, const void *b)
{
    const ctc_hash_bucket_t *const *nodes1 = a;
    const ctc_hash_bucket_t *const *nodes2 = b;
    ctc_l2_addr_t* p_l2addr_1 = (ctc_l2_addr_t*)(*nodes1);
    ctc_l2_addr_t* p_l2addr_2 = (ctc_l2_addr_t*)(*nodes2);
    char    str1[20] = {0};
    char    str2[20] = {0};

    sal_sprintf(str1, "%.4u%.2x%.2x%.2x%.2x%.2x%.2x",
                    p_l2addr_1->fid,
                    p_l2addr_1->mac[0],
                    p_l2addr_1->mac[1],
                    p_l2addr_1->mac[2],
                    p_l2addr_1->mac[3],
                    p_l2addr_1->mac[4],
                    p_l2addr_1->mac[5]);

    sal_sprintf(str2, "%.4u%.2x%.2x%.2x%.2x%.2x%.2x",
                    p_l2addr_2->fid,
                    p_l2addr_2->mac[0],
                    p_l2addr_2->mac[1],
                    p_l2addr_2->mac[2],
                    p_l2addr_2->mac[3],
                    p_l2addr_2->mac[4],
                    p_l2addr_2->mac[5]);

    return sal_strcmp(str1, str2);

    
}

const ctc_l2_addr_t **
sys_usw_fdb_entry_sort(uint8 lchip, uint8 index)
{
    const ctc_l2_addr_t** nodes;
    uint32 n = 0;
    uint32 loop = 0;
    uint32 i = 0;
    ctc_slist_t*           fdb_list = NULL;
    ctc_slistnode_t*       node = NULL;
    ctc_l2_addr_t*         p_l2addr = NULL;
    sys_fdb_sort_listnode_t* fdb_node = NULL;
    
    SYS_L2_FDB_GET_LIST(fdb_list);
    CTC_SLIST_LOOP(fdb_list, node)
    {
        fdb_node = (sys_fdb_sort_listnode_t*)node;
        n += fdb_node->entry_cnt;
    }
    if (n == 0) 
    {
        return NULL;
    }
    nodes = mem_malloc(MEM_FDB_MODULE, n * sizeof (*nodes));
    if(NULL == nodes)
    {
        return NULL;
    }
    CTC_SLIST_LOOP(fdb_list, node)
    {
        fdb_node = (sys_fdb_sort_listnode_t*)node;
        p_l2addr = fdb_node->pfdb_entrys;
        for (loop = 0; loop < fdb_node->entry_cnt; loop++)
        {
            nodes[i++] = p_l2addr+loop;
        }
    }
    sal_qsort(nodes, n, sizeof(*nodes), sys_usw_fdb_compare_data);
     p_usw_fdb_sort_master[lchip]->sort[index].sorted_count = n;
    return nodes;
}

STATIC int32
_sys_usw_fdb_sort_dump_sort(uint8 lchip)
{

    uint8 index = 0;
    index = (p_usw_fdb_sort_master[lchip]->sort_flag == SYS_L2_FDB_DUMP_USE_DB0) ? 0 : 1;

    p_usw_fdb_sort_master[lchip]->sort[index].nodes = sys_usw_fdb_entry_sort(lchip, index);

    return CTC_E_NONE;
}

STATIC int32
_sys_usw_fdb_sort_dump_sort_change_flag(uint8 lchip)
{
    uint8 flag = p_usw_fdb_sort_master[lchip]->sort_flag;
    FDB_SORT_LOCK;
    p_usw_fdb_sort_master[lchip]->sort_flag = flag ? SYS_L2_FDB_DUMP_USE_DB0 : SYS_L2_FDB_DUMP_USE_DB1;
    FDB_SORT_UNLOCK;

    return CTC_E_NONE;
}

STATIC int32
_sys_usw_fdb_sort_dump_sort_free(uint8 lchip)
{

    uint8   index = 0;

    index = (p_usw_fdb_sort_master[lchip]->sort_flag == SYS_L2_FDB_DUMP_USE_DB0) ? 0 : 1;

    if (p_usw_fdb_sort_master[lchip]->sort[index].nodes != NULL)
    {
        mem_free(p_usw_fdb_sort_master[lchip]->sort[index].nodes);
        p_usw_fdb_sort_master[lchip]->sort[index].nodes = NULL;
    }

    return CTC_E_NONE;
}

STATIC int32
_sys_usw_fdb_sort_add_list_node(uint8 lchip, ctc_l2_addr_t* l2_addr, uint32 entry_cnt)
{
    sys_fdb_sort_listnode_t* fdb_node;
    ctc_slist_t*           fdb_list = NULL;

    if (entry_cnt == 0)
    {
        return CTC_E_NONE;
    }
    SYS_L2_FDB_GET_LIST(fdb_list);
    fdb_node = mem_malloc(MEM_FDB_MODULE, sizeof(sys_fdb_sort_listnode_t));
    if (fdb_node == NULL)
    {
        return CTC_E_NO_MEMORY;
    }
    fdb_node->pfdb_entrys = mem_malloc(MEM_FDB_MODULE, entry_cnt * sizeof(ctc_l2_addr_t));
    if (fdb_node->pfdb_entrys == NULL)
    {
        mem_free(fdb_node);
        return CTC_E_NO_MEMORY;
    }
    sal_memcpy(fdb_node->pfdb_entrys, l2_addr, entry_cnt * sizeof(ctc_l2_addr_t));
    fdb_node->entry_cnt = entry_cnt;
    ctc_slist_add_tail(fdb_list, &(fdb_node->head));

    return CTC_E_NONE;
}

STATIC int32
_sys_usw_fdb_sort_free_list_node(uint8 lchip, ctc_slist_t* fdb_list)
{
    sys_fdb_sort_listnode_t* fdb_node = NULL;
    ctc_slistnode_t*       next_node = NULL;
    ctc_slistnode_t*       node = NULL;

    CTC_SLIST_LOOP_DEL(fdb_list, node, next_node)
    {
        fdb_node = (sys_fdb_sort_listnode_t*)node;
        if (fdb_node->pfdb_entrys)
        {
            mem_free(fdb_node->pfdb_entrys);
        }
        ctc_slist_delete_node(fdb_list, node);
        mem_free(node);
    }
    return CTC_E_NONE;
}

void
sys_usw_fdb_sort_dump_fdb_handler(void* arg)
{
    uint8                   lchip = (uintptr)arg;
    uint32                  index = 0;
    ctc_l2_fdb_query_rst_t  query_rst;
    int32                   ret = 0;
    uint16                  entry_cnt = 1024;
    ctc_l2_fdb_query_t      pQuery;
    ctc_slist_t*           fdb_list = NULL;

    while (1)
    {
        if((ret = sys_usw_chip_check_active(lchip)) < 0 ||NULL == p_usw_fdb_sort_master[lchip])
        {
            goto error_0;
        }

        while (g_usw_sys_fdb_sort_init[lchip].p_trie_sort_en && (!g_usw_sys_fdb_sort_init[lchip].p_trie_sort_en(lchip)))
        {
            sal_task_sleep(1000);
        }

        sal_memset(&pQuery, 0, sizeof(ctc_l2_fdb_query_t));
        sal_memset(&query_rst, 0, sizeof(ctc_l2_fdb_query_rst_t));

        query_rst.buffer_len = sizeof(ctc_l2_addr_t) * entry_cnt;
        query_rst.buffer = mem_malloc(MEM_FDB_MODULE, query_rst.buffer_len);
        if (NULL == query_rst.buffer)
        {
            sal_task_sleep(1000);
            continue;
        }
        sal_memset(query_rst.buffer, 0, query_rst.buffer_len);
        SYS_L2_FDB_GET_LIST(fdb_list);
        do
        {
            query_rst.start_index = query_rst.next_query_index;

            if (g_usw_sys_fdb_sort_init[lchip].p_fdb_get_entry_cb)
            {
                ret = g_usw_sys_fdb_sort_init[lchip].p_fdb_get_entry_cb(lchip, &pQuery, &query_rst);
                if (ret < 0)
                {
                    sal_task_sleep(1000);
                    goto error_0;
                }

            }

            ret = _sys_usw_fdb_sort_add_list_node(lchip, query_rst.buffer, pQuery.count);
            if (ret < 0)
            {
                sal_task_sleep(1000);
                goto error_0;
            }

            sal_memset(query_rst.buffer, 0, sizeof(ctc_l2_addr_t) * pQuery.count);
            if (p_usw_fdb_sort_master[lchip]->deinit_flag)
            {
                goto error_1;
            }
            sal_task_sleep(100);
            if((ret = sys_usw_chip_check_active(lchip)) < 0)
            {
                goto error_0;
            }
        }
        while (query_rst.is_end == 0);

        if(((ret = sys_usw_chip_check_active(lchip)) < 0) || p_usw_fdb_sort_master[lchip]->deinit_flag)
        {
            goto error_1;
        }
        _sys_usw_fdb_sort_dump_sort(lchip);
        _sys_usw_fdb_sort_dump_sort_change_flag(lchip);

        error_1:
            SYS_L2_FDB_GET_LIST(fdb_list);
            _sys_usw_fdb_sort_free_list_node(lchip, fdb_list);
            _sys_usw_fdb_sort_dump_sort_free(lchip);
        error_0:
            if (query_rst.buffer)
            {
                mem_free(query_rst.buffer);
            }

        for (index = 0;
            index < ((p_usw_fdb_sort_master[lchip] == NULL)?
            0:((p_usw_fdb_sort_master[lchip]->dump_time < 10)?
            1:(p_usw_fdb_sort_master[lchip]->dump_time/10))); index++)
        {
            sal_task_sleep(10000); /*10s*/
        }

        continue;
    }
    return;

}
#if 0
int32
sys_usw_fdb_sort_set_mac_limit_en(uint8 lchip, uint8 limit_en, uint8 is_remove)
{
    SYS_L2_FDB_INIT_CHECK();
    p_usw_fdb_sort_master[lchip]->mac_limt_en = limit_en;
    p_usw_fdb_sort_master[lchip]->mac_limit_remove_fdb_en = is_remove;

    return CTC_E_NONE;
}
#endif
/**uint: s, dump_time = (time < 10)? 10: (time / 10 * 10)*/
int32
sys_usw_fdb_sort_set_dump_task_time(uint8 lchip, uint32 time)
{
    SYS_L2_FDB_INIT_CHECK();

    p_usw_fdb_sort_master[lchip]->dump_time = time;


    return CTC_E_NONE;
}

int32
sys_usw_fdb_sort_get_fdb_entry(uint8 lchip, ctc_l2_fdb_query_t* pQuery, ctc_l2_fdb_query_rst_t* query_rst)
{
    SYS_L2_FDB_INIT_CHECK();
    CTC_PTR_VALID_CHECK(pQuery);
    CTC_PTR_VALID_CHECK(query_rst);

    CTC_ERROR_RETURN(_sys_usw_fdb_sort_get_entry(lchip, pQuery, query_rst));

    return CTC_E_NONE;
}

STATIC int32
sys_usw_fdb_sort_dump_thread(uint8 lchip)
{

    int32 ret = 0;
    uintptr chip_id = lchip;
    uint64 cpu_mask = 0;


    cpu_mask = sys_usw_chip_get_affinity(lchip, 0);
    ret = sys_usw_task_create(lchip, &(p_usw_fdb_sort_master[lchip]->p_thread), "dumpfdb",
                          SAL_DEF_TASK_STACK_SIZE, SAL_TASK_PRIO_DEF,SAL_TASK_TYPE_FDB,cpu_mask, sys_usw_fdb_sort_dump_fdb_handler, (void*)chip_id);
    if (ret < 0)
    {
        return CTC_E_NOT_INIT;
    }

    return CTC_E_NONE;
}

int32
sys_usw_fdb_sort_init(uint8 lchip)
{

    int32 ret = 0;
    if (p_usw_fdb_sort_master[lchip]) /*already init */
    {
        return CTC_E_NONE;
    }

    p_usw_fdb_sort_master[lchip] = mem_malloc(MEM_FDB_MODULE, sizeof(sys_fdb_sort_master_t));
    if (NULL == p_usw_fdb_sort_master[lchip])
    {
        return CTC_E_NO_MEMORY;
    }

    ret = sal_mutex_create(&(p_usw_fdb_sort_master[lchip]->l2_mutex));
    if (ret || !(p_usw_fdb_sort_master[lchip]->l2_mutex))
    {
        return CTC_E_NO_RESOURCE;
    }
    sal_memset(&p_usw_fdb_sort_master[lchip]->sort[0], 0, sizeof(sys_fdb_sort_t));
    sal_memset(&p_usw_fdb_sort_master[lchip]->sort[1], 0, sizeof(sys_fdb_sort_t));

    p_usw_fdb_sort_master[lchip]->sort[0].fdb_list = ctc_slist_new();
    if (p_usw_fdb_sort_master[lchip]->sort[0].fdb_list == NULL)
    {
        return CTC_E_NO_MEMORY;
    }
    p_usw_fdb_sort_master[lchip]->sort[1].fdb_list = ctc_slist_new();
    if (p_usw_fdb_sort_master[lchip]->sort[1].fdb_list == NULL)
    {
        return CTC_E_NO_MEMORY;
    }

    p_usw_fdb_sort_master[lchip]->sort_flag = SYS_L2_FDB_FIRST_DUMP;
    p_usw_fdb_sort_master[lchip]->dump_time = 300;  /* 5 min*/

    /*p_usw_fdb_sort_master[lchip]->mac_limit_remove_fdb_en = 0;*/
    p_usw_fdb_sort_master[lchip]->deinit_flag = 0;
    CTC_ERROR_RETURN(sys_usw_fdb_sort_dump_thread(lchip));

    return CTC_E_NONE;
}

int32
sys_usw_fdb_sort_deinit(uint8 lchip)
{
    LCHIP_CHECK(lchip);
    if (NULL == p_usw_fdb_sort_master[lchip])
    {
        return CTC_E_NONE;
    }
    p_usw_fdb_sort_master[lchip]->deinit_flag = 1;
    sal_task_sleep(10);
    if(p_usw_fdb_sort_master[lchip]->p_thread)
    {
        #ifdef SDK_IN_USERMODE
        /*special process for thread may be sleep 1 minute or more*/
        p_usw_fdb_sort_master[lchip]->p_thread->done = 1;
        sal_task_sleep(10);
        #endif
        sal_task_destroy(p_usw_fdb_sort_master[lchip]->p_thread);
    }
    _sys_usw_fdb_sort_free_list_node(lchip, p_usw_fdb_sort_master[lchip]->sort[0].fdb_list);
    _sys_usw_fdb_sort_free_list_node(lchip, p_usw_fdb_sort_master[lchip]->sort[1].fdb_list);

    if (p_usw_fdb_sort_master[lchip]->sort[0].fdb_list)
    {
        ctc_slist_free(p_usw_fdb_sort_master[lchip]->sort[0].fdb_list);
    }
    if (p_usw_fdb_sort_master[lchip]->sort[1].fdb_list)
    {
        ctc_slist_free(p_usw_fdb_sort_master[lchip]->sort[1].fdb_list);
    }
    if (p_usw_fdb_sort_master[lchip]->sort[0].nodes != NULL)
    {
        mem_free(p_usw_fdb_sort_master[lchip]->sort[0].nodes);
    }
    if (p_usw_fdb_sort_master[lchip]->sort[1].nodes != NULL)
    {
        mem_free(p_usw_fdb_sort_master[lchip]->sort[1].nodes);
    }

    sal_mutex_destroy(p_usw_fdb_sort_master[lchip]->l2_mutex);
    mem_free(p_usw_fdb_sort_master[lchip]);

    return CTC_E_NONE;
}


