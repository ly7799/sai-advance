 /**
 @file memmgr.h  

 @author  Copyright (C) 2020 Centec Networks Inc.  All rights reserved.

 @date 2020-03-05

 @version v2.0

   This file contains memory debug manager
*/

#ifndef _SAL_MEMMGR_H_
#define _SAL_MEMMGR_H_

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 *
* Header Files
*
****************************************************************************/
#include "sal_mutex.h"


#define    MEM_MAX_NAME_SIZE    20
#define MAX_MEM_FILE_SZ 20



#define MEM_HEADER_MAGIC_NUMBER 0xA1D538
#define MEM_TAIL_MAGIC_NUMBER   0xA1D538FB

#define MEMMGR_LOCK(ptr) \
    if (ptr != NULL) sal_mutex_lock(ptr)

#define MEMMGR_UNLOCK(ptr) \
    if (ptr != NULL) sal_mutex_unlock(ptr)

/****************************************************************
*
* Defines and Macros
*
****************************************************************/
#ifdef _SAL_LINUX_UM
#ifndef MEMMGR
#define MEMMGR
//#define MEM_RUNTIME_CHECK
#endif
#else
#undef MEMMGR
#endif

#ifdef MEMMGR
#define mem_malloc(type, size) mem_mgr_malloc(size, type, __FILE__, __LINE__)
#define mem_free(ptr)   \
    { \
      if(ptr) mem_mgr_free(ptr, __FILE__,  __LINE__); \
        ptr = NULL; /*must set to NULL*/ \
    }
#define mem_realloc(type, ptr, size) mem_mgr_realloc(type, ptr, size, __FILE__,  __LINE__)
#else
#define mem_malloc(type, size)  (size == 0 ? NULL : sal_malloc(size))
#define mem_free(ptr)   \
    { \
        if(ptr) sal_free(ptr); \
        ptr = NULL;/*must set to NULL*/ \
    }
#define mem_realloc(type, ptr, size) sal_realloc(ptr, size)
#endif

#define CTC_MEMMNGR_DBG_INFO(FMT, ...)                          \
    { \
        CTC_DEBUG_OUT_INFO(memmngr, memmngr, MEMMNGR_CTC, FMT, ##__VA_ARGS__); \
    }

/* bucket array indices */

#ifdef MEM_RUNTIME_CHECK

/*
 *  Mtype memory debug information for tracing back each memory allocation
 *  or freeing to a filename and line number at which it is initiated. This
 *  information is appended at the end of each memblock. This feature is
 *  used only for internal builds.
 */
struct mem_block_trailer
{
    uint32  guard;
    uint16 line_number;
    uint8 rsv[2];
    char filename[MAX_MEM_FILE_SZ];
};
#endif

/*
 *  Memory manager table for holding stats for each mtype.
 *   - allocated table is based on mtype
 *   - free tbale is based on buckets having fixed size blocks.
 */
struct mem_table
{
    void* list;           /* points to mtype memory list */
    uint32    ext_size;       /* total size of memory allocated or free */
    uint32    req_size;   /* total user requested size */
    uint32    count;      /* number of blocks allocated or free */   
   sal_mutex_t* p_mem_mutex;
};
typedef struct mem_table mem_table_t;

/*
 *  This header precedes each user buffer.  This header size must be multiple of
 *  16 bytes to avoid alignment exceptions.
 */
struct mem_block_header
{
   uint32    guard:24;             /*  overlap check guard */
   uint32    mid:8;             /* mtype idx */
   uint32    req_size;         /* user requested size */
#ifdef MEM_RUNTIME_CHECK
   struct mem_block_header* next;   /* linked list pointer */
   struct mem_block_header* prev;
 #endif
};

enum mem_mgr_type
{
    MEM_SYSTEM_MODULE,
    MEM_FDB_MODULE,
    MEM_MPLS_MODULE,
    MEM_QUEUE_MODULE,
    MEM_IPUC_MODULE,
    MEM_IPMC_MODULE,
    MEM_RPF_MODULE,
    MEM_NEXTHOP_MODULE,
    MEM_AVL_MODULE,
    MEM_STATS_MODULE,
    MEM_L3IF_MODULE,
    MEM_PORT_MODULE,
    MEM_VLAN_MODULE,
    MEM_APS_MODULE,
    MEM_VPN_MODULE,
    MEM_SORT_KEY_MODULE,
    MEM_LINKAGG_MODULE,
    MEM_USRID_MODULE,
    MEM_SCL_MODULE,
    MEM_ACL_MODULE,
    MEM_LINKLIST_MODULE,
    MEM_CLI_MODULE,
    MEM_VECTOR_MODULE,
    MEM_HASH_MODULE,
    MEM_SPOOL_MODULE,
    MEM_OPF_MODULE,
    MEM_SAL_MODULE,
    MEM_OAM_MODULE,
    MEM_PTP_MODULE,
    MEM_FTM_MODULE,
    MEM_STK_MODULE,
    MEM_LIBCTCCLI_MODULE,
    MEM_DMA_MODULE,
    MEM_STMCTL_MODULE,
    MEM_L3_HASH_MODULE,
    MEM_FPA_MODULE,
    MEM_MIRROR_MODULE,
    MEM_SYNC_ETHER_MODULE,
    MEM_APP_MODULE,
    MEM_MONITOR_MODULE,
    MEM_OVERLAY_TUNNEL_MODULE,
    MEM_EFD_MODULE,
    MEM_IPFIX_MODULE,
    MEM_TRILL_MODULE,
    MEM_FCOE_MODULE,
    MEM_WLAN_MODULE,
    MEM_DOT1AE_MODULE,
    MEM_DIAG_MODULE,
    MEM_TYPE_MAX

};

struct bucket_info_s
{
    uint32 block_size;
    uint32 block_count;
    uint32 used_count;
    int8 name[MEM_MAX_NAME_SIZE];
};
typedef struct bucket_info_s bucket_info_t;

struct mem_type_name_s
{
    enum mem_mgr_type mtype;
    int8 name[40];
};
typedef struct mem_type_name_s mem_type_name_t;
int32 mem_mgr_init(void);
int32 mem_mgr_deinit(void);
void mem_mgr_free(void* ptr, char* file, int line);
void* mem_mgr_malloc(uint32 size, enum mem_mgr_type mtype, char* filename, uint16 line);
void* mem_mgr_realloc(uint32 mtype, void* ptr, uint32 size, char* filename, uint16 line);
void mem_mgr_check_mtype(enum mem_mgr_type mtype,bool is_show_detail);
int32 mem_mgr_overlap_chk_en(uint8 enable,uint32 interval );
int32 mem_mgr_get_mtype_info(enum mem_mgr_type mtype, mem_table_t* p_mtype_table_info, int8** m_name);
int32 mem_mgr_ptr_check(void* ptr);

#ifdef __cplusplus
}
#endif

#endif

