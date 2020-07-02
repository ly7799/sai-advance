/**
 @file ctc_greatbelt_pdu.h

 @author  Copyright (C) 2011 Centec Networks Inc.  All rights reserved.

 @date 2009-12-31

 @version v2.0

 Protocol Data Unit (PDU) contains layer2 pdu and layer3 pdu. The pdu API is used to control layer2 and layer3 pdu packet forwarding action.
 Different action index control pdu packet different forwarding action.
 \b

 The pdu API can provide:
 \p
    1.Classify PDU
 \p
     Layer2 pdu:
 \p
        auto identified: BPDU, SLOW_PROTOCOL, EAPOL, LLDP, L2ISIS, EFM, FIP
 \p
        user defined based on: macda,macda-low24 bit, layer2 header protocol
 \b
     Layer3 pdu:
 \p
     auto identified: RIP, OSPF, BGP, PIM, VRRP, RSVP, MSDP, LDP
 \p
     user defined based on: layer3 header protocol,layer4 dest port.
 \b
    2.Set global action
 \p
            set action index:0-31
 \p
            Layer2 action index 0-31 can per port control four action types
 \b
    3.Set port/layer 3 interface action
 \p
            Layer2 pdu have four action types: redirect to cpu, copy to cpu, normal forwarding and discard.
 \p
            Layer3 pdu can only control copy to cpu on all layer 3 interface.
*/

#ifndef _CTC_GREATBELT_PDU_H
#define _CTC_GREATBELT_PDU_H
#ifdef __cplusplus
extern "C" {
#endif

#include "sal.h"
#include "ctc_pdu.h"
#include "ctc_parser.h"

/**********************************************************************************
                      Define API function interfaces
***********************************************************************************/
/**
 @addtogroup  pdu PDU
 @{
*/

/**
 @addtogroup l2pdu L2PDU
 @{
*/

/**
 @brief  Classify layer2 pdu based on macda,macda-low24 bit, layer2 header protocol

 @param[in] lchip    local chip id

 @param[in]  l2pdu_type  CTC_PDU_L2PDU_TYPE_XXX

 @param[in] index  classify layer2 pdu index.

 @param[in] key  layer2 pdu action key

 @remark  Classify layer2 pdu based on macda, macda-low24 bit, layer2 header protocol.
         \p
         input index:
         \p
         based on macda, max index num is 4, its mask fixed as 48bit;
         \p
         based on macda low24 bit, max index num is 10;
         \p
         based on layer2 header protocol, max index num is 16.
         \p
         Layer2 pdu: BPDU, SLOW_PROTOCOL, EAPOL, LLDP, L2ISIS, EFM, FIP auto identified in asic.
         \p
         Use CTC_PDU_L2PDU_TYPE_L2HDR_PROTO to classify layer2 pdu based on layer2 header protocol.
         \p
         Use the CTC_PDU_L2PDU_TYPE_MACDA flag to classify layer2 pdu based on macda.
         \p
         Use the CTC_PDU_L2PDU_TYPE_MACDA_LOW24 flag to classify layer2 pdu based on macda-low24 bit.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l2pdu_classify_l2pdu(uint8 lchip, ctc_pdu_l2pdu_type_t l2pdu_type, uint8 index, ctc_pdu_l2pdu_key_t* key);

/**
 @brief  Get layer2 pdu key

 @param[in] lchip    local chip id

 @param[in] l2pdu_type  CTC_PDU_L2PDU_TYPE_XXX

 @param[in] index  classify layer2 pdu index.

 @param[out] key  layer2 pdu action key

 @remark  Get layer2 pdu classified key based on macda,macda-low24 bit, layer2 header protocol.
         \p
         input index:
         \p
         based on macda, max index num is 4;
         \p
         based on macda low24 bit, max index num is 8;
         \p
         based on layer2 header protocol, max index num is 16.
         \p
         Use the CTC_PDU_L2PDU_TYPE_L2HDR_PROTO flag to get layer2 pdu key based on layer2 header protocol.
         \p
         Use the CTC_PDU_L2PDU_TYPE_MACDA flag to get layer2 pdu key based on macda.
         \p
         Use the CTC_PDU_L2PDU_TYPE_MACDA_LOW24 flag to get layer2 pdu key based on macda-low24 bit.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l2pdu_get_classified_key(uint8 lchip, ctc_pdu_l2pdu_type_t l2pdu_type, uint8 index, ctc_pdu_l2pdu_key_t* key);

/**
 @brief  Set layer2 pdu global property

 @param[in] lchip    local chip id

 @param[in] l2pdu_type  CTC_PDU_L2PDU_TYPE_XXX

 @param[in] index  classify layer2 pdu index.

 @param[in] action  layer2 pdu global property filed

 @remark  Set layer2 pdu global action (entry valid and action index) based on macda,macda-low24 bit, layer2 header protocol,layer3 type,bpdu.
         \p
         input index:
         \p
         based on macda, max index num is 4;
         \p
         based on macda, max index num is 4;
         \p
         based on macda low24 bit, max index num is 10;
         \p
         based on layer2 header protocol, max index num is 16;
         \p
         based on layer3 type, max index num is 16;
         \p
         based on bpdu, index is 1.
         \p
         Use the CTC_PDU_L2PDU_TYPE_L2HDR_PROTO flag to set layer2 pdu global action based on layer2 header protocol.
         \p
         Use the CTC_PDU_L2PDU_TYPE_MACDA flag to set layer2 pdu global action based on macda.
         \p
         Use the CTC_PDU_L2PDU_TYPE_MACDA_LOW24 flag to set layer2 pdu global action based on macda-low24 bit.
         \p
         Use the CTC_PDU_L2PDU_TYPE_L3TYPE flag to set layer2 pdu global action based on layer3 type.
         \p
         Use the CTC_PDU_L2PDU_TYPE_BPDU flag to set layer2 pdu global action based on bpdu.
         \p
         Action index  is 0-31.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l2pdu_set_global_action(uint8 lchip, ctc_pdu_l2pdu_type_t l2pdu_type, uint8 index, ctc_pdu_global_l2pdu_action_t* action);

/**
 @brief  Get layer2 pdu global property

 @param[in] lchip    local chip id

 @param[in] l2pdu_type  CTC_PDU_L2PDU_TYPE_XXX

 @param[in] index  classify layer2 pdu index.

 @param[out] action  layer2 pdu global property filed

 @remark  Get layer2 pdu global action (entry valid and action index) based on macda,macda-low24 bit, layer2 header protocol,layer3 type,bpdu.
         \p
         input index:
         \p
         based on macda, max index num is 4;
         \p
         based on macda low24 bit, max index num is 10;
         \p
         based on layer2 header protocol, max index num is 16;
         \p
         based on layer3 type, the index  is equal to l3type (CTC_PARSER_L3_TYPE_XXX);
         \p
         based on bpdu, index num is 1.
         \p
         Use the CTC_PDU_L2PDU_TYPE_L2HDR_PROTO flag to get layer2 pdu global action based on layer2 header protocol.
         \p
         Use the CTC_PDU_L2PDU_TYPE_MACDA flag to get layer2 pdu global action based on macda.
         \p
         Use the CTC_PDU_L2PDU_TYPE_MACDA_LOW24 flag to get layer2 pdu global action based on macda-low24 bit.
         \p
         Use the CTC_PDU_L2PDU_TYPE_L3TYPE flag to get layer2 pdu global action based on layer3 type, when this flag is set,mus set action->entry_valid.
         \p
         Use the CTC_PDU_L2PDU_TYPE_BPDU flag to get layer2 pdu global action based on bpdu, when this flag is set, action->entry_valid must
         be set, and don't use action->copy_to_cpu.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l2pdu_get_global_action(uint8 lchip, ctc_pdu_l2pdu_type_t l2pdu_type, uint8 index, ctc_pdu_global_l2pdu_action_t* action);

/**
 @brief  Per port control layer2 pdu action

 @param[in] lchip    local chip id

 @param[in] gport  global port

 @param[in] action_index  per port control action index, it's from action_index from ctc_pdu_global_l2pdu_action_t ds

 @param[in] action  layer2 pdu action type

 @remark  Set layer2 pdu per port action of a certain action index on port. There are four actions:
         \p
         Use the CTC_PDU_L2PDU_ACTION_TYPE_REDIRECT_TO_CPU flag to set redirect to cpu action.
         \p
         Use the CTC_PDU_L2PDU_ACTION_TYPE_COPY_TO_CPU flag to set copy to cpu action.
         \p
         Use the CTC_PDU_L2PDU_ACTION_TYPE_FWD flag to set forward action.
         \p
         Use the CTC_PDU_L2PDU_ACTION_TYPE_DISCARD flag to set discard action.
         \p
         Per port control action index is 0-31.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l2pdu_set_port_action(uint8 lchip, uint32 gport, uint8 action_index, ctc_pdu_port_l2pdu_action_t action);

/**
 @brief  Per port control layer2 pdu action

 @param[in] lchip    local chip id

 @param[in] gport  global port

 @param[in] action_index  per port control action index, it's from action_index from ctc_pdu_global_l2pdu_action_t ds

 @param[out] action  layer2 pdu action type

 @remark  Get layer2 pdu per port action of a certain action index on port. There are four actions:
          \p
          CTC_PDU_L2PDU_ACTION_TYPE_REDIRECT_TO_CPU flag : redirect to cpu action.
          \p
          CTC_PDU_L2PDU_ACTION_TYPE_COPY_TO_CPU flag : copy to cpu action.
          \p
          CTC_PDU_L2PDU_ACTION_TYPE_FWD flag : forward action.
          \p
          CTC_PDU_L2PDU_ACTION_TYPE_DISCARD flag : discard action.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l2pdu_get_port_action(uint8 lchip, uint32 gport, uint8 action_index, ctc_pdu_port_l2pdu_action_t* action);

/**@} end of @addtogroup  l2pdu L2PDU */

/**
 @addtogroup  l3pdu L3PDU
 @{
*/

/**
 @brief  Classify layer3 pdu based on layer3 header protocol, layer4 dest port

 @param[in] lchip    local chip id

 @param[in] l3pdu_type  CTC_PDU_L3PDU_TYPE_XXX

 @param[in] index  classify layer3 pdu index.

 @param[in] key  layer3 pdu action key

 @remark  Classify layer3 pdu based on layer3 header protocol,layer4 dest port.
         \p
         input index:
         \p
         based on layer3 header protocol, max index num is 8;
         \p
         based on layer4 dest port, max index num is 8.
         \p
         Layer3 pdu:RIP, OSPF, BGP, PIM, VRRP, RSVP, MSDP, LDP auto identified in asic.
         \p
         Use the CTC_PDU_L3PDU_TYPE_L3HDR_PROTO flag to classify layer3 pdu based on layer3 header protocol.
         \p
         Use the CTC_PDU_L3PDU_TYPE_LAYER4_PORT flag to classify layer3 pdu based on layer4 dest port.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l3pdu_classify_l3pdu(uint8 lchip, ctc_pdu_l3pdu_type_t l3pdu_type, uint8 index, ctc_pdu_l3pdu_key_t* key);
/**
 @brief  Get layer3 pdu classified key

 @param[in] lchip    local chip id

 @param[in] l3pdu_type  CTC_PDU_L3PDU_TYPE_XXX

 @param[in] index  classify layer3 pdu index.

 @param[out] key  layer3 pdu action key

 @remark  Get layer3 pdu key based on layer3 header protocol,layer4 dest port.
          \p
          input index:
          \p
          based on layer3 header protocol, max index num is 8;
          \p
          based on layer4 dest port, max index num is 8.
          \p
          Use the CTC_PDU_L3PDU_TYPE_L3HDR_PROTO flag to get layer3 pdu key based on layer3 header protocol.
          \p
          Use the CTC_PDU_L3PDU_TYPE_LAYER4_PORT flag to get layer3 pdu key based on layer4 dest port.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l3pdu_get_classified_key(uint8 lchip, ctc_pdu_l3pdu_type_t l3pdu_type, uint8 index, ctc_pdu_l3pdu_key_t* key);

/**
 @brief  Set layer3 pdu global property

 @param[in] lchip    local chip id

 @param[in] l3pdu_type  CTC_PDU_L3PDU_TYPE_XXX

 @param[in] index  classify layer3 pdu index.

 @param[in] action  layer3 pdu global property filed

 @remark  Set layer3 pdu global action (entry valid and action index) based on layer3 header protocol,layer4 dest port.
          \p
          input index:
          \p
          based on layer3 header protocol, max index num is 8;
          \p
          based on layer4 dest port, max index num is 8.
          \p
          Use the CTC_PDU_L3PDU_TYPE_L3HDR_PROTO flag to set layer3 pdu global action based on layer3 header protocol.
          \p
          Use the CTC_PDU_L3PDU_TYPE_LAYER4_PORT flag to set layer3 pdu global action based on layer4 dest port.
          \p
          Action index based on layer3 header protocol and layer4 dest port is 0-31.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l3pdu_set_global_action(uint8 lchip, ctc_pdu_l3pdu_type_t l3pdu_type, uint8 index, ctc_pdu_global_l3pdu_action_t* action);
/**
 @brief  Get layer3 pdu global property

 @param[in] lchip    local chip id

 @param[in] l3pdu_type  CTC_PDU_L3PDU_TYPE_XXX

 @param[in] index  classify layer3 pdu index.

 @param[out] action  layer3 pdu global property filed

 @remark  Get layer3 pdu global action (entry valid and action index) based on layer3 header protocol,layer4 dest port.
          \p
          input index:
          \p
          based on layer3 header protocol, max index num is 8;
          \p
          based on layer4 dest port, max index num is 8.
          \p
          Use the CTC_PDU_L3PDU_TYPE_L3HDR_PROTO flag to get layer3 pdu global action based on layer3 header protocol.
          \p
          Use the CTC_PDU_L3PDU_TYPE_LAYER4_PORT flag to get layer3 pdu global action based on layer4 dest port.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_l3pdu_get_global_action(uint8 lchip, ctc_pdu_l3pdu_type_t l3pdu_type, uint8 index, ctc_pdu_global_l3pdu_action_t* action);

/**@} end of @addtogroup  l3pdu L3PDU */

/**
 @brief Init pdu module

 @param[in] lchip    local chip id

 @param[in] pdu_global_cfg  Data of the pdu initialization

 @remark  Initialize the pdu module, including layer2 and layer3 pdu exception enable,cam init,etc.
          some pdu have been classify by asic automatically, as below
          Layer2 pdu:BPDU, SLOW_PROTOCOL, EAPOL, LLDP, L2ISIS
          layer3 pdu:RIP, OSPF, BGP, PIM, VRRP, RSVP, MSDP, LDP
          SDK have initialize the according action index(refer to ctc_l2pdu_action_index_t and ctc_l3pdu_action_index_t),
          but action index can be always changed by api which seting action.

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_pdu_init(uint8 lchip, void* pdu_global_cfg);

/**
 @brief De-Initialize pdu module

 @param[in] lchip    local chip id

 @remark  User can de-initialize the pdu configuration

 @return CTC_E_XXX
*/
extern int32
ctc_greatbelt_pdu_deinit(uint8 lchip);

/**@} end of @addtogroup  pdu PDU*/

#ifdef __cplusplus
}
#endif

#endif

