/**
 @file sys_greatbelt_oam.h

 @date 2010-1-19

 @version v2.0

  The file defines Macro, stored data structure for  Ethernet OAM module
*/
#ifndef _SYS_GREATBELT_OAM_H
#define _SYS_GREATBELT_OAM_H
#ifdef __cplusplus
extern "C" {
#endif
/****************************************************************************
 *
 * Header Files
 *
****************************************************************************/

#include "ctc_oam.h"
#include "ctc_interrupt.h"
/****************************************************************************
*
* Defines and Macros
*
****************************************************************************/

/****************************************************************************
 *
* Function
*
****************************************************************************/
/**
 @brief  sys layer oam master init
*/
extern int32
sys_greatbelt_oam_add_maid(uint8 lchip, ctc_oam_maid_t* p_maid);

extern int32
sys_greatbelt_oam_remove_maid(uint8 lchip, ctc_oam_maid_t* p_maid);

extern int32
sys_greatbelt_oam_add_lmep(uint8 lchip, ctc_oam_lmep_t* p_lmep);

extern int32
sys_greatbelt_oam_remove_lmep(uint8 lchip, ctc_oam_lmep_t* p_lmep);

extern int32
sys_greatbelt_oam_update_lmep(uint8 lchip, ctc_oam_update_t* p_lmep);

extern int32
sys_greatbelt_oam_add_rmep(uint8 lchip, ctc_oam_rmep_t* p_rmep);

extern int32
sys_greatbelt_oam_remove_rmep(uint8 lchip, ctc_oam_rmep_t* p_rmep);

extern int32
sys_greatbelt_oam_update_rmep(uint8 lchip, ctc_oam_update_t* p_rmep);

extern int32
sys_greatbelt_oam_add_mip(uint8 lchip, ctc_oam_mip_t* p_mip);

extern int32
sys_greatbelt_oam_remove_mip(uint8 lchip, ctc_oam_mip_t* p_mip);

extern int32
sys_greatbelt_oam_set_property(uint8 lchip, ctc_oam_property_t* p_prop);

extern int32
sys_greatbelt_oam_get_defect_info(uint8 lchip, void* p_defect_info);

extern int32
sys_greatbelt_oam_get_mep_info(uint8 lchip,  void* p_mep_info, uint8 with_key);

extern int32
sys_greatbelt_oam_get_stats_info(uint8 lchip, ctc_oam_stats_info_t* p_stat_info);

extern int32
sys_greatbelt_oam_set_defect_cb(uint8 lchip, CTC_INTERRUPT_EVENT_FUNC cb);

extern int32
sys_greatbelt_oam_init(uint8 lchip, ctc_oam_global_t* p_cfg);

/**
 @brief De-Initialize oam module
*/
extern int32
sys_greatbelt_oam_deinit(uint8 lchip);

extern int32
sys_greatbelt_oam_set_trpt_cfg(uint8 lchip, ctc_oam_trpt_t* p_trpt);

extern int32
sys_greatbelt_oam_set_trpt_en(uint8 lchip, uint8 gchip, uint8 session_id, uint8 enable);

extern int32
sys_greatbelt_oam_get_trpt_stats(uint8 lchip, uint8 gchip, uint8 session_id, ctc_oam_trpt_stats_t* p_trpt_stats);

extern int32
sys_greatbelt_oam_clear_trpt_stats(uint8 lchip, uint8 gchip, uint8 session_id);

extern bool
sys_greatbelt_oam_bfd_init(uint8 lchip);

#ifdef __cplusplus
}
#endif

#endif

