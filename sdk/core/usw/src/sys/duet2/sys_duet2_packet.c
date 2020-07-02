/**
 @file sys_duet2_struct.c

 @author  Copyright (C) 2020 Centec Networks Inc.  All rights reserved.

 @date 2020-03-17

 @version v1.0

*/

/****************************************************************************
 *
* Header Files
*
****************************************************************************/
#include "sal.h"
#include "ctc_error.h"
#include "ctc_debug.h"
#include "ctc_crc.h"
#include "ctc_packet.h"

#include "sys_usw_common.h"
#include "sys_usw_register.h"
#include "sys_usw_chip.h"
#include "sys_usw_port.h"
#include "sys_usw_nexthop_api.h"
#include "sys_usw_packet.h"
#include "../sys_usw_packet_priv.h"
#include "drv_api.h"

/****************************************************************************
 *
* Global and Declaration
*
*****************************************************************************/
extern sys_pkt_master_t* p_usw_pkt_master[];

#define SYS_PKT_U3_OTHER_I2E_SRC_CID_VALID 7
#define SYS_PKT_U3_OTHER_FROM_CPU 8
#define SYS_PKT_U1_OAM_RX_OAM 0
#define SYS_PKT_U2_OAM_MIP_EN 5
#define SYS_PKT_u3_OTHER_DST_CHAN_ID_FROM_CPU 9
#define SYS_PKT_U3_OTHER_DST_CHAN_ID_VALID_RROM_CPU 15
/****************************************************************************
 *
* Functions
*
****************************************************************************/
extern int32
_sys_usw_packet_get_svlan_tpid_index(uint8 lchip, uint16 svlan_tpid, uint8* svlan_tpid_index, sys_pkt_tx_hdr_info_t* p_tx_hdr_info);
extern int32
_sys_usw_packet_ts_to_bit62(uint8 lchip, uint32 ns_only_format, ctc_packet_ts_t* p_ts, uint64* p_ts_61_0);
extern int32
_sys_usw_packet_bit62_to_ts(uint8 lchip, uint32 ns_only_format, uint64 ts_61_0, ctc_packet_ts_t* p_ts);
extern int32
_sys_usw_packet_recover_timestamp(uint8 lchip, uint32 ts_36_5, uint64* p_ts_61_0);

int32
sys_duet2_packet_txinfo_to_rawhdr(uint8 lchip, ctc_pkt_info_t* p_tx_info, uint32* p_raw_hdr_net, uint8 mode, uint8* data)
{
    uint32 bheader[SYS_USW_PKT_HEADER_LEN/4] = {0};
    ms_packet_header_d2_t* p_raw_hdr              = (ms_packet_header_d2_t*)bheader;
    uint32 dest_map                = 0;
    uint16 vlan_ptr                = 0;
    uint8 gchip                    = 0;
    uint32 lport                   = 0;
    uint8 hash                     = 0;
    uint8 src_gchip                = 0;
    uint8 packet_type              = 0;
    uint8 priority                 = 0;
    uint8 color                    = 0;
    uint32 src_port                = 0;
    uint32 next_hop_ptr            = 0;
    uint32 ts_36_5 = 0;
    uint32 ts_4_0 = 0;
    uint32 ts_61_37 = 0;
    uint64 ts_61_0 = 0;
    uint32 offset_ns = 0;
    uint32 offset_s = 0;
    uint64 offset = 0;
    uint8  svlan_tpid_index = 0;
    uint8  chan_id = 0xFF;
    sys_nh_info_dsnh_t      sys_nh_info;
    uint32 gport = 0;
    drv_work_platform_type_t platform_type = MAX_WORK_PLATFORM;
    bool* p_protect_en = NULL;
    bool protect_en = FALSE;
    uint32 mep_index = 0;
    uint8 sub_queue_id = 0;

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);

    sal_memset(p_raw_hdr, 0, SYS_USW_PKT_HEADER_LEN);
    if (p_tx_info->priority >= 16)
    {
        return CTC_E_INVALID_PARAM;
    }

    drv_get_platform_type(lchip, &platform_type);
    /* Must be inited */
    p_raw_hdr->packet_type = p_tx_info->packet_type;

    /* Support offload data tunnel cryption in hw */
    p_raw_hdr->packet_offset = p_tx_info->payload_offset;

        p_raw_hdr->from_cpu_or_oam = 1;
    p_raw_hdr->operation_type = p_tx_info->oper_type;
    p_raw_hdr->critical_packet = p_tx_info->is_critical;
    p_raw_hdr->src_stag_cos = p_tx_info->src_cos;
    p_raw_hdr->ttl = p_tx_info->ttl;
    p_raw_hdr->mac_known = TRUE;
    p_raw_hdr->fid = p_tx_info->fid;
    CTC_ERROR_RETURN(_sys_usw_packet_get_svlan_tpid_index(lchip, p_tx_info->svlan_tpid, &svlan_tpid_index, &p_usw_pkt_master[lchip]->tx_hdr_info));
    p_raw_hdr->svlan_tpid_index = svlan_tpid_index;

    p_raw_hdr->u3_data_2 |= (1<<SYS_PKT_U3_OTHER_FROM_CPU); /*u3_other_fromCpu{4,8,1}*/
    p_raw_hdr->logic_src_port_13_5 = (p_tx_info->logic_src_port>>5)&0x1FF;
    p_raw_hdr->logic_src_port_4_0 = p_tx_info->logic_src_port&0x1F;
    p_raw_hdr->logic_port_type = p_tx_info->logic_port_type;

    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_DIAG_PKT))
    {
        p_raw_hdr->is_debugged_pkt = 1;
    }

    /*don't eccrypt specified pkt*/
    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_CANCEL_DOT1AE))
    {
        p_raw_hdr->u1.normal.no_dot1ae_encrypt = TRUE;
    }

    /*Set oamuse fid lookup */
    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_OAM_USE_FID))
    {
        p_raw_hdr->oam_use_fid = TRUE;
    }

   /*Set Vlan domain */
    if (p_tx_info->vlan_domain == CTC_PORT_VLAN_DOMAIN_SVLAN)
    {
        p_raw_hdr->outer_vlan_is_c_vlan = FALSE;
        vlan_ptr = p_usw_pkt_master[lchip]->tx_hdr_info.vlanptr[p_tx_info->src_svid];
        p_raw_hdr->src_vlan_ptr = vlan_ptr;
    }
    else  if (p_tx_info->vlan_domain == CTC_PORT_VLAN_DOMAIN_CVLAN)
    {
        p_raw_hdr->outer_vlan_is_c_vlan = TRUE;
        vlan_ptr = p_usw_pkt_master[lchip]->tx_hdr_info.vlanptr[p_tx_info->src_cvid];
        p_raw_hdr->src_vlan_ptr = vlan_ptr;
    }
    else
    {
        return CTC_E_INVALID_PARAM;
    }

    /* Next_hop_ptr and DestMap*/
    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_BPE_TRANSPARENT))
    {
        /* Bypass NH is 8W  and egress edit mode*/
        p_raw_hdr->bypass_ingress_edit = TRUE;
        p_raw_hdr->next_hop_ext = TRUE;
        next_hop_ptr = p_usw_pkt_master[lchip]->tx_hdr_info.rsv_nh[SYS_NH_RES_OFFSET_TYPE_BPE_TRANSPARENT_NH];
    }
    else if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_NH_OFFSET_BYPASS))
    {
        /* Bypass NH is 8W  and egress edit mode*/
        p_raw_hdr->bypass_ingress_edit = TRUE;
        p_raw_hdr->next_hop_ext = TRUE;
        next_hop_ptr = p_usw_pkt_master[lchip]->tx_hdr_info.rsv_nh[SYS_NH_RES_OFFSET_TYPE_BYPASS_NH];
    }
    else
    {
        if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_NH_OFFSET_VALID))
        {
            next_hop_ptr = p_tx_info->nh_offset;
            if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_NH_OFFSET_IS_8W))
            {
                p_raw_hdr->next_hop_ext = TRUE;
            }
        }
        else
        {
            /* GG Bridge NH is 4W */
            p_raw_hdr->next_hop_ext = FALSE;
            next_hop_ptr = p_usw_pkt_master[lchip]->tx_hdr_info.rsv_nh[SYS_NH_RES_OFFSET_TYPE_BRIDGE_NH];
        }
    }

    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_MCAST))
    {
        dest_map = SYS_ENCODE_MCAST_IPE_DESTMAP(p_tx_info->dest_group_id);
        next_hop_ptr = 0;
    }
    else
    {
        if (CTC_IS_LINKAGG_PORT(p_tx_info->dest_gport))
        {
            dest_map = SYS_ENCODE_DESTMAP(CTC_LINKAGG_CHIPID, p_tx_info->dest_gport&0xFF);
        }
        else
        {
            gchip = CTC_MAP_GPORT_TO_GCHIP(p_tx_info->dest_gport);
            if (CTC_IS_CPU_PORT(p_tx_info->dest_gport))
            {
                sub_queue_id = p_usw_pkt_master[lchip]->tx_hdr_info.fwd_cpu_sub_queue_id;
                dest_map = SYS_ENCODE_EXCP_DESTMAP(gchip, sub_queue_id);
            }
            else
            {
                dest_map = SYS_ENCODE_DESTMAP(gchip, SYS_MAP_CTC_GPORT_TO_DRV_LPORT(p_tx_info->dest_gport));
            }
        }
    }

    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_NHID_VALID))
    {
        sal_memset(&sys_nh_info, 0, sizeof(sys_nh_info_dsnh_t));
        if(CTC_E_NONE == sys_usw_nh_get_nhinfo(lchip, p_tx_info->nhid, &sys_nh_info, 1))
        {
            next_hop_ptr = sys_nh_info.dsnh_offset;
            p_raw_hdr->next_hop_ext = sys_nh_info.nexthop_ext;
            dest_map = sys_nh_info.dest_map;
            if(sys_nh_info.aps_en)
            {
                if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_APS_PATH_EN))
                {
                    mep_index = p_tx_info->aps_p_path ? 0x3 : 0x1;
                    protect_en = p_tx_info->aps_p_path;
                    p_protect_en = &protect_en;

                    if (!sys_nh_info.mep_on_tunnel)
                    {
                        mep_index = 0;
                    }
                }

                CTC_ERROR_RETURN(sys_usw_nh_get_aps_working_path(lchip, p_tx_info->nhid, &gport, &next_hop_ptr, p_protect_en));
                dest_map = SYS_ENCODE_DESTMAP(SYS_MAP_CTC_GPORT_TO_GCHIP(gport), SYS_MAP_CTC_GPORT_TO_DRV_LPORT(gport));
            }
        }

        if (sys_nh_info.dsnh_offset & SYS_NH_DSNH_BY_PASS_FLAG)
        {
            next_hop_ptr = sys_nh_info.dsnh_offset & (~SYS_NH_DSNH_BY_PASS_FLAG);
            p_raw_hdr->bypass_ingress_edit = 1;
        }
    }

    p_raw_hdr->next_hop_ptr = next_hop_ptr;
    p_tx_info->nh_offset = next_hop_ptr;

    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_ASSIGN_DEST_PORT))
    {
        uint8 gchip = 0;

        sys_usw_get_gchip_id(lchip, &gchip);
        chan_id = SYS_GET_CHANNEL_ID(lchip, CTC_MAP_LPORT_TO_GPORT(gchip, p_tx_info->lport));
        if(0xff != chan_id)
        {
             p_raw_hdr->u3_data_2 |= (chan_id<<SYS_PKT_u3_OTHER_DST_CHAN_ID_FROM_CPU);   /* u3_other_destChannelIdFromCpu, 6, {4,9,6}*/
             p_raw_hdr->u3_data_2 |= 1<<SYS_PKT_U3_OTHER_DST_CHAN_ID_VALID_RROM_CPU; /* u3_other_destChannelIdValidFromCpu, 1, {4,15,1}*/
        }
    }

    /* SrcPort */
    sys_usw_get_gchip_id(lchip, &src_gchip);
    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_SRC_PORT_VALID))
    {
        src_port = p_tx_info->src_port;
        if(CTC_IS_LINKAGG_PORT(p_tx_info->src_port))
        {
            p_raw_hdr->from_lag = 1;
            src_port = CTC_MAP_LPORT_TO_GPORT(src_gchip, CTC_MAP_GPORT_TO_LPORT(src_port));
        }
    }
    else
    {
        src_port = CTC_MAP_LPORT_TO_GPORT(src_gchip, SYS_RSV_PORT_OAM_CPU_ID);
    }
    src_port = SYS_MAP_CTC_GPORT_TO_DRV_GPORT(src_port);
    p_raw_hdr->source_port = src_port;

    /* Svid */
    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_SRC_SVID_VALID))
    {
        p_raw_hdr->src_svlan_id_10_0 = p_tx_info->src_svid&0x7FF;
        p_raw_hdr->src_svlan_id_11_11 = (p_tx_info->src_svid>>11)&0x1;

        p_raw_hdr->stag_action = CTC_VLAN_TAG_OP_ADD;
        p_raw_hdr->svlan_tag_operation_valid = TRUE;
    }

    /* Cvid */
    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_SRC_CVID_VALID))
    {
        p_raw_hdr->u1.normal.src_cvlan_id = p_tx_info->src_cvid;
        p_raw_hdr->u1.normal.ctag_action = CTC_VLAN_TAG_OP_ADD;
        p_raw_hdr->u1.normal.cvlan_tag_operation_valid = TRUE;
    }
    /* isolated_id*/
    if (0 != p_tx_info->isolated_id)
    {
        p_raw_hdr->u1.normal.source_port_isolateId = p_tx_info->isolated_id;
    }
    /* Linkagg Hash*/
    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_HASH_VALID))
    {
        hash = p_tx_info->hash;
    }
    else
    {
        hash = ctc_crc_calculate_crc8(data, 12, 0);
    }
    p_raw_hdr->header_hash = hash;

    /* PHB priority + color */
    priority = (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_PRIORITY)) ? p_tx_info->priority : 15;
    color = (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_COLOR)) ? p_tx_info->color : CTC_QOS_COLOR_GREEN;
    p_raw_hdr->color = color;

    /* Iloop */
    if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_INGRESS_MODE))
    {
        gchip = CTC_MAP_GPORT_TO_GCHIP(p_tx_info->dest_gport);
        lport = SYS_MAP_CTC_GPORT_TO_DRV_LPORT(p_tx_info->dest_gport);
        /* dest_map is ILOOP */
        dest_map = SYS_ENCODE_DESTMAP(gchip, SYS_RSV_PORT_ILOOP_ID);
        dest_map = dest_map | (lport&0x100); /*sliceId*/
        p_raw_hdr->dest_map = dest_map;

        /* nexthop_ptr is lport */
        next_hop_ptr = SYS_PACKET_ENCAP_LOOP_NHPTR(lport, 0);
        p_raw_hdr->next_hop_ptr = next_hop_ptr;
    }

    /* OAM Operation*/
    if (CTC_PKT_OPER_OAM == p_tx_info->oper_type)
    {
        p_raw_hdr->u1.oam.mep_index = mep_index;

        p_raw_hdr->u1.oam.oam_type = p_tx_info->oam.type;
        CTC_BIT_UNSET(p_raw_hdr->u1_data, SYS_PKT_U1_OAM_RX_OAM); /*u1_oam_rxOam, u1_oam_rxOam, 1, {2,0,1}*/

        if (CTC_FLAG_ISSET(p_tx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_DM))
        {
            p_raw_hdr->u1.oam.dm_en = TRUE;
            p_raw_hdr->u1.dmtx.dm_offset = p_tx_info->oam.dm_ts_offset;
        }
        else
        {
            p_raw_hdr->u1.oam.dm_en = FALSE;
        }

        if (CTC_FLAG_ISSET(p_tx_info->oam.flags, CTC_PKT_OAM_FLAG_LINK_OAM))
        {
            p_raw_hdr->u1.oam.link_oam = TRUE;
        }

        /* TPOAM Y.1731 Section need not append 13 label */
        if (CTC_OAM_TYPE_ACH == p_tx_info->oam.type)
        {
             p_raw_hdr->u2_data |= (1<<SYS_PKT_U2_OAM_MIP_EN);   /*u2_oam_mipEn, u2_oam_mipEn, 1, {2,15,1}*/
            /* only ACH encapsulation need to change packet_type */
            if (CTC_FLAG_ISSET(p_tx_info->oam.flags, CTC_PKT_OAM_FLAG_HAS_MPLS_GAL))
            {
                p_raw_hdr->u1.oam.gal_exist = TRUE;
                packet_type = CTC_PARSER_PKT_TYPE_MPLS;
            }
            else
            {
                packet_type = CTC_PARSER_PKT_TYPE_RESERVED;
            }
            p_raw_hdr->packet_type = packet_type;
        }

        /* set for UP MEP */
        if (CTC_FLAG_ISSET(p_tx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_UP_MEP))
        {
            p_raw_hdr->u1.oam.is_up = TRUE;
            gchip = CTC_MAP_GPORT_TO_GCHIP(p_tx_info->dest_gport);
            lport = SYS_MAP_CTC_GPORT_TO_DRV_LPORT(p_tx_info->dest_gport);
            /* dest_map is ILOOP */
            dest_map = SYS_ENCODE_DESTMAP(gchip, SYS_RSV_PORT_ILOOP_ID);
            dest_map = dest_map | (lport&0x100);/*SliceId*/
            p_raw_hdr->dest_map = dest_map;

            next_hop_ptr = SYS_PACKET_ENCAP_LOOP_NHPTR(lport, (p_tx_info->ttl != 0));
            p_raw_hdr->next_hop_ptr = next_hop_ptr;

            /* src vlan_ptr is vlan_ptr by vid */
            vlan_ptr = p_usw_pkt_master[lchip]->tx_hdr_info.vlanptr[p_tx_info->oam.vid];
            p_raw_hdr->src_vlan_ptr = vlan_ptr;

        }

        /* set bypass MIP port */
        if (CTC_FLAG_ISSET(p_tx_info->oam.flags, CTC_PKT_OAM_FLAG_TX_MIP_TUNNEL))
        {
            /* bypass OAM packet to MIP configured port; otherwise, packet will send to CPU again */
            p_raw_hdr->u1.oam.oam_type = CTC_OAM_TYPE_NONE;
            p_raw_hdr->oam_tunnel_en = TRUE;
        }
    }
    else if (CTC_PKT_OPER_PTP == p_tx_info->oper_type)
    {
        /* 7. get PTP */
        /*  DM Timestamp Bits Mapping
            Timestamp               Field Name          BIT Width   BIT Base
            dmTimestamp[36:5]       timestamp_36_5           32          5
            dmTimestamp[4:0]      u1_ptp_timestamp_4_0  5          0
            dmTimestamp[61:37]      u1_ptp_timestamp_61_37  25          37
         */
        _sys_usw_packet_ts_to_bit62(lchip, TRUE, &p_tx_info->ptp.ts, &ts_61_0);

        if ((CTC_PTP_CORRECTION == p_tx_info->ptp.oper) && (!(CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_PTP_TS_PRECISION))))
        {
            offset_ns = p_usw_pkt_master[lchip]->tx_hdr_info.offset_ns;
            offset_s = p_usw_pkt_master[lchip]->tx_hdr_info.offset_s;
            offset = offset_s * 1000000000ULL + offset_ns;
            if (ts_61_0 >= offset)
            {
                ts_61_0 = ts_61_0 -offset;
            }
            else
            {
                return CTC_E_INVALID_PARAM;
            }
        }

        ts_4_0 = ts_61_0 & 0x1FULL;
        ts_36_5 = (ts_61_0 >> 5) & 0xFFFFFFFFULL;
        ts_61_37 = (ts_61_0 >> 37) & 0x01FFFFFFULL;
        p_raw_hdr->timestamp_36_5_15_0 = ts_36_5&0xFFFF;
        p_raw_hdr->timestamp_36_5_31_16 = (ts_36_5>>16)&0x7FFF;
        p_raw_hdr->u1.ptp.timestamp_4_0 = ts_4_0;
        p_raw_hdr->u1.ptp.timestamp61_37_9_0 = ts_61_37&0x3FF;
        p_raw_hdr->u1.ptp.timestamp61_37_24_10 = (ts_61_37>>10)&0x7FFF;

        switch (p_tx_info->ptp.oper)
        {
            case CTC_PTP_CAPTURE_ONLY:
            {
                /* Duet2 do not support  CTC_PTP_CAPTURE_ONLY */
                return CTC_E_INVALID_PARAM;
            }
            case CTC_PTP_REPLACE_ONLY:
            {
                p_raw_hdr->u1.ptp.ptp_replace_timestamp = TRUE;
                p_raw_hdr->u1.ptp.ptp_offset = p_tx_info->ptp.ts_offset;
                break;
            }
            case CTC_PTP_NULL_OPERATION:
            {
                /* do nothing */
                break;
            }
            case CTC_PTP_CORRECTION:
            {
                /*SetMsPacketHeader(V, u1_ptp_ptpReplaceTimestamp_f, p_raw_hdr, TRUE);*/
                p_raw_hdr->u1.ptp.ptp_update_residence_time = TRUE;
                p_raw_hdr->u1.ptp.ptp_offset = p_tx_info->ptp.ts_offset;
                break;
            }
            default:
            {
                return CTC_E_INVALID_PARAM;
            }
        }
    }
    else if (CTC_PKT_OPER_C2C == p_tx_info->oper_type)
    {
        uint8  sub_queue_id = 0;
        uint8  grp_id       = 0;
        p_raw_hdr->logic_src_port_13_5 = 0; /*logic port mean c2c extend group id*/
        p_raw_hdr->logic_src_port_4_0 = 0;
        p_raw_hdr->bypass_ingress_edit = 1;
        if (CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_MCAST))
        {
            p_raw_hdr->u1.c2c.c2c_check_disable = 1;
        }

        if (!CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_MCAST) &&
            !CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_INGRESS_MODE))
        {
            if (!sys_usw_chip_is_local(lchip, SYS_DECODE_DESTMAP_GCHIP(dest_map)) ||
                 CTC_FLAG_ISSET(p_tx_info->flags, CTC_PKT_FLAG_ASSIGN_DEST_PORT))
            {
                sub_queue_id = p_usw_pkt_master[lchip]->tx_hdr_info.c2c_sub_queue_id;
                grp_id = (sub_queue_id / MCHIP_CAP(SYS_CAP_REASON_GRP_QUEUE_NUM)) + (priority/8);
                /*to cpu based on prio*/
                dest_map = SYS_ENCODE_EXCP_DESTMAP_GRP(gchip, grp_id);
                dest_map |= 0x10;
            }
            else
            {
                p_raw_hdr->u1.c2c.neighbor_discover_packet = 1;
                p_raw_hdr->next_hop_ptr = CTC_PKT_CPU_REASON_BUILD_NHPTR(CTC_PKT_CPU_REASON_C2C_PKT, 0);
                p_raw_hdr->bypass_ingress_edit = TRUE;
            }
        }
    }

    p_raw_hdr->prio = priority;
    p_raw_hdr->dest_map = dest_map;

    if(CTC_PKT_MODE_ETH == mode)
    {
        /*6bytes MACDA + 6 bytes MACSA + 4 bytes VLAN Tag + 2 bytes EtherType + 40 bytes bridge header*/
        p_raw_hdr->packet_offset = 58;
    }

    if (platform_type == HW_PLATFORM)
    {
        CTC_ERROR_RETURN(sys_usw_dword_reverse_copy(p_raw_hdr_net, (uint32*)p_raw_hdr, SYS_USW_PKT_HEADER_LEN/4));
        CTC_ERROR_RETURN(sys_usw_swap32((uint32*)p_raw_hdr_net, SYS_USW_PKT_HEADER_LEN / 4, FALSE))
    }
    else
    {
        CTC_ERROR_RETURN(sys_usw_byte_reverse_copy((uint8*)p_raw_hdr_net, (uint8*)p_raw_hdr, SYS_USW_PKT_HEADER_LEN));
    }

    return CTC_E_NONE;
}

int32
sys_duet2_packet_rawhdr_to_rxinfo(uint8 lchip, uint32* p_raw_hdr_net, ctc_pkt_rx_t* p_pkt_rx)
{
    uint32 bheader[SYS_USW_PKT_HEADER_LEN/4] = {0};
    ms_packet_header_d2_t* p_raw_hdr = (ms_packet_header_d2_t*)bheader;
    uint32 dest_map                = 0;
    uint32 ts_36_5 = 0;
    uint32 ts_4_0 = 0;
    uint32 ts_61_37 = 0;
    uint64 ts_61_0 = 0;
    uint32 src_port                = 0;
    uint32 next_hop_ptr            = 0;
    ctc_pkt_info_t* p_rx_info      = NULL;
    uint32 value = 0;
    uint8 gchip_id = 0;
    uint32 offset_ns = 0;
    uint32 offset_s = 0;
    uint32 cmd = 0;
    uint16 vlan_ptr = 0;
    uint16 lport = 0;
    TsEngineOffsetAdj_m ptp_offset_adjust;

    p_rx_info = &p_pkt_rx->rx_info;

    CTC_ERROR_RETURN(sys_usw_dword_reverse_copy((uint32*)bheader, (uint32*)p_raw_hdr_net, SYS_USW_PKT_HEADER_LEN/4));
    CTC_ERROR_RETURN(sys_usw_swap32((uint32*)p_raw_hdr, SYS_USW_PKT_HEADER_LEN / 4, FALSE));

    sal_memset(p_rx_info, 0, sizeof(ctc_pkt_info_t));

    /* Must be inited */
    p_rx_info->packet_type    = p_raw_hdr->packet_type;
    p_rx_info->oper_type      = p_raw_hdr->operation_type;
    p_rx_info->priority       = p_raw_hdr->prio;
    p_rx_info->color          = p_raw_hdr->color;
    p_rx_info->src_cos        = p_raw_hdr->src_stag_cos;
    p_rx_info->fid          = p_raw_hdr->fid;
    p_rx_info->payload_offset = p_raw_hdr->packet_offset;
    p_rx_info->ttl            = p_raw_hdr->ttl;
    p_rx_info->is_critical    = p_raw_hdr->critical_packet;
    p_rx_info->logic_src_port  = (p_raw_hdr->logic_src_port_13_5<<5)|p_raw_hdr->logic_src_port_4_0;
    p_rx_info->lport            = p_raw_hdr->local_phy_port;

    if (!p_raw_hdr->mac_known)
    {
        CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_UNKOWN_MACDA);
    }

    if (CTC_PKT_OPER_NORMAL == p_rx_info->oper_type)
    {
        if (p_raw_hdr->u1.normal.metadata_type == 0x01)
        {
            /*meta*/
            p_rx_info->meta_data= p_raw_hdr->u1.normal.metadata;
            CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_METADATA_VALID);
        }
        else if (p_raw_hdr->u1.normal.metadata_type == 0)
        {
            /*vrfid*/
            p_rx_info->vrfid= p_raw_hdr->u1.normal.metadata;
            CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_VRFID_VALID);
        }
    }
    src_port = p_raw_hdr->source_port;
    p_rx_info->src_chip = SYS_DRV_GPORT_TO_GCHIP(src_port);
    if (p_raw_hdr->from_lag)
    {
        p_rx_info->src_port = SYS_MAP_DRV_LPORT_TO_CTC_GPORT(CTC_LINKAGG_CHIPID, SYS_DRV_GPORT_TO_LPORT(src_port));
    }
    else
    {
        p_rx_info->src_port = SYS_MAP_DRV_GPORT_TO_CTC_GPORT(src_port);
    }

    gchip_id = p_rx_info->src_chip;
    if(sys_usw_chip_is_local(lchip, gchip_id) && !CTC_IS_LINKAGG_PORT(p_rx_info->src_port))
    {
        sys_usw_port_get_local_phy_port(lchip, p_rx_info->src_port, &value);
        if (SYS_INVALID_LOCAL_PHY_PORT != value)
        {
            p_rx_info->src_port = CTC_MAP_LPORT_TO_GPORT(p_rx_info->src_chip, value);
        }
    }
    if (p_rx_info->lport >= MCHIP_CAP(SYS_CAP_SPEC_MAX_PHY_PORT_NUM))
    {
        CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_INTERNAl_PORT);
    }

    /* RX Reason from next_hop_ptr */
    next_hop_ptr = p_raw_hdr->next_hop_ptr;
    p_rx_info->reason = CTC_PKT_CPU_REASON_GET_BY_NHPTR(next_hop_ptr);
    if ((p_rx_info->reason == CTC_PKT_CPU_REASON_DIAG_DISCARD_PKT)
        && (next_hop_ptr & 0x1))
    {
        /* EPE Discard */
        cmd = DRV_IOR(EpeHeaderEditCtl_t, EpeHeaderEditCtl_dropPktLogWithDiscardType_f);
        DRV_IOCTL(lchip, 0, cmd, &value);
        if (value)
        {
            /*ttl means discard type, set bit6 means EPE discard type*/
            p_rx_info->ttl |= 0x40;
        }
    }
    /*EFM src port*/
    if (SYS_DRV_GPORT_TO_LPORT(src_port) >= MCHIP_CAP(SYS_CAP_SPEC_MAX_PHY_PORT_NUM)
        && (sys_usw_chip_is_local(lchip, gchip_id) && !CTC_IS_LINKAGG_PORT(p_rx_info->src_port)))
    {
        lport = SYS_DRV_GPORT_TO_LPORT(src_port);
        sys_usw_port_lport_convert(lchip, SYS_DRV_GPORT_TO_LPORT(src_port), &lport);
        p_rx_info->src_port = CTC_MAP_LPORT_TO_GPORT(gchip_id, lport);
    }
    if (p_raw_hdr->operation_type == 2)
    {
        uint32 cmd = 0;
        uint32 decap_nh = 0;
        uint32 ttl_limit = 0;

        cmd = DRV_IOR(IpeFwdCtl_t, IpeFwdCtl_capwapDecapNextHopPtr_f);
        DRV_IOCTL(lchip, 0, cmd, &decap_nh);
        cmd = DRV_IOR(IpeTunnelDecapCtl_t, IpeTunnelDecapCtl_ipTtlLimit_f);
        DRV_IOCTL(lchip, 0, cmd, &ttl_limit);

        if ((next_hop_ptr == decap_nh) && (p_rx_info->ttl < ttl_limit))
        {
            /*cpu reason overwrite, need recover*/
            p_rx_info->reason = CTC_PKT_CPU_REASON_IP_TTL_CHECK_FAIL;
        }
    }

    /* SubQueId */
    dest_map = p_raw_hdr->dest_map;
    p_rx_info->dest_gport = dest_map & 0xFF;

    /* Source svlan */
    p_rx_info->src_svid = (p_raw_hdr->src_svlan_id_11_11<<11) | p_raw_hdr->src_svlan_id_10_0 ;
    vlan_ptr = p_raw_hdr->src_vlan_ptr;
    if((p_rx_info->src_svid == 0) && (vlan_ptr < 4096))
    {
        p_rx_info->src_svid = vlan_ptr;
    }

    if (p_rx_info->src_svid)
    {
        CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_SRC_SVID_VALID);
    }

    /* Source cvlan */
    if (CTC_PKT_OPER_NORMAL == p_rx_info->oper_type)
    {
        p_rx_info->src_cvid = p_raw_hdr->u1.normal.src_cvlan_id;
        if (p_rx_info->src_cvid)
        {
            CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_SRC_CVID_VALID);
        }
        CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_ENCRYPTED);
    }

    if (p_raw_hdr->svlan_tag_operation_valid)
    {
        switch (p_raw_hdr->stag_action)
        {
            case SYS_PKT_CTAG_ACTION_MODIFY:
                p_rx_info->stag_op = CTC_VLAN_TAG_OP_REP;
                break;
            case SYS_PKT_CTAG_ACTION_ADD:
                p_rx_info->stag_op = CTC_VLAN_TAG_OP_ADD;
                break;
            case SYS_PKT_CTAG_ACTION_DELETE:
                p_rx_info->stag_op = CTC_VLAN_TAG_OP_DEL;
                break;
        }
    }

    if (CTC_PKT_OPER_NORMAL == p_rx_info->oper_type)
    {
        if (p_raw_hdr->u1.normal.cvlan_tag_operation_valid)
        {
            switch (p_raw_hdr->u1.normal.ctag_action)
            {
                case SYS_PKT_CTAG_ACTION_MODIFY:
                    p_rx_info->ctag_op = CTC_VLAN_TAG_OP_REP;
                    break;
                case SYS_PKT_CTAG_ACTION_ADD:
                    p_rx_info->ctag_op = CTC_VLAN_TAG_OP_ADD;
                    break;
                case SYS_PKT_CTAG_ACTION_DELETE:
                    p_rx_info->ctag_op = CTC_VLAN_TAG_OP_DEL;
                    break;
            }
        }
    }
            /*u3_other_i2eSrcCidValid, 1, {4,7,1} */
            /*u3_other_i2eSrcCid, 8, {4,0,7}, {3,31,1}*/
     if (CTC_IS_BIT_SET(p_raw_hdr->u3_data_2,SYS_PKT_U3_OTHER_I2E_SRC_CID_VALID)
         && (CTC_PKT_OPER_OAM != p_rx_info->oper_type) && (1 != p_rx_info->oper_type))
     {
         p_rx_info->cid = (((p_raw_hdr->u3_data_2&0x7F)<<1) | ((p_raw_hdr->u3_data_1>>7)&0x1));
     }

    /* Timestamp */
    ts_36_5 = (p_raw_hdr->timestamp_36_5_31_16<<16) | p_raw_hdr->timestamp_36_5_15_0 ;

    if (CTC_PKT_CPU_REASON_PTP == p_rx_info->reason)
    {
        ts_4_0 = p_raw_hdr->u1.ptp.timestamp_4_0;
        ts_61_37 = (p_raw_hdr->u1.ptp.timestamp61_37_24_10<<10) | p_raw_hdr->u1.ptp.timestamp61_37_9_0;

        ts_61_0 = (((uint64)ts_4_0) << 0) | (((uint64)ts_36_5) << 5) | (((uint64)ts_61_37) << 37);
    }
    else
    {
        CTC_ERROR_RETURN(_sys_usw_packet_recover_timestamp(lchip, ts_36_5, &ts_61_0));
    }

    sal_memset(&ptp_offset_adjust, 0, sizeof(ptp_offset_adjust));
    cmd = DRV_IOR(TsEngineOffsetAdj_t, DRV_ENTRY_FLAG);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &ptp_offset_adjust));

    GetTsEngineOffsetAdj(A, offsetNs_f, &ptp_offset_adjust, &offset_ns);
    GetTsEngineOffsetAdj(A, offsetSecond_f, &ptp_offset_adjust, &offset_s);

    ts_61_0 = ts_61_0 + offset_s * 1000000000ULL + offset_ns;
    _sys_usw_packet_bit62_to_ts(lchip, TRUE, ts_61_0, &p_rx_info->ptp.ts);


    /* OAM */
    if (CTC_PKT_OPER_OAM == p_rx_info->oper_type)
    {
        p_rx_info->oam.type = p_raw_hdr->u1.oam.oam_type;
        p_rx_info->oam.mep_index = p_raw_hdr->u1.oam.mep_index;

        if (p_raw_hdr->u1.oam.is_up)
        {
            CTC_SET_FLAG(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_UP_MEP);
        }

        if (p_raw_hdr->u1.oam.lm_received_packet)
        {
            CTC_SET_FLAG(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_LM);
        }

        if (p_raw_hdr->u1.oam.link_oam)
        {
            CTC_SET_FLAG(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_LINK_OAM);
        }

        if (p_raw_hdr->u1.oam.dm_en)
        {
            CTC_SET_FLAG(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_DM);
        }

        if (p_raw_hdr->u1.oam.gal_exist)
        {
            CTC_SET_FLAG(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_HAS_MPLS_GAL);
        }

        if (CTC_IS_BIT_SET(p_raw_hdr->u2_data, SYS_PKT_U2_OAM_MIP_EN)) /*u2_oam_mipEn, 1, {2,8,1}*/
        {
            CTC_SET_FLAG(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_MIP);
        }

        /* get timestamp offset in bytes for DM */
        if (CTC_FLAG_ISSET(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_DM))
        {
            /*  DM Timestamp Bits Mapping
                Timestamp               Field Name          BIT Width   BIT Base
                dmTimestamp[37:0]       timestamp           38          0
                dmTimestamp[61:38]      u3_other_timestamp  24          38
             */
 #if 0
            GetMsPacketHeader(A, timestamp_f, p_raw_hdr, &ts_37_0_tmp);
            ts_37_0 = ts_37_0_tmp[1];
            ts_37_0 <<= 32;
            ts_37_0 |= ts_37_0_tmp[0];

            ts_61_38 = GetMsPacketHeader(V, u3_other_timestamp_f, p_raw_hdr);

            ts_61_0 = ((uint64)(ts_37_0) << 0)
                    | ((uint64)(ts_61_38) << 38);
            ns_only_format = FALSE;
            _sys_usw_packet_bit62_to_ts(lchip, ns_only_format, ts_61_0, &p_rx_info->oam.dm_ts);
#endif
        }

        if (CTC_FLAG_ISSET(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_LM))
        {
            p_rx_info->oam.lm_fcl = (p_raw_hdr->u3_data_2<<8) | p_raw_hdr->u3_data_1; /*u3_oam_rxtxFcl, 24, {4,0,16}, {3,24,8}*/
            p_rx_info->oam.lm_fcl = (p_rx_info->oam.lm_fcl << 8) + p_raw_hdr->u1.oam.rxtx_fcl;   /*u1_oam_rxtxFcl, 8, {0,0,8}*/
        }

        /* get svid from vlan_ptr for Up MEP/Up MIP */
        if (CTC_FLAG_ISSET(p_rx_info->oam.flags, CTC_PKT_OAM_FLAG_IS_UP_MEP))
        {
            CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_SRC_SVID_VALID);
            p_rx_info->src_svid = p_raw_hdr->src_vlan_ptr;
        }
    }
    else if (CTC_PKT_OPER_C2C == p_rx_info->oper_type)
    {
        uint8  sub_queue_id = 0;
        /* Stacking*/
        p_rx_info->reason       = CTC_PKT_CPU_REASON_C2C_PKT;
        p_rx_info->stacking_src_port     = p_raw_hdr->u1.c2c.stacking_src_port;
        p_rx_info->stacking_src_port     = SYS_MAP_DRV_GPORT_TO_CTC_GPORT(p_rx_info->stacking_src_port);

        if (CTC_FLAG_ISSET(p_rx_info->flags, CTC_PKT_FLAG_MCAST))
        {
            CTC_UNSET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_MCAST);
            p_rx_info->dest_group_id = 0;
        }
        p_rx_info->vrfid = 0;

        if (p_raw_hdr->u1.c2c.neighbor_discover_packet)
        {
            p_rx_info->dest_gport = (dest_map & 0xF) * MCHIP_CAP(SYS_CAP_REASON_GRP_QUEUE_NUM) + (p_rx_info->priority/2);
        }
        else
        {
           p_rx_info->dest_gport = (dest_map & 0xF) * MCHIP_CAP(SYS_CAP_REASON_GRP_QUEUE_NUM) + (p_rx_info->priority%8);
        }

        CTC_ERROR_RETURN(sys_usw_get_sub_queue_id_by_cpu_reason(lchip, CTC_PKT_CPU_REASON_C2C_PKT, &sub_queue_id));
        if(DRV_IS_DUET2(lchip))
        {
            p_rx_info->priority = (p_rx_info->dest_gport > sub_queue_id) ? (p_rx_info->dest_gport - sub_queue_id) : 0;
        }
    }
    else if(2 == p_rx_info->oper_type)  /* OPERATIONTYPE_E2ILOOP */
    {
        if(p_raw_hdr->u1.e2iloop_out.need_dot1ae_encrypt
            || p_raw_hdr->u1.e2iloop_in.need_capwap_decap)
        {
             CTC_UNSET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_ENCRYPTED);
        }
    }

    if (p_rx_info->reason == CTC_PKT_CPU_REASON_MONITOR_BUFFER_LOG)
    {
        if (p_raw_hdr->src_stag_cfi)
        {
            CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_BUFFER_VICTIM_PKT);
        }
    }

    if (p_raw_hdr->from_fabric)
    {
        uint8 legacy_en = 0;
        uint16 cloud_hdr_len = 0;
        uint16 extHeaderLen = 0;
        uint16 stk_hdr_len = 0;
        uint32 stk_basic[4] = {0};
        uint32 gport = 0;
        uint8  packet_rx_header_en = 0;
        uint16 local_phy_port =0;
        uint8 gchip_id = 0;
        CFHeaderBasic_m* p_stk_header = NULL;
        bool inner_pkt_hdr_en = FALSE;
        uint8 oper_type = 0;

        local_phy_port = p_raw_hdr->local_phy_port;
        sys_usw_get_gchip_id(lchip, &gchip_id);
        gport = CTC_MAP_LPORT_TO_GPORT(gchip_id, local_phy_port);


        if (REGISTER_API(lchip)->stacking_get_stkhdr_len)
        {
            CTC_ERROR_RETURN(REGISTER_API(lchip)->stacking_get_stkhdr_len(lchip, gport, &packet_rx_header_en, &legacy_en, &cloud_hdr_len));
        }

        if (packet_rx_header_en)
        {
            if (!legacy_en)
            {
                /*Using CFlex header, get from stacking module only means Cloud Header Length*/
                p_stk_header = (CFHeaderBasic_m*)(p_pkt_rx->pkt_buf[0].data + SYS_USW_PKT_HEADER_LEN + p_pkt_rx->eth_hdr_len + cloud_hdr_len);
                CTC_ERROR_RETURN(sys_usw_dword_reverse_copy((uint32*)stk_basic, (uint32*)p_stk_header, 4));
                CTC_ERROR_RETURN(sys_usw_swap32((uint32*)stk_basic, 4, FALSE));
                extHeaderLen = GetCFHeaderBasic(V, extHeaderLen_f, (uint32*)stk_basic);
                stk_hdr_len = (16 + (extHeaderLen << 3));
            }
            else
            {
                stk_hdr_len = SYS_PACKET_BASIC_STACKING_HEADER_LEN;
            }

            p_pkt_rx->stk_hdr_len = cloud_hdr_len + stk_hdr_len;
        }
        CTC_ERROR_RETURN(sys_usw_global_ctl_get(lchip, CTC_GLOBAL_STK_WITH_IGS_PKT_HDR_EN, &inner_pkt_hdr_en))
        if (inner_pkt_hdr_en)
        {
            p_raw_hdr_net = (uint32*)(p_pkt_rx->pkt_buf[0].data + SYS_USW_PKT_HEADER_LEN + p_pkt_rx->eth_hdr_len + p_pkt_rx->stk_hdr_len);
            CTC_ERROR_RETURN(sys_usw_dword_reverse_copy((uint32*)bheader, (uint32*)p_raw_hdr_net, SYS_USW_PKT_HEADER_LEN/4));
            CTC_ERROR_RETURN(sys_usw_swap32((uint32*)p_raw_hdr, SYS_USW_PKT_HEADER_LEN / 4, FALSE));
            oper_type  = p_raw_hdr->operation_type;
            if ((CTC_PKT_OPER_NORMAL == oper_type) && (p_raw_hdr->u1.normal.metadata_type == 0))
            {
                /*vrfid*/
                p_rx_info->vrfid= p_raw_hdr->u1.normal.metadata;
                CTC_SET_FLAG(p_rx_info->flags, CTC_PKT_FLAG_VRFID_VALID);
            }
            p_pkt_rx->stk_hdr_len += SYS_USW_PKT_HEADER_LEN;
        }
    }
    else
    {
        uint8 legacy_en = 0;
        uint16 cloud_hdr_len = 0;
        uint16 extHeaderLen = 0;
        uint16 stk_hdr_len = 0;
        uint8  packet_rx_header_en = 0;
        uint32 stk_basic[8] = {0};
        CFHeaderBasic_m* p_stk_header = NULL;
        PacketHeaderOuter_m* p_legacy_header = NULL;
        IpePhyPortMuxCtl_m port_mux;
        uint8 cflex_mode = 0;
        uint16 cflex_port = 0;
        IpeHeaderAdjustCtl_m ipe_header;
        sys_datapath_lport_attr_t* p_port_attr = NULL;
        sal_memset(&port_mux, 0, sizeof(IpePhyPortMuxCtl_m));

        lport = p_raw_hdr->local_phy_port;
        CTC_ERROR_RETURN(sys_usw_mac_get_port_capability(lchip, lport, &p_port_attr));
        if (p_port_attr->port_type == SYS_DMPS_NETWORK_PORT)
        {
            cmd = DRV_IOR(IpePhyPortMuxCtl_t, DRV_ENTRY_FLAG);
            CTC_ERROR_RETURN(DRV_IOCTL(lchip, lport, cmd, &port_mux));
            cmd = DRV_IOR(IpeHeaderAdjustCtl_t, DRV_ENTRY_FLAG);
            CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &ipe_header));
        }

        cflex_mode = GetIpePhyPortMuxCtl(V, cFlexLookupMode_f, &port_mux);
        cflex_port = (GetIpeHeaderAdjustCtl(V, cFlexLookupDestMap_f, &ipe_header) & GetIpeHeaderAdjustCtl(V, cFlexLookupDestMapMask_f, &ipe_header));

        if (REGISTER_API(lchip)->stacking_get_stkhdr_len)
        {
            CTC_ERROR_RETURN(REGISTER_API(lchip)->stacking_get_stkhdr_len(lchip, p_rx_info->src_port, &packet_rx_header_en, &legacy_en, &cloud_hdr_len));
        }

        if (packet_rx_header_en)
        {
            if (!legacy_en)
            {
                /*Using CFlex header, get from stacking module only means Cloud Header Length*/
                p_stk_header = (CFHeaderBasic_m*)(p_pkt_rx->pkt_buf[0].data + SYS_USW_PKT_HEADER_LEN + p_pkt_rx->eth_hdr_len + cloud_hdr_len);
                CTC_ERROR_RETURN(sys_usw_dword_reverse_copy((uint32*)stk_basic, (uint32*)p_stk_header, 4));
                CTC_ERROR_RETURN(sys_usw_swap32((uint32*)stk_basic, 4, FALSE));
                extHeaderLen = GetCFHeaderBasic(V, extHeaderLen_f, (uint32*)stk_basic);
                stk_hdr_len = (16 + (extHeaderLen << 3));
                if (cflex_mode == 1 || ((cflex_mode == 2) && ((GetCFHeaderBasic(V, destMap_f, (uint32*)stk_basic) &
                     GetIpeHeaderAdjustCtl(V, cFlexLookupDestMapMask_f, &ipe_header)) == cflex_port)))
                {
                    p_rx_info->src_port = GetCFHeaderBasic(V, sourcePort_f,(uint32*)stk_basic);
                    p_rx_info->src_chip = SYS_DRV_GPORT_TO_GCHIP(p_rx_info->src_port);
                    if (GetCFHeaderBasic(V, fromLag_f, (uint32*)stk_basic))
                    {
                        p_rx_info->src_port = SYS_MAP_DRV_LPORT_TO_CTC_GPORT(CTC_LINKAGG_CHIPID, SYS_DRV_GPORT_TO_LPORT(p_rx_info->src_port));
                    }
                    else
                    {
                        p_rx_info->src_port = SYS_MAP_DRV_GPORT_TO_CTC_GPORT(p_rx_info->src_port);
                    }
                }
            }
            else
            {
                p_legacy_header = (PacketHeaderOuter_m*)(p_pkt_rx->pkt_buf[0].data + SYS_USW_PKT_HEADER_LEN + p_pkt_rx->eth_hdr_len + cloud_hdr_len);
                CTC_ERROR_RETURN(sys_usw_dword_reverse_copy((uint32*)stk_basic, (uint32*)p_legacy_header, 8));
                CTC_ERROR_RETURN(sys_usw_swap32((uint32*)stk_basic, 8, FALSE));
                stk_hdr_len = SYS_PACKET_BASIC_STACKING_HEADER_LEN;
                if (cflex_mode == 1 || ((cflex_mode == 2) && ((GetPacketHeaderOuter(V, destMap_f, (uint32*)stk_basic) &
                        GetIpeHeaderAdjustCtl(V, cFlexLookupDestMapMask_f, &ipe_header))== cflex_port)))
                {
                    p_rx_info->src_port = GetPacketHeaderOuter(V, sourcePort_f,(uint32*)stk_basic);
                    p_rx_info->src_chip = SYS_DRV_GPORT_TO_GCHIP(p_rx_info->src_port);
                    if (p_rx_info->src_chip == 0x1f)
                    {
                        p_rx_info->src_chip = (((p_rx_info->src_port>>6)&0x7)|(((p_rx_info->src_port>>14)&0x3) << 3));
                        p_rx_info->src_port = SYS_MAP_DRV_GPORT_TO_CTC_GPORT((p_rx_info->src_port&0xfe3f));
                    }
                    else
                    {
                        p_rx_info->src_port = SYS_MAP_DRV_GPORT_TO_CTC_GPORT(p_rx_info->src_port);
                    }
                }
            }
            p_pkt_rx->stk_hdr_len = cloud_hdr_len + stk_hdr_len;
        }
    }

    return CTC_E_NONE;
}

