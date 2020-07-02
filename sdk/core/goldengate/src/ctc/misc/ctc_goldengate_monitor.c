#include "ctc_const.h"
#include "ctc_error.h"

#include "ctc_goldengate_monitor.h"
#include "sys_goldengate_monitor.h"
#include "sys_goldengate_chip.h"



int32
ctc_goldengate_monitor_init(uint8 lchip, void* p_global_cfg)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_MONITOR);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_goldengate_monitor_init(lchip, p_global_cfg));
    }

    return CTC_E_NONE;
}

int32
ctc_goldengate_monitor_deinit(uint8 lchip)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_MONITOR);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_goldengate_monitor_deinit(lchip));
    }

    return CTC_E_NONE;
}

int32
ctc_goldengate_monitor_register_cb(uint8 lchip, ctc_monitor_fn_t callback, void* userdata)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_MONITOR);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_goldengate_monitor_register_cb(lchip, callback, userdata));
    }

    return CTC_E_NONE;
}

int32
ctc_goldengate_monitor_set_config(uint8 lchip, ctc_monitor_config_t* p_cfg)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_chip                 = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_MONITOR);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_cfg);
    switch(p_cfg->cfg_type)
    {
    case CTC_MONITOR_CONFIG_MON_INFORM_EN:
    case CTC_MONITOR_CONFIG_MON_INFORM_MIN:
    case CTC_MONITOR_CONFIG_MON_INFORM_MAX:
    case CTC_MONITOR_CONFIG_LOG_EN:
    case CTC_MONITOR_CONFIG_MON_SCAN_EN:
        SYS_MAP_GPORT_TO_LCHIP(p_cfg->gport, lchip);
        break;

    case CTC_MONITOR_CONFIG_LOG_THRD_LEVEL:
        if (p_cfg->monitor_type == CTC_MONITOR_LATENCY)
        {
            SYS_MAP_GPORT_TO_LCHIP(p_cfg->gport, lchip);
        }
        break;

    case  CTC_MONITOR_CONFIG_MON_INTERVAL:
        all_chip = 1;
        break;

    default:
        return CTC_E_INVALID_PARAM;
    }

    lchip_start = lchip;
    lchip_end = lchip_start + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_chip)
    {
        CTC_ERROR_RETURN(sys_goldengate_monitor_set_config(lchip, p_cfg));
    }

    return CTC_E_NONE;
}


int32
ctc_goldengate_monitor_get_config(uint8 lchip, ctc_monitor_config_t* p_cfg)
{

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_MONITOR);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_cfg);

    switch(p_cfg->cfg_type)
    {
    case CTC_MONITOR_CONFIG_MON_INFORM_EN:
    case CTC_MONITOR_CONFIG_MON_INFORM_MIN:
    case CTC_MONITOR_CONFIG_MON_INFORM_MAX:
    case CTC_MONITOR_CONFIG_LOG_EN:
    case CTC_MONITOR_CONFIG_MON_SCAN_EN:
        SYS_MAP_GPORT_TO_LCHIP(p_cfg->gport, lchip);
        break;

    case CTC_MONITOR_CONFIG_LOG_THRD_LEVEL:
        if (p_cfg->monitor_type == CTC_MONITOR_LATENCY)
        {
            SYS_MAP_GPORT_TO_LCHIP(p_cfg->gport, lchip);
        }
        break;

    case  CTC_MONITOR_CONFIG_MON_INTERVAL:
        lchip = 0;
        break;

    default:
        return CTC_E_INVALID_PARAM;
    }

    CTC_ERROR_RETURN(sys_goldengate_monitor_get_config(lchip, p_cfg));

    return CTC_E_NONE;
}

int32
ctc_goldengate_monitor_get_watermark(uint8 lchip, ctc_monitor_watermark_t *p_watermark)
{

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_MONITOR);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_watermark);
    SYS_MAP_GPORT_TO_LCHIP(p_watermark->gport, lchip);
    CTC_ERROR_RETURN(sys_goldengate_monitor_get_watermark(lchip, p_watermark));

    return CTC_E_NONE;
}

int32
ctc_goldengate_monitor_clear_watermark(uint8 lchip, ctc_monitor_watermark_t *p_watermark)
{

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_MONITOR);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_watermark);
    SYS_MAP_GPORT_TO_LCHIP(p_watermark->gport, lchip);
    CTC_ERROR_RETURN(sys_goldengate_monitor_clear_watermark(lchip, p_watermark));

    return CTC_E_NONE;
}

int32
ctc_goldengate_monitor_set_global_config(uint8 lchip, ctc_monitor_glb_cfg_t* p_cfg)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_MONITOR);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_goldengate_monitor_set_global_config(lchip, p_cfg));
    }

    return CTC_E_NONE;
}


