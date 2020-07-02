/**
 @file sys_usw_nexthop_l3.c

 @date 2009-11-23

 @version v2.0

 The file contains all nexthop layer3 related callback function
*/

/****************************************************************************
 *
* Header Files
*
****************************************************************************/
#include "ctc_const.h"
#include "ctc_error.h"
#include "ctc_linklist.h"

#include "ctc_packet.h"
#include "ctc_internal_port.h"

#include "sys_usw_chip.h"
#include "sys_usw_register.h"
#include "sys_usw_nexthop_api.h"
#include "sys_usw_nexthop.h"
#include "sys_usw_nexthop_hw.h"
#include "sys_usw_vlan.h"
#include "sys_usw_l3if.h"
#include "sys_usw_aps.h"
#include "drv_api.h"

extern int32 sys_usw_internal_port_allocate(uint8 lchip, ctc_internal_port_assign_para_t* p_port_assign);
extern sys_usw_nh_api_master_t* p_usw_nh_api_master[CTC_MAX_LOCAL_CHIP_NUM];

int32
sys_usw_nh_update_dsipda_cb(uint8 lchip, uint32 nhid, sys_nh_info_com_t* p_nh_db)
{
    sys_nh_info_ipuc_t* p_nh_info;
    sys_nh_info_dsnh_t        dsnh_info;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_EQUAL_CHECK(SYS_NH_TYPE_IPUC, p_nh_db->hdr.nh_entry_type);
    p_nh_info = (sys_nh_info_ipuc_t*)(p_nh_db);

    sal_memset(&dsnh_info, 0, sizeof(sys_nh_info_dsnh_t));


    if (NULL != p_nh_info->updateAd)
    {
        CTC_ERROR_RETURN(_sys_usw_nh_get_nhinfo(lchip, nhid, &dsnh_info));
        dsnh_info.need_lock = 1;
        CTC_ERROR_RETURN(p_nh_info->updateAd(lchip, p_nh_info->data, &dsnh_info));
    }

    return CTC_E_NONE;
}


int32
sys_usw_nh_ipuc_update_dsfwd(uint8 lchip, uint32 nhid, sys_nh_info_com_t* p_nh_db)
{
    sys_nh_info_dsnh_t        dsnh_info;
    uint32 dsfwd_offset = 0;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_EQUAL_CHECK(SYS_NH_TYPE_IPUC, p_nh_db->hdr.nh_entry_type);

    sal_memset(&dsnh_info, 0, sizeof(sys_nh_info_dsnh_t));
    CTC_ERROR_RETURN(_sys_usw_nh_get_nhinfo(lchip, nhid, &dsnh_info));

    if (dsnh_info.dsfwd_valid)
    {
        dsfwd_offset = dsnh_info.dsfwd_offset;

        CTC_ERROR_RETURN(sys_usw_nh_update_entry_dsfwd(lchip, &dsfwd_offset, dsnh_info.dest_map, dsnh_info.dsnh_offset, dsnh_info.nexthop_ext, 0, 1));
    }

    return CTC_E_NONE;
}

STATIC int32
_sys_usw_nh_ipuc_add_loop(uint8 lchip, sys_nh_info_ipuc_t* p_nh_info, sys_l3if_prop_t* p_l3if_prop,
            uint32 gport, uint16* p_dest_vlan_ptr)
{
    uint32 eloop_nhid = 0;
    int32 ret = 0;
    ctc_internal_port_assign_para_t port_alloc;
    sys_nh_info_brguc_t* p_br_nh = NULL;
    sys_nh_info_com_t* p_nh_com_db = NULL;
    if (p_l3if_prop->l3if_type == CTC_L3IF_TYPE_VLAN_IF)
    {
        *p_dest_vlan_ptr = p_l3if_prop->l3if_id + 2048 + 0x1000;
    }
    else
    {
        *p_dest_vlan_ptr = p_l3if_prop->vlan_ptr + 2048;
    }
    if (CTC_IS_CPU_PORT(gport) || (p_l3if_prop->l3if_type != CTC_L3IF_TYPE_VLAN_IF) || CTC_IS_LINKAGG_PORT(gport))
    {
        return CTC_E_NONE;
    }
    CTC_ERROR_RETURN(sys_usw_l2_get_ucast_nh(lchip, gport, CTC_NH_PARAM_BRGUC_SUB_TYPE_BASIC, &eloop_nhid));
    CTC_ERROR_RETURN(sys_usw_nh_get_nhinfo_by_nhid(lchip, eloop_nhid, (sys_nh_info_com_t**)&p_nh_com_db));
    p_br_nh = (sys_nh_info_brguc_t*)p_nh_com_db;
    if ((eloop_nhid != p_nh_info->eloop_nhid) && (p_l3if_prop->l3if_type == CTC_L3IF_TYPE_VLAN_IF))
    {
        uint8 gchip = 0;
        if (!g_usw_nh_master[lchip]->rsv_nh_ptr)
        {
            sys_nexthop_t dsnh;
            sal_memset(&dsnh, 0, sizeof(sys_nexthop_t));
            CTC_ERROR_RETURN(sys_usw_nh_offset_alloc(lchip, SYS_NH_ENTRY_TYPE_NEXTHOP_4W, 1, &g_usw_nh_master[lchip]->rsv_nh_ptr));
            dsnh.derive_stag_cos = 1;
            dsnh.svlan_tagged  = 1;
            dsnh.payload_operation = SYS_NH_OP_BRIDGE;
            dsnh.share_type = SYS_NH_SHARE_TYPE_VLANTAG;
            dsnh.vlan_xlate_mode = 1;
            dsnh.destVlanPtrDeriveMode = 1;
            CTC_ERROR_RETURN(sys_usw_nh_write_asic_table(lchip,SYS_NH_ENTRY_TYPE_NEXTHOP_4W, g_usw_nh_master[lchip]->rsv_nh_ptr, &dsnh));
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Alloc reserve nh ptr for ipuc keep inner vlan, %u \n", g_usw_nh_master[lchip]->rsv_nh_ptr);
        }

        if (!p_br_nh->eloop_port)
        {
            sal_memset(&port_alloc, 0, sizeof(ctc_internal_port_assign_para_t));
            p_br_nh->eloop_port = 0xffff;   /*special used for get nexthop info*/
            sys_usw_get_gchip_id(lchip, &gchip);
            port_alloc.gchip = gchip;
            port_alloc.type = CTC_INTERNAL_PORT_TYPE_ELOOP;
            port_alloc.nhid = eloop_nhid;
            ret = (sys_usw_internal_port_allocate(lchip, &port_alloc));
            if (ret)
            {
                SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " Internel port alloc fail \n");
                p_br_nh->eloop_port = 0;
                return ret;
            }
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Alloc internal eloop port:%u \n", port_alloc.inter_port);
            p_nh_info->eloop_port = port_alloc.inter_port;
            p_nh_info->eloop_nhid = eloop_nhid;
            p_br_nh->eloop_port = port_alloc.inter_port;
        }
        else
        {
            p_nh_info->eloop_port = p_br_nh->eloop_port;
            p_nh_info->eloop_nhid = eloop_nhid;
        }
    }
    return CTC_E_NONE;
}

int32
_sys_usw_nh_ipuc_update_dsnh_cb(uint8 lchip, sys_nh_db_arp_t* p_arp_db)
{
    sys_nexthop_t sys_dsnh;
    sys_nh_info_com_t* p_nh_com_db = NULL;
    uint32 nh_id = 0;
    uint8 use_l2edit = 0;

    sys_nh_info_ipuc_t* p_nh_info = NULL;


    nh_id = p_arp_db->nh_id;

    CTC_ERROR_RETURN(sys_usw_nh_get_nhinfo_by_nhid(lchip, nh_id, (sys_nh_info_com_t**)&p_nh_com_db));
    use_l2edit = CTC_FLAG_ISSET(p_nh_com_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_L2EDIT);
    p_nh_info = (sys_nh_info_ipuc_t*)(p_nh_com_db);
    if(!use_l2edit && !p_arp_db->in_l2_offset && !(sys_usw_chip_get_rchain_en() && lchip))
    {
        sys_l3if_prop_t l3if_prop;

        l3if_prop.l3if_id = p_arp_db->l3if_id;
        CTC_ERROR_RETURN(sys_usw_l3if_get_l3if_info( lchip, 1,&l3if_prop));

        sys_dsnh.update_type = 1;
        sal_memcpy(sys_dsnh.mac_da, p_arp_db->mac_da, sizeof(mac_addr_t));
        sys_dsnh.dest_vlan_ptr = l3if_prop.vlan_ptr;

        sys_dsnh.offset = p_nh_com_db->hdr.dsfwd_info.dsnh_offset;

        if (CTC_FLAG_ISSET(p_nh_info->flag, SYS_NH_IPUC_FLAG_REPLACE_MODE_EN))
        {
            CTC_ERROR_RETURN(_sys_usw_nh_ipuc_add_loop(lchip, p_nh_info, &l3if_prop, p_arp_db->gport, &sys_dsnh.dest_vlan_ptr));
        }
        CTC_ERROR_RETURN(sys_usw_nh_update_asic_table(lchip, SYS_NH_ENTRY_TYPE_NEXTHOP_4W, sys_dsnh.offset, &sys_dsnh));
    }

    if (!CTC_FLAG_ISSET(p_nh_com_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
    {
        /*Not use destmap proifle , Need update dsipda and dsfwd*/
        CTC_ERROR_RETURN(sys_usw_nh_update_dsipda_cb(lchip, nh_id, p_nh_com_db));
    }
    else
    {
        CTC_ERROR_RETURN(sys_usw_nh_ipuc_update_dsfwd(lchip, nh_id, p_nh_com_db));
    }

    CTC_ERROR_RETURN(sys_usw_nh_update_loopback_l2edit(lchip, nh_id, 1));

	/*update oam mep*/
    CTC_ERROR_RETURN(_sys_usw_nh_update_oam_mep(lchip, p_nh_info->p_ref_oam_list, p_arp_db->arp_id, NULL));

   /*for aps nexthop member to update aps group information*/
   if (p_nh_info->hdr.p_ref_nh_list)
   {
        sys_nh_ref_list_node_t* p_ref_node = NULL;
        sys_nh_info_com_t* p_nh_com = NULL;

        p_ref_node = p_nh_info->hdr.p_ref_nh_list;
        while (p_ref_node)
        {
            p_nh_com = p_ref_node->p_ref_nhinfo;
            sys_usw_nh_bind_aps_nh(lchip, p_nh_com->hdr.nh_id, p_nh_info->hdr.nh_id);
            p_ref_node = p_ref_node->p_next;
        }
   }
    return CTC_E_NONE;
}


STATIC int32
_sys_usw_nh_ipuc_update_dsnh(uint8 lchip, sys_nh_param_ipuc_t* p_nhpara,sys_nh_info_ipuc_t* p_nhdb, bool working_path)
{
    sys_l3if_prop_t l3if_prop;
    sys_nh_param_dsnh_t dsnh_param;
    ctc_nh_oif_info_t *p_oif;
    sys_nh_db_dsl2editeth4w_t   dsl2edit_4w;
    sys_nh_db_dsl2editeth4w_t*  p_dsl2edit_4w = NULL;
    sys_nh_db_dsl2editeth8w_t   dsl2edit_8w;
    sys_nh_db_dsl2editeth8w_t*  p_dsl2edit_8w  = NULL;
    ctc_vlan_egress_edit_info_t  vlan_edit;
    uint16 arp_id = 0;
    ctc_ip_nh_param_t* p_ipuc_param = p_nhpara->p_ipuc_param;
    mac_addr_t mac = {0};
    uint8 l2edit_8w = 0;
    sys_nh_db_arp_t *p_arp = NULL;
    sys_aps_bind_nh_param_t bind_param;
    uint8 is_phy_interface = 0;
    int32 ret = 0;
    uint32 temp_gport = 0;
    uint8 use_l2edit = CTC_FLAG_ISSET(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_L2EDIT);

    sal_memset(&dsnh_param, 0, sizeof(sys_nh_param_dsnh_t));
    sal_memset(&dsl2edit_4w, 0, sizeof(dsl2edit_4w));
    sal_memset(&dsl2edit_8w, 0, sizeof(dsl2edit_8w));
    sal_memset(&l3if_prop, 0, sizeof(l3if_prop));

    p_oif = working_path ? &p_ipuc_param->oif : &p_ipuc_param->p_oif;
    arp_id = working_path ? p_ipuc_param->arp_id: p_ipuc_param->p_arp_id;

    if (p_nhpara->hdr.l3if_id)
    {
        l3if_prop.l3if_id = p_nhpara->hdr.l3if_id;
        CTC_ERROR_RETURN(sys_usw_l3if_get_l3if_info(lchip, 1, &l3if_prop));
        p_nhdb->flag |= SYS_NH_IPUC_FLAG_L3IF_VALID;
    }
    else if (!(CTC_IS_CPU_PORT(p_oif->gport) || p_oif->is_l2_port || p_nhpara->hdr.is_drop))
    {
        CTC_ERROR_RETURN(sys_usw_l3if_get_l3if_info_with_port_and_vlan(lchip, p_oif->gport, p_oif->vid, p_oif->cvid, &l3if_prop));
        is_phy_interface = (l3if_prop.l3if_type == CTC_L3IF_TYPE_PHY_IF) ? 1 :0;
    }

    if ((l3if_prop.l3if_type == CTC_L3IF_TYPE_PHY_IF || p_nhpara->hdr.l3if_id) && p_oif->vid)
    {
        sal_memset(&vlan_edit, 0, sizeof(ctc_vlan_egress_edit_info_t));
        dsl2edit_4w.output_vid = p_oif->vid;
        use_l2edit = 1;
    }

    if (CTC_FLAG_ISSET(p_ipuc_param->flag, CTC_IP_NH_FLAG_FPMA))
    {
        use_l2edit = 1;
        dsl2edit_4w.fpma = 1;
    }

    if(0 != sal_memcmp(p_ipuc_param->mac_sa, mac, sizeof(mac_addr_t)))/*edit mac sa, must use l2edit8w, for openflow*/
    {
        use_l2edit = 1;
        l2edit_8w  =1;
        dsl2edit_8w.update_mac_sa = 1;
    }

    if(working_path)
    {
        sal_memcpy(dsnh_param.macDa, p_ipuc_param->mac, sizeof(mac_addr_t));
        dsnh_param.dsnh_offset = p_nhdb->hdr.dsfwd_info.dsnh_offset;
    }
    else
    {
        sal_memcpy(dsnh_param.macDa, p_ipuc_param->p_mac, sizeof(mac_addr_t));
        dsnh_param.dsnh_offset = (p_nhdb->hdr.dsfwd_info.dsnh_offset) + 1;
    }
    sal_memcpy(dsl2edit_4w.mac_da, dsnh_param.macDa, sizeof(mac_addr_t));

    if (0 != arp_id)
    {
        /*To be processed in function sys_usw_nh_bind_arp_cb */
        use_l2edit = 0;
    }

    if (use_l2edit)
    {
        if (l2edit_8w)
        {
            sal_memcpy(dsl2edit_8w.mac_da, p_ipuc_param->mac, sizeof(mac_addr_t));
            sal_memcpy(dsl2edit_8w.mac_sa, p_ipuc_param->mac_sa, sizeof(mac_addr_t));
            dsl2edit_8w.output_vid = p_oif->vid;

            p_dsl2edit_8w  = &dsl2edit_8w;
            CTC_ERROR_RETURN(sys_usw_nh_add_l2edit_6w_inner(lchip, &p_dsl2edit_8w, &dsnh_param.inner_l2edit_ptr));
            p_nhdb->p_dsl2edit = (sys_nh_db_dsl2editeth4w_t*)p_dsl2edit_8w;
            p_nhdb->l2edit_8w = l2edit_8w;
            p_nhdb->l2_edit_ptr = dsnh_param.inner_l2edit_ptr;
        }
        else
        {
            p_dsl2edit_4w   = &dsl2edit_4w;
            CTC_ERROR_RETURN(sys_usw_nh_add_l2edit_3w_inner(lchip, &p_dsl2edit_4w, SYS_NH_ENTRY_TYPE_L2EDIT_INNER_3W, &dsnh_param.inner_l2edit_ptr));
            if (working_path)
            {
                p_nhdb->p_dsl2edit = p_dsl2edit_4w;
                p_nhdb->l2_edit_ptr = dsnh_param.inner_l2edit_ptr;
            }
            else
            {
                p_nhdb->protection_path->p_dsl2edit = p_dsl2edit_4w;
                p_nhdb->protection_path->l2_edit_ptr = dsnh_param.inner_l2edit_ptr;
            }
        }
        CTC_SET_FLAG(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_L2EDIT);
    }
    else
    {
        temp_gport = p_oif->gport;

        if (working_path)
        {
            sal_memcpy(p_nhdb->mac_da, p_ipuc_param->mac, sizeof(mac_addr_t));
        }
        else
        {
            sal_memcpy(p_nhdb->protection_path->mac_da, p_ipuc_param->p_mac, sizeof(mac_addr_t));
        }

        if (arp_id)
        {
            sys_nh_info_arp_param_t arp_parm;

            sal_memset(&arp_parm, 0, sizeof(sys_nh_info_arp_param_t));
            arp_parm.nh_entry_type  = SYS_NH_TYPE_IPUC;
            arp_parm.nh_id          = p_nhpara->hdr.nhid;
            arp_parm.updateNhp      = (updatenh_fn)_sys_usw_nh_ipuc_update_dsnh_cb;
            arp_parm.is_aps = p_ipuc_param->aps_en;
            CTC_ERROR_RETURN(sys_usw_nh_bind_arp_cb(lchip, &arp_parm, arp_id));
            if (working_path)
            {
                p_nhdb->p_arp_nh_node = arp_parm.p_arp_nh_node;
            }

            p_arp = (sys_usw_nh_lkup_arp_id(lchip, arp_id));
            if (NULL == p_arp)
            {
                return CTC_E_NOT_EXIST;
            }

            dsnh_param.inner_l2edit_ptr  = p_arp->in_l2_offset;
            sal_memcpy(dsnh_param.macDa, p_arp->mac_da, sizeof(mac_addr_t));
            temp_gport = p_arp->gport;
            is_phy_interface = (p_arp->l3if_type == CTC_L3IF_TYPE_PHY_IF) ? 1 :0;
        }
    }

    if (working_path)
    {
        p_nhdb->l3ifid = l3if_prop.l3if_id;
        p_nhdb->arp_id = arp_id;

    }
    else
    {
        p_nhdb->protection_path->l3ifid = l3if_prop.l3if_id;
        p_nhdb->protection_path->arp_id = arp_id;

    }
    dsnh_param.dsnh_type = SYS_NH_PARAM_DSNH_TYPE_IPUC;
    dsnh_param.dest_vlan_ptr = l3if_prop.vlan_ptr;
    dsnh_param.flag  = p_nhdb->flag;
    dsnh_param.mtu_no_chk = p_nhpara->p_ipuc_param->mtu_no_chk;
    dsnh_param.cid  = p_nhpara->p_ipuc_param->cid;

    if(CTC_FLAG_ISSET(p_ipuc_param->flag, CTC_IP_NH_FLAG_TTL_NO_DEC))
    {
        CTC_SET_FLAG(dsnh_param.flag, SYS_NH_PARAM_FLAG_ROUTE_NOTTL);
    }

    if (DRV_IS_TSINGMA(lchip) && SYS_IPUC_REPLACE_MODE_EN && !use_l2edit && !p_ipuc_param->aps_en && !is_phy_interface)
    {
        SYS_NH_UNLOCK;
        ret = _sys_usw_nh_ipuc_add_loop(lchip, p_nhdb, &l3if_prop, temp_gport, &dsnh_param.dest_vlan_ptr);
        SYS_NH_LOCK;
        if (ret)
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " _sys_usw_nh_ipuc_add_loop fail \n");
            return ret;
        }
        CTC_SET_FLAG(p_nhdb->flag, SYS_NH_IPUC_FLAG_REPLACE_MODE_EN);
    }

    CTC_ERROR_RETURN(sys_usw_nh_write_entry_dsnh4w(lchip, &dsnh_param));

    if ((p_ipuc_param->aps_en)&&p_arp)
    {
        sal_memset(&bind_param, 0, sizeof(bind_param));
        bind_param.different_nh = 1;
        bind_param.nh_id = p_nhpara->hdr.nhid;
        bind_param.working_path = working_path;
        bind_param.l3if_id = p_arp->l3if_id;
        bind_param.dest_id = p_arp->destmap_profile;
        bind_param.destmap_profile = 1;
        CTC_ERROR_RETURN(sys_usw_aps_bind_nexthop(lchip, p_ipuc_param->aps_bridge_group_id, &bind_param));
    }

    return CTC_E_NONE;
}
int32
_sys_usw_nh_free_ipuc_nh_resource(uint8 lchip, sys_nh_info_ipuc_t* p_nhinfo, uint32 nh_id)
{
    if (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_L2EDIT))
    {
        if (p_nhinfo->l2edit_8w)
        {
            sys_usw_nh_remove_l2edit_6w_inner(lchip, (sys_nh_db_dsl2editeth8w_t*)(p_nhinfo->p_dsl2edit));
            p_nhinfo->l2edit_8w = 0;
        }
        else
        {
            sys_usw_nh_remove_l2edit_3w_inner(lchip, p_nhinfo->p_dsl2edit, SYS_NH_ENTRY_TYPE_L2EDIT_INNER_3W);
        }

        if (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN) && p_nhinfo->protection_path && p_nhinfo->protection_path->p_dsl2edit)
        {
            sys_usw_nh_remove_l2edit_3w_inner(lchip, p_nhinfo->protection_path->p_dsl2edit, SYS_NH_ENTRY_TYPE_L2EDIT_INNER_3W);
        }
    }

   if(p_nhinfo->arp_id)
   {
         sys_usw_nh_unbind_arp_cb(lchip,p_nhinfo->arp_id, 1, p_nhinfo->p_arp_nh_node);
   }
   if (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN)
             && p_nhinfo->protection_path
             && p_nhinfo->protection_path->arp_id)
    {
         sys_usw_nh_unbind_arp_cb(lchip,p_nhinfo->protection_path->arp_id, 1, NULL);
    }

    if (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN)&&p_nhinfo->arp_id)
    {
        sys_aps_bind_nh_param_t  bind_nh;
        sal_memset(&bind_nh, 0, sizeof(bind_nh));
        bind_nh.nh_id = nh_id;
        sys_usw_aps_unbind_nexthop(lchip, p_nhinfo->gport, &bind_nh);
        bind_nh.working_path = 1;
        sys_usw_aps_unbind_nexthop(lchip, p_nhinfo->gport, &bind_nh);
    }

    CTC_UNSET_FLAG(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_L2EDIT);
    CTC_UNSET_FLAG(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN);

    return CTC_E_NONE;
}

int32
_sys_usw_nh_ipuc_add_dsfwd(uint8 lchip, sys_nh_info_com_t* p_com_db)
{
   sys_nh_param_dsfwd_t dsfwd_param;
   sys_nh_info_ipuc_t* p_nhinfo = (sys_nh_info_ipuc_t*)(p_com_db);
   int ret = 0;
   uint8 alloc_dsfwd = 0;

   sal_memset(&dsfwd_param, 0, sizeof(sys_nh_param_dsfwd_t));


   dsfwd_param.nexthop_ext = CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_USE_DSNH8W);
   dsfwd_param.is_mcast = 0;
   dsfwd_param.drop_pkt  = CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_UNROV);

   dsfwd_param.dsnh_offset = p_nhinfo->hdr.dsfwd_info.dsnh_offset;
   dsfwd_param.dsfwd_offset  = p_nhinfo->hdr.dsfwd_info.dsfwd_offset;
   dsfwd_param.is_egress_edit =  CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_EGRESS_EDIT_MODE);


   if (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN))
   {
       dsfwd_param.dest_id = p_nhinfo->gport;
       dsfwd_param.aps_type = 3;
   }
   else if (p_nhinfo->arp_id)
   {
       sys_nh_db_arp_t *p_arp = NULL;
       p_arp = (sys_usw_nh_lkup_arp_id(lchip, p_nhinfo->arp_id));
       if (NULL == p_arp)
       {
           ret =  CTC_E_NOT_EXIST;
           goto error;
       }

       if (CTC_FLAG_ISSET(p_arp->flag, SYS_NH_ARP_FLAG_REDIRECT_TO_CPU))
       {
           dsfwd_param.dest_chipid  =  SYS_MAP_CTC_GPORT_TO_GCHIP(p_arp->gport);
           dsfwd_param.is_lcpu    =   CTC_FLAG_ISSET(p_arp->flag, SYS_NH_ARP_FLAG_REDIRECT_TO_CPU);
           dsfwd_param.dsnh_offset    =   CTC_PKT_CPU_REASON_BUILD_NHPTR(CTC_PKT_CPU_REASON_ARP_MISS, 0);
       }
       else if (CTC_FLAG_ISSET(p_arp->flag, SYS_NH_ARP_FLAG_DROP))
       {
           dsfwd_param.dest_chipid  =  SYS_MAP_CTC_GPORT_TO_GCHIP(p_arp->gport);
           dsfwd_param.drop_pkt = 1;
       }
       else
       {
           dsfwd_param.dest_chipid  =  SYS_MAP_CTC_GPORT_TO_GCHIP(p_arp->gport);
           dsfwd_param.dest_id      =  CTC_MAP_GPORT_TO_LPORT(p_arp->gport);
           if (p_nhinfo->eloop_port)
           {
                uint8 gchip = 0;
                sys_usw_get_gchip_id(lchip, &gchip);
                dsfwd_param.dest_chipid = gchip;
                dsfwd_param.dest_id = p_nhinfo->eloop_port;
           }
       }
   }
   else
   {
       dsfwd_param.dest_chipid = SYS_MAP_CTC_GPORT_TO_GCHIP(p_nhinfo->gport);
       dsfwd_param.dest_id = SYS_MAP_CTC_GPORT_TO_DRV_LPORT(p_nhinfo->gport);
        if (p_nhinfo->eloop_port)
        {
            uint8 gchip = 0;
            sys_usw_get_gchip_id(lchip, &gchip);
            dsfwd_param.dest_chipid = gchip;
            dsfwd_param.dest_id = p_nhinfo->eloop_port;
        }
   }

   if (CTC_IS_CPU_PORT(p_nhinfo->gport))
   {
        dsfwd_param.is_cpu = 1;
        dsfwd_param.dsnh_offset    =   CTC_PKT_CPU_REASON_BUILD_NHPTR(CTC_PKT_CPU_REASON_FWD_CPU, 0);
   }

    if (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_LEN_ADJUST_EN))
    {
        sys_usw_lkup_adjust_len_index(lchip, p_nhinfo->hdr.adjust_len, &dsfwd_param.adjust_len_idx);
    }

    dsfwd_param.is_6w = dsfwd_param.truncate_profile_id || dsfwd_param.stats_ptr || dsfwd_param.is_reflective;
    if (!CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
    {
        p_nhinfo->hdr.nh_entry_flags |= (dsfwd_param.is_6w)?SYS_NH_INFO_FLAG_DSFWD_IS_6W:0;
        ret = sys_usw_nh_offset_alloc(lchip, dsfwd_param.is_6w?SYS_NH_ENTRY_TYPE_FWD:SYS_NH_ENTRY_TYPE_FWD_HALF, 1,
                                             &(p_nhinfo->hdr.dsfwd_info.dsfwd_offset));
        dsfwd_param.dsfwd_offset  = p_nhinfo->hdr.dsfwd_info.dsfwd_offset;
        alloc_dsfwd = 1;
    }
    else
    {
        if (dsfwd_param.is_6w && (!CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_DSFWD_IS_6W)))
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "Cannot update dsfwd from half to full! \n");
            return CTC_E_NOT_SUPPORT;
        }
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
        sys_usw_nh_offset_free(lchip,  dsfwd_param.is_6w?SYS_NH_ENTRY_TYPE_FWD:SYS_NH_ENTRY_TYPE_FWD_HALF, 1, p_nhinfo->hdr.dsfwd_info.dsfwd_offset);
    }
    return ret;
}
STATIC int32
_sys_usw_nh_update_ipuc_fwd_attr(uint8 lchip, sys_nh_param_ipuc_t* p_nhpara, sys_nh_info_ipuc_t* p_nhinfo)
{
    int ret = 0;
    uint8 edit_dsnh = 1;
    uint8 gchip_id = 0;
    ctc_ip_nh_param_t* p_ipuc_param = p_nhpara->p_ipuc_param;
    uint8 is_update = (p_nhpara->hdr.change_type != SYS_NH_CHANGE_TYPE_NULL);
    sys_nh_info_arp_param_t arp_parm;
    sys_nh_info_ipuc_t old_nh_info ;
    uint32 dsnh_offset = 0;
    uint32 cmd = 0;
    uint32 tbl_id = 0;
    DsNextHop8W_m data_w;
    uint32 arp_id = 0;
    uint32 arp_id_p = 0;

    if(is_update)
    {
       sal_memcpy(&old_nh_info, p_nhinfo, sizeof(sys_nh_info_ipuc_t));
       /*reset*/
       if (p_nhinfo->arp_id)
       {
           sys_usw_nh_unbind_arp_cb(lchip, p_nhinfo->arp_id, 1, p_nhinfo->p_arp_nh_node);
           arp_id = p_nhinfo->arp_id;
           old_nh_info.arp_id = 0;
        }
       if ( CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN)
           && p_nhinfo->protection_path
       && p_nhinfo->protection_path->arp_id)
       {
           sys_usw_nh_unbind_arp_cb(lchip, p_nhinfo->protection_path->arp_id, 1, NULL);
           arp_id_p = p_nhinfo->protection_path->arp_id;
           old_nh_info.protection_path->arp_id = 0;
       }
        sal_memset(p_nhinfo, 0, sizeof(sys_nh_info_ipuc_t));
        sys_usw_nh_copy_nh_entry_flags(lchip, &old_nh_info.hdr, &p_nhinfo->hdr);
        p_nhinfo->updateAd = old_nh_info.updateAd;
        p_nhinfo->data = old_nh_info.data;
        p_nhinfo->chk_data = old_nh_info.chk_data;

        tbl_id = CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_USE_DSNH8W)?DsNextHop8W_t:DsNextHop4W_t;
        sal_memset(&data_w, 0, sizeof(DsNextHop8W_m));
        dsnh_offset = p_nhinfo->hdr.dsfwd_info.dsnh_offset;
        cmd = DRV_IOR(tbl_id, DRV_ENTRY_FLAG);
        DRV_IOCTL(lchip, dsnh_offset, cmd, &data_w);
    }


    if (CTC_FLAG_ISSET(p_nhpara->p_ipuc_param->flag, CTC_IP_NH_FLAG_MERGE_DSFWD))
    {
       CTC_SET_FLAG(p_nhinfo->flag, SYS_NH_IPUC_FLAG_MERGE_DSFWD);
    }
    else
    {
       CTC_UNSET_FLAG(p_nhinfo->flag, SYS_NH_IPUC_FLAG_MERGE_DSFWD);
    }

    if (p_ipuc_param->adjust_length != 0)
    {
        CTC_SET_FLAG(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_LEN_ADJUST_EN);
        p_nhinfo->hdr.adjust_len = p_ipuc_param->adjust_length ;
    }

    /*first do l3if check*/
    if(!p_ipuc_param->aps_en)
    {
        p_nhinfo->gport = p_ipuc_param->oif.gport;
    }
    else
    {
        p_nhinfo->gport = p_ipuc_param->aps_bridge_group_id;
    }
    sys_usw_get_gchip_id(lchip,   &gchip_id);

    if (edit_dsnh)
    {
        CTC_ERROR_GOTO(_sys_usw_nh_ipuc_update_dsnh(lchip,  p_nhpara, p_nhinfo, TRUE),ret, error_proc);

        if (p_ipuc_param->aps_en && !CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_SHARE_DSNH))
        {
            CTC_ERROR_GOTO(_sys_usw_nh_ipuc_update_dsnh(lchip,  p_nhpara, p_nhinfo, FALSE),ret, error_proc);

        }
    }

    if (p_nhpara->hdr.have_dsfwd)
    {
        CTC_ERROR_GOTO(_sys_usw_nh_ipuc_add_dsfwd(lchip, (sys_nh_info_com_t*) p_nhinfo),ret, error_proc);
    }

    if(p_ipuc_param->aps_en)
    {
        CTC_SET_FLAG(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN);
    }

    if (is_update)
    {
        _sys_usw_nh_free_ipuc_nh_resource(lchip, &old_nh_info, p_nhpara->hdr.nhid);
    }
    return CTC_E_NONE;

error_proc:
      _sys_usw_nh_free_ipuc_nh_resource(lchip, p_nhinfo, p_nhpara->hdr.nhid);
     /*rollback*/
     if (is_update)
     {
         sal_memcpy(p_nhinfo, &old_nh_info,  sizeof(sys_nh_info_ipuc_t));
         p_nhinfo->arp_id = arp_id;
         /*bind arp*/
         sal_memset(&arp_parm, 0, sizeof(sys_nh_info_arp_param_t));
         arp_parm.nh_entry_type  = SYS_NH_TYPE_IPUC;
         arp_parm.nh_id          = p_nhpara->hdr.nhid;
         arp_parm.updateNhp      = (updatenh_fn)_sys_usw_nh_ipuc_update_dsnh_cb;
         arp_parm.is_aps         = CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN);

         if (arp_id)
         {
             sys_usw_nh_bind_arp_cb(lchip, &arp_parm, arp_id);
             p_nhinfo->p_arp_nh_node = arp_parm.p_arp_nh_node;
         }
         if ( CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN)
             && p_nhinfo->protection_path && arp_id_p)
         {
             sys_usw_nh_bind_arp_cb(lchip, &arp_parm, arp_id_p);
             p_nhinfo->protection_path->arp_id = arp_id_p;
         }

        tbl_id = CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_USE_DSNH8W)?DsNextHop8W_t:DsNextHop4W_t;
        dsnh_offset = p_nhinfo->hdr.dsfwd_info.dsnh_offset;
        cmd = DRV_IOW(tbl_id, DRV_ENTRY_FLAG);
        DRV_IOCTL(lchip, dsnh_offset, cmd, &data_w);
     }

    return ret;
}

int32
sys_usw_nh_create_ipuc_cb(uint8 lchip, sys_nh_param_com_t* p_com_nh_para,
                                sys_nh_info_com_t* p_com_db)
{
    sys_nh_param_ipuc_t* p_nh_para;
    sys_nh_info_ipuc_t* p_nhdb;
	int32 ret = 0;
    ctc_ip_nh_param_t* p_ipuc_param = NULL;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_PTR_VALID_CHECK(p_com_nh_para);
    CTC_PTR_VALID_CHECK(p_com_db);
    CTC_EQUAL_CHECK(SYS_NH_TYPE_IPUC, p_com_nh_para->hdr.nh_param_type);
    p_nh_para = (sys_nh_param_ipuc_t*)(p_com_nh_para);
    p_nhdb = (sys_nh_info_ipuc_t*)(p_com_db);
    p_ipuc_param = p_nh_para->p_ipuc_param;


    if (p_ipuc_param->cid && !g_usw_nh_master[lchip]->cid_use_4w && !CTC_FLAG_ISSET(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_USE_DSNH8W) )
    {
        SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " Ipuc Nexthop Must using 4w mode for cid \n");
        return CTC_E_INVALID_CONFIG;
    }

    p_nhdb->hdr.nh_entry_type = SYS_NH_TYPE_IPUC;
    p_nhdb->gport = p_ipuc_param->oif.gport;
    sal_memcpy(p_nhdb->mac_da, p_ipuc_param->mac, sizeof(mac_addr_t));

    if(p_ipuc_param->aps_en)
    {
        /*protection path -- */
        p_nhdb->protection_path  = mem_malloc(MEM_NEXTHOP_MODULE, sizeof(sys_nh_info_ipuc_edit_info_t));
        if (!p_nhdb->protection_path)
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " No memory \n");
            return CTC_E_NO_MEMORY;
        }

        sal_memset(p_nhdb->protection_path, 0, sizeof(sys_nh_info_ipuc_edit_info_t));
        sal_memcpy(p_nhdb->protection_path->mac_da, p_ipuc_param->p_mac, sizeof(mac_addr_t));
    }

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_PARAM, "dsnh_offset:%d  gport:0x%x vid:%d cvid:%d\n",
                   p_ipuc_param->dsnh_offset, p_ipuc_param->oif.gport, p_ipuc_param->oif.vid, p_ipuc_param->oif.cvid);

    /*Create unresolved ipuc nh*/
    if (p_nh_para->is_unrov_nh)
    {
        sys_l3if_prop_t l3if_prop;
        CTC_SET_FLAG(p_nhdb->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_UNROV);

        sal_memset(&l3if_prop, 0, sizeof(sys_l3if_prop_t));
        if (p_nh_para->hdr.l3if_id)
        {
            l3if_prop.l3if_id = p_nh_para->hdr.l3if_id;
        }
        else if (!(CTC_IS_CPU_PORT(p_ipuc_param->oif.gport) || p_ipuc_param->oif.is_l2_port || p_com_nh_para->hdr.is_drop))
        {
            sys_usw_l3if_get_l3if_info_with_port_and_vlan(lchip, p_ipuc_param->oif.gport, p_ipuc_param->oif.vid, p_ipuc_param->oif.cvid, &l3if_prop);
        }

        p_nhdb->l3ifid  = l3if_prop.l3if_id;

        return CTC_E_NONE;
    }

    CTC_ERROR_GOTO(_sys_usw_nh_update_ipuc_fwd_attr(lchip, p_nh_para, p_nhdb),ret, error1);
 error1:
    if ((ret != CTC_E_NONE) && p_nhdb->protection_path && p_ipuc_param->aps_en)
    {
        mem_free(p_nhdb->protection_path);
    }
    return ret;

}

/**
 @brief Callback function of delete unicast ip nexthop

 @param[in] p_com_db, pointer used to store nexthop data

 @return CTC_E_XXX
 */
int32
sys_usw_nh_delete_ipuc_cb(uint8 lchip, sys_nh_info_com_t* p_data, uint32* p_nhid)
{
    sys_nh_info_ipuc_t* p_nhinfo;
    sys_nh_ref_list_node_t* p_ref_node, * p_tmp_node;
    sys_nh_param_dsfwd_t dsfwd_param;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_PTR_VALID_CHECK(p_data);
    CTC_PTR_VALID_CHECK(p_nhid);

    CTC_EQUAL_CHECK(SYS_NH_TYPE_IPUC, p_data->hdr.nh_entry_type);
    p_nhinfo = (sys_nh_info_ipuc_t*)(p_data);

    sys_usw_nh_update_ecmp_member(lchip, *p_nhid, SYS_NH_CHANGE_TYPE_NH_DELETE);

    /*1. Delete ecmp  list */
    p_ref_node = p_nhinfo->hdr.p_ref_nh_list;

    while (p_ref_node)
    {
        /*Remove from db*/
        p_tmp_node = p_ref_node;
        p_ref_node = p_ref_node->p_next;
        mem_free(p_tmp_node);
    }

    p_nhinfo->hdr.p_ref_nh_list = NULL;

    /*2. Delete this ipuc nexthop*/
    sal_memset(&dsfwd_param, 0, sizeof(sys_nh_param_dsfwd_t));


    if (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
    {
        dsfwd_param.dsfwd_offset = p_nhinfo->hdr.dsfwd_info.dsfwd_offset;

        dsfwd_param.drop_pkt = 1;
        /*Write table*/
        sys_usw_nh_write_entry_dsfwd(lchip, &dsfwd_param);

        /*Free DsFwd offset*/
        sys_usw_nh_offset_free(lchip,
            CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_DSFWD_IS_6W)?SYS_NH_ENTRY_TYPE_FWD:SYS_NH_ENTRY_TYPE_FWD_HALF, 1, p_nhinfo->hdr.dsfwd_info.dsfwd_offset);
    }

    CTC_UNSET_FLAG(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD);
	_sys_usw_nh_free_ipuc_nh_resource(lchip, p_nhinfo, *p_nhid);
	mem_free(p_nhinfo->protection_path);
    return CTC_E_NONE;
}

STATIC int32
_sys_usw_nh_update_ipuc_fwd_to_unrov(uint8 lchip, sys_nh_param_ipuc_t* p_nhpara, sys_nh_info_ipuc_t* p_nhinfo)
{
    sys_nh_param_special_t nh_para_spec;

    if (CTC_FLAG_ISSET(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
    {
        sal_memset(&nh_para_spec, 0, sizeof(sys_nh_param_special_t));
        nh_para_spec.hdr.have_dsfwd= TRUE;
        nh_para_spec.hdr.nh_param_type = SYS_NH_TYPE_UNROV;
        nh_para_spec.hdr.is_internal_nh = TRUE;
        /*1.2 update dsfwd to unrov nh's dsfwd, write dsfwd*/
        CTC_ERROR_RETURN(sys_usw_nh_create_special_cb(lchip, (sys_nh_param_com_t*)(&nh_para_spec), (sys_nh_info_com_t*)(p_nhinfo)));

    }
    CTC_SET_FLAG(p_nhinfo->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_UNROV);
    _sys_usw_nh_free_ipuc_nh_resource(lchip, p_nhinfo, p_nhpara->hdr.nhid);
    _sys_usw_nh_free_offset_by_nhinfo(lchip, SYS_NH_TYPE_IPUC, (sys_nh_info_com_t*)p_nhinfo);
    return CTC_E_NONE;
}



/**
 @brief Callback function used to update ipuc nexthop
 */
int32
sys_usw_nh_update_ipuc_cb(uint8 lchip, sys_nh_info_com_t* p_nh_db,
                                sys_nh_param_com_t* p_para)
{
    sys_nh_info_ipuc_t* p_nh_info;
    sys_nh_param_ipuc_t* p_nh_para;
    sys_nh_info_dsnh_t        dsnh_info;
    int32 ret = 0;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_PTR_VALID_CHECK(p_para);
    CTC_PTR_VALID_CHECK(p_nh_db);
    CTC_EQUAL_CHECK(SYS_NH_TYPE_IPUC, p_para->hdr.nh_param_type);
    CTC_EQUAL_CHECK(SYS_NH_TYPE_IPUC, p_nh_db->hdr.nh_entry_type);
    p_nh_info = (sys_nh_info_ipuc_t*)(p_nh_db);
    p_nh_para = (sys_nh_param_ipuc_t*)(p_para);

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_PARAM, "change_type:%d  have_dsfwd:%d \n",
                   p_nh_para->hdr.change_type, CTC_FLAG_ISSET(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD));

    if ((p_nh_para->hdr.change_type == SYS_NH_CHANGE_TYPE_UPDATE_FWD_ATTR)||
        (p_nh_para->hdr.change_type == SYS_NH_CHANGE_TYPE_UNROV_TO_FWD))
    {
        if(p_nh_para->p_ipuc_param->arp_id)
        {
           CTC_ERROR_RETURN(sys_usw_nh_get_arp_oif(lchip, p_nh_para->p_ipuc_param->arp_id, &p_nh_para->p_ipuc_param->oif, (uint8*)p_nh_para->p_ipuc_param->mac, &p_para->hdr.is_drop, &p_para->hdr.l3if_id));
           SYS_GLOBAL_PORT_CHECK(p_nh_para->p_ipuc_param->oif.gport);
           CTC_MAX_VALUE_CHECK(p_nh_para->p_ipuc_param->oif.vid, CTC_MAX_VLAN_ID);
        }
        if(p_nh_para->p_ipuc_param->p_arp_id)
        {
           CTC_ERROR_RETURN(sys_usw_nh_get_arp_oif(lchip, p_nh_para->p_ipuc_param->p_arp_id, &p_nh_para->p_ipuc_param->p_oif, (uint8*)p_nh_para->p_ipuc_param->p_mac, &p_para->hdr.is_drop, &p_para->hdr.p_l3if_id));
           SYS_GLOBAL_PORT_CHECK(p_nh_para->p_ipuc_param->p_oif.gport);
           CTC_MAX_VALUE_CHECK(p_nh_para->p_ipuc_param->p_oif.vid, CTC_MAX_VLAN_ID);
        }
    }

    sal_memset(&dsnh_info, 0, sizeof(sys_nh_info_dsnh_t));
    switch (p_nh_para->hdr.change_type)
    {
    case SYS_NH_CHANGE_TYPE_FWD_TO_UNROV:

        if (p_nh_info->arp_id)
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " [Nexthop] Arp nexthop cannot update to unrov \n");
           return CTC_E_INVALID_CONFIG;
        }

        CTC_ERROR_RETURN(_sys_usw_nh_update_ipuc_fwd_to_unrov(lchip, p_nh_para, p_nh_info));

        CTC_SET_FLAG(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_UNROV);

        break;
    case SYS_NH_CHANGE_TYPE_UPDATE_FWD_ATTR:

        p_nh_para->hdr.have_dsfwd  = CTC_FLAG_ISSET(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD);

        CTC_ERROR_RETURN(_sys_usw_nh_update_ipuc_fwd_attr(lchip, p_nh_para, p_nh_info));

        break;

    case SYS_NH_CHANGE_TYPE_UNROV_TO_FWD:

        if (!CTC_FLAG_ISSET(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_UNROV))
        {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " [Nexthop] Current nexthop isnt unresolved nexthop \n");
            return CTC_E_INVALID_CONFIG;

        }
        CTC_UNSET_FLAG(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_UNROV);
        p_nh_para->is_unrov_nh = FALSE;
        p_nh_para->hdr.have_dsfwd  = CTC_FLAG_ISSET(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD);


        CTC_ERROR_RETURN(_sys_usw_nh_update_ipuc_fwd_attr(lchip, p_nh_para, p_nh_info));


        break;

    case SYS_NH_CHANGE_TYPE_ADD_DYNTBL:
        {
           if (CTC_FLAG_ISSET(p_nh_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_DSFWD))
           {
                return CTC_E_NONE;
           }
           CTC_ERROR_RETURN(_sys_usw_nh_ipuc_add_dsfwd(lchip, p_nh_db));
        }
        break;

    default:
        return CTC_E_INVALID_PARAM;
    }
   if ((p_nh_info->hdr.p_ref_nh_list)&& (p_nh_info->hdr.p_ref_nh_list->p_ref_nhinfo->hdr.nh_entry_type != SYS_NH_TYPE_APS))
   {
       ret = (sys_usw_nh_update_ecmp_member(lchip, p_nh_para->hdr.nhid, p_nh_para->hdr.change_type));
       if (ret)
       {
            SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " [Nexthop] Nexthop update for ecmp fail!, ret:%d \n", ret);
       }
   }
   else if (p_nh_info->hdr.p_ref_nh_list)
   {
        sys_nh_ref_list_node_t* p_ref_node = NULL;
        sys_nh_info_com_t* p_nh_com = NULL;

        p_ref_node = p_nh_info->hdr.p_ref_nh_list;
        while (p_ref_node)
        {
            p_nh_com = p_ref_node->p_ref_nhinfo;
            sys_usw_nh_bind_aps_nh(lchip, p_nh_com->hdr.nh_id, p_nh_info->hdr.nh_id);
            p_ref_node = p_ref_node->p_next;
        }
   }
   CTC_ERROR_RETURN(sys_usw_nh_update_loopback_l2edit(lchip, p_nh_para->hdr.nhid, 1));

    return CTC_E_NONE;
}

/**
 @brief Callback function used to get ipuc nexthop
 */
int32
sys_usw_nh_get_ipuc_cb(uint8 lchip, sys_nh_info_com_t* p_nh_db, void* p_para)
{
    sys_nh_info_ipuc_t* p_nh_info;
    int32 ret = 0;
    ctc_ip_nh_param_t* p_ipuc_param = NULL;
    uint16 ifid = 0;
    ctc_l3if_t l3if_info;
    uint32 cmd = 0;
    DsNextHop4W_m ds_nexthop;

    SYS_NH_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    CTC_PTR_VALID_CHECK(p_para);
    CTC_PTR_VALID_CHECK(p_nh_db);
    CTC_EQUAL_CHECK(SYS_NH_TYPE_IPUC, p_nh_db->hdr.nh_entry_type);
    p_nh_info = (sys_nh_info_ipuc_t*)(p_nh_db);
    p_ipuc_param = (ctc_ip_nh_param_t*)p_para;

    if (CTC_FLAG_ISSET(p_nh_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_UNROV))
    {
        CTC_SET_FLAG(p_ipuc_param->flag, CTC_IP_NH_FLAG_UNROV);
    }
    if (CTC_FLAG_ISSET(p_nh_info->flag, SYS_NH_IPUC_FLAG_MERGE_DSFWD))
    {
       CTC_SET_FLAG(p_ipuc_param->flag, CTC_IP_NH_FLAG_MERGE_DSFWD);
    }

    if (CTC_FLAG_ISSET(p_nh_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_EGRESS_EDIT_MODE))
    {
        p_ipuc_param->dsnh_offset = p_nh_db->hdr.dsfwd_info.dsnh_offset;
    }
    p_ipuc_param->aps_en = CTC_FLAG_ISSET(p_nh_db->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_IS_APS_EN)?1:0;
    p_ipuc_param->arp_id = p_nh_info->arp_id;
    p_ipuc_param->adjust_length = p_nh_db->hdr.adjust_len;
    if (p_ipuc_param->aps_en)
    {
        p_ipuc_param->aps_bridge_group_id = p_nh_info->gport;
    }
    else
    {
        p_ipuc_param->oif.gport = p_nh_info->gport;
    }

    sal_memcpy(p_ipuc_param->mac, p_nh_info->mac_da, sizeof(mac_addr_t));
    ifid = p_nh_info->l3ifid;
    sal_memset(&l3if_info, 0, sizeof(l3if_info));
    l3if_info.l3if_type = 0xff;

    if (p_ipuc_param->aps_en)
    {
        ctc_aps_bridge_group_t aps_grp;
        sys_usw_aps_get_ports(lchip, p_nh_info->gport, &aps_grp);
        p_ipuc_param->oif.gport = aps_grp.working_gport;
        p_ipuc_param->p_oif.gport = aps_grp.protection_gport;
    }

    if (!CTC_IS_CPU_PORT(p_ipuc_param->oif.gport))
    {
        ret = (sys_usw_l3if_get_l3if_id(lchip, &l3if_info, &ifid));
        if (ret != CTC_E_NOT_EXIST)
        {
            p_ipuc_param->oif.vid = l3if_info.vlan_id;
           if (CTC_FLAG_ISSET(p_nh_info->flag, SYS_NH_IPUC_FLAG_L3IF_VALID))
           {
               p_ipuc_param->oif.ecmp_if_id = ifid;
           }
        }
        else
        {
            p_ipuc_param->oif.is_l2_port = 1;
        }

    }

    if (p_ipuc_param->aps_en && p_nh_info->protection_path)
    {
        p_ipuc_param->p_arp_id = p_nh_info->protection_path->arp_id;
        sal_memcpy(p_ipuc_param->p_mac, p_nh_info->protection_path->mac_da, sizeof(mac_addr_t));
        sal_memset(&l3if_info, 0, sizeof(l3if_info));
        ifid = p_nh_info->protection_path->l3ifid;
        l3if_info.l3if_type = 0xff;
        ret = (sys_usw_l3if_get_l3if_id(lchip, &l3if_info, &ifid));
        if (ret != CTC_E_NOT_EXIST)
        {
           p_ipuc_param->p_oif.vid = l3if_info.vlan_id;
           if (CTC_FLAG_ISSET(p_nh_info->flag, SYS_NH_IPUC_FLAG_L3IF_VALID))
           {
               p_ipuc_param->p_oif.ecmp_if_id = ifid;
           }
        }
        else
        {
           p_ipuc_param->p_oif.is_l2_port = 1;
        }
    }

    cmd = DRV_IOR(DsNextHop4W_t, DRV_ENTRY_FLAG);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, p_nh_db->hdr.dsfwd_info.dsnh_offset, cmd, &ds_nexthop));
    p_ipuc_param->mtu_no_chk = GetDsNextHop4W(V, mtuCheckEn_f, &ds_nexthop)?0:1;
    p_ipuc_param->cid = (GetDsNextHop4W(V, statsPtrLow_f, &ds_nexthop)&0xff);
    if (GetDsNextHop4W(V, ttlNoDecrease_f,  &ds_nexthop))
    {
       CTC_SET_FLAG(p_ipuc_param->flag, CTC_IP_NH_FLAG_TTL_NO_DEC);
    }

    if (p_nh_info->p_dsl2edit && p_nh_info->p_dsl2edit->fpma)
    {
       CTC_SET_FLAG(p_ipuc_param->flag, CTC_IP_NH_FLAG_FPMA);
    }

    if (CTC_FLAG_ISSET(p_nh_info->hdr.nh_entry_flags, SYS_NH_INFO_FLAG_HAVE_L2EDIT) && p_nh_info->l2edit_8w)
    {
        sys_nh_db_dsl2editeth8w_t*  p_dsl2edit_8w  = NULL;
        p_dsl2edit_8w = (sys_nh_db_dsl2editeth8w_t*)p_nh_info->p_dsl2edit;
        sal_memcpy(p_ipuc_param->mac_sa, p_dsl2edit_8w->mac_sa, sizeof(mac_addr_t));
        if (GetDsNextHop4W(V, payloadOperation_f, &ds_nexthop) == SYS_NH_OP_ROUTE_NOTTL)
        {
           CTC_SET_FLAG(p_ipuc_param->flag, CTC_IP_NH_FLAG_TTL_NO_DEC);
        }
    }

    return CTC_E_NONE;
}
