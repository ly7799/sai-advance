#ifndef _DRV_STRUCT_D2_H
#define _DRV_STRUCT_D2_H

#ifdef __cplusplus
extern "C" {
#endif

#if (HOST_IS_LE == 1)
struct ms_packet_header_d2_s
{
    /*word0[31:0] word1[31:0]*/
    union 
    {
        struct
        {
            uint32 stacking_src_port                                        : 16;
            uint32 c2c_check_disable                                        : 1;
            uint32 sgmac_strip_header                                       : 1;
            uint32 neighbor_discover_packet                                 : 1;
            uint32 rsv0                                                         : 13;
            
            uint32 rsv1                                                         : 32;
            
        }c2c;

        struct
        {
            uint32 dm_offset                                                : 8;
            uint32 rsv0                                                         : 24;

            uint32 rsv1                                                         : 32;
        }dmtx;

        struct
        {
            uint32 user_default_cfi                                         : 1;
            uint32 user_default_cos                                         : 3;
            uint32 user_default_vlan_id                                     : 12;
            uint32 use_default_vlan_tag                                     : 1;
            uint32 user_default_vlan_valid                                  : 1;
            uint32 service_acl_qos_en                                       : 1;
            uint32 capwap_tunnel_type                                       : 2;
            uint32 capwap_mcast                                             : 1;
            uint32 need_dot1ae_decrypt                                      : 1;
            uint32 need_capwap_decap                                        : 1;
            uint32 need_capwap_decrypt                                      : 1;
            uint32 mirrored_packet_with_sec_tag                             : 1;
            uint32 mirrored_packet_with_sci                                 : 1;
            uint32 src_dscp_valid                                           : 1;
            uint32 acl_dscp_valid                                           : 1;
            uint32 capwap_encaped                                           : 1;
            uint32 decrypt_key_index_1_0                                    : 2;

            uint32 decrypt_key_index_7_2                                    : 6;
            uint32 reassemble_offset                                        : 8;
            uint32 need_capwap_reassemble                                   : 1;
            uint32 capwap_last_frag                                         : 1;
            uint32 rsv1                                                         : 16;
        }e2iloop_in;

        struct
        {
            uint32 user_default_cfi                                         : 1;
            uint32 user_default_cos                                         : 3;
            uint32 user_default_vlan_id                                     : 12;
            uint32 use_default_vlan_tag                                     : 1;
            uint32 user_default_vlan_valid                                  : 1;
            uint32 service_acl_qos_en                                       : 1;
            uint32 capwap_tunnel_type                                       : 2;
            uint32 capwap_mcast                                             : 1;
            uint32 need_dot1ae_decrypt                                      : 1;
            uint32 need_capwap_decap                                        : 1;
            uint32 need_capwap_decrypt                                      : 1;
            uint32 mirrored_packet_with_sec_tag                             : 1;
            uint32 mirrored_packet_with_sci                                 : 1;
            uint32 src_dscp_valid                                           : 1;
            uint32 acl_dscp_valid                                           : 1;
            uint32 capwap_encaped                                           : 1;
            uint32 need_dot1ae_encrypt                                      : 1;

            uint32 rsv1                                                         : 32;
        }e2iloop_out;

        struct
        {
            uint32 rxtx_fcl                                                 : 8;
            uint32 rx_fcb_23_0                                              : 24;

            uint32 rx_fcb_31_24                                             : 8;
            uint32 tx_fcb_23_0                                              : 24;
        }lmtx;

        struct
        {
            uint32 no_dot1ae_encrypt                                        : 1;
            uint32 micro_burst_valid                                        : 1;
            uint32 acl_dscp_valid                                           : 1;
            uint32 src_dscp_valid                                           : 1;
            uint32 ipsa_prefix_length                                       : 3;
            uint32 ipsa_mode                                                : 2;
            uint32 pt_enable                                                : 1;
            uint32 ipv4_src_embeded_mode                                    : 1;
            uint32 src_address_mode                                         : 1;
            uint32 ipsa_valid                                               : 1;
            uint32 ipsa_18_0                                                : 19;

            uint32 ipsa_39_19                                               : 21;
            uint32 l4_source_port_valid                                     : 1;
            uint32 l4_source_port_9_0                                       : 10;
        }nat;

        struct
        {
            uint32 no_dot1ae_encrypt                                        : 1;
            uint32 metadata                                                 : 14;
            uint32 metadata_type                                            : 2;
            uint32 microburst_valid                                         : 1;
            uint32 cvlan_tag_operation_valid                                : 1;
            uint32 isid_valid                                                 : 1;
            uint32 src_cvlan_id                                             : 12;

            uint32 source_port_isolateId                                : 7;
            uint32 src_ctag_offset_type                                     : 1;
            uint32 src_ctag_cfi                                             : 1;
            uint32 src_ctag_cos                                             : 3;
            uint32 ctag_action                                              : 2;
            uint32 pbb_check_discard                                        :1;
            uint32 cn_action                                                : 2;
            uint32 src_dscp_valid                                           : 1;
            uint32 acl_dscp_valid                                           : 1;
            uint32 ecmp_hash                                                : 8;
            uint32 dot1ae_encrypted                                         : 1;
            uint32 acl_ctag_cfi_valid                                       : 1;
            uint32 acl_stag_cfi_valid                                       : 1;
            uint32 acl_ctag_cos_valid                                       : 1;
            uint32 acl_stag_cos_valid                                       : 1;
        }normal;

        struct
        {
            uint32 rxtx_fcl                                                 : 8;
            uint32 rx_fcb_23_0                                              : 24;

            uint32 rx_fcb_31_24                                             : 8;
            uint32 lm_received_packet                                       : 1;
            uint32 dm_en                                                    : 1;
            uint32 is_up                                                    : 1;
            uint32 use_oam_ttl                                              : 1;
            uint32 gal_exist                                                : 1;
            uint32 iloop_to_agg_up_mep                                      : 1;
            uint32 link_oam                                                 : 1;
            uint32 oam_type                                                 : 4;
            uint32 mep_index                                                : 13;
        }oam;

        struct
        {
            uint32 is_cflex_update_residence_time                           : 1;
            uint32 src_dscp_valid                                           : 1;
            uint32 acl_dscp_valid                                           : 1;
            uint32 ptp_offset                                               : 8;
            uint32 ptp_update_residence_time                                : 1;
            uint32 ptp_apply_egress_asymmetry_delay                         : 1;
            uint32 ptp_replace_timestamp                                    : 1;
            uint32 is_l4_ptp                                                : 1;
            uint32 is_l2_ptp                                                : 1;
            uint32 ptp_deep_parser                                          : 1;
            uint32 timestamp_4_0                                            : 5;
            uint32 timestamp61_37_9_0                                       : 10;

            uint32 timestamp61_37_24_10                                     : 15;
            uint32 rsv1                                                         : 17;
        }ptp;
    }u1;

    /*word2[31:0]*/
    uint32 u1_data                                                          : 10;  /*u1_oam_rxOam, u1_oam_rxOam{2,0,1}*/
    uint32 u2_data : 6;
    uint32 timestamp_36_5_15_0                                              : 16;

    /*word3[31:0]*/
    uint32 timestamp_36_5_31_16                                             : 16;
    uint32 local_phy_port                                                  : 8;
    uint32 u3_data_1                                                        : 8;

    /*word4[31:0]*/
    uint32 u3_data_2                                                        : 16;  /*u3_other_i2eSrcCidValid{4,7,1}
                                                                                      u3_other_from_cpu{4,8,1}
                                                                                      u3_other_destChannelIdFromCpu{4,8,6
                                                                                      u3_other_destChannelIdValidFromCpu{4,14,1}*/
    uint32 operation_type                                                   : 3;
    uint32 ttl                                                              : 8;
    uint32 logic_src_port_4_0                                               : 5;

    /*word5[31:0]*/
    uint32 logic_src_port_13_5                                              : 9;
    uint32 fid                                                              : 14;
    uint32 oam_use_fid                                                      : 1;
    uint32 mac_known                                                        : 1;
    uint32 stag_action                                                      : 2;
    uint32 iloop_from_cpu                                                   : 1;
    uint32 is_debugged_pkt                                                  : 1;
    uint32 mac_learning_en                                                  : 1;
    uint32 bridge_operation                                                 : 1;
    uint32 oam_tunnel_en                                                    : 1;

    /*word6[31:0]*/
    uint32 mux_length_type                                                  : 2;
    uint32 src_vlan_ptr                                                     : 13;
    uint32 svlan_tpid_index                                                 : 2;
    uint32 from_cpu_or_oam                                                  : 1;
    uint32 outer_vlan_is_c_vlan                                        : 1;
    uint32 ingress_edit_en                                                  : 1;
    uint32 from_cpu_lm_down_disable                                         : 1;
    uint32 ds_next_hop_bypass_all                                           : 1;
    uint32 bypass_ingress_edit                                              : 1;
    uint32 from_fabric                                                      : 1;
    uint32 source_port_extender                                             : 1;
    uint32 svlan_tag_operation_valid                                        : 1;
    uint32 from_cpu_lm_up_disable                                           : 1;
    uint32 port_mac_sa_en                                                   : 1;
    uint32 next_hop_ext                                                     : 1;
    uint32 src_stag_cfi                                                     : 1;
    uint32 is_leaf                                                          : 1;
    uint32 critical_packet                                                  : 1;

    /*word7[31:0]*/
    uint32 from_lag                                                         : 1;
    uint32 next_hop_ptr                                                     : 18;
    uint32 color                                                            : 2;
    uint32 src_svlan_id_10_0                                                : 11;

    /*word8[31:0]*/
    uint32 src_svlan_id_11_11                                               : 1;
    uint32 source_port                                                      : 16;
    uint32 logic_port_type                                                  : 1;
    uint32 header_hash                                                      : 8;
    uint32 src_stag_cos                                                     : 3;
    uint32 packet_type                                                     : 3;

    /*word9[31:0]*/
    uint32 prio                                                             : 4;
    uint32 terminate_cid_hdr                                                : 1;
    uint32 dest_map                                                         : 19;
    uint32 packet_offset                                                    : 8;
};
#else
struct ms_packet_header_d2_s
{
    /*word0[31:0] word1[31:0]*/
    union 
    {
        struct
        {
            uint32 rsv0                                                         : 13;
            uint32 neighbor_discover_packet                                 : 1;
            uint32 sgmac_strip_header                                       : 1;
            uint32 c2c_check_disable                                        : 1;
            uint32 stacking_src_port                                        : 16;
            
            uint32 rsv1                                                         : 32;
            
        }c2c;

        struct
        {
            uint32 rsv0                                                         : 24;
            uint32 dm_offset                                                : 8;
            

            uint32 rsv1                                                         : 32;
        }dmtx;

        struct
        {
            uint32 decrypt_key_index_1_0                                    : 2;
            uint32 capwap_encaped                                           : 1;
            uint32 acl_dscp_valid                                           : 1;
            uint32 src_dscp_valid                                           : 1;
            uint32 mirrored_packet_with_sci                                 : 1;
            uint32 mirrored_packet_with_sec_tag                             : 1;
            uint32 need_capwap_decrypt                                      : 1;
            uint32 need_capwap_decap                                        : 1;
            uint32 need_dot1ae_decrypt                                      : 1;
            uint32 capwap_mcast                                             : 1;
            uint32 capwap_tunnel_type                                       : 2;
            uint32 service_acl_qos_en                                       : 1;
            uint32 user_default_vlan_valid                                  : 1;
            uint32 use_default_vlan_tag                                     : 1;
            uint32 user_default_vlan_id                                     : 12;
            uint32 user_default_cos                                         : 3;
            uint32 user_default_cfi                                         : 1;

            uint32 rsv1                                                         : 16;
            uint32 capwap_last_frag                                         : 1;
            uint32 need_capwap_reassemble                                   : 1;
            uint32 reassemble_offset                                        : 8;
            uint32 decrypt_key_index_7_2                                    : 6;
        }e2iloop_in;

        struct
        {
            uint32 need_dot1ae_encrypt                                      : 1;
            uint32 capwap_encaped                                           : 1;
            uint32 acl_dscp_valid                                           : 1;
            uint32 src_dscp_valid                                           : 1;
            uint32 mirrored_packet_with_sci                                 : 1;
            uint32 mirrored_packet_with_sec_tag                             : 1;
            uint32 need_capwap_decrypt                                      : 1;
            uint32 need_capwap_decap                                        : 1;
            uint32 need_dot1ae_decrypt                                      : 1;
            uint32 capwap_mcast                                             : 1;
            uint32 capwap_tunnel_type                                       : 2;
            uint32 service_acl_qos_en                                       : 1;
            uint32 user_default_vlan_valid                                  : 1;
            uint32 use_default_vlan_tag                                     : 1;
            uint32 user_default_vlan_id                                     : 12;
            uint32 user_default_cos                                         : 3;
            uint32 user_default_cfi                                         : 1;

            uint32 rsv1                                                         : 32;
        }e2iloop_out;

        struct
        {
            uint32 rx_fcb_23_0                                              : 24;
            uint32 rxtx_fcl                                                 : 8;

            uint32 tx_fcb_23_0                                              : 24;
            uint32 rx_fcb_31_24                                             : 8;
        }lmtx;

        struct
        {
            uint32 ipsa_18_0                                                : 19;
            uint32 ipsa_valid                                               : 1;
            uint32 src_address_mode                                         : 1;
            uint32 ipv4_src_embeded_mode                                    : 1;
            uint32 pt_enable                                                : 1;
            uint32 ipsa_mode                                                : 2;
            uint32 ipsa_prefix_length                                       : 3;
            uint32 src_dscp_valid                                           : 1;
            uint32 acl_dscp_valid                                           : 1;
            uint32 micro_burst_valid                                        : 1;
            uint32 no_dot1ae_encrypt                                        : 1;
            
            uint32 l4_source_port_9_0                                       : 10;
            uint32 l4_source_port_valid                                     : 1;
            uint32 ipsa_39_19                                               : 21;
        }nat;

        struct
        {
            uint32 src_cvlan_id                                             : 12;
            uint32 isid_valid                                                 : 1;
            uint32 cvlan_tag_operation_valid                                : 1;
            uint32 microburst_valid                                         : 1;
            uint32 metadata_type                                            : 2;
            uint32 metadata                                                 : 14;
            uint32 no_dot1ae_encrypt                                        : 1;

            uint32 acl_stag_cos_valid                                       : 1;
            uint32 acl_ctag_cos_valid                                       : 1;
            uint32 acl_stag_cfi_valid                                       : 1;
            uint32 acl_ctag_cfi_valid                                       : 1;
            uint32 dot1ae_encrypted                                         : 1;
            uint32 ecmp_hash                                                : 8;
            uint32 acl_dscp_valid                                           : 1;
            uint32 src_dscp_valid                                           : 1;
            uint32 cn_action                                                : 2;
            uint32 pbb_check_discard                                        :1;
            uint32 ctag_action                                              : 2;
            uint32 src_ctag_cos                                             : 3;
            uint32 src_ctag_cfi                                             : 1;
            uint32 src_ctag_offset_type                                     : 1;
            uint32 source_port_isolateId                                : 7;
        }normal;

        struct
        {
            uint32 rx_fcb_23_0                                              : 24;
            uint32 rxtx_fcl                                                 : 8;
            
            uint32 mep_index                                                : 13;
            uint32 oam_type                                                 : 4;
            uint32 link_oam                                                 : 1;
            uint32 iloop_to_agg_up_mep                                      : 1;
            uint32 gal_exist                                                : 1;
            uint32 use_oam_ttl                                              : 1;
            uint32 is_up                                                    : 1;
            uint32 dm_en                                                    : 1;
            uint32 lm_received_packet                                       : 1;
            uint32 rx_fcb_31_24                                             : 8;
        }oam;

        struct
        {
            uint32 timestamp61_37_9_0                                       : 10;
            uint32 timestamp_4_0                                            : 5;
            uint32 ptp_deep_parser                                          : 1;
            uint32 is_l2_ptp                                                : 1;
            uint32 is_l4_ptp                                                : 1;
            uint32 ptp_replace_timestamp                                    : 1;
            uint32 ptp_apply_egress_asymmetry_delay                         : 1;
            uint32 ptp_update_residence_time                                : 1;
            uint32 ptp_offset                                               : 8;
            uint32 acl_dscp_valid                                           : 1;
            uint32 src_dscp_valid                                           : 1;
            uint32 is_cflex_update_residence_time                           : 1;

            uint32 rsv1                                                         : 17;
            uint32 timestamp61_37_24_10                                     : 15;
        }ptp;
    }u1;

    /*word2[31:0]*/
    uint32 timestamp_36_5_15_0                                              : 16;
    uint32 u2_data                                                          : 6;
    uint32 u1_data                                                          : 10;  /*u1_oam_rxOam, u1_oam_rxOam{2,0,1}*/

    /*word3[31:0]*/
    uint32 u3_data_1                                                        : 8;
    uint32 local_phy_port                                                  : 8;
    uint32 timestamp_36_5_31_16                                            : 16;

    /*word4[31:0]*/
    uint32 logic_src_port_4_0                                               : 5;
    uint32 ttl                                                              : 8;
    uint32 operation_type                                                   : 3;
    uint32 u3_data_2                                                        : 16;  /*u3_other_i2eSrcCidValid{4,7,1}
                                                                                      u3_other_from_cpu{4,7,1}
                                                                                      u3_other_destChannelIdFromCpu{4,9,6
                                                                                      u3_other_destChannelIdValidFromCpu{4,14,1}*/
    /*word5[31:0]*/
    uint32 oam_tunnel_en                                                    : 1;
    uint32 bridge_operation                                                 : 1;
    uint32 mac_learning_en                                                  : 1;
    uint32 is_debugged_pkt                                                  : 1;
    uint32 iloop_from_cpu                                                   : 1;
    uint32 stag_action                                                      : 2;
    uint32 mac_known                                                        : 1;
    uint32 oam_use_fid                                                      : 1;
    uint32 fid                                                              : 14;
    uint32 logic_src_port_13_5                                              : 9;

    /*word6[31:0]*/
    uint32 critical_packet                                                  : 1;
    uint32 is_leaf                                                          : 1;
    uint32 src_stag_cfi                                                     : 1;
    uint32 next_hop_ext                                                     : 1;
    uint32 port_mac_sa_en                                                   : 1;
    uint32 from_cpu_lm_up_disable                                           : 1;
    uint32 svlan_tag_operation_valid                                        : 1;
    uint32 source_port_extender                                             : 1;
    uint32 from_fabric                                                      : 1;
    uint32 bypass_ingress_edit                                              : 1;
    uint32 ds_next_hop_bypass_all                                           : 1;
    uint32 from_cpu_lm_down_disable                                         : 1;
    uint32 ingress_edit_en                                                  : 1;
    uint32 outer_vlan_is_c_vlan                                        : 1;
    uint32 from_cpu_or_oam                                                  : 1;
    uint32 svlan_tpid_index                                                 : 2;
    uint32 src_vlan_ptr                                                     : 13;
    uint32 mux_length_type                                                  : 2;

    /*word7[31:0]*/
    uint32 src_svlan_id_10_0                                                : 11;
    uint32 color                                                            : 2;
    uint32 next_hop_ptr                                                     : 18;
    uint32 from_lag                                                         : 1;

    /*word8[31:0]*/
    uint32 packet_type                                                     : 3;
    uint32 src_stag_cos                                                     : 3;
    uint32 header_hash                                                      : 8;
    uint32 logic_port_type                                                  : 1;
    uint32 source_port                                                      : 16;
    uint32 src_svlan_id_11_11                                               : 1;

    /*word9[31:0]*/
    uint32 packet_offset                                                    : 8;
    uint32 dest_map                                                         : 19;
    uint32 terminate_cid_hdr                                                : 1;
    uint32 prio                                                             : 4;
};
#endif

typedef struct ms_packet_header_d2_s  ms_packet_header_d2_t;

#ifdef __cplusplus
}
#endif

#endif

