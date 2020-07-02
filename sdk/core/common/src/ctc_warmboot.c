#include "sal.h"
#include "ctc_warmboot.h"

#include "ctc_register.h"
#include "ctcs_api.h"
#include "ctc_api.h"

extern uint8 g_lchip_num;
ctc_wb_master_t *g_wb_master;

#define CTC_WB_APPID_HASH_NUM 512

#define CTC_WB_APPID_HASH_BlOCK_NUM 16

#define CTC_WB_APPID_MEM_SIZE   0x1c00000

uint32
ctc_wb_dm_hash_make(ctc_wb_appid_t* node)
{
    return ctc_hash_caculate(sizeof(node->app_id), &(node->app_id));
}

int32
_ctc_wb_dm_hash_free_node_data(void* node_data, void* user_data)
{
    sal_free(node_data);

    return CTC_E_NONE;
}

bool
ctc_wb_dm_hash_cmp(ctc_wb_appid_t* stored_node, ctc_wb_appid_t* lkup_node)
{
    if (!stored_node || !lkup_node)
    {
        return FALSE;
    }

    if (stored_node->app_id== lkup_node->app_id)
    {
        return TRUE;
    }

    return FALSE;
}

uint32 ctc_wb_get_sync_bmp(uint8 lchip, uint8 mod_id)
{
   if (!CTC_WB_DM_MODE)
   {
       return 0;
   }

   return (mod_id < CTC_FEATURE_MAX)? g_wb_master->sync_bmp[lchip][mod_id] : 0;
}

uint8 _ctc_wb_bmp_clear(uint8 lchip)
{
    uint8 loop = 0;
    if(!CTC_WB_DM_MODE)
    {
        return TRUE;
    }

    for(;loop < CTC_FEATURE_MAX;loop++)
    {
        if(g_wb_master->sync_bmp[lchip][loop] && (loop != CTC_FEATURE_STATS))
        {
            CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "loop[%d] bmp[%d].\n", loop, g_wb_master->sync_bmp[lchip][loop]);
            return FALSE;
        }
    }
    return TRUE;
}

void ctc_wb_set_sync_bmp(uint8 lchip, uint8 mod_id, uint8 sub_id,uint8 enable)
{
    if (sub_id < 32)
    {
        if(enable)
        {
            CTC_BIT_SET(g_wb_master->sync_bmp[lchip][mod_id], sub_id);
        }
        else
        {
            CTC_BIT_UNSET(g_wb_master->sync_bmp[lchip][mod_id], sub_id);
        }
    }
    return ;
}


int32 _ctc_wb_sync_bmp_init(uint8 lchip)
{
    uint8 loop = 0;
    if (!CTC_WB_DM_MODE)
    {
       return CTC_E_NOT_SUPPORT;
    }

    for(;loop < CTC_FEATURE_MAX;loop++)
    {
        g_wb_master->sync_bmp[lchip][loop] = 0;
    }
    return CTC_E_NONE;
}


int32 ctc_wb_init(uint8 lchip, ctc_wb_api_t *wb_api)
{
    int32 ret = CTC_E_NONE;
    uint8 status = 0;

    lchip = g_ctcs_api_en ? lchip : 0;

    if (!wb_api	 ||
     ((!wb_api->init || !wb_api->init_done	 || !wb_api->sync || !wb_api->add_entry || !wb_api->sync_done || !wb_api->query_entry) && !wb_api->mode) )
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "warmboot is not support!\n");
        return CTC_E_NONE;
    }

    if (!g_wb_master)
    {
        g_wb_master = sal_malloc(sizeof(ctc_wb_master_t));
        if(wb_api->mode)
        {
            g_wb_master->start_addr = wb_api->start_addr;
            g_wb_master->total_size = wb_api->size;
            for (lchip = 0; lchip < CTC_MAX_LOCAL_CHIP_NUM; lchip++)
            {
                g_wb_master->sync_bmp[lchip] = (uint32 *)g_wb_master->start_addr + lchip * CTC_FEATURE_MAX;
            }
            g_wb_master->cur_addr = g_wb_master->start_addr + sizeof(uint32) * CTC_FEATURE_MAX * CTC_MAX_CHIP_NUM;

        }
        else
        {
            g_wb_master->init = wb_api->init;
            g_wb_master->init_done = wb_api->init_done;
            g_wb_master->sync = wb_api->sync;
            g_wb_master->sync_done = wb_api->sync_done;
            g_wb_master->add_entry = wb_api->add_entry;
            g_wb_master->query_entry = wb_api->query_entry;
            ret = g_wb_master->init(lchip, wb_api->reloading);
        }
        /* Get actual memory size under DB mode*/
        g_wb_master->appid_hash = ctc_hash_create(CTC_WB_APPID_HASH_BlOCK_NUM,
                                  CTC_WB_APPID_HASH_NUM / CTC_WB_APPID_HASH_BlOCK_NUM,
                                           (hash_key_fn) ctc_wb_dm_hash_make,
                                           (hash_cmp_fn) ctc_wb_dm_hash_cmp);

        g_wb_master->enable  = wb_api->enable;
        g_wb_master->mode = wb_api->mode;
    }

    if( wb_api->enable && wb_api->reloading && ret == CTC_E_NONE)
    { /*warmboot*/
        status = CTC_WB_STATUS_RELOADING;   /*reloading*/
    }
    else
    {
        status = CTC_WB_STATUS_DONE;  /*done*/
    }

    if (g_ctcs_api_en)
    {
        if(status == CTC_WB_STATUS_RELOADING && _ctc_wb_bmp_clear(lchip))
        {
            g_wb_master->wb_status[lchip] = CTC_WB_STATUS_RELOADING;
            CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "chip[%d] init by warmbootb reloading.\n", lchip);
        }
        else
        {
            g_wb_master->wb_status[lchip] = CTC_WB_STATUS_DONE;
            _ctc_wb_sync_bmp_init(lchip);
        }
    }
    else
    {
        for (lchip = 0; lchip < CTC_MAX_LOCAL_CHIP_NUM; lchip++)
        {
            if(status == CTC_WB_STATUS_RELOADING && _ctc_wb_bmp_clear(lchip))
            {
                g_wb_master->wb_status[lchip] = CTC_WB_STATUS_RELOADING;
                //CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "chip[%d] init by warmbootb reloading.\n", lchip);
            }
            else
            {
                g_wb_master->wb_status[lchip] = CTC_WB_STATUS_DONE;
                _ctc_wb_sync_bmp_init(lchip);
            }
        }
    }

    if (ret != CTC_E_NONE)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "ctc_wb_init error, ret = %d\n", ret);
    }

    return CTC_E_NONE;
 }

int32 ctc_wb_init_done(uint8 lchip)
{
    int32 ret = CTC_E_NONE;
    uint32 status = CTC_WB_STATUS_DONE;

    lchip = g_ctcs_api_en ? lchip : 0;

    if (!CTC_WB_ENABLE)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "warmboot is not enable!\n");
        return CTC_E_NOT_READY;
    }

    if (g_wb_master->wb_status[lchip] == CTC_WB_STATUS_RELOADING)
    {
        if(g_wb_master->init_done)
        {
            ret = g_wb_master->init_done(lchip);
            if (ret != CTC_E_NONE)
            {
                CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "ctc_wb_init_done error, ret = %d\n", ret);
                return CTC_E_INVALID_PARAM;
            }
        }

        if (g_ctcs_api_en)
        {
            CTC_ERROR_RETURN(ctcs_global_ctl_set(lchip, CTC_GLOBAL_WARMBOOT_STATUS, &status));
            g_wb_master->wb_status[lchip] = status;/*done*/
        }
        else
        {
            CTC_ERROR_RETURN(ctc_global_ctl_set(CTC_GLOBAL_WARMBOOT_STATUS, &status));
            for (lchip = 0; lchip < CTC_MAX_LOCAL_CHIP_NUM; lchip++)
            {
                g_wb_master->wb_status[lchip] = status;/*done*/
            }
        }
    }

    return CTC_E_NONE;
}

int32 ctc_wb_add_appid(uint8 lchip, ctc_wb_appid_t *app_id)
{
    int32 ret = CTC_E_NONE;
    ctc_wb_appid_t *new_node;
    if (CTC_WB_DM_MODE)
    {
        new_node = (ctc_wb_appid_t *)g_wb_master->cur_addr;
    }
    else
    { /* Get actual memory size under DB mode*/
       new_node = sal_malloc(sizeof(ctc_wb_appid_t));
    }

    new_node->app_id = app_id->app_id;
    new_node->entry_num = app_id->entry_num;
    new_node->entry_size = app_id->entry_size;

    if(CTC_WB_ENABLE && CTC_WB_STATUS(lchip) != CTC_WB_STATUS_RELOADING)
    {
        new_node->valid_cnt = 0;
    }
    //printf("app_id 0x%x entry_num %d entry_size %d valid_cnt %d new_node %p\n", new_node->app_id, new_node->entry_num, new_node->entry_size, new_node->valid_cnt, new_node);
    new_node->address = g_wb_master->cur_addr + sizeof(ctc_wb_appid_t);

    if (ctc_hash_insert(g_wb_master->appid_hash, new_node))
    {
        g_wb_master->cur_addr += new_node->entry_num * new_node->entry_size + sizeof(ctc_wb_appid_t);

    }

    if (CTC_WB_DM_MODE && ((g_wb_master->cur_addr < g_wb_master->start_addr)
        || ((g_wb_master->cur_addr - g_wb_master->start_addr)> g_wb_master->total_size)))
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "warmboot not reserve enough membory, ret = %d\n", ret);
        return CTC_E_NO_MEMORY;
    }

    return ret;

}
uint8 ctc_wb_get_mode(void)
{
   if (!CTC_WB_ENABLE)
   {
       return 0;
   }

   return g_wb_master->mode;
}

uint32 ctc_wb_show_status(uint8 lchip)
{
    ctc_wb_appid_t * p_new_node, lkup_node;
    uint8 mod_id, sub_id, bit;
    uint32 total_actual_mem = 0;
    uint32 actual_mem;
    CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Work mode : %s\n", CTC_WB_DM_MODE?"DM":"DB");
    CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n-----------------------------------------------------------------------\n");
    CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%6s %-12s %-12s %-12s %-12s %-4s %-12s\n","app_id", "entry_num", "memory(KB)", "valid_cnt", "used(%)","bmp", "node_addr");

    for (mod_id = 0; mod_id < CTC_FEATURE_MAX; mod_id++)
    {
        for (sub_id = 0; sub_id < 32; sub_id++)
        {
            lkup_node.app_id = lchip << 16 | CTC_WB_APPID(mod_id, sub_id);
            p_new_node = ctc_hash_lookup(g_wb_master->appid_hash, &lkup_node);
            if (!p_new_node)
            {
                CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_INFO,"%-8X find hash node error!\n",lkup_node.app_id);
                continue;
            }
            actual_mem = p_new_node->entry_num * p_new_node->entry_size;
            total_actual_mem += actual_mem;

            bit = CTC_WB_DM_MODE?CTC_IS_BIT_SET(g_wb_master->sync_bmp[lchip][mod_id], sub_id):0;
            CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"0x%.4X %-12d %-12d %-12d %-12d %-4d %-12p\n", p_new_node->app_id, p_new_node->entry_num, actual_mem>>10, p_new_node->valid_cnt,p_new_node->valid_cnt?( p_new_node->valid_cnt*100/p_new_node->entry_num):0, bit, p_new_node);
        }
    }

    CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n-----------------------------------------------------------------------\n");

    CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Reserved memory size: %u KB\n", (g_wb_master->total_size)>>10);
    CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Acutal memory size  : %u KB\n", total_actual_mem>>10);
    return CTC_E_NONE;

}


int32 ctc_wb_sync(uint8 lchip)
{
    int32 ret = CTC_E_NONE;
    uint32 status = CTC_WB_STATUS_SYNC;

    lchip = g_ctcs_api_en ? lchip : 0;

    if (!CTC_WB_ENABLE)
    {
        return CTC_E_NOT_READY;
    }

    if (!g_wb_master->mode)
    {
        ret = g_wb_master->sync(lchip);
        if (ret != CTC_E_NONE)
        {
            CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "ctc_wb_sync error, ret = %d\n", ret);
            return CTC_E_INVALID_PARAM;
        }
    }

    if (g_ctcs_api_en)
    {
        CTC_ERROR_RETURN(ctcs_global_ctl_set(lchip, CTC_GLOBAL_WARMBOOT_STATUS, &status));
        g_wb_master->wb_status[lchip] = g_wb_master->mode ?CTC_WB_STATUS_DONE: CTC_WB_STATUS_SYNC;/*sync*/
    }
    else
    {
        CTC_ERROR_RETURN(ctc_global_ctl_set(CTC_GLOBAL_WARMBOOT_STATUS, &status));
        for (lchip = 0; lchip < g_lchip_num; lchip++)
        {
            g_wb_master->wb_status[lchip] = g_wb_master->mode ?CTC_WB_STATUS_DONE: CTC_WB_STATUS_SYNC;/*sync*/
        }
    }

    return CTC_E_NONE;
}

int32 ctc_wb_sync_done(uint8 lchip, int32 result)
{
    int32 ret = CTC_E_NONE;
    uint32 status = CTC_WB_STATUS_DONE;

    lchip = g_ctcs_api_en ? lchip : 0;

    if (!CTC_WB_ENABLE)
    {
        return CTC_E_NOT_READY;
    }

     if (!g_wb_master->mode)
    {
        ret = g_wb_master->sync_done(lchip, result);
        if (ret != CTC_E_NONE)
        {
            CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "ctc_wb_sync_done error\n");
        }
    }

    if (g_ctcs_api_en)
    {
        g_wb_master->wb_status[lchip] = status;/*done*/
        CTC_ERROR_RETURN(ctcs_global_ctl_set(lchip, CTC_GLOBAL_WARMBOOT_STATUS, &status));
    }
    else
    {
        for (lchip = 0; lchip < g_lchip_num; lchip++)
        {
            g_wb_master->wb_status[lchip] = status;/*done*/
        }
        CTC_ERROR_RETURN(ctc_global_ctl_set(CTC_GLOBAL_WARMBOOT_STATUS, &status));
    }

    return ret;
}

int32 ctc_wb_add_entry(ctc_wb_data_t *data)
{
    int32 ret = CTC_E_NONE;

    if (!CTC_WB_ENABLE)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "warmboot is not enable!\n");
        return CTC_E_NOT_READY;
    }

    if(data == NULL)
        return CTC_E_INVALID_PTR;

    CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Add %4d entry to mod_id: %2d sub_id:%2d (appid: %4d) table on chip %u\n",
        data->valid_cnt, CTC_WB_MODID(data->app_id), CTC_WB_SUBID(data->app_id),
        data->app_id, CTC_WB_CHIPID(data->app_id));

    if (g_wb_master->mode)
    {
        ctc_wb_appid_t  lkup_node, *p_new_node;
        lkup_node.app_id = data->app_id;

        p_new_node = ctc_hash_lookup(g_wb_master->appid_hash, &lkup_node);
        if (p_new_node == NULL)
        {
            return CTC_E_INVALID_PARAM;
        }

        if ((p_new_node->valid_cnt + data->valid_cnt) > p_new_node->entry_num)
        {
            CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Memory overlap !! appid:0x%x, validCnt %4u > maxNum %4u\n",
                           data->app_id,(p_new_node->valid_cnt + data->valid_cnt), p_new_node->entry_num);
	    return CTC_E_NONE;
        }
        p_new_node->valid_cnt += data->valid_cnt;
    }
    else
    {
        ret = g_wb_master->add_entry ? g_wb_master->add_entry(data) : CTC_E_NOT_SUPPORT;
        if (ret != CTC_E_NONE && ret != CTC_E_NOT_SUPPORT)
        {
            CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "ctc_wb_add_entry error\n");
            ret = CTC_E_INVALID_PARAM;
        }
    }
    return ret;
}

int32 ctc_wb_query_entry(ctc_wb_query_t *query)
{
    int32 ret = CTC_E_NONE;

    if (!CTC_WB_ENABLE)
    {
        return CTC_E_NONE;
    }
    if (g_wb_master->mode)
    {
        ctc_wb_appid_t  lkup_node, *p_new_node;
        lkup_node.app_id = query->app_id;

        p_new_node = ctc_hash_lookup(g_wb_master->appid_hash, &lkup_node);
        if (!p_new_node)
        {
            return CTC_E_INVALID_PARAM;
        }
        query->buffer = (ctc_wb_key_data_t*)p_new_node->address;
        query->buffer_len = p_new_node->entry_size * p_new_node->valid_cnt;
        query->valid_cnt = p_new_node->valid_cnt;
        query->is_end = 1;
    }
    else
    {

        ret = g_wb_master->query_entry ? g_wb_master->query_entry(query) : CTC_E_NOT_SUPPORT;


    }
    CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Query mod_id: %2d sub_id:%2d (appid: %4d) table on chip %u, entry count: %4d, cursor: %6d, end: %d\n",
            CTC_WB_MODID(query->app_id), CTC_WB_SUBID(query->app_id), query->app_id,
            CTC_WB_CHIPID(query->app_id), query->valid_cnt, query->cursor, query->is_end);

    if (ret != CTC_E_NONE && ret != CTC_E_NOT_SUPPORT)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "ctc_wb_query_entry error, ret\n");
        ret = CTC_E_INVALID_PARAM;
    }

    return ret;
}

int32 ctc_wb_set_cpu_rx_en(uint8 lchip, bool enable)
{
    bool value = FALSE;

    if (!CTC_WB_ENABLE)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "warmboot is not enable!\n");
        return CTC_E_NOT_READY;
    }

    value = enable;
    if (g_ctcs_api_en)
    {
        CTC_ERROR_RETURN(ctcs_global_ctl_set(lchip, CTC_GLOBAL_WARMBOOT_CPU_RX_EN, &value));
    }
    else
    {
        CTC_ERROR_RETURN(ctc_global_ctl_set(CTC_GLOBAL_WARMBOOT_CPU_RX_EN, &value));
    }

    return CTC_E_NONE;
}

int32 ctc_wb_get_cpu_rx_en(uint8 lchip, bool* enable)
{
    uint32 value = FALSE;
    CTC_PTR_VALID_CHECK(enable);

    if (!CTC_WB_ENABLE)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "warmboot is not enable!\n");
        return CTC_E_NOT_READY;
    }

    if (g_ctcs_api_en)
    {
        CTC_ERROR_RETURN(ctcs_global_ctl_get(lchip, CTC_GLOBAL_WARMBOOT_CPU_RX_EN, &value));
    }
    else
    {
        CTC_ERROR_RETURN(ctc_global_ctl_get(CTC_GLOBAL_WARMBOOT_CPU_RX_EN, &value));
    }
    *enable = value ? TRUE : FALSE;

    return CTC_E_NONE;
}

int32
ctc_wb_set_interval(uint8 lchip, uint32 interval)
{
    if (!CTC_WB_ENABLE)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "warmboot is not enable!\n");
        return CTC_E_NOT_READY;
    }

    if (g_ctcs_api_en)
    {
        CTC_ERROR_RETURN(ctcs_global_ctl_set(lchip, CTC_GLOBAL_WARMBOOT_INTERVAL, &interval));
    }
    else
    {
        CTC_ERROR_RETURN(ctc_global_ctl_set(CTC_GLOBAL_WARMBOOT_INTERVAL, &interval));
    }

    return CTC_E_NONE;
}

int32
ctc_wb_get_interval(uint8 lchip, uint32* interval)
{
    CTC_PTR_VALID_CHECK(interval);

    if (!CTC_WB_ENABLE)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "warmboot is not enable!\n");
        return CTC_E_NOT_READY;
    }

    if (g_ctcs_api_en)
    {
        CTC_ERROR_RETURN(ctcs_global_ctl_get(lchip, CTC_GLOBAL_WARMBOOT_INTERVAL, interval));
    }
    else
    {
        CTC_ERROR_RETURN(ctc_global_ctl_get(CTC_GLOBAL_WARMBOOT_INTERVAL, interval));
    }

    return CTC_E_NONE;
}

int32 ctc_wb_deinit(uint8 lchip)
{
    uint8 is_free = 1;

    if (!g_wb_master)
    {
        return CTC_E_NONE;
    }

    if (g_ctcs_api_en)
    {
        if (g_wb_master->wb_status[lchip])
        {
            g_wb_master->wb_status[lchip] = 0;
        }

        for (lchip = 0; lchip < g_lchip_num; lchip++)
        {
            if (g_wb_master->wb_status[lchip])
            {
                is_free = 0;
            }
        }
    }
    if (is_free)
    {
        if(!CTC_WB_DM_MODE)
        {
            ctc_hash_traverse(g_wb_master->appid_hash, (hash_traversal_fn)_ctc_wb_dm_hash_free_node_data, NULL);
        }
        ctc_hash_free(g_wb_master->appid_hash);
        sal_free(g_wb_master);
        g_wb_master = NULL;
    }

    return CTC_E_NONE;
}

