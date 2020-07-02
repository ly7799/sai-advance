#include "sal.h"
#include "ctc_cli.h"
#include "usw/include/drv_enum.h"
#include "drv_api.h"
#include "usw/include/drv_common.h"
#include "usw/include/drv_chip_ctrl.h"
#include "ctc_dkit.h"
#include "ctc_dkit_common.h"
#include "ctc_usw_dkit.h"
#include "ctc_usw_dkit_interface.h"

extern ctc_dkit_master_t* g_usw_dkit_master[CTC_DKITS_MAX_LOCAL_CHIP_NUM];
#define SYS_DKIT_TSINGMA_SENSOR_TIMEOUT 1000
#define SYS_DKIT_TSINGMA_TEMP_TABLE_NUM 166

STATIC int32
_ctc_tm_dkit_get_temp_with_code(uint8 lchip, uint32 temp_code, uint32* p_temp_val)
{
    uint16 temp_mapping_tbl[SYS_DKIT_TSINGMA_TEMP_TABLE_NUM] = {
        804, 801, 798, 795, 792, 790, 787, 784, 781, 778, 775, 772, 769, 766, 763, 761, 758, 755, 752, 749, /*-40~-21*/
        746, 743, 740, 737, 734, 731, 728, 725, 722, 719, 717, 714, 711, 708, 705, 702, 699, 696, 693, 690, /*-20~-1*/
        687, 684, 681, 678, 675, 672, 669, 666, 663, 660, 658, 655, 652, 649, 646, 643, 640, 637, 634, 631, /*0~19*/
        628, 625, 622, 619, 616, 613, 610, 607, 604, 601, 599, 596, 593, 590, 587, 584, 581, 578, 575, 572, /*20~39*/
        569, 566, 563, 560, 557, 554, 551, 548, 545, 542, 540, 537, 534, 531, 528, 525, 522, 519, 516, 513, /*40~59*/
        510, 507, 504, 501, 498, 495, 492, 489, 486, 483, 481, 478, 475, 472, 469, 466, 463, 460, 457, 454, /*60~79*/
        451, 448, 445, 442, 439, 436, 433, 430, 427, 424, 421, 418, 415, 412, 409, 406, 403, 400, 397, 394, /*80~99*/
        391, 388, 385, 382, 379, 376, 373, 370, 367, 364, 361, 358, 355, 352, 349, 346, 343, 340, 337, 334, /*100~119*/
        331, 328, 325, 322, 329, 316};                                                                      /*120~125*/
    uint8 index = 0;

    for (index = 0; index< (SYS_DKIT_TSINGMA_TEMP_TABLE_NUM-1); index++)
    {
        if ((temp_code <= temp_mapping_tbl[index]) && (temp_code > temp_mapping_tbl[index+1]))
        {
            break;
        }
    }

    if (index < 39)
    {
        *p_temp_val = 40 - index + (1 << 31);
    }
    else
    {
        *p_temp_val = index - 40;
    }

    return CLI_SUCCESS;
}

STATIC int32
_ctc_tm_dkit_monitor_sensor(uint8 lchip, ctc_dkit_monitor_sensor_t type, uint32* p_value)
{
    uint32 cmd = 0;
    uint32 field_val = 0;
    uint32 timeout = SYS_DKIT_TSINGMA_SENSOR_TIMEOUT;
    uint32 index = 0;

    DKITS_PTR_VALID_CHECK(p_value);
    if ((CTC_DKIT_MONITOR_SENSOR_TEMP != type) && (CTC_DKIT_MONITOR_SENSOR_VOL != type))
    {
        return CLI_ERROR;
    }

    /*config RCLK_EN=1,RAVG_SEL,RCLK_DIV*/
    /*mask_write tbl-reg OmcMem 0xf offset 0x0 0x00001000 0x00001000*/
    index = 0xf;
    DKITS_BIT_SET(field_val, 12);
    field_val = 0x310c7;
    cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));

    /*config RTHMC_RST=1*/
    /*mask_write tbl-reg OmcMem 0x10 offset 0x0 0x00000010 0x00000010*/
    index = 0x10;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));
    DKITS_BIT_SET(field_val, 4);
    cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));

    /*wait RTHMC_RST=1*/
    /*read tbl-reg OmcMem 0x10 offset 0x0*/
    timeout = SYS_DKIT_TSINGMA_SENSOR_TIMEOUT;
    index = 0x10;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    while(timeout)
    {
        timeout--;
        (DRV_IOCTL(lchip, index, cmd, &field_val));
        if (0 == DKITS_IS_BIT_SET(field_val, 4))
        {
            break;
        }
        sal_task_sleep(1);
    }
    if (0 == timeout)
    {
        return CLI_ERROR;
    }

    /*config ENBIAS=1£¬ENVR=1£¬ENAD=1*/
    /*mask_write tbl-reg OmcMem 0x11 offset 0x0 0x02000007 0x03000007*/
    index = 0x11;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));
    DKITS_BIT_SET(field_val, 0);
    DKITS_BIT_SET(field_val, 1);
    DKITS_BIT_SET(field_val, 2);
    DKITS_BIT_UNSET(field_val, 24);
    DKITS_BIT_SET(field_val, 25);
    cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));

    sal_task_sleep(10);

    /*set RTHM_MODE=1,RSAMPLE_DONE_INTEN=1,RDUMMY_THMRD=1,RV_SAMPLE_EN=1*/
    /*mask_write tbl-reg OmcMem 0x10 offset 0x0 0x00000203 0x00000003*/
    /*mask_write tbl-reg OmcMem 0x8 offset 0x0 0x00000004 0x00000004*/
    /*mask_write tbl-reg OmcMem 0x12 offset 0x0 0x00000001 0x00000001*/
    index = 0x10;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));
    DKITS_BIT_SET(field_val, 0);
    DKITS_BIT_SET(field_val, 1);
    DKITS_BIT_SET(field_val, 9);
    cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));
    index = 0x8;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));
    DKITS_BIT_SET(field_val, 2);
    cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));
    index = 0x12;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));
    DKITS_BIT_SET(field_val, 0);
    cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));

    if (CTC_DKIT_MONITOR_SENSOR_TEMP == type)
    {
        /*mask_write tbl-reg OmcMem 0x10 offset 0x0 0x00000001 0x00000001*/
        index = 0x10;
        cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
        (DRV_IOCTL(lchip, index, cmd, &field_val));
        DKITS_BIT_SET(field_val, 0);
        cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
        (DRV_IOCTL(lchip, index, cmd, &field_val));
    }
    else
    {
        /*mask_write tbl-reg OmcMem 0x10 offset 0x0 0x00000000 0x00000001*/
        index = 0x10;
        cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
        (DRV_IOCTL(lchip, index, cmd, &field_val));
        DKITS_BIT_UNSET(field_val, 0);
        cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
        (DRV_IOCTL(lchip, index, cmd, &field_val));
    }
    sal_task_sleep(10);

    /*mask_write tbl-reg OmcMem 0x12 offset 0x0 0x00000001 0x00000001*/
    index = 0x12;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));
    DKITS_BIT_SET(field_val, 0);
    cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
    (DRV_IOCTL(lchip, index, cmd, &field_val));


    /*Wait((mask_read tbl-reg OmcMem 0xa offset 0x0 0x00000004) =1)*/
    index = 0xa;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    timeout = SYS_DKIT_TSINGMA_SENSOR_TIMEOUT;
    while(timeout)
    {
        timeout--;
        (DRV_IOCTL(lchip, index, cmd, &field_val));
        if (DKITS_IS_BIT_SET(field_val, 2))
        {
            break;
        }
        sal_task_sleep(1);
    }
    if (0 == timeout)
    {
        return CLI_ERROR;
    }

    /*mask_write tbl-reg OmcMem 0x11 offset 0x0 0x00000006 0x00000006*/
    index = 0x11;
    cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
    DRV_IOCTL(lchip, index, cmd, &field_val);
    DKITS_BIT_SET(field_val, 1);
    DKITS_BIT_SET(field_val, 2);
    cmd = DRV_IOW(OmcMem_t, OmcMem_data_f);
    DRV_IOCTL(lchip, index, cmd, &field_val);

    if (CTC_DKIT_MONITOR_SENSOR_TEMP == type)
    {
        /*read-reg OmcMem 0xd offset 0x0*/
        index = 0xd;
        cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
        DRV_IOCTL(lchip, index, cmd, &field_val);
        _ctc_tm_dkit_get_temp_with_code(lchip, field_val, p_value);
    }
    else
    {
        /*read-reg OmcMem 0xc offset 0x0*/
        index = 0xc;
        cmd = DRV_IOR(OmcMem_t, OmcMem_data_f);
        DRV_IOCTL(lchip, index, cmd, &field_val);
        *p_value =  (field_val*1232)/1023;
    }

    return CLI_SUCCESS;
}

STATIC int32
_ctc_tm_dkit_monitor_show_sensor_temperature(uint8 lchip)
{
    uint32 temp_value = 0;
    uint32 ret = 0;

    ret = _ctc_tm_dkit_monitor_sensor(lchip, CTC_DKIT_MONITOR_SENSOR_TEMP, &temp_value);
    if (ret)
    {
        CTC_DKIT_PRINT("Read temperature fail!!!\n");
        return CLI_ERROR;
    }
    CTC_DKIT_PRINT("Temperature is %d C\n", temp_value);

    return CLI_SUCCESS;
}


STATIC int32
_ctc_tm_dkit_monitor_show_sensor_voltage(uint8 lchip)
{
    uint32 vol_value = 0;
    uint32 ret = 0;

    ret = _ctc_tm_dkit_monitor_sensor(lchip, CTC_DKIT_MONITOR_SENSOR_VOL, &vol_value);
    if (ret)
    {
        CTC_DKIT_PRINT("Read voltage fail!!!\n");
        return CLI_ERROR;
    }
    CTC_DKIT_PRINT("Voltage is %d mV\n", vol_value);

    return CLI_SUCCESS;

}

STATIC void
_ctc_tm_dkit_monitor_temperature_handler(void* arg)
{
    uint32 ret = 0;
    sal_time_t tv;
    char* p_time_str = NULL;
    sal_file_t p_file = NULL;
    uint32 temperature = 0;
    ctc_dkit_monitor_para_t* para = (ctc_dkit_monitor_para_t*)arg;
    uint8 lchip = para->lchip;

    if (para->log)
    {
        p_file = sal_fopen(para->str, "wt");
        if (!p_file)
        {
            CTC_DKIT_PRINT("Open file %s fail!!!\n", para->str);
            return;
        }
        sal_fclose(p_file);
        p_file = NULL;
    }

    while (1)
    {
        if (para->log && (NULL == p_file))
        {
            p_file = sal_fopen(para->str, "a");
        }
        /*get systime*/
        sal_time(&tv);
        p_time_str = sal_ctime(&tv);
        ret = _ctc_tm_dkit_monitor_sensor(lchip, CTC_DKIT_MONITOR_SENSOR_TEMP, &temperature);
        if (ret)
        {
            CTC_DKITS_PRINT_FILE(p_file, "Read temperature fail!!\n");
        }
        else if (temperature >= para->temperature)
        {
            CTC_DKITS_PRINT_FILE(p_file, "t = %-4d, %s", temperature, p_time_str);
        }

        if (temperature >= para->power_off_temp) /*power off*/
        {

            CTC_DKITS_PRINT_FILE(p_file, "Power off!!!\n");
            goto END;
        }
        sal_task_sleep(para->interval*1000);

        if(0 == para->enable)
        {
            goto END;
        }
    }

END:
    if (p_file)
    {
        sal_fclose(p_file);
    }
    return;
}

STATIC int32
_ctc_tm_dkit_monitor_temperature(void* p_para)
{
    int ret = 0;
    uint8 lchip = 0;
    uint8 task_id = CTC_DKIT_MONITOR_TASK_TEMPERATURE;
    ctc_dkit_monitor_para_t* p_monitor_para = (ctc_dkit_monitor_para_t*)p_para;
    char buffer[SAL_TASK_MAX_NAME_LEN] = {0};

    DKITS_PTR_VALID_CHECK(p_monitor_para);
    lchip = p_monitor_para->lchip;
    CTC_DKIT_LCHIP_CHECK(lchip);
    DKITS_PTR_VALID_CHECK(g_usw_dkit_master[lchip]);

    if ((p_monitor_para->enable) && (NULL == g_usw_dkit_master[lchip]->monitor_task[task_id].monitor_task))
    {
        if (NULL == g_usw_dkit_master[lchip]->monitor_task[task_id].para)
        {
            g_usw_dkit_master[lchip]->monitor_task[task_id].para
                   = (ctc_dkit_monitor_para_t*)mem_malloc(MEM_CLI_MODULE, sizeof(ctc_dkit_monitor_para_t));
            if (NULL == g_usw_dkit_master[lchip]->monitor_task[task_id].para)
            {
                return CLI_ERROR;
            }
        }
        sal_memcpy(g_usw_dkit_master[lchip]->monitor_task[task_id].para, p_para , sizeof(ctc_dkit_monitor_para_t));

        sal_sprintf(buffer, "Temperature-%d", lchip);
        ret = sal_task_create(&g_usw_dkit_master[lchip]->monitor_task[task_id].monitor_task,
                                                  buffer,
                                                  SAL_DEF_TASK_STACK_SIZE,
                                                  SAL_TASK_PRIO_DEF,
                                                  _ctc_tm_dkit_monitor_temperature_handler,
                                                  g_usw_dkit_master[lchip]->monitor_task[task_id].para);

        if (0 != ret)
        {
            CTC_DKIT_PRINT("Temperature monitor task create fail!\n");
            return CLI_ERROR;
        }
    }
    else if(0 == p_monitor_para->enable)
    {
        if (g_usw_dkit_master[lchip]->monitor_task[task_id].para)
        {
            sal_memset(g_usw_dkit_master[lchip]->monitor_task[task_id].para, 0 ,sizeof(ctc_dkit_monitor_para_t));
        }
        if (g_usw_dkit_master[lchip]->monitor_task[task_id].monitor_task)
        {
            sal_task_destroy(g_usw_dkit_master[lchip]->monitor_task[task_id].monitor_task);
            g_usw_dkit_master[lchip]->monitor_task[task_id].monitor_task = NULL;
        }
    }

    return CLI_SUCCESS;
}

int32
ctc_tm_dkit_monitor_show_sensor_result(void* p_para)
{
    ctc_dkit_monitor_para_t* p_monitor_para = (ctc_dkit_monitor_para_t*)p_para;
    DKITS_PTR_VALID_CHECK(p_para);
    CTC_DKIT_LCHIP_CHECK(p_monitor_para->lchip);
    DRV_INIT_CHECK(p_monitor_para->lchip);

    if (CTC_DKIT_MONITOR_SENSOR_TEMP == p_monitor_para->sensor_mode)
    {
        _ctc_tm_dkit_monitor_show_sensor_temperature(p_monitor_para->lchip);
    }
    else if (CTC_DKIT_MONITOR_SENSOR_VOL == p_monitor_para->sensor_mode)
    {
        _ctc_tm_dkit_monitor_show_sensor_voltage(p_monitor_para->lchip);
    }
    else if(CTC_DKIT_MONITOR_SENSOR_TEMP_NOMITOR == p_monitor_para->sensor_mode)
    {
        _ctc_tm_dkit_monitor_temperature(p_para);
    }

    return CLI_SUCCESS;
}

STATIC int32
_ctc_tm_dkit_interface_get_mac_mode_by_macid(uint8 lchip, uint16 mac_id, uint32* pcs_idx, uint32* mii_idx, ctc_dkit_interface_status_t* status)
{
    uint8  pcs_id    = 0xff;
    uint8  qu_pcs_id = 0xff;
    uint8  mii_id    = 0xff;
    uint32 tbl_id    = 0;
    uint32 fld_id    = 0;
    uint32 step      = 0;
    uint32 val32     = 0;
    uint32 cmd       = 0;
    uint8  lg_id     = 0;
    uint8  internal_mac_id = 0;
    uint32 txqm1mode = 0;
    uint32 txqm3mode = 0;
    RlmMacMuxModeCfg_m mac_cfg;

    lg_id = (mac_id % 4) ? 1 : 0;
    internal_mac_id = mac_id % 4;
    DKITS_PTR_VALID_CHECK(pcs_idx);
    DKITS_PTR_VALID_CHECK(mii_idx);

    *pcs_idx = pcs_id;
    *mii_idx = mii_id;

    tbl_id = RlmMacMuxModeCfg_t;
    cmd = DRV_IOR(tbl_id, DRV_ENTRY_FLAG);
    DRV_IOCTL(lchip, 0, cmd, &mac_cfg);
    DRV_IOR_FIELD(lchip, tbl_id, RlmMacMuxModeCfg_cfgTxqm1Mode_f, &txqm1mode, &mac_cfg);
    DRV_IOR_FIELD(lchip, tbl_id, RlmMacMuxModeCfg_cfgTxqm3Mode_f, &txqm3mode, &mac_cfg);

    /* #1, first check 25G/50G/100G */
    if(CTC_DKITS_TSINGMA_MAC_IS_HSS28G(mac_id, txqm3mode))
    {
        if((mac_id >= 44) && (mac_id <= 47))
        {
            pcs_id = 6;
            mii_id = 15;
        }
        else if ((mac_id >= 60) && (mac_id <= 63))
        {
            pcs_id = 7;
            mii_id = 16;
        }
        *pcs_idx = pcs_id;
        *mii_idx = mii_id;
        tbl_id = SharedPcsCfg0_t + pcs_id;
        fld_id = SharedPcsCfg0_cgMode_f;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_CG;
            return CLI_SUCCESS;
        }

        fld_id = SharedPcsCfg0_xauiMode_f;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_XAUI;  /* XAUI/DXAUI */
            return CLI_SUCCESS;
        }

        fld_id = SharedPcsCfg0_xlgMode_f;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_XLG;
            return CLI_SUCCESS;
        }

        fld_id = SharedPcsCfg0_lgMode0_f + lg_id;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_LG;
            return CLI_SUCCESS;
        }

        fld_id = SharedPcsCfg0_xxvgMode0_f + internal_mac_id;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_XXVG;
            return CLI_SUCCESS;
        }

        fld_id = SharedPcsCfg0_sgmiiModeRx0_f + internal_mac_id;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_SGMII;
            return CLI_SUCCESS;
        }
        status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_XFI;
    }
    else
    {
        if (mac_id <= 3)
        {
            pcs_id = 0;
            qu_pcs_id = 0;
            mii_id = 0;
        }
        else if ((mac_id >= 4) && (mac_id <= 7))
        {
            qu_pcs_id = 1;
            mii_id = 1;
        }
        else if ((mac_id >= 8) && (mac_id <= 11))
        {
            pcs_id = 1;
            qu_pcs_id = 4;
            mii_id = 4;
        }
        else if ((mac_id >= 12) && (mac_id <= 15))
        {
            pcs_id = 3;
            mii_id = 12;
        }
        else if ((mac_id >= 16) && (mac_id <= 19))
        {
            qu_pcs_id = 2;
            mii_id = 2;
        }
        else if ((mac_id >= 20) && (mac_id <= 23))
        {
            if(0 == txqm1mode)
            {
                pcs_id = 2;
                qu_pcs_id = 8;
                mii_id = 8;
            }
            else
            {
                qu_pcs_id = 3;
                mii_id = 3;
            }
        }
        else if ((mac_id >= 24) && (mac_id <= 27))
        {
            if(0 == txqm1mode)
            {
                pcs_id = 4;
                mii_id = 13;
            }
            else
            {
                pcs_id = 2;
                qu_pcs_id = 8;
                mii_id = 8;
            }
        }
        else if ((mac_id >= 28) && (mac_id <= 31))
        {
            if(0 == txqm1mode)
            {
                pcs_id = 4;
                mii_id = 13;
            }
            else if(1 == txqm1mode)
            {
                pcs_id = 5;
                mii_id = 14;
            }
            else
            {
                if(28 == mac_id)
                {
                    pcs_id = 4;
                    mii_id = 13;
                }
                else if(31 == mac_id)
                {
                    pcs_id = 5;
                    mii_id = 14;
                }
            }
        }
        else if ((mac_id >= 32) && (mac_id <= 35))
        {
            qu_pcs_id = 5;
            mii_id = 5;
        }
        else if ((mac_id >= 36) && (mac_id <= 39))
        {
            qu_pcs_id = 6;
            mii_id = 6;
        }
        else if ((mac_id >= 40) && (mac_id <= 43))
        {
            qu_pcs_id = 7;
            mii_id = 7;
        }
        else if ((mac_id >= 48) && (mac_id <= 51))
        {
            qu_pcs_id = 9;
            mii_id = 9;
        }
        else if ((mac_id >= 52) && (mac_id <= 55))
        {
            qu_pcs_id = 10;
            mii_id = 10;
        }
        else if ((mac_id >= 56) && (mac_id <= 59))
        {
            if(1 == txqm3mode)
            {
                pcs_id = 5;
                mii_id = 14;
            }
            else
            {
                qu_pcs_id = 11;
                mii_id = 11;
            }
        }
        else if ((mac_id >= 60) && (mac_id <= 63))
        {
            pcs_id = 5;
            mii_id = 14;
        }
        *pcs_idx = pcs_id;
        *mii_idx = mii_id;
        /*Must not change the order of reading different tables, must obey the priority */
        step = SharedMii1Cfg0_t - SharedMii0Cfg0_t;
        tbl_id = SharedMii0Cfg0_t + internal_mac_id*step + mii_id;
        fld_id = SharedMii0Cfg0_cfgMiiUsxgmiiEn0_f;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_USXGMII0;  /* USXGMII0/1/2 */
            *pcs_idx = qu_pcs_id;
            return CLI_SUCCESS;
        }

        tbl_id = SharedMiiCfg0_t + mii_id;
        fld_id = SharedMiiCfg0_cfgMiiQsgmiiMode_f;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_QSGMII;
            *pcs_idx = qu_pcs_id;
            return CLI_SUCCESS;
        }
        tbl_id = SharedPcsCfg0_t + pcs_id;
        fld_id = SharedPcsCfg0_xauiMode_f;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_XAUI;  /* XAUI/DXAUI */
            return CLI_SUCCESS;
        }

        fld_id = SharedPcsCfg0_xlgMode_f;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_XLG;
            return CLI_SUCCESS;
        }

        fld_id = SharedPcsCfg0_fxMode0_f + internal_mac_id;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_100BASEFX;
            return CLI_SUCCESS;
        }

        fld_id = SharedPcsCfg0_sgmiiModeRx0_f + internal_mac_id;
        cmd = DRV_IOR(tbl_id, fld_id);
        DRV_IOCTL(lchip, 0, cmd, &val32);
        if (val32)
        {
            status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_SGMII;
            return CLI_SUCCESS;
        }

        status->mac_mode[mac_id] = CTC_DKIT_INTERFACE_XFI;
    }

    return CLI_SUCCESS;
}

STATIC int32
_ctc_tm_dkit_interface_mac_to_gport(uint8 lchip,ctc_dkit_interface_status_t* status)
{
    uint32 cmd = 0;
    uint8 mac_id = 0;
    uint32 tbl_id = 0;
    uint8 gchip = 0;
    uint32 channel_id = 0;
    uint32 local_phy_port = 0;
    uint16 gport  = 0;

    CTC_DKIT_GET_GCHIP(lchip, gchip);
    for (mac_id = 0; mac_id < CTC_DKIT_INTERFACE_MAC_NUM; mac_id++)
    {
        /* mac to channel */
        tbl_id = NetRxChannelMap_t;
        cmd = DRV_IOR(tbl_id, NetRxChannelMap_cfgPortToChanMapping_f);
        DRV_IOCTL(lchip, mac_id, cmd, &channel_id);

        /* channel to lport */
        cmd = DRV_IOR(IpeHeaderAdjustPhyPortMap_t, IpeHeaderAdjustPhyPortMap_localPhyPort_f);
        DRV_IOCTL(lchip, channel_id, cmd, &local_phy_port);
        gport = CTC_DKIT_DRV_LPORT_TO_CTC_GPORT(gchip, local_phy_port);
        status->gport[mac_id] = gport;
        status->valid[mac_id] = 1;
    }
    return CLI_SUCCESS;
}

int32
ctc_tm_dkit_interface_show_pcs_status(uint8 lchip, uint16 mac_id)
{
    uint32 mac_index = 0;
    char* mac_mode_str[CTC_DKIT_INTERFACE_MAC_MAX] =
    {
        "SGMII", "XFI", "XAUI", "XLG", "CG", "QSGMII", "DXAUI", "SGMII-2.5G",
        "USXGMII-S", "USXGMII-M5G", "USXGMII-M2.5G", "XXVG", "LG", "100BASE-FX"
    };
    uint32 pcs_idx = 0;
    uint16 step = 0;
    uint8 lg_index = 0;
    uint32 cmd = 0;
    uint8 shared_fec_idx = 0;
    uint32 mii_idx = 0;
    uint8 fec_en = 0;
    uint8 rs_fec = 0;
    ctc_dkit_interface_status_t status = {{0}};
    SharedPcsFecCfg0_m  pcscfg;
    uint32 tbl_id = MaxTblId_t;

    sal_memset(&status, 0 , sizeof(status));
    sal_memset(&pcscfg, 0 , sizeof(pcscfg));

    CTC_DKIT_LCHIP_CHECK(lchip);
    if (mac_id >= CTC_DKIT_INTERFACE_MAC_NUM)
    {
        CTC_DKIT_PRINT("Mac id is out of range 0~%d!!!\n", CTC_DKIT_INTERFACE_MAC_NUM);
        return CLI_ERROR;
    }
    _ctc_tm_dkit_interface_mac_to_gport(lchip, &status);
    _ctc_tm_dkit_interface_get_mac_mode_by_macid(lchip, mac_id, &pcs_idx, &mii_idx, &status);
    if (!status.valid[mac_id])
    {
        CTC_DKIT_PRINT("Mac id %d is unused!\n", mac_id);
        return CLI_ERROR;
    }
    if(0xff == pcs_idx || 0xff == mii_idx)
    {
        CTC_DKIT_PRINT("Mac id %d is invalid!\n", mac_id);
        return CLI_ERROR;
    }

    mac_index = mac_id % 4;
    if (mac_index <= 1)
    {
        lg_index = 0;
    }
    else
    {
        lg_index = 1;
    }
    shared_fec_idx = (mii_idx >=12) ? (mii_idx - 12) : 0;
    tbl_id = SharedPcsFecCfg0_t + pcs_idx;
    cmd = DRV_IOR(tbl_id, DRV_ENTRY_FLAG);
    DRV_IOCTL(lchip, 0, cmd, &pcscfg);
    
    CTC_DKIT_PRINT("Mac ID = %u, Inner ID = %u, PCS index = %u, MII index = %u, LG index = %u, mac_mode %s\n", 
                   mac_id, mac_index, pcs_idx, mii_idx, lg_index, mac_mode_str[status.mac_mode[mac_id]]);

    if (CTC_DKIT_INTERFACE_SGMII == status.mac_mode[mac_id])
    {
        step = SharedPcsSgmii1Status0_t - SharedPcsSgmii0Status0_t;
        tbl_id = SharedPcsSgmii0Status0_t + mac_index*step + pcs_idx;
    }
    else if ((CTC_DKIT_INTERFACE_USXGMII0 == status.mac_mode[mac_id])
        || (CTC_DKIT_INTERFACE_USXGMII1 == status.mac_mode[mac_id])
        || (CTC_DKIT_INTERFACE_USXGMII2 == status.mac_mode[mac_id]))
    {
        step = UsxgmiiPcsXfi1Status0_t - UsxgmiiPcsXfi0Status0_t;
        tbl_id = UsxgmiiPcsXfi0Status0_t + mac_index*step + pcs_idx;
    }
    else if (CTC_DKIT_INTERFACE_QSGMII == status.mac_mode[mac_id])
    {
        GET_QSGMII_TBL(mac_id, pcs_idx, tbl_id);
    }
    else if ((CTC_DKIT_INTERFACE_XAUI == status.mac_mode[mac_id])
        || (CTC_DKIT_INTERFACE_DXAUI == status.mac_mode[mac_id]))
    {
        tbl_id = SharedPcsXauiStatus0_t + pcs_idx;
    }
    else if (CTC_DKIT_INTERFACE_XFI == status.mac_mode[mac_id])
    {
        if (0 == mac_index % 4)
            fec_en = GetSharedPcsFecCfg0(V, xfiPcsFecEn0_f, &pcscfg);
        else if (1 == mac_index % 4)
            fec_en = GetSharedPcsFecCfg0(V, xfiPcsFecEn1_f, &pcscfg);
        else if (2 == mac_index % 4)
            fec_en = GetSharedPcsFecCfg0(V, xfiPcsFecEn2_f, &pcscfg);
        else
            fec_en = GetSharedPcsFecCfg0(V, xfiPcsFecEn3_f, &pcscfg);
        if(fec_en) //baser fec status
        {
            step = SharedPcsXfi1Status0_t - SharedPcsXfi0Status0_t;
            tbl_id = SharedPcsXfi0Status0_t + mac_index*step + pcs_idx;
        }
        else
        {
            step = SharedPcsXfi1Status0_t - SharedPcsXfi0Status0_t;
            tbl_id = SharedPcsXfi0Status0_t + mac_index*step + pcs_idx;
        }
    }
    else if (CTC_DKIT_INTERFACE_XLG == status.mac_mode[mac_id])
    {
        fec_en = GetSharedPcsFecCfg0(V, xlgPcsFecEn_f, &pcscfg);
        if(fec_en) //baser fec status
        {
            step = SharedPcsXfi1Status0_t - SharedPcsXfi0Status0_t;
            tbl_id = SharedPcsXfi0Status0_t + mac_index*step + pcs_idx;
        }
        else
        {
            tbl_id = SharedPcsXlgStatus0_t + pcs_idx;
        }
    }
    else if (CTC_DKIT_INTERFACE_XXVG == status.mac_mode[mac_id])
    {
        if (0 == mac_index % 4)
        {
            fec_en = GetSharedPcsFecCfg0(V, xfiPcsFecEn0_f, &pcscfg);
        }
        else if (1 == mac_index % 4)
        {
            fec_en = GetSharedPcsFecCfg0(V, xfiPcsFecEn1_f, &pcscfg);
        }
        else if (2 == mac_index % 4)
        {
            fec_en = GetSharedPcsFecCfg0(V, xfiPcsFecEn2_f, &pcscfg);
        }
        else
        {
            fec_en = GetSharedPcsFecCfg0(V, xfiPcsFecEn3_f, &pcscfg);
        }
        if (fec_en)
        {
            step = SharedMii1Cfg0_t - SharedMii0Cfg0_t;
            tbl_id = SharedMii0Cfg0_t + mac_index*step + mii_idx;
            cmd = DRV_IOR(tbl_id, SharedMii0Cfg0_cfgMiiTxRsFecEn0_f);
            DRV_IOCTL(lchip, 0, cmd, &rs_fec);
            if(rs_fec)
            {
                step = RsFec1StatusSharedFec0_t - RsFec0StatusSharedFec0_t;
                tbl_id = RsFec0StatusSharedFec0_t + mac_index*step + shared_fec_idx;
            }
            else //baser fec status
            {
                step = SharedPcsXfi1Status0_t - SharedPcsXfi0Status0_t;
                tbl_id = SharedPcsXfi0Status0_t + mac_index*step + pcs_idx;
            }
        }
        else
        {
            step = SharedPcsXfi1Status0_t - SharedPcsXfi0Status0_t;
            tbl_id = SharedPcsXfi0Status0_t + mac_index*step + pcs_idx;
        }
    }
    else if (CTC_DKIT_INTERFACE_LG == status.mac_mode[mac_id])
    {
        if (0 == mac_index % 4)
        {
            fec_en = GetSharedPcsFecCfg0(V, lgPcsFecEn0_f, &pcscfg);
            rs_fec = GetSharedPcsFecCfg0(V, lgPcsFecRsMode0_f, &pcscfg);
        }
        else
        {
            fec_en = GetSharedPcsFecCfg0(V, lgPcsFecEn1_f, &pcscfg);
            rs_fec = GetSharedPcsFecCfg0(V, lgPcsFecRsMode1_f, &pcscfg);
        }
        if(fec_en)
        {
            if(rs_fec)
            {
                step = RsFec1StatusSharedFec0_t - RsFec0StatusSharedFec0_t;
                tbl_id = RsFec0StatusSharedFec0_t + mac_index*step + shared_fec_idx;
            }
            else //baser fec status
            {
                if (0 == lg_index)
                {
                    tbl_id = SharedPcsXlgStatus0_t + pcs_idx;
                }
                else
                {
                    tbl_id = SharedPcsLgStatus0_t + pcs_idx;
                }
            }
        }
        else
        {
            if (0 == lg_index)
            {
                tbl_id = SharedPcsXlgStatus0_t + pcs_idx;
            }
            else
            {
                tbl_id = SharedPcsLgStatus0_t + pcs_idx;
            }
        }
    }
    else if (CTC_DKIT_INTERFACE_CG == status.mac_mode[mac_id])
    {
        fec_en = GetSharedPcsFecCfg0(V, cgfecEn_f, &pcscfg);
        if(fec_en)
        {
            tbl_id = RsFec0StatusSharedFec0_t + shared_fec_idx;
        }
        else
        {
            tbl_id = SharedPcsCgStatus0_t + pcs_idx;
        }
    }
    else if (CTC_DKIT_INTERFACE_100BASEFX == status.mac_mode[mac_id])
    {
        step = SharedPcsFx1Status0_t - SharedPcsFx0Status0_t;
        tbl_id = SharedPcsFx0Status0_t + mac_index*step + pcs_idx;
    }
    else
    {
        return CLI_ERROR;
    }

    if (tbl_id != MaxTblId_t)
    {
        CTC_DKIT_PRINT("%s mac pcs status---->\n", mac_mode_str[status.mac_mode[mac_id]]);
        ctc_usw_dkit_interface_print_pcs_table(lchip, tbl_id, 0);
    }
    else
    {
        CTC_DKIT_PRINT("Cannot match mac mode!!!");
    }
    return CLI_SUCCESS;
}

