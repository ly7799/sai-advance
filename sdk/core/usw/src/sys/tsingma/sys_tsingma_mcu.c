/**
 @file sys_tsingma_mcu.c

 @author  Copyright (C) 2018 Centec Networks Inc.  All rights reserved.

 @date 2018-03-19

 @version v1.0

*/

/****************************************************************************
 *
* Header Files
*
****************************************************************************/
#include "sal.h"
#include "ctc_error.h"
#include "ctc_debug.h"

#include "sys_usw_common.h"
#include "sys_usw_datapath.h"
#include "sys_tsingma_datapath.h"
#include "sys_usw_mac.h"
#include "sys_tsingma_mac.h"
#include "sys_usw_mcu.h"
#include "sys_tsingma_mcu.h"

#include "drv_api.h"
#include "usw/include/drv_chip_ctrl.h"
#include "usw/include/drv_common.h"
#include "sys_tsingma_hss12g_firmware.inc"

extern sys_datapath_master_t* p_usw_datapath_master[];
/*DFE operation switch of 12G RX auto & LT    0-keep DFE enable  1-DFE may be disabled in low loss channel*/
uint32 g_tm_dfe_opr_switch = 0;


extern int32 drv_usw_mcu_tbl_lock(uint8 lchip, tbls_id_t tbl_id, uint32 op);
extern int32 drv_usw_mcu_tbl_unlock(uint8 lchip, tbls_id_t tbl_id, uint32 op);
extern int32 drv_usw_mcu_lock(uint8 lchip, uint32 lock_id, uint8 mcu_id);
extern int32 drv_usw_mcu_unlock(uint8 lchip, uint32 lock_id, uint8 mcu_id);
/****************************************************************************
 *
* Functions
*
****************************************************************************/
int32
sys_tsingma_mcu_write_glb_info(uint8 lchip, uint8 core_id)
{
    uint32 glb_ctl_addr[9][3] = {
        {SYS_TSINGMA_MCU_0_CORE_ID_ADDR,      SYS_TSINGMA_MCU_1_CORE_ID_ADDR,      SYS_TSINGMA_MCU_2_CORE_ID_ADDR},
        {SYS_TSINGMA_MCU_0_SWITCH_CTL_ADDR,   SYS_TSINGMA_MCU_1_SWITCH_CTL_ADDR,   SYS_TSINGMA_MCU_2_SWITCH_CTL_ADDR},
        {SYS_TSINGMA_MCU_0_QM_CHOICE_ADDR,    SYS_TSINGMA_MCU_1_QM_CHOICE_ADDR,    SYS_TSINGMA_MCU_2_QM_CHOICE_ADDR},
        {SYS_TSINGMA_MCU_0_DBG_LOG_CTL_ADDR,  SYS_TSINGMA_MCU_1_DBG_LOG_CTL_ADDR,  SYS_TSINGMA_MCU_2_DBG_LOG_CTL_ADDR},
        {SYS_TSINGMA_MCU_0_RX_EQ_LT_ADDR,     SYS_TSINGMA_MCU_1_RX_EQ_LT_ADDR,     SYS_TSINGMA_MCU_2_RX_EQ_LT_ADDR},
        {SYS_TSINGMA_MCU_0_ISCAN_MIN_VAL_THD, SYS_TSINGMA_MCU_1_ISCAN_MIN_VAL_THD, SYS_TSINGMA_MCU_2_ISCAN_MIN_VAL_THD},
        {SYS_TSINGMA_MCU_0_ISCAN_MODE_CTL,    SYS_TSINGMA_MCU_1_ISCAN_MODE_CTL,    SYS_TSINGMA_MCU_2_ISCAN_MODE_CTL}, 
        {SYS_TSINGMA_MCU_0_EYE_DRIFT_TOLERANCE, SYS_TSINGMA_MCU_1_EYE_DRIFT_TOLERANCE, SYS_TSINGMA_MCU_2_EYE_DRIFT_TOLERANCE}, 
        {SYS_TSINGMA_MCU_0_DFE_OPR_IND,       SYS_TSINGMA_MCU_1_DFE_OPR_IND,       SYS_TSINGMA_MCU_2_DFE_OPR_IND}
    };
    uint32 value[9] = {
        0,  //core id (default 0, need change)
        1,  //switch set 1
        0,  //qm choice (default 0, need change)
        2,  //record fatal log
        1,  //RX eq after LT
        25, //minimun iscan fine value
        4,  //EyeH+EyeW
        3,  //Eye drift tolerance
        2   //DFE operation ind
    };
    uint8 step = 0;
    LCHIP_CHECK(lchip);
    
    if(NULL == p_usw_datapath_master[lchip])
    {
        return CTC_E_INVALID_PTR;
    }
    if(2 < core_id)
    {
        return CTC_E_INVALID_PARAM;
    }

    value[0] = core_id;
    value[2] = p_usw_datapath_master[lchip]->qm_choice.muxmode;
    value[2] = ((value[2])<<8)|(p_usw_datapath_master[lchip]->qm_choice.qmmode);
    value[2] = ((value[2])<<8)|(p_usw_datapath_master[lchip]->qm_choice.txqm3);
    value[2] = ((value[2])<<8)|(p_usw_datapath_master[lchip]->qm_choice.txqm1);
    if(1 == g_tm_dfe_opr_switch)
    {
        value[8] = 1;
    }

    for(step = 0; step < 9; step++)
    {
        CTC_ERROR_RETURN(sys_usw_mcu_chip_write(lchip, glb_ctl_addr[step][core_id], value[step]));
    }

    return CTC_E_NONE;
}

int32
sys_tsingma_mcu_get_init_info(uint8 core_id, uint32 *prog_mem_addr, uint32 *data_mem_addr, uint32 *mcu_proc_len, uint32 **p_tsingma_mcu_proc)
{
    uint32 tmp_prog_mem_addr;
    uint32 tmp_data_mem_addr;
    uint32 tmp_mcu_proc_len;
    uint32 *p_tmp_tsingma_mcu_proc;
    
    switch(core_id)
    {
        case 0:
            tmp_prog_mem_addr = SYS_TSINGMA_MCU_SUP_PROG_MEM_0_ADDR;
            tmp_data_mem_addr = SYS_TSINGMA_MCU_SUP_DATA_MEM_0_ADDR;
            break;
        case 1:
            tmp_prog_mem_addr = SYS_TSINGMA_MCU_SUP_PROG_MEM_1_ADDR;
            tmp_data_mem_addr = SYS_TSINGMA_MCU_SUP_DATA_MEM_1_ADDR;
            break;
        case 2:
            tmp_prog_mem_addr = SYS_TSINGMA_MCU_SUP_PROG_MEM_2_ADDR;
            tmp_data_mem_addr = SYS_TSINGMA_MCU_SUP_DATA_MEM_2_ADDR;
            break;
        case 3:
            tmp_prog_mem_addr = SYS_TSINGMA_MCU_SUP_PROG_MEM_3_ADDR;
            tmp_data_mem_addr = SYS_TSINGMA_MCU_SUP_DATA_MEM_3_ADDR;
            break;
        default:
            return CTC_E_INIT_FAIL;
    }
    p_tmp_tsingma_mcu_proc = g_tsingma_mcu_firmware;
    tmp_mcu_proc_len = sizeof(g_tsingma_mcu_firmware)/sizeof(g_tsingma_mcu_firmware[0]);
    
    if(prog_mem_addr)       *prog_mem_addr      = tmp_prog_mem_addr;
    if(data_mem_addr)       *data_mem_addr      = tmp_data_mem_addr;
    if(mcu_proc_len)        *mcu_proc_len       = tmp_mcu_proc_len;
    if(p_tsingma_mcu_proc)  *p_tsingma_mcu_proc = p_tmp_tsingma_mcu_proc;
    
    return CTC_E_NONE;
}

int32
sys_tsingma_mcu_clear_data_mem(uint8 lchip, uint32 data_mem_addr)
{
    uint32 cnt;
    uint32 max_cnt = SYS_TSINGMA_MCU_DATA_MEM_LEN;

    for (cnt = 0; cnt < max_cnt; cnt++)
    {
        sys_usw_mcu_chip_write(lchip, (data_mem_addr + cnt), 0);
    }

    return CTC_E_NONE;
}

int32
sys_tsingma_mcu_download_program(uint8 lchip, uint32 *p_tsingma_mcu_proc, uint32 prog_mem_addr, uint32 mcu_proc_len)
{
    uint32 cnt;

    for (cnt = 0; cnt < mcu_proc_len + 1; cnt++)
    {
        CTC_ERROR_RETURN(sys_usw_mcu_chip_write(lchip, (prog_mem_addr + cnt*4), p_tsingma_mcu_proc[cnt]));
    }

    return CTC_E_NONE;
}

int32
sys_tsingma_mcu_write_serdes_info(uint8 lchip, uint8 inner_serdes_id, uint8 core_id)
{
    uint32 gport = 0;
    uint16 lport = 0;
    uint16 serdes_id = 8 * core_id + inner_serdes_id;
    uint32 serdes_info_addr = inner_serdes_id*SYS_TSINGMA_MCU_SERDES_ALLOC_BYTE;
    uint32 serdes_info = 0;
    sys_datapath_lport_attr_t* port_attr = NULL;
    
    if(inner_serdes_id >= 8)
    {
        return CTC_E_INVALID_PARAM;
    }
    
    CTC_ERROR_RETURN(sys_usw_datapath_get_gport_with_serdes(lchip, serdes_id, &gport));
    lport = CTC_MAP_GPORT_TO_LPORT(gport);
    CTC_ERROR_RETURN(sys_usw_mac_get_port_capability(lchip, lport, &port_attr));
    if((port_attr->port_type != SYS_DMPS_NETWORK_PORT))
    {
        return CTC_E_INVALID_PARAM;
    }

    switch(core_id)
    {
        case 0:
            serdes_info_addr += SYS_TSINGMA_MCU_0_SERDES_INFO_BASE_ADDR;
            break;
        case 1:
            serdes_info_addr += SYS_TSINGMA_MCU_1_SERDES_INFO_BASE_ADDR;
            break;
        case 2:
            serdes_info_addr += SYS_TSINGMA_MCU_2_SERDES_INFO_BASE_ADDR;
            break;
        case 3:
            serdes_info_addr += SYS_TSINGMA_MCU_3_SERDES_INFO_BASE_ADDR;
            break;
        default:
            return CTC_E_INVALID_PARAM;
    }

    MCU_SET_U8_FIELD(0, serdes_info, inner_serdes_id); //tsingma_mcu_serdes_attr_t.inner_serdes_id
    MCU_SET_U8_FIELD(2, serdes_info, 1); //tsingma_mcu_serdes_attr_t.valid_flag
    MCU_SET_U8_FIELD(3, serdes_info, 0);
    CTC_ERROR_RETURN(sys_usw_mcu_chip_write(lchip, serdes_info_addr, serdes_info));

    return CTC_E_NONE;
}

/*
Set enable/disable of IEEE 802.3 clause 72 link training.
Two directions - train local and train remote are set enable/disable together.
*/
int32
sys_tsingma_mcu_set_cl72_enable(uint8 lchip, uint8 hss_id, uint8 lane_id, bool enable)
{
    uint32 serdes_info_addr = 0;
    uint32 serdes_info_data = 0;
    uint32 mode_ctl_addr    = 0;
    uint32 mode_ctl_val     = 0;
    uint32 restart_addr    = 0;
    uint32 restart_val     = 0;
    uint32 restart_mask    = 0;
    uint32 intr_val  = 0;
    uint32 intr_addr = 0;
    uint32 time_cnt = 1000000;

    if(lane_id >= 8)
    {
        return CTC_E_INVALID_PARAM;
    }

    switch(hss_id)
    {
        case 0:
            serdes_info_addr = SYS_TSINGMA_MCU_0_SERDES_INFO_BASE_ADDR + lane_id*SYS_TSINGMA_MCU_SERDES_ALLOC_BYTE + 4;
            mode_ctl_addr    = SYS_TSINGMA_MCU_0_SW_MODE_CTL_ADDR;
            restart_addr     = SYS_TSINGMA_MCU_0_LT_RESTART_LANE;
            intr_addr        = 0x00483340;
            break;
        case 1:
            serdes_info_addr = SYS_TSINGMA_MCU_1_SERDES_INFO_BASE_ADDR + lane_id*SYS_TSINGMA_MCU_SERDES_ALLOC_BYTE + 4;
            mode_ctl_addr    = SYS_TSINGMA_MCU_1_SW_MODE_CTL_ADDR;
            restart_addr     = SYS_TSINGMA_MCU_1_LT_RESTART_LANE;
            intr_addr        = 0x00501340;
            break;
        case 2:
            serdes_info_addr = SYS_TSINGMA_MCU_2_SERDES_INFO_BASE_ADDR + lane_id*SYS_TSINGMA_MCU_SERDES_ALLOC_BYTE + 4;
            mode_ctl_addr    = SYS_TSINGMA_MCU_2_SW_MODE_CTL_ADDR;
            restart_addr     = SYS_TSINGMA_MCU_2_LT_RESTART_LANE;
            intr_addr        = 0x00584340;
            break;
        default:
            return CTC_E_INVALID_PARAM;
    }
    restart_mask = ((uint32)0x1) << lane_id;

    /*1. mask/unmask LT mcu interrupt*/
    CTC_ERROR_RETURN(sys_usw_mcu_chip_read(lchip, mode_ctl_addr, &mode_ctl_val));
    if(NORMAL_MODE != mode_ctl_val)
    {
        CTC_ERROR_RETURN(sys_usw_mcu_chip_write(lchip, mode_ctl_addr, NORMAL_MODE));
    }
    if(enable)
    {
        /*1.1 enable local FFE & iscan*/
        /*CTC_ERROR_RETURN(sys_tsingma_mcu_set_tx_adj_enable(lchip, hss_id, lane_id, TRUE));
        CTC_ERROR_RETURN(sys_tsingma_mcu_set_lt_initial_value(lchip, hss_id, lane_id));*/
        /*CTC_ERROR_RETURN(sys_tsingma_mcu_set_iscan_enable(lchip, hss_id, lane_id, TRUE));*/

        /*1.2 unmask interrupt*/
        intr_addr += SYS_TSINGMA_MCU_WORD_BYTE*4*3; //entry 3: unmask intr
        if(5 > lane_id)
        {
            intr_addr += 2*SYS_TSINGMA_MCU_WORD_BYTE; //word 2 for lane 0~4
            intr_val   = (uint32)(0x38 << lane_id * 6);
        }
        else
        {
            intr_addr += 3*SYS_TSINGMA_MCU_WORD_BYTE; //word 3 for lane 5~7
            intr_val   = (uint32)(0xe << (lane_id-5) * 6);
        }
        CTC_ERROR_RETURN(sys_usw_mcu_chip_write(lchip, intr_addr, intr_val));
    }
    else
    {
        /*mask interrupt*/
        intr_addr += SYS_TSINGMA_MCU_WORD_BYTE*4*2; //entry 2: mask intr
        if(5 > lane_id)
        {
            intr_addr += 2*SYS_TSINGMA_MCU_WORD_BYTE; //word 2 for lane 0~4
            intr_val   = (uint32)(0x38 << lane_id * 6);
        }
        else
        {
            intr_addr += 3*SYS_TSINGMA_MCU_WORD_BYTE; //word 3 for lane 5~7
            intr_val   = (uint32)(0xe << (lane_id-5) * 6);
        }
        CTC_ERROR_RETURN(sys_usw_mcu_chip_write(lchip, intr_addr, intr_val));
    }
    
    /*2. set train_remote_en and train_local_en
         for disable scene, first set lt_reset_flag as 1 */
    if(FALSE == enable)
    {
        CTC_ERROR_RETURN(sys_usw_mcu_chip_read(lchip, serdes_info_addr, &serdes_info_data));
        if(0 != (serdes_info_data & 0xffff)) //if train_remote_en and train_local_en is 0, do not set reset flag
        {
            CTC_ERROR_RETURN(sys_usw_mcu_chip_read(lchip, restart_addr, &restart_val));
            if(0 != restart_val)
            {
                SYS_CL73_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "MCU restart value %u error!\n", restart_val);
                return CTC_E_HW_BUSY;
            }
            restart_val |= restart_mask; //set restart flag
            CTC_ERROR_RETURN(sys_usw_mcu_chip_write(lchip, restart_addr, restart_val));
        }
    }
    CTC_ERROR_RETURN(sys_usw_mcu_chip_read(lchip, serdes_info_addr, &serdes_info_data));
    MCU_SET_U8_FIELD(0, serdes_info_data, (uint8)enable); //tsingma_mcu_serdes_attr_t.train_remote_en
    MCU_SET_U8_FIELD(1, serdes_info_data, (uint8)enable); //tsingma_mcu_serdes_attr_t.train_local_en
    CTC_ERROR_RETURN(sys_usw_mcu_chip_write(lchip, serdes_info_addr, serdes_info_data));

    /*if disable, waiting for MCU clear restart flag*/
    if(FALSE == enable)
    {
        while(--time_cnt)
        {
            CTC_ERROR_RETURN(sys_usw_mcu_chip_read(lchip, restart_addr, &restart_val));
            if(0 == (restart_val & restart_mask))
            {
                break;
            }
        }
        if(!time_cnt)
        {
            SYS_CL73_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "MCU restart timeout!\n");
            return CTC_E_HW_TIME_OUT;
        }
    }

    return CTC_E_NONE;
}

int32
sys_tsingma_mcu_init_single_core(uint8 lchip, uint8 core_id)
{
    uint8  inner_serdes_id;
    uint32 tbl_id = 0;
    uint32 fld_id = 0;
    uint32 cmd = 0;
    uint32 tmp_val32 = 0;
    uint32 *p_tsingma_mcu_proc = NULL;
    uint32 mcu_proc_len = 0;
    uint32 prog_mem_addr = 0;
    uint32 data_mem_addr = 0;

    /*HSS28G process*/
    if(((0 == SDK_WORK_PLATFORM) && (0 == SDK_WORK_ENV)) && (3 == core_id))
    {
        return CTC_E_NONE;
    }

    /*HSS12G process*/
    tbl_id = McuSupInterruptNormal0_t + core_id;
    fld_id = McuSupInterruptNormal0_mcuSupDataMemEccError_f;
    tmp_val32 = 1;
    cmd = DRV_IOW(tbl_id, fld_id);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, 2, cmd, &tmp_val32)); ///TODO: Is this entry right?

    tbl_id = McuSupRstCtl0_t + core_id;
    fld_id = McuSupRstCtl0_mcuSoftRst_f;
    cmd = DRV_IOR(tbl_id, fld_id);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &tmp_val32));
    if (!tmp_val32)
    {
        return CTC_E_NONE;
    }
    
    CTC_ERROR_RETURN(sys_tsingma_mcu_get_init_info(core_id, &prog_mem_addr, &data_mem_addr, &mcu_proc_len, &p_tsingma_mcu_proc));

    CTC_ERROR_RETURN(sys_tsingma_mcu_clear_data_mem(lchip, data_mem_addr));

    CTC_ERROR_RETURN(sys_tsingma_mcu_download_program(lchip, p_tsingma_mcu_proc, prog_mem_addr, mcu_proc_len));

    for(inner_serdes_id = 0; inner_serdes_id < 8; inner_serdes_id++)
    {
        sys_tsingma_mcu_write_serdes_info(lchip, inner_serdes_id, core_id);
    }

    /* write mcu global info & sw enable */
    CTC_ERROR_RETURN(sys_tsingma_mcu_write_glb_info(lchip, core_id));
 
    tbl_id = McuSupRstCtl0_t + core_id;
    fld_id = McuSupRstCtl0_mcuSoftRst_f;
    tmp_val32 = 0;
    cmd = DRV_IOW(tbl_id, fld_id);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &tmp_val32));

    return CTC_E_NONE;
}

int32
sys_tsingma_mcu_init(uint8 lchip)
{
    uint8 core_id;
    sys_datapath_hss_attribute_t *p_hss = NULL;

    for(core_id = 0; core_id < SYS_TSINGMA_MCU_MAX_NUM; core_id++)
    {
        CTC_ERROR_RETURN(sys_usw_datapath_get_hss_info(lchip, core_id, &p_hss));
        if (NULL == p_hss)
        {
            continue;
        }
        if(CTC_E_NONE != sys_tsingma_mcu_init_single_core(lchip, core_id))
        {
            sal_printf("sys_tsingma_mac_mcu_init: MCU init fail! core_id %d\n", core_id);
        }
    }

    return CTC_E_NONE;
}

int32
sys_tsingma_mcu_show_debug_info(uint8 lchip)
{
    uint8  core_id = 0;
    uint32 base_addr[SYS_TSINGMA_MCU_MAX_NUM]        = {SYS_TSINGMA_MCU_SUP_DATA_MEM_0_ADDR, 
                                                        SYS_TSINGMA_MCU_SUP_DATA_MEM_1_ADDR, 
                                                        SYS_TSINGMA_MCU_SUP_DATA_MEM_2_ADDR, 
                                                        SYS_TSINGMA_MCU_SUP_DATA_MEM_3_ADDR};
    uint32 port_info_addr[SYS_TSINGMA_MCU_MAX_NUM]   = {SYS_TSINGMA_MCU_0_PORT_INFO_BASE_ADDR,
                                                        SYS_TSINGMA_MCU_1_PORT_INFO_BASE_ADDR,
                                                        SYS_TSINGMA_MCU_2_PORT_INFO_BASE_ADDR,
                                                        SYS_TSINGMA_MCU_3_PORT_INFO_BASE_ADDR};
    uint32 serdes_info_addr[SYS_TSINGMA_MCU_MAX_NUM] = {SYS_TSINGMA_MCU_0_SERDES_INFO_BASE_ADDR,
                                                        SYS_TSINGMA_MCU_1_SERDES_INFO_BASE_ADDR,
                                                        SYS_TSINGMA_MCU_2_SERDES_INFO_BASE_ADDR,
                                                        SYS_TSINGMA_MCU_3_SERDES_INFO_BASE_ADDR};
    
    sys_datapath_hss_attribute_t *p_hss = NULL;
    
    uint32 mode_ctl = 0;
    //uint32 port_pos;
    uint32 serdes_pos;
    uint32 tmp_addr = 0;
    uint32 port_info[4] = {0};
    uint32 serdes_info[8] = {0};
    uint8  word_cnt = 0;
    uint32 addr = 0;
    char*  item[11] = {"Core ID", "Switch Controller", "Switch status", "QM choice", "Current Heartbeat", "Interrupt counter", 
                       "Software mode", "Test mode controller", "Test mode status", "Test lock success", "Test lock fail"};
    uint8  item_idx;
    //tsingma_mcu_port_attr_t port = {0};
    tsingma_mcu_serdes_attr_t serdes = {0};

    for(core_id = 0; core_id < (SYS_TSINGMA_MCU_MAX_NUM-1); core_id++)
    {
        /*if(0 != core_id)
        {
            continue;
        }*/
        CTC_ERROR_RETURN(sys_usw_datapath_get_hss_info(lchip, core_id, &p_hss));
        if (NULL == p_hss)
        {
            continue;;
        }
        SYS_MAC_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n----------------------------------------- MCU %u info -----------------------------------------\n", core_id);
        for(item_idx = 0; item_idx < 11; item_idx++)
        {
            addr = ((0 == item_idx) ? (item_idx) : ((4<item_idx) ? (item_idx+2) : (item_idx+1))) * SYS_TSINGMA_MCU_WORD_BYTE
                   + base_addr[core_id];
            CTC_ERROR_RETURN(sys_usw_mcu_chip_read(lchip, addr, &mode_ctl));
            if(3 == item_idx)
            {
                continue;
            }
            if((6 == item_idx) && (TEST_MODE != mode_ctl))
            {
                break;
            }
            SYS_MAC_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%s: %u\n", item[item_idx], mode_ctl);
        }
        /*LT info*/
        SYS_MAC_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Link training info: \n");
        SYS_MAC_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "----------------------------------------------------------------------------------------------------\n");
        SYS_MAC_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, " %-2s%-4s%-6s%-4s%-4s%-5s%-4s%-4s%-5s%-5s| %-5s%-5s%-4s%-4s%-5s%-3s%-6s%-5s%-4s%-4s%-3s%-3s\n", "L", "Stg", "CfVal", 
                        "Frm", "Age", "txmg", "dly", "adv", "CfUp", "CfSt",   "EnLT", "LdDn", "Inc", "ISC", "MISC", "SP", "LpSt0", "Stop", "Rst", "REn", "SS", "RX");
        SYS_MAC_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "----------------------------------------------------------------------------------------------------\n");

        for (serdes_pos = 0; serdes_pos < SYS_TSINGMA_MCU_MAX_SERDES_PER_CORE; serdes_pos++)
        {
            tmp_addr = serdes_info_addr[core_id]+serdes_pos*SYS_TSINGMA_MCU_SERDES_ALLOC_BYTE;
            for(word_cnt = 0; word_cnt < 8; word_cnt++)
            {
                CTC_ERROR_RETURN(sys_usw_mcu_chip_read(lchip, tmp_addr, &serdes_info[word_cnt]));
                tmp_addr += SYS_TSINGMA_MCU_WORD_BYTE;
                /*sal_printf("0x%x\n", serdes_info[word_cnt]);*/
            }
            MCU_GET_U8_FIELD(2, serdes_info[0], serdes.valid_flag);
            if(0 == serdes.valid_flag)
            {
                continue;
            }
            MCU_GET_U8_FIELD(0, serdes_info[0], serdes.inner_serdes_id);
            MCU_GET_U8_FIELD(3, serdes_info[0], serdes.lt_stage);
            MCU_GET_U8_FIELD(0, serdes_info[1], serdes.train_remote_en);
            MCU_GET_U8_FIELD(1, serdes_info[1], serdes.train_local_en);
            MCU_GET_U8_FIELD(2, serdes_info[1], serdes.loc_done_flag);
            MCU_GET_U8_FIELD(3, serdes_info[1], serdes.coeff_stat);
            MCU_GET_U8_FIELD(0, serdes_info[2], serdes.cur_loc_state);
            /*MCU_GET_U8_FIELD(2, serdes_info[2], serdes.rmt_rcv_cnt);
            MCU_GET_U8_FIELD(3, serdes_info[2], serdes.rmt_rcv_info);*/
            MCU_GET_U8_FIELD(0, serdes_info[3], serdes.coeff_val);
            MCU_GET_U8_FIELD(1, serdes_info[3], serdes.inc_flag);
            MCU_GET_U8_FIELD(2, serdes_info[3], serdes.max_iscan_val);
            MCU_GET_U8_FIELD(3, serdes_info[3], serdes.max_iscan_coeff);
            MCU_GET_U8_FIELD(0, serdes_info[4], serdes.txmargin);
            MCU_GET_U8_FIELD(1, serdes_info[4], serdes.pcs_tap_dly);
            MCU_GET_U8_FIELD(2, serdes_info[4], serdes.pcs_tap_adv);
            MCU_GET_U8_FIELD(3, serdes_info[4], serdes.lp_status0);
            serdes.loc_adj_value = serdes_info[5];
            MCU_GET_U8_FIELD(0, serdes_info[6], serdes.cur_iscan_val);
            MCU_GET_U8_FIELD(1, serdes_info[6], serdes.intr_lt_stop);
            MCU_GET_U8_FIELD(2, serdes_info[6], serdes.ageing_cnt);
            MCU_GET_U8_FIELD(3, serdes_info[6], serdes.lt_reset_flag);
            MCU_GET_U8_FIELD(0, serdes_info[7], serdes.rxeq_en);
            MCU_GET_U8_FIELD(1, serdes_info[7], serdes.cur_step);
            MCU_GET_U8_FIELD(2, serdes_info[7], serdes.cur_stage);
            CTC_ERROR_RETURN(sys_usw_mcu_chip_read(lchip, (port_info_addr[core_id] + (serdes.port_pos)*SYS_TSINGMA_MCU_PORT_ALLOC_BYTE), 
                                                   &(port_info[0])));
            SYS_MAC_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, " %-2d%-4d%-6d%-4d%-4d%-5d%-4d%-4d0x%-3x0x%-3x| %-5d%-5d%-4d%-4d%-5d%-3d0x%-4x%-5d%-4d%-4d%-3d%-3d\n", serdes.inner_serdes_id, 
                            serdes.lt_stage, serdes.coeff_val, serdes.cur_loc_state, serdes.ageing_cnt, serdes.txmargin, serdes.pcs_tap_dly, serdes.pcs_tap_adv, 
                            serdes.loc_adj_value, serdes.coeff_stat,  serdes.train_local_en, serdes.loc_done_flag, serdes.inc_flag, serdes.cur_iscan_val, serdes.max_iscan_val, 
                            serdes.cur_step, serdes.lp_status0, serdes.intr_lt_stop, serdes.lt_reset_flag, serdes.train_remote_en, serdes.cur_stage, serdes.rxeq_en);
        }
        SYS_MAC_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "----------------------------------------------------------------------------------------------------\n\n");
        
    }
    return CTC_E_NONE;
}


#ifdef _TSINGMA_MCU_DEBUG_
void sys_tsingma_dump_mcu_dbg_log(uint8 core_id)
{
    sal_file_t mcu_dbg_log = NULL;
    uint8  intr_flag = TRUE;
    uint32 mem_addr;
    uint32 value = 0;
    uint32 max_mem_addr;
    char   file_name[20];
    uint8  prv_err_code  = 0;
    uint32 mem_start_addr[SYS_TSINGMA_MCU_MAX_NUM] = {SYS_TSINGMA_MCU_SUP_DATA_MEM_0_ADDR+0x700,
                                                      SYS_TSINGMA_MCU_SUP_DATA_MEM_1_ADDR+0x700,
                                                      SYS_TSINGMA_MCU_SUP_DATA_MEM_2_ADDR+0x700,
                                                      SYS_TSINGMA_MCU_SUP_DATA_MEM_3_ADDR+0x700};
    
    sal_snprintf(file_name, 20, "./mcu_%u_dbg_log.csv", core_id);
    mcu_dbg_log = sal_fopen(file_name, "w+");
    if(NULL == mcu_dbg_log)
    {
        return;
    }

    max_mem_addr = 0xc100 + mem_start_addr[core_id];
    
    sys_usw_mcu_chip_read(0, mem_start_addr[core_id], &value);
    sal_fprintf(mcu_dbg_log, "Current interrupt log address: 0x%08x\n", value);
    sys_usw_mcu_chip_read(0, mem_start_addr[core_id]+0x2000, &value);
    sal_fprintf(mcu_dbg_log, "Current main loop log address: 0x%08x\n", value);
        
    sal_fprintf(mcu_dbg_log, "MCU ID,Intr flag,Address,Code,Heartbeat,Value,Key info\n");
    for(mem_addr = mem_start_addr[core_id]+SYS_TSINGMA_MCU_WORD_BYTE; mem_addr < max_mem_addr; mem_addr += SYS_TSINGMA_MCU_WORD_BYTE)
    {
        if(0x2000 == mem_addr - mem_start_addr[core_id])
        {
            intr_flag = FALSE;
            continue;
        }
        sys_usw_mcu_chip_read(0, mem_addr, &value);
        sal_fprintf(mcu_dbg_log, "%u,%u,0x%08x,%u,%u,%u,", 
                    core_id, intr_flag, mem_addr, 
                    (uint8)value, (uint16)(value>>8), (uint8)(value>>24));
        /*immportant info print*/
        switch((uint8)value)
        {
            case 15:
            case 16:
                sal_fprintf(mcu_dbg_log, "lane %u,", (uint8)(value>>24));
                break;
            case 6:
                if(5 == (uint8)(value>>24))
                {
                    sal_fprintf(mcu_dbg_log, "RX ready,");
                }
                break;
            case 27:
            case 28:
                if((uint8)value != prv_err_code)
                {
                    switch((uint8)(value>>24))
                    {
                        case 1:
                            sal_fprintf(mcu_dbg_log, "-1 inc,");
                            break;
                        case 2:
                            sal_fprintf(mcu_dbg_log, "-1 dec,");
                            break;
                        case 3:
                            sal_fprintf(mcu_dbg_log, "-1 unknow,");
                            break;
                        case 4:
                            sal_fprintf(mcu_dbg_log, "0 inc,");
                            break;
                        case 8:
                            sal_fprintf(mcu_dbg_log, "0 dec,");
                            break;
                        case 12:
                            sal_fprintf(mcu_dbg_log, "0 unknow,");
                            break;
                        case 16:
                            sal_fprintf(mcu_dbg_log, "+1 inc,");
                            break;
                        case 32:
                            sal_fprintf(mcu_dbg_log, "+1 dec,");
                            break;
                        case 48:
                            sal_fprintf(mcu_dbg_log, "+1 unknow,");
                            break;
                        default:
                            break;
                    }
                }
                else
                {
                    if(((28 == (uint8)value) && (1 == (uint16)(value>>8))) || 
                       ((27 == (uint8)value) && (16 == (uint16)(value>>8))))
                    {
                        sal_fprintf(mcu_dbg_log, "init,");
                    }
                    else if(((28 == (uint8)value) && (2 == (uint16)(value>>8))) || 
                            ((27 == (uint8)value) && (32 == (uint16)(value>>8))))
                    {
                        sal_fprintf(mcu_dbg_log, "preset,");
                    }
                }
                break;
            case 89:
            case 13:
                if((uint8)value != prv_err_code)
                {
                    switch((uint8)(value>>24))
                    {
                        case 1:
                            sal_fprintf(mcu_dbg_log, "-1 upd,");
                            break;
                        case 2:
                            sal_fprintf(mcu_dbg_log, "-1 min,");
                            break;
                        case 3:
                            sal_fprintf(mcu_dbg_log, "-1 max,");
                            break;
                        case 4:
                            sal_fprintf(mcu_dbg_log, "0 upd,");
                            break;
                        case 8:
                            sal_fprintf(mcu_dbg_log, "0 min,");
                            break;
                        case 12:
                            sal_fprintf(mcu_dbg_log, "0 max,");
                            break;
                        case 16:
                            sal_fprintf(mcu_dbg_log, "+1 upd,");
                            break;
                        case 32:
                            sal_fprintf(mcu_dbg_log, "+1 min,");
                            break;
                        case 48:
                            sal_fprintf(mcu_dbg_log, "+1 max,");
                            break;
                        case 21:
                            sal_fprintf(mcu_dbg_log, "all upd,");
                            break;
                        case 63:
                            sal_fprintf(mcu_dbg_log, "all max,");
                            break;
                        default:
                            break;
                    }
                }
                break;
            case 19:
                sal_fprintf(mcu_dbg_log, "RX ready,");
                break;
            default:
                sal_fprintf(mcu_dbg_log, "omit,");
                break;
        }

        sal_fprintf(mcu_dbg_log, "\n");

        prv_err_code  = (uint8)value;
    }
    
    sal_fclose(mcu_dbg_log);
}
#endif



