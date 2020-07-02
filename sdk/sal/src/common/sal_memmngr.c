/****************************************************************************
 * memmgr.h      memory debug manager defines and macros.
 *
 * Copyright (C) 2020 Centec Networks Inc.  All rights reserved.
 *
 ****************************************************************************/

/****************************************************************************
 *
* Header Files
*
****************************************************************************/
#include "sal.h"

uint8 g_sal_ext_size = 0;
uint8 g_sal_ext_hdr_size = 0;
uint8 g_sal_overlap_chk = 0;

sal_timer_t* p_check_timer = NULL;
static struct mem_table  mtype_table[MEM_TYPE_MAX];


mem_type_name_t mtype_name[MEM_TYPE_MAX] =
{
    {MEM_SYSTEM_MODULE, "MEM_SYSTEM_MODULE"},

    {MEM_FDB_MODULE, "MEM_FDB_MODULE"},

    {MEM_MPLS_MODULE, "MEM_MPLS_MODULE"},

    {MEM_QUEUE_MODULE, "MEM_QUEUE_MODULE"},

    {MEM_IPUC_MODULE, "MEM_IPUC_MODULE"},

    {MEM_IPMC_MODULE, "MEM_IPMC_MODULE"},

    {MEM_RPF_MODULE, "MEM_RPF_MODULE"},

    {MEM_NEXTHOP_MODULE, "MEM_NEXTHOP_MODULE"},

    {MEM_AVL_MODULE, "MEM_AVL_MODULE"},

    {MEM_STATS_MODULE, "MEM_STATS_MODULE"},

    {MEM_L3IF_MODULE, "MEM_L3IF_MODULE"},

    {MEM_PORT_MODULE, "MEM_PORT_MODULE"},

    {MEM_VLAN_MODULE, "MEM_VLAN_MODULE"},

    {MEM_APS_MODULE, "MEM_APS_MODULE"},

    {MEM_VPN_MODULE, "MEM_VPN_MODULE"},

    {MEM_SORT_KEY_MODULE, "MEM_SORT_KEY_MODULE"},

    {MEM_LINKAGG_MODULE, "MEM_LINKAGG_MODULE"},

    {MEM_USRID_MODULE, "MEM_USRID_MODULE"},

    {MEM_SCL_MODULE, "MEM_SCL_MODULE"},

    {MEM_ACL_MODULE, "MEM_ACL_MODULE"},

    {MEM_LINKLIST_MODULE, "MEM_LINKLIST_MODULE"},

    {MEM_CLI_MODULE, "MEM_CLI_MODULE"},

    {MEM_VECTOR_MODULE, "MEM_VECTOR_MODULE"},

    {MEM_HASH_MODULE, "MEM_HASH_MODULE"},

    {MEM_SPOOL_MODULE, "MEM_SPOOL_MODULE"},

    {MEM_OPF_MODULE, "MEM_OPF_MODULE"},

    {MEM_SAL_MODULE, "MEM_SAL_MODULE"},

    {MEM_OAM_MODULE, "MEM_OAM_MODULE"},

    {MEM_PTP_MODULE, "MEM_PTP_MODULE"},

    {MEM_FTM_MODULE, "MEM_FTM_MODULE"},

    {MEM_STK_MODULE, "MEM_STK_MODULE"},

    {MEM_LIBCTCCLI_MODULE, "MEM_LIBCTCCLI_MODULE"},

    {MEM_DMA_MODULE, "MEM_DMA_MODULE"},

    {MEM_STMCTL_MODULE, "MEM_STMCTL_MODULE"},

    {MEM_L3_HASH_MODULE, "MEM_L3_HASH_MODULE"},

    {MEM_FPA_MODULE, "MEM_FPA_MODULE"},

    {MEM_MIRROR_MODULE, "MEM_MIRROR_MODULE"},

    {MEM_SYNC_ETHER_MODULE, "MEM_SYNCE_MODULE"},

    {MEM_APP_MODULE, "MEM_APP_MODULE"},
    {MEM_MONITOR_MODULE, "MEM_MONITOR_MODULE "},

    {MEM_OVERLAY_TUNNEL_MODULE, "MEM_OVERLAY_MODULE "},

    {MEM_EFD_MODULE, "MEM_EFD_MODULE "},

    {MEM_IPFIX_MODULE, "MEM_IPFIX_MODULE"},

    {MEM_TRILL_MODULE, "MEM_TRILL_MODULE"},

    {MEM_FCOE_MODULE, "MEM_FCOE_MODULE"},

    {MEM_WLAN_MODULE, "MEM_WLAN_MODULE"},

    {MEM_DOT1AE_MODULE, "MEM_DOT1AE_MODULE"},

    {MEM_DIAG_MODULE, "MEM_DIAG_MODULE"},
};

/******************************************************************************
 * Name        : mem_get_mtype_name(enum mem_mgr_type mtype)
 * Purpose     : get mem type name
 * Input         : mtype
 * Output       : bucket name
 * Return       : N/A
 * Note          : N/A
 *****************************************************************************/
STATIC int8*
mem_get_mtype_name(enum mem_mgr_type mtype)
{
    int32 incr;

    for (incr = 0; incr < MEM_TYPE_MAX; incr++)
    {
        if (mtype == mtype_name[incr].mtype)
        {
            return mtype_name[incr].name;
        }
    }

    return NULL;
}
#ifdef MEM_RUNTIME_CHECK
/******************************************************************************
 * Name        : mem_mgr_set_memblock_trailer (struct mem_block_header *hdr, uint32 index, int8 *filename, uint32 line)
 * Purpose     : set memory block trailer
 * Input         : struct mem_block_header *hdr, uint32 index, int8 *filename, uint32 line
 * Output       : N/A
 * Return       : N/A
 * Note          : N/A
 *****************************************************************************/
void
mem_mgr_set_memblock_tail(struct mem_block_header* hdr, char* filename, uint16 line)
{

    struct mem_block_trailer* mtlr;
    mtlr = (struct mem_block_trailer*) ((int8*)hdr + g_sal_ext_hdr_size+ hdr->req_size);
    /* get filename and its length */
    if (filename != NULL)
    {
        uint32 len = sal_strlen((char*)filename);

        /* truncate filename if it exceeds MAX file size */
        if (len >= MAX_MEM_FILE_SZ)
        {
            len = len - MAX_MEM_FILE_SZ + 1;
            filename = &filename[len];
            len = MAX_MEM_FILE_SZ - 1;
        }
        sal_strcpy((char*)mtlr->filename, (char*)filename);
        mtlr->filename[len] = '\0';
    }
    mtlr->line_number = line;
    /* set up post guard area */
    mtlr->guard = MEM_TAIL_MAGIC_NUMBER;


}

/******************************************************************************
 * Name        : mem_mgr_buffer_check (void *ptr)
 * Purpose     : check if buffer has error.
 * Input         : void *ptr
 * Output       : N/A
 * Return       : N/A
 * Note          : N/A
 *****************************************************************************/
 int32
mem_mgr_ptr_check(void* ptr)
{
    struct mem_block_header* mhdr;
    struct mem_block_trailer* mtlr;
    mhdr = (struct mem_block_header*)((int8*)ptr - g_sal_ext_hdr_size);
    mtlr = (struct mem_block_trailer*)((int8*)ptr +mhdr->req_size);

    /* see if any of the pre guard area is corrupted */
    if (mhdr->guard != MEM_HEADER_MAGIC_NUMBER)
    {
        SAL_LOG_ERROR("MEMMGR::re guard check failed  - hdr %p - mtype %d - file %s - line %d\n",
                     mhdr, mhdr->mid, mtlr->filename, mtlr->line_number);
       return -1;
    }

    /* get user buffer size */
    if (mtlr->guard != MEM_TAIL_MAGIC_NUMBER)
    {
        SAL_LOG_ERROR("MEMMGR::Post guard check failed  - hdr %p - mtype %d - file %s - line %d\n",
       mhdr, mhdr->mid, mtlr->filename, mtlr->line_number);
     return -1;
    }

return 0;
}

#endif
/******************************************************************************
 * Name        : mem_mgr_check_mtype(enum mem_mgr_type mtype, bool is_show_detail)
 * Purpose     : check allocated block, show information
 * Input         : memory type, is_show_detail
 * Output       : N/A
 * Return       : N/A
 * Note          : N/A
 *****************************************************************************/
void
mem_mgr_check_mtype(enum mem_mgr_type mtype,bool is_show_detail)
{
    int8* m_name;

 #ifdef MEM_RUNTIME_CHECK
    struct mem_block_header* mhdr;
    struct mem_block_trailer* mtlr;
    uint32 count=0, invalid_mem = 0;
    uint8 fisrt_valid = 1;
    char* p_time_str = NULL;
    sal_time_t tm;

#endif

    if (mtype < 0 || mtype >= MEM_TYPE_MAX)
    {
        SAL_LOG_INFO("\nMEMMGR::Unknown memory type specified (%d)\n", mtype);
        return;
    }

    m_name = mem_get_mtype_name(mtype);
    if (NULL == m_name)
    {
        return;
    }
#ifdef MEM_RUNTIME_CHECK
    MEMMGR_LOCK(mtype_table[mtype].p_mem_mutex);
    mhdr = (struct mem_block_header*)mtype_table[mtype].list;
    count=0;
    invalid_mem = 0;

     while (count < mtype_table[mtype].count && mhdr != NULL )
    {
        mtlr = (struct mem_block_trailer*)( (int8*)mhdr+ g_sal_ext_hdr_size + mhdr->req_size);

        if(mhdr->guard !=MEM_HEADER_MAGIC_NUMBER || mtlr->guard !=MEM_TAIL_MAGIC_NUMBER)
        {
        if(fisrt_valid)
        {
           sal_time(&tm);
           p_time_str = sal_ctime(&tm);
           SAL_LOG_ERROR("Memory overlap  in %s ,time:%s\n",m_name,p_time_str);

         }
          invalid_mem++;
          fisrt_valid =0;
          SAL_LOG_ERROR("block:%u, ptr:%p req_size:%u, head guard:0x%6x, tail guard:0x%8x, filename:%s, line_no:%d \n",
              count,
              ( (int8*)mhdr + sizeof(struct mem_block_header)),
               mhdr->req_size,mhdr->guard,mtlr->guard,mtlr->filename, mtlr->line_number);
       }

       mhdr = mhdr->next;
       count++;
    }
     MEMMGR_UNLOCK(mtype_table[mtype].p_mem_mutex);
   if(invalid_mem)
    {
       SAL_LOG_ERROR("Total inalid memory count :  %u\n",invalid_mem);
   }
 #endif

}

int32
mem_mgr_get_mtype_info(enum mem_mgr_type mtype, mem_table_t* p_mtype_table_info, int8** m_name)
{
    *m_name = mem_get_mtype_name(mtype);
    if (NULL == *m_name)
    {
        return -1;
    }

    if (mtype < 0 || mtype >= MEM_TYPE_MAX)
    {
        SAL_LOG_INFO("\nMEMMGR::Unknown memory type specified (%d)\n", mtype);
        return -1;
    }

    p_mtype_table_info->req_size = mtype_table[mtype].req_size;
    p_mtype_table_info->ext_size = mtype_table[mtype].ext_size;
    p_mtype_table_info->count = mtype_table[mtype].count;

    return 0;

}

STATIC void
mem_mgr_add_to_mtype_table(uint32 mtype, struct mem_block_header* nmhdr)
{
#ifdef MEM_RUNTIME_CHECK
   struct mem_block_header* mhdr;
   mhdr = NULL;
#endif

   MEMMGR_LOCK(mtype_table[mtype].p_mem_mutex);

#ifdef MEM_RUNTIME_CHECK


    if (mtype_table[mtype].list == NULL)
    {
        mtype_table[mtype].list = (void*)nmhdr;
        nmhdr->next = NULL;
        nmhdr->prev = NULL;
    }
    else
    {
        mhdr = (struct mem_block_header*)mtype_table[mtype].list;
        /* add to the head of the list */
        nmhdr->prev = mhdr->prev;
        nmhdr->next = mhdr;
        mhdr->prev = nmhdr;
        mtype_table[mtype].list = nmhdr;

    }
#endif
    mtype_table[mtype].ext_size += g_sal_ext_size;
    mtype_table[mtype].req_size += nmhdr->req_size;
    mtype_table[mtype].count++;
  MEMMGR_UNLOCK(mtype_table[mtype].p_mem_mutex);

}

/******************************************************************************
 * Name        : mem_mgr_remove_from_mtype_table (int mtype, void *ptr)
 * Purpose     : remove from memory type table
 * Input         : int mtype, void *ptr
 * Output       : N/A
 * Return       : N/A
 * Note          : N/A
 *****************************************************************************/
STATIC int32
mem_mgr_remove_from_mtype_table(enum mem_mgr_type mtype,   struct mem_block_header* mhdr)
{

 MEMMGR_LOCK(mtype_table[mtype].p_mem_mutex);
#ifdef MEM_RUNTIME_CHECK
    if (mtype_table[mtype].list == NULL)
    {
        SAL_LOG_INFO("MEMMGR::Can't delete - mtype list is empty\n");
        MEMMGR_UNLOCK(mtype_table[mtype].p_mem_mutex);
        return -1;
    }
    /* in case of list having only one memory block */
    if (mhdr->next == NULL && mhdr->prev == NULL)
    {
        mtype_table[mtype].list = NULL;
    }
    /* memory block happens to be the first element in the list */
    else if (mhdr == (struct mem_block_header*)mtype_table[mtype].list)
    {
        mtype_table[mtype].list = mhdr->next;
        mhdr->next->prev = NULL;
    }
    /* remove block from mtype list and update pointers */
    else
    {
        mhdr->prev->next = mhdr->next;
        if (mhdr->next != NULL)
        {
            mhdr->next->prev = mhdr->prev;
        }
    }
#endif
    mtype_table[mtype].ext_size -= g_sal_ext_size;
    mtype_table[mtype].req_size -= mhdr->req_size;
    mtype_table[mtype].count--;
   MEMMGR_UNLOCK(mtype_table[mtype].p_mem_mutex);
    return 0; /* success */
}

int32
mem_mgr_init(void)
{
    int32  index = 0;
    int32  ret;

    /* init memory table */
    sal_memset(mtype_table,  0,  sizeof(struct mem_table) * MEM_TYPE_MAX);

   #ifdef MEM_RUNTIME_CHECK
       g_sal_overlap_chk = 1;
      g_sal_ext_size = sizeof(struct mem_block_header) +  sizeof(struct mem_block_trailer);
   #else
       g_sal_ext_size = sizeof(struct mem_block_header) ;
   #endif
     g_sal_ext_hdr_size = sizeof(struct mem_block_header);


    for (index = 0; index < MEM_TYPE_MAX; index++)
    {
        mtype_table[index].list  = NULL;
        ret = sal_mutex_create(&(mtype_table[index].p_mem_mutex));
        if ((ret != 0) || (NULL == mtype_table[index].p_mem_mutex))
        {
            return -1;
        }
    }

    return 0;
}

int32
mem_mgr_deinit(void)
{
  int32  index = 0;

    for (index = 0; index < MEM_TYPE_MAX; index++)
    {
        sal_mutex_destroy(mtype_table[index].p_mem_mutex);
    }
    return 0;
}

void*
mem_mgr_malloc(uint32 size, enum mem_mgr_type mtype, char* filename, uint16 line)
{
    struct    mem_block_header* mhdr;

    if ((mtype < 0) && (mtype >= MEM_TYPE_MAX))
    {
        SAL_LOG_ERROR("MEMMGR::Unknown memory type specified\n");
        return NULL;
    }
    if (0 == size)
    {
        return NULL;
    }

    mhdr = (struct mem_block_header*)sal_malloc(size + g_sal_ext_size);
    if (!mhdr)
    {
        return NULL;
    }
    mhdr->mid = mtype;
    mhdr->req_size = size;
    mhdr->guard = MEM_HEADER_MAGIC_NUMBER;

#ifdef MEM_RUNTIME_CHECK
    mem_mgr_set_memblock_tail(mhdr,  filename, line);
#endif
    mem_mgr_add_to_mtype_table( mtype, mhdr);

    return (int8*)mhdr + sizeof(struct mem_block_header);
}

void
mem_mgr_free(void* ptr, char* file, int line)
{
    struct mem_block_header* mhdr;

    mhdr = (struct mem_block_header*)((int8*)ptr - g_sal_ext_hdr_size);
    if (mhdr->mid >= MEM_TYPE_MAX)
    {
        SAL_LOG_ERROR("MEMMGR::Unknown memory type specified\n");

    }
 #ifdef MEM_RUNTIME_CHECK
      /* see if any of the pre guard area is corrupted */
    if (mhdr->guard != MEM_HEADER_MAGIC_NUMBER)
    {
        SAL_LOG_ERROR("MEMMGR::head guard 0x%6x check failed  - hdr %p - mtype %d - file %s - line %d\n",
                mhdr->guard,  mhdr, mhdr->mid, file, line);
    }

     mem_mgr_ptr_check(ptr);
 #endif
     mem_mgr_remove_from_mtype_table(mhdr->mid, mhdr);
    sal_free((void*)mhdr);

}

void*
mem_mgr_realloc(uint32 mtype, void* ptr, uint32 size, char* filename, uint16 line)
{

    struct    mem_block_header* mhdr;
    struct    mem_block_header* new_mhdr;

    if ((mtype < 0) && (mtype >= MEM_TYPE_MAX))
    {
        SAL_LOG_INFO("MEMMGR::Unknown memory type specified\n");
        return NULL;
    }

    mhdr = (struct mem_block_header*)((int8*)ptr - g_sal_ext_hdr_size);
 #ifdef MEM_RUNTIME_CHECK
    if(mhdr->mid != mtype)
   {
      SAL_LOG_ERROR("realloc no exist poiner!!!!,filename:%s,line:%u\n",filename,line);
      return NULL ;
   }
   if (mhdr->guard != MEM_HEADER_MAGIC_NUMBER)
    {
        SAL_LOG_ERROR("MEMMGR::head guard 0x%6x check failed  - hdr %p - mtype %d - file %s - line %d\n",
                mhdr->guard,  mhdr, mhdr->mid, filename, line);
    }
#endif
    mem_mgr_remove_from_mtype_table(mhdr->mid,  mhdr);
     new_mhdr = (struct mem_block_header*)sal_realloc(mhdr,size+g_sal_ext_size);
     if (!new_mhdr)
     {
        mem_mgr_add_to_mtype_table( mtype, mhdr);
       SAL_LOG_ERROR("realloc fail!!!!,filename:%s,line:%u\n",filename,line);
       return NULL;
     }
     new_mhdr->mid = mtype;
     new_mhdr->req_size = size;
     new_mhdr->guard = MEM_HEADER_MAGIC_NUMBER;
 #ifdef MEM_RUNTIME_CHECK
     mem_mgr_set_memblock_tail(new_mhdr,  filename, line);
 #endif
     mem_mgr_add_to_mtype_table( mtype, new_mhdr);
    return (void*)new_mhdr+sizeof(struct mem_block_header);
}



STATIC void
mem_mgr_check_handler(void* arg)
{
    uint8 mtype = 0;
    for (mtype = 0; mtype < MEM_TYPE_MAX; mtype++)
    {
        mem_mgr_check_mtype(  mtype,1);
    }
}

int32
mem_mgr_overlap_chk_en(uint8 enable,uint32 interval )
{

    if(!g_sal_overlap_chk)
    {
       return -1;
    }

    if(enable)
    {
      if(interval == 0)
      {
          mem_mgr_check_handler(NULL);
          if(p_check_timer) sal_timer_stop(p_check_timer);
     }
     else
     {
             if(!p_check_timer)
              {
                 sal_timer_create(&p_check_timer, mem_mgr_check_handler, NULL);
              }
              else
              {
               sal_timer_stop(p_check_timer);
              }
              sal_timer_start(p_check_timer, interval*1000);
     }

    }
    else if(p_check_timer)
    {
       sal_timer_stop(p_check_timer);
       sal_timer_destroy(p_check_timer);
       p_check_timer = NULL;
    }
    return 0;
}
