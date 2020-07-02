
/**
 @file ctc_goldengate_packet.h

 @author  Copyright (C) 2012 Centec Networks Inc.  All rights reserved.

 @date 2012-11-25

 @version v2.0
 \p
 \p The packet module is used for communication protocol packets between CPU and Goldengate.
 \p There are two transport mode for packet: DMA or Ethernet.
 \p Goldengate will encapsulate CPUMAC (20B) + Packet (40B) for Ethernet mode.
 \p Goldengate will encapsulate Packet (40B) for DMA mode.

 \S ctc_packet.h:ctc_pkt_mode_t
 \S ctc_packet.h:ctc_pkt_skb_t
 \S ctc_packet.h:ctc_pkt_info_t
 \S ctc_packet.h:ctc_pkt_oam_info_t
 \S ctc_packet.h:ctc_pkt_ptp_info_t
 \S ctc_packet.h:ctc_pkt_cpu_reason_t
 \S ctc_packet.h:ctc_pkt_tx_t
 \S ctc_packet.h:ctc_pkt_global_cfg_t

*/

#ifndef _CTC_GOLDENGATE_PACKET_H
#define _CTC_GOLDENGATE_PACKET_H
#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************
*
* Header Files
*
***************************************************************/
#include "ctc_packet.h"

/****************************************************************
*
* Defines and Macros
*
****************************************************************/

/**
 @addtogroup packet PACKET
 @{
*/

/****************************************************************************
*
* Function
*
*****************************************************************************/

/**
 @brief Send packet in DMA mode.

 @param[in] lchip    local chip id

 @param[in|out] p_pkt_tx    pointer to packet TX

 @remark  This API will call ctc_goldengate_packet_encap() internal.
          SDK will automatically encapsulate packet header if SDK is in DMA mode.

 @return CTC_E_XXX
*/
extern int32
ctc_goldengate_packet_tx(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx);

/**
 @brief Encapsulate packet header for packet from CPU to Chip

 @param[in] lchip    local chip id

 @param[in|out] p_pkt_tx    pointer to packet TX

 @remark  User need to call this API to encapsulate packet header if SDK is in ETH mode.
          SDK will automatically encapsulate packet header if SDK is in DMA mode.

 @return CTC_E_XXX
*/
extern int32
ctc_goldengate_packet_encap(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx);

/**
 @brief Decapsulate packet header for packet from Chip to CPU

 @param[in] lchip    local chip id

 @param[in|out] p_pkt_rx    pointer to packet RX

 @remark  User need to call this API to decapsulate packet header if SDK is in ETH mode.
          SDK will automatically decapsulate packet header if SDK is in DMA mode.

 @return CTC_E_XXX
*/
extern int32
ctc_goldengate_packet_decap(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx);

/**
 @brief Initialize packet module

 @param[in] lchip    local chip id

 @param[in] p_global_cfg    pointer to packet initialize parameter, refer to ctc_pkt_global_cfg_t

 @remark  User need to decide SDK's packet transport mode

 @return CTC_E_XXX
*/
extern int32
ctc_goldengate_packet_init(uint8 lchip, void* p_global_cfg);

/**
 @brief De-Initialize packet module

 @param[in] lchip    local chip id

 @remark  User can de-initialize the packet configuration

 @return CTC_E_XXX
*/
extern int32
ctc_goldengate_packet_deinit(uint8 lchip);

/**@} end of @addtogroup packet PACKET */

#ifdef __cplusplus
}
#endif

#endif

