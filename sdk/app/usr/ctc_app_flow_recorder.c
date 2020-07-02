#ifdef CTC_APP_FLOW_RECORDER
#include "sal.h"
#include "sal_task.h"
#include "api/include/ctc_api.h"
#include "common/include/ctc_opf.h"
#include "common/include/ctc_hash.h"
#include "ctc_app_flow_recorder.h"

struct ctc_app_flow_recorder_entry_s
{
    ctc_ipfix_data_t data;
    uint32  acl_entry_id;
    uint32  stats_id;
    uint32  aging_status:1;
    uint32  rsv:31;
};
typedef struct ctc_app_flow_recorder_entry_s ctc_app_flow_recorder_entry_t;

struct ctc_app_flow_recorder_master_s
{
    ctc_hash_t* hash_entry;
    uint32   aging_count;
    uint32   aging_interval;
    uint8    resolve_conflict_level;
    uint8    resolve_conflict_en;
    uint8    enable_ipfix_level;
    uint8    queue_drop_stats_en;
    uint16   hash_sel_ref_cnt[CTC_ACL_KEY_HASH_IPV6-CTC_ACL_KEY_HASH_MAC+1][16];

};
typedef struct ctc_app_flow_recorder_master_s ctc_app_flow_recorder_master_t;


ctc_app_flow_recorder_master_t* g_app_flow_recorder_master = NULL;

#define CTC_APP_FR_RESOLVE_CONFILICT_LEVEL (g_app_flow_recorder_master->resolve_conflict_level)
#define CTC_APP_FR_ENABLE_IPFIX_LEVEL (g_app_flow_recorder_master->enable_ipfix_level)
#define CTC_APP_FR_MAX_ENTRIES  (32*1024)
#define CTC_APP_FR_OPF_TYPE  (CTC_OPF_CUSTOM_ID_START+0)
#define CTC_APP_FR_GROUP_ID  0xF0000001


enum ctc_app_fr_opf_type_e
{
    CTC_APP_FR_OPF_ENTRY_ID,
    CTC_APP_FR_OPF_STATS_ID,

    CTC_APP_FR_OPF_MAX
};

extern int32
sys_usw_ipfix_get_hash_field_sel(uint8 lchip, uint8 field_sel_id, uint8 key_type, ctc_ipfix_hash_field_sel_t* field_sel);
extern int32
sys_usw_ipfix_delete_entry_by_index(uint8 lchip, uint8 type, uint32 index, uint8 dir);

void ctc_app_flow_recorder_new_flow_isr( ctc_ipfix_data_t* info, void* userdata);

uint32 _ctc_app_flow_recorder_hash_key(void* data)
{
    return ctc_hash_caculate((uintptr)&((ctc_ipfix_data_t*)0)->export_reason, data);
}


bool _ctc_app_flow_recorder_hash_cmp(void* data1, void* data2)
{
    return sal_memcmp(data1, data2, ((uintptr)&((ctc_ipfix_data_t*)0)->export_reason)) ? FALSE : TRUE;
}

#define CTC_APP_ERROR_DEBUG(op) \
    { \
        int32 rv = (op); \
        if (rv < 0) \
        { \
            sal_printf("Call %s() error, Line:%d ret:%d\n",__FUNCTION__,__LINE__, rv); \
            return;\
        } \
    }

uint8 __ctc_app_fr_map_key_type(uint8 ipfix_key_type)
{
    if(ipfix_key_type == CTC_IPFIX_KEY_HASH_MAC)
    {
        return CTC_ACL_KEY_HASH_MAC;
    }
    else if(ipfix_key_type == CTC_IPFIX_KEY_HASH_IPV4)
    {
        return CTC_ACL_KEY_HASH_IPV4;
    }
    else if(ipfix_key_type == CTC_IPFIX_KEY_HASH_L2_L3)
    {
        return CTC_ACL_KEY_HASH_L2_L3;
    }
    else if(ipfix_key_type == CTC_IPFIX_KEY_HASH_IPV6)
    {
        return CTC_ACL_KEY_HASH_IPV6;
    }
    else if(ipfix_key_type == CTC_IPFIX_KEY_HASH_MPLS)
    {
        return CTC_ACL_KEY_HASH_MPLS;
    }

    return 0;
}
STATIC int32 _ctc_app_flow_recorder_build_hash_sel(ctc_ipfix_data_t* info, uint32 entry_id, uint8 is_key)
{
    int32 ret = 0;
    ctc_ipfix_hash_field_sel_t field_sel;
    ctc_field_key_t  field_key;

    uint8 acl_key_type = __ctc_app_fr_map_key_type(info->key_type);

    sal_memset(&field_sel, 0, sizeof(field_sel));
    CTC_ERROR_RETURN(sys_usw_ipfix_get_hash_field_sel(0, info->field_sel_id, info->key_type, &field_sel));

    sal_memset(&field_key, 0, sizeof(field_key));
    field_key.data = is_key ? 0 : 1;

    switch(info->key_type)
    {
        case CTC_IPFIX_KEY_HASH_MAC:
            if(field_sel.u.mac.gport || field_sel.u.mac.logic_port)
            {
                ctc_field_port_t field_port;
                ctc_field_port_t field_port_mask;
                sal_memset(&field_port, 0, sizeof(field_port));
                sal_memset(&field_port_mask, 0xFF, sizeof(field_port_mask));
                field_port.type = field_sel.u.mac.gport ? CTC_FIELD_PORT_TYPE_GPORT : CTC_FIELD_PORT_TYPE_LOGIC_PORT;

                field_key.type = CTC_FIELD_KEY_PORT;
                field_key.ext_data = &field_port;
                field_key.ext_mask = &field_port_mask;

                if(is_key)
                {
                    if(field_sel.u.mac.gport)
                    {
                        field_port.gport = info->gport;
                    }
                    else
                    {
                        field_port.logic_port = info->logic_port;
                    }
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mac.metadata)
            {
                field_key.type = CTC_FIELD_KEY_METADATA;
                if(is_key)
                {
                    field_key.data = info->logic_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mac.mac_da)
            {
                field_key.type = CTC_FIELD_KEY_MAC_DA;
                field_key.ext_data = (void*)&info->dst_mac;
                if(is_key)
                {
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.mac.mac_sa)
            {
                field_key.type = CTC_FIELD_KEY_MAC_SA;
                field_key.ext_data = (void*)&info->src_mac;
                if(is_key)
                {
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.mac.vlan_id)
            {
                field_key.type =  CTC_FIELD_KEY_SVLAN_ID;
                if(is_key)
                {
                    field_key.data = info->svlan ? info->svlan: info->cvlan;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.mac.cfi)
            {
                field_key.type = CTC_FLAG_ISSET(info->flags, CTC_IPFIX_DATA_SVLAN_TAGGED) ? CTC_FIELD_KEY_STAG_CFI: CTC_FIELD_KEY_CTAG_CFI;
                if(is_key)
                {
                    field_key.data = CTC_FLAG_ISSET(info->flags, CTC_IPFIX_DATA_SVLAN_TAGGED) ? info->svlan_cfi: info->cvlan_cfi;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.mac.cos)
            {
                field_key.type = CTC_FLAG_ISSET(info->flags, CTC_IPFIX_DATA_SVLAN_TAGGED) ? CTC_FIELD_KEY_STAG_COS: CTC_FIELD_KEY_CTAG_COS;
                if(is_key)
                {
                    field_key.data = CTC_FLAG_ISSET(info->flags, CTC_IPFIX_DATA_SVLAN_TAGGED) ? info->svlan_prio: info->cvlan_prio;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.mac.eth_type)
            {
                field_key.type = CTC_FIELD_KEY_ETHER_TYPE;
                if(is_key)
                {
                    field_key.data = info->ether_type;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }
            break;
        case CTC_IPFIX_KEY_HASH_IPV4:
            if(field_sel.u.ipv4.gport || field_sel.u.ipv4.logic_port)
            {
                ctc_field_port_t field_port;

                sal_memset(&field_port, 0, sizeof(field_port));
                field_port.type = field_sel.u.ipv4.gport ? CTC_FIELD_PORT_TYPE_GPORT : CTC_FIELD_PORT_TYPE_LOGIC_PORT;

                field_key.type = CTC_FIELD_KEY_PORT;
                field_key.ext_data = &field_key;
                CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
            }
            if(field_sel.u.ipv4.metadata)
            {
                field_key.type = CTC_FIELD_KEY_METADATA;
                if(is_key)
                {
                    field_key.data = info->logic_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.ipv4.ip_da && (32 == field_sel.u.ipv4.ip_da_mask))
            {
                field_key.type = CTC_FIELD_KEY_IP_DA;
                if(is_key)
                {
                    field_key.data = info->l3_info.ipv4.ipda;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.ip_sa && (32 == field_sel.u.ipv4.ip_sa_mask))
            {
                field_key.type = CTC_FIELD_KEY_IP_SA;
                if(is_key)
                {
                    field_key.data = info->l3_info.ipv4.ipsa;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.ip_protocol)
            {
                field_key.type = CTC_FIELD_KEY_IP_PROTOCOL;
                if(is_key)
                {
                    field_key.data = info->l4_info.type.ip_protocol;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.l4_src_port)
            {
                field_key.type = CTC_FIELD_KEY_L4_SRC_PORT;
                if(is_key)
                {
                    field_key.data = info->l4_info.l4_port.source_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.l4_dst_port)
            {
                field_key.type = CTC_FIELD_KEY_L4_DST_PORT;
                if(is_key)
                {
                    field_key.data = info->l4_info.l4_port.dest_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.icmp_type)
            {
                field_key.type = CTC_FIELD_KEY_ICMP_TYPE;
                if(is_key)
                {
                    field_key.data = info->l4_info.icmp.icmp_type;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.icmp_code)
            {
                field_key.type = CTC_FIELD_KEY_ICMP_CODE;
                if(is_key)
                {
                    field_key.data = info->l4_info.icmp.icmpcode;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.igmp_type)
            {
                field_key.type = CTC_FIELD_KEY_IGMP_TYPE;
                if(is_key)
                {
                    field_key.data = info->l4_info.igmp.igmp_type;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.vxlan_vni)
            {
                field_key.type = CTC_FIELD_KEY_VN_ID;
                if(is_key)
                {
                    field_key.data = info->l4_info.vni;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.gre_key)
            {
                field_key.type = CTC_FIELD_KEY_GRE_KEY;
                if(is_key)
                {
                    field_key.data = info->l4_info.gre_key;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv4.nvgre_key)
            {
                field_key.type = CTC_FIELD_KEY_NVGRE_KEY;
                if(is_key)
                {
                    field_key.data = info->l4_info.gre_key;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }
            break;
        case CTC_IPFIX_KEY_HASH_IPV6:
            if(field_sel.u.ipv6.gport || field_sel.u.ipv6.logic_port)
            {
                ctc_field_port_t field_port;

                sal_memset(&field_port, 0, sizeof(field_port));
                field_port.type = field_sel.u.ipv6.gport ? CTC_FIELD_PORT_TYPE_GPORT : CTC_FIELD_PORT_TYPE_LOGIC_PORT;

                field_key.type = CTC_FIELD_KEY_PORT;
                field_key.ext_data = &field_key;
                CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
            }
            if(field_sel.u.ipv6.metadata)
            {
                field_key.type = CTC_FIELD_KEY_METADATA;
                if(is_key)
                {
                    field_key.data = info->logic_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }
            if(field_sel.u.ipv6.ip_da && (128 == field_sel.u.ipv6.ip_da_mask))
            {
                field_key.type = CTC_FIELD_KEY_IPV6_DA;
                if(is_key)
                {
                    field_key.ext_data = (uint32*)&info->l3_info.ipv6.ipda;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.ip_sa && (128 == field_sel.u.ipv6.ip_sa_mask))
            {
                field_key.type = CTC_FIELD_KEY_IPV6_SA;
                if(is_key)
                {
                    field_key.ext_data = (uint32*)&info->l3_info.ipv6.ipsa;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.ip_protocol)
            {
                field_key.type = CTC_FIELD_KEY_IP_PROTOCOL;
                if(is_key)
                {
                    field_key.data = info->l4_info.type.ip_protocol;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.l4_src_port)
            {
                field_key.type = CTC_FIELD_KEY_L4_SRC_PORT;
                if(is_key)
                {
                    field_key.data = info->l4_info.l4_port.source_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.l4_dst_port)
            {
                field_key.type = CTC_FIELD_KEY_L4_DST_PORT;
                if(is_key)
                {
                    field_key.data = info->l4_info.l4_port.dest_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.icmp_type)
            {
                field_key.type = CTC_FIELD_KEY_ICMP_TYPE;
                if(is_key)
                {
                    field_key.data = info->l4_info.icmp.icmp_type;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.icmp_code)
            {
                field_key.type = CTC_FIELD_KEY_ICMP_CODE;
                if(is_key)
                {
                    field_key.data = info->l4_info.icmp.icmpcode;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.igmp_type)
            {
                field_key.type = CTC_FIELD_KEY_IGMP_TYPE;
                if(is_key)
                {
                    field_key.data = info->l4_info.igmp.igmp_type;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.vxlan_vni)
            {
                field_key.type = CTC_FIELD_KEY_VN_ID;
                if(is_key)
                {
                    field_key.data = info->l4_info.vni;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.gre_key)
            {
                field_key.type = CTC_FIELD_KEY_GRE_KEY;
                if(is_key)
                {
                    field_key.data = info->l4_info.gre_key;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.ipv6.nvgre_key)
            {
                field_key.type = CTC_FIELD_KEY_NVGRE_KEY;
                if(is_key)
                {
                    field_key.data = info->l4_info.gre_key;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }
            break;
        case CTC_IPFIX_KEY_HASH_L2_L3:
            if(field_sel.u.l2_l3.gport || field_sel.u.l2_l3.logic_port)
            {
                ctc_field_port_t field_port;

                sal_memset(&field_port, 0, sizeof(field_port));
                field_port.type = field_sel.u.l2_l3.gport ? CTC_FIELD_PORT_TYPE_GPORT : CTC_FIELD_PORT_TYPE_LOGIC_PORT;

                field_key.type = CTC_FIELD_KEY_PORT;
                field_key.ext_data = &field_key;
                CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
            }
            if(field_sel.u.l2_l3.metadata)
            {
                field_key.type = CTC_FIELD_KEY_METADATA;
                if(is_key)
                {
                    field_key.data = info->logic_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }
            if(field_sel.u.l2_l3.mac_da)
            {
                field_key.type = CTC_FIELD_KEY_MAC_DA;
                field_key.ext_data = (void*)&info->dst_mac;
                if(is_key)
                {
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.mac_sa)
            {
                field_key.type = CTC_FIELD_KEY_MAC_SA;
                field_key.ext_data = (void*)&info->src_mac;
                if(is_key)
                {
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.stag_vlan)
            {
                field_key.type = CTC_FIELD_KEY_SVLAN_ID;
                if(is_key)
                {
                    field_key.data = info->svlan;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.stag_cfi)
            {
                field_key.type = CTC_FIELD_KEY_STAG_CFI;
                if(is_key)
                {
                    field_key.data = info->svlan_cfi;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.stag_cos)
            {
                field_key.type = CTC_FIELD_KEY_STAG_COS;
                if(is_key)
                {
                    field_key.data = info->svlan_prio;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.ctag_vlan)
            {
                field_key.type = CTC_FIELD_KEY_CVLAN_ID;
                if(is_key)
                {
                    field_key.data = info->cvlan;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.ctag_cfi)
            {
                field_key.type = CTC_FIELD_KEY_CTAG_CFI;
                if(is_key)
                {
                    field_key.data = info->cvlan_cfi;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.ctag_cos)
            {
                field_key.type = CTC_FIELD_KEY_CTAG_COS;
                if(is_key)
                {
                    field_key.data = info->cvlan_prio;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.eth_type)
            {
                field_key.type = CTC_FIELD_KEY_ETHER_TYPE;
                if(is_key)
                {
                    field_key.data = info->ether_type;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.ip_da && (32 == field_sel.u.l2_l3.ip_da_mask))
            {
                field_key.type = CTC_FIELD_KEY_IP_DA;
                if(is_key)
                {
                    field_key.data = info->l3_info.ipv4.ipda;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.ip_sa && (32 == field_sel.u.l2_l3.ip_sa_mask))
            {
                field_key.type = CTC_FIELD_KEY_IP_SA;
                if(is_key)
                {
                    field_key.data = info->l3_info.ipv4.ipsa;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.ip_protocol)
            {
                field_key.type = CTC_FIELD_KEY_IP_PROTOCOL;
                if(is_key)
                {
                    field_key.data = info->l4_info.type.ip_protocol;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.l4_src_port)
            {
                field_key.type = CTC_FIELD_KEY_L4_SRC_PORT;
                if(is_key)
                {
                    field_key.data = info->l4_info.l4_port.source_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.l4_dst_port)
            {
                field_key.type = CTC_FIELD_KEY_L4_DST_PORT;
                if(is_key)
                {
                    field_key.data = info->l4_info.l4_port.dest_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.icmp_type)
            {
                field_key.type = CTC_FIELD_KEY_ICMP_TYPE;
                if(is_key)
                {
                    field_key.data = info->l4_info.icmp.icmp_type;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.icmp_code)
            {
                field_key.type = CTC_FIELD_KEY_ICMP_CODE;
                if(is_key)
                {
                    field_key.data = info->l4_info.icmp.icmpcode;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.igmp_type)
            {
                field_key.type = CTC_FIELD_KEY_IGMP_TYPE;
                if(is_key)
                {
                    field_key.data = info->l4_info.igmp.igmp_type;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.vxlan_vni)
            {
                field_key.type = CTC_FIELD_KEY_VN_ID;
                if(is_key)
                {
                    field_key.data = info->l4_info.vni;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.gre_key)
            {
                field_key.type = CTC_FIELD_KEY_GRE_KEY;
                if(is_key)
                {
                    field_key.data = info->l4_info.gre_key;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }

            if(field_sel.u.l2_l3.nvgre_key)
            {
                field_key.type = CTC_FIELD_KEY_NVGRE_KEY;
                if(is_key)
                {
                    field_key.data = info->l4_info.gre_key;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id, &field_key));
                }
            }
            break;
        default:
            if(field_sel.u.mpls.gport || field_sel.u.mpls.logic_port)
            {
                ctc_field_port_t field_port;
                ctc_field_port_t field_port_mask;
                sal_memset(&field_port, 0, sizeof(field_port));
                sal_memset(&field_port_mask, 0xFF, sizeof(field_port_mask));
                field_port.type = field_sel.u.mpls.gport ? CTC_FIELD_PORT_TYPE_GPORT : CTC_FIELD_PORT_TYPE_LOGIC_PORT;

                field_key.type = CTC_FIELD_KEY_PORT;
                field_key.ext_data = &field_port;
                field_key.ext_mask = &field_port_mask;

                if(is_key)
                {
                    if(field_sel.u.mpls.gport)
                    {
                        field_port.gport = info->gport;
                    }
                    else
                    {
                        field_port.logic_port = info->logic_port;
                    }
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.metadata)
            {
                field_key.type = CTC_FIELD_KEY_METADATA;
                if(is_key)
                {
                    field_key.data = info->logic_port;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.label_num)
            {
                field_key.type = CTC_FIELD_KEY_LABEL_NUM;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label_num;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label0_label)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_LABEL0;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[0].label;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }
            if(field_sel.u.mpls.mpls_label0_exp)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_EXP0;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[0].exp;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label0_ttl)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_TTL0;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[0].ttl;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label0_s)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_SBIT0;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[0].sbit;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label1_label)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_LABEL1;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[1].label;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }
            if(field_sel.u.mpls.mpls_label1_exp)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_EXP1;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[1].exp;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label1_ttl)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_TTL1;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[1].ttl;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label1_s)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_SBIT1;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[1].sbit;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label2_label)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_LABEL2;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[2].label;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }
            if(field_sel.u.mpls.mpls_label2_exp)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_EXP2;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[2].exp;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label2_ttl)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_TTL2;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[2].ttl;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }

            if(field_sel.u.mpls.mpls_label2_s)
            {
                field_key.type = CTC_FIELD_KEY_MPLS_SBIT2;
                if(is_key)
                {
                    field_key.data = info->l3_info.mpls.label[2].sbit;
                    CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
                }
                else
                {
                    CTC_ERROR_RETURN(ctc_acl_set_field_to_hash_field_sel(acl_key_type, info->field_sel_id,&field_key));
                }
            }
            break;
    }
    return ret;

}

STATIC int32 _ctc_app_flow_recorder_build_ad(uint32 entry_id, uint32 stats_id)
{
    int32 ret = 0;
    ctc_acl_field_action_t action_field;

    sal_memset(&action_field, 0, sizeof(action_field));
    action_field.type = CTC_ACL_FIELD_ACTION_STATS;
    action_field.data0 = stats_id;
    ctc_acl_add_action_field( entry_id, &action_field);

    action_field.type = CTC_ACL_FIELD_ACTION_CANCEL_ACL_TCAM_EN;
    action_field.data0 = CTC_APP_FR_ENABLE_IPFIX_LEVEL;

    ctc_acl_add_action_field( entry_id, &action_field);

    return ret;

}
STATIC int32 _ctc_app_flow_recorder_build_key(ctc_ipfix_data_t* info, uint32 entry_id)
{
    int32 ret = 0;
    uint8 key_type = 0;
    ctc_acl_entry_t acl_entry;

    key_type = __ctc_app_fr_map_key_type(info->key_type);
    if(g_app_flow_recorder_master->hash_sel_ref_cnt[key_type-CTC_ACL_KEY_HASH_MAC][info->field_sel_id] == 0)
    {
        CTC_ERROR_RETURN(_ctc_app_flow_recorder_build_hash_sel(info, entry_id, 0));
    }

    sal_memset(&acl_entry, 0, sizeof(acl_entry));
    acl_entry.entry_id =  entry_id;
    acl_entry.hash_field_sel_id = info->field_sel_id;
    acl_entry.mode = 1;
    acl_entry.key_type = key_type;
    CTC_ERROR_GOTO(ctc_acl_add_entry(CTC_APP_FR_GROUP_ID, &acl_entry), ret, error_0);

    CTC_ERROR_GOTO(_ctc_app_flow_recorder_build_hash_sel(info, entry_id, 1), ret, error_0);

    {
        ctc_field_key_t field_key;
        sal_memset(&field_key, 0, sizeof(field_key));

        field_key.type = CTC_FIELD_KEY_HASH_VALID;
        field_key.data = 1;
        CTC_ERROR_RETURN(ctc_acl_add_key_field(entry_id, &field_key));
    }
    g_app_flow_recorder_master->hash_sel_ref_cnt[key_type-CTC_ACL_KEY_HASH_MAC][info->field_sel_id]++;

    return CTC_E_NONE;

error_0:
    ctc_acl_remove_entry(entry_id);
    return ret;

}

void ctc_app_flow_recorder_new_flow_isr( ctc_ipfix_data_t* info, void* userdata)
{
    ctc_app_flow_recorder_entry_t* tmp_ipfix_info = NULL;
    ctc_opf_t opf;
    uint32   entry_id = 0;
    ctc_stats_statsid_t  stats;
    int32 ret = 0;

    if(g_app_flow_recorder_master == NULL)
    {
        return ;
    }

    if(info == NULL)
    {
        return ;
    }

    if(info->export_reason != CTC_IPFIX_REASON_NEW_FLOW_INSERT)
    {
        return;
    }
    if(ctc_hash_lookup(g_app_flow_recorder_master->hash_entry, (void*)info))
    {
        sal_printf("Entry exist, key index is 0x%x\n", info->key_index);
        return;
    }

    if(info->dir == CTC_EGRESS)
    {
        sal_printf("Only support ingress ipfix\n", info->key_index);
        return;
    }
    sal_memset(&opf, 0, sizeof(opf));
    sal_memset(&stats, 0, sizeof(stats));

    opf.pool_type = CTC_APP_FR_OPF_TYPE;
    opf.pool_index = CTC_APP_FR_OPF_STATS_ID;

    CTC_APP_ERROR_DEBUG(ctc_opf_alloc_offset(&opf, 1, &stats.stats_id));
    stats.type = CTC_STATS_STATSID_TYPE_FLOW_HASH;
    stats.dir = CTC_INGRESS;
    CTC_ERROR_GOTO(ctc_stats_create_statsid(&stats), ret, error_0);

    opf.pool_index = CTC_APP_FR_OPF_ENTRY_ID;
    CTC_ERROR_GOTO(ctc_opf_alloc_offset(&opf, 1, &entry_id), ret, error_1);

    /*1. Try to build hash entry*/
    CTC_APP_ERROR_DEBUG(_ctc_app_flow_recorder_build_key(info, entry_id));
    CTC_APP_ERROR_DEBUG(_ctc_app_flow_recorder_build_ad(entry_id, stats.stats_id));

    CTC_APP_ERROR_DEBUG(ctc_acl_install_entry(entry_id));

    ret = ctc_ipfix_delete_entry(info);
    if(ret && ret != CTC_E_NOT_EXIST)
    {
        goto error_1;
    }

    tmp_ipfix_info = mem_malloc(MEM_SYSTEM_MODULE, sizeof(ctc_ipfix_data_t));
    if(NULL == tmp_ipfix_info)
    {
        return;
    }

    sal_memset(tmp_ipfix_info, 0, sizeof(ctc_app_flow_recorder_entry_t));
    sal_memcpy(&tmp_ipfix_info->data, info, sizeof(ctc_ipfix_data_t));
    tmp_ipfix_info->acl_entry_id = entry_id;
    tmp_ipfix_info->stats_id = stats.stats_id;

    ctc_hash_insert(g_app_flow_recorder_master->hash_entry, tmp_ipfix_info);
    return;

error_1:
    ctc_stats_destroy_statsid(stats.stats_id);
error_0:
    opf.pool_type = CTC_APP_FR_OPF_TYPE;
    opf.pool_index = CTC_APP_FR_OPF_STATS_ID;
    ctc_opf_free_offset(&opf, 1, stats.stats_id);

    if (ret < 0)
    {
        sal_printf("Call %s() error, Line:%d ret:%d\n",__FUNCTION__,__LINE__, ret);
    }
    return;

}

int32 _ctc_app_flow_recoreder_delete_entry(void* data, void* user_data)
{
    ctc_app_flow_recorder_entry_t* pe = (ctc_app_flow_recorder_entry_t*)data;
    if(!pe)
    {
        return 0;
    }
    ctc_stats_destroy_statsid(pe->stats_id);
    return 0;
}

#define __API__
int32 ctc_app_flow_recorder_init(ctc_app_flow_recorder_init_param_t *p_param)
{
    int32  ret = 0;

    if(g_app_flow_recorder_master)
    {
        return CTC_E_NONE;
    }

    CTC_PTR_VALID_CHECK(p_param);
    g_app_flow_recorder_master = mem_malloc(MEM_SYSTEM_MODULE, sizeof(ctc_app_flow_recorder_master_t));
    if(NULL == g_app_flow_recorder_master)
    {
        return CTC_E_NO_MEMORY;
    }

    g_app_flow_recorder_master->hash_entry = ctc_hash_create(CTC_APP_FR_MAX_ENTRIES/512, 512,\
        _ctc_app_flow_recorder_hash_key,_ctc_app_flow_recorder_hash_cmp);
    if(g_app_flow_recorder_master->hash_entry == NULL)
    {
        return CTC_E_NO_MEMORY;
    }

    g_app_flow_recorder_master->enable_ipfix_level = p_param->enable_ipfix_level;
    g_app_flow_recorder_master->queue_drop_stats_en = p_param->queue_drop_stats_en;
    g_app_flow_recorder_master->resolve_conflict_en = p_param->resolve_conflict_en;
    g_app_flow_recorder_master->resolve_conflict_level = p_param->resolve_conflict_level;

    if(p_param->resolve_conflict_en && (p_param->enable_ipfix_level  <= p_param->resolve_conflict_level))
    {
        sal_printf("Resolve conflict level must greater than enable ipfix level");
        ret = CTC_E_INVALID_PARAM;
        goto error_0;
    }
    /*global enable*/
    {
        ctc_global_flow_recorder_t  fr;

        fr.queue_drop_stats_en = p_param->queue_drop_stats_en;
        fr.resolve_conflict_en = p_param->resolve_conflict_en;
        fr.resolve_conflict_level = p_param->resolve_conflict_level;
        CTC_ERROR_GOTO(ctc_global_ctl_set(CTC_GLOBAL_FLOW_RECORDER_EN, &fr), ret, error_0);
    }

    /*init opf*/
    {
        ctc_opf_t opf;
        uint32 index = 0;

        CTC_ERROR_GOTO(ctc_opf_init(CTC_APP_FR_OPF_TYPE, CTC_APP_FR_OPF_MAX), ret, error_0);

        opf.pool_type = CTC_APP_FR_OPF_TYPE;
        for(index=0; index < CTC_APP_FR_OPF_MAX; index++)
        {
            opf.pool_index = index;
            CTC_ERROR_GOTO(ctc_opf_init_offset(&opf, 1, CTC_APP_FR_MAX_ENTRIES), ret, error_0);
        }
    }

    /*register ipfix call back*/
    {
        CTC_ERROR_GOTO(ctc_ipfix_register_cb(ctc_app_flow_recorder_new_flow_isr, NULL), ret, error_0);
    }

    {
        ctc_acl_group_info_t group;

        sal_memset(&group, 0, sizeof(group));
        group.dir = 0;
        group.type = CTC_ACL_GROUP_TYPE_NONE;

        CTC_ERROR_GOTO(ctc_acl_create_group(CTC_APP_FR_GROUP_ID, &group), ret, error_0)
    }

    return CTC_E_NONE;

error_0:
    ctc_app_flow_recorder_deinit();
    return ret;
}

int32 ctc_app_flow_recorder_deinit(void)
{
    if(g_app_flow_recorder_master == NULL)
    {
        return CTC_E_NONE;
    }

    ctc_opf_deinit(0, CTC_APP_FR_OPF_TYPE);
    ctc_acl_uninstall_group(CTC_APP_FR_GROUP_ID);
    ctc_acl_remove_all_entry(CTC_APP_FR_GROUP_ID);
    ctc_acl_destroy_group(CTC_APP_FR_GROUP_ID);

    ctc_hash_traverse(g_app_flow_recorder_master->hash_entry, _ctc_app_flow_recoreder_delete_entry, NULL);
    ctc_hash_free(g_app_flow_recorder_master->hash_entry);

    mem_free(g_app_flow_recorder_master);

    return CTC_E_NONE;
}

int32 ctc_app_flow_recorder_show_status(void)
{
    if(g_app_flow_recorder_master == NULL)
    {
        return CTC_E_NONE;
    }

    sal_printf("Flow recorder status\n");
    sal_printf("=============================================\n");
    sal_printf("%-25s:%-20u\n", "Entry count", g_app_flow_recorder_master->hash_entry->count);
    sal_printf("%-25s:%-20u\n", "Resolve conflict en", g_app_flow_recorder_master->resolve_conflict_en);
    sal_printf("%-25s:%-20u\n", "Resolve conflict level", g_app_flow_recorder_master->resolve_conflict_level);
    sal_printf("%-25s:%-20u\n", "Enable ipfix level", g_app_flow_recorder_master->enable_ipfix_level);
    sal_printf("%-25s:%-20u\n", "Queue stats drop en", g_app_flow_recorder_master->queue_drop_stats_en);
    sal_printf("=============================================\n");

    return CTC_E_NONE;
}
#endif

