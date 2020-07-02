/**
 @file sys_goldengate_nexthop_l3.c

 @date 2009-11-23

 @version v2.0

 The file contains all nexthop layer2 related callback function
*/

/****************************************************************************
 *
* Header Files
*
****************************************************************************/
#include "ctc_const.h"
#include "ctc_macro.h"
#include "ctc_error.h"
#include "ctc_linklist.h"
#include "ctc_l3if.h"

#include "sys_goldengate_chip.h"
#include "sys_goldengate_port.h"
#include "sys_goldengate_nexthop_api.h"
#include "sys_goldengate_nexthop_hw.h"
#include "sys_goldengate_nexthop.h"
#include "sys_goldengate_vlan.h"
#include "sys_goldengate_stats.h"
#include "sys_goldengate_queue_enq.h"
#include "sys_goldengate_cpu_reason.h"
#include "sys_goldengate_l3if.h"

#include "goldengate/include/drv_enum.h"
#include "goldengate/include/drv_io.h"

#define SYS_NH_DSMET_BITMAP_MAX_PORT_ID   0x1FF

extern int32
sys_goldengate_stacking_get_mcast_profile_met_offset(uint8 lchip,  uint16 mcast_profile_id, uint32 *p_stacking_met_offset);
extern int32
sys_goldengate_stacking_get_rsv_trunk_number(uint8 lchip, uint8* number);
extern int32
sys_goldengate_linkagg_show_ports(uint8 lchip, uint8 tid, uint32* p_gports, uint16* cnt);
extern int32
sys_goldengate_linkagg_get_max_mem_num(uint8 lchip, uint16* max_num);

/****************************************************************************
 *
* Defines and Macros
*
*****************************************************************************/
#define SYS_NH_MCAST_REPLI_INCR_STEP   4
#define SYS_NH_LOGICAL_REPLI_MAX_NUM   32
#define SYS_NH_MET_END_REPLICATE_OFFSET  0xFFFF
#define SYS_NH_PHY_REP_MAX_NUM             0xFFF

struct sys_nh_mcast_dsmet_io_s
{
    sys_nh_param_mcast_member_t* p_mem_param;
    uint32 met_offset;
    uint32 next_met_offset;
    uint32 dsnh_offset;
    uint8 use_pbm;
    sys_nh_mcast_meminfo_t* p_next_mem;
};
typedef struct sys_nh_mcast_dsmet_io_s sys_nh_mcast_dsmet_io_t;

#define SYS_MC_BITMAP_MAX_PORT 125
#define SYS_MC_MAP_PORT_ID(dest_id)  ((dest_id>>8)?((dest_id&0x3F) + 64):(dest_id&0x3F))

#define SYS_MC_MAP_PBM_PORT_ID(dest_id, port_type, port_id)\
    do{\
      port_type = (dest_id >> 6)&0x3;\
      port_id   = SYS_MC_MAP_PORT_ID(dest_id);\
    }while(0)

#define SYS_MC_DESTID_IS_NOT_TOCPU(destid) (!SYS_LPORT_IS_CPU_ETH(lchip, destid))

#define SYS_MC_DESTID_IS_INTERNAL(port) (((port&0xFF)>=SYS_RSV_PORT_BASE && (port&0xFF)<=SYS_RSV_PORT_END) && \
    SYS_MC_DESTID_IS_NOT_TOCPU(port))

/****************************************************************************
 *
* Global and Declaration
*
*****************************************************************************/

/****************************************************************************
 *
* Function
*
*****************************************************************************/
STATIC int32
_sys_goldengate_nh_mcast_get_port_reflect(uint8 lchip, sys_nh_param_mcast_member_t* p_member)
{
    uint32 port_reflective_en = 0;
    uint16 gport = 0;
    uint8 gchip = 0;
    uint32* p_gports = NULL;
    uint16 cnt  = 0;
    uint8 i = 0;
    uint16 max_num = 0;
    int32 ret = 0;

    if ((SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH_DESTID == p_member->member_type)
        || (SYS_NH_PARAM_BRGMC_MEM_LOCAL == p_member->member_type)
        || (SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH == p_member->member_type))
    {
        sys_goldengate_linkagg_get_max_mem_num(lchip, &max_num);
        p_gports = (uint32*)mem_malloc(MEM_NEXTHOP_MODULE, sizeof(uint32)*max_num);
        if (NULL == p_gports)
        {
            return CTC_E_NO_MEMORY;
        }

        if (!p_member->is_linkagg)
        {
            sys_goldengate_get_gchip_id(lchip, &gchip);
            p_gports[0]  = CTC_MAP_LPORT_TO_GPORT(gchip, p_member->destid);
            cnt = 1;
        }
        else
        {
            sys_goldengate_linkagg_show_ports(lchip, p_member->destid, p_gports, &cnt);
        }

        if (cnt)
        {
            for (i = 0; i < cnt; i++)
            {
                gport = p_gports[i];
                gchip = CTC_MAP_GPORT_TO_GCHIP(gport);
                if (FALSE == sys_goldengate_chip_is_local(lchip, gchip))
                {
                    continue;
                }

                ret = sys_goldengate_port_get_property(lchip, gport, CTC_PORT_PROP_REFLECTIVE_BRIDGE_EN, &port_reflective_en);
                if (ret < 0)
                {
                    mem_free(p_gports);
                    return ret;
                }
                if (port_reflective_en)/*p_member.is_src_port_check_dis will be set, if set from nexthop API or set reflect on the port*/
                {
                    p_member->is_src_port_check_dis = 1;
                }
            }
        }

        if (SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH == p_member->member_type && sys_goldengate_nh_is_cpu_nhid(lchip, p_member->ref_nhid))
        {
            p_member->is_src_port_check_dis = 1;
        }

        mem_free(p_gports);
    }

    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_nh_mcast_analyze_member(uint8 lchip, ctc_list_pointer_t* p_db_member_list,
                                       sys_nh_param_mcast_member_t* p_member,
                                       sys_nh_mcast_meminfo_t** pp_mem_node,
                                       bool* p_entry_exit,
                                       uint32* p_repli_pos)
{

    sys_nh_mcast_meminfo_t* p_meminfo;
    ctc_list_pointer_node_t* p_pos_mem;
    uint8 index = 0;
     uint8 port_type = 0;
    uint8 port_id = 0;


    *pp_mem_node = NULL;
    *p_entry_exit = FALSE;
    *p_repli_pos = 0;
    if ((p_member->member_type != SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH) &&
        (p_member->member_type != SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE) &&
        (0 == p_member->logic_port))
    {
        p_member->is_logic_port = 0;
    }

    CTC_LIST_POINTER_LOOP(p_pos_mem, p_db_member_list)
    {
        p_meminfo = _ctc_container_of(p_pos_mem, sys_nh_mcast_meminfo_t, list_head);

        if (p_meminfo->dsmet.member_type != p_member->member_type)
        {
            if(((SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE == p_meminfo->dsmet.member_type)
                ||(SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE == p_member->member_type))
                &&(p_meminfo->dsmet.ref_nhid == p_member->ref_nhid))
            {

            }
            else
            {
                continue;
            }
        }

        switch (p_member->member_type)
        {
        case  SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE:
        case  SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH:
        case  SYS_NH_PARAM_IPMC_MEM_LOOP_BACK:
            if ((p_meminfo->dsmet.ref_nhid == p_member->ref_nhid))
            {
                *pp_mem_node = p_meminfo;
                *p_entry_exit = TRUE;
                return CTC_E_NONE;
            }

            break;
        case SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH_DESTID:
        if (!p_member->is_logic_port && CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_USE_PBM)
            && CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg
        && (SYS_MC_MAP_PORT_ID(p_member->destid) <= SYS_MC_BITMAP_MAX_PORT)
        && (SYS_MC_DESTID_IS_NOT_TOCPU(p_member->destid))
        && (p_meminfo->dsmet.ref_nhid == p_member->ref_nhid))
            {
                 /*check port/linkagg bitmap  */
                 if ( !p_member->is_linkagg) /*port bitmap*/
                 {
                     SYS_MC_MAP_PBM_PORT_ID(p_member->destid, port_type, port_id);
                 }
                 else /*linkagg bitmap*/
                 {
                     port_id = p_member->destid;
                 }

                 SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "port_type:%d, port_id:%d\n", port_type, port_id);

                 if (port_type == p_meminfo->dsmet.port_type)
                 {
                     *pp_mem_node = p_meminfo;
                     *p_entry_exit = CTC_BMP_ISSET(p_meminfo->dsmet.pbm, port_id);
                     return CTC_E_NONE;
                 }
            }
            else if (
                CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg
                && (p_meminfo->dsmet.ucastid == p_member->destid)
                && (p_meminfo->dsmet.ref_nhid == p_member->ref_nhid)
                && (CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_LOGIC_PORT_CHK) == p_member->is_logic_port)
                && (p_meminfo->dsmet.logic_port == p_member->logic_port))
            {
                *pp_mem_node = p_meminfo;
                *p_entry_exit = TRUE;
                return CTC_E_NONE;
            }

            break;

        case SYS_NH_PARAM_MCAST_MEM_MIRROR_WITH_NH:
            if ((p_meminfo->dsmet.ref_nhid == p_member->ref_nhid)
                && (p_meminfo->dsmet.ucastid == p_member->destid)
                && (CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg))
            {
                *pp_mem_node = p_meminfo;
                *p_entry_exit = TRUE;
                return CTC_E_NONE;
            }

            break;

        case SYS_NH_PARAM_BRGMC_MEM_LOCAL:
        case SYS_NH_PARAM_BRGMC_MEM_RAPS:
            if (p_meminfo->dsmet.ucastid == SYS_NH_MET_DROP_UCAST_ID)
            {
                *pp_mem_node = p_meminfo;
                *p_entry_exit = FALSE;
                return CTC_E_NONE;
            }
            else if (!p_member->is_logic_port && CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_USE_PBM)
                && CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg
            && (SYS_MC_MAP_PORT_ID(p_member->destid) <= SYS_MC_BITMAP_MAX_PORT)
            && (SYS_MC_DESTID_IS_NOT_TOCPU(p_member->destid)))
            {
                 /*check port/linkagg bitmap  */
                 if ( !p_member->is_linkagg) /*port bitmap*/
                 {
                     SYS_MC_MAP_PBM_PORT_ID(p_member->destid, port_type, port_id);
                 }
                 else /*linkagg bitmap*/
                 {
                     port_id = p_member->destid;
                 }

                 SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "port_type:%d, port_id:%d\n", port_type, port_id);

                 if (port_type == p_meminfo->dsmet.port_type)
                 {
                     *pp_mem_node = p_meminfo;
                     *p_entry_exit = CTC_BMP_ISSET(p_meminfo->dsmet.pbm, port_id);
                     return CTC_E_NONE;
                 }


            }
            else if (
                CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg &&
                p_meminfo->dsmet.ucastid == p_member->destid &&
                CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_LOGIC_PORT_CHK) == p_member->is_logic_port &&
                p_meminfo->dsmet.logic_port == p_member->logic_port)
            {
                *pp_mem_node = p_meminfo;
                *p_entry_exit = TRUE;
                return CTC_E_NONE;
            }

            break;

        case SYS_NH_PARAM_MCAST_MEM_REMOTE:

            if ((CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg) &&
                (p_meminfo->dsmet.ucastid == p_member->destid))
            {
                *pp_mem_node = p_meminfo;
                *p_entry_exit = TRUE;
                return CTC_E_NONE;
            }

            break;

        case  SYS_NH_PARAM_IPMC_MEM_LOCAL:
            if (p_meminfo->dsmet.ucastid == SYS_NH_MET_DROP_UCAST_ID
                &&  p_member->l3if_type == CTC_L3IF_TYPE_VLAN_IF)
            {
                *pp_mem_node = p_meminfo;
                *p_entry_exit = FALSE;
                return CTC_E_NONE;
            }
            else if (CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_USE_PBM)
                && (p_member->l3if_type == CTC_L3IF_TYPE_VLAN_IF)
                && (p_meminfo->dsmet.vid == p_member->vid)
                && (SYS_MC_MAP_PORT_ID(p_member->destid) <= SYS_MC_BITMAP_MAX_PORT)
                && CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg)
            {
                 uint8 port_type = 0;
                 uint8 port_id = 0;

                 if ( !p_member->is_linkagg) /*port bitmap*/
                 {
                     SYS_MC_MAP_PBM_PORT_ID(p_member->destid, port_type, port_id);
                 }
                 else /*linkagg bitmap*/
                 {
                     port_id = p_member->destid;
                 }

                 SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "port_type:%d, port_id:%d\n", port_type, port_id);

                 if (port_type == p_meminfo->dsmet.port_type)
                 {
                     *pp_mem_node = p_meminfo;
                     *p_entry_exit = CTC_BMP_ISSET(p_meminfo->dsmet.pbm, port_id);
                     return CTC_E_NONE;
                 }


            }
            else
            {
                if ((CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg)
                    && (p_meminfo->dsmet.ucastid == p_member->destid)
                    && (p_meminfo->dsmet.vid == p_member->vid))
                {
                    *pp_mem_node = p_meminfo;
                    *p_entry_exit = TRUE;
                    return CTC_E_NONE;
                }
            }

            break;

        case SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP:  /* logic replication  */
            /* logic replication  */
            if ((CTC_FLAG_ISSET(p_meminfo->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG) == p_member->is_linkagg)
                && (p_meminfo->dsmet.ucastid == p_member->destid))
            {
                if ((p_meminfo->dsmet.replicate_num + 1) < SYS_NH_LOGICAL_REPLI_MAX_NUM)
                {
                    *pp_mem_node = p_meminfo;
                }

                if (p_meminfo->dsmet.replicate_num == 0)
                {
                    if (p_meminfo->dsmet.vid == p_member->vid)
                    {
                        *p_entry_exit = TRUE;
                        *pp_mem_node = p_meminfo;
                        return CTC_E_NONE;
                    }
                }
                else
                {
                    for (index = 0; index < (p_meminfo->dsmet.replicate_num + 1); index++)
                    {
                        if (p_meminfo->dsmet.vid_list[index] == p_member->vid)
                        {
                            *p_repli_pos = index;
                            *p_entry_exit = TRUE;
                            *pp_mem_node = p_meminfo;
                            return CTC_E_NONE;
                        }
                    }
                }
            }
            break;
        default:
            break;
        }
    }


    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_nh_mcast_repli_update_dsnh(uint8 lchip, uint32 old_dsnh_offset,
                                          uint32 new_dsnh_offset, uint32 update_cnt)
{
    DsNextHop4W_m dsnh[SYS_NH_LOGICAL_REPLI_MAX_NUM];
    uint32 cmd = 0;
    uint32 offset = 0;
    uint32 i = 0;

    cmd = DRV_IOR(DsNextHop4W_t, DRV_ENTRY_FLAG);
    for (i = 0; i < update_cnt ; i++)
    {
        offset = old_dsnh_offset + i;
        CTC_ERROR_RETURN(DRV_IOCTL(lchip, offset, cmd, &dsnh[i]));
    }


    cmd = DRV_IOW(DsNextHop4W_t, DRV_ENTRY_FLAG);
    for (i = 0; i < update_cnt ; i++)
    {
        offset = new_dsnh_offset + i;
        CTC_ERROR_RETURN(DRV_IOCTL(lchip, offset, cmd, &dsnh[i]));
    }

    return CTC_E_NONE;

}

STATIC int32
_sys_goldengate_nh_mcast_process_dsnh(uint8 lchip, sys_nh_param_mcast_member_t* p_mem_param,
                                     sys_nh_mcast_meminfo_t* p_member_info,
                                     uint32* p_dsnh_offset)
{

    sys_nh_param_dsnh_t dsnh_param;
    sys_l3if_prop_t l3if_prop;
    uint8 entry_num = 0;

    sal_memset(&dsnh_param, 0, sizeof(sys_nh_param_dsnh_t));
    sal_memset(&l3if_prop, 0, sizeof(l3if_prop));

    if (p_mem_param->is_linkagg)
    {
        l3if_prop.gport = CTC_MAP_TID_TO_GPORT(p_mem_param->destid);
    }
    else
    {
        uint8 gchip;
        CTC_ERROR_RETURN(sys_goldengate_get_gchip_id(lchip, &gchip));
        l3if_prop.gport = CTC_MAP_LPORT_TO_GPORT(gchip, p_mem_param->destid);
    }

    l3if_prop.l3if_type = p_mem_param->l3if_type;
    l3if_prop.vlan_id = p_mem_param->vid & 0xFFF;

    CTC_ERROR_RETURN(sys_goldengate_l3if_get_l3if_info(lchip, 0, &l3if_prop));
    dsnh_param.l2edit_ptr = 0;
    dsnh_param.l3edit_ptr = 0;
    dsnh_param.dest_vlan_ptr = l3if_prop.vlan_ptr;

    if (p_member_info && p_member_info->dsmet.free_dsnh_offset_cnt)
    /*Logical replicate*/
    {
        if (p_member_info->dsmet.replicate_num + 1 >= SYS_NH_LOGICAL_REPLI_MAX_NUM)
        {
            return CTC_E_NH_EXCEED_MAX_LOGICAL_REPLI_CNT;
        }

        *p_dsnh_offset = p_member_info->dsmet.dsnh_offset + p_member_info->dsmet.replicate_num + 1;
        p_member_info->dsmet.free_dsnh_offset_cnt--;
        p_member_info->dsmet.vid_list[p_member_info->dsmet.replicate_num + 1] = l3if_prop.vlan_id;

    }
    else if (p_member_info)
    /*Update dsnh offset*/
    {
        if ((p_member_info->dsmet.replicate_num + SYS_NH_MCAST_REPLI_INCR_STEP) > \
            SYS_NH_LOGICAL_REPLI_MAX_NUM)
        {
            return CTC_E_NH_EXCEED_MAX_LOGICAL_REPLI_CNT;
        }
        else
        {
            /*Logical replicate*/
            if (p_member_info->dsmet.vid_list == NULL)
            {
                p_member_info->dsmet.vid_list = mem_malloc(MEM_NEXTHOP_MODULE, SYS_NH_LOGICAL_REPLI_MAX_NUM * sizeof(uint16));

                if (!p_member_info->dsmet.vid_list)
                {
                    return CTC_E_NO_MEMORY;
                }

                p_member_info->dsmet.vid_list[0] = p_member_info->dsmet.vid;
                CTC_UNSET_FLAG(p_member_info->dsmet.flag, SYS_NH_DSMET_FLAG_USE_RSV_NH);
            }

            if ((p_member_info->dsmet.replicate_num + 1) < SYS_NH_MCAST_REPLI_INCR_STEP)
            {
                entry_num = SYS_NH_MCAST_REPLI_INCR_STEP;
            }
            else
            {
                entry_num = p_member_info->dsmet.replicate_num + 1 + SYS_NH_MCAST_REPLI_INCR_STEP;
            }

            /*Allocate new dsnh*/
            CTC_ERROR_RETURN(sys_goldengate_nh_offset_alloc(lchip,
                                                           SYS_NH_ENTRY_TYPE_NEXTHOP_4W, entry_num, p_dsnh_offset));
            /*Free old dsnh offset*/
            CTC_ERROR_RETURN(sys_goldengate_nh_offset_free(lchip,
                                                          SYS_NH_ENTRY_TYPE_NEXTHOP_4W, (p_member_info->dsmet.replicate_num + 1),
                                                          (p_member_info->dsmet.dsnh_offset)));

            CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_repli_update_dsnh(lchip,
                                                                       p_member_info->dsmet.dsnh_offset, (*p_dsnh_offset), (p_member_info->dsmet.replicate_num + 1)));

            p_member_info->dsmet.free_dsnh_offset_cnt = SYS_NH_MCAST_REPLI_INCR_STEP;

            p_member_info->dsmet.free_dsnh_offset_cnt -= ((p_member_info->dsmet.replicate_num) ? 1 : 2);

            p_member_info->dsmet.vid_list[p_member_info->dsmet.replicate_num + 1] = l3if_prop.vlan_id;
            p_member_info->dsmet.dsnh_offset = *p_dsnh_offset;

            /*Current used dsnh offset*/
            *p_dsnh_offset = (*p_dsnh_offset) + p_member_info->dsmet.replicate_num + 1;

        }
    }
    else
    {   /*First member */
        CTC_ERROR_RETURN(sys_goldengate_nh_offset_alloc(lchip,
                                                       SYS_NH_ENTRY_TYPE_NEXTHOP_4W,
                                                       1, p_dsnh_offset));
    }


    dsnh_param.dsnh_offset = (*p_dsnh_offset);
    dsnh_param.dsnh_type = SYS_NH_PARAM_DSNH_TYPE_IPMC;
    dsnh_param.mtu_no_chk = p_mem_param->mtu_no_chk;

    CTC_ERROR_RETURN(sys_goldengate_nh_write_entry_dsnh4w(lchip, &dsnh_param));
    return CTC_E_NONE;

}

STATIC int32
_sys_goldengate_nh_mcast_write_dsmet(uint8 lchip, sys_nh_info_dsmet_t* p_met_info)
{
    sys_met_t dsmet;
    uint8 enable = 0;
    uint32 dsmet_offset = 0;

    sal_memset(&dsmet, 0, sizeof(dsmet));

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);
    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_PARAM, "\n\
    lchip = %d\n\
    dsmet_offset = 0x%x, \n\
    dsnh_offset = 0x %x, \n\
    is_linkagg = %d, \n\
    member_type = %d, \n\
    replicate_num = %d, \n\
    ucastid = %d, \n\
    use_pbm = %d,  port_type = %d, pbm0 = 0x%x, pbm1 = 0x%x,  pbm1 = 0x%x, pbm2 = 0x%x, \n\
    next_met_offset = 0x %x, \n\
    port_check_discard = %d\n",
                   lchip,
                   p_met_info->dsmet_offset,
                   p_met_info->dsnh_offset,
                   CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_IS_LINKAGG),
                   p_met_info->member_type,
                   p_met_info->replicate_num,
                   p_met_info->ucastid,
                   CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_USE_PBM), p_met_info->port_type,
                   p_met_info->pbm[0], p_met_info->pbm[1], p_met_info->pbm[2], p_met_info->pbm[3],
                   p_met_info->next_dsmet_offset,
                   CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_PORT_CHK_DISCARD));

    /*
    dsmet.is_met = 1;
    */
    sys_goldengate_nh_get_reflective_brg_en(lchip, &enable);
    if (enable)
    {
        CTC_UNSET_FLAG(p_met_info->flag, SYS_NH_DSMET_FLAG_PORT_CHK_DISCARD);
    }

    dsmet.next_hop_ptr   = p_met_info->dsnh_offset;

    if (CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_USE_PBM))
    {
        if ((p_met_info->pbm[0] | p_met_info->pbm[1] | p_met_info->pbm[2] | p_met_info->pbm[3]))
        {
            dsmet.mcast_mode  = 1;
            sal_memcpy(dsmet.port_bitmap, p_met_info->pbm, sizeof(p_met_info->pbm));
            dsmet.port_bitmap_base = p_met_info->port_type;
            dsmet.is_agg = CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_IS_LINKAGG);
        }
        else
        {
            dsmet.mcast_mode  = 0;
            dsmet.dest_id  = SYS_NH_MET_DROP_UCAST_ID;
        }

        dsmet.phy_port_chk_discard = CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_PORT_CHK_DISCARD);

    }
    else
    {
        dsmet.dest_id = p_met_info->ucastid ;
        dsmet.logic_port_check_en = CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_LOGIC_PORT_CHK);

        if (CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_HORIZON_SPLIT_EN))
        {
            dsmet.logic_port_type_check = 1;
        }

        if (CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_REFECTIVE))
        {
            dsmet.force_back_en = 1;
        }



        dsmet.remote_chip = (SYS_NH_PARAM_MCAST_MEM_REMOTE == p_met_info->member_type) ? 1 : 0;
        dsmet.is_agg = CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_IS_LINKAGG);
        if (CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_LEAF_CHECK_EN))
        {
            dsmet.leaf_check_en = 1;
        }

        if (CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_LOGIC_PORT_CHK))
        {  /*only apply to vpls/trill/wlan/pbb/overlay*/
            dsmet.logic_dest_port = p_met_info->logic_port;
        }
        else
        {
            dsmet.phy_port_chk_discard = CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_PORT_CHK_DISCARD);
        }

        dsmet.aps_bridge_en  = (p_met_info->member_type == SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE) ? 1 : 0;

    }

    dsmet.end_local_rep = CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_ENDLOCAL);
    dsmet.next_hop_ext  = CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_USE_DSNH8W);
    dsmet.replicate_num = p_met_info->replicate_num;
    dsmet.is_6w = (p_met_info->entry_type == SYS_NH_ENTRY_TYPE_MET_6W);

    dsmet.next_met_entry_ptr = SYS_NH_NEXT_MET_ENTRY(lchip, p_met_info->next_dsmet_offset);
    dsmet_offset = CTC_FLAG_ISSET(p_met_info->flag, SYS_NH_DSMET_FLAG_GLOBAL_MET)?
                                p_met_info->dsmet_offset + sys_goldengate_nh_get_dsmet_base(lchip) : p_met_info->dsmet_offset;

    CTC_ERROR_RETURN(sys_goldengate_nh_write_asic_table(lchip, p_met_info->entry_type, dsmet_offset, &dsmet));

    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_nh_mcast_entry_set_default(uint8 lchip,
                                           sys_nh_info_mcast_t* p_info_mcast,
                                           uint32 met_offset,
                                           bool bCreateGroup,
                                           bool bIs6w)
{

    sys_nh_info_dsmet_t dsmet;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    sal_memset(&dsmet, 0, sizeof(sys_nh_info_dsmet_t));

    dsmet.dsmet_offset = met_offset;

    dsmet.entry_type = SYS_NH_ENTRY_TYPE_MET_6W;
    dsmet.ucastid = SYS_NH_MET_DROP_UCAST_ID;
    CTC_SET_FLAG(dsmet.flag, SYS_NH_DSMET_FLAG_GLOBAL_MET);
    if (bCreateGroup && p_info_mcast->stacking_met_offset)
    {
        dsmet.next_dsmet_offset = p_info_mcast->stacking_met_offset;
    }
    else
    {
        dsmet.next_dsmet_offset = SYS_NH_MET_END_REPLICATE_OFFSET;
    }

    CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_write_dsmet(lchip, &dsmet));

    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_nh_mcast_build_dsmet_info(uint8 lchip,
                                          sys_nh_info_mcast_t* p_info_mcast,
                                          sys_nh_mcast_dsmet_io_t* p_dsmet_io,
                                          sys_nh_info_dsmet_t* p_dsmet)
{
    p_dsmet->replicate_num = 0;
    p_dsmet->dsnh_offset = p_dsmet_io->dsnh_offset,
    p_dsmet->dsmet_offset = p_dsmet_io->met_offset;
    p_dsmet->member_type = p_dsmet_io->p_mem_param->member_type;
    p_dsmet->flag      |= p_dsmet_io->p_mem_param->is_linkagg ? SYS_NH_DSMET_FLAG_IS_LINKAGG : 0;
    p_dsmet->flag      |= p_dsmet_io->p_mem_param->is_logic_port ? SYS_NH_DSMET_FLAG_LOGIC_PORT_CHK : 0;
    p_dsmet->flag      |= p_dsmet_io->p_mem_param->is_horizon_split ? SYS_NH_DSMET_FLAG_HORIZON_SPLIT_EN : 0;
    p_dsmet->flag      |= p_dsmet_io->p_mem_param->is_reflective? SYS_NH_DSMET_FLAG_REFECTIVE: 0;
    p_dsmet->flag      |= p_dsmet_io->p_mem_param->leaf_check_en? SYS_NH_DSMET_FLAG_LEAF_CHECK_EN:0;

    p_dsmet->logic_port  = p_dsmet_io->p_mem_param->logic_port;
    p_dsmet->ref_nhid = p_dsmet_io->p_mem_param->ref_nhid;
    p_dsmet->vid      = p_dsmet_io->p_mem_param->vid;
    p_dsmet->ucastid = p_dsmet_io->p_mem_param->destid;

    switch (p_dsmet_io->p_mem_param->member_type)
    {
    case SYS_NH_PARAM_BRGMC_MEM_LOCAL:
    case SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE:
    case SYS_NH_PARAM_BRGMC_MEM_RAPS:
    case SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH:
    case SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH_DESTID:
        p_dsmet->flag      |= p_dsmet_io->p_mem_param->is_src_port_check_dis ? 0 : SYS_NH_DSMET_FLAG_PORT_CHK_DISCARD;

        if (SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH == p_dsmet_io->p_mem_param->member_type
            && sys_goldengate_nh_is_cpu_nhid(lchip, p_dsmet->ref_nhid))
        {
            uint32 reason_id = 0;
            uint32 dest_map = 0;
            reason_id = CTC_PKT_CPU_REASON_GET_BY_NHPTR(p_dsmet->dsnh_offset);
            CTC_ERROR_RETURN(sys_goldengate_cpu_reason_get_info(lchip, reason_id, &dest_map));
            p_dsmet->ucastid = dest_map;
            CTC_UNSET_FLAG(p_dsmet->flag, SYS_NH_DSMET_FLAG_PORT_CHK_DISCARD);
        }

    /*No need break here*/
    case SYS_NH_PARAM_MCAST_MEM_MIRROR_WITH_NH:
    case SYS_NH_PARAM_IPMC_MEM_LOCAL:
    case SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP:
    case SYS_NH_PARAM_IPMC_MEM_LOOP_BACK:

        if (p_dsmet_io->p_next_mem)
        {
            p_dsmet->next_dsmet_offset = p_dsmet_io->p_next_mem->dsmet.dsmet_offset;
        }
        else
        {

            CTC_SET_FLAG(p_dsmet->flag, SYS_NH_DSMET_FLAG_ENDLOCAL);
            p_dsmet->next_dsmet_offset = SYS_NH_MET_END_REPLICATE_OFFSET;
        }

        break;


    case SYS_NH_PARAM_MCAST_MEM_REMOTE:
		break;

    default:
        return CTC_E_INVALID_PARAM;

    }

    /*If stacking en, need hook stacking mcast group*/
    if (SYS_NH_MET_END_REPLICATE_OFFSET == p_dsmet->next_dsmet_offset)
    {
        if (p_info_mcast->stacking_met_offset)
        {
            p_dsmet->next_dsmet_offset = p_info_mcast->stacking_met_offset;
        }
    }

    return CTC_E_NONE;
}


STATIC int32
_sys_goldengate_nh_mcast_add_remote_dsmet_entry(uint8 lchip, sys_nh_param_mcast_member_t* p_mem_param,
                                                uint32 basic_met_offset,
                                                ctc_list_pointer_t* p_db_member_list,
                                                sys_nh_info_mcast_t* p_info_mcast)
{
    uint16 mcast_profile_id = 0;

    mcast_profile_id = p_mem_param->destid;

    CTC_ERROR_RETURN(sys_goldengate_stacking_get_mcast_profile_met_offset(lchip, mcast_profile_id, &p_info_mcast->stacking_met_offset));

    if (ctc_list_pointer_empty(p_db_member_list))
    {
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_entry_set_default(lchip,
                                                                    p_info_mcast,
                                                                    p_info_mcast->basic_met_offset,
                                                                    TRUE,
                                                                    TRUE));
    }
    else
    {
        sys_nh_mcast_meminfo_t* p_mem_flex = NULL;

        p_mem_flex = _ctc_container_of(ctc_list_pointer_node_tail(p_db_member_list),
                                       sys_nh_mcast_meminfo_t, list_head);

        /*(2) update previous member's next met offset*/
        p_mem_flex->dsmet.next_dsmet_offset = p_info_mcast->stacking_met_offset;

        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_write_dsmet(lchip, &(p_mem_flex->dsmet)));

    }

    p_info_mcast->stacking_mcast_profile_id = mcast_profile_id;

    return CTC_E_NONE;

 }


STATIC int32
_sys_goldengate_nh_mcast_remove_remote_dsmet_entry(uint8 lchip, sys_nh_param_mcast_member_t* p_mem_param,
                                                   uint32 basic_met_offset,
                                                   ctc_list_pointer_t* p_db_member_list,
                                                   sys_nh_info_mcast_t* p_info_mcast)
{
    uint16 mcast_profile_id = 0;

    mcast_profile_id = 0;

    CTC_ERROR_RETURN(sys_goldengate_stacking_get_mcast_profile_met_offset(lchip, mcast_profile_id, &p_info_mcast->stacking_met_offset));

    if (ctc_list_pointer_empty(p_db_member_list))
    {
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_entry_set_default(lchip,
                                                                    p_info_mcast,
                                                                    p_info_mcast->basic_met_offset,
                                                                    TRUE,
                                                                    TRUE));
    }
    else
    {
        sys_nh_mcast_meminfo_t* p_mem_flex = NULL;

        p_mem_flex = _ctc_container_of(ctc_list_pointer_node_tail(p_db_member_list),
                                       sys_nh_mcast_meminfo_t, list_head);

        /*(2) update previous member's next met offset*/
        p_mem_flex->dsmet.next_dsmet_offset = p_info_mcast->stacking_met_offset;

        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_write_dsmet(lchip, &(p_mem_flex->dsmet)));

    }

    p_info_mcast->stacking_mcast_profile_id = 0;

    return CTC_E_NONE;

 }




STATIC int32
_sys_goldengate_nh_mcast_add_dsmet_entry(uint8 lchip, sys_nh_param_mcast_member_t* p_mem_param,
                                        uint32 basic_met_offset, uint32 dsnh_offset,
                                        uint8 use_dsnh8w,
                                        ctc_list_pointer_t* p_db_member_list,
                                        sys_nh_info_mcast_t* p_info_mcast)
{
    int32 ret = CTC_E_NONE;
    sys_nh_mcast_meminfo_t* p_mem_flex = NULL;
    uint32 new_met_offset = 0;
    sys_nh_mcast_dsmet_io_t dsmet_io;
    sys_nh_mcast_meminfo_t* p_member_info;


    p_member_info = mem_malloc(MEM_NEXTHOP_MODULE, sizeof(sys_nh_mcast_meminfo_t));

    if (!p_member_info)
    {
        return CTC_E_NO_MEMORY;
    }

    sal_memset(p_member_info, 0, sizeof(sys_nh_mcast_meminfo_t));
    p_member_info->dsmet.flag |= use_dsnh8w ? SYS_NH_DSMET_FLAG_USE_DSNH8W : 0;
    p_member_info->dsmet.entry_type = SYS_NH_ENTRY_TYPE_MET;

    /*1. Init param*/
    sal_memset(&dsmet_io, 0, sizeof(sys_nh_mcast_dsmet_io_t));
    dsmet_io.p_mem_param = p_mem_param;

    if ((SYS_NH_PARAM_BRGMC_MEM_LOCAL == p_mem_param->member_type ||
        SYS_NH_PARAM_BRGMC_MEM_RAPS == p_mem_param->member_type ||
    SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH_DESTID == p_mem_param->member_type ||
    (SYS_NH_PARAM_IPMC_MEM_LOCAL == p_mem_param->member_type &&
    p_mem_param->l3if_type == CTC_L3IF_TYPE_VLAN_IF)  )
    && !p_mem_param->is_logic_port
    && !p_mem_param->leaf_check_en
    && !p_mem_param->is_src_port_check_dis
    && (SYS_MC_DESTID_IS_NOT_TOCPU(p_mem_param->destid))
    && (SYS_MC_MAP_PORT_ID(p_mem_param->destid) <= SYS_MC_BITMAP_MAX_PORT))
    {
        uint8 port_type = 0;
        uint8 port_id = 0;

        if (p_mem_param->is_linkagg)
        {
            CTC_SET_FLAG(p_member_info->dsmet.flag, SYS_NH_DSMET_FLAG_IS_LINKAGG);
            port_id = p_mem_param->destid;
        }
        else
        {
            SYS_MC_MAP_PBM_PORT_ID(p_mem_param->destid, port_type, port_id);
        }

        p_member_info->dsmet.port_type = port_type;
        CTC_BMP_SET(p_member_info->dsmet.pbm, port_id);
        CTC_SET_FLAG(p_member_info->dsmet.flag, SYS_NH_DSMET_FLAG_USE_PBM);

        p_member_info->dsmet.vid = p_mem_param->vid;
        p_member_info->dsmet.ucastid = 0;
        p_member_info->dsmet.entry_type = SYS_NH_ENTRY_TYPE_MET_6W;
        p_member_info->dsmet.replicate_num  = 0;


    }

    /*2. Build new allocate member, and update flex member(prev member or next member)*/
    /*2.1 New member is the list's first member*/
    if (ctc_list_pointer_empty(p_db_member_list))
    {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Mcast list is empty!\n");

            p_member_info->dsmet.entry_type = SYS_NH_ENTRY_TYPE_MET_6W;
            dsmet_io.met_offset = basic_met_offset;
            dsmet_io.dsnh_offset = dsnh_offset;
            dsmet_io.p_next_mem = NULL;
            CTC_ERROR_GOTO(_sys_goldengate_nh_mcast_build_dsmet_info(lchip, p_info_mcast, &dsmet_io, &(p_member_info->dsmet)), ret, error1);
            CTC_SET_FLAG(p_member_info->dsmet.flag, SYS_NH_DSMET_FLAG_GLOBAL_MET);
            /*Add this new member to db member list*/
            ctc_list_pointer_insert_head(p_db_member_list, &(p_member_info->list_head));
            CTC_ERROR_GOTO(_sys_goldengate_nh_mcast_write_dsmet(lchip,
                                                               &(p_member_info->dsmet)),
                           ret, error2);

    }
    else
    {

      /*2.2 New member is not the list's first member*/
       CTC_ERROR_GOTO(sys_goldengate_nh_offset_alloc(lchip,
                                                     p_member_info->dsmet.entry_type, 1, &new_met_offset), ret, error1);

       /*(1) insert new member into tail*/
       dsmet_io.met_offset = new_met_offset;
       dsmet_io.dsnh_offset = dsnh_offset;
       dsmet_io.p_next_mem = NULL;
       CTC_ERROR_GOTO(_sys_goldengate_nh_mcast_build_dsmet_info(lchip, p_info_mcast, &dsmet_io, &(p_member_info->dsmet)), ret, error4);
       /*The list have one member at least, so the tail node should not be NULL*/
       p_mem_flex = _ctc_container_of(ctc_list_pointer_node_tail(p_db_member_list),
                                      sys_nh_mcast_meminfo_t, list_head);
       /*Add this new member to db member list*/
       ctc_list_pointer_insert_tail(p_db_member_list, &(p_member_info->list_head));
       CTC_ERROR_GOTO(_sys_goldengate_nh_mcast_write_dsmet(lchip,
                                                          &(p_member_info->dsmet)),
                      ret, error3);

       /*(2) update previous member's next met offset*/
       p_mem_flex->dsmet.next_dsmet_offset = p_member_info->dsmet.dsmet_offset;
       CTC_UNSET_FLAG(p_mem_flex->dsmet.flag, SYS_NH_DSMET_FLAG_ENDLOCAL);

   CTC_ERROR_GOTO(_sys_goldengate_nh_mcast_write_dsmet(lchip,
                                                          &(p_mem_flex->dsmet)),
                      ret, error3);
    }

    return CTC_E_NONE;
error4:
  sys_goldengate_nh_offset_free(lchip, p_member_info->dsmet.entry_type,
                                1, p_member_info->dsmet.dsmet_offset);
  mem_free(p_member_info);
  return ret;
error3:
  sys_goldengate_nh_offset_free(lchip, p_member_info->dsmet.entry_type,
                                                           1, p_member_info->dsmet.dsmet_offset);
error2:
    ctc_list_pointer_delete(p_db_member_list, &(p_member_info->list_head));
error1:
    mem_free(p_member_info);
    return ret;
}

STATIC int32
_sys_goldengate_nh_mcast_del_dsmet_entry(uint8 lchip,
                                         sys_nh_info_mcast_t* p_info_mcast,
                                         sys_nh_mcast_meminfo_t* p_mem_info,
                                         ctc_list_pointer_t* p_db_member_list)
{
    sys_nh_mcast_meminfo_t* p_mem_target, * p_mem_flex;

    /*1. Init param*/
    p_mem_target = _ctc_container_of(ctc_list_pointer_head(p_db_member_list),
                                     sys_nh_mcast_meminfo_t, list_head);

    /*2. Remove member*/
    if (ctc_list_pointer_head(p_db_member_list) == (&(p_mem_info->list_head)))
    /*Target member is first member*/
    {
        SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Remove first memeber!\n");

        if (p_mem_target->list_head.p_next)
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Exist next member!\n");

            /*Get next member*/
            p_mem_flex = _ctc_container_of(p_mem_target->list_head.p_next,
                                           sys_nh_mcast_meminfo_t, list_head);

            CTC_ERROR_RETURN(sys_goldengate_nh_offset_free(lchip, p_mem_flex->dsmet.entry_type,
                                                           1, p_mem_flex->dsmet.dsmet_offset));

            p_mem_flex->dsmet.dsmet_offset = p_mem_target->dsmet.dsmet_offset;
            p_mem_flex->dsmet.entry_type = SYS_NH_ENTRY_TYPE_MET_6W;
            CTC_SET_FLAG(p_mem_flex->dsmet.flag, SYS_NH_DSMET_FLAG_GLOBAL_MET);
            CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_write_dsmet(lchip, &(p_mem_flex->dsmet)));

        }
        else
        /*This is the last member of this group in this chip, deinit basic met entry*/
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Remove member is the last member!\n");
            /*Free basic met offset*/
            CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_entry_set_default(lchip,
                                                                        p_info_mcast,
                                                                        p_mem_target->dsmet.dsmet_offset,
                                                                        TRUE, TRUE));
        }
    }
    else
    {
        p_mem_flex = _ctc_container_of(p_mem_info->list_head.p_prev, sys_nh_mcast_meminfo_t, list_head);
        if (SYS_NH_PARAM_MCAST_MEM_REMOTE != p_mem_info->dsmet.member_type)
        {
            if (CTC_FLAG_ISSET(p_mem_info->dsmet.flag, SYS_NH_DSMET_FLAG_ENDLOCAL))
            {
                SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Remove member is the last member!\n");
                CTC_SET_FLAG(p_mem_flex->dsmet.flag, SYS_NH_DSMET_FLAG_ENDLOCAL);
                if (p_info_mcast->stacking_met_offset)
                {
                    p_mem_info->dsmet.next_dsmet_offset = p_info_mcast->stacking_met_offset;
                }
                else
                {
                    p_mem_info->dsmet.next_dsmet_offset = SYS_NH_MET_END_REPLICATE_OFFSET;
                }
            }
            else
            {
                CTC_UNSET_FLAG(p_mem_flex->dsmet.flag, SYS_NH_DSMET_FLAG_ENDLOCAL);
            }
        }

        p_mem_flex->dsmet.next_dsmet_offset = p_mem_info->dsmet.next_dsmet_offset;
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_write_dsmet(lchip, &(p_mem_flex->dsmet)));
        CTC_ERROR_RETURN(sys_goldengate_nh_offset_free(lchip, p_mem_info->dsmet.entry_type,
                                                       1, p_mem_info->dsmet.dsmet_offset));
    }

    ctc_list_pointer_delete(p_db_member_list, &(p_mem_info->list_head));

    if (p_mem_info->dsmet.vid_list)
    {
        mem_free(p_mem_info->dsmet.vid_list);
    }

    mem_free(p_mem_info);
    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_nh_mcast_get_nh_and_edit_data(uint8 lchip, sys_nh_param_mcast_member_t* p_mem_param,
                                             sys_nh_mcast_meminfo_t* p_member_info,
                                             uint32* p_dsnh_offset, uint8* p_use_dsnh8w)
{

    uint32 dsnh_offset;

    *p_use_dsnh8w = 0;

    switch (p_mem_param->member_type)
    {
    case SYS_NH_PARAM_BRGMC_MEM_LOCAL:
        if (p_mem_param->is_mirror)
        {
            CTC_ERROR_RETURN(sys_goldengate_nh_get_resolved_offset(lchip,
                                 SYS_NH_RES_OFFSET_TYPE_MIRROR_NH, p_dsnh_offset));
        }
        else
        {
            CTC_ERROR_RETURN(sys_goldengate_nh_get_resolved_offset(lchip,
                                 SYS_NH_RES_OFFSET_TYPE_BRIDGE_NH, p_dsnh_offset));
        }

        break;

    case SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH:
    case SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE:
    case SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH_DESTID:

        CTC_ERROR_RETURN(sys_goldengate_nh_get_dsnh_offset_by_nhid(lchip,
                             p_mem_param->ref_nhid, &dsnh_offset, p_use_dsnh8w));
        *p_dsnh_offset = dsnh_offset;
        break;

    case SYS_NH_PARAM_MCAST_MEM_MIRROR_WITH_NH:
        CTC_ERROR_RETURN(sys_goldengate_nh_get_dsnh_offset_by_nhid(lchip,
                             p_mem_param->ref_nhid, &dsnh_offset, p_use_dsnh8w));
        *p_dsnh_offset = dsnh_offset;
        break;

    case SYS_NH_PARAM_IPMC_MEM_LOCAL:
    case SYS_NH_PARAM_IPMC_MEM_LOOP_BACK:
        {
            sys_l3if_prop_t l3if_prop;
            if (p_mem_param->is_linkagg)
            {
                l3if_prop.gport = CTC_MAP_TID_TO_GPORT(p_mem_param->destid);
            }
            else
            {
                uint8 gchip = 0;
                uint16 sys_lport = 0;
                CTC_ERROR_RETURN(sys_goldengate_get_gchip_id(lchip, &gchip));
                sys_lport = SYS_MAP_DRV_LPORT_TO_SYS_LPORT(p_mem_param->destid);
                l3if_prop.gport = CTC_MAP_LPORT_TO_GPORT(gchip, sys_lport);
                SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "L3if -> destid:0x%x, lport=%d, Gport:0x%x\n",
                               p_mem_param->destid, sys_lport, l3if_prop.gport);

            }

            l3if_prop.l3if_type = p_mem_param->l3if_type;
            l3if_prop.vlan_id = p_mem_param->vid & 0xFFF;

            if (SYS_NH_PARAM_IPMC_MEM_LOOP_BACK == p_mem_param->member_type)
            {
                l3if_prop.l3if_id = SYS_L3IF_RSV_L3IF_ID_FOR_INTERNAL_PORT;
            }
            else
            {
                CTC_ERROR_RETURN(sys_goldengate_l3if_get_l3if_info(lchip, 0, &l3if_prop));
            }
            CTC_ERROR_RETURN(sys_goldengate_nh_get_ipmc_dsnh_offset(lchip, l3if_prop.l3if_id, p_mem_param->mtu_no_chk, p_dsnh_offset));

        }
        break;

    case SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP:
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_process_dsnh(lchip, p_mem_param, p_member_info, p_dsnh_offset));
        break;

    case SYS_NH_PARAM_MCAST_MEM_REMOTE:
        break;

    case SYS_NH_PARAM_BRGMC_MEM_RAPS:
        *p_dsnh_offset = 0;
        break;

    default:
        return CTC_E_INVALID_PARAM;
    }

    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_nh_mcast_free_nh_and_edit_data(uint8 lchip,
                                              sys_nh_mcast_meminfo_t* p_member_info,
                                              uint32 del_pos)
{
    uint32 dsnh_offset_del, dsnh_offset_last;

    if (p_member_info->dsmet.member_type != SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP)
    {
        return CTC_E_NONE;
    }

    if (p_member_info->dsmet.replicate_num > 0)
    /*Remove none last member*/
    {
        dsnh_offset_last  = p_member_info->dsmet.dsnh_offset + p_member_info->dsmet.replicate_num;
        dsnh_offset_del  = p_member_info->dsmet.dsnh_offset + del_pos;
        /*Update dsnh table*/
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_repli_update_dsnh(lchip, dsnh_offset_last, dsnh_offset_del, 1));

        p_member_info->dsmet.vid_list[del_pos] = p_member_info->dsmet.vid_list[p_member_info->dsmet.replicate_num];
        if (1 == p_member_info->dsmet.replicate_num)
        {
            p_member_info->dsmet.vid = p_member_info->dsmet.vid_list[del_pos];
        }

        p_member_info->dsmet.free_dsnh_offset_cnt++;
        if (SYS_NH_MCAST_REPLI_INCR_STEP == p_member_info->dsmet.free_dsnh_offset_cnt)
        {
            p_member_info->dsmet.free_dsnh_offset_cnt = 0;
            CTC_ERROR_RETURN(sys_goldengate_nh_offset_free(lchip,
                                                          SYS_NH_ENTRY_TYPE_NEXTHOP_4W,
                                                          SYS_NH_MCAST_REPLI_INCR_STEP,
                                                          dsnh_offset_last));
        }
    }
    else
    /*Remove last member*/
    {
        uint8 entry_num = 0;

        if (p_member_info->dsmet.vid_list)
        {
            entry_num = SYS_NH_MCAST_REPLI_INCR_STEP;
        }
        else
        {
            entry_num = 1;
        }

        CTC_ERROR_RETURN(sys_goldengate_nh_offset_free(lchip,
                                                       SYS_NH_ENTRY_TYPE_NEXTHOP_4W,
                                                       entry_num,
                                                       p_member_info->dsmet.dsnh_offset));
    }

    return CTC_E_NONE;
}

int32
_sys_goldengate_nh_mcast_add_member(uint8 lchip, uint32 basic_met_offset,
                                   sys_nh_param_mcast_member_t* p_mem_param,
                                   ctc_list_pointer_t* p_db_member_list,
                                   sys_nh_info_mcast_t* p_info_mcast)
{
    sys_nh_mcast_meminfo_t* p_member_info = NULL;
    bool entry_exist = FALSE;
    uint8 use_dsnh8w = 0;
    uint8 stk_rsv_mem = 0;
    uint32 repli_pos = 0;
    uint32 dsnh_offset = 0;
    int32 ret = 0;

    /*1. Check if mcast member have been existed*/
    CTC_PTR_VALID_CHECK(p_mem_param);

	if (p_mem_param->is_linkagg &&
        (p_mem_param->destid >= SYS_GG_MAX_LINKAGG_GROUP_NUM))
    {
        return CTC_E_INVALID_TID;
    }

    if ((SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE != p_mem_param->member_type) &&
        (SYS_NH_PARAM_MCAST_MEM_REMOTE != p_mem_param->member_type) &&
    (p_mem_param->destid >= SYS_GG_MAX_PORT_NUM_PER_CHIP) &&
    !sys_goldengate_nh_is_cpu_nhid(lchip, p_mem_param->ref_nhid))
    {
        return CTC_E_INVALID_LOCAL_PORT;
    }

    p_mem_param->is_mirror = p_info_mcast->is_mirror;
    if (p_mem_param->is_mirror
        && (p_mem_param->member_type == SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH))
    {
        p_mem_param->member_type = SYS_NH_PARAM_MCAST_MEM_MIRROR_WITH_NH;
    }

    if (p_info_mcast->is_logic_rep_en && (p_mem_param->member_type == SYS_NH_PARAM_IPMC_MEM_LOCAL))
    {
        p_mem_param->member_type = SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP;
    }


    if (!p_mem_param->is_linkagg &&
		p_mem_param->member_type != SYS_NH_PARAM_MCAST_MEM_REMOTE &&
         p_mem_param->member_type != SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE)
    {
        p_mem_param->destid = SYS_MAP_SYS_LPORT_TO_DRV_LPORT(p_mem_param->destid);
        SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "p_mem_param->dest(id:0x%x\n", p_mem_param->destid);

        if ((p_mem_param->member_type == SYS_NH_PARAM_BRGMC_MEM_LOCAL ||
            p_mem_param->member_type == SYS_NH_PARAM_IPMC_MEM_LOCAL)
        && SYS_MC_DESTID_IS_INTERNAL(p_mem_param->destid))
        {
            return CTC_E_INVALID_PARAM;
        }
    }

    /*Add remote member*/
    if (SYS_NH_PARAM_MCAST_MEM_REMOTE == p_mem_param->member_type)
    {
        /* mcast trunk profile, check exist */
        if (p_info_mcast->stacking_mcast_profile_id)
        {
            return CTC_E_EXIST;
        }

        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_add_remote_dsmet_entry( lchip,  p_mem_param,
                                                                         basic_met_offset,
                                                                         p_db_member_list,
                                                                         p_info_mcast));
        return CTC_E_NONE;
    }

    CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_analyze_member(lchip, p_db_member_list,
                                                            p_mem_param, &p_member_info,
                                                            &entry_exist, &repli_pos));
    if (entry_exist)
    {
        return CTC_E_NONE;
    }

    /*reflective member only used for new added members,and can only be used in linked lists*/
     CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_get_port_reflect(lchip, p_mem_param));
    if(p_mem_param->is_src_port_check_dis || p_mem_param->leaf_check_en)
    {
       p_member_info = NULL;
    }
    if (p_member_info
        && (p_mem_param->member_type == SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP)
        && ((p_member_info->dsmet.replicate_num + 1) == SYS_NH_LOGICAL_REPLI_MAX_NUM))
    {
        p_member_info = NULL;
    }

    sys_goldengate_stacking_get_rsv_trunk_number(lchip, &stk_rsv_mem);
    if (((!p_member_info) || CTC_FLAG_ISSET(p_member_info->dsmet.flag, SYS_NH_DSMET_FLAG_USE_PBM))
        && ((p_info_mcast->physical_replication_num + stk_rsv_mem) >= SYS_NH_PHY_REP_MAX_NUM))
    {
        return CTC_E_NH_EXCEED_MCAST_PHY_REP_NUM;
    }

    /*3. Op  DsNH*/
    CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_get_nh_and_edit_data(lchip,
                         p_mem_param, p_member_info, &dsnh_offset, &use_dsnh8w));

    /*3. Op DsMet*/
    if (p_member_info)
    {
        SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "2. Update old met!!!!!!!!!!!!\n");

        SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "\ndsnh_offset:%d use_dsnh8w:%d replicate_num:%d\n",
                       dsnh_offset, use_dsnh8w, p_member_info->dsmet.replicate_num);

        if (CTC_FLAG_ISSET(p_member_info->dsmet.flag, SYS_NH_DSMET_FLAG_USE_PBM))
        {
            uint8 port_type = 0;
            uint8 port_id = 0;
            if (!p_mem_param->is_linkagg)
            {
                SYS_MC_MAP_PBM_PORT_ID(p_mem_param->destid, port_type, port_id);
            }
            else
            {
                port_id = p_mem_param->destid;
            }

            p_member_info->dsmet.port_type= port_type;
            CTC_BMP_SET(p_member_info->dsmet.pbm, port_id);
            p_member_info->dsmet.entry_type = SYS_NH_ENTRY_TYPE_MET_6W;
            p_member_info->dsmet.dsnh_offset = dsnh_offset;
            p_member_info->dsmet.vid = p_mem_param->vid;
            if (p_member_info->dsmet.ucastid != SYS_NH_MET_DROP_UCAST_ID)
            {
                p_member_info->dsmet.replicate_num += 1;
                p_info_mcast->physical_replication_num += 1;
            }

            p_member_info->dsmet.ucastid = 0;


        }
        else if (SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP == p_mem_param->member_type)
        {
            p_member_info->dsmet.replicate_num += 1;
        }
        else
        {
            return CTC_E_NONE;
        }

        p_member_info->dsmet.flag |= use_dsnh8w ? SYS_NH_DSMET_FLAG_USE_DSNH8W : 0;
        if (NULL == p_member_info->list_head.p_next)
        {
            p_member_info->dsmet.next_dsmet_offset = p_info_mcast->stacking_met_offset;
        }
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_write_dsmet(lchip, &p_member_info->dsmet));
    }
    else
    {
        SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "1. First/New met!!!!!!!!!!!!\n");

        ret = _sys_goldengate_nh_mcast_add_dsmet_entry(lchip, p_mem_param,
                                                       basic_met_offset,
                                                       dsnh_offset,
                                                       use_dsnh8w,
                                                       p_db_member_list,
                                                       p_info_mcast);
        if (ret == CTC_E_NONE)
        {
            p_info_mcast->physical_replication_num += 1;
        }

    }

    return ret;
}

int32
_sys_goldengate_nh_mcast_remove_member(uint8 lchip, sys_nh_param_mcast_member_t* p_mem_param,
                                      ctc_list_pointer_t* p_db_member_list,
                                      sys_nh_info_mcast_t* p_info_mcast)
{
    sys_nh_mcast_meminfo_t* p_member_info = NULL;
    bool entry_exist = FALSE;
    uint32 repli_pos;



    /*1. Check if mcast member have been existed*/
    if (p_mem_param->is_linkagg &&
        (p_mem_param->destid > SYS_GG_MAX_LINKAGG_GROUP_NUM))
    {
        return CTC_E_INVALID_TID;
    }

    p_mem_param->is_mirror = p_info_mcast->is_mirror;
    if (p_mem_param->is_mirror
        && (p_mem_param->member_type == SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH))
    {
        p_mem_param->member_type = SYS_NH_PARAM_MCAST_MEM_MIRROR_WITH_NH;
    }

    if (p_info_mcast->is_logic_rep_en
        && (p_mem_param->member_type == SYS_NH_PARAM_IPMC_MEM_LOCAL))
    {
        p_mem_param->member_type = SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP;
    }


    if (!p_mem_param->is_linkagg &&
	    p_mem_param->member_type != SYS_NH_PARAM_MCAST_MEM_REMOTE &&
        p_mem_param->member_type != SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE)
    {
        p_mem_param->destid = SYS_MAP_SYS_LPORT_TO_DRV_LPORT(p_mem_param->destid);

        if ((p_mem_param->member_type == SYS_NH_PARAM_BRGMC_MEM_LOCAL ||
            p_mem_param->member_type == SYS_NH_PARAM_IPMC_MEM_LOCAL)
        && SYS_MC_DESTID_IS_INTERNAL(p_mem_param->destid))
        {
            return CTC_E_INVALID_PARAM;
        }
    }
    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "p_mem_param->destid:0x%x\n", p_mem_param->destid);

    /*Remove remote member*/
    if (SYS_NH_PARAM_MCAST_MEM_REMOTE == p_mem_param->member_type)
    {
        /* mcast trunk profile, check exist */
        if ((0 == p_info_mcast->stacking_mcast_profile_id)
            ||(p_info_mcast->stacking_mcast_profile_id != p_mem_param->destid))
        {
            return CTC_E_NOT_EXIST;
        }

        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_remove_remote_dsmet_entry( lchip,  p_mem_param,
                                                                            p_info_mcast->basic_met_offset,
                                                                            p_db_member_list,
                                                                            p_info_mcast));
        return CTC_E_NONE;
    }


    CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_analyze_member(lchip, p_db_member_list,
                                                            p_mem_param, &p_member_info,  &entry_exist, &repli_pos));
    if (!entry_exist || NULL == p_member_info)
    {
        return CTC_E_MEMBER_NOT_EXIST;
    }

    CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_free_nh_and_edit_data(lchip,
                                                                   p_member_info,  repli_pos));

    if (p_member_info->dsmet.replicate_num == 0)
    {

        /*Remove member node*/
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_del_dsmet_entry(lchip,
                                                                  p_info_mcast,
                                                                  p_member_info,
                                                                  p_db_member_list));
        if (ctc_list_pointer_head(p_db_member_list) != (ctc_list_pointer_node_t*)p_member_info)
        {
            p_info_mcast->physical_replication_num -= 1;
        }

    }
    else
    {
        if (CTC_FLAG_ISSET(p_member_info->dsmet.flag, SYS_NH_DSMET_FLAG_USE_PBM))
        {
            uint8 port_id = 0;
            if (!p_mem_param->is_linkagg)
            {
                port_id = SYS_MC_MAP_PORT_ID(p_mem_param->destid);
            }
            else
            {
                port_id = p_mem_param->destid;
            }
            CTC_BMP_UNSET(p_member_info->dsmet.pbm, port_id);
            p_member_info->dsmet.replicate_num -= 1;
            p_info_mcast->physical_replication_num -= 1;

        }
        else if (SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP == p_mem_param->member_type)
        {
            p_member_info->dsmet.replicate_num -= 1;
        }

        if (NULL == p_member_info->list_head.p_next)
        {
            p_member_info->dsmet.next_dsmet_offset = p_info_mcast->stacking_met_offset;
        }
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_write_dsmet(lchip, &p_member_info->dsmet));
    }

    return CTC_E_NONE;
}
STATIC int32
_sys_goldengate_nh_mcast_add_dsfwd(uint8 lchip, sys_nh_info_com_t* p_com_db)
{
    sys_nh_info_mcast_t* p_nh_info = NULL;
    sys_nh_param_dsfwd_t dsfwd_param;
	int32 ret = 0;

    p_nh_info     = (sys_nh_info_mcast_t*)(p_com_db);
    sal_memset(&dsfwd_param, 0, sizeof(sys_nh_param_dsfwd_t));

    /*Build DsFwd Table and write table*/
	if (!CTC_FLAG_ISSET(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
    {
       ret = sys_goldengate_nh_offset_alloc(lchip, SYS_NH_ENTRY_TYPE_FWD, 1, &p_nh_info->hdr.dsfwd_info.dsfwd_offset);
    }
    CTC_SET_FLAG(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD);

    sys_goldengate_get_gchip_id(lchip, & dsfwd_param.dest_chipid);


    dsfwd_param.drop_pkt = FALSE;
    dsfwd_param.dest_id = 0xFFFF & (p_nh_info->basic_met_offset/2);
    dsfwd_param.is_mcast = TRUE;
    dsfwd_param.dsfwd_offset = p_nh_info->hdr.dsfwd_info.dsfwd_offset;
    dsfwd_param.dsnh_offset = 0;
    dsfwd_param.stats_ptr = p_nh_info->hdr.dsfwd_info.stats_ptr;
    /*Write table*/
    ret = sys_goldengate_nh_write_entry_dsfwd(lchip, &dsfwd_param);

    if (ret != CTC_E_NONE)
    {
      sys_goldengate_nh_offset_free(lchip,  SYS_NH_ENTRY_TYPE_FWD, 1, p_nh_info->hdr.dsfwd_info.dsfwd_offset);

    }
	return ret;
}

STATIC int32
_sys_goldengate_nh_mcast_map_member_info(uint8 lchip, ctc_mcast_nh_param_member_t * p_member_info,
                                         sys_nh_info_dsmet_t * p_dsmet, ctc_nh_info_t* p_nh_info,
                                         uint32 * p_valid_cnt, uint32 * p_member_idx, uint16 vid_index)
{
    uint8 gchip = 0;
    uint8 is_linkagg = 0;
    switch (p_dsmet->member_type)
    {
        case SYS_NH_PARAM_BRGMC_MEM_LOCAL:
        case SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE:
        case SYS_NH_PARAM_BRGMC_MEM_RAPS:
        case SYS_NH_PARAM_MCAST_MEM_MIRROR_WITH_NH:
            p_member_info->member_type = CTC_NH_PARAM_MEM_BRGMC_LOCAL;
            break;
        case SYS_NH_PARAM_IPMC_MEM_LOCAL:
        case SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP:
        case SYS_NH_PARAM_IPMC_MEM_LOOP_BACK:
            p_member_info->member_type = CTC_NH_PARAM_MEM_IPMC_LOCAL;
            break;
        case SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH:
        case SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH_DESTID:
            p_member_info->member_type = CTC_NH_PARAM_MEM_LOCAL_WITH_NH;
            p_member_info->ref_nhid    = p_dsmet->ref_nhid;
            break;
        case SYS_NH_PARAM_MCAST_MEM_REMOTE:
            p_member_info->member_type = CTC_NH_PARAM_MEM_REMOTE;
            break;
        default:
            p_member_info->member_type = CTC_NH_PARAM_MEM_INVALID;
            break;
    }


    is_linkagg = CTC_FLAG_ISSET(p_dsmet->flag, SYS_NH_DSMET_FLAG_IS_LINKAGG);
    if (is_linkagg)
    {
        gchip = 0x1F;
    }
    else
    {
        sys_goldengate_get_gchip_id(lchip, &gchip);
    }
    p_member_info->gchip_id = gchip;
    p_member_info->is_source_check_dis = !CTC_FLAG_ISSET(p_dsmet->flag, SYS_NH_DSMET_FLAG_PORT_CHK_DISCARD)? 1 : 0;

    if ((SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP == p_dsmet->member_type)&&(p_dsmet->replicate_num))
    {
        p_member_info->vid = p_dsmet->vid_list[vid_index];
    }
    else
    {
        p_member_info->vid = p_dsmet->vid;
    }


    p_member_info->is_reflective = CTC_FLAG_ISSET(p_dsmet->flag, SYS_NH_DSMET_FLAG_REFECTIVE);
    p_member_info->destid = p_dsmet->ucastid;

    if (CTC_FLAG_ISSET(p_dsmet->flag, SYS_NH_DSMET_FLAG_USE_PBM))
    {
        p_member_info->port_bmp[0] = p_dsmet->pbm[0];
        p_member_info->port_bmp[1] = p_dsmet->pbm[1];
        p_member_info->port_bmp[8] = p_dsmet->pbm[2];
        p_member_info->port_bmp[9] = p_dsmet->pbm[3];
        p_member_info->port_bmp_en = 1;

    }

    if (CTC_NH_PARAM_MEM_IPMC_LOCAL == p_member_info->member_type)
    {
        if ((p_member_info->port_bmp_en)||(SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP == p_dsmet->member_type))
        {
            p_member_info->l3if_type = CTC_L3IF_TYPE_VLAN_IF;
        }
        else if (p_dsmet->vid)
        {
            p_member_info->l3if_type = CTC_L3IF_TYPE_SUB_IF;
        }
        else
        {
            p_member_info->l3if_type = CTC_L3IF_TYPE_PHY_IF;
        }
    }

    return CTC_E_NONE;

}

int32
sys_goldengate_nh_get_mcast_member_info(uint8 lchip, sys_nh_info_mcast_t* p_mcast_db, ctc_nh_info_t* p_nh_info)
{
    ctc_list_pointer_t* p_db_member_list = NULL;
    sys_nh_mcast_meminfo_t* p_meminfo = NULL;
    ctc_list_pointer_node_t* p_pos_mem = NULL;
    sys_nh_info_dsmet_t * p_dsmet = NULL;
    ctc_mcast_nh_param_member_t * p_member_info = NULL;
    uint32 valid_cnt = 0;
    uint32 member_idx = 0;
    uint32 ret = CTC_E_NONE;
    uint8 not_end = 0;
    uint16 vid_index = 0;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "\n%s()\n", __FUNCTION__);
    if (0 == p_nh_info->buffer_len)
    {
        return CTC_E_INVALID_PARAM;
    }

    p_nh_info->valid_number = 0;
    p_member_info = p_nh_info->buffer;
    member_idx = 0;
    valid_cnt = 0;
    p_db_member_list = &(p_mcast_db->p_mem_list);


    CTC_LIST_POINTER_LOOP(p_pos_mem, p_db_member_list)
    {
        p_meminfo = _ctc_container_of(p_pos_mem, sys_nh_mcast_meminfo_t, list_head);
        if (NULL == p_meminfo)
        {
            break;
        }
        p_dsmet = &(p_meminfo->dsmet);
        if ((SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP == p_dsmet->member_type)
            &&(p_dsmet->replicate_num))
        {
            for (vid_index = 0; vid_index <= p_dsmet->replicate_num; vid_index++)
            {
                member_idx++;
                if (member_idx <= p_nh_info->start_index)
                {
                    continue;
                }
                if (p_nh_info->buffer_len <= valid_cnt)
                {
                    not_end = 1;
                    goto end;
                }
                ret = _sys_goldengate_nh_mcast_map_member_info(lchip, p_member_info, p_dsmet, p_nh_info,
                                                               &valid_cnt, &member_idx, vid_index);
                if (CTC_E_NONE != ret)
                {
                    not_end = 1;
                    goto end;
                }
                valid_cnt++;
                p_member_info++;
            }
        }
        else
        {
            member_idx++;
            if (member_idx <= p_nh_info->start_index)
            {
                continue;
            }
            if (p_nh_info->buffer_len <= valid_cnt)
            {
                not_end = 1;
                break;
            }
            ret = _sys_goldengate_nh_mcast_map_member_info(lchip, p_member_info, p_dsmet, p_nh_info,
                                                           &valid_cnt, &member_idx, 0);
            if (CTC_E_NONE != ret)
            {
                not_end = 1;
                break;
            }
            valid_cnt++;
            p_member_info++;
        }
    }

end:
    p_nh_info->next_query_index += valid_cnt;
    if (!not_end)
    {
        p_nh_info->is_end = 1;
        p_nh_info->next_query_index = 0;
    }
    p_nh_info->valid_number = valid_cnt;

    return CTC_E_NONE;
}

int32
sys_goldengate_nh_create_mcast_cb(uint8 lchip, sys_nh_param_com_t* p_com_nh_para,
                                 sys_nh_info_com_t* p_com_db)
{
    sys_nh_param_mcast_t* p_nh_para_mcast;
    sys_nh_info_mcast_t* p_mcast_db;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    /* 1. sanity check & init */
    CTC_PTR_VALID_CHECK(p_com_nh_para);
    CTC_PTR_VALID_CHECK(p_com_db);
    CTC_EQUAL_CHECK(SYS_NH_TYPE_MCAST, p_com_nh_para->hdr.nh_param_type);
    p_nh_para_mcast = (sys_nh_param_mcast_t*)p_com_nh_para;
    p_mcast_db = (sys_nh_info_mcast_t*)p_com_db;
    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "nhType = %d, NHID = %d, isInternaNH = %d, \n\
        GroupId = %d, Opcode = %d\n",
                   p_nh_para_mcast->hdr.nh_param_type,
                   p_nh_para_mcast->hdr.nhid,
                   p_nh_para_mcast->hdr.is_internal_nh,
                   p_nh_para_mcast->groupid,
                   p_nh_para_mcast->opcode);

    /*2. Malloc and init DB structure*/

    p_mcast_db->hdr.nh_entry_type = SYS_NH_TYPE_MCAST;
    p_mcast_db->basic_met_offset = p_nh_para_mcast->groupid*2;
    p_mcast_db->is_mirror = p_nh_para_mcast->is_mirror;
    p_mcast_db->nhid      = p_nh_para_mcast->hdr.nhid;
    CTC_ERROR_RETURN(sys_goldengate_nh_check_glb_met_offset(lchip, p_mcast_db->basic_met_offset, 2, TRUE));
    if (p_nh_para_mcast->hdr.stats_valid)
    {
        CTC_ERROR_RETURN(sys_goldengate_stats_get_statsptr(lchip,
                                                           p_nh_para_mcast->hdr.stats_id,
                                                           CTC_STATS_STATSID_TYPE_NEXTHOP_MCAST,
                                                           &p_nh_para_mcast->hdr.stats_ptr));
        p_mcast_db->hdr.dsfwd_info.stats_ptr = p_nh_para_mcast->hdr.stats_ptr;
    }

    CTC_ERROR_RETURN(sys_goldengate_stacking_get_mcast_profile_met_offset(lchip, p_mcast_db->stacking_mcast_profile_id, &p_mcast_db->stacking_met_offset));

    ctc_list_pointer_init(&p_mcast_db->p_mem_list);

    /*Init basic dsmet entry*/
    CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_entry_set_default(lchip,
                                                                p_mcast_db,
                                                                p_mcast_db->basic_met_offset,
                                                                TRUE,
                                                                TRUE));
    CTC_ERROR_RETURN(sys_goldengate_nh_set_glb_met_offset(lchip, p_mcast_db->basic_met_offset, 2, TRUE));

    CTC_ERROR_RETURN(sys_goldengate_nh_add_mcast_db(lchip, p_mcast_db->basic_met_offset/2, p_mcast_db));

    p_mcast_db->is_logic_rep_en = _sys_goldengate_nh_is_ipmc_logic_rep_enable(lchip)?1:0;

    if (!p_nh_para_mcast->hdr.have_dsfwd)
    {
       return CTC_E_NONE;
    }

	CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_add_dsfwd(lchip, p_com_db));



    return CTC_E_NONE;
}

/**
 @brief Callback function to delete multicast bridge nexthop

 @param[in] p_data, pointer used to store nexthop data

 @return CTC_E_XXX
 */
int32
sys_goldengate_nh_delete_mcast_cb(uint8 lchip, sys_nh_info_com_t* p_data, uint32* p_nhid)
{
    sys_nh_info_mcast_t* p_mcast_db;

    ctc_list_pointer_node_t* p_pos, * p_pos_next;
    sys_nh_mcast_meminfo_t* p_member;
    sys_nh_param_dsfwd_t dsfwd_param;
    bool first_node = TRUE;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_PTR_VALID_CHECK(p_data);
    CTC_EQUAL_CHECK(SYS_NH_TYPE_MCAST, p_data->hdr.nh_entry_type);
    p_mcast_db = (sys_nh_info_mcast_t*)(p_data);


    sal_memset(&dsfwd_param, 0, sizeof(sys_nh_param_dsfwd_t));


    first_node = TRUE;
    /*Free basic met offset*/
    CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_entry_set_default(lchip,
                                                                p_mcast_db,
                                                                p_mcast_db->basic_met_offset, FALSE, TRUE));

    CTC_LIST_POINTER_LOOP_DEL(p_pos, p_pos_next, &p_mcast_db->p_mem_list)
    {
        p_member = _ctc_container_of(p_pos, sys_nh_mcast_meminfo_t, list_head);
        if (first_node)
        {
            first_node = FALSE;
        }
        else
        {

            CTC_ERROR_RETURN(sys_goldengate_nh_offset_free(lchip, \
            p_member->dsmet.entry_type, 1, p_member->dsmet.dsmet_offset));

        }

        if (SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP == p_member->dsmet.member_type)
        {
            uint8 entry_num = 0;

            if (p_member->dsmet.vid_list)
            {
                mem_free(p_member->dsmet.vid_list);
                entry_num = (p_member->dsmet.replicate_num / SYS_NH_MCAST_REPLI_INCR_STEP + 1)*SYS_NH_MCAST_REPLI_INCR_STEP;
            }
            else
            {
                entry_num = 1;
            }

            CTC_ERROR_RETURN(sys_goldengate_nh_offset_free(lchip,
                                                           SYS_NH_ENTRY_TYPE_NEXTHOP_4W,
                                                           entry_num,
                                                           p_member->dsmet.dsnh_offset));

        }

        mem_free(p_member);

    }

    if (CTC_FLAG_ISSET(p_mcast_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
    {
        if (p_mcast_db->hdr.dsfwd_info.stats_ptr != 0)
        {
             /*sys_goldengate_stats_free_flow_stats_ptr(lchip, SYS_STATS_FLOW_TYPE_DSFWD, p_mcast_db->hdr.dsfwd_info.stats_ptr);*/
        }

        CTC_ERROR_RETURN(sys_goldengate_nh_offset_free(lchip, \
        SYS_NH_ENTRY_TYPE_FWD, 1, \
        p_mcast_db->hdr.dsfwd_info.dsfwd_offset));
        dsfwd_param.dsfwd_offset = p_mcast_db->hdr.dsfwd_info.dsfwd_offset;

        dsfwd_param.drop_pkt = 1;
        dsfwd_param.dsnh_offset = 0;
        /*Write table*/
        CTC_ERROR_RETURN(sys_goldengate_nh_write_entry_dsfwd(lchip, &dsfwd_param));
    }
    CTC_ERROR_RETURN(sys_goldengate_nh_set_glb_met_offset(lchip, p_mcast_db->basic_met_offset, 2, FALSE));
    CTC_ERROR_RETURN(sys_goldengate_nh_del_mcast_db(lchip, p_mcast_db->basic_met_offset / 2));

    return CTC_E_NONE;
}

/**
 @brief Callback function used to update bridge multicast nexthop

 @param[in] p_nh_ptr, pointer of multicast nexthop DB

 @param[in] p_para, member information

 @return CTC_E_XXX
 */
int32
sys_goldengate_nh_update_mcast_cb(uint8 lchip, sys_nh_info_com_t* p_nh_info,
                                 sys_nh_param_com_t* p_para /*Member info*/)
{

    sys_nh_param_mcast_t* p_nh_para_mcast;
    sys_nh_info_mcast_t* p_mcast_db;
    uint8 gchip;
    sys_nh_param_dsfwd_t dsfwd_param;
    bool update_dsfwd = FALSE;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_PTR_VALID_CHECK(p_para);
    CTC_PTR_VALID_CHECK(p_nh_info);
    CTC_EQUAL_CHECK(SYS_NH_TYPE_MCAST, p_para->hdr.nh_param_type);
    if (SYS_NH_TYPE_IP_TUNNEL != p_nh_info->hdr.nh_entry_type)
    {
        CTC_EQUAL_CHECK(SYS_NH_TYPE_MCAST, p_nh_info->hdr.nh_entry_type);
    }
    p_nh_para_mcast = (sys_nh_param_mcast_t*)(p_para);

    if(p_nh_para_mcast->hdr.change_type == SYS_NH_CHANGE_TYPE_ADD_DYNTBL)
    {
          if (!CTC_FLAG_ISSET(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
          {
              _sys_goldengate_nh_mcast_add_dsfwd(lchip,  p_nh_info);
          }
          return CTC_E_NONE;
     }

    if (NULL == p_nh_para_mcast->p_member)
    {
        return CTC_E_NONE;
    }

    sal_memset(&dsfwd_param, 0, sizeof(sys_nh_param_dsfwd_t));
    p_mcast_db = (sys_nh_info_mcast_t*)(p_nh_info);
    CTC_ERROR_RETURN(sys_goldengate_stacking_get_mcast_profile_met_offset(lchip, p_mcast_db->stacking_mcast_profile_id, &p_mcast_db->stacking_met_offset));

    if (p_nh_para_mcast->p_member->member_type == SYS_NH_PARAM_BRGMC_MEM_LOCAL_WITH_APS_BRIDGE
        || p_nh_para_mcast->p_member->member_type == SYS_NH_PARAM_MCAST_MEM_LOCAL_WITH_NH )
    {
        sys_nh_info_com_t* p_nhinfo;
        CTC_ERROR_RETURN(sys_goldengate_nh_get_nhinfo_by_nhid(lchip,   p_nh_para_mcast->p_member->ref_nhid, &p_nhinfo));
        if (p_nhinfo && CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_LOGIC_PORT))
        {
            if (SYS_NH_TYPE_BRGUC == p_nhinfo->hdr.nh_entry_type)
            {
                p_nh_para_mcast->p_member->logic_port = ((sys_nh_info_brguc_t*)p_nhinfo)->dest_logic_port;
            }
            else if(SYS_NH_TYPE_MPLS == p_nhinfo->hdr.nh_entry_type)
            {
                p_nh_para_mcast->p_member->logic_port = ((sys_nh_info_mpls_t*)p_nhinfo)->dest_logic_port;
            }
            else if (SYS_NH_TYPE_IP_TUNNEL == p_nhinfo->hdr.nh_entry_type)
            {
                p_nh_para_mcast->p_member->logic_port = ((sys_nh_info_ip_tunnel_t*)p_nhinfo)->dest_logic_port;
            }
            p_nh_para_mcast->p_member->is_logic_port = 1;
        }

        if (p_nhinfo && (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HORIZON_SPLIT_EN)))
        {
            p_nh_para_mcast->p_member->is_horizon_split = 1;
        }
    }

    switch (p_nh_para_mcast->opcode)
    {
    case SYS_NH_PARAM_MCAST_ADD_MEMBER:
        if (ctc_list_pointer_empty(&(p_mcast_db->p_mem_list)))
        {
            update_dsfwd = TRUE;
        }

        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_add_member(lchip, p_mcast_db->basic_met_offset,
                                                            p_nh_para_mcast->p_member,
                                                            &(p_mcast_db->p_mem_list),
                                                            p_mcast_db));

        if (update_dsfwd
            && CTC_FLAG_ISSET(p_mcast_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
        {
            sys_goldengate_get_gchip_id(lchip, &gchip);
            dsfwd_param.dsfwd_offset = p_mcast_db->hdr.dsfwd_info.dsfwd_offset;

            dsfwd_param.dest_chipid = gchip;
            dsfwd_param.drop_pkt = FALSE;
            dsfwd_param.dest_id = 0xFFFF & (p_mcast_db->basic_met_offset/2);
            dsfwd_param.is_mcast = TRUE;
            dsfwd_param.dsnh_offset = 0;    /*This value should be 0 for multicast*/
            dsfwd_param.stats_ptr = p_mcast_db->hdr.dsfwd_info.stats_ptr;
            /*Write table*/
            CTC_ERROR_RETURN(sys_goldengate_nh_write_entry_dsfwd(lchip, &dsfwd_param));
        }

        break;

    case SYS_NH_PARAM_MCAST_DEL_MEMBER:
        CTC_ERROR_RETURN(_sys_goldengate_nh_mcast_remove_member(lchip, p_nh_para_mcast->p_member,
                                                               &(p_mcast_db->p_mem_list),
                                                               p_mcast_db));
        break;
    default:
        return CTC_E_INVALID_PARAM;
    }

    return CTC_E_NONE;
}

int32
sys_goldengate_nh_remove_all_members(uint8 lchip,  uint32 nhid)
{
    sys_nh_info_com_t*  p_nh_info = NULL;
    sys_nh_info_mcast_t* p_mcast_db = NULL;
    ctc_list_pointer_node_t* p_pos, * p_pos_next;
    sys_nh_mcast_meminfo_t* p_member;
    bool first_node = TRUE;

    CTC_ERROR_RETURN(sys_goldengate_nh_get_nhinfo_by_nhid(lchip, nhid, &p_nh_info));

    p_mcast_db = (sys_nh_info_mcast_t*)p_nh_info;

    _sys_goldengate_nh_mcast_entry_set_default(lchip, p_mcast_db,  p_mcast_db->basic_met_offset, FALSE, TRUE);

    CTC_LIST_POINTER_LOOP_DEL(p_pos, p_pos_next, &p_mcast_db->p_mem_list)
    {
        p_member = _ctc_container_of(p_pos, sys_nh_mcast_meminfo_t, list_head);
        if (first_node)
        {
            first_node = FALSE;
        }
        else
        {
            sys_goldengate_nh_offset_free(lchip, p_member->dsmet.entry_type, 1, p_member->dsmet.dsmet_offset);
        }


        if (SYS_NH_PARAM_IPMC_MEM_LOCAL_LOGIC_REP == p_member->dsmet.member_type)
        {
            uint8 entry_num = 0;

            if (p_member->dsmet.vid_list)
            {
                mem_free(p_member->dsmet.vid_list);
                entry_num = (p_member->dsmet.replicate_num / SYS_NH_MCAST_REPLI_INCR_STEP + 1)*SYS_NH_MCAST_REPLI_INCR_STEP;
            }
            else
            {
                entry_num = 1;
            }

            sys_goldengate_nh_offset_free(lchip, SYS_NH_ENTRY_TYPE_NEXTHOP_4W, entry_num, p_member->dsmet.dsnh_offset);
        }

        ctc_list_pointer_delete(&p_mcast_db->p_mem_list, p_pos);
        mem_free(p_member);
    }

    return CTC_E_NONE;
}


