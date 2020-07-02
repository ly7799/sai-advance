/**
 @file ctc_greatbelt_queue.c

 @author  Copyright (C) 2011 Centec Networks Inc.  All rights reserved.

 @date 2010-01-13

 @version v2.0

   The file provide all queue related APIs of greatbelt SDK.
*/

/****************************************************************************
 *
 * Header Files
 *
 ****************************************************************************/
#include "sal.h"
#include "ctc_error.h"
#include "ctc_const.h"

#include "ctc_greatbelt_qos.h"
#include "sys_greatbelt_qos.h"
#include "sys_greatbelt_chip.h"

/****************************************************************************
 *
 * Defines and Macros
 *
 ****************************************************************************/

/****************************************************************************
 *
 * Global and Declaration
 *
 ****************************************************************************/

/****************************************************************************
 *
 * Function
 *
 ****************************************************************************/

/*init*/
extern int32
ctc_greatbelt_qos_init(uint8 lchip, void* p_glb_parm)
{
    uint8 lchip_start               = 0;
    uint8 lchip_end                 = 0;
    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_init(lchip, p_glb_parm));
    }
    return CTC_E_NONE;
}

/**
 @brief get lchip from queue
*/
STATIC int32
_ctc_greatbelt_get_lchip_from_queue(ctc_qos_queue_id_t queue, uint8* lchip, uint8* all_lchip)
{
    CTC_PTR_VALID_CHECK(lchip);
    CTC_PTR_VALID_CHECK(all_lchip);
    switch(queue.queue_type)
    {
    case CTC_QUEUE_TYPE_NETWORK_EGRESS:
        SYS_MAP_GPORT_TO_LCHIP(queue.gport, *lchip);
        *all_lchip = 0;
        break;
    case CTC_QUEUE_TYPE_INTERNAL_PORT:
        SYS_MAP_GPORT_TO_LCHIP(queue.gport, *lchip);
        *all_lchip = 0;
        break;
    case CTC_QUEUE_TYPE_SERVICE_INGRESS:
        SYS_MAP_GPORT_TO_LCHIP(queue.gport, *lchip);
        *all_lchip = 0;
        break;  
    default:
        *all_lchip = 1;
    }

    return CTC_E_NONE;
}

int32
ctc_greatbelt_qos_deinit(uint8 lchip)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_deinit(lchip));
    }

    return CTC_E_NONE;
}

/*policer*/
extern int32
ctc_greatbelt_qos_set_policer(uint8 lchip, ctc_qos_policer_t* p_policer)
{
    uint8 lchip_start                  = 0;
    uint8 lchip_end                    = 0;
    uint8 all_lchip                    = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_policer);
    switch(p_policer->type)
    {
    case CTC_QOS_POLICER_TYPE_PORT:
        SYS_MAP_GPORT_TO_LCHIP(p_policer->id.gport, lchip);
        break;

    default:
        all_lchip = 1;

    }

    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_set_policer(lchip, p_policer));
    }

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_get_policer(uint8 lchip, ctc_qos_policer_t* p_policer)
{
    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_greatbelt_qos_get_policer(lchip, p_policer));
    return CTC_E_NONE;
}

/*global param*/
extern int32
ctc_greatbelt_qos_set_global_config(uint8 lchip, ctc_qos_glb_cfg_t* p_glb_cfg)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 1;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_glb_cfg);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_set_global_config(lchip, p_glb_cfg));
    }
    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_get_global_config(uint8 lchip, ctc_qos_glb_cfg_t* p_glb_cfg)
{
    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_glb_cfg);

    CTC_ERROR_RETURN(sys_greatbelt_qos_get_global_config(lchip, p_glb_cfg));
    return CTC_E_NONE;
}

/*mapping*/
extern int32
ctc_greatbelt_qos_set_domain_map(uint8 lchip, ctc_qos_domain_map_t* p_domain_map)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_set_domain_map(lchip, p_domain_map));
    }

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_get_domain_map(uint8 lchip, ctc_qos_domain_map_t* p_domain_map)
{
    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_greatbelt_qos_get_domain_map(lchip, p_domain_map));
    return CTC_E_NONE;
}

/*queue*/
extern int32
ctc_greatbelt_qos_set_queue(uint8 lchip, ctc_qos_queue_cfg_t* p_que_cfg)
{
    uint8 lchip_start               = 0;
    uint8 lchip_end                 = 0;
    uint8 all_lchip                 = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_que_cfg);
    switch(p_que_cfg->type)
    {
    case CTC_QOS_QUEUE_CFG_QUEUE_STATS_EN:
        _ctc_greatbelt_get_lchip_from_queue(p_que_cfg->value.stats.queue, &lchip, &all_lchip);
        break;

    case CTC_QOS_QUEUE_CFG_LENGTH_ADJUST:
        _ctc_greatbelt_get_lchip_from_queue(p_que_cfg->value.pkt.queue, &lchip, &all_lchip);
        break;
        
    case CTC_QOS_QUEUE_CFG_SERVICE_BIND:
        if(p_que_cfg->value.srv_queue_info.opcode == CTC_QOS_SERVICE_ID_BIND_DESTPORT ||
           p_que_cfg->value.srv_queue_info.opcode == CTC_QOS_SERVICE_ID_UNBIND_DESTPORT)
        {
            SYS_MAP_GPORT_TO_LCHIP(p_que_cfg->value.srv_queue_info.gport, lchip);
        }
        else
        {
           all_lchip = 1; 
        }
        break;

    default:
        all_lchip = 1;
    }


    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_set_queue(lchip, p_que_cfg));
    }
    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_get_queue(uint8 lchip, ctc_qos_queue_cfg_t* p_que_cfg)
{
    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    CTC_ERROR_RETURN(sys_greatbelt_qos_get_queue(lchip, p_que_cfg));
    return CTC_E_NONE;
}

/*shape*/
extern int32
ctc_greatbelt_qos_set_shape(uint8 lchip, ctc_qos_shape_t* p_shape)
{
    uint8 lchip_start               = 0;
    uint8 lchip_end                 = 0;
    uint8 all_lchip                 = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_shape);
    switch(p_shape->type)
    {
    case CTC_QOS_SHAPE_PORT:
        SYS_MAP_GPORT_TO_LCHIP(p_shape->shape.port_shape.gport, lchip);
        break;

    case CTC_QOS_SHAPE_QUEUE:
        _ctc_greatbelt_get_lchip_from_queue(p_shape->shape.queue_shape.queue, &lchip, &all_lchip);
        break;

    case CTC_QOS_SHAPE_GROUP:
        _ctc_greatbelt_get_lchip_from_queue(p_shape->shape.group_shape.queue, &lchip, &all_lchip);
        break;

    default:
        all_lchip = 1;

    }

    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_set_shape(lchip, p_shape));
    }

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_get_shape(uint8 lchip, ctc_qos_shape_t* p_shape)
{
    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    CTC_ERROR_RETURN(sys_greatbelt_qos_get_shape(lchip, p_shape));
    return CTC_E_NONE;
}

/*schedule*/
extern int32
ctc_greatbelt_qos_set_sched(uint8 lchip, ctc_qos_sched_t* p_sched)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_sched);
    switch(p_sched->type)
    {
    case CTC_QOS_SCHED_QUEUE:
        _ctc_greatbelt_get_lchip_from_queue(p_sched->sched.queue_sched.queue, &lchip, &all_lchip);
        break;

    case CTC_QOS_SCHED_GROUP:
        _ctc_greatbelt_get_lchip_from_queue(p_sched->sched.group_sched.queue, &lchip, &all_lchip);
        break;

    default:
        all_lchip = 1;
    }

    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_set_sched(lchip, p_sched));
    }

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_get_sched(uint8 lchip, ctc_qos_sched_t* p_sched)
{
    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    CTC_ERROR_RETURN(sys_greatbelt_qos_get_sched(lchip, p_sched));
    return CTC_E_NONE;
}

/*drop*/
extern int32
ctc_greatbelt_qos_set_drop_scheme(uint8 lchip, ctc_qos_drop_t* p_drop)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_drop);
    _ctc_greatbelt_get_lchip_from_queue(p_drop->queue, &lchip, &all_lchip);
    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_set_drop_scheme(lchip, p_drop));
    }

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_get_drop_scheme(uint8 lchip, ctc_qos_drop_t* p_drop)
{
    uint8 all_lchip                = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_drop);
    _ctc_greatbelt_get_lchip_from_queue(p_drop->queue, &lchip, &all_lchip);

    CTC_ERROR_RETURN(sys_greatbelt_qos_get_drop_scheme(lchip, p_drop));

    return CTC_E_NONE;
}

/*resrc*/
extern int32
ctc_greatbelt_qos_set_resrc(uint8 lchip, ctc_qos_resrc_t* p_resrc)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;

    FEATURE_SUPPORT_CHECK(lchip, CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_resrc);
    switch(p_resrc->cfg_type)
    {
    case CTC_QOS_RESRC_CFG_POOL_CLASSIFY:
        SYS_MAP_GPORT_TO_LCHIP(p_resrc->u.pool.gport, lchip);
        break;

    case CTC_QOS_RESRC_CFG_PORT_MIN:
        SYS_MAP_GPORT_TO_LCHIP(p_resrc->u.port_min.gport, lchip);
        break;

    case CTC_QOS_RESRC_CFG_QUEUE_DROP:
        _ctc_greatbelt_get_lchip_from_queue(p_resrc->u.queue_drop[0].queue,
                                             &lchip, &all_lchip);
        break;

    case CTC_QOS_RESRC_CFG_FLOW_CTL:
        SYS_MAP_GPORT_TO_LCHIP(p_resrc->u.flow_ctl.gport, lchip);
        break;

    default:
        all_lchip = 1;
    }


    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_set_resrc(lchip, p_resrc));
    }

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_get_resrc(uint8 lchip, ctc_qos_resrc_t* p_resrc)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;

    FEATURE_SUPPORT_CHECK(lchip, CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_resrc);
    switch(p_resrc->cfg_type)
    {
    case CTC_QOS_RESRC_CFG_POOL_CLASSIFY:
        SYS_MAP_GPORT_TO_LCHIP(p_resrc->u.pool.gport, lchip);
        break;

    case CTC_QOS_RESRC_CFG_PORT_MIN:
        SYS_MAP_GPORT_TO_LCHIP(p_resrc->u.port_min.gport, lchip);
        break;

    case CTC_QOS_RESRC_CFG_QUEUE_DROP:
        _ctc_greatbelt_get_lchip_from_queue(p_resrc->u.queue_drop[0].queue,
                                             &lchip, &all_lchip);
        break;

    case CTC_QOS_RESRC_CFG_FLOW_CTL:
        SYS_MAP_GPORT_TO_LCHIP(p_resrc->u.flow_ctl.gport, lchip);
        break;

    default:
        all_lchip = 1;
    }


    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_get_resrc(lchip, p_resrc));
    }

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_query_pool_stats(uint8 lchip, ctc_qos_resrc_pool_stats_t* p_stats)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;
    ctc_qos_queue_id_t queue_temp;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_stats);
    
    sal_memset(&queue_temp, 0, sizeof(ctc_qos_queue_id_t));
    if (sal_memcmp(&(p_stats->queue), &queue_temp, sizeof(ctc_qos_queue_id_t)))
    {
        _ctc_greatbelt_get_lchip_from_queue(p_stats->queue, &lchip, &all_lchip);
    }
    else
    {
        SYS_MAP_GPORT_TO_LCHIP(p_stats->gport, lchip);
    }
    
    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_query_pool_stats(lchip, p_stats));
    }

    return CTC_E_NONE;
}

/*stats*/
extern int32
ctc_greatbelt_qos_query_queue_stats(uint8 lchip, ctc_qos_queue_stats_t* p_queue_stats)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;
    ctc_qos_queue_stats_info_t stats;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_queue_stats);
    _ctc_greatbelt_get_lchip_from_queue(p_queue_stats->queue, &lchip, &all_lchip);
    lchip_start = lchip;
    lchip_end = lchip + 1;

    sal_memset(&stats, 0, sizeof(ctc_qos_queue_stats_info_t));
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_query_queue_stats(lchip, p_queue_stats));
        stats.deq_packets  += p_queue_stats->stats.deq_packets;
        stats.deq_bytes    += p_queue_stats->stats.deq_bytes;
        stats.drop_packets += p_queue_stats->stats.drop_packets;
        stats.drop_bytes   += p_queue_stats->stats.drop_bytes;
    }
    sal_memcpy(&p_queue_stats->stats, &stats, sizeof(ctc_qos_queue_stats_info_t));

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_clear_queue_stats(uint8 lchip, ctc_qos_queue_stats_t* p_queue_stats)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_queue_stats);
    _ctc_greatbelt_get_lchip_from_queue(p_queue_stats->queue, &lchip, &all_lchip);
    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_clear_queue_stats(lchip, p_queue_stats));
    }

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_query_policer_stats(uint8 lchip, ctc_qos_policer_stats_t* p_policer_stats)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;
    ctc_qos_policer_stats_info_t stats;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_policer_stats);
    switch(p_policer_stats->type)
    {
    case CTC_QOS_POLICER_TYPE_PORT:
        SYS_MAP_GPORT_TO_LCHIP(p_policer_stats->id.gport, lchip);
        break;

    default:
        all_lchip = 1;

    }

    lchip_start = lchip;
    lchip_end = lchip + 1;

    sal_memset(&stats, 0, sizeof(stats));

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_query_policer_stats(lchip, p_policer_stats));
        stats.confirm_pkts += p_policer_stats->stats.confirm_pkts;
        stats.confirm_bytes += p_policer_stats->stats.confirm_bytes;
        stats.exceed_pkts += p_policer_stats->stats.exceed_pkts;
        stats.exceed_bytes += p_policer_stats->stats.exceed_bytes;
        stats.violate_pkts += p_policer_stats->stats.violate_pkts;
        stats.violate_bytes += p_policer_stats->stats.violate_bytes;
    }
    sal_memcpy(&p_policer_stats->stats, &stats, sizeof(ctc_qos_policer_stats_info_t));

    return CTC_E_NONE;
}

extern int32
ctc_greatbelt_qos_clear_policer_stats(uint8 lchip, ctc_qos_policer_stats_t* p_policer_stats)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;
    uint8 all_lchip                = 0;

    FEATURE_SUPPORT_CHECK(lchip,CTC_FEATURE_QOS);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_policer_stats);
    switch(p_policer_stats->type)
    {
    case CTC_QOS_POLICER_TYPE_PORT:
        SYS_MAP_GPORT_TO_LCHIP(p_policer_stats->id.gport, lchip);
        break;

    default:
        all_lchip = 1;
    }

    lchip_start = lchip;
    lchip_end = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ERROR_RETURN(sys_greatbelt_qos_clear_policer_stats(lchip, p_policer_stats));
    }

    return CTC_E_NONE;
}
