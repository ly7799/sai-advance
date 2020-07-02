/**
 @file ctc_goldengate_acl.c

 @date 2009-10-17

 @version v2.0

 The file contains acl APIs
*/

/***************************************************************
 *
 * Header Files
 *
 ***************************************************************/
#include "ctc_error.h"
#include "ctc_const.h"
#include "ctc_linklist.h"
#include "ctc_goldengate_acl.h"

#include "sys_goldengate_acl.h"
#include "sys_goldengate_chip.h"

int32
ctc_goldengate_acl_init(uint8 lchip, ctc_acl_global_cfg_t* acl_global_cfg)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    ctc_acl_global_cfg_t acl_cfg;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);
    if (NULL == acl_global_cfg)
    {
        sal_memset(&acl_cfg, 0, sizeof(ctc_acl_global_cfg_t));
        acl_cfg.ingress_port_service_acl_en = 0xF;
        acl_cfg.ingress_vlan_service_acl_en = 0xF;
        acl_global_cfg = &acl_cfg;
    }

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_goldengate_acl_init(lchip, acl_global_cfg));
    }

    return CTC_E_NONE;
}

int32
ctc_goldengate_acl_deinit(uint8 lchip)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_goldengate_acl_deinit(lchip));
    }

    return CTC_E_NONE;
}

int32
ctc_goldengate_acl_create_group(uint8 lchip, uint32 group_id, ctc_acl_group_info_t* group_info)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    uint8 all_lchip         = 0;
    int32 ret               = CTC_E_NONE;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(group_info);
    CTC_SET_API_LCHIP(lchip, group_info->lchip);

    if(CTC_ACL_GROUP_TYPE_PORT_BITMAP == group_info->type)
    {
        lchip_start = lchip;
        lchip_end   = lchip_start + 1;
        all_lchip = 0;
    }
    else if((CTC_ACL_GROUP_TYPE_PORT == group_info->type) && (!CTC_IS_LINKAGG_PORT(group_info->un.gport)))
    {
        SYS_MAP_GPORT_TO_LCHIP(group_info->un.gport, lchip_start);
        lchip_end = lchip_start + 1;
        all_lchip = 0;
    }
    else
    {
        all_lchip = 1;
    }

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_FOREACH_ERROR_RETURN(CTC_E_ACL_GROUP_EXIST,
                                  ret,
                                  sys_goldengate_acl_create_group(lchip, group_id, group_info));
    }

    /*rollback if error exist*/
    if(all_lchip)
    {
        CTC_FOREACH_ROLLBACK(lchip_start, lchip_end)
        {
            sys_goldengate_acl_destroy_group(lchip, group_id);
        }
    }

    return ret;
}

int32
ctc_goldengate_acl_destroy_group(uint8 lchip, uint32 group_id)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ACL_SCL_FOREACH_ERROR_RETURN(CTC_E_ACL_GROUP_UNEXIST,
                                  ret,
                                  sys_goldengate_acl_destroy_group(lchip, group_id));
    }

    return ret;
}

int32
ctc_goldengate_acl_install_group(uint8 lchip, uint32 group_id, ctc_acl_group_info_t* group_info)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    uint8 all_lchip         = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    if (group_info)
    {
        if(CTC_ACL_GROUP_TYPE_PORT_BITMAP == group_info->type)
        {
            CTC_SET_API_LCHIP(lchip, group_info->lchip);
            lchip_start = lchip;
            lchip_end   = lchip_start + 1;
            all_lchip = 0;
        }
        else if((CTC_ACL_GROUP_TYPE_PORT == group_info->type) && (!CTC_IS_LINKAGG_PORT(group_info->un.gport)))
        {
            SYS_MAP_GPORT_TO_LCHIP(group_info->un.gport, lchip_start);
            lchip_end = lchip_start + 1;
            all_lchip = 0;
        }
        else
        {
            all_lchip = 1;
        }
    }
    else
    {
        all_lchip = 1;
    }

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_ACL_SCL_FOREACH_ERROR_RETURN(CTC_E_ACL_GROUP_UNEXIST,
                                 ret,
                                 sys_goldengate_acl_install_group(lchip, group_id, group_info));
    }

    /*rollback if error exist*/
    if(all_lchip)
    {
        CTC_FOREACH_ROLLBACK(lchip_start, lchip_end)
        {
            sys_goldengate_acl_uninstall_group(lchip, group_id);
        }
    }

    return ret;
}

int32
ctc_goldengate_acl_uninstall_group(uint8 lchip, uint32 group_id)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ACL_SCL_FOREACH_ERROR_RETURN(CTC_E_ACL_GROUP_UNEXIST,
                                  ret,
                                  sys_goldengate_acl_uninstall_group(lchip, group_id));
    }

    /*rollback if error exist*/
    CTC_FOREACH_ROLLBACK(lchip_start, lchip_end)
    {
        sys_goldengate_acl_install_group(lchip, group_id, NULL);
    }

    return ret;
}

int32
ctc_goldengate_acl_get_group_info(uint8 lchip, uint32 group_id, ctc_acl_group_info_t* group_info)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        ret = sys_goldengate_acl_get_group_info(lchip, group_id, group_info);
        if(CTC_E_NONE == ret)   /*get group info success*/
        {
            break;
        }
    }

    return ret;
}

STATIC int32
_ctc_goldengate_acl_get_lchip_by_port_field(uint32 port_type,
                                       uint8 lchip,
                                       uint32 gport,
                                       uint8* lchip_id,
                                       uint8* all_lchip)
{
    if(CTC_FIELD_PORT_TYPE_PORT_BITMAP == port_type)
    {
        *lchip_id = lchip;
        *all_lchip = 0;
    }
    else if (CTC_FIELD_PORT_TYPE_GPORT == port_type)
    {
        SYS_MAP_GPORT_TO_LCHIP(gport, *lchip_id);
        *all_lchip = 0;
    }
    else
    {
        *all_lchip = 1;
    }

    return CTC_E_NONE;
}

int32
ctc_goldengate_acl_add_entry(uint8 lchip, uint32 group_id, ctc_acl_entry_t* acl_entry)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    uint8 all_lchip         = 0;
    uint8 lchip_id          = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count             = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    switch(acl_entry->key.type)
    {
        case CTC_ACL_KEY_MAC:
            _ctc_goldengate_acl_get_lchip_by_port_field(acl_entry->key.u.mac_key.port.type,
                                                        acl_entry->key.u.mac_key.port.lchip,
                                                        acl_entry->key.u.mac_key.port.gport,
                                                        &lchip_id,
                                                        &all_lchip);
            break;
        case CTC_ACL_KEY_MPLS:
            _ctc_goldengate_acl_get_lchip_by_port_field(acl_entry->key.u.mpls_key.port.type,
                                                        acl_entry->key.u.mpls_key.port.lchip,
                                                        acl_entry->key.u.mpls_key.port.gport,
                                                        &lchip_id,
                                                        &all_lchip);
            break;
        case CTC_ACL_KEY_IPV4:
            _ctc_goldengate_acl_get_lchip_by_port_field(acl_entry->key.u.ipv4_key.port.type,
                                                        acl_entry->key.u.ipv4_key.port.lchip,
                                                        acl_entry->key.u.ipv4_key.port.gport,
                                                        &lchip_id,
                                                        &all_lchip);
            break;
        case CTC_ACL_KEY_IPV6:
            _ctc_goldengate_acl_get_lchip_by_port_field(acl_entry->key.u.ipv6_key.port.type,
                                                        acl_entry->key.u.ipv6_key.port.lchip,
                                                        acl_entry->key.u.ipv6_key.port.gport,
                                                        &lchip_id,
                                                        &all_lchip);
            break;
        case CTC_ACL_KEY_PBR_IPV4:
            _ctc_goldengate_acl_get_lchip_by_port_field(acl_entry->key.u.pbr_ipv4_key.port.type,
                                                        acl_entry->key.u.pbr_ipv4_key.port.lchip,
                                                        acl_entry->key.u.pbr_ipv4_key.port.gport,
                                                        &lchip_id,
                                                        &all_lchip);
            break;
        case CTC_ACL_KEY_PBR_IPV6:
            _ctc_goldengate_acl_get_lchip_by_port_field(acl_entry->key.u.pbr_ipv6_key.port.type,
                                                        acl_entry->key.u.pbr_ipv6_key.port.lchip,
                                                        acl_entry->key.u.pbr_ipv6_key.port.gport,
                                                        &lchip_id,
                                                        &all_lchip);
            break;

        default:
             all_lchip                 = 1;
    }

    CTC_SET_API_LCHIP(lchip, lchip_id);
    lchip_start         = lchip;
    lchip_end           = lchip + 1;

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, all_lchip)
    {
        CTC_FOREACH_ERROR_RETURN2(CTC_E_ACL_GROUP_UNEXIST, CTC_E_CHIP_IS_REMOTE,
                                 ret,
                                 sys_goldengate_acl_add_entry(lchip, group_id, acl_entry));
    }

    /*rollback if error exist*/
    CTC_FOREACH_ROLLBACK(lchip_start, lchip_end)
    {
        sys_goldengate_acl_remove_entry(lchip, acl_entry->entry_id);
    }

    return ret;
}

int32
ctc_goldengate_acl_remove_entry(uint8 lchip, uint32 entry_id)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count             = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_FOREACH_ERROR_RETURN2(CTC_E_ACL_ENTRY_UNEXIST, CTC_E_ACL_GROUP_UNEXIST,
                                 ret,
                                 sys_goldengate_acl_remove_entry(lchip, entry_id));
    }

    return ret;
}

int32
ctc_goldengate_acl_install_entry(uint8 lchip, uint32 entry_id)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count             = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_FOREACH_ERROR_RETURN2(CTC_E_ACL_ENTRY_UNEXIST, CTC_E_ACL_GROUP_UNEXIST,
                                 ret,
                                 sys_goldengate_acl_install_entry(lchip, entry_id));
    }

    /*rollback if error exist*/
    CTC_FOREACH_ROLLBACK(lchip_start, lchip_end)
    {
        sys_goldengate_acl_uninstall_entry(lchip, entry_id);
    }

    return ret;
}

int32
ctc_goldengate_acl_uninstall_entry(uint8 lchip, uint32 entry_id)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count             = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_FOREACH_ERROR_RETURN2(CTC_E_ACL_ENTRY_UNEXIST, CTC_E_ACL_GROUP_UNEXIST,
                                 ret,
                                 sys_goldengate_acl_uninstall_entry(lchip, entry_id));
    }

    /*rollback if error exist*/
    CTC_FOREACH_ROLLBACK(lchip_start, lchip_end)
    {
        sys_goldengate_acl_install_entry(lchip, entry_id);
    }

    return ret;
}

int32
ctc_goldengate_acl_remove_all_entry(uint8 lchip, uint32 group_id)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ACL_SCL_FOREACH_ERROR_RETURN(CTC_E_ACL_GROUP_UNEXIST,
                                 ret,
                                 sys_goldengate_acl_remove_all_entry(lchip, group_id));
    }

    return ret;
}

int32
ctc_goldengate_acl_set_entry_priority(uint8 lchip, uint32 entry_id, uint32 priority)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;
    uint8 count             = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_FOREACH_ERROR_RETURN2(CTC_E_ACL_ENTRY_UNEXIST, CTC_E_ACL_GROUP_UNEXIST,
                                  ret,
                                  sys_goldengate_acl_set_entry_priority(lchip, entry_id, priority));
    }

    return ret;
}

int32
ctc_goldengate_acl_get_multi_entry(uint8 lchip, ctc_acl_query_t* query)
{
    uint8 lchip_start       = 0;
    uint8 lchip_end         = 0;
    int32 ret               = CTC_E_NONE;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        ret = sys_goldengate_acl_get_multi_entry(lchip, query);
        if(CTC_E_NONE == ret)    /*get entries successfully*/
        {
            break;
        }
    }

    return ret;
}

int32
ctc_goldengate_acl_update_action(uint8 lchip, uint32 entry_id, ctc_acl_action_t* action)
{
    uint8 lchip_start   = 0;
    uint8 lchip_end     = 0;
    int32 ret           = CTC_E_NONE;
    uint8 count         = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_FOREACH_ERROR_RETURN2(CTC_E_ACL_GROUP_UNEXIST, CTC_E_ACL_ENTRY_UNEXIST,
                                  ret,
                                  sys_goldengate_acl_update_action(lchip, entry_id, action));
    }

    return ret;
}

int32
ctc_goldengate_acl_copy_entry(uint8 lchip, ctc_acl_copy_entry_t* copy_entry)
{
    uint8 lchip_start   = 0;
    uint8 lchip_end     = 0;
    int32 ret           = CTC_E_NONE;
    uint8 count         = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_FOREACH_ERROR_RETURN2(CTC_E_ACL_GROUP_UNEXIST, CTC_E_ACL_ENTRY_UNEXIST,
                                  ret,
                                  sys_goldengate_acl_copy_entry(lchip, copy_entry));
    }

    return ret;
}

int32
ctc_goldengate_acl_set_hash_field_sel(uint8 lchip, ctc_acl_hash_field_sel_t* field_sel)
{
    uint8 lchip_start   = 0;
    uint8 lchip_end     = 0;
    int32 ret           = CTC_E_NONE;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_ACL);
    LCHIP_CHECK(lchip);

    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_goldengate_acl_set_hash_field_sel(lchip, field_sel));
    }

    return ret;
}

