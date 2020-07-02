/**
 @file sys_usw_common.c

 @date 2011-11-28

 @version v2.0

 This file provides functions which can be used for GB
*/

#include "ctc_qos.h"
#include "ctc_error.h"
#include "ctc_port.h"
#include "ctc_register.h"
#include "ctc_vlan.h"
#include "sal.h"

#include "sys_usw_common.h"
#include "sys_usw_wb_common.h"
#include "usw/include/drv_io.h"
#include "sys_usw_wb_nh.h"
#include "sys_usw_register.h"

/* feature support array, 1 means enable, 0 means disable */
uint8 feature_support_arr[CTC_FEATURE_MAX] =
    /*PORT VLAN LINKAGG CHIP FTM NEXTHOP L2 L3IF IPUC IPMC IP_TUNNEL SCL ACL QOS SECURITY STATS MPLS OAM APS PTP DMA INTERRUPT PACKET PDU MIRROR BPE STACKING OVERLAY IPFIX EFD MONITOR FCOE TRILL WLAN, NPM VLAN_MAPPING DOT1AE DIAG*/
    { 1,   1,   1,       1,   1,  1,      1,   1,   1,  1,   1,        1,  1,  1,  1,       1,    1,   1,  1,  1,  1,   1,        1,     1,  1,     1,   1,       1,   1,    1,  1,      1,    1,    1,   1,   1,       1,    1,     1, 1, 1, 1};

extern int32
sys_usw_ftm_query_table_entry_num(uint8 lchip, uint32 table_id, uint32* entry_num);
extern int32
ctc_wb_add_appid(uint8 lchip, ctc_wb_appid_t *app_id);
extern int32
ctc_wb_show_appid_info(uint8 lchip);
extern uint32
ctc_wb_get_sync_bmp_addr(uint8 lchip, uintptr * addr);
extern uint32
ctc_wb_show_sync_status(uint8 lchip);

sys_wb_app_id_t  g_app_id_info[]={
{CTC_WB_APPID(CTC_FEATURE_PORT, SYS_WB_APPID_PORT_SUBID_MASTER),sizeof(sys_wb_port_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_PORT, SYS_WB_APPID_PORT_SUBID_PROP),sizeof(sys_wb_port_prop_t),256},
{CTC_WB_APPID(CTC_FEATURE_PORT, SYS_WB_APPID_PORT_SUBID_MAC_PROP),sizeof(sys_wb_port_mac_prop_t),256},
{CTC_WB_APPID(CTC_FEATURE_PORT, SYS_WB_APPID_INTERPORT_SUBID_MASTER),sizeof(sys_wb_interport_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_PORT, SYS_WB_APPID_INTERPORT_SUBID_USED),sizeof(sys_wb_interport_used_t),16},

{CTC_WB_APPID(CTC_FEATURE_VLAN, SYS_WB_APPID_VLAN_SUBID_MASTER),sizeof(sys_wb_vlan_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_VLAN, SYS_WB_APPID_VLAN_SUBID_OVERLAY_MAPPING_KEY),sizeof(sys_wb_vlan_overlay_mapping_key_t),4096},

{CTC_WB_APPID(CTC_FEATURE_VLAN_MAPPING, SYS_WB_APPID_SUBID_ADV_VLAN_MASTER),sizeof(sys_wb_adv_vlan_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_VLAN_MAPPING, SYS_WB_APPID_SUBID_ADV_VLAN_DEFAULT_ENTRY_ID),sizeof(sys_wb_adv_vlan_default_entry_id_t),18},
{CTC_WB_APPID(CTC_FEATURE_VLAN_MAPPING, SYS_WB_APPID_SUBID_ADV_VLAN_RANGE_GROUP),sizeof(sys_wb_adv_vlan_range_group_t),128},

{CTC_WB_APPID(CTC_FEATURE_APS, SYS_WB_APPID_APS_SUBID_MASTER),sizeof(sys_wb_aps_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_APS, SYS_WB_APPID_APS_SUBID_NODE),sizeof(sys_wb_aps_node_t),4096},

{CTC_WB_APPID(CTC_FEATURE_REGISTER, SYS_WB_APPID_REGISTER_SUBID_MASTER),sizeof(sys_wb_register_master_t),1},

{CTC_WB_APPID(CTC_FEATURE_STACKING, SYS_WB_APPID_STACKING_SUBID_MASTER),sizeof(sys_wb_stacking_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_STACKING, SYS_WB_APPID_STACKING_SUBID_TRUNK),sizeof(sys_wb_stacking_trunk_t),64},
{CTC_WB_APPID(CTC_FEATURE_STACKING, SYS_WB_APPID_STACKING_SUBID_MCAST_DB),sizeof(sys_wb_stacking_mcast_db_t),1280},

{CTC_WB_APPID(CTC_FEATURE_L3IF, SYS_WB_APPID_L3IF_SUBID_MASTER),sizeof(sys_wb_l3if_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_L3IF, SYS_WB_APPID_L3IF_SUBID_PROP),sizeof(sys_wb_l3if_prop_t),4095},
{CTC_WB_APPID(CTC_FEATURE_L3IF, SYS_WB_APPID_L3IF_SUBID_ROUTER_MAC),sizeof(sys_wb_l3if_router_mac_t),64},
{CTC_WB_APPID(CTC_FEATURE_L3IF, SYS_WB_APPID_L3IF_SUBID_ECMP_IF),sizeof(sys_wb_l3if_ecmp_if_t),1023},

{CTC_WB_APPID(CTC_FEATURE_IPMC, SYS_WB_APPID_IPMC_SUBID_MASTER),sizeof(sys_wb_ipmc_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_IPMC, SYS_WB_APPID_IPMC_SUBID_GROUP_NODE),sizeof(sys_wb_ipmc_group_node_t),2048},
{CTC_WB_APPID(CTC_FEATURE_IPMC, SYS_WB_APPID_IPMC_SUBID_G_NODE),sizeof(sys_wb_ipmc_g_node_t),2048},

{CTC_WB_APPID(CTC_FEATURE_IPUC, SYS_WB_APPID_IPUC_SUBID_MASTER),sizeof(sys_wb_ipuc_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_IPUC, SYS_WB_APPID_IPUC_SUBID_OFB),sizeof(sys_wb_ofb_t),7776/sizeof(sys_wb_ofb_t)},
{CTC_WB_APPID(CTC_FEATURE_IPUC, SYS_WB_APPID_IPUC_SUBID_INFO),sizeof(sys_wb_ipuc_info_t),102400},
{CTC_WB_APPID(CTC_FEATURE_IPUC, SYS_WB_APPID_IPUC_SUBID_INFO1),sizeof(sys_wb_nalpm_prefix_info_t),16384},

{CTC_WB_APPID(CTC_FEATURE_MPLS, SYS_WB_APPID_MPLS_SUBID_MASTER),sizeof(sys_wb_mpls_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_MPLS, SYS_WB_APPID_MPLS_SUBID_ILM_HASH),sizeof(sys_wb_mpls_ilm_hash_t),16448},

{CTC_WB_APPID(CTC_FEATURE_SCL, SYS_WB_APPID_SCL_SUBID_MASTER),sizeof(sys_wb_scl_master_t),16},
{CTC_WB_APPID(CTC_FEATURE_SCL, SYS_WB_APPID_SCL_SUBID_GROUP),sizeof(sys_wb_scl_group_t),2048},
{CTC_WB_APPID(CTC_FEATURE_SCL, SYS_WB_APPID_SCL_SUBID_ENTRY),sizeof(sys_wb_scl_entry_t),18432},
{CTC_WB_APPID(CTC_FEATURE_SCL, SYS_WB_APPID_SCL_SUBID_HASH_KEY_ENTRY),sizeof(sys_wb_scl_hash_key_entry_t),16384},
{CTC_WB_APPID(CTC_FEATURE_SCL, SYS_WB_APPID_SCL_SUBID_TCAM_KEY_ENTRY),sizeof(sys_wb_scl_tcam_key_entry_t),2048},
{CTC_WB_APPID(CTC_FEATURE_SCL, SYS_WB_APPID_SCL_SUBID_DEFAULT_ENTRY),sizeof(sys_wb_scl_entry_t),256*3},

{CTC_WB_APPID(CTC_FEATURE_WLAN, SYS_WB_APPID_WLAN_SUBID_MASTER),sizeof(sys_wb_wlan_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_WLAN, SYS_WB_APPID_WLAN_SUBID_TUNNEL_SW_ENTRY),sizeof(sys_wb_wlan_tunnel_sw_entry_t),2048},

{CTC_WB_APPID(CTC_FEATURE_DOT1AE, SYS_WB_APPID_DOT1AE_SUBID_MASTER),sizeof(sys_wb_dot1ae_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_DOT1AE, SYS_WB_APPID_DOT1AE_SUBID_CHANNEL),sizeof(sys_wb_dot1ae_channel_t),64},
{CTC_WB_APPID(CTC_FEATURE_DOT1AE, SYS_WB_APPID_DOT1AE_SUBID_CHAN_BIND_NODE),sizeof(sys_wb_dot1ae_channel_bind_node_t),64},

{CTC_WB_APPID(CTC_FEATURE_CHIP, SYS_WB_APPID_CHIP_SUBID_MASTER),sizeof(sys_wb_chip_master_t),1},

{CTC_WB_APPID(CTC_FEATURE_OAM, SYS_WB_APPID_OAM_SUBID_MASTER),sizeof(sys_wb_oam_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_OAM, SYS_WB_APPID_OAM_SUBID_CHAN),sizeof(sys_wb_oam_chan_t),16384},
{CTC_WB_APPID(CTC_FEATURE_OAM, SYS_WB_APPID_OAM_SUBID_MAID),sizeof(sys_wb_oam_maid_t),4096},
{CTC_WB_APPID(CTC_FEATURE_OAM, SYS_WB_APPID_OAM_SUBID_OAMID),sizeof(sys_wb_oam_oamid_t),2048},

{CTC_WB_APPID(CTC_FEATURE_MIRROR, SYS_WB_APPID_MIRROR_SUBID_MASTER),sizeof(sys_wb_mirror_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_MIRROR, SYS_WB_APPID_MIRROR_SUBID_DEST),sizeof(sys_wb_mirror_dest_t),104},

{CTC_WB_APPID(CTC_FEATURE_DATAPATH, SYS_WB_APPID_DATAPATH_SUBID_MASTER),sizeof(sys_wb_datapath_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_DATAPATH, SYS_WB_APPID_DATAPATH_SUBID_HSS_ATTRIBUTE),sizeof(sys_wb_datapath_hss_attribute_t),5},

{CTC_WB_APPID(CTC_FEATURE_FTM, SYS_WB_APPID_FTM_SUBID_AD),sizeof(sys_wb_ftm_ad_t),131072},

{CTC_WB_APPID(CTC_FEATURE_OVERLAY, SYS_WB_APPID_OVERLAY_SUBID_TUNNEL_MASTER),sizeof(sys_wb_overlay_tunnel_master_t),1},

{CTC_WB_APPID(CTC_FEATURE_IP_TUNNEL, SYS_WB_APPID_IP_TUNNEL_SUBID_MASTER),sizeof(sys_wb_ip_tunnel_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_IP_TUNNEL, SYS_WB_APPID_IP_TUNNEL_SUBID_NATSA_INFO),sizeof(sys_wb_ip_tunnel_natsa_info_t),16384},

{CTC_WB_APPID(CTC_FEATURE_ACL, SYS_WB_APPID_ACL_SUBID_MASTER),sizeof(sys_wb_acl_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_ACL, SYS_WB_APPID_ACL_SUBID_GROUP),sizeof(sys_wb_acl_group_t),2048},
{CTC_WB_APPID(CTC_FEATURE_ACL, SYS_WB_APPID_ACL_SUBID_ENTRY),sizeof(sys_wb_acl_entry_t),19456},
{CTC_WB_APPID(CTC_FEATURE_ACL, SYS_WB_APPID_ACL_SUBID_TCAM_CID),sizeof(sys_wb_ofb_t),128},
{CTC_WB_APPID(CTC_FEATURE_ACL, SYS_WB_APPID_ACL_SUBID_BLOCK),sizeof(sys_wb_acl_block_t),16},

{CTC_WB_APPID(CTC_FEATURE_IPFIX, SYS_WB_APPID_IPFIX_SUBID_MASTER),sizeof(sys_wb_ipfix_master_t),1},

{CTC_WB_APPID(CTC_FEATURE_SYNC_ETHER, SYS_WB_APPID_SYNCETHER_SUBID_MASTER),sizeof(sys_wb_syncether_master_t),1},

{CTC_WB_APPID(CTC_FEATURE_PTP, SYS_WB_APPID_PTP_SUBID_MASTER),sizeof(sys_wb_ptp_master_t),1},

{CTC_WB_APPID(CTC_FEATURE_SECURITY, SYS_WB_APPID_SECURITY_SUBID_MASTER),sizeof(sys_wb_security_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_SECURITY, SYS_WB_APPID_SECURITY_SUBID_TRHOLD_PROFILE),sizeof(sys_wb_security_trhold_profile_t),128},

{CTC_WB_APPID(CTC_FEATURE_LINKAGG, SYS_WB_APPID_LINKAGG_SUBID_MASTER),sizeof(sys_wb_linkagg_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_LINKAGG, SYS_WB_APPID_LINKAGG_SUBID_GROUP),sizeof(sys_wb_linkagg_group_t),512},
{CTC_WB_APPID(CTC_FEATURE_LINKAGG, SYS_WB_APPID_LINKAGG_SUBID_CHANGROUP),sizeof(sys_wb_linkagg_group_t),512},

{CTC_WB_APPID(CTC_FEATURE_L2, SYS_WB_APPID_L2_SUBID_MASTER),sizeof(sys_wb_l2_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_L2, SYS_WB_APPID_L2_SUBID_FDB),sizeof(sys_wb_l2_fdb_t), 32768},
{CTC_WB_APPID(CTC_FEATURE_L2, SYS_WB_APPID_L2_SUBID_VPORT_2_NHID),sizeof(sys_wb_l2_vport_2_nhid_t),16384},
{CTC_WB_APPID(CTC_FEATURE_L2, SYS_WB_APPID_L2_SUBID_FID_NODE),sizeof(sys_wb_l2_fid_node_t),16384},
{CTC_WB_APPID(CTC_FEATURE_L2, SYS_WB_APPID_L2_SUBID_MC),sizeof(sys_wb_l2_fdb_t),0},
{CTC_WB_APPID(CTC_FEATURE_L2, SYS_WB_APPID_L2_SUBID_FID_MEMBER),sizeof(sys_wb_l2_fdb_t),0},

{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_MASTER),sizeof(sys_wb_nh_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_BRGUC_NODE),sizeof(sys_wb_nh_brguc_node_t),1024},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_INFO_COM),sizeof(sys_wb_nh_info_com_t),32768},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_MPLS_TUNNEL),sizeof(sys_wb_nh_mpls_tunnel_t),1024},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_ECMP),sizeof(sys_wb_nh_info_ecmp_t),1024},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_HECMP),sizeof(sys_wb_nh_info_ecmp_t),1024},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_INFO_MCAST),sizeof(sys_wb_nh_info_mcast_t),16384},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_ARP),sizeof(sys_wb_nh_arp_t),16384},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_VNI),sizeof(sys_wb_nh_vni_fid_t),16384},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_EDIT),sizeof(sys_wb_nh_edit_t),8192},
{CTC_WB_APPID(CTC_FEATURE_NEXTHOP, SYS_WB_APPID_NH_SUBID_APS),sizeof(sys_wb_nh_info_aps_t),2048},

{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_MASTER),sizeof(sys_wb_qos_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_POLICER),sizeof(sys_wb_qos_policer_t),25088},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_MASTER),sizeof(sys_wb_qos_queue_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_QUEUE_NODE),sizeof(sys_wb_qos_queue_queue_node_t),4096},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_GROUP_NODE),sizeof(sys_wb_qos_queue_group_node_t),384},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_FC_PROFILE),sizeof(sys_wb_qos_queue_fc_profile_t),512},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_PFC_PROFILE),sizeof(sys_wb_qos_queue_pfc_profile_t),4096},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_PORT_EXTENDER),sizeof(sys_wb_qos_queue_port_extender_t),512},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_CPU_REASON),sizeof(sys_wb_qos_cpu_reason_t),1024},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_FC_DROPTH_PROFILE),sizeof(sys_wb_qos_queue_fc_profile_t),512},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_PFC_DROPTH_PROFILE),sizeof(sys_wb_qos_queue_pfc_profile_t),4096},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_LOGIC_SRC_PORT),sizeof(sys_wb_qos_logicsrc_port_t),512},
{CTC_WB_APPID(CTC_FEATURE_QOS, SYS_WB_APPID_QOS_SUBID_QUEUE_DESTPORT),sizeof(sys_wb_qos_destport_t),512},

{CTC_WB_APPID(CTC_FEATURE_STATS, SYS_WB_APPID_STATS_SUBID_MASTER),sizeof(sys_wb_stats_master_t),1},
{CTC_WB_APPID(CTC_FEATURE_STATS, SYS_WB_APPID_STATS_SUBID_STATSID),sizeof(sys_wb_stats_statsid_t),32768},
{CTC_WB_APPID(CTC_FEATURE_STATS, SYS_WB_APPID_STATS_SUBID_STATSPTR),sizeof(sys_wb_stats_statsptr_t),32768},
{CTC_WB_APPID(CTC_FEATURE_STATS, SYS_WB_APPID_STATS_SUBID_MAC_STATS),sizeof(sys_wb_mac_stats_t),60},
};

int32 sys_usw_common_wb_hash_init(uint8 lchip)
{
    ctc_wb_appid_t app_id_info ;
    uint16 loop, loop_num;

    loop_num = sizeof(g_app_id_info) / sizeof(sys_wb_app_id_t);

    for (loop = 0 ; loop < loop_num; loop++)
    {
        app_id_info.app_id = lchip << 16 | g_app_id_info[loop].app_id;
        app_id_info.entry_num = g_app_id_info[loop].entry_num;
        app_id_info.entry_size = g_app_id_info[loop].entry_size;
        CTC_ERROR_RETURN(ctc_wb_add_appid( lchip, &app_id_info));
    }
   return CTC_E_NONE;
}

char*
sys_output_mac(mac_addr_t in_mac)
{
    static char out_mac[20] = {0};

    sal_sprintf(out_mac, "%.2x%.2x.%.2x%.2x.%.2x%.2x", in_mac[0], in_mac[1], in_mac[2], in_mac[3], in_mac[4], in_mac[5]);
    return out_mac;
}


uint8
sys_usw_map_acl_tcam_lkup_type(uint8 lchip, uint8 ctc_lkup_t)
{
    switch(ctc_lkup_t)
    {
        case CTC_ACL_TCAM_LKUP_TYPE_L2:
            return SYS_ACL_TCAM_LKUP_TYPE_L2;
        case CTC_ACL_TCAM_LKUP_TYPE_L2_L3:
            return SYS_ACL_TCAM_LKUP_TYPE_L2_L3;
        case CTC_ACL_TCAM_LKUP_TYPE_L3:
            return SYS_ACL_TCAM_LKUP_TYPE_L3;
        case CTC_ACL_TCAM_LKUP_TYPE_VLAN:
            return SYS_ACL_TCAM_LKUP_TYPE_VLAN;
        case CTC_ACL_TCAM_LKUP_TYPE_L3_EXT:
            return SYS_ACL_TCAM_LKUP_TYPE_L3_EXT;
        case CTC_ACL_TCAM_LKUP_TYPE_L2_L3_EXT:
            return SYS_ACL_TCAM_LKUP_TYPE_L2_L3_EXT;
        case CTC_ACL_TCAM_LKUP_TYPE_CID:
            return SYS_ACL_TCAM_LKUP_TYPE_CID;
        case CTC_ACL_TCAM_LKUP_TYPE_INTERFACE:
            return SYS_ACL_TCAM_LKUP_TYPE_INTERFACE;
        case CTC_ACL_TCAM_LKUP_TYPE_FORWARD:
            return SYS_ACL_TCAM_LKUP_TYPE_FORWARD;
        case CTC_ACL_TCAM_LKUP_TYPE_FORWARD_EXT:
            return SYS_ACL_TCAM_LKUP_TYPE_FORWARD_EXT;
        case CTC_ACL_TCAM_LKUP_TYPE_UDF:
            return SYS_ACL_TCAM_LKUP_TYPE_UDF;
        case CTC_ACL_TCAM_LKUP_TYPE_COPP:
            return SYS_ACL_TCAM_LKUP_TYPE_COPP;
        default:
            return SYS_ACL_TCAM_LKUP_TYPE_NONE;
    }
}

uint8
sys_usw_unmap_acl_tcam_lkup_type(uint8 lchip, uint8 sys_lkup_t)
{
    switch(sys_lkup_t)
    {
        case SYS_ACL_TCAM_LKUP_TYPE_L2:
            return CTC_ACL_TCAM_LKUP_TYPE_L2;
        case SYS_ACL_TCAM_LKUP_TYPE_L2_L3:
            return CTC_ACL_TCAM_LKUP_TYPE_L2_L3;
        case SYS_ACL_TCAM_LKUP_TYPE_L3:
            return  CTC_ACL_TCAM_LKUP_TYPE_L3;
        case SYS_ACL_TCAM_LKUP_TYPE_VLAN:
            return CTC_ACL_TCAM_LKUP_TYPE_VLAN;
        case SYS_ACL_TCAM_LKUP_TYPE_L3_EXT:
            return  CTC_ACL_TCAM_LKUP_TYPE_L3_EXT;
        case SYS_ACL_TCAM_LKUP_TYPE_L2_L3_EXT:
            return CTC_ACL_TCAM_LKUP_TYPE_L2_L3_EXT;
        case SYS_ACL_TCAM_LKUP_TYPE_CID:
            return CTC_ACL_TCAM_LKUP_TYPE_CID;
        case SYS_ACL_TCAM_LKUP_TYPE_INTERFACE:
            return CTC_ACL_TCAM_LKUP_TYPE_INTERFACE;
        case SYS_ACL_TCAM_LKUP_TYPE_FORWARD:
            return CTC_ACL_TCAM_LKUP_TYPE_FORWARD;
        case SYS_ACL_TCAM_LKUP_TYPE_FORWARD_EXT:
            return CTC_ACL_TCAM_LKUP_TYPE_FORWARD_EXT;
        case SYS_ACL_TCAM_LKUP_TYPE_COPP:
            return CTC_ACL_TCAM_LKUP_TYPE_COPP;
        case SYS_ACL_TCAM_LKUP_TYPE_UDF:
            return CTC_ACL_TCAM_LKUP_TYPE_UDF;
        default:
            return CTC_ACL_TCAM_LKUP_TYPE_L2;
    }
}


/* value = division << shift_bits,is_negative = 0,the computed value is slightly larger*/
int32
sys_usw_common_get_compress_near_division_value(uint8 lchip, uint32 value,uint8 division_wide,
                                                                  uint8 shift_bit_wide, uint16* division, uint16* shift_bits, uint8 is_negative)
{
    uint8 i = 0;
    uint32 valuetmp = 0;
    uint32 base = 0;
    uint16 max_division = 0;
    uint16 max_shift_bit = 0;

    CTC_PTR_VALID_CHECK(division);
    CTC_PTR_VALID_CHECK(shift_bits);
    max_division = 1 << division_wide;
    max_shift_bit = 1 << shift_bit_wide;

    for (i = 0; i < max_shift_bit; i++)
    {
        base = (value >> i);

        if (base < max_division)
        {
            *division = base;
            *shift_bits = i;
            valuetmp = base << i;
            if (valuetmp != value)
            {
                break;
            }
            return CTC_E_NONE;
        }
    }

    valuetmp = value / max_division;
    if(0 == valuetmp)
    {
        *division = value;
        *shift_bits = 0;
        return CTC_E_NONE;
    }

    for (i = 1; i < max_shift_bit; i++)
    {
        if ((valuetmp >= (1 << (i - 1))) && (valuetmp <= ((1 << i)- 1)))
        {
            if(0 == is_negative)
            {
                if ((value + ((1 << i) - 1)) / (1 << i) >= max_division)
                {
                    i++;
                }

                *shift_bits = i;
                *division = (value + ((1 << i) - 1)) / (1 << i);
            }
            else
            {
                if (value  / (1 << i) >= max_division)
                {
                    i++;
                }

                *shift_bits = i;
                *division = value / (1 << i);
            }
            return CTC_E_NONE;
        }
    }

    return CTC_E_INVALID_PARAM;

}

extern uint32
sys_usw_global_get_chip_capability(uint8 lchip, ctc_global_capability_type_t type);

uint32
sys_usw_common_get_vport_num(uint8 lchip)
{
    return MCHIP_CAP(SYS_CAP_SPEC_LOGIC_PORT_NUM);
}

bool
sys_usw_common_check_feature_support(uint8 feature)
{
    if (feature_support_arr[feature])
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

const char*
sys_usw_get_pkt_type_desc(uint8 pkt_type)
{
    switch(pkt_type)
    {
        case CTC_PARSER_PKT_TYPE_ETHERNET:
            return "ETHERNET";
        case CTC_PARSER_PKT_TYPE_IP_OR_IPV6:
            return "IP/IPv4";
        case CTC_PARSER_PKT_TYPE_MPLS:
            return "MPLS";
        case CTC_PARSER_PKT_TYPE_IPV6:
            return "IPV6";
        case CTC_PARSER_PKT_TYPE_MCAST_MPLS:
            return "MCAST_MPLS";
        case CTC_PARSER_PKT_TYPE_CESOETH:
            return "CESOETH";
        case CTC_PARSER_PKT_TYPE_FLEXIBLE:
            return "FLEXIBLE";
        case CTC_PARSER_PKT_TYPE_RESERVED:
            return "RESERVED";
        default:
            return "Error type";
    }

}

int32
sys_usw_dword_reverse_copy(uint32* dest, uint32* src, uint32 len)
{
    uint32 i = 0;
    for (i = 0; i < len; i++)
    {
        *(dest + i) = *(src + (len - i - 1));
    }
    return 0;
}

int32
sys_usw_byte_reverse_copy(uint8* dest, uint8* src, uint32 len)
{
    uint32 i = 0;
    for (i = 0; i < len; i++)
    {
        *(dest + i) = *(src + (len - 1 - i));
    }
    return 0;
}

int32
sys_usw_swap32(uint32* data, int32 len, uint32 hton)
{
    int32 cnt = 0;

    for (cnt = 0; cnt < len; cnt++)
    {
        if (hton)
        {
            data[cnt] = sal_htonl(data[cnt]);
        }
        else
        {
            data[cnt] = sal_ntohl(data[cnt]);
        }
    }

    return CTC_E_NONE;
}

int32
sys_usw_task_create(uint8 lchip,
                                sal_task_t** ptask,
                                char* name,
                                size_t stack_size,
                                int prio,
                                sal_task_type_t task_type,
                                unsigned long long cpu_mask,
                                void (* start)(void*),
                                void* arg)
{
    int32 ret = 0;
    sal_task_t tmp_task;

    sal_memset(&tmp_task, 0, sizeof(sal_task_t));
    tmp_task.cpu_mask = cpu_mask;
    tmp_task.task_type = task_type;
    sal_sprintf(tmp_task.name, "%s-%d", name, lchip);

    ret = sal_task_create(ptask, tmp_task.name,stack_size, prio, start, arg);

    return ret;
}

int32
sys_usw_ma_clear_write_cache(uint8 lchip)
{
    uint32 cmd = 0;
    DsMaName_m ds_ma_name;
    uint8  trigger_timer = 0;
    uint32 ma_name_entry_num = 0;
    uint8  rsv_maname_idx = 0;

    if (!DRV_IS_DUET2(lchip))
    {
        return CTC_E_NONE;
    }

    sys_usw_ftm_query_table_entry_num(lchip, DsMaName_t,  &ma_name_entry_num);
    if (!ma_name_entry_num)
    {
        return CTC_E_NONE;
    }

    rsv_maname_idx = ma_name_entry_num - 1;
    cmd = DRV_IOW(DsMaName_t, DRV_ENTRY_FLAG);
    sal_memset(&ds_ma_name, 0xff, sizeof(ds_ma_name));
    for (trigger_timer = 0; trigger_timer < 8; trigger_timer++)
    {
        DRV_IOCTL(lchip, rsv_maname_idx, cmd, &ds_ma_name);
    }
    return CTC_E_NONE;
}

int32
sys_usw_ma_add_to_asic(uint8 lchip, uint32 ma_index, void* dsma)
{
    uint32 cmd = 0;

    cmd = DRV_IOW(DsMa_t, DRV_ENTRY_FLAG);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, ma_index, cmd, dsma));
    sys_usw_ma_clear_write_cache(lchip);
    return CTC_E_NONE;
}

