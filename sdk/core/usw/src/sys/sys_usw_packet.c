/**
 @file sys_usw_packet.c

 @author  Copyright (C) 2012 Centec Networks Inc.  All rights reserved.

 @date 2012-11-25

 @version v2.0

 This file define sys functions

*/

/****************************************************************************
*
* Header Files
*
****************************************************************************/
#include "sal.h"
#include "dal.h"
#include "ctc_error.h"
#include "ctc_macro.h"
#include "ctc_debug.h"
#include "ctc_packet.h"
#include "ctc_parser.h"
#include "ctc_oam.h"
#include "ctc_qos.h"
#include "ctc_crc.h"
#include "ctc_vlan.h"
#include "ctc_nexthop.h"
#include "ctc_usw_packet.h"
#include "ctc_usw_nexthop.h"
#include "sys_usw_common.h"
#include "sys_usw_packet.h"
#include "sys_usw_packet_priv.h"
#include "sys_usw_dma.h"
#include "sys_usw_chip.h"
#include "sys_usw_vlan.h"
#include "sys_usw_port.h"
#include "sys_usw_dmps.h"
#include "sys_usw_qos_api.h"
#include "sys_usw_nexthop_api.h"
#include "sys_usw_register.h"
#include "sys_usw_dma_priv.h"
#include "sys_usw_wb_common.h"
#include "drv_api.h"
#include "usw/include/drv_chip_ctrl.h"
#include "ctc_hash.h"

/****************************************************************************
*
* Defines and Macros
*
*****************************************************************************/


#ifdef CTC_PKT_RX_CB_LIST
   #define SYS_PACKET_RX_CB_MAX_COUNT 128   
#else
   #define SYS_PACKET_RX_CB_MAX_COUNT 1
#endif
    
/****************************************************************************
*
* Global and Declaration
*
*****************************************************************************/
sys_pkt_master_t* p_usw_pkt_master[CTC_MAX_LOCAL_CHIP_NUM] = {NULL};
extern int32 sys_usw_packet_set_cpu_mac(uint8 lchip, uint8 idx, uint32 gport, mac_addr_t da, mac_addr_t sa);

typedef struct sys_pkt_rx_cb_node_s
{
    ctc_slistnode_t node;
    ctc_pkt_rx_register_t data;
    uint8 is_global; /*means used for all received packets*/
}sys_pkt_rx_cb_node_t;

/****************************************************************************
*
* Lock
*
*****************************************************************************/
#ifndef PACKET_TX_USE_SPINLOCK
#define SYS_PACKET_LOCK(lchip) \
    if(p_usw_pkt_master[lchip]->mutex) sal_mutex_lock(p_usw_pkt_master[lchip]->mutex)

#define SYS_PACKET_UNLOCK(lchip) \
    if(p_usw_pkt_master[lchip]->mutex) sal_mutex_unlock(p_usw_pkt_master[lchip]->mutex)
#else
#define SYS_PACKET_LOCK(lchip) \
    if(p_usw_pkt_master[lchip]->mutex) sal_spinlock_lock((sal_spinlock_t*)p_usw_pkt_master[lchip]->mutex)

#define SYS_PACKET_UNLOCK(lchip) \
    if(p_usw_pkt_master[lchip]->mutex) sal_spinlock_unlock((sal_spinlock_t*)p_usw_pkt_master[lchip]->mutex)
#endif

#define SYS_PACKET_RX_LOCK(lchip) \
    if(p_usw_pkt_master[lchip]->mutex_rx) sal_mutex_lock(p_usw_pkt_master[lchip]->mutex_rx)

#define SYS_PACKET_RX_UNLOCK(lchip) \
    if(p_usw_pkt_master[lchip]->mutex_rx) sal_mutex_unlock(p_usw_pkt_master[lchip]->mutex_rx)
#define SYS_PACKET_RX_CB_LOCK(lchip) \
    if(p_usw_pkt_master[lchip]->mutex_rx_cb) sal_mutex_lock(p_usw_pkt_master[lchip]->mutex_rx_cb)

#define SYS_PACKET_RX_CB_UNLOCK(lchip) \
    if(p_usw_pkt_master[lchip]->mutex_rx_cb) sal_mutex_unlock(p_usw_pkt_master[lchip]->mutex_rx_cb)

#define SYS_PACKET_INIT_CHECK(lchip) \
    if (p_usw_pkt_master[lchip] == NULL)\
    {\
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " Feature not initialized \n");\
        return CTC_E_NOT_INIT;\
    }

#define CTC_ERROR_RETURN_PACKET_UNLOCK(lchip, op) \
    do \
    { \
        int32 rv = (op); \
        if (rv < 0) \
        { \
            sal_mutex_unlock(p_usw_pkt_master[lchip]->mutex); \
            return rv; \
        } \
    } while (0)

#define CTC_ERROR_RETURN_PACKET_RX_UNLOCK(lchip, op) \
    do \
    { \
        int32 rv = (op); \
        if (rv < 0) \
        { \
            sal_mutex_unlock(p_usw_pkt_master[lchip]->mutex_rx); \
            return rv; \
        } \
    } while (0)

#define CTC_RETURN_PACKET_UNLOCK(lchip, op) \
    do \
    { \
        sal_mutex_unlock(p_usw_pkt_master[lchip]->mutex); \
        return (op); \
    } while (0)

extern char*
sys_usw_reason_2Str(uint16 reason_id);

extern dal_op_t g_dal_op;
/****************************************************************************
*
* Function
*
*****************************************************************************/
#define ROT(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
#define MIX(a, b, c) \
    do \
    { \
        a -= c;  a ^= ROT(c, 4);  c += b; \
        b -= a;  b ^= ROT(a, 6);  a += c; \
        c -= b;  c ^= ROT(b, 8);  b += a; \
        a -= c;  a ^= ROT(c, 16);  c += b; \
        b -= a;  b ^= ROT(a, 19);  a += c; \
        c -= b;  c ^= ROT(b, 4);  b += a; \
    } while (0)

static uint32
_sys_usw_netif_hash_make(ctc_pkt_netif_t* p_netif)
{
    uint32 a, b, c;

    /* Set up the internal state */
    a = b = c = 0xdeadbeef + (((uint32)96) << 2);

    a += p_netif->gport;
    b += p_netif->vlan;
    MIX(a, b, c);

    return (c & SYS_PKT_NETIF_HASH_SIZE);
}

static bool
_sys_usw_netif_hash_cmp(ctc_pkt_netif_t* p_netif1, ctc_pkt_netif_t* p_netif2)
{
    if (p_netif1->type != p_netif2->type)
    {
        return FALSE;
    }

    if ((p_netif1->type == CTC_PKT_NETIF_T_PORT) &&
        (p_netif1->gport == p_netif2->gport))
    {
        return TRUE;
    }

    if ((p_netif1->type == CTC_PKT_NETIF_T_VLAN) &&
        (p_netif1->vlan == p_netif2->vlan))
    {
        return TRUE;
    }

    return FALSE;
}

extern int32
_ctc_usw_dkit_memory_show_ram_by_data_tbl_id(uint8 lchip,  uint32 tbl_id, uint32 index, uint32* data_entry, char* regex, uint8 acc_io);

static int32
_sys_usw_packet_dump_stacking_header(uint8 lchip, uint8* data, uint32 len, uint8 version, uint8 brife)
{
    uint8* pkt_buf = NULL;
    int32 ret = CTC_E_NONE;
    if (!data)
    {
        return CTC_E_INVALID_PARAM;
    }
    pkt_buf = mem_malloc(MEM_SYSTEM_MODULE, CTC_PKT_MTU*sizeof(uint8));
    if (!pkt_buf)
    {
        return CTC_E_NO_MEMORY;
    }
    sal_memset(pkt_buf, 0, CTC_PKT_MTU*sizeof(uint8));
    sal_memcpy(pkt_buf, data, len);
    if (CTC_STACKING_VERSION_1_0 == version)
    {
        uint8 pkt_hdr_out[32] = {0};
        CTC_ERROR_GOTO((sys_usw_dword_reverse_copy((uint32*)pkt_hdr_out, (uint32*)pkt_buf, 32/4)), ret, error0);
        CTC_ERROR_GOTO((sys_usw_swap32((uint32*)pkt_hdr_out, 32 / 4, TRUE)), ret, error0);
        if (!brife)
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8s%-8s%-13s%-9s%-40s\n", "Index", "FldID", "Value", "BitLen", "FldName");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
            _ctc_usw_dkit_memory_show_ram_by_data_tbl_id(lchip, PacketHeaderOuter_t, 0, (uint32 *)pkt_hdr_out, NULL, 0);
        }
        else
        {
            SYS_PKT_DUMP("-------------------------------------------------------------\n");
            SYS_PKT_DUMP( "%-20s: %d\n", "Priority",  GetPacketHeaderOuter(V, _priority_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: 0x%08x\n", "DestMap", GetPacketHeaderOuter(V, destMap_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: %d\n", "HeaderType", GetPacketHeaderOuter(V, headerType_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: %d\n", "HeaderVersion", GetPacketHeaderOuter(V, headerVersion_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: %d\n", "LogicPortType", GetPacketHeaderOuter(V, logicPortType_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: %d\n", "LogicSrcPort", GetPacketHeaderOuter(V, logicSrcPort_f, pkt_hdr_out));
            if (CTC_PKT_OPER_C2C == GetPacketHeaderOuter(V, operationType_f, pkt_hdr_out))
            {
                SYS_PKT_DUMP( "%-20s: %d\n", "NextHopPtr", GetPacketHeaderOuter(V, nextHopPtr_f, pkt_hdr_out));
                SYS_PKT_DUMP( "%-20s: %d\n", "NextHopExt", GetPacketHeaderOuter(V, nextHopExt_f, pkt_hdr_out));
            }
            else
            {
                SYS_PKT_DUMP("%-20s: %d\n", "CpuReason", CTC_PKT_CPU_REASON_GET_BY_NHPTR(GetPacketHeaderOuter(V, nextHopPtr_f, pkt_hdr_out)));
            }
            SYS_PKT_DUMP( "%-20s: %d\n", "OperationType", GetPacketHeaderOuter(V, operationType_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: 0x%04x\n", "SourcePort", GetPacketHeaderOuter(V, sourcePort_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: %d\n", "PacketType", GetPacketHeaderOuter(V, packetType_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: %d\n", "SrcVlanPtr", GetPacketHeaderOuter(V, srcVlanPtr_f, pkt_hdr_out));
            SYS_PKT_DUMP( "%-20s: %d\n", "Ttl", GetPacketHeaderOuter(V, ttl_f, pkt_hdr_out));
            if (CTC_PKT_OPER_OAM == GetPacketHeaderOuter(V, operationType_f, pkt_hdr_out))
            {
                SYS_PKT_DUMP("OAM info\n");
                SYS_PKT_DUMP("-------------------------------------------------------------\n");
                 SYS_PKT_DUMP( "%-20s: %d\n", "DmEn", GetPacketHeaderOuter(V, u1_oam_dmEn_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "FromCpuOrOAM", GetPacketHeaderOuter(V, u1_oam_fromCpuOrOam_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "GalExist", GetPacketHeaderOuter(V, u1_oam_galExist_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "IsUp", GetPacketHeaderOuter(V, u1_oam_isUp_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "LinkOAM", GetPacketHeaderOuter(V, u1_oam_linkOam_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "MipEn", GetPacketHeaderOuter(V, u1_oam_mipEn_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "OAMType", GetPacketHeaderOuter(V, u1_oam_oamType_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "RxOAM", GetPacketHeaderOuter(V, u1_oam_rxOam_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "OAMLocalPhyPort", GetPacketHeaderOuter(V, u2_oam_localPhyPort_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "MepIndex", GetPacketHeaderOuter(V, u2_oam_mepIndex_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "MepIndex", GetPacketHeaderOuter(V, u2_oam_mepIndex_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "DestChipId", GetPacketHeaderOuter(V, u2_oam_oamDestChipId_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "Offset3to2", GetPacketHeaderOuter(V, u2_oam_oamPacketOffset_3to2_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "Offset4", GetPacketHeaderOuter(V, u2_oam_oamPacketOffset_4_f, pkt_hdr_out));
                 SYS_PKT_DUMP( "%-20s: %d\n", "Offset7to5", GetPacketHeaderOuter(V, u2_oam_oamPacketOffset_7to5_f, pkt_hdr_out));
            }
            if (CTC_PKT_OPER_PTP == GetPacketHeaderOuter(V, operationType_f, pkt_hdr_out))
            {
                SYS_PKT_DUMP("PTP info\n");
                SYS_PKT_DUMP("-------------------------------------------------------------\n");
                SYS_PKT_DUMP( "%-20s: %d\n", "PtpEgsAsyDly", GetPacketHeaderOuter(V, u1_ptp_ptpApplyEgressAsymmetryDelay_f, pkt_hdr_out));
                SYS_PKT_DUMP( "%-20s: %d\n", "PtpEditType", GetPacketHeaderOuter(V, u1_ptp_ptpEditType_f, pkt_hdr_out));
                SYS_PKT_DUMP( "%-20s: %d\n", "PtpExtraOffset", GetPacketHeaderOuter(V, u1_ptp_ptpExtraOffset_f, pkt_hdr_out));
                SYS_PKT_DUMP( "%-20s: %d\n", "PtpSequenceId", GetPacketHeaderOuter(V, u1_ptp_ptpSequenceId_f, pkt_hdr_out));
                SYS_PKT_DUMP( "%-20s: %d\n", "PtpFid", GetPacketHeaderOuter(V, u2_ptp_fid_f, pkt_hdr_out));
                SYS_PKT_DUMP( "%-20s: %d\n", "PtpOffset", GetPacketHeaderOuter(V, u2_ptp_ptpOffset_f, pkt_hdr_out));
                SYS_PKT_DUMP( "%-20s: %d\n", "Timestamp63to32", GetPacketHeaderOuter(V, u3_ptp_timestamp_63to32_f, pkt_hdr_out));
                SYS_PKT_DUMP( "%-20s: %d\n", "Timestamp31to0", GetPacketHeaderOuter(V, u4_ptp_timestamp_31to0_f, pkt_hdr_out));
            }
        }
    }
    else
    {
        uint8 *p_temp = NULL;
        uint8 CFHeaderBasic[16] = {0};
        uint8 ext_hdr_len = 0;
        uint8 ext_type = 0;
        uint8 CFExtHeader[8] = {0};
        uint8 i = 0;
        sal_memcpy(CFHeaderBasic, pkt_buf, 16);
        ext_hdr_len = CFHeaderBasic[0]>>4;
        p_temp = &pkt_buf[16];
        CTC_ERROR_GOTO((sys_usw_dword_reverse_copy((uint32*)CFHeaderBasic, (uint32*)pkt_buf, 16/4)), ret, error0);
        CTC_ERROR_GOTO((sys_usw_swap32((uint32*)CFHeaderBasic, 16 / 4, TRUE)), ret, error0);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"CFHeaderBasic\n");
        if (!brife)
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8s%-8s%-13s%-9s%-40s\n", "Index", "FldID", "Value", "BitLen", "FldName");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
            _ctc_usw_dkit_memory_show_ram_by_data_tbl_id(lchip, CFHeaderBasic_t, 0, (uint32 *)CFHeaderBasic, NULL, 0);
            for (i = 0; i < ext_hdr_len; i++)
            {
                ext_type = p_temp[0] >> 4;
                CTC_ERROR_GOTO((sys_usw_dword_reverse_copy((uint32*)CFExtHeader, (uint32*)p_temp, 8 / 4)), ret, error0);
                CTC_ERROR_GOTO((sys_usw_swap32((uint32*)CFExtHeader, 8 / 4, TRUE)), ret, error0);
                if (1 == ext_type)
                {
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "CFHeaderExtEgrEdit\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8s%-8s%-13s%-9s%-40s\n", "Index", "FldID", "Value", "BitLen", "FldName");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                    _ctc_usw_dkit_memory_show_ram_by_data_tbl_id(lchip, CFHeaderExtEgrEdit_t, 0, (uint32 *)CFExtHeader, NULL, 0);
                }
                else if (2 == ext_type)
                {
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "CFHeaderExtCid\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8s%-8s%-13s%-9s%-40s\n", "Index", "FldID", "Value", "BitLen", "FldName");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                    _ctc_usw_dkit_memory_show_ram_by_data_tbl_id(lchip, CFHeaderExtCid_t, 0, (uint32 *)CFExtHeader, NULL, 0);
                }
                else if (3 == ext_type)
                {
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "CFHeaderExtLearning\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8s%-8s%-13s%-9s%-40s\n", "Index", "FldID", "Value", "BitLen", "FldName");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                    _ctc_usw_dkit_memory_show_ram_by_data_tbl_id(lchip, CFHeaderExtLearning_t, 0, (uint32 *)CFExtHeader, NULL, 0);
                }
                else if (4 == ext_type)
                {
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "CFHeaderExtOam\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8s%-8s%-13s%-9s%-40s\n", "Index", "FldID", "Value", "BitLen", "FldName");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                    _ctc_usw_dkit_memory_show_ram_by_data_tbl_id(lchip, CFHeaderExtOam_t, 0, (uint32 *)CFExtHeader, NULL, 0);
                }
                p_temp += 8;
            }
        }
        else
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: 0x%08x\n", "DestMap", GetCFHeaderBasic(V, destMap_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "ExtHeaderLen", GetCFHeaderBasic(V, extHeaderLen_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "Fid", GetCFHeaderBasic(V, fid_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "LogicSrcPort", GetCFHeaderBasic(V, logicSrcPort_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "OperationType", GetCFHeaderBasic(V, operationType_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "PacketType", GetCFHeaderBasic(V, packetType_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: 0x%04x\n", "SourcePort", GetCFHeaderBasic(V, sourcePort_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "SrcVlanPtr", GetCFHeaderBasic(V, srcVlanPtr_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "Priority", GetCFHeaderBasic(V, prio_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "FromCpuOrOam", GetCFHeaderBasic(V, fromCpuOrOam_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "FromLag", GetCFHeaderBasic(V, fromLag_f, CFHeaderBasic));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
            for (i = 0; i < ext_hdr_len && brife; i++)
            {
                ext_type = p_temp[0] >> 4;
                CTC_ERROR_GOTO((sys_usw_dword_reverse_copy((uint32*)CFExtHeader, (uint32*)p_temp, 8 / 4)), ret, error0);
                CTC_ERROR_GOTO((sys_usw_swap32((uint32*)CFExtHeader, 8 / 4, TRUE)), ret, error0);
                if (1 == ext_type)
                {
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "CFHeaderExtEgrEdit\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "EcmpHash", GetCFHeaderExtEgrEdit(V, ecmpHash_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "EgressEditEn", GetCFHeaderExtEgrEdit(V, egressEditEn_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "NextHopPtr", GetCFHeaderExtEgrEdit(V, nextHopPtr_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "SrcDscp", GetCFHeaderExtEgrEdit(V, srcDscp_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "Ttl", GetCFHeaderExtEgrEdit(V, ttl_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                }
                else if (2 == ext_type)
                {
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "CFHeaderExtCid\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "IsSpanPkt", GetCFHeaderExtCid(V, isSpanPkt_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "LogicPortType", GetCFHeaderExtCid(V, logicPortType_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "NoDot1AeEncrypt", GetCFHeaderExtCid(V, noDot1AeEncrypt_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "PktWithCidHeader", GetCFHeaderExtCid(V, pktWithCidHeader_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "TerminateCidHdr", GetCFHeaderExtCid(V, terminateCidHdr_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                }
                else if (3 == ext_type)
                {
                    uint32 mac[2] = {0};
                    GetCFHeaderExtLearning(A, macAddr_f, CFExtHeader, mac);
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "CFHeaderExtLearning\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: 0x%08x\n", "MacAddr", mac[1]);
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: 0x%08x\n", "", mac[0]);
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                }
                else if (4 == ext_type)
                {
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "CFHeaderExtOam\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "DmEn", GetCFHeaderExtOam(V, dmEn_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "DmOffset", GetCFHeaderExtOam(V, dmOffset_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "FromCpuLmDownDis", GetCFHeaderExtOam(V, fromCpuLmDownDisable_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "FromCpuLmUpDis", GetCFHeaderExtOam(V, fromCpuLmUpDisable_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "GalExist", GetCFHeaderExtOam(V, galExist_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "IsUp", GetCFHeaderExtOam(V, isUp_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "LinkOam", GetCFHeaderExtOam(V, linkOam_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "LocalPhyPort", GetCFHeaderExtOam(V, localPhyPort_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "LinkOam", GetCFHeaderExtOam(V, linkOam_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "MepIndex", GetCFHeaderExtOam(V, mepIndex_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "MipEn", GetCFHeaderExtOam(V, mipEn_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "OamPacketOffset", GetCFHeaderExtOam(V, oamPacketOffset_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "OamTunnelEn", GetCFHeaderExtOam(V, oamTunnelEn_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "OamType", GetCFHeaderExtOam(V, oamType_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "RxOam", GetCFHeaderExtOam(V, rxOam_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP,"%-20s: %d\n", "UseOamTtl", GetCFHeaderExtOam(V, useOamTtl_f, CFExtHeader));
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
                }
                p_temp += 8;
            }
        }
    }

error0:
    if (pkt_buf)
    {
        mem_free(pkt_buf);
    }
    return ret;
}

static int32
_sys_usw_packet_dump(uint8 lchip, uint8* data, uint32 len)
{
    uint32 cnt = 0;
    char line[256] = {'\0'};
    char tmp[32] = {'\0'};

    if (0 == len)
    {
        return CTC_E_NONE;
    }

    for (cnt = 0; cnt < len; cnt++)
    {
        if ((cnt % 16) == 0)
        {
            if (cnt != 0)
            {
                SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%s", line);
            }

            sal_memset(line, 0, sizeof(line));
            if (cnt == 0)
            {
                sal_sprintf(tmp, "0x%04x:  ", cnt);
            }
            else
            {
                sal_sprintf(tmp, "\n0x%04x:  ", cnt);
            }
            sal_strcat(line, tmp);
        }

        sal_sprintf(tmp, "%02x", data[cnt]);
        sal_strcat(line, tmp);

        if ((cnt % 2) == 1)
        {
            sal_strcat(line, " ");
        }
    }

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%s", line);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n");

    return CTC_E_NONE;
}

static int32
_sys_usw_packet_dump_rx_header(uint8 lchip, ctc_pkt_rx_t  *p_rx_info)
{
    char* p_str_tmp = NULL;
    char str[40] = {0};
    char* str_op_type[] = {"NORMAL", "LMTX", "E2ILOOP", "DMTX", "C2C", "PTP", "NAT", "OAM"};

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Header Info(Length : %d): \n", SYS_USW_PKT_HEADER_LEN);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

    p_str_tmp = sys_usw_reason_2Str(p_rx_info->rx_info.reason);
    sal_sprintf((char*)&str, "%s%s%d%s", p_str_tmp, "(ID:", p_rx_info->rx_info.reason, ")");

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %s\n", "cpu reason", (char*)&str);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "queue id", p_rx_info->rx_info.dest_gport);

    /*source*/
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: 0x%.04x\n", "src port", p_rx_info->rx_info.src_port);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: 0x%.04x\n", "stacking src port", p_rx_info->rx_info.stacking_src_port);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "src svid", p_rx_info->rx_info.src_svid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "src cvid", p_rx_info->rx_info.src_cvid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "src cos", p_rx_info->rx_info.src_cos);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "stag op", p_rx_info->rx_info.stag_op);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "ctag op", p_rx_info->rx_info.ctag_op);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "vrf", p_rx_info->rx_info.vrfid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "fid", p_rx_info->rx_info.fid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "ttl", p_rx_info->rx_info.ttl);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "priority", p_rx_info->rx_info.priority);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "color", p_rx_info->rx_info.color);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "hash value", p_rx_info->rx_info.hash);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "critical packet", p_rx_info->rx_info.is_critical);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "packet type", p_rx_info->rx_info.packet_type);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "src chip", p_rx_info->rx_info.src_chip);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "payload offset", p_rx_info->rx_info.payload_offset);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "buffer log victim packet",CTC_FLAG_ISSET(p_rx_info->rx_info.flags, CTC_PKT_FLAG_BUFFER_VICTIM_PKT));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %s\n", "operation type", str_op_type[p_rx_info->rx_info.oper_type]);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "logic src port", p_rx_info->rx_info.logic_src_port);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "meta data", p_rx_info->rx_info.meta_data);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "cid", p_rx_info->rx_info.cid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "lport", p_rx_info->rx_info.lport);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "mac unknown", CTC_FLAG_ISSET(p_rx_info->rx_info.flags, CTC_PKT_FLAG_UNKOWN_MACDA));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "stk_hdr_len", p_rx_info->stk_hdr_len);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "From internal port",CTC_FLAG_ISSET(p_rx_info->rx_info.flags, CTC_PKT_FLAG_INTERNAl_PORT));

    if (p_rx_info->rx_info.oper_type == CTC_PKT_OPER_OAM)
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "oam info:\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "oam type", p_rx_info->rx_info.oam.type);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "mep index", p_rx_info->rx_info.oam.mep_index);
    }
    else if (p_rx_info->rx_info.oper_type == CTC_PKT_OPER_PTP)
    {

        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "timestamp info:\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %u\n", "seconds", p_rx_info->rx_info.ptp.ts.seconds);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %u\n", "nanoseconds", p_rx_info->rx_info.ptp.ts.nanoseconds);
    }

    if (CTC_PKT_CPU_REASON_DOT1AE_REACH_PN_THRD == p_rx_info->rx_info.reason)
    {
        if(CTC_FLAG_ISSET(p_rx_info->rx_info.flags, CTC_PKT_FLAG_ENCRYPTED))
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %s\n", "dot1ae type", "ENCRYPTED" );
        }
        else
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %s\n", "dot1ae type", "DECRYPTED" );
        }

    }

    return CTC_E_NONE;
}

static int32
_sys_usw_packet_dump_tx_header(uint8 lchip, ctc_pkt_tx_t  *p_tx_info, uint8 dump_mode)
{
    char* str_op_type[] = {"NORMAL", "LMTX", "E2ILOOP", "DMTX", "C2C", "PTP", "NAT", "OAM"};
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Header Raw Info(Length : %d): \n", SYS_USW_PKT_HEADER_LEN);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: 0x%x\n", "dest gport", p_tx_info->tx_info.dest_gport);

    /*source*/
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: 0x%.04x\n", "src port", p_tx_info->tx_info.src_port);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "src svid", p_tx_info->tx_info.src_svid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "src cvid", p_tx_info->tx_info.src_cvid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "src cos", p_tx_info->tx_info.src_cos);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "stag op", p_tx_info->tx_info.stag_op);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "ctag op", p_tx_info->tx_info.ctag_op);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "fid", p_tx_info->tx_info.fid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "ttl", p_tx_info->tx_info.ttl);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %s\n", "operation type", str_op_type[p_tx_info->tx_info.oper_type]);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "priority", p_tx_info->tx_info.priority);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "dest group id", p_tx_info->tx_info.dest_group_id);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "color", p_tx_info->tx_info.color);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "critical packet", p_tx_info->tx_info.is_critical);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "nexthop offset", p_tx_info->tx_info.nh_offset);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "hash ", p_tx_info->tx_info.hash);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "vlan domain", p_tx_info->tx_info.vlan_domain);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "svlan tpid", p_tx_info->tx_info.svlan_tpid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %u\n", "nexthop id", p_tx_info->tx_info.nhid);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "buffer log victim packet",CTC_FLAG_ISSET(p_tx_info->tx_info.flags, CTC_PKT_FLAG_BUFFER_VICTIM_PKT));

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "MCAST", CTC_FLAG_ISSET(p_tx_info->tx_info.flags, CTC_PKT_FLAG_MCAST));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "NH_OFFSET_BYPASS", CTC_FLAG_ISSET(p_tx_info->tx_info.flags, CTC_PKT_FLAG_NH_OFFSET_BYPASS));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "NH_OFFSET_IS_8W", CTC_FLAG_ISSET(p_tx_info->tx_info.flags, CTC_PKT_FLAG_NH_OFFSET_IS_8W));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "INGRESS_MODE", CTC_FLAG_ISSET(p_tx_info->tx_info.flags, CTC_PKT_FLAG_INGRESS_MODE));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "CANCEL_DOT1AE", CTC_FLAG_ISSET(p_tx_info->tx_info.flags, CTC_PKT_FLAG_CANCEL_DOT1AE));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "ASSIGN_DEST_PORT", CTC_FLAG_ISSET(p_tx_info->tx_info.flags, CTC_PKT_FLAG_ASSIGN_DEST_PORT));

    if (p_tx_info->tx_info.oper_type == CTC_PKT_OPER_OAM)
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "oam info:\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "oam type", p_tx_info->tx_info.oam.type);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "mep index", p_tx_info->tx_info.oam.mep_index);
    }

    if (p_tx_info->tx_info.oper_type == CTC_PKT_OPER_PTP)
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "ptp info:\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "seconds", p_tx_info->tx_info.ptp.ts.seconds);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d\n", "nanoseconds", p_tx_info->tx_info.ptp.ts.nanoseconds);
    }

    if (dump_mode)
    {
        if (p_tx_info->skb.data)
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n");
            CTC_ERROR_RETURN(_sys_usw_packet_dump(lchip, p_tx_info->skb.head, SYS_USW_PKT_HEADER_LEN));
        }
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
    }


    return CTC_E_NONE;
}

int32
_sys_usw_packet_bit62_to_ts(uint8 lchip, uint32 ns_only_format, uint64 ts_61_0, ctc_packet_ts_t* p_ts)
{
    if (ns_only_format)
    {
        /* [61:0] nano seconds */
        p_ts->seconds = ts_61_0 / 1000000000ULL;
        p_ts->nanoseconds = ts_61_0 % 1000000000ULL;
    }
    else
    {
        /* [61:30] seconds + [29:0] nano seconds */
        p_ts->seconds = (ts_61_0 >> 30) & 0xFFFFFFFFULL;
        p_ts->nanoseconds = ts_61_0 & 0x3FFFFFFFULL;
    }

    return CTC_E_NONE;
}

int32
_sys_usw_packet_ts_to_bit62(uint8 lchip, uint32 ns_only_format, ctc_packet_ts_t* p_ts, uint64* p_ts_61_0)
{
    if (ns_only_format)
    {
        /* [61:0] nano seconds */
        *p_ts_61_0 = p_ts->seconds * 1000000000ULL + p_ts->nanoseconds;
    }
    else
    {
        /* [61:30] seconds + [29:0] nano seconds */
        *p_ts_61_0 = ((uint64)(p_ts->seconds) << 30) | ((uint64)p_ts->nanoseconds);
    }

    return CTC_E_NONE;
}

int32
_sys_usw_packet_recover_timestamp(uint8 lchip, uint32 ts_36_5, uint64* p_ts_61_0)
{
    uint32 cmd = 0;
    uint32 rc_s = 0;
    uint32 rc_ns = 0;
    uint64 rc_36_5 = 0;
    uint64 rc_61_0 = 0;
    uint64 delta_36_5 = 0;
    uint64 delta_36_0 = 0;
    uint8 delta_s = 0;
    TsEngineRefRc_m ptp_ref_frc;

    /*read frc time, 32bit s + 30bit ns*/
    sal_memset(&ptp_ref_frc, 0, sizeof(ptp_ref_frc));
    cmd = DRV_IOR(TsEngineRefRc_t, DRV_ENTRY_FLAG);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &ptp_ref_frc));

    GetTsEngineRefRc(A, rcSecond_f, &ptp_ref_frc, &rc_s);
    GetTsEngineRefRc(A, rcNs_f, &ptp_ref_frc, &rc_ns);

    rc_61_0 = ((uint64)rc_s) * 1000000000ULL + rc_ns;
    rc_36_5 = (rc_61_0 >> 5) & 0xFFFFFFFF;

    if (rc_36_5 >= ts_36_5)
    {
        delta_36_5 = rc_36_5 - ts_36_5;
    }
    else
    {
        delta_36_5 = 0x100000000ULL + rc_36_5 - ts_36_5;
    }

    delta_s = (delta_36_5 << 5) / 1000000000ULL;
    if (delta_s > rc_s)
    {
        return CTC_E_INVALID_CONFIG;
    }

    rc_s = rc_s - delta_s;
    delta_36_0 = (delta_36_5 << 5) - ((uint64)delta_s) * 1000000000ULL;

    if (rc_ns >= (delta_36_0 & 0x3FFFFFFF))
    {
        rc_ns = rc_ns - (delta_36_0 & 0x3FFFFFFF);
    }
    else
    {
        rc_ns = 1000000000ULL + rc_ns - (delta_36_0 & 0x3FFFFFFF);
        if (1 > rc_s)
        {
            return CTC_E_INVALID_CONFIG;
        }
        rc_s = rc_s -1;
    }

    *p_ts_61_0 = ((uint64)rc_s) * 1000000000ULL + rc_ns;

    return CTC_E_NONE;
}

#if 0
static int32
_sys_usw_packet_rx_dump(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{
    uint32 len = 0;
    uint8 header_len = 0;
    uint8 index = 0;

    if (CTC_BMP_ISSET(p_usw_pkt_master[lchip]->rx_reason_bm, p_pkt_rx->rx_info.reason))
    {
        header_len = (p_pkt_rx->mode == CTC_PKT_MODE_ETH)?(SYS_USW_PKT_HEADER_LEN+SYS_USW_PKT_CPUMAC_LEN):SYS_USW_PKT_HEADER_LEN;
        if (p_pkt_rx->pkt_len < header_len)
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet length is too small!!!Pkt len: %d\n",p_pkt_rx->pkt_len);
            return CTC_E_INVALID_PARAM;
        }

        if (p_usw_pkt_master[lchip]->rx_header_en[p_pkt_rx->rx_info.reason])
        {
            CTC_ERROR_RETURN(_sys_usw_packet_dump_rx_header(lchip, p_pkt_rx));
        }

        if (p_pkt_rx->pkt_len - header_len < SYS_PKT_BUF_PKT_LEN)
        {
            len = p_pkt_rx->pkt_len- header_len;
        }
        else
        {
            len = SYS_PKT_BUF_PKT_LEN;
        }

        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n-----------------------------------------------\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Info(Length : %d):\n", (p_pkt_rx->pkt_len-header_len));
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
        CTC_ERROR_RETURN(_sys_usw_packet_dump(lchip, (p_pkt_rx->pkt_buf[0].data+header_len), len));

        for (index=1 ; index<p_pkt_rx->buf_count; index++)
            CTC_ERROR_RETURN(_sys_usw_packet_dump(lchip, (p_pkt_rx->pkt_buf[index].data), len));
    }

    return 0;
}

static int32
_sys_usw_packet_tx_dump(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    if(p_usw_pkt_master[lchip]->tx_dump_enable)
    {
        CTC_ERROR_RETURN(_sys_usw_packet_dump_tx_header(lchip, p_pkt_tx,1));

        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n-----------------------------------------------\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Info(Length : %d):\n", p_pkt_tx->skb.len);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
        CTC_ERROR_RETURN(_sys_usw_packet_dump(lchip, p_pkt_tx->skb.data, p_pkt_tx->skb.len));


    }
    return 0;
}
#endif

int32
_sys_usw_packet_get_svlan_tpid_index(uint8 lchip, uint16 svlan_tpid, uint8* svlan_tpid_index, sys_pkt_tx_hdr_info_t* p_tx_hdr_info)
{
    int32 ret = CTC_E_NONE;
    CTC_PTR_VALID_CHECK(p_tx_hdr_info);

    if ((0 == svlan_tpid) || (svlan_tpid == p_tx_hdr_info->tp_id[0]))
    {
        *svlan_tpid_index = 0;
    }
    else if (svlan_tpid == p_tx_hdr_info->tp_id[1])
    {
        *svlan_tpid_index = 1;
    }
    else if (svlan_tpid == p_tx_hdr_info->tp_id[2])
    {
        *svlan_tpid_index = 2;
    }
    else if (svlan_tpid == p_tx_hdr_info->tp_id[3])
    {
        *svlan_tpid_index = 3;
    }
    else
    {
        ret = CTC_E_INVALID_PARAM;
    }

    return ret;
}

STATIC int32
_sys_usw_packet_init_svlan_tpid_index(uint8 lchip)
{
    uint32 cmd = 0;
    ParserEthernetCtl_m parser_ethernet_ctl;
    int32 ret = CTC_E_NONE;

    sal_memset(&parser_ethernet_ctl, 0, sizeof(ParserEthernetCtl_m));
    cmd = DRV_IOR(ParserEthernetCtl_t, DRV_ENTRY_FLAG);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &parser_ethernet_ctl));

    p_usw_pkt_master[lchip]->tx_hdr_info.tp_id[0] = GetParserEthernetCtl(V, array_0_svlanTpid_f, &parser_ethernet_ctl);
    p_usw_pkt_master[lchip]->tx_hdr_info.tp_id[1] = GetParserEthernetCtl(V, array_1_svlanTpid_f, &parser_ethernet_ctl);
    p_usw_pkt_master[lchip]->tx_hdr_info.tp_id[2] = GetParserEthernetCtl(V, array_2_svlanTpid_f, &parser_ethernet_ctl);
    p_usw_pkt_master[lchip]->tx_hdr_info.tp_id[3] = GetParserEthernetCtl(V, array_3_svlanTpid_f, &parser_ethernet_ctl);

    return ret;
}

int32
sys_usw_packet_tx_set_property(uint8 lchip, uint16 type, uint32 value1, uint32 value2)
{
    int32 ret = CTC_E_NONE;
    SYS_PACKET_INIT_CHECK(lchip);

    SYS_PACKET_LOCK(lchip);
    switch(type)
    {
        case SYS_PKT_TX_TYPE_TPID:
            p_usw_pkt_master[lchip]->tx_hdr_info.tp_id[value1] = value2;
            break;
        case SYS_PKT_TX_TYPE_C2C_SUB_QUEUE_ID:
            p_usw_pkt_master[lchip]->tx_hdr_info.c2c_sub_queue_id = value1;
            break;
        case SYS_PKT_TX_TYPE_FWD_CPU_SUB_QUEUE_ID:
            p_usw_pkt_master[lchip]->tx_hdr_info.fwd_cpu_sub_queue_id = value1;
            break;
        case SYS_PKT_TX_TYPE_PTP_ADJUST_OFFSET:
            p_usw_pkt_master[lchip]->tx_hdr_info.offset_ns = value1;
            p_usw_pkt_master[lchip]->tx_hdr_info.offset_s = value2;
            break;
        case SYS_PKT_TX_TYPE_VLAN_PTR:
            p_usw_pkt_master[lchip]->tx_hdr_info.vlanptr[value1] = value2;
            break;
        case SYS_PKT_TX_TYPE_RSV_NH:
            p_usw_pkt_master[lchip]->tx_hdr_info.rsv_nh[value1] = value2;
            break;
        default:
            ret = CTC_E_INVALID_PARAM;
            break;

    }
    SYS_PACKET_UNLOCK(lchip);

    return ret;
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!
 Note that you should not add other module APIs in this function.
 If you need to add it, save it to the packet module database.*/
int32
_sys_usw_packet_txinfo_to_rawhdr(uint8 lchip, ctc_pkt_info_t* p_tx_info, uint32* p_raw_hdr_net, uint8 mode, uint8* data)
{
    if(MCHIP_API(lchip)->packet_txinfo_to_rawhdr)
    {
        CTC_ERROR_RETURN(MCHIP_API(lchip)->packet_txinfo_to_rawhdr(lchip, p_tx_info, p_raw_hdr_net, mode, data));
    }
    else
    {
        return CTC_E_NOT_INIT;
    }
    return CTC_E_NONE;
}


int32
_sys_usw_packet_rawhdr_to_rxinfo(uint8 lchip, uint32* p_raw_hdr_net, ctc_pkt_rx_t* p_pkt_rx)
{

    MCHIP_API(lchip)->packet_rawhdr_to_rxinfo(lchip, p_raw_hdr_net, p_pkt_rx);
    
    return CTC_E_NONE;
}

static int32
_sys_usw_packet_tx_param_check(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{
    uint32 p_nhid;
    sys_nh_info_dsnh_t nh_info;
    LCHIP_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_pkt_tx);
    CTC_PTR_VALID_CHECK(p_pkt_tx->skb.data);
    SYS_DMA_PKT_LEN_CHECK(p_pkt_tx->skb.len);
    SYS_GLOBAL_PORT_CHECK(p_pkt_tx->tx_info.src_port);
    SYS_GLOBAL_PORT_CHECK(p_pkt_tx->tx_info.dest_gport);
    if (CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_SRC_SVID_VALID))
    {
        CTC_VLAN_RANGE_CHECK(p_pkt_tx->tx_info.src_svid);
    }
    if(CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_SRC_CVID_VALID))
    {
        CTC_VLAN_RANGE_CHECK(p_pkt_tx->tx_info.src_cvid);
    }
    if (CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_ASSIGN_DEST_PORT))
    {
        uint8 gchip = 0;
        uint8 chan_id = 0;

        sys_usw_get_gchip_id(lchip, &gchip);
        chan_id = SYS_GET_CHANNEL_ID(lchip, CTC_MAP_LPORT_TO_GPORT(gchip, p_pkt_tx->tx_info.lport));
        if (0xff == chan_id)
        {
            return CTC_E_INVALID_PARAM;
        }
    }
    CTC_MAX_VALUE_CHECK(p_pkt_tx->tx_info.priority,15);
    CTC_MAX_VALUE_CHECK(p_pkt_tx->tx_info.isolated_id,31);
    CTC_MAX_VALUE_CHECK(p_pkt_tx->tx_info.color, CTC_QOS_COLOR_GREEN);
    CTC_COS_RANGE_CHECK(p_pkt_tx->tx_info.src_cos);
    //CTC_MIN_VALUE_CHECK(p_pkt_tx->tx_info.ttl, 1);
    CTC_MAX_VALUE_CHECK(p_pkt_tx->tx_info.vlan_domain,CTC_PORT_VLAN_DOMAIN_CVLAN);
    CTC_FID_RANGE_CHECK(p_pkt_tx->tx_info.fid);
    if(p_pkt_tx->tx_info.oper_type == CTC_PKT_OPER_OAM)
    {
        if (CTC_FLAG_ISSET(p_pkt_tx->tx_info.oam.flags, CTC_PKT_OAM_FLAG_IS_UP_MEP))
        {
            if (CTC_OAM_TYPE_ETH != p_pkt_tx->tx_info.oam.type)
            {
                /* only Eth OAM support UP MEP */
                return CTC_E_INVALID_PARAM;
            }
            CTC_VLAN_RANGE_CHECK(p_pkt_tx->tx_info.oam.vid);
        }

        CTC_MAX_VALUE_CHECK(p_pkt_tx->tx_info.oam.type ,CTC_OAM_TYPE_TRILL_BFD_ECHO);

        if ((CTC_FLAG_ISSET(p_pkt_tx->tx_info.oam.flags, CTC_PKT_OAM_FLAG_IS_DM)) &&
         ((1 ==(p_pkt_tx->tx_info.oam.dm_ts_offset % 2))
         || (p_pkt_tx->tx_info.oam.dm_ts_offset > p_pkt_tx->skb.len - 8)
         || (p_pkt_tx->tx_info.oam.dm_ts_offset > MCHIP_CAP(SYS_CAP_NPM_MAX_TS_OFFSET))))
        {
            return CTC_E_INVALID_PARAM;
        }

    }
    if(p_pkt_tx->tx_info.oper_type == CTC_PKT_OPER_PTP)
    {
        CTC_MAX_VALUE_CHECK(p_pkt_tx->tx_info.ptp.oper,CTC_PTP_CORRECTION);
    }
    if (CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_MCAST))
    {
        if(p_pkt_tx->tx_info.oper_type == CTC_PKT_OPER_C2C)
        {
            if(CTC_E_IN_USE != sys_usw_nh_check_glb_met_offset(lchip, p_pkt_tx->tx_info.dest_group_id, 1, TRUE))
            {
                SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "Mcast group not created\n");
                return CTC_E_INVALID_PARAM;
            }
        }
        else
        {
            if(CTC_E_NOT_EXIST == sys_usw_nh_get_mcast_nh(lchip, p_pkt_tx->tx_info.dest_group_id, &p_nhid))
            {
                return CTC_E_INVALID_PARAM;
            }
        }
    }

    if(CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_NHID_VALID))
    {
        if(0 > sys_usw_nh_get_nhinfo(lchip, p_pkt_tx->tx_info.nhid, &nh_info, 0))
        {
            return CTC_E_INVALID_PARAM;
        }
    }

    if (p_pkt_tx->mode > CTC_PKT_MODE_ETH)
    {
        return CTC_E_INVALID_PARAM;
    }

    if(CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_SESSION_ID_EN))
    {
        if ((p_pkt_tx->mode == CTC_PKT_MODE_ETH) || (p_pkt_tx->tx_info.session_id >= SYS_PKT_MAX_TX_SESSION))
        {
            return CTC_E_INVALID_PARAM;
        }
    }
    if(CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_ZERO_COPY))
    {
        if ((p_pkt_tx->mode == CTC_PKT_MODE_ETH) || (p_pkt_tx->skb.pkt_hdr_len != SYS_USW_PKT_HEADER_LEN))
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "zero copy only used for dma mode and must reserve enough packet header length\n");
            return CTC_E_INVALID_PARAM;
        }
    }
    return CTC_E_NONE;
}

static int32
_sys_usw_packet_register_tx_cb(uint8 lchip, void* tx_cb)
{
    LCHIP_CHECK(lchip);
    if (NULL == p_usw_pkt_master[lchip])
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " Feature not initialized \n");
			return CTC_E_NOT_INIT;

    }

     p_usw_pkt_master[lchip]->cfg.dma_tx_cb = (CTC_PKT_TX_CALLBACK)tx_cb;

    return CTC_E_NONE;
}

static int32
_sys_usw_packet_decap(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{
    ds_t raw_hdr;
    ds_t* p_raw_hdr = &raw_hdr;
    uint16 stk_hdr_len = 0;
    
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "len %d, buf_count %d \n", p_pkt_rx->pkt_len,
                     p_pkt_rx->buf_count);

    if (p_pkt_rx->pkt_buf[0].len < (p_pkt_rx->eth_hdr_len+ SYS_USW_PKT_HEADER_LEN))
    {
        return DRV_E_WRONG_SIZE;
    }

    p_pkt_rx->pkt_hdr_len = SYS_USW_PKT_HEADER_LEN;
    p_pkt_rx->stk_hdr_len = stk_hdr_len;
    /* 2. decode raw header */
    sal_memcpy(p_raw_hdr, p_pkt_rx->pkt_buf[0].data + p_pkt_rx->eth_hdr_len, SYS_USW_PKT_HEADER_LEN);

    /* 3. convert raw header to rx_info */
   /*When chip init, must be register the callback, so not need judge the callback is NULL. The callback must be not return error.*/
   MCHIP_API(lchip)->packet_rawhdr_to_rxinfo(lchip, (uint32*)p_raw_hdr, p_pkt_rx);

    /* add stats */
    p_usw_pkt_master[lchip]->stats.decap++;

    return CTC_E_NONE;
}

static int32
_sys_usw_packet_encap(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{
    sys_usw_cpumac_header_t* p_cpumac_hdr = NULL;
    uint32 cmd = 0;
    IpeHeaderAdjustCtl_m ipe_header_adjust_ctl;
    uint16 tpid = 0;

    CTC_PTR_VALID_CHECK(p_pkt_tx);
    LCHIP_CHECK(lchip);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n", __FUNCTION__);

    /*When chip init, must register the callback.*/
    CTC_ERROR_RETURN(MCHIP_API(lchip)->packet_txinfo_to_rawhdr(lchip, &p_pkt_tx->tx_info, (uint32*)p_pkt_tx->skb.head, p_pkt_tx->mode, p_pkt_tx->skb.data));

    if (CTC_PKT_MODE_ETH == p_pkt_tx->mode)
    {
        uint8 index = 0;
        uint8 loop;
        p_cpumac_hdr  = (sys_usw_cpumac_header_t*)(p_pkt_tx->skb.data - SYS_USW_PKT_HEADER_LEN - SYS_USW_PKT_CPUMAC_LEN);
        cmd = DRV_IOR(IpeHeaderAdjustCtl_t, DRV_ENTRY_FLAG);
        CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &ipe_header_adjust_ctl));

        tpid = GetIpeHeaderAdjustCtl(V, headerTpid_f, &ipe_header_adjust_ctl);
        for(loop=0; loop < SYS_PKT_CPU_PORT_NUM && CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_SRC_PORT_VALID); loop++)
        {
            if(p_usw_pkt_master[lchip]->gport[loop] != 0xFFFFFFFF && p_pkt_tx->tx_info.src_port == p_usw_pkt_master[lchip]->gport[loop])
            {
                index = loop;
                break;
            }
        }
        /* 2. encode CPUMAC Header */
        p_cpumac_hdr->macda[0] = p_usw_pkt_master[lchip]->cpu_mac_sa[index][0];
        p_cpumac_hdr->macda[1] = p_usw_pkt_master[lchip]->cpu_mac_sa[index][1];
        p_cpumac_hdr->macda[2] = p_usw_pkt_master[lchip]->cpu_mac_sa[index][2];
        p_cpumac_hdr->macda[3] = p_usw_pkt_master[lchip]->cpu_mac_sa[index][3];
        p_cpumac_hdr->macda[4] = p_usw_pkt_master[lchip]->cpu_mac_sa[index][4];
        p_cpumac_hdr->macda[5] = p_usw_pkt_master[lchip]->cpu_mac_sa[index][5];
        p_cpumac_hdr->macsa[0] = p_usw_pkt_master[lchip]->cpu_mac_da[index][0];
        p_cpumac_hdr->macsa[1] = p_usw_pkt_master[lchip]->cpu_mac_da[index][1];
        p_cpumac_hdr->macsa[2] = p_usw_pkt_master[lchip]->cpu_mac_da[index][2];
        p_cpumac_hdr->macsa[3] = p_usw_pkt_master[lchip]->cpu_mac_da[index][3];
        p_cpumac_hdr->macsa[4] = p_usw_pkt_master[lchip]->cpu_mac_da[index][4];
        p_cpumac_hdr->macsa[5] = p_usw_pkt_master[lchip]->cpu_mac_da[index][5];
        p_cpumac_hdr->vlan_tpid = sal_htons(tpid);
        p_cpumac_hdr->vlan_vid = sal_htons(0x23);
        p_cpumac_hdr->type = sal_htons(0x5A5A);
    }

    /* add stats */
    p_usw_pkt_master[lchip]->stats.encap++;

    return CTC_E_NONE;

}

static int32
_sys_usw_packet_tx(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{
    int32 ret =  CTC_E_NONE;

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    ret = _sys_usw_packet_tx_param_check(lchip, p_pkt_tx);
    if(ret < 0)
    {
        return ret;
    }

    /*In order to be compatible with codes before V5.5.5*/
    if(p_pkt_tx->skb.pkt_hdr_len == 0)
    {
    #ifndef CTC_PKT_SKB_BUF_DIS
        if(p_pkt_tx->skb.buf != p_pkt_tx->skb.data)
        {
            sal_memcpy(p_pkt_tx->skb.buf+CTC_PKT_HDR_ROOM, p_pkt_tx->skb.data, p_pkt_tx->skb.len);
            p_pkt_tx->skb.data = p_pkt_tx->skb.buf+CTC_PKT_HDR_ROOM;
        }
    #else
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "No skb buffer mode, must reserve packet header before skb.data\n");
        return CTC_E_INVALID_PARAM;
    #endif
    }
    p_pkt_tx->skb.head = p_pkt_tx->skb.data - SYS_USW_PKT_HEADER_LEN;
    p_pkt_tx->lchip = lchip;

    CTC_ERROR_RETURN(_sys_usw_packet_encap(lchip, p_pkt_tx));
#if 0
     _sys_usw_packet_tx_dump(lchip, p_pkt_tx);
#endif
#ifdef CTC_PKT_INBAD_EN
    if (REGISTER_API(lchip)->inband_tx)
    {
        CTC_ERROR_RETURN(REGISTER_API(lchip)->inband_tx(lchip, p_pkt_tx));
        return CTC_E_NONE;
    }
#endif

#ifndef CTC_PKT_DIS_BUF
    /*tx buf dump data store*/
    if ((p_usw_pkt_master[lchip]->cursor[CTC_EGRESS] < SYS_PKT_BUF_MAX) && (p_usw_pkt_master[lchip]->pkt_buf_en[CTC_EGRESS]== 1))
    {
        sys_pkt_tx_buf_t* p_tx_buf = NULL;
        uint8   buffer_pos = 0;
        uint8   is_pkt_found = 0;
        uint8   hdr_len = (p_pkt_tx->mode == CTC_PKT_MODE_DMA) ? SYS_USW_PKT_HEADER_LEN:(SYS_USW_PKT_HEADER_LEN+SYS_USW_PKT_CPUMAC_LEN);
        uint16   stored_len = (p_pkt_tx->skb.len +hdr_len)>SYS_PKT_BUF_PKT_LEN ? SYS_PKT_BUF_PKT_LEN:(p_pkt_tx->skb.len +hdr_len);
        p_tx_buf = (sys_pkt_tx_buf_t*)mem_malloc(MEM_SYSTEM_MODULE, sizeof(sys_pkt_tx_buf_t));
        if (p_tx_buf == NULL)
        {
            return CTC_E_NO_MEMORY;
        }
        sal_memset(p_tx_buf, 0, sizeof(sys_pkt_tx_buf_t));
        p_tx_buf->mode  = p_pkt_tx->mode;
        p_tx_buf->pkt_len = p_pkt_tx->skb.len+SYS_USW_PKT_HEADER_LEN;

        sal_time(&p_tx_buf->tm);
        sal_memcpy(&p_tx_buf->tx_info,&(p_pkt_tx->tx_info),sizeof(ctc_pkt_info_t));
        sal_memcpy(&p_tx_buf->pkt_data[0],p_pkt_tx->skb.head, stored_len);
        p_tx_buf->hash_seed = ctc_hash_caculate(stored_len-hdr_len, &p_tx_buf->pkt_data[hdr_len]);

        for (buffer_pos = 0; buffer_pos < SYS_PKT_BUF_MAX; buffer_pos++)
        {
            if ((p_tx_buf->hash_seed == p_usw_pkt_master[lchip]->tx_buf[buffer_pos].hash_seed)
                && (p_tx_buf->pkt_len ==  p_usw_pkt_master[lchip]->tx_buf[buffer_pos].pkt_len))
            {
                is_pkt_found = 1;
                break;
            }
        }

        /*update buf data*/
        if (!is_pkt_found )
        {
            buffer_pos = p_usw_pkt_master[lchip]->cursor[CTC_EGRESS];
            if(buffer_pos < SYS_PKT_BUF_MAX)
            {
                p_usw_pkt_master[lchip]->cursor[CTC_EGRESS]++;
            }
        }
        sal_memcpy(&p_usw_pkt_master[lchip]->tx_buf[buffer_pos] ,p_tx_buf , sizeof(sys_pkt_tx_buf_t));

        mem_free(p_tx_buf);

    }
#endif

    if (CTC_PKT_MODE_ETH == p_pkt_tx->mode)
    {
        if (p_usw_pkt_master[lchip]->cfg.socket_tx_cb )
        {
            /*In order to be compatible with codes before V5.5.5*/
            p_pkt_tx->skb.data  = p_pkt_tx->skb.data - SYS_USW_PKT_HEADER_LEN - SYS_USW_PKT_CPUMAC_LEN;
            p_pkt_tx->skb.len = p_pkt_tx->skb.len + SYS_USW_PKT_HEADER_LEN + SYS_USW_PKT_CPUMAC_LEN;
            CTC_ERROR_RETURN(p_usw_pkt_master[lchip]->cfg.socket_tx_cb(p_pkt_tx));
        }
        else
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Do not support Socket Tx!\n");
            return CTC_E_NOT_SUPPORT;
        }
    }
    else if (CTC_PKT_MODE_DMA == p_pkt_tx->mode)
    {
        if (p_usw_pkt_master[lchip]->cfg.dma_tx_cb )
        {
            CTC_ERROR_RETURN(p_usw_pkt_master[lchip]->cfg.dma_tx_cb(p_pkt_tx));
        }
        else
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Do not support Dma Tx!\n");
            return CTC_E_NOT_SUPPORT;
        }
    }

    if (CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_SESSION_ID_EN))
    {
        CTC_BMP_SET(p_usw_pkt_master[lchip]->session_bm, p_pkt_tx->tx_info.session_id);
    }

    if (CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_MCAST))
    {
         p_usw_pkt_master[lchip]->stats.mc_tx++;
    }
    else
    {
         p_usw_pkt_master[lchip]->stats.uc_tx++;
    }

    return CTC_E_NONE;
}

static int32
_sys_usw_packet_rx(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{
    int32 ret = 0;
    uint8 lchip_master = 0;

    lchip_master = sys_usw_chip_get_rchain_en()?0:lchip;

    ret = (_sys_usw_packet_decap(lchip, p_pkt_rx));


    if (p_pkt_rx->rx_info.reason < CTC_PKT_CPU_REASON_MAX_COUNT)
    {
        p_usw_pkt_master[lchip_master]->stats.rx[p_pkt_rx->rx_info.reason]++;
    }
    
#if 0
    _sys_usw_packet_rx_dump(lchip, p_pkt_rx);
#endif 


#ifndef CTC_PKT_DIS_OAM
    if ((p_pkt_rx->rx_info.reason - CTC_PKT_CPU_REASON_OAM) == CTC_OAM_EXCP_LEARNING_BFD_TO_CPU
        || (CTC_OAM_EXCP_BFD_TIMER_NEGOTIATION == (p_pkt_rx->rx_info.reason - CTC_PKT_CPU_REASON_OAM)))
    {
        if (p_usw_pkt_master[lchip_master]->oam_rx_cb)
        {
            p_usw_pkt_master[lchip_master]->oam_rx_cb(lchip_master, p_pkt_rx);
        }
    }
#endif

#ifndef CTC_PKT_DIS_BUF
    if ((p_usw_pkt_master[lchip_master]->cursor[CTC_INGRESS]  < SYS_PKT_BUF_MAX)
        && (p_usw_pkt_master[lchip_master]->pkt_buf_en[CTC_INGRESS]== 1)
        && ((!p_usw_pkt_master[lchip_master]->rx_buf_reason_id) || (p_usw_pkt_master[lchip_master]->rx_buf_reason_id == p_pkt_rx->rx_info.reason)))
    {
        sys_pkt_buf_t* p_pkt_rx_buf = NULL;
        uint8   buffer_pos = 0;
        uint8   is_pkt_found = 0;

        p_pkt_rx_buf = (sys_pkt_buf_t*)mem_malloc(MEM_SYSTEM_MODULE, sizeof(sys_pkt_buf_t));
        if (p_pkt_rx_buf)
        {
            p_pkt_rx_buf->mode = p_pkt_rx->mode;
            p_pkt_rx_buf->pkt_len = p_pkt_rx->pkt_len;

            sal_time(&p_pkt_rx_buf->tm);

            if (p_pkt_rx->pkt_buf->len <= SYS_PKT_BUF_PKT_LEN)
            {
                sal_memcpy(p_pkt_rx_buf->pkt_data, p_pkt_rx->pkt_buf->data, p_pkt_rx->pkt_buf->len);
                p_pkt_rx_buf->hash_seed = ctc_hash_caculate(p_pkt_rx->pkt_buf->len, &p_pkt_rx_buf->pkt_data[0]);
            }
            else
            {
                sal_memcpy(p_pkt_rx_buf->pkt_data, p_pkt_rx->pkt_buf->data, SYS_PKT_BUF_PKT_LEN);
                p_pkt_rx_buf->hash_seed = ctc_hash_caculate(SYS_PKT_BUF_PKT_LEN, &p_pkt_rx_buf->pkt_data[0]);
            }

            p_pkt_rx_buf->seconds = p_pkt_rx->rx_info.ptp.ts.seconds;
            p_pkt_rx_buf->nanoseconds = p_pkt_rx->rx_info.ptp.ts.nanoseconds;

            for (buffer_pos = 0; buffer_pos < SYS_PKT_BUF_MAX; buffer_pos++)
            {
                if ((p_pkt_rx_buf->hash_seed == p_usw_pkt_master[lchip_master]->rx_buf[buffer_pos].hash_seed)
                    && (p_pkt_rx_buf->pkt_len ==  p_usw_pkt_master[lchip_master]->rx_buf[buffer_pos].pkt_len))
                {
                    is_pkt_found = 1;
                    break;
                }
            }

            /*update buf data*/
            if (!is_pkt_found )
            {
                buffer_pos = p_usw_pkt_master[lchip_master]->cursor[CTC_INGRESS];
                if(buffer_pos < SYS_PKT_BUF_MAX)
                {
                    p_usw_pkt_master[lchip_master]->cursor[CTC_INGRESS]++;
                }
            }
            sal_memcpy(&p_usw_pkt_master[lchip_master]->rx_buf[buffer_pos] ,p_pkt_rx_buf , sizeof(sys_pkt_buf_t));

            mem_free(p_pkt_rx_buf);
        }
    }
#endif

    return ret;
}

static int32
_sys_usw_packet_tx_common_dump(uint8 lchip,uint8 buf_id ,uint8 flag, uint8* p_index, uint8 header_len)
{
    char* p_time_str = NULL;
    uint32 len  = 0;
    char str_time[64] = {0};

    if (p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_len == 0)
    {
        return CTC_E_NONE;
    }
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet No.%d, Pkt len: %d\n", (*p_index)++, p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_len);

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
    p_time_str = sal_ctime(&(p_usw_pkt_master[lchip]->tx_buf[buf_id].tm));
    sal_strncpy(str_time, p_time_str, (sal_strlen(p_time_str)-1));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Timestamp:%s\n", str_time);

    if (CTC_IS_BIT_SET(flag, 1))
    {
        ctc_pkt_tx_t* p_pkt_tx = NULL;
        ctc_pkt_info_t* p_pkt_info = NULL;

        p_pkt_tx = (ctc_pkt_tx_t*)mem_malloc(MEM_SYSTEM_MODULE, sizeof(ctc_pkt_tx_t));
        if (NULL == p_pkt_tx)
        {
            return CTC_E_NO_MEMORY;
        }
        sal_memset(p_pkt_tx, 0, sizeof(ctc_pkt_tx_t));
        p_pkt_info = &(p_usw_pkt_master[lchip]->tx_buf[buf_id].tx_info);
        sal_memcpy(&(p_pkt_tx->tx_info),p_pkt_info ,sizeof(ctc_pkt_info_t));
        _sys_usw_packet_dump_tx_header(lchip, p_pkt_tx, 0);
        mem_free(p_pkt_tx);

        if (p_usw_pkt_master[lchip]->tx_buf[buf_id].mode == CTC_PKT_MODE_ETH)
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet CPU Header\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

            _sys_usw_packet_dump(lchip, p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_data, SYS_USW_PKT_CPUMAC_LEN);

            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Bridge Header\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

            _sys_usw_packet_dump(lchip, p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_data+SYS_USW_PKT_CPUMAC_LEN, SYS_USW_PKT_HEADER_LEN);
        }
        else
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Bridge Header\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

            _sys_usw_packet_dump(lchip, p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_data, SYS_USW_PKT_HEADER_LEN);
        }

    }

    if (CTC_IS_BIT_SET(flag, 0))
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Info(Length : %d):\n", (p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_len-header_len));
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

        /*print packet*/
        len = (p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_len <= SYS_PKT_BUF_PKT_LEN)?
        (p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_len-header_len): (SYS_PKT_BUF_PKT_LEN-header_len);
        _sys_usw_packet_dump(lchip, p_usw_pkt_master[lchip]->tx_buf[buf_id].pkt_data+header_len, len);
    }


    return CTC_E_NONE;
}


static int32
_sys_usw_packet_rx_common_dump(uint8 lchip,uint8 buf_id ,uint8 flag, uint8* p_index, uint8 header_len)
{
    char* p_time_str = NULL;
    uint32 len  = 0;
    char str_time[64] = {0};
    ctc_pkt_rx_t pkt_rx;
    ds_t raw_hdr;
    ctc_pkt_buf_t pkt_data_buf;

    if (p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_len == 0)
    {
        return CTC_E_NONE;
    }

    sal_memset(&pkt_rx, 0, sizeof(pkt_rx));
    sal_memset(raw_hdr, 0, sizeof(raw_hdr));
    sal_memset(&pkt_data_buf, 0, sizeof(pkt_data_buf));

    if (p_usw_pkt_master[lchip]->rx_buf[buf_id].mode == CTC_PKT_MODE_ETH)
    {
        sal_memcpy(raw_hdr, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data + SYS_USW_PKT_CPUMAC_LEN, SYS_USW_PKT_HEADER_LEN);
    }
    else
    {
        sal_memcpy(raw_hdr, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data, SYS_USW_PKT_HEADER_LEN);
    }

    /*for get stacking len in _sys_usw_packet_rawhdr_to_rxinfo*/
    pkt_rx.pkt_buf = &pkt_data_buf;
    pkt_data_buf.data = p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data;

    /* convert raw header to rx_info */
    _sys_usw_packet_rawhdr_to_rxinfo(lchip, (uint32*)&raw_hdr, &pkt_rx);

    /* none ptp packet timestamp dump should not change */
    pkt_rx.rx_info.ptp.ts.seconds = p_usw_pkt_master[lchip]->rx_buf[buf_id].seconds;
    pkt_rx.rx_info.ptp.ts.nanoseconds = p_usw_pkt_master[lchip]->rx_buf[buf_id].nanoseconds;

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet No.%d, Pkt len: %d\n", (*p_index)++, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_len);

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
    p_time_str = sal_ctime(&(p_usw_pkt_master[lchip]->rx_buf[buf_id].tm));
    sal_strncpy(str_time, p_time_str, (sal_strlen(p_time_str)-1));
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Timestamp:%s\n", str_time);

    if (CTC_IS_BIT_SET(flag, 1))
    {
        if (CTC_IS_BIT_SET(flag, 3))
        {
            uint8 buf[SYS_USW_PKT_HEADER_LEN] = {0};
            uint8 real_buf[SYS_USW_PKT_HEADER_LEN] = {0};
            if (p_usw_pkt_master[lchip]->rx_buf[buf_id].mode == CTC_PKT_MODE_ETH)
            {
                sal_memcpy(buf, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data + SYS_USW_PKT_CPUMAC_LEN, SYS_USW_PKT_HEADER_LEN);
            }
            else
            {
                sal_memcpy(buf, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data, SYS_USW_PKT_HEADER_LEN);
            }
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Header Raw Info(Length : %d): \n", SYS_USW_PKT_HEADER_LEN);
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
            CTC_ERROR_RETURN(sys_usw_dword_reverse_copy((uint32*)real_buf, (uint32*)buf, SYS_USW_PKT_HEADER_LEN/4));
            CTC_ERROR_RETURN(sys_usw_swap32((uint32*)real_buf, SYS_USW_PKT_HEADER_LEN / 4, TRUE));
            _ctc_usw_dkit_memory_show_ram_by_data_tbl_id(lchip, MsPacketHeader_t, 0, (uint32 *)real_buf, NULL, 0);
        }
        else
        {
            _sys_usw_packet_dump_rx_header(lchip, &pkt_rx);
        }

    }

    if (CTC_IS_BIT_SET(flag, 2) && pkt_rx.stk_hdr_len)
    {
        uint32 cmd = 0;
        uint32 version = 0;
        cmd = DRV_IOR(IpeHeaderAdjustCtl_t, IpeHeaderAdjustCtl_legacyCFHeaderEn_f);
        CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &version));
        version = (1 == version)? CTC_STACKING_VERSION_1_0:CTC_STACKING_VERSION_2_0;
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Stacking Header Info(Length : %d): \n", pkt_rx.stk_hdr_len);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-------------------------------------------------------------\n");
        if (pkt_rx.stk_hdr_len)
        {
            _sys_usw_packet_dump_stacking_header(lchip, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data+header_len, pkt_rx.stk_hdr_len, (uint8)version, !CTC_IS_BIT_SET(flag, 3));
        }
    }

    if (CTC_IS_BIT_SET(flag, 0))
    {

        if (p_usw_pkt_master[lchip]->rx_buf[buf_id].mode == CTC_PKT_MODE_ETH)
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet CPU Header\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

            _sys_usw_packet_dump(lchip, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data, SYS_USW_PKT_CPUMAC_LEN);

            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Bridge Header(Length : %d):\n", SYS_USW_PKT_HEADER_LEN);
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

            _sys_usw_packet_dump(lchip, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data + SYS_USW_PKT_CPUMAC_LEN, SYS_USW_PKT_HEADER_LEN);
        }
        else
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Bridge Header(Length : %d):\n", SYS_USW_PKT_HEADER_LEN);
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

            _sys_usw_packet_dump(lchip, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data, SYS_USW_PKT_HEADER_LEN);
        }
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Info(Length : %d):\n", (p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_len-header_len));
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "-----------------------------------------------\n");

        /*print packet*/
        len = (p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_len <= SYS_PKT_BUF_PKT_LEN)?
        (p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_len-header_len): (SYS_PKT_BUF_PKT_LEN-header_len);
        _sys_usw_packet_dump(lchip, p_usw_pkt_master[lchip]->rx_buf[buf_id].pkt_data+header_len, len);
    }

    return CTC_E_NONE;
}

static int32
_sys_usw_packet_timer_param_check(uint8 lchip, ctc_pkt_tx_timer_t* p_pkt_timer)
{
    Pcie0IpStatus_m pcei_status;
    uint32 cmd = 0;
    uint8 pcie_gen = 0;
    uint8 lane_num = 0;
    uint32 max_trpt = 0;
    uint32 real_rate = 0;

    if (p_pkt_timer->session_num > SYS_PKT_MAX_TX_SESSION || !p_pkt_timer->session_num || !p_pkt_timer->interval)
    {
        return CTC_E_INVALID_PARAM;
    }

    SYS_DMA_PKT_LEN_CHECK(p_pkt_timer->pkt_len);

    cmd = DRV_IOR(Pcie0IpStatus_t, DRV_ENTRY_FLAG);
    CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &pcei_status));
    pcie_gen = GetPcie0IpStatus(V, pcie0DlCurLinkSpeed_f, &pcei_status);
    lane_num = GetPcie0IpStatus(V, pcie0DlNegLinkWidth_f, &pcei_status);


    if ((pcie_gen == 2) && lane_num)
    {
        /*gen2: maxrate = 16G*92.8% x 80% x len/(len+24) x 92.8%*/
        max_trpt = 5511*(p_pkt_timer->pkt_len+40)*lane_num/(p_pkt_timer->pkt_len+40+24);
        real_rate = (p_pkt_timer->pkt_len+40)*p_pkt_timer->session_num*8/p_pkt_timer->interval;
        if (max_trpt*1000 < real_rate)
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "Exceed pcie Gen2 trpt, max_trpt:%uK, cfg_rate:%uK\n", max_trpt*1000, real_rate);
            return CTC_E_INVALID_PARAM;
        }

    }
    else if ((pcie_gen == 3) && lane_num)
    {
        /*gen3: maxrate = 16G*92.8% x 98% x len/(len+24) x 92.8%*/
        max_trpt = 6751*(p_pkt_timer->pkt_len+40)*lane_num/(p_pkt_timer->pkt_len+40+24);
        real_rate = (p_pkt_timer->pkt_len+40)*p_pkt_timer->session_num*8/p_pkt_timer->interval;
        if (max_trpt*1000 < real_rate)
        {
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "Exceed pcie Gen3 trpt, max_trpt:%uK, cfg_rate:%uK\n", max_trpt*1000, real_rate);
            return CTC_E_INVALID_PARAM;
        }
    }

    return CTC_E_NONE;
}

static int32
_sys_usw_packet_rx_register(uint8 lchip, ctc_pkt_rx_register_t* p_register, bool is_register)
{
    uint32 idx = 0;
    uint32 bmp_len = CTC_B2W_SIZE(CTC_PKT_CPU_REASON_MAX_COUNT);
    int32 ret = CTC_E_NONE;
    ctc_slistnode_t* node = NULL;
    sys_pkt_rx_cb_node_t* rx_cb_node = NULL;
    uint32 reason_temp[CTC_B2W_SIZE(CTC_PKT_CPU_REASON_MAX_COUNT)] = {0};

    CTC_PTR_VALID_CHECK(p_register);
    CTC_PTR_VALID_CHECK(p_register->cb);

    if (is_register)
    {
        if (SYS_PACKET_RX_CB_MAX_COUNT <= CTC_SLISTCOUNT(p_usw_pkt_master[lchip]->rx_cb_list))
        {
            return CTC_E_NO_RESOURCE;
        }
        /*register*/
        CTC_SLIST_LOOP(p_usw_pkt_master[lchip]->rx_cb_list, node)
        {
            rx_cb_node = (sys_pkt_rx_cb_node_t*)node;
            if (rx_cb_node->data.cb != p_register->cb)
            {
                continue;
            }
            if (!sal_memcmp(p_register->reason_bmp, reason_temp, bmp_len * sizeof(uint32))
                || rx_cb_node->is_global)
            {
                return CTC_E_EXIST;
            }
            for (idx = 0; idx < bmp_len; idx++)
            {
                rx_cb_node->data.reason_bmp[idx] |= p_register->reason_bmp[idx];
            }
            return CTC_E_NONE;
        }
        rx_cb_node = mem_malloc(MEM_SYSTEM_MODULE, sizeof(sys_pkt_rx_cb_node_t));
        if (NULL == rx_cb_node)
        {
            return CTC_E_NO_MEMORY;
        }
        sal_memset(rx_cb_node, 0, sizeof(sys_pkt_rx_cb_node_t));
        rx_cb_node->data.cb = p_register->cb;
        if (!sal_memcmp(p_register->reason_bmp, reason_temp, bmp_len * sizeof(uint32)))
        {
            rx_cb_node->is_global = 1;
        }
        else
        {
            sal_memcpy(rx_cb_node->data.reason_bmp, p_register->reason_bmp, bmp_len * sizeof(uint32));
        }
        ctc_slist_add_tail(p_usw_pkt_master[lchip]->rx_cb_list, &rx_cb_node->node);
    }
    else
    {
        /*unregister*/
        uint8 successed = 0;
        ctc_slistnode_t* next_node = NULL;
        CTC_SLIST_LOOP_DEL(p_usw_pkt_master[lchip]->rx_cb_list, node, next_node)
        {
            rx_cb_node = (sys_pkt_rx_cb_node_t*)node;
            if (rx_cb_node->data.cb != p_register->cb)
            {
                continue;
            }
            if (sal_memcmp(p_register->reason_bmp, reason_temp, bmp_len * sizeof(uint32)))
            {
                if (rx_cb_node->is_global)
                {
                    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "Unregister callback with error reason_bmp, because it is global.\n");
                    return CTC_E_INTR_INVALID_PARAM;
                }
                for (idx = 0; idx < bmp_len; idx++)
                {
                    rx_cb_node->data.reason_bmp[idx] &= (~p_register->reason_bmp[idx]);
                }
            }
            if (!sal_memcmp(p_register->reason_bmp, reason_temp, bmp_len * sizeof(uint32))
                || !sal_memcmp(rx_cb_node->data.reason_bmp, reason_temp, bmp_len * sizeof(uint32)))
            {
                ctc_slist_delete_node(p_usw_pkt_master[lchip]->rx_cb_list, &rx_cb_node->node);
                mem_free(rx_cb_node);
            }
            successed = 1;
        }
        if (!successed)
        {
            ret = CTC_E_NOT_EXIST;
        }
    }

    return ret;
}


int32
sys_usw_packet_dump_db(uint8 lchip, sal_file_t dump_db_fp,ctc_global_dump_db_t* p_dump_param)
{
    int32 ret = CTC_E_NONE;
    uint32 loop = 0;

    SYS_PACKET_INIT_CHECK(lchip);
    SYS_PACKET_LOCK(lchip);

    SYS_DUMP_DB_LOG(dump_db_fp, "\n");
    SYS_DUMP_DB_LOG(dump_db_fp, "%s\n", "# Packet");
    SYS_DUMP_DB_LOG(dump_db_fp, "%s\n", "{");
    SYS_DUMP_DB_LOG(dump_db_fp, "%s\n", "Master config:");
    SYS_DUMP_DB_LOG(dump_db_fp, "%s\n", "----------------------------------------------------------------------------------------------------------------------");
    for(loop=0; loop < SYS_PKT_CPU_PORT_NUM; loop++)
    {
        if(p_usw_pkt_master[lchip]->gport[loop] != 0xFFFFFFFF)
        {
            SYS_DUMP_DB_LOG(dump_db_fp, "Eth to CPU idx : %u\n", loop);
            SYS_DUMP_DB_LOG(dump_db_fp, "  %-30s:0x%x\n","GPORT", p_usw_pkt_master[lchip]->gport[loop]);
            SYS_DUMP_DB_LOG(dump_db_fp, "  %-30s:%-48s\n","cpu_mac_sa", sys_output_mac(p_usw_pkt_master[lchip]->cpu_mac_sa[loop]));
            SYS_DUMP_DB_LOG(dump_db_fp, "  %-30s:%-48s\n","cpu_mac_da", sys_output_mac(p_usw_pkt_master[lchip]->cpu_mac_da[loop]));
        }
    }
    SYS_DUMP_DB_LOG(dump_db_fp, "%-30s:%u\n","tx_dump_enable",p_usw_pkt_master[lchip]->tx_dump_enable);
    SYS_DUMP_DB_LOG(dump_db_fp, "%-30s:%u\n","pkt_buf_en[CTC_INGRESS]",p_usw_pkt_master[lchip]->pkt_buf_en[CTC_INGRESS]);
    SYS_DUMP_DB_LOG(dump_db_fp, "%-30s:%u\n","pkt_buf_en[CTC_EGRESS]",p_usw_pkt_master[lchip]->pkt_buf_en[CTC_EGRESS]);
    SYS_DUMP_DB_LOG(dump_db_fp, "%-30s:%u\n","rx_buf_reason_id",p_usw_pkt_master[lchip]->rx_buf_reason_id);
    SYS_DUMP_DB_LOG(dump_db_fp, "%-30s:0x","session_bm");
    for (loop = 0; loop < STS_PKT_SESSION_BLOCK_NUM; loop++)
    {
        SYS_DUMP_DB_LOG(dump_db_fp, "%x",p_usw_pkt_master[lchip]->session_bm[loop]);
    }
    SYS_DUMP_DB_LOG(dump_db_fp, "\n");
    SYS_DUMP_DB_LOG(dump_db_fp, "%s\n", "----------------------------------------------------------------------------------------------------------------------\n");
    SYS_DUMP_DB_LOG(dump_db_fp, "%s\n", "}");
    SYS_PACKET_UNLOCK(lchip);

    return ret;
}

#define ___________InnerAPI_____________


int32
sys_usw_packet_buffer_clear(uint8 lchip, uint8 bitmap)
{
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    /*rx clear*/
    SYS_PACKET_RX_LOCK(lchip);
    if (CTC_IS_BIT_SET(bitmap, 0))
    {
        sal_memset(&p_usw_pkt_master[lchip]->rx_buf, 0, sizeof(p_usw_pkt_master[lchip]->rx_buf));
        (p_usw_pkt_master[lchip]->cursor[CTC_INGRESS]) = 0;
    }
    SYS_PACKET_RX_UNLOCK(lchip);

    /*tx clear*/
    SYS_PACKET_LOCK(lchip);
    if (CTC_IS_BIT_SET(bitmap, 1))
    {
        sal_memset(&p_usw_pkt_master[lchip]->tx_buf, 0, sizeof(p_usw_pkt_master[lchip]->tx_buf));
        (p_usw_pkt_master[lchip]->cursor[CTC_EGRESS]) = 0;
    }
    SYS_PACKET_UNLOCK(lchip);

    return CTC_E_NONE;
}

/**
 @brief Display packet
*/
int32
sys_usw_packet_stats_dump(uint8 lchip)
{
    uint16 idx = 0;
    uint64 rx = 0;
    char* p_str_tmp = NULL;
    char str[40] = {0};
    uint32 reason_cnt = 0;

    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

   /*dump tx stats*/
    SYS_PACKET_LOCK(lchip);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Tx Statistics:\n");
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "------------------------------\n");
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %"PRIu64" \n", "Total TX Count", (p_usw_pkt_master[lchip]->stats.uc_tx+p_usw_pkt_master[lchip]->stats.mc_tx));

    if ((p_usw_pkt_master[lchip]->stats.uc_tx) || (p_usw_pkt_master[lchip]->stats.mc_tx))
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "--%-18s: %"PRIu64" \n", "Uc Count", p_usw_pkt_master[lchip]->stats.uc_tx);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "--%-18s: %"PRIu64" \n", "Mc Count", p_usw_pkt_master[lchip]->stats.mc_tx);
    }


    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n");
    SYS_PACKET_UNLOCK(lchip);



    /*dump rx stats*/
    SYS_PACKET_RX_LOCK(lchip);
    for (idx  = 0; idx  < CTC_PKT_CPU_REASON_MAX_COUNT; idx ++)
    {
        rx += p_usw_pkt_master[lchip]->stats.rx[idx];
    }

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Packet Rx Statistics:\n");
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "------------------------------\n");
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %"PRIu64" \n", "Total RX Count", rx);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "\n");
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s\n", "Detail RX Count");

    if (rx)
    {
        for (idx  = 0; idx  < CTC_PKT_CPU_REASON_MAX_COUNT; idx ++)
        {
            reason_cnt = p_usw_pkt_master[lchip]->stats.rx[idx];

            if (reason_cnt)
            {
                p_str_tmp = sys_usw_reason_2Str(idx);
                sal_sprintf((char*)&str, "%s%s%d%s", p_str_tmp, "(ID:", idx, ")");

                SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-20s: %d \n", (char*)&str, reason_cnt);
            }
        }
    }
    SYS_PACKET_RX_UNLOCK(lchip);
    return CTC_E_NONE;
}

int32
sys_usw_packet_stats_clear(uint8 lchip)
{
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    /*clear tx stats*/
    SYS_PACKET_LOCK(lchip);
    p_usw_pkt_master[lchip]->stats.encap = 0;
    p_usw_pkt_master[lchip]->stats.mc_tx= 0;
    p_usw_pkt_master[lchip]->stats.uc_tx= 0;
    SYS_PACKET_UNLOCK(lchip);

    /*clear rx stats*/
    SYS_PACKET_RX_LOCK(lchip);
    sal_memset(p_usw_pkt_master[lchip]->stats.rx, 0, (CTC_PKT_CPU_REASON_MAX_COUNT*sizeof(uint64)));
    p_usw_pkt_master[lchip]->stats.decap = 0;
    SYS_PACKET_RX_UNLOCK(lchip);

    return CTC_E_NONE;
}


int32
sys_usw_packet_set_rx_reason_log(uint8 lchip, uint16 reason_id, uint8 enable, uint8 is_all, uint8 is_detail)
{
    uint16 index = 0;
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    if ((reason_id >= CTC_PKT_CPU_REASON_MAX_COUNT) && !is_all)
    {
        return CTC_E_INVALID_PARAM;
    }
    SYS_PACKET_RX_LOCK(lchip);
    if (enable)
    {
        if (is_all)
        {
            for (index = 0; index < CTC_PKT_CPU_REASON_MAX_COUNT; index++)
            {
                CTC_BMP_SET(p_usw_pkt_master[lchip]->rx_reason_bm, index);
                p_usw_pkt_master[lchip]->rx_header_en[index] = is_detail;
            }
        }
        else
        {
            CTC_BMP_SET(p_usw_pkt_master[lchip]->rx_reason_bm, reason_id);
            p_usw_pkt_master[lchip]->rx_header_en[reason_id] = is_detail;
        }
    }
    else
    {
        if (is_all)
        {
            for (index = 0; index < CTC_PKT_CPU_REASON_MAX_COUNT; index++)
            {
                CTC_BMP_UNSET(p_usw_pkt_master[lchip]->rx_reason_bm, index);
                p_usw_pkt_master[lchip]->rx_header_en[index] = is_detail;
            }
        }
        else
        {
            CTC_BMP_UNSET(p_usw_pkt_master[lchip]->rx_reason_bm, reason_id);
            p_usw_pkt_master[lchip]->rx_header_en[reason_id] = is_detail;
        }
    }
    SYS_PACKET_RX_UNLOCK(lchip);
    return CTC_E_NONE;
}

int32
sys_usw_packet_register_internal_rx_cb(uint8 lchip, SYS_PKT_RX_CALLBACK oam_rx_cb)
{
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    SYS_PACKET_RX_LOCK(lchip);
    p_usw_pkt_master[lchip]->oam_rx_cb = oam_rx_cb;
    SYS_PACKET_RX_UNLOCK(lchip);
    return CTC_E_NONE;
}


int32
sys_usw_packet_rx_buffer_dump(uint8 lchip, uint8 start , uint8 end, uint8 flag, uint8 is_brief)
{
    uint8 header_len = 0;
    uint8 idx    = 0;
    uint8 cursor = 0;
    uint8 loop = 0;
    sys_pkt_buf_t* p_pkt_rx = NULL;
    char str_time[64] = {0};

    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);
    SYS_PACKET_RX_LOCK(lchip);

    cursor = p_usw_pkt_master[lchip]->cursor[CTC_INGRESS];
    /*Brief overview */
    if (is_brief)
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8s  %-8s  %-8s %-6s    %-10s  %-10s    %-32s\n",
                                                 "PKT-NO.","Pkt-len","srcPort"," lPort", "CPU reason","Pkt-type","Timestamp");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "------------------------------------------------------------------------------------------\n");
        for (loop = 0;loop < cursor ; loop++)
        {
            ctc_pkt_rx_t pkt_rx;
            ds_t raw_hdr;
            ctc_pkt_info_t* p_rx_info = NULL;
            ctc_pkt_buf_t  pkt_data_buf;

            p_pkt_rx = &p_usw_pkt_master[lchip]->rx_buf[loop];
            sal_memset(&pkt_rx, 0, sizeof(pkt_rx));
            sal_memset(raw_hdr, 0, sizeof(raw_hdr));
            sal_memset(&pkt_data_buf, 0, sizeof(pkt_data_buf));
            if (p_pkt_rx->mode == CTC_PKT_MODE_ETH)
            {
                sal_memcpy(raw_hdr, p_pkt_rx->pkt_data+SYS_USW_PKT_CPUMAC_LEN, SYS_USW_PKT_HEADER_LEN);
            }
            else
            {
                sal_memcpy(raw_hdr, p_pkt_rx->pkt_data, SYS_USW_PKT_HEADER_LEN);
            }

            /*for get stacking len in _sys_usw_packet_rawhdr_to_rxinfo*/
            pkt_rx.pkt_buf = &pkt_data_buf;
            pkt_data_buf.data = p_usw_pkt_master[lchip]->rx_buf[loop].pkt_data;

            _sys_usw_packet_rawhdr_to_rxinfo(lchip, (uint32*)&raw_hdr, &pkt_rx);
            p_rx_info = &(pkt_rx.rx_info);

            header_len = p_pkt_rx->mode?(SYS_USW_PKT_HEADER_LEN+SYS_USW_PKT_CPUMAC_LEN):SYS_USW_PKT_HEADER_LEN;
            sal_strncpy(str_time, sal_ctime(&p_pkt_rx->tm), (sal_strlen(sal_ctime(&p_pkt_rx->tm))-1));
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8d  %-8d  0x%.4x    0x%.5x  %-10d  %-11s   %-32s\n",
                loop ,p_pkt_rx->pkt_len -header_len ,p_rx_info->src_port ,p_rx_info->lport ,p_rx_info->reason ,sys_usw_get_pkt_type_desc(p_rx_info->packet_type) ,str_time );

        }
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "------------------------------------------------------------------------------------------\n");
    }
    else
    {
        for (;start <= end && start <= cursor ; start++)
        {
            CTC_BIT_SET(flag, 0);
            header_len = p_usw_pkt_master[lchip]->rx_buf[start].mode?(SYS_USW_PKT_HEADER_LEN+SYS_USW_PKT_CPUMAC_LEN):SYS_USW_PKT_HEADER_LEN;

            _sys_usw_packet_rx_common_dump(lchip, start, flag, &idx, header_len);
        }
    }

    SYS_PACKET_RX_UNLOCK(lchip);
    return CTC_E_NONE;
}

int32
sys_usw_packet_rx(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{
#ifdef CTC_PKT_RX_CB_LIST
    uint8 index_list = 0;
    uint8 index_arr = 0;
    CTC_PKT_RX_CALLBACK cb[SYS_PACKET_RX_CB_MAX_COUNT];
    ctc_slistnode_t* node = NULL;
#endif
    sys_pkt_rx_cb_node_t* rx_cb = NULL;

#ifndef CTC_HOT_PLUG_DIS
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);
 #endif
    SYS_PACKET_RX_LOCK(lchip);
    p_pkt_rx->lchip = lchip;
     _sys_usw_packet_rx(lchip, p_pkt_rx);
#ifdef CTC_PKT_RX_CB_LIST
    CTC_SLIST_LOOP(p_usw_pkt_master[lchip]->rx_cb_list, node)
    {
        rx_cb = (sys_pkt_rx_cb_node_t*)node;
        if (rx_cb->data.cb && (rx_cb->is_global || CTC_BMP_ISSET(rx_cb->data.reason_bmp, p_pkt_rx->rx_info.reason)))
        {
            cb[index_list] = rx_cb->data.cb;
            index_list++;
        }
    }
    SYS_PACKET_RX_UNLOCK(lchip);
    for(index_arr=0;index_arr<index_list;index_arr++)
    {
        SYS_PACKET_RX_CB_LOCK(0);
        cb[index_arr](p_pkt_rx);
        SYS_PACKET_RX_CB_UNLOCK(0);
    }
#else
    SYS_PACKET_RX_UNLOCK(lchip);
    rx_cb =  (sys_pkt_rx_cb_node_t*)p_usw_pkt_master[lchip]->rx_cb_list->head;
    rx_cb->data.cb(p_pkt_rx);
#endif

    return CTC_E_NONE;
}


int32
sys_usw_packet_tx_buffer_dump(uint8 lchip, uint8 start , uint8 end, uint8 flag, uint8 is_brief)
{
    uint8 header_len = 0;
    uint8 idx    = 0;
    uint8 cursor = 0;
    uint8 loop = 0;
    sys_pkt_tx_buf_t* p_pkt_tx = NULL;
    char str_time[64] = {0};

    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    SYS_PACKET_LOCK(lchip);

    cursor = p_usw_pkt_master[lchip]->cursor[CTC_EGRESS];
    /*Brief overview*/
    if (is_brief)
    {
        char* p_temp = NULL;
        uint32 dest_id = 0;

        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8s  %-8s  %-12s  %-20s %-12s  %-32s\n",
                                                 "PKT-NO.","Pkt-len","Ucast/Mcast","Dest-port/Group-id","Dsnh-offset","Timestamp");
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "------------------------------------------------------------------------------------------------\n");
        for (loop = 0;loop <cursor ; loop++)
        {
            p_pkt_tx = &p_usw_pkt_master[lchip]->tx_buf[loop];
            header_len = p_pkt_tx->mode?(SYS_USW_PKT_HEADER_LEN+SYS_USW_PKT_CPUMAC_LEN):SYS_USW_PKT_HEADER_LEN;
            sal_strncpy(str_time, sal_ctime(&p_pkt_tx->tm), (sal_strlen(sal_ctime(&p_pkt_tx->tm))-1));
            p_temp = CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_MCAST) ? "Mcast":"Ucast";
            dest_id = CTC_FLAG_ISSET(p_pkt_tx->tx_info.flags, CTC_PKT_FLAG_MCAST) ?  p_pkt_tx->tx_info.dest_group_id:p_pkt_tx->tx_info.dest_gport;
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%-8d  %-8d  %-12s  %-20d %-12d  %-32s\n",
                                                     loop ,p_pkt_tx->pkt_len -header_len ,p_temp ,dest_id ,p_pkt_tx->tx_info.nh_offset, str_time);

        }
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "------------------------------------------------------------------------------------------------\n");
    }
    else
    {
        for (;start <= end && start <= cursor ; start++)
        {
            CTC_BIT_SET(flag, 0);
            header_len = p_usw_pkt_master[lchip]->tx_buf[start].mode?(SYS_USW_PKT_HEADER_LEN+SYS_USW_PKT_CPUMAC_LEN):SYS_USW_PKT_HEADER_LEN;

            _sys_usw_packet_tx_common_dump(lchip, start, flag, &idx, header_len);
        }
    }

    SYS_PACKET_UNLOCK(lchip);

    return CTC_E_NONE;
}

int32
sys_usw_packet_tx_dump_enable(uint8 lchip, uint8 enable)
{
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    SYS_PACKET_LOCK(lchip);
    if (enable)
    {
        p_usw_pkt_master[lchip]->tx_dump_enable = 1;
    }
    else
    {
        p_usw_pkt_master[lchip]->tx_dump_enable = 0;
    }
    SYS_PACKET_UNLOCK(lchip);
    return CTC_E_NONE;
}

int32
sys_usw_packet_tx_discard_pkt(uint8 lchip, uint8 buf_id, ctc_pkt_tx_t* p_pkt_tx)
{
    uint8* p_data = NULL;
    uint32 pkt_len = 0;
    ctc_pkt_skb_t* p_skb = &(p_pkt_tx->skb);
    uint32  header_len = 0;
    sys_pkt_buf_t* p_rx_buf = NULL;

    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    if (buf_id >= SYS_PKT_BUF_MAX )
    {
        return CTC_E_INVALID_PARAM;
    }

    SYS_PACKET_RX_LOCK(lchip);

    p_rx_buf = &(p_usw_pkt_master[lchip]->rx_buf[buf_id]);
    header_len = p_rx_buf->mode?(SYS_USW_PKT_HEADER_LEN+SYS_USW_PKT_CPUMAC_LEN):SYS_USW_PKT_HEADER_LEN;
    pkt_len    = (p_rx_buf->pkt_len < header_len)?0:(p_rx_buf->pkt_len - header_len);
    if (!pkt_len)
    {
        SYS_PACKET_RX_UNLOCK(lchip);
        return CTC_E_INVALID_PARAM;
    }

    ctc_packet_skb_init(p_skb);
    p_data = ctc_packet_skb_put(p_skb, pkt_len);
    sal_memcpy(p_data,p_rx_buf->pkt_data + header_len, pkt_len);

    SYS_PACKET_RX_UNLOCK(lchip);

    SYS_PACKET_LOCK(lchip);
    CTC_ERROR_RETURN_PACKET_UNLOCK(lchip, _sys_usw_packet_tx(lchip, p_pkt_tx));
    SYS_PACKET_UNLOCK(lchip);

    return CTC_E_NONE;
}

int32
sys_usw_packet_pkt_buf_en(uint8 lchip, uint8 dir,uint16 reason_id,uint8 enable)
{
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    if (reason_id >= CTC_PKT_CPU_REASON_MAX_COUNT)
    {
        return CTC_E_INVALID_PARAM;
    }

    if (dir == CTC_EGRESS) /*tx direction*/
    {
        SYS_PACKET_LOCK(lchip);

        p_usw_pkt_master[lchip]->pkt_buf_en[CTC_EGRESS] = enable?1:0;

        SYS_PACKET_UNLOCK(lchip);
    }
    else if (dir == CTC_INGRESS) /*rx direction*/
    {
        SYS_PACKET_RX_LOCK(lchip);

        if (enable <= 1)
        {
            p_usw_pkt_master[lchip]->pkt_buf_en[CTC_INGRESS] = enable?1:0;
        }
        p_usw_pkt_master[lchip]->rx_buf_reason_id = reason_id;

        SYS_PACKET_RX_UNLOCK(lchip);
    }

    return CTC_E_NONE;
}


#define __________OuterAPI__________

int32
sys_usw_packet_rx_register(uint8 lchip, ctc_pkt_rx_register_t* p_register)
{
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    SYS_PACKET_RX_LOCK(lchip);
    CTC_ERROR_RETURN_PACKET_RX_UNLOCK(lchip, _sys_usw_packet_rx_register(lchip, p_register, TRUE));
    SYS_PACKET_RX_UNLOCK(lchip);
    return CTC_E_NONE;
}

int32
sys_usw_packet_rx_unregister(uint8 lchip, ctc_pkt_rx_register_t* p_register)
{
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    SYS_PACKET_RX_LOCK(lchip);
    CTC_ERROR_RETURN_PACKET_RX_UNLOCK(lchip, _sys_usw_packet_rx_register(lchip, p_register, FALSE));
    SYS_PACKET_RX_UNLOCK(lchip);
    return CTC_E_NONE;
}

int32
sys_usw_packet_tx_performance_test(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{
    int32 ret = 0;
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    CTC_PTR_VALID_CHECK(p_pkt_tx);
    p_pkt_tx->mode = CTC_PKT_MODE_DMA;
    p_pkt_tx->tx_info.flags = CTC_PKT_FLAG_NH_OFFSET_BYPASS;
    SYS_PACKET_LOCK(lchip);
    CTC_ERROR_GOTO(_sys_usw_packet_tx_param_check(lchip, p_pkt_tx), ret, error_end);

    CTC_ERROR_GOTO(_sys_usw_packet_encap(lchip, p_pkt_tx), ret, error_end);

    p_pkt_tx->tx_info.flags = 0xFFFFFFFF;
    if (p_usw_pkt_master[lchip]->cfg.dma_tx_cb )
    {
        CTC_ERROR_GOTO(p_usw_pkt_master[lchip]->cfg.dma_tx_cb(p_pkt_tx), ret, error_end);
    }
    else
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Do not support Dma Tx!\n");
        SYS_PACKET_UNLOCK(lchip);
        return CTC_E_NOT_SUPPORT;
    }
error_end:
    SYS_PACKET_UNLOCK(lchip);
    return ret;
}
int32
sys_usw_packet_set_tx_timer(uint8 lchip, ctc_pkt_tx_timer_t* p_pkt_timer)
{
    LCHIP_CHECK(lchip);
    if (!p_usw_pkt_master[lchip])
    {
        return CTC_E_NOT_INIT;
    }

    switch (p_pkt_timer->timer_state)
    {
        case 0:
            /*pending state, cfg packet tx timer parameter*/
            CTC_ERROR_RETURN(_sys_usw_packet_timer_param_check(lchip, p_pkt_timer));
            CTC_ERROR_RETURN(sys_usw_dma_set_packet_timer_cfg(lchip, p_pkt_timer->session_num, p_pkt_timer->interval, p_pkt_timer->pkt_len, 0));
            CTC_ERROR_RETURN(sys_usw_dma_set_chan_en(lchip, SYS_DMA_PKT_TX_TIMER_CHAN_ID, 0));
            break;

        case 1:
            /*start tx, enable timer*/
            CTC_ERROR_RETURN(sys_usw_dma_set_pkt_timer(lchip, p_pkt_timer->interval, 1));
            break;

        case 2:
            /*timer stop*/
            CTC_ERROR_RETURN(sys_usw_dma_set_pkt_timer(lchip, p_pkt_timer->interval, 0));
            break;

        case 3:
            /*destroy timer*/
            CTC_ERROR_RETURN(sys_usw_dma_set_pkt_timer(lchip, p_pkt_timer->interval, 0));
            CTC_ERROR_RETURN(sys_usw_dma_set_packet_timer_cfg(lchip, p_pkt_timer->session_num, p_pkt_timer->interval, p_pkt_timer->pkt_len, 1));
            sal_memset(p_usw_pkt_master[lchip]->session_bm, 0, sizeof(p_usw_pkt_master[lchip]->session_bm));

            break;

        default:
            return CTC_E_INVALID_PARAM;
    }

    return CTC_E_NONE;
}

void*
sys_usw_packet_tx_alloc(uint8 lchip, uint32 size)
{
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    return sys_usw_dma_tx_alloc(lchip, size);
}

void
sys_usw_packet_tx_free(uint8 lchip, void* addr)
{
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    sys_usw_dma_tx_free(lchip, addr);
    return;
}

int32
sys_usw_packet_tx(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    CTC_ERROR_RETURN(_sys_usw_packet_tx(lchip, p_pkt_tx));

    return CTC_E_NONE;
}

int32
sys_usw_packet_tx_array(uint8 lchip, ctc_pkt_tx_t **p_pkt_array, uint32 count, ctc_pkt_callback all_done_cb, void *cookie)
{
    int32 ret = CTC_E_NONE;
    uint32 pkt_idx = 0;

    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);
    CTC_PTR_VALID_CHECK(p_pkt_array);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    SYS_PACKET_LOCK(lchip);
    for(pkt_idx = 0; pkt_idx < count; pkt_idx++)
    {
        if(!p_pkt_array[pkt_idx])
        {
            SYS_PACKET_UNLOCK(lchip);
            return CTC_E_INVALID_PARAM;
        }
        if(p_pkt_array[pkt_idx]->callback)
        {
            SYS_PACKET_UNLOCK(lchip);
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "tx array not support async mode, packet->callback must be NULL\n");
            return CTC_E_INVALID_PARAM;
        }
        ret = _sys_usw_packet_tx(lchip, p_pkt_array[pkt_idx]);
        if(ret < 0)
        {
            SYS_PACKET_UNLOCK(lchip);
            SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, "tx packet error, current pkt index:%d!\n",pkt_idx);
            return ret;
        }
    }

    (all_done_cb)(*p_pkt_array ,cookie);
    SYS_PACKET_UNLOCK(lchip);
    return ret;
}

int32
sys_usw_packet_encap(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    SYS_PACKET_LOCK(lchip);
    CTC_ERROR_RETURN_PACKET_UNLOCK(lchip, _sys_usw_packet_encap(lchip, p_pkt_tx));
    SYS_PACKET_UNLOCK(lchip);
    return CTC_E_NONE;
}

int32
sys_usw_packet_decap(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    LCHIP_CHECK(lchip);
    SYS_PACKET_INIT_CHECK(lchip);

    SYS_PACKET_RX_LOCK(lchip);
    if (CTC_PKT_MODE_ETH == p_pkt_rx->mode)
    {
        p_pkt_rx->eth_hdr_len = SYS_USW_PKT_CPUMAC_LEN;
    }
    else
    {
        p_pkt_rx->eth_hdr_len = 0;
    }
    CTC_ERROR_RETURN_PACKET_RX_UNLOCK(lchip, _sys_usw_packet_decap(lchip, p_pkt_rx));   
    SYS_PACKET_RX_UNLOCK(lchip);
    return CTC_E_NONE;
}

int32
sys_usw_packet_create_netif(uint8 lchip, ctc_pkt_netif_t* p_netif)
{
    uint8 gchip = 0;
    int32 ret = CTC_E_NONE;
    dal_netif_t dal_netif = {0};
    ctc_pkt_netif_t *p_new_netif = NULL;

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);
    SYS_PACKET_INIT_CHECK(lchip);
    CTC_MAX_VALUE_CHECK(p_netif->type, CTC_PKT_NETIF_T_VLAN);
    SYS_GLOBAL_PORT_CHECK(p_netif->gport);
    if ((p_netif->type == CTC_PKT_NETIF_T_VLAN) || p_netif->vlan)
    {
        CTC_VLAN_RANGE_CHECK(p_netif->vlan);
    }

    if (!g_dal_op.handle_netif)
    {
        return CTC_E_NOT_SUPPORT;
    }
    SYS_PACKET_LOCK(lchip);
    p_new_netif = ctc_hash_lookup(p_usw_pkt_master[lchip]->netif_hash, p_netif);
    if (p_new_netif)
    {
        SYS_PACKET_UNLOCK(lchip);
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " Netif exist!\n");
        return CTC_E_EXIST;
    }

    sys_usw_get_gchip_id(lchip, &gchip);
    dal_netif.op_type = DAL_OP_CREATE;
    dal_netif.lchip = lchip;
    dal_netif.type = p_netif->type;
    dal_netif.vlan = p_netif->vlan;
    dal_netif.gport = (p_netif->type == CTC_PKT_NETIF_T_PORT) ? p_netif->gport : (gchip << CTC_LOCAL_PORT_LENGTH);
    sal_memcpy(dal_netif.mac, p_netif->mac, 6);
    sal_memcpy(dal_netif.name, p_netif->name, CTC_PKT_MAX_NETIF_NAME_LEN);

    ret = g_dal_op.handle_netif(lchip, &dal_netif);
    if (ret)
    {
        SYS_PACKET_UNLOCK(lchip);
        return CTC_E_INVALID_CONFIG;
    }

    p_new_netif = (ctc_pkt_netif_t*)mem_malloc(MEM_SYSTEM_MODULE, sizeof(ctc_pkt_netif_t));
    if (!p_new_netif)
    {
        SYS_PACKET_UNLOCK(lchip);
        return CTC_E_NO_MEMORY;
    }
    sal_memcpy(p_new_netif, p_netif, sizeof(ctc_pkt_netif_t));
    sal_memcpy(p_new_netif->name, dal_netif.name, CTC_PKT_MAX_NETIF_NAME_LEN);
    p_new_netif->vlan = (p_netif->type == CTC_PKT_NETIF_T_PORT) ? 0 : p_netif->vlan;
    p_new_netif->gport = (p_netif->type == CTC_PKT_NETIF_T_PORT) ? p_netif->gport : 0;

    ctc_hash_insert(p_usw_pkt_master[lchip]->netif_hash, p_new_netif);
    SYS_USW_REGISTER_WB_SYNC_EN(lchip, CTC_FEATURE_PACKET, SYS_WB_APPID_PACKET_SUBID_NETIF, 1);
    SYS_PACKET_UNLOCK(lchip);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Create netif Name: %s\tType: %s\tGport: 0x%x\tVlan: %d\tMac: %s\n",
        p_new_netif->name, ((p_new_netif->type == CTC_PKT_NETIF_T_PORT) ? "port" : "vlan"), p_new_netif->gport, p_new_netif->vlan, sys_output_mac(p_new_netif->mac));

    return CTC_E_NONE;
}

int32
sys_usw_packet_destory_netif(uint8 lchip, ctc_pkt_netif_t* p_netif)
{
    int32 ret = CTC_E_NONE;
    dal_netif_t dal_netif = {0};
    ctc_pkt_netif_t* p_del_netif = NULL;

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);

    CTC_MAX_VALUE_CHECK(p_netif->type, CTC_PKT_NETIF_T_VLAN);
    SYS_GLOBAL_PORT_CHECK(p_netif->gport);
    if ((p_netif->type == CTC_PKT_NETIF_T_VLAN) || p_netif->vlan)
    {
        CTC_VLAN_RANGE_CHECK(p_netif->vlan);
    }

    if (!g_dal_op.handle_netif)
    {
        return CTC_E_NOT_SUPPORT;
    }

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Remove netif Type: %s\tGport: 0x%x\tvlan: %d\n",
        ((p_netif->type == CTC_PKT_NETIF_T_PORT) ? "port" : "vlan"), p_netif->gport, p_netif->vlan);

    dal_netif.op_type = DAL_OP_DESTORY;
    dal_netif.type = p_netif->type;
    dal_netif.gport = p_netif->gport;
    dal_netif.vlan = p_netif->vlan;

    ret = g_dal_op.handle_netif(lchip, &dal_netif);
    if (ret)
    {
        return CTC_E_INVALID_CONFIG;
    }
    SYS_PACKET_LOCK(lchip);
    p_del_netif = ctc_hash_remove(p_usw_pkt_master[lchip]->netif_hash, p_netif);
    SYS_USW_REGISTER_WB_SYNC_EN(lchip, CTC_FEATURE_PACKET, SYS_WB_APPID_PACKET_SUBID_NETIF, 1);
    SYS_PACKET_UNLOCK(lchip);
    if (p_del_netif)
    {
        mem_free(p_del_netif);
    }

    return CTC_E_NONE;
}

int32
sys_usw_packet_get_netif(uint8 lchip, ctc_pkt_netif_t* p_netif)
{
    int32 ret = CTC_E_NONE;
    dal_netif_t dal_netif = {0};
    ctc_pkt_netif_t* p_get_netif = NULL;

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_FUNC, "%s()\n",__FUNCTION__);

    CTC_MAX_VALUE_CHECK(p_netif->type, CTC_PKT_NETIF_T_VLAN);
    SYS_GLOBAL_PORT_CHECK(p_netif->gport);
    if ((p_netif->type == CTC_PKT_NETIF_T_VLAN) || p_netif->vlan)
    {
        CTC_VLAN_RANGE_CHECK(p_netif->vlan);
    }

    if (!g_dal_op.handle_netif)
    {
        return CTC_E_NOT_SUPPORT;
    }

    dal_netif.op_type = DAL_OP_GET;
    dal_netif.type = p_netif->type;
    dal_netif.gport = p_netif->gport;
    dal_netif.vlan = p_netif->vlan;

    ret = g_dal_op.handle_netif(lchip, &dal_netif);
    if (ret)
    {
        return CTC_E_NOT_EXIST;
    }

    p_get_netif = ctc_hash_lookup(p_usw_pkt_master[lchip]->netif_hash, p_netif);
    if (!p_get_netif)
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " Entry not found \n");
        return CTC_E_NOT_EXIST;
    }
    sal_memcpy(p_netif, p_get_netif, sizeof(ctc_pkt_netif_t));

    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_INFO, "Get netif Name: %s\tType: %s\tGport: 0x%x\tVlan: %d\tMac: %s\n",
        p_netif->name, ((p_netif->type == CTC_PKT_NETIF_T_PORT) ? "port" : "vlan"), p_netif->gport, p_netif->vlan, sys_output_mac(p_netif->mac));

    return CTC_E_NONE;
}

int32
_sys_usw_packet_show_netif_info(ctc_pkt_netif_t* p_netif, void* data)
{
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%8s\t%s\t0x%08x\t%s\n",
        p_netif->name, ((p_netif->type == CTC_PKT_NETIF_T_PORT) ? "port" : "vlan"),
        ((p_netif->type == CTC_PKT_NETIF_T_PORT) ? p_netif->gport : p_netif->vlan), sys_output_mac(p_netif->mac));

    return CTC_E_NONE;
}

int32
sys_usw_packet_netif_show(uint8 lchip)
{
    if (!g_dal_op.handle_netif)
    {
        return CTC_E_NOT_SUPPORT;
    }
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "Netif count: %d\n", p_usw_pkt_master[lchip]->netif_hash->count);
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "%8s\t%s\t%10s\t%14s\n", "Name", "Type", "Gport/Vlan", "Mac");
    SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "--------------------------------------------------------------\n");
    ctc_hash_traverse_through(p_usw_pkt_master[lchip]->netif_hash, (hash_traversal_fn)_sys_usw_packet_show_netif_info, NULL);

    return CTC_E_NONE;
}

int32
_sys_usw_packet_wb_sync_netif_func(ctc_pkt_netif_t* p_netif, void *user_data)
{
    uint16 max_entry_cnt = 0;
    sys_wb_pkt_netif_t  *p_wb_netif;
    sys_traverse_t *data = (sys_traverse_t *)user_data;
    ctc_wb_data_t *wb_data = (ctc_wb_data_t *)(data->data);

    max_entry_cnt =  wb_data->buffer_len / (wb_data->key_len + wb_data->data_len);

    p_wb_netif = (sys_wb_pkt_netif_t *)wb_data->buffer + wb_data->valid_cnt;

    p_wb_netif->type = p_netif->type;
    p_wb_netif->rsv = 0;
    p_wb_netif->vlan = p_netif->vlan;
    p_wb_netif->gport = p_netif->gport;
    sal_memcpy(p_wb_netif->mac, p_netif->mac, sizeof(mac_addr_t));
    sal_memcpy(p_wb_netif->name, p_netif->name, CTC_PKT_MAX_NETIF_NAME_LEN);

    if (++wb_data->valid_cnt == max_entry_cnt)
    {
        CTC_ERROR_RETURN(ctc_wb_add_entry(wb_data));
        wb_data->valid_cnt = 0;
    }

    return CTC_E_NONE;
}


int32
sys_usw_packet_wb_sync(uint8 lchip, uint32 app_id)
{
    int32 ret = CTC_E_NONE;
    ctc_wb_data_t wb_data;
    sys_traverse_t pkt_hash_data;
    sys_wb_pkt_master_t  *p_wb_pkt_master;

    /*syncup packet matser*/
    CTC_WB_ALLOC_BUFFER(&wb_data.buffer);
    SYS_PACKET_LOCK(lchip);
    if (app_id == 0 || CTC_WB_SUBID(app_id) == SYS_WB_APPID_PACKET_SUBID_MASTER)
    {
        CTC_WB_INIT_DATA_T((&wb_data), sys_wb_pkt_master_t, CTC_FEATURE_PACKET, SYS_WB_APPID_PACKET_SUBID_MASTER);

        p_wb_pkt_master = (sys_wb_pkt_master_t  *)wb_data.buffer;

        p_wb_pkt_master->lchip = lchip;
        p_wb_pkt_master->version = SYS_WB_VERSION_PACKET;

        wb_data.valid_cnt = 1;
        CTC_ERROR_GOTO(ctc_wb_add_entry(&wb_data), ret, done);
    }
    if (app_id == 0 || CTC_WB_SUBID(app_id) == SYS_WB_APPID_PACKET_SUBID_NETIF)
    {
        /*syncup netif*/
        CTC_WB_INIT_DATA_T((&wb_data), sys_wb_pkt_netif_t, CTC_FEATURE_PACKET, SYS_WB_APPID_PACKET_SUBID_NETIF);
        pkt_hash_data.data = &wb_data;
        pkt_hash_data.value1 = lchip;

        CTC_ERROR_GOTO(ctc_hash_traverse(p_usw_pkt_master[lchip]->netif_hash, (hash_traversal_fn) _sys_usw_packet_wb_sync_netif_func, (void *)&pkt_hash_data), ret, done);
        if (wb_data.valid_cnt > 0)
        {
            CTC_ERROR_GOTO(ctc_wb_add_entry(&wb_data), ret, done);
            wb_data.valid_cnt = 0;
        }
    }

    done:
    SYS_PACKET_UNLOCK(lchip);
    CTC_WB_FREE_BUFFER(wb_data.buffer);

    return ret;
}

int32
sys_usw_packet_wb_restore(uint8 lchip)
{
    int32 ret = CTC_E_NONE;
    uint16 entry_cnt = 0;
    ctc_wb_query_t wb_query;
    sys_wb_pkt_master_t  wb_pkt_master = {0};
    sys_wb_pkt_netif_t  wb_netif;
    ctc_pkt_netif_t  *p_netif;

    CTC_WB_ALLOC_BUFFER(&wb_query.buffer);

    CTC_WB_INIT_QUERY_T((&wb_query),sys_wb_pkt_master_t, CTC_FEATURE_PACKET, SYS_WB_APPID_PACKET_SUBID_MASTER);

    CTC_ERROR_GOTO(ctc_wb_query_entry(&wb_query), ret, done);

    /*restore  packet master*/
    if (wb_query.valid_cnt != 1 || wb_query.is_end != 1)
    {
        CTC_WB_DBG_OUT(CTC_DEBUG_LEVEL_DUMP, "query packet master error! valid_cnt: %d, is_end: %d.\n", wb_query.valid_cnt, wb_query.is_end);
        ret = CTC_E_NONE;
        goto done;
    }

    sal_memcpy((uint8*)&wb_pkt_master, (uint8*)wb_query.buffer, wb_query.key_len + wb_query.data_len);

    if (CTC_WB_VERSION_CHECK(SYS_WB_VERSION_PACKET, wb_pkt_master.version))
    {
        ret = CTC_E_VERSION_MISMATCH;
        goto done;
    }

    /*restore  netif*/
    CTC_WB_INIT_QUERY_T((&wb_query),sys_wb_pkt_netif_t, CTC_FEATURE_PACKET, SYS_WB_APPID_PACKET_SUBID_NETIF);

    CTC_WB_QUERY_ENTRY_BEGIN((&wb_query));
        sal_memcpy((uint8*)&wb_netif, (uint8*)(wb_query.buffer) + entry_cnt * (wb_query.key_len + wb_query.data_len), wb_query.key_len + wb_query.data_len);
        entry_cnt++;

        p_netif = mem_malloc(MEM_SYSTEM_MODULE,  sizeof(ctc_pkt_netif_t));
        if (NULL == p_netif)
        {
            ret = CTC_E_NO_MEMORY;

            goto done;
        }
        sal_memset(p_netif, 0, sizeof(ctc_pkt_netif_t));

        p_netif->type = wb_netif.type;
        p_netif->vlan = wb_netif.vlan;
        p_netif->gport = wb_netif.gport;
        sal_memcpy(p_netif->mac, wb_netif.mac, sizeof(mac_addr_t));
        sal_memcpy(p_netif->name, wb_netif.name, CTC_PKT_MAX_NETIF_NAME_LEN);

        sys_usw_packet_create_netif(lchip, p_netif);
    CTC_WB_QUERY_ENTRY_END((&wb_query));
done:
   CTC_WB_FREE_BUFFER(wb_query.buffer);

    return ret;
}

int32
sys_usw_packet_init(uint8 lchip, void* p_global_cfg)
{
    ctc_pkt_global_cfg_t* p_pkt_cfg = (ctc_pkt_global_cfg_t*)p_global_cfg;
    mac_addr_t mac_sa = {0xFE, 0xFD, 0x0, 0x0, 0x0, 0x1};
    mac_addr_t mac_da = {0xFE, 0xFD, 0x0, 0x0, 0x0, 0x0};
    uint32 ds_nh_offset = 0;
    uint8 sub_queue_id = 0;
    int32 ret = CTC_E_NONE;
    uint8 enable = 0;
    uint8 cpu_eth_chan;
    uint8 gchip;

    LCHIP_CHECK(lchip);
    /* 2. allocate interrupt master */
    if (p_usw_pkt_master[lchip])
    {
        return CTC_E_NONE;
    }

    p_usw_pkt_master[lchip] = (sys_pkt_master_t*)mem_malloc(MEM_SYSTEM_MODULE, sizeof(sys_pkt_master_t));
    if (NULL == p_usw_pkt_master[lchip])
    {
        SYS_PACKET_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " No memory \n");
        return CTC_E_NO_MEMORY;

    }

    sal_memset(p_usw_pkt_master[lchip], 0, sizeof(sys_pkt_master_t));
    sal_memset(p_usw_pkt_master[lchip]->gport, 0xFF, sizeof(uint32)*SYS_PKT_CPU_PORT_NUM);
    sal_memcpy(&p_usw_pkt_master[lchip]->cfg, p_pkt_cfg, sizeof(ctc_pkt_global_cfg_t));

    sal_memcpy(p_usw_pkt_master[lchip]->cpu_mac_sa, mac_sa, sizeof(mac_sa));
    sal_memcpy(p_usw_pkt_master[lchip]->cpu_mac_da, mac_da, sizeof(mac_da));

#ifndef PACKET_TX_USE_SPINLOCK
    sal_mutex_create(&p_usw_pkt_master[lchip]->mutex);
#else
    sal_spinlock_create((sal_spinlock_t**)&p_usw_pkt_master[lchip]->mutex);
#endif
    if (NULL == p_usw_pkt_master[lchip]->mutex)
    {
        ret = CTC_E_NO_MEMORY;
        goto roll_back_0;
    }

    sal_mutex_create(&p_usw_pkt_master[lchip]->mutex_rx);
    if (NULL == p_usw_pkt_master[lchip]->mutex_rx)
    {
        ret = CTC_E_NO_MEMORY;
        goto roll_back_1;
    }

    p_usw_pkt_master[lchip]->pkt_buf_en[CTC_INGRESS] = 1;
    p_usw_pkt_master[lchip]->pkt_buf_en[CTC_EGRESS] = 1;
    p_usw_pkt_master[lchip]->rx_buf_reason_id = 0;
    sys_usw_get_chip_cpu_eth_en(lchip, &enable, &cpu_eth_chan);
    if(enable)
    {
        sys_usw_get_chip_cpumac(lchip, p_usw_pkt_master[lchip]->cpu_mac_sa[0], p_usw_pkt_master[lchip]->cpu_mac_da[0]);
        sys_usw_get_gchip_id(lchip, &gchip);
        p_usw_pkt_master[lchip]->gport[0] = CTC_MAP_LPORT_TO_GPORT(gchip, sys_usw_dmps_get_lport_with_chan(lchip, cpu_eth_chan));
    }
    _sys_usw_packet_register_tx_cb(lchip, sys_usw_dma_pkt_tx);
    _sys_usw_packet_init_svlan_tpid_index(lchip);
    CTC_ERROR_GOTO(sys_usw_nh_get_resolved_offset(lchip, SYS_NH_RES_OFFSET_TYPE_BRIDGE_NH,  &ds_nh_offset), ret, roll_back_2);
    CTC_ERROR_GOTO(sys_usw_packet_tx_set_property(lchip, SYS_PKT_TX_TYPE_RSV_NH, SYS_NH_RES_OFFSET_TYPE_BRIDGE_NH, ds_nh_offset), ret, roll_back_2);
    CTC_ERROR_GOTO(sys_usw_nh_get_resolved_offset(lchip, SYS_NH_RES_OFFSET_TYPE_BYPASS_NH,  &ds_nh_offset), ret, roll_back_2);
    CTC_ERROR_GOTO(sys_usw_packet_tx_set_property(lchip, SYS_PKT_TX_TYPE_RSV_NH, SYS_NH_RES_OFFSET_TYPE_BYPASS_NH, ds_nh_offset), ret, roll_back_2);
    CTC_ERROR_GOTO(sys_usw_nh_get_resolved_offset(lchip, SYS_NH_RES_OFFSET_TYPE_MIRROR_NH,  &ds_nh_offset), ret, roll_back_2);
    CTC_ERROR_GOTO(sys_usw_packet_tx_set_property(lchip, SYS_PKT_TX_TYPE_RSV_NH, SYS_NH_RES_OFFSET_TYPE_MIRROR_NH, ds_nh_offset), ret, roll_back_2);

    CTC_ERROR_GOTO(sys_usw_get_sub_queue_id_by_cpu_reason(lchip, CTC_PKT_CPU_REASON_FWD_CPU, &sub_queue_id), ret, roll_back_2);
    CTC_ERROR_GOTO(sys_usw_packet_tx_set_property(lchip, SYS_PKT_TX_TYPE_FWD_CPU_SUB_QUEUE_ID, sub_queue_id, 0), ret, roll_back_2);
    CTC_ERROR_GOTO(sys_usw_get_sub_queue_id_by_cpu_reason(lchip, CTC_PKT_CPU_REASON_C2C_PKT, &sub_queue_id), ret, roll_back_2);
    CTC_ERROR_GOTO(sys_usw_packet_tx_set_property(lchip, SYS_PKT_TX_TYPE_C2C_SUB_QUEUE_ID, sub_queue_id, 0), ret, roll_back_2);

    if (g_dal_op.handle_netif)
    {
        p_usw_pkt_master[lchip]->netif_hash = ctc_hash_create(1, SYS_PKT_NETIF_HASH_SIZE,
                                    (hash_key_fn)_sys_usw_netif_hash_make,
                                    (hash_cmp_fn)_sys_usw_netif_hash_cmp);
        if (NULL == p_usw_pkt_master[lchip]->netif_hash)
        {
            ret = CTC_E_NO_MEMORY;
            goto roll_back_2;
        }

        sys_usw_wb_sync_register_cb(lchip, CTC_FEATURE_PACKET, sys_usw_packet_wb_sync);

        if (CTC_WB_ENABLE && (CTC_WB_STATUS(lchip) == CTC_WB_STATUS_RELOADING))
        {
            sys_usw_packet_wb_restore(lchip);
        }
    }
    if (CTC_WB_ENABLE && (CTC_WB_STATUS(lchip) == CTC_WB_STATUS_RELOADING))
    {
        uint32 cmd;
        BufferStoreCtl_m buf_stro_ctl;
        cmd = DRV_IOR(BufferStoreCtl_t, DRV_ENTRY_FLAG);
        CTC_ERROR_RETURN(DRV_IOCTL(lchip, 0, cmd, &buf_stro_ctl));
        if(1 == GetBufferStoreCtl(V, cpuRxPortModeEn_f, &buf_stro_ctl))
        {
            uint8 loop = 0;
            uint8 gchip;
            uint16 lport;
            uint32 gport;
            hw_mac_addr_t hw_mac_sa;
            hw_mac_addr_t hw_mac_da;
            mac_addr_t mac_sa;
            mac_addr_t mac_da;
            DsPacketHeaderEditTunnel_m ds;

            sys_usw_get_gchip_id(lchip, &gchip);
            cmd = DRV_IOR(DsPacketHeaderEditTunnel_t, DRV_ENTRY_FLAG);
            for(loop=0; loop < 4; loop++)
            {
                lport = GetBufferStoreCtl(V, g0_0_cpuPort_f+loop, &buf_stro_ctl);
                if(0 != loop && lport == GetBufferStoreCtl(V, g0_0_cpuPort_f, &buf_stro_ctl))
                {
                    continue;
                }
                gport = CTC_MAP_LPORT_TO_GPORT(gchip, lport);
                CTC_ERROR_RETURN(DRV_IOCTL(lchip, SYS_GET_CHANNEL_ID(lchip, gport), cmd, &ds));
                GetDsPacketHeaderEditTunnel(A, macSa_f, &ds, hw_mac_sa);
                GetDsPacketHeaderEditTunnel(A, macDa_f, &ds, hw_mac_da);
                SYS_USW_SET_USER_MAC(mac_sa, hw_mac_sa);
                SYS_USW_SET_USER_MAC(mac_da, hw_mac_da);
                sys_usw_packet_set_cpu_mac(lchip, loop, gport, mac_da, mac_sa);
            }
        }
    }
    /*dump db*/
    CTC_ERROR_GOTO(sys_usw_dump_db_register_cb(lchip, CTC_FEATURE_PACKET, sys_usw_packet_dump_db), ret, roll_back_2);

    p_usw_pkt_master[lchip]->rx_cb_list = ctc_slist_new();
    if (NULL == p_usw_pkt_master[lchip]->rx_cb_list)
    {
        ret = CTC_E_NO_MEMORY;
        goto roll_back_2;
    }
    if((0 == g_ctcs_api_en) && (0 == lchip) && (g_lchip_num > 1))
    {
        /*for packet rx channel cb, create mux*/
        ret = sal_mutex_create(&(p_usw_pkt_master[lchip]->mutex_rx_cb));
        if (ret || !(p_usw_pkt_master[lchip]->mutex_rx_cb))
        {
            SYS_DMA_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " No resource in ASIC \n");
            ret = CTC_E_NO_MEMORY;
            goto roll_back_3;
        }
    }

    if (p_usw_pkt_master[lchip]->cfg.rx_cb)
    {
        ctc_pkt_rx_register_t cb_register;
        sal_memset(&cb_register, 0, sizeof(cb_register));
        cb_register.cb = p_usw_pkt_master[lchip]->cfg.rx_cb;
        ret = _sys_usw_packet_rx_register(lchip, &cb_register, TRUE);
        if (ret < 0)
        {
            goto roll_back_4;
        }
    }
    return CTC_E_NONE;

roll_back_4:
    sal_mutex_destroy(p_usw_pkt_master[lchip]->mutex_rx_cb);

roll_back_3:
    ctc_slist_free(p_usw_pkt_master[lchip]->rx_cb_list);
roll_back_2:
    sal_mutex_destroy(p_usw_pkt_master[lchip]->mutex_rx);

roll_back_1:
#ifndef PACKET_TX_USE_SPINLOCK
    sal_mutex_destroy(p_usw_pkt_master[lchip]->mutex);
#else
    sal_spinlock_destroy((sal_spinlock_t*)p_usw_pkt_master[lchip]->mutex);
#endif

roll_back_0:
    mem_free(p_usw_pkt_master[lchip]);

    return ret;
}

static int32
_sys_usw_packet_free_netif_hash(void* node_data, void* user_data)
{
    uint8 lchip = *(uint8*)user_data;
    dal_netif_t dal_netif = {0};
    ctc_pkt_netif_t* p_del_netif = (ctc_pkt_netif_t *)node_data;

    if (!g_dal_op.handle_netif)
    {
        return CTC_E_NOT_SUPPORT;
    }

    dal_netif.op_type = DAL_OP_DESTORY;
    dal_netif.type = p_del_netif->type;
    dal_netif.gport = p_del_netif->gport;
    dal_netif.vlan = p_del_netif->vlan;

    g_dal_op.handle_netif(lchip, &dal_netif);

    mem_free(node_data);

    return CTC_E_NONE;
}

int32
sys_usw_packet_deinit(uint8 lchip)
{
    ctc_slistnode_t *node = NULL, *next_node = NULL;
    LCHIP_CHECK(lchip);
    if (NULL == p_usw_pkt_master[lchip])
    {
        return CTC_E_NONE;
    }
    CTC_SLIST_LOOP_DEL(p_usw_pkt_master[lchip]->rx_cb_list, node, next_node)
    {
        ctc_slist_delete_node(p_usw_pkt_master[lchip]->rx_cb_list, &(((sys_pkt_rx_cb_node_t*)node)->node));
        mem_free(node);
    }
    ctc_slist_free(p_usw_pkt_master[lchip]->rx_cb_list);
#ifndef PACKET_TX_USE_SPINLOCK
    sal_mutex_destroy(p_usw_pkt_master[lchip]->mutex);
#else
    sal_spinlock_destroy((sal_spinlock_t*)p_usw_pkt_master[lchip]->mutex);
#endif
    if (p_usw_pkt_master[lchip]->mutex_rx)
    {
        sal_mutex_destroy(p_usw_pkt_master[lchip]->mutex_rx);
    }

    if (p_usw_pkt_master[lchip]->mutex_rx_cb)
    {
        sal_mutex_destroy(p_usw_pkt_master[lchip]->mutex_rx_cb);
    }

    if (p_usw_pkt_master[lchip]->netif_hash)
    {
        ctc_hash_traverse(p_usw_pkt_master[lchip]->netif_hash, (hash_traversal_fn) _sys_usw_packet_free_netif_hash, (void*)&lchip);
        ctc_hash_free(p_usw_pkt_master[lchip]->netif_hash);
    }

    mem_free(p_usw_pkt_master[lchip]);

    return CTC_E_NONE;
}
int32
sys_usw_packet_set_cpu_mac(uint8 lchip, uint8 idx, uint32 gport, mac_addr_t da, mac_addr_t sa)
{
    SYS_PACKET_INIT_CHECK(lchip);
    p_usw_pkt_master[lchip]->gport[idx] = gport;
    sal_memcpy(p_usw_pkt_master[lchip]->cpu_mac_da[idx], da, sizeof(mac_addr_t));
    sal_memcpy(p_usw_pkt_master[lchip]->cpu_mac_sa[idx], sa, sizeof(mac_addr_t));
    return CTC_E_NONE;
}
