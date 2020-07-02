/**
 @file sys_goldengate_dma_debug.c

 @author  Copyright (C) 2012 Centec Networks Inc.  All rights reserved.

 @date 2012-10-23

 @version v2.0

 This file define sys functions

*/

/****************************************************************************
*
* Header Files
*
****************************************************************************/
#include "sal.h"
#include "dal.h"
#include "ctc_packet.h"
#include "ctc_goldengate_interrupt.h"
#include "sys_goldengate_chip.h"
#include "sys_goldengate_interrupt.h"
#include "sys_goldengate_common.h"
#include "sys_goldengate_packet.h"
#include "sys_goldengate_dma.h"
#include "sys_goldengate_dma_priv.h"

extern sys_dma_master_t* p_gg_dma_master[CTC_MAX_LOCAL_CHIP_NUM];
extern dal_op_t g_dal_op;

extern int32
sys_goldengate_dma_function_pause(uint8 lchip, uint8 func_type, uint8 en);
extern int32
sys_goldengate_dma_sync_pkt_rx_stats(uint8 lchip);
extern int32
sys_goldengate_dma_sync_pkt_tx_stats(uint8 lchip);

STATIC void
_sys_goldengate_show_desc_info(uint8 lchip, sys_dma_chan_t* p_chan_info, uint32 index)
{
    sys_dma_desc_t* p_desc = NULL;
    DsDesc_m* p_desc_mem = NULL;
    uint64 phy_addr = 0;
    p_desc = &(p_chan_info->p_desc[index]);

    if (NULL == p_desc)
    {
        return;
    }

    p_desc_mem = &(p_desc->desc_info);
    COMBINE_64BITS_DATA(p_gg_dma_master[lchip]->dma_high_addr, (GetDsDesc(V, memAddr_f, p_desc)<<4), phy_addr);
    SYS_DMA_DUMP("%-5d %p %p  %-4d %-4d %-4d %-3d %-4d %-5d %-5d %-3d %-3d %-3d %-3d %-3d 0x%-8x 0x%-8x\n",
        index,
        p_desc,
        g_dal_op.phy_to_logic(lchip, phy_addr),
        GetDsDesc(V, realSize_f, p_desc_mem),
        GetDsDesc(V, cfgSize_f, p_desc_mem),
        GetDsDesc(V, dataStruct_f, p_desc_mem),
        GetDsDesc(V, done_f, p_desc_mem),
        GetDsDesc(V, error_f, p_desc_mem),
        GetDsDesc(V, dataError_f, p_desc_mem),
        GetDsDesc(V, descError_f, p_desc_mem),
        GetDsDesc(V, timeout_f, p_desc_mem),
        GetDsDesc(V, pause_f, p_desc_mem),
        GetDsDesc(V, intrMask_f, p_desc_mem),
        GetDsDesc(V, u1_pkt_eop_f, p_desc_mem),
        GetDsDesc(V, u1_pkt_sop_f, p_desc_mem),
        GetDsDesc(V, timestamp_ns_f, p_desc_mem),
        GetDsDesc(V, timestamp_s_f, p_desc_mem)
        );
    return;
}

uint8 sys_goldengate_dma_get_func_type(uint8 chan_id)
{
    if (chan_id < SYS_DMA_RX_CHAN_NUM)
    {
        return CTC_DMA_FUNC_PACKET_RX;
    }

    switch (chan_id)
    {
        case SYS_DMA_PACKET_TX0_CHAN_ID:
            return CTC_DMA_FUNC_PACKET_TX;

        case SYS_DMA_PACKET_TX1_CHAN_ID:
            return CTC_DMA_FUNC_PACKET_TX;

        case SYS_DMA_TBL_WR_CHAN_ID:
            return CTC_DMA_FUNC_TABLE_W;

        case SYS_DMA_TBL_RD_CHAN_ID:
            return CTC_DMA_FUNC_TABLE_R;

        case SYS_DMA_PORT_STATS_CHAN_ID:
            return CTC_DMA_FUNC_STATS;

        case SYS_DMA_LEARNING_CHAN_ID:
            return CTC_DMA_FUNC_HW_LEARNING;

        case SYS_DMA_IPFIX_CHAN_ID:
            return CTC_DMA_FUNC_IPFIX;

        case SYS_DMA_SDC_CHAN_ID:
            return CTC_DMA_FUNC_SDC;

        case SYS_DMA_MONITOR_CHAN_ID:
            return CTC_DMA_FUNC_MONITOR;

        default:
            return 0xff;
    }
}

STATIC int32
_sys_goldengate_dma_show_dyn(uint8 lchip, uint8 chan_id, uint8 dmasel)
{
    DmaDynInfo0_m dyn_info;
    uint32 cmd = 0;
    uint32 tbl_id = 0;

    sal_memset(&dyn_info, 0, sizeof(DmaDynInfo0_m));

    tbl_id = DmaDynInfo0_t + dmasel;
    cmd = DRV_IOR(tbl_id, DRV_ENTRY_FLAG);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, chan_id, cmd, &dyn_info));

    SYS_DMA_DUMP("%-6d%-9d%-9d%-9d%-9d%-9d%-6d%-6d%-6d%-6d%-6d\n",
        chan_id,
        GetDmaDynInfo0(V, ringWrPtr_f, &dyn_info),
        GetDmaDynInfo0(V, ringRdPtr_f, &dyn_info),
        GetDmaDynInfo0(V, cacheWrPtr_f, &dyn_info),
        GetDmaDynInfo0(V, cacheRdPtr_f, &dyn_info),
        GetDmaDynInfo0(V, ringTailPtr_f, &dyn_info),
        GetDmaDynInfo0(V, fetchCnt_f, &dyn_info),
        GetDmaDynInfo0(V, cacheCnt_f, &dyn_info),
        GetDmaDynInfo0(V, dataSeq_f, &dyn_info),
        GetDmaDynInfo0(V, procCnt_f, &dyn_info),
        GetDmaDynInfo0(V, resCnt_f, &dyn_info)
        );

    return 0;
}

STATIC int32
_sys_goldengate_dma_show_chan(uint8 lchip, uint8 chan_id)
{
    uint32 tbl_id = 0;
    uint32 cmd = 0;
    sys_dma_chan_t* p_chan_info = NULL;
    DmaCtlTab0_m ctl_tab;
    uint32 used_mem = 0;
    uint16 valid_desc_nmu = 0;
    uint8 sync_chan = 0;
    uint8 no_sync = 0;
    char func_str[SYS_DMA_MAX_CHAN_NUM][12] =
    {
        "PtkRx0", "PtkRx1", "PtkRx2", "PtkRx3", "PktTx0", "PktTx1", "TblWr", "Tblrd",
        "Stats",   "Interal",    "QueStats", "Learning", "HashKey", "Ipfix", "SDC", "Monitor"
    };
    sys_dma_thread_t* p_thread_info = NULL;

    if (chan_id >= SYS_DMA_MAX_CHAN_NUM)
    {
        return CTC_E_EXCEED_MAX_SIZE;
    }

    SYS_DMA_INIT_CHECK(lchip);

    p_chan_info = (sys_dma_chan_t*)&(p_gg_dma_master[lchip]->dma_chan_info[chan_id]);

    tbl_id = DmaCtlTab0_t + p_chan_info->dmasel;
    cmd = DRV_IOR(tbl_id, DRV_ENTRY_FLAG);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, chan_id, cmd, &ctl_tab));

    valid_desc_nmu = GetDmaCtlTab0(V, vldNum_f, &ctl_tab);
    used_mem = sizeof(sys_dma_desc_t) * p_chan_info->desc_depth + p_chan_info->data_size * p_chan_info->desc_depth;

    /*Get sync chan*/
    if (p_chan_info->sync_en)
    {
        sync_chan = p_chan_info->sync_chan;

        /*get sync thread info*/
        p_thread_info = ctc_vector_get(p_gg_dma_master[lchip]->p_thread_vector, sync_chan);
        if (!p_thread_info)
        {
            return CTC_E_INVALID_PARAM;
        }
    }
    else
    {
        no_sync = 1;
    }

    if (!no_sync)
    {
        SYS_DMA_DUMP("%-6d%-10s%-10d%-7d%-8d%p   %-15d%-10s%4u\n",
        p_chan_info->channel_id,
        func_str[chan_id],
        valid_desc_nmu,
        p_chan_info->desc_depth,
        p_chan_info->current_index,
        (void*)p_chan_info->mem_base,
        used_mem,
        p_thread_info->desc,
        p_thread_info->prio);
    }
    else
    {
        SYS_DMA_DUMP("%-6d%-10s%-10d%-7d%-8d%p   %-15d%-10s%3s\n",
        p_chan_info->channel_id,
        func_str[chan_id],
        valid_desc_nmu,
        p_chan_info->desc_depth,
        p_chan_info->current_index,
        (void*)p_chan_info->mem_base,
        used_mem,
        "-",
        "-");
    }
    return CTC_E_NONE;
}

int32
sys_goldengate_dma_show_status(uint8 lchip)
{
    uint8 index = 0;
    uint32 dma_mem_size = 0;
    uint32 used_size = 0;
    uint32 total_used_size = 0;
    dal_dma_info_t dma_info;
    sys_dma_chan_t* p_chan_info = NULL;

    SYS_DMA_INIT_CHECK(lchip);
    sal_memset(&dma_info, 0, sizeof(dal_dma_info_t));
    if (p_gg_dma_master[lchip] == NULL)
    {
        SYS_DMA_DUMP("DMA does not inited\n");
        return CTC_E_NOT_INIT;
    }

    /*get total dma memory*/
    dal_get_dma_info(lchip, &dma_info);
    dma_mem_size = (uint32)dma_info.size;

    /*get total used dma memory*/
    for(index = 0; index < SYS_DMA_MAX_CHAN_NUM; index++)
    {
        if (CTC_IS_BIT_SET(p_gg_dma_master[lchip]->dma_en_flag, index)&& (index != 9))
        {
            p_chan_info = (sys_dma_chan_t*)&(p_gg_dma_master[lchip]->dma_chan_info[index]);
            used_size = sizeof(sys_dma_desc_t) * p_chan_info->desc_depth + p_chan_info->data_size * p_chan_info->desc_depth;

            total_used_size += used_size;
        }
    }

    SYS_DMA_DUMP("\n");
    SYS_DMA_DUMP("%-25s%-3s%-10d%-4s\n", " DMA Total Memory", ":", dma_mem_size, "Byte");
    SYS_DMA_DUMP("%-25s%-3s%-10d%-4s\n", " DMA Used Memory", ":",total_used_size, "Byte");
    SYS_DMA_DUMP("%-25s%-3s%p\n", " DMA Virtual Base", ":", dma_info.virt_base);
    SYS_DMA_DUMP("%-25s%-3s0x%x\n", " DMA Physical Base(Low)", ":", dma_info.phy_base);
    SYS_DMA_DUMP("%-25s%-3s0x%x\n\n", " DMA Physical Base(High)", ":", dma_info.phy_base_hi);

    SYS_DMA_DUMP("%-6s%-10s%-10s%-7s%-8s%-10s%-15s%-10s%-15s\n",
        "Chan", "Function", "ValidNum", "Depth", "CurIdx", "MemBase", "UsedMem(Byte)", "Thread", "Pri");
    SYS_DMA_DUMP("--------------------------------------------------------------------------------\n");

    for(index = 0; index < SYS_DMA_MAX_CHAN_NUM; index++)
    {
        if (CTC_IS_BIT_SET(p_gg_dma_master[lchip]->dma_en_flag, index)&& (index != 9))
        {
            _sys_goldengate_dma_show_chan(lchip, index);
        }
    }
    SYS_DMA_DUMP("\n");

    return CTC_E_NONE;
}

int32
sys_goldengate_dma_dynamic_info(uint8 lchip)
{
    uint8 index = 0;

    SYS_DMA_INIT_CHECK(lchip);
    if (p_gg_dma_master[lchip] == NULL)
    {
        SYS_DMA_DUMP("DMA does not inited\n");
        return CTC_E_NOT_INIT;
    }

    SYS_DMA_DUMP("\n");
    SYS_DMA_DUMP(" %-20s", "CRdPtr-CacheRdPtr");
    SYS_DMA_DUMP("%-20s", "RTaiPtr-RTailPtr");
    SYS_DMA_DUMP("%-20s\n", "FCnt-FetchCnt");
    SYS_DMA_DUMP(" %-20s", "CCnt-CacheCnt");
    SYS_DMA_DUMP("%-20s", "DSeq-DataSeq");
    SYS_DMA_DUMP("%-20s\n", "PCnt-ProcCnt");
    SYS_DMA_DUMP(" %-20s", "RCnt-ResCnt");
    SYS_DMA_DUMP("%-20s", "RWrPtr-RingWrPtr");
    SYS_DMA_DUMP("%-20s", "RRdPtr-RingRdPtr");
    SYS_DMA_DUMP("%-20s\n", "CWrPtr-CacheWrPtr");
    SYS_DMA_DUMP("\n");
    SYS_DMA_DUMP("%-6s%-9s%-9s%-9s%-9s%-9s%-6s%-6s%-6s%-6s%-6s\n",
        "Chan", "RWrPtr", "RRdPtr", "CWrPtr", "CRdPtr", "RTaiPtr", "FCnt", "CCnt", "DSeq", "PCnt", "RCnt");
    SYS_DMA_DUMP("-------------------------------------------------------------------------------\n");
    for(index = 0; index < SYS_DMA_MAX_CHAN_NUM; index++)
    {

        if (CTC_IS_BIT_SET(p_gg_dma_master[lchip]->dma_en_flag, index) && (p_gg_dma_master[lchip]->dma_chan_info[index].dmasel == 0) && (index != 9))
        {
            _sys_goldengate_dma_show_dyn(lchip, index, 0);

        }
    }
    SYS_DMA_DUMP("\n");

    return CTC_E_NONE;
}

int32
sys_goldengate_dma_clear_pkt_stats(uint8 lchip, uint8 para)
{
    SYS_DMA_INIT_CHECK(lchip);

    if (para == 0)   /*clear all*/
    {
        sys_goldengate_dma_sync_pkt_rx_stats(lchip);
        sys_goldengate_dma_sync_pkt_tx_stats(lchip);
        sal_memset(&(p_gg_dma_master[lchip]->dma_stats), 0, sizeof(sys_dma_stats_t));
    }
    else if (para == 1)  /*clear rx*/
    {
        sys_goldengate_dma_sync_pkt_rx_stats(lchip);
        p_gg_dma_master[lchip]->dma_stats.rx0_pkt_cnt       = 0;
         p_gg_dma_master[lchip]->dma_stats.rx0_byte_cnt     = 0;
         p_gg_dma_master[lchip]->dma_stats.rx0_drop_cnt     = 0;
         p_gg_dma_master[lchip]->dma_stats.rx0_error_cnt    = 0;

         p_gg_dma_master[lchip]->dma_stats.rx1_pkt_cnt      = 0;
         p_gg_dma_master[lchip]->dma_stats.rx1_byte_cnt     = 0;
         p_gg_dma_master[lchip]->dma_stats.rx1_drop_cnt     = 0;
         p_gg_dma_master[lchip]->dma_stats.rx1_error_cnt    = 0;

         p_gg_dma_master[lchip]->dma_stats.rx2_pkt_cnt      = 0;
         p_gg_dma_master[lchip]->dma_stats.rx2_byte_cnt     = 0;
         p_gg_dma_master[lchip]->dma_stats.rx2_drop_cnt     = 0;
         p_gg_dma_master[lchip]->dma_stats.rx2_error_cnt    = 0;

         p_gg_dma_master[lchip]->dma_stats.rx3_pkt_cnt      = 0;
         p_gg_dma_master[lchip]->dma_stats.rx3_byte_cnt     = 0;
         p_gg_dma_master[lchip]->dma_stats.rx3_drop_cnt     = 0;
         p_gg_dma_master[lchip]->dma_stats.rx3_error_cnt    = 0;
    }
    else      /*clear tx*/
    {
         sys_goldengate_dma_sync_pkt_tx_stats(lchip);
         p_gg_dma_master[lchip]->dma_stats.tx0_bad_byte   = 0;
         p_gg_dma_master[lchip]->dma_stats.tx0_bad_cnt    = 0;
         p_gg_dma_master[lchip]->dma_stats.tx0_good_byte  = 0;
         p_gg_dma_master[lchip]->dma_stats.tx0_good_cnt   = 0;

         p_gg_dma_master[lchip]->dma_stats.tx1_bad_byte   = 0;
         p_gg_dma_master[lchip]->dma_stats.tx1_bad_cnt    = 0;
         p_gg_dma_master[lchip]->dma_stats.tx1_good_byte  = 0;
         p_gg_dma_master[lchip]->dma_stats.tx1_good_cnt   = 0;
    }
    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_dma_show_rx_stats(uint8 lchip)
{

    CTC_ERROR_RETURN(sys_goldengate_dma_sync_pkt_rx_stats(lchip));

    SYS_DMA_DUMP("\n");
    SYS_DMA_DUMP("DMA Receive Stats:\n");
    SYS_DMA_DUMP("------------------------------------------------------------------------------------\n");
    SYS_DMA_DUMP("%-6s%-21s%-21s%-21s%-21s\n", "Chan", "RecPktCnt", "TotalByte", "DropPktCnt", "ErrPktCnt");
    SYS_DMA_DUMP("------------------------------------------------------------------------------------\n");
    SYS_DMA_DUMP("%-6d%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"\n", 0,
                    p_gg_dma_master[lchip]->dma_stats.rx0_pkt_cnt, p_gg_dma_master[lchip]->dma_stats.rx0_byte_cnt,
                    p_gg_dma_master[lchip]->dma_stats.rx0_drop_cnt, p_gg_dma_master[lchip]->dma_stats.rx0_error_cnt);

    SYS_DMA_DUMP("%-6d%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"\n", 1,
                    p_gg_dma_master[lchip]->dma_stats.rx1_pkt_cnt, p_gg_dma_master[lchip]->dma_stats.rx1_byte_cnt,
                    p_gg_dma_master[lchip]->dma_stats.rx1_drop_cnt, p_gg_dma_master[lchip]->dma_stats.rx1_error_cnt);

    SYS_DMA_DUMP("%-6d%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"\n", 2,
                    p_gg_dma_master[lchip]->dma_stats.rx2_pkt_cnt, p_gg_dma_master[lchip]->dma_stats.rx2_byte_cnt,
                    p_gg_dma_master[lchip]->dma_stats.rx2_drop_cnt, p_gg_dma_master[lchip]->dma_stats.rx2_error_cnt);

    SYS_DMA_DUMP("%-6d%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"\n", 3,
                    p_gg_dma_master[lchip]->dma_stats.rx3_pkt_cnt, p_gg_dma_master[lchip]->dma_stats.rx3_byte_cnt,
                    p_gg_dma_master[lchip]->dma_stats.rx3_drop_cnt, p_gg_dma_master[lchip]->dma_stats.rx3_error_cnt);

    SYS_DMA_DUMP("------------------------------------------------------------------------------------\n");
    SYS_DMA_DUMP("%-6s%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"\n", "Total",
                                                  p_gg_dma_master[lchip]->dma_stats.rx0_pkt_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx1_pkt_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx2_pkt_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx3_pkt_cnt,
                                                  p_gg_dma_master[lchip]->dma_stats.rx0_byte_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx1_byte_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx2_byte_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx3_byte_cnt,
                                                  p_gg_dma_master[lchip]->dma_stats.rx0_drop_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx1_drop_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx2_drop_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx3_drop_cnt,
                                                  p_gg_dma_master[lchip]->dma_stats.rx0_error_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx1_error_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx2_error_cnt
                                                + p_gg_dma_master[lchip]->dma_stats.rx3_error_cnt);

    SYS_DMA_DUMP("\n");

    return CTC_E_NONE;

}

STATIC int32
_sys_goldengate_dma_show_tx_stats(uint8 lchip)
{

    CTC_ERROR_RETURN(sys_goldengate_dma_sync_pkt_tx_stats(lchip));
    SYS_DMA_DUMP("\n");
    SYS_DMA_DUMP("DMA Transmit Stats:\n");
    SYS_DMA_DUMP("------------------------------------------------------------------------------------\n");
    SYS_DMA_DUMP("%-6s%-21s%-21s%-21s%-21s\n", "Chan", "GoodPktCnt", "GoodPktByte", "BadPktCnt", "BadPktByte");
    SYS_DMA_DUMP("------------------------------------------------------------------------------------\n");
    SYS_DMA_DUMP("%-6d%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"\n", 0,
                    p_gg_dma_master[lchip]->dma_stats.tx0_good_cnt, p_gg_dma_master[lchip]->dma_stats.tx0_good_byte,
                    p_gg_dma_master[lchip]->dma_stats.tx0_bad_cnt, p_gg_dma_master[lchip]->dma_stats.tx0_bad_byte);
    SYS_DMA_DUMP("%-6d%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"\n", 1,
                    p_gg_dma_master[lchip]->dma_stats.tx1_good_cnt, p_gg_dma_master[lchip]->dma_stats.tx1_good_byte,
                    p_gg_dma_master[lchip]->dma_stats.tx1_bad_cnt, p_gg_dma_master[lchip]->dma_stats.tx1_bad_byte);

    SYS_DMA_DUMP("------------------------------------------------------------------------------------\n");
    SYS_DMA_DUMP("%-6s%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"%-21"PRIu64"\n", "Total",
                                                                      p_gg_dma_master[lchip]->dma_stats.tx0_good_cnt
                                                                    + p_gg_dma_master[lchip]->dma_stats.tx1_good_cnt,
                                                                      p_gg_dma_master[lchip]->dma_stats.tx0_good_byte
                                                                    + p_gg_dma_master[lchip]->dma_stats.tx1_good_byte,
                                                                      p_gg_dma_master[lchip]->dma_stats.tx0_bad_cnt
                                                                    + p_gg_dma_master[lchip]->dma_stats.tx1_bad_cnt,
                                                                      p_gg_dma_master[lchip]->dma_stats.tx0_bad_byte
                                                                    + p_gg_dma_master[lchip]->dma_stats.tx1_bad_byte);
    SYS_DMA_DUMP("\n");

    return CTC_E_NONE;
}

int32
sys_goldengate_dma_show_stats(uint8 lchip)
{
    SYS_DMA_INIT_CHECK(lchip);
    CTC_ERROR_RETURN(_sys_goldengate_dma_show_rx_stats(lchip));
    CTC_ERROR_RETURN(_sys_goldengate_dma_show_tx_stats(lchip));

    return CTC_E_NONE;
}

int32
sys_goldengate_dma_show_desc(uint8 lchip, uint8 chan_id, uint32 start_idx, uint32 end_idx)
{
    sys_dma_chan_t* p_dma_chan = NULL;
    uint32 desc_dep = 0;
    uint32 index = 0;

    SYS_DMA_INIT_CHECK(lchip);

    if (!CTC_IS_BIT_SET(p_gg_dma_master[lchip]->dma_en_flag, chan_id))
    {
        SYS_DMA_DUMP("DMA function is not enable!\n");
        return CTC_E_NONE;
    }

    if (SYS_DMA_MAX_CHAN_NUM <= chan_id)
    {
        return CTC_E_EXCEED_MAX_SIZE;
    }

    p_dma_chan =  (sys_dma_chan_t*)&(p_gg_dma_master[lchip]->dma_chan_info[chan_id]);
    desc_dep =  p_dma_chan->desc_depth;

    if (start_idx > end_idx)
    {
        return CTC_E_EXCEED_MAX_SIZE;
    }

    if (end_idx >= desc_dep)
    {
        return CTC_E_EXCEED_MAX_SIZE;
    }
    SYS_DMA_DUMP("\n");
    SYS_DMA_DUMP(" %-22s", "P-Pause");
    SYS_DMA_DUMP("%-26s", "DR-Data Error");
    SYS_DMA_DUMP("%-26s\n", "D-Descriptor Done");
    SYS_DMA_DUMP(" %-22s", "ER-Total Error");
    SYS_DMA_DUMP("%-26s", "CSize-Config Size");
    SYS_DMA_DUMP("%-26s\n", "S-Descriptor SOP");
    SYS_DMA_DUMP(" %-22s", "RSize-Actual Size");
    SYS_DMA_DUMP("%-26s", "T-Descriptor Timeout");
    SYS_DMA_DUMP("%-26s\n", "M-Interrupt Mask");
    SYS_DMA_DUMP(" %-22s", "E-Descriptor EOP");
    SYS_DMA_DUMP("%-26s", "DS-DataStruct(For Reg)");
    SYS_DMA_DUMP("%-26s\n", "DeE-Descriptor Error");
    SYS_DMA_DUMP(" %-38s", "DAddr-Data Memory Address");
    SYS_DMA_DUMP("%-27s\n", "CfgAddr-Descriptor Config Address");
    SYS_DMA_DUMP("\n");

    SYS_DMA_DUMP("%-5s%-10s%-10s%-4s %-4s %-4s %-3s %-4s %-5s %-5s %-3s %-3s %-3s %-3s %-3s %-4s  %-4s\n",
        "IDX", "DescAddr", "DAddr", "RSize", "CSize", "DS", "D", "ER", "DER", "DeE", "T", "P", "M", "E", "S", "TS(ns)", "TS(s)");
    SYS_DMA_DUMP("-------------------------------------------------------------------------------------------\n");

    for (index = start_idx; index <= end_idx; index++)
    {
        _sys_goldengate_show_desc_info(lchip, p_dma_chan, index);
    }

    return 0;
}

int32
sys_goldengate_dma_table_rw(uint8 lchip, uint8 is_read, uint32 addr, uint16 entry_len, uint16 entry_num, uint32* p_value)
{
    int32 ret = 0;
    sys_dma_tbl_rw_t tbl_cfg;

    SYS_DMA_INIT_CHECK(lchip);
    sal_memset(&tbl_cfg, 0, sizeof(sys_dma_tbl_rw_t));

    if (is_read)
    {
        tbl_cfg.rflag = 1;
        tbl_cfg.tbl_addr = addr;
        tbl_cfg.entry_len = entry_len;
        tbl_cfg.entry_num = entry_num;
        tbl_cfg.buffer = p_value;
        ret = sys_goldengate_dma_rw_table(lchip, &tbl_cfg);
    }
    else
    {
        tbl_cfg.rflag = 0;
        tbl_cfg.tbl_addr = addr;
        tbl_cfg.entry_len = entry_len;
        tbl_cfg.entry_num = entry_num;
        tbl_cfg.buffer = p_value;
        ret = sys_goldengate_dma_rw_table(lchip, &tbl_cfg);
    }


    return ret;
}

