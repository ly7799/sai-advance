/**
 @file sys_usw_nexthop_aps.c

 @date 2020-02-25

 @version v2.0

 The file contains all aps related function
*/

/****************************************************************************
*
* Header Files
*
****************************************************************************/
#include "ctc_const.h"
#include "ctc_error.h"
#include "sys_usw_chip.h"
#include "sys_usw_nexthop_api.h"
#include "sys_usw_nexthop.h"
#include "sys_usw_nexthop_hw.h"
#include "sys_usw_wb_nh.h"
#include "sys_usw_register.h"
#include "sys_usw_aps.h"

#include "drv_api.h"

/****************************************************************************
*
* Global and Declaration
*
*****************************************************************************/
#define SYS_NH_APS_MAX_MEM_NUM 2

int32
sys_usw_nh_bind_aps_nh(uint8 lchip, uint32 aps_nhid, uint32 mem_nhid)
{
    sys_aps_bind_nh_param_t bind_param;
    sys_nh_info_dsnh_t nh_info;
    sys_nh_info_com_t* p_nh_com_info = NULL;
    sys_nh_info_aps_t* p_nhdb = NULL;
    uint8 loop, loop_cnt;
    uint8 gchip = 0;

    sal_memset(&bind_param, 0, sizeof(bind_param));
    sal_memset(&nh_info, 0, sizeof(sys_nh_info_dsnh_t));

    CTC_ERROR_RETURN(sys_usw_nh_get_nhinfo_by_nhid(lchip, aps_nhid, (sys_nh_info_com_t**)&p_nh_com_info));
    CTC_ERROR_RETURN(_sys_usw_nh_get_nhinfo(lchip, mem_nhid, &nh_info));
    p_nhdb = (sys_nh_info_aps_t*)p_nh_com_info;

    if ((mem_nhid != p_nhdb->w_nexthop_id) && (mem_nhid != p_nhdb->p_nexthop_id))
    {
        return CTC_E_NONE;
    }
    bind_param.working_path = (mem_nhid == p_nhdb->w_nexthop_id);
    loop_cnt = (p_nhdb->w_nexthop_id == p_nhdb->p_nexthop_id)?2:1;
    sys_usw_get_gchip_id( lchip,  &gchip);
    for (loop = 0; loop < loop_cnt; loop++)
    {
      bind_param.dsnh_offset = nh_info.dsnh_offset;
      bind_param.nh_id = aps_nhid;
      bind_param.next_aps_en = nh_info.aps_en;
      if(nh_info.use_destmap_profile)
      {
          bind_param.dest_id  = (nh_info.dest_map & 0xFFFF);
          bind_param.destmap_profile = 1;
      }
      else
      {
          bind_param.destmap_profile = 0;
         bind_param.dest_id  =  nh_info.drop_pkt ? CTC_MAP_LPORT_TO_GPORT(gchip, SYS_RSV_PORT_DROP_ID):nh_info.gport;
      }
      bind_param.destmap_profile = nh_info.use_destmap_profile;
      bind_param.different_nh =  CTC_FLAG_ISSET(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_SHARE_DSNH)?0:1;
      bind_param.p_nexthop_en =  bind_param.different_nh;
      bind_param.assign_port = p_nhdb->assign_port;
     /* no need do rollback process*/
      sys_usw_aps_bind_nexthop(lchip, p_nhdb->aps_group_id, &bind_param);
      bind_param.working_path = !bind_param.working_path ;
   }
    return CTC_E_NONE;
}

int32
_sys_usw_nh_aps_update_ref_list(uint8 lchip, sys_nh_info_aps_t* p_nhdb, sys_nh_info_com_t* p_nh_info, uint8 is_del)
{
    sys_nh_ref_list_node_t* p_curr = NULL;
    sys_nh_ref_list_node_t* p_nh_ref_list_node = NULL;
    uint8 find_flag = 0;
    sys_nh_ref_list_node_t* p_prev = NULL;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);
    p_curr = p_nh_info->hdr.p_ref_nh_list;
    while (p_curr)
    {
        if ((sys_nh_info_aps_t*)p_curr->p_ref_nhinfo == p_nhdb)
        {
            find_flag = 1;
            break;
        }
        p_prev = p_curr;
        p_curr = p_curr->p_next;
    }

    if (find_flag == 0 && !is_del)
    {
        p_nh_ref_list_node = (sys_nh_ref_list_node_t*)mem_malloc(MEM_NEXTHOP_MODULE, sizeof(sys_nh_ref_list_node_t));
        if (p_nh_ref_list_node == NULL)
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " No memory \n");
			return CTC_E_NO_MEMORY;

        }
        p_nh_ref_list_node->p_ref_nhinfo = (sys_nh_info_com_t*)p_nhdb;
        p_nh_ref_list_node->p_next = p_nh_info->hdr.p_ref_nh_list;
        p_nh_info->hdr.p_ref_nh_list = p_nh_ref_list_node;
    }
    else if (find_flag && is_del)
    {
        if (p_prev == NULL)
        {
            p_nh_info->hdr.p_ref_nh_list = p_curr->p_next;
        }
        else
        {
            p_prev->p_next = p_curr->p_next;
        }
        mem_free(p_curr);
    }

    return CTC_E_NONE;
}

int32
_sys_usw_nh_aps_add_dsfwd(uint8 lchip, sys_nh_info_com_t* p_com_db)
{
    sys_nh_param_dsfwd_t dsfwd_param;
    sys_nh_info_aps_t* p_nhinfo = (sys_nh_info_aps_t*)(p_com_db);
    int ret = 0;
    uint8 alloc_dsfwd = 0;

    sal_memset(&dsfwd_param, 0, sizeof(sys_nh_param_dsfwd_t));

    dsfwd_param.nexthop_ext   = CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_USE_DSNH8W);
    dsfwd_param.dsnh_offset   = p_nhinfo->hdr.dsfwd_info.dsnh_offset;
    dsfwd_param.dsfwd_offset  = p_nhinfo->hdr.dsfwd_info.dsfwd_offset;
    dsfwd_param.is_egress_edit =  CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_EGRESS_EDIT_MODE);
    dsfwd_param.dest_id = p_nhinfo->aps_group_id;
    dsfwd_param.aps_type = 3;

    if (!CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
    {
         ret = sys_usw_nh_offset_alloc(lchip, SYS_NH_ENTRY_TYPE_FWD_HALF, 1,
                                             &(p_nhinfo->hdr.dsfwd_info.dsfwd_offset));
        dsfwd_param.dsfwd_offset  = p_nhinfo->hdr.dsfwd_info.dsfwd_offset;
        alloc_dsfwd = 1;
    }

    /*Write table*/
    ret = ret ? ret : sys_usw_nh_write_entry_dsfwd(lchip, &dsfwd_param);

    if (ret != CTC_E_NONE)
    {
        goto error;
    }
    else
    {
        CTC_SET_FLAG(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD);
    }
    return ret;

error:
    if (alloc_dsfwd)
    {
        sys_usw_nh_offset_free(lchip, SYS_NH_ENTRY_TYPE_FWD_HALF, 1, p_nhinfo->hdr.dsfwd_info.dsfwd_offset);
    }
    return ret;
}

int32
_sys_usw_nh_update_aps_fwd_attr(uint8 lchip, sys_nh_info_com_t* p_com_db, sys_nh_param_com_t* p_com_nh_param)
{
    sys_nh_param_aps_t* p_nh_aps;
    sys_nh_info_aps_t* p_nhdb = NULL;
    ctc_nh_aps_param_t* p_nh_param = NULL;
    int32 ret = 0;
    uint8  gchip = 0;
    uint8 loop = 0,loop_cnt = 0;
    sys_nh_info_dsnh_t nh_info;
    sys_nh_info_com_t* p_nh_com_info = NULL;
    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);
    p_nh_aps = (sys_nh_param_aps_t*)p_com_nh_param;
    p_nhdb    = (sys_nh_info_aps_t*)p_com_db;
    p_nh_param = p_nh_aps->p_aps_param;

    loop_cnt =(p_nh_param->p_nhid != p_nh_param->w_nhid)?2:1;
    for (loop = 0; loop < loop_cnt; loop++)
    {
        sal_memset(&nh_info, 0, sizeof(sys_nh_info_dsnh_t));
        CTC_ERROR_GOTO(_sys_usw_nh_get_nhinfo(lchip, (loop?p_nh_param->p_nhid:p_nh_param->w_nhid), &nh_info), ret, error);
        if (nh_info.nh_entry_type == SYS_NH_TYPE_MCAST || nh_info.nh_entry_type == SYS_NH_TYPE_ECMP ||
            nh_info.nh_entry_type == SYS_NH_TYPE_APS  || (nh_info.nh_entry_type == SYS_NH_TYPE_MPLS && nh_info.pw_aps_en) ||
            (nh_info.nh_entry_type != SYS_NH_TYPE_MPLS && nh_info.aps_en))
        {
            CTC_ERROR_GOTO(CTC_E_INVALID_PARAM, ret, error);
        }
    }

    if ((p_nh_param->w_nhid != p_nhdb->w_nexthop_id) && p_nhdb->w_nexthop_id)
    {
        CTC_ERROR_RETURN(sys_usw_nh_get_nhinfo_by_nhid(lchip, p_nhdb->w_nexthop_id, (sys_nh_info_com_t**)&p_nh_com_info));
        CTC_ERROR_RETURN(_sys_usw_nh_aps_update_ref_list(lchip, p_nhdb, p_nh_com_info, 1));
    }

    if ((p_nh_param->p_nhid != p_nhdb->p_nexthop_id) && p_nhdb->p_nexthop_id)
    {
        CTC_ERROR_RETURN(sys_usw_nh_get_nhinfo_by_nhid(lchip,  p_nhdb->p_nexthop_id, (sys_nh_info_com_t**)&p_nh_com_info));
        CTC_ERROR_RETURN(_sys_usw_nh_aps_update_ref_list(lchip, p_nhdb, p_nh_com_info, 1));
    }

    if (p_nh_param->p_nhid == p_nh_param->w_nhid)
    {
        CTC_SET_FLAG(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_SHARE_DSNH);
    }
    else
    {
        CTC_UNSET_FLAG(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_SHARE_DSNH);
    }
    sys_usw_get_gchip_id( lchip,  &gchip);
    for (loop = 0; loop < SYS_NH_APS_MAX_MEM_NUM; loop++)
    {
        sys_nh_info_com_t* p_nh_com_info = NULL;
        sys_aps_bind_nh_param_t bind_param;
        sal_memset(&nh_info, 0, sizeof(sys_nh_info_dsnh_t));
        _sys_usw_nh_get_nhinfo(lchip, loop?p_nh_param->p_nhid:p_nh_param->w_nhid, &nh_info);
        sal_memset(&bind_param, 0, sizeof(bind_param));
        bind_param.working_path = loop?0:1;
        bind_param.dsnh_offset = nh_info.dsnh_offset;
        p_nhdb->hdr.dsfwd_info.dsnh_offset = loop ? p_nhdb->hdr.dsfwd_info.dsnh_offset:nh_info.dsnh_offset;
        bind_param.nh_id = p_nhdb->hdr.nh_id;
        bind_param.next_aps_en = nh_info.aps_en;
        if(nh_info.use_destmap_profile)
        {
            bind_param.dest_id  = (nh_info.dest_map & 0xFFFF);
            bind_param.destmap_profile = 1;
        }
        else
        {
           bind_param.dest_id  =  nh_info.drop_pkt ? CTC_MAP_LPORT_TO_GPORT(gchip, SYS_RSV_PORT_DROP_ID):nh_info.gport;
        }
        bind_param.different_nh = CTC_FLAG_ISSET(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_SHARE_DSNH)?0:1;
        bind_param.p_nexthop_en = bind_param.different_nh;
        bind_param.assign_port = CTC_FLAG_ISSET(p_nh_param->flag, CTC_APS_NH_FLAG_ASSIGN_PORT)?1:0;
        CTC_ERROR_RETURN(sys_usw_aps_bind_nexthop(lchip, p_nh_param->aps_group_id, &bind_param));
        CTC_ERROR_RETURN(sys_usw_nh_get_nhinfo_by_nhid(lchip, loop?p_nh_param->p_nhid:p_nh_param->w_nhid, (sys_nh_info_com_t**)&p_nh_com_info));
        CTC_ERROR_RETURN(_sys_usw_nh_aps_update_ref_list(lchip, p_nhdb, p_nh_com_info, 0));
    }
    p_nhdb->w_nexthop_id = p_nh_param->w_nhid;
    p_nhdb->p_nexthop_id = p_nh_param->p_nhid;
    p_nhdb->aps_group_id = p_nh_param->aps_group_id;
    p_nhdb->assign_port = CTC_FLAG_ISSET(p_nh_param->flag, CTC_APS_NH_FLAG_ASSIGN_PORT)?1:0;
    p_nhdb->hdr.nh_entry_type = SYS_NH_TYPE_APS;
    CTC_SET_FLAG(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN);
    if (p_nh_aps->hdr.have_dsfwd)
    {
        CTC_ERROR_RETURN(_sys_usw_nh_aps_add_dsfwd(lchip, p_com_db));
    }
error:
    return ret;
}

int32
sys_usw_nh_create_aps_cb(uint8 lchip, sys_nh_param_com_t* p_com_nh_param, sys_nh_info_com_t* p_com_db)
{
    sys_nh_param_aps_t* p_nh_aps;
    sys_nh_info_aps_t* p_nhdb = NULL;
    ctc_nh_aps_param_t* p_nh_param = NULL;
    ctc_aps_bridge_group_t aps_grp;
    int32 ret = 0;
    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);
    CTC_PTR_VALID_CHECK(p_com_nh_param);
    CTC_PTR_VALID_CHECK(p_com_db);
    p_nh_aps = (sys_nh_param_aps_t*)p_com_nh_param;
    p_nhdb    = (sys_nh_info_aps_t*)p_com_db;
    p_nh_param = p_nh_aps->p_aps_param;
    if (!p_nh_param->w_nhid || !p_nh_param->p_nhid)
    {
        SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "%s()\n", __FUNCTION__);
        return CTC_E_INVALID_CONFIG;
    }
    ret = sys_usw_aps_get_ports(lchip, p_nh_param->aps_group_id, &aps_grp);
    if (CTC_E_NOT_EXIST == ret)
    {
        return CTC_E_NOT_EXIST;
    }
   p_nhdb->aps_group_id = p_nh_param->aps_group_id;
   CTC_ERROR_RETURN(_sys_usw_nh_update_aps_fwd_attr( lchip,p_com_db, p_com_nh_param));
   return CTC_E_NONE;
}

#define ___SYS_APS__

int32
sys_usw_nh_update_aps_cb(uint8 lchip, sys_nh_info_com_t* p_com_db, sys_nh_param_com_t* p_com_nh_param)
{
    int32 ret = 0;
    sys_nh_param_aps_t* p_nh_aps;
    sys_nh_info_aps_t* p_nhdb = NULL;
    ctc_nh_aps_param_t* p_nh_param = NULL;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_PTR_VALID_CHECK(p_com_nh_param);
    CTC_PTR_VALID_CHECK(p_com_db);

    p_nh_aps = (sys_nh_param_aps_t*)p_com_nh_param;
    p_nhdb    = (sys_nh_info_aps_t*)p_com_db;

    CTC_EQUAL_CHECK(SYS_NH_TYPE_APS, p_nh_aps->hdr.nh_param_type);

    switch (p_nh_aps->hdr.change_type)
    {
    case SYS_NH_CHANGE_TYPE_UPDATE_FWD_ATTR:
        p_nh_param = p_nh_aps->p_aps_param;
        if (!p_nh_param->w_nhid || !p_nh_param->p_nhid || (p_nhdb->aps_group_id != p_nh_param->aps_group_id))
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "%s()\n", __FUNCTION__);
            return CTC_E_INVALID_CONFIG;
        }
        p_nh_aps->hdr.have_dsfwd  = CTC_FLAG_ISSET(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD);

        CTC_ERROR_RETURN(_sys_usw_nh_update_aps_fwd_attr(lchip, p_com_db, p_com_nh_param));

        break;
    case SYS_NH_CHANGE_TYPE_ADD_DYNTBL:
        {
           if (CTC_FLAG_ISSET(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
           {
                return CTC_E_NONE;
           }
           CTC_ERROR_RETURN(_sys_usw_nh_aps_add_dsfwd(lchip, p_com_db));
        }
        break;

    default:
        return CTC_E_INVALID_PARAM;
    }
    return ret;
}

int32
sys_usw_nh_delete_aps_cb(uint8 lchip, sys_nh_info_com_t* p_com_db, uint32* p_nhid)
{
    sys_aps_bind_nh_param_t aps_bind;
    sys_nh_info_aps_t* p_nhdb = NULL;
    sys_nh_info_com_t* p_nh_com_info = NULL;
    uint8 loop = 0;

    CTC_PTR_VALID_CHECK(p_com_db);
    CTC_PTR_VALID_CHECK(p_nhid);

    CTC_EQUAL_CHECK(SYS_NH_TYPE_APS, p_com_db->hdr.nh_entry_type);

    p_nhdb = (sys_nh_info_aps_t*)p_com_db;

    sal_memset(&aps_bind, 0, sizeof(sys_aps_bind_nh_param_t));
    aps_bind.nh_id = p_com_db->hdr.nh_id;
    for (loop = 0; loop < SYS_NH_APS_MAX_MEM_NUM; loop++)
    {
        aps_bind.working_path = loop?0:1;
        aps_bind.nh_id = p_com_db->hdr.nh_id;
        CTC_ERROR_RETURN(sys_usw_aps_unbind_nexthop(lchip, p_nhdb->aps_group_id, &aps_bind));
        CTC_ERROR_RETURN(sys_usw_nh_get_nhinfo_by_nhid(lchip, loop?p_nhdb->p_nexthop_id:p_nhdb->w_nexthop_id, (sys_nh_info_com_t**)&p_nh_com_info));
        CTC_ERROR_RETURN(_sys_usw_nh_aps_update_ref_list(lchip, p_nhdb, p_nh_com_info, 1));
    }


    if (CTC_FLAG_ISSET(p_com_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
    {
        sys_nh_param_dsfwd_t dsfwd_param;

        sal_memset(&dsfwd_param, 0, sizeof(sys_nh_param_dsfwd_t));
        dsfwd_param.dsfwd_offset = p_com_db->hdr.dsfwd_info.dsfwd_offset;
        dsfwd_param.drop_pkt = 1;
        sys_usw_nh_write_entry_dsfwd(lchip, &dsfwd_param);
        /*Free DsFwd offset*/
        sys_usw_nh_offset_free(lchip,SYS_NH_ENTRY_TYPE_FWD_HALF, 1, p_com_db->hdr.dsfwd_info.dsfwd_offset);
    }
    return CTC_E_NONE;
}

int32
sys_usw_nh_get_aps_cb(uint8 lchip, sys_nh_info_com_t* p_nh_db, void* p_para)
{
    sys_nh_info_aps_t* p_nh_info;
    ctc_nh_aps_param_t* p_aps_param = NULL;

    CTC_PTR_VALID_CHECK(p_para);
    CTC_PTR_VALID_CHECK(p_nh_db);
    CTC_EQUAL_CHECK(SYS_NH_TYPE_APS, p_nh_db->hdr.nh_entry_type);
    p_nh_info = (sys_nh_info_aps_t*)(p_nh_db);
    p_aps_param = (ctc_nh_aps_param_t*)p_para;

    p_aps_param->aps_group_id = p_nh_info->aps_group_id;
    p_aps_param->w_nhid = p_nh_info->w_nexthop_id;
    p_aps_param->p_nhid = p_nh_info->p_nexthop_id;
    if (p_nh_info->assign_port)
    {
        CTC_SET_FLAG(p_aps_param->flag, CTC_APS_NH_FLAG_ASSIGN_PORT);
    }

    return CTC_E_NONE;
}



