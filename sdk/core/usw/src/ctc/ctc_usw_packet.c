/**
 @file ctc_usw_packet.c

 @author  Copyright (C) 2012 Centec Networks Inc.  All rights reserved.

 @date 2012-11-25

 @version v2.0

 This file define ctc functions

 \p The packet module is used for communication protocol packets between CPU and Greatbelt.
 \p There are two transport mode for packet: DMA or Ethernet.
 \p Duet2 will encapsulate CPUMAC (20B) + Packet (40B) for Ethernet mode.
 \p Duet2 will encapsulate Packet (40B) for DMA mode.

*/
/****************************************************************************
*
* Header Files
*
****************************************************************************/
#include "ctc_error.h"
#include "ctc_packet.h"
#include "ctc_usw_packet.h"
#include "sys_usw_packet.h"
#include "sys_usw_common.h"

/****************************************************************************
*
* Defines and Macros
*
*****************************************************************************/

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

int32
ctc_usw_packet_tx_alloc(uint8 lchip, uint32 size, void** addr)
{
    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    *addr = sys_usw_packet_tx_alloc(lchip, size);
    if(*addr != NULL)
    {
        return CTC_E_NONE;
    }
    else
    {
        return CTC_E_NO_MEMORY;
    }

}

int32
ctc_usw_packet_tx_free(uint8 lchip, void* addr)
{
    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);

    sys_usw_packet_tx_free(lchip, addr);
    return CTC_E_NONE;
}

int32
ctc_usw_packet_tx(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{
    CTC_PTR_VALID_CHECK(p_pkt_tx);
    CTC_SET_API_LCHIP(lchip, p_pkt_tx->lchip);

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_usw_packet_tx(lchip, p_pkt_tx));

    return CTC_E_NONE;
}

int32
ctc_usw_packet_tx_array(uint8 lchip, ctc_pkt_tx_t** p_pkt_array, uint32 count, ctc_pkt_callback all_done_cb, void *cookie)
{
    CTC_PTR_VALID_CHECK(p_pkt_array);

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_usw_packet_tx_array(lchip, p_pkt_array, count, all_done_cb, cookie));

    return CTC_E_NONE;
}

int32
ctc_usw_packet_encap(uint8 lchip, ctc_pkt_tx_t* p_pkt_tx)
{


    CTC_PTR_VALID_CHECK(p_pkt_tx);
    CTC_SET_API_LCHIP(lchip, p_pkt_tx->lchip);
    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_usw_packet_encap(lchip, p_pkt_tx));

    return CTC_E_NONE;
}

int32
ctc_usw_packet_decap(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_usw_packet_decap(lchip, p_pkt_rx));

    return CTC_E_NONE;
}

int32
ctc_usw_packet_init(uint8 lchip, void* p_global_cfg)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_usw_packet_init(lchip, p_global_cfg));
    }

    return CTC_E_NONE;
}

int32
ctc_usw_packet_deinit(uint8 lchip)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_usw_packet_deinit(lchip));
    }

    return CTC_E_NONE;
}

int32
ctc_usw_packet_set_tx_timer(uint8 lchip_id, ctc_pkt_tx_timer_t* p_pkt_timer)
{
    uint8 lchip = lchip_id;
    CTC_PTR_VALID_CHECK(p_pkt_timer);

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_usw_packet_set_tx_timer(lchip, p_pkt_timer));

    return CTC_E_NONE;
}

int32
ctc_usw_packet_create_netif(uint8 lchip_id, ctc_pkt_netif_t* p_netif)
{
    uint8 lchip = lchip_id;
    CTC_PTR_VALID_CHECK(p_netif);

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_usw_packet_create_netif(lchip, p_netif));

    return CTC_E_NONE;
}

int32
ctc_usw_packet_destory_netif(uint8 lchip_id, ctc_pkt_netif_t* p_netif)
{
    uint8 lchip = lchip_id;
    CTC_PTR_VALID_CHECK(p_netif);

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_usw_packet_destory_netif(lchip, p_netif));

    return CTC_E_NONE;
}

int32
ctc_usw_packet_get_netif(uint8 lchip_id, ctc_pkt_netif_t* p_netif)
{
    uint8 lchip = lchip_id;
    CTC_PTR_VALID_CHECK(p_netif);

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_ERROR_RETURN(sys_usw_packet_get_netif(lchip, p_netif));

    return CTC_E_NONE;
}

int32
ctc_usw_packet_rx_register(uint8 lchip, ctc_pkt_rx_register_t* p_register)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_usw_packet_rx_register(lchip, p_register));
    }
    return CTC_E_NONE;
}

int32
ctc_usw_packet_rx_unregister(uint8 lchip, ctc_pkt_rx_register_t* p_register)
{
    uint8 lchip_start              = 0;
    uint8 lchip_end                = 0;

    FEATURE_SUPPORT_CHECK(CTC_FEATURE_PACKET);
    LCHIP_CHECK(lchip);
    CTC_FOREACH_LCHIP(lchip_start, lchip_end, 1)
    {
        CTC_ERROR_RETURN(sys_usw_packet_rx_unregister(lchip, p_register));
    }
    return CTC_E_NONE;
}



