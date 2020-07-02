/**
 *  @file ctc_greatbel_vlan_cli.h
 *
 *   @date 2011-11-25
 *
 *    @version v2.0
 *
 *    ---file comments----
 *    */

#ifndef _CTC_GOLDENGATE_VLAN_CLI_H
#define _CTC_GOLDENGATE_VLAN_CLI_H
#ifdef __cplusplus
extern "C" {
#endif
#include "sal.h"
#include "ctc_cli.h"

/**
 *  @brief  Initialize sdk vlan module CLIs, for greatbelt only
 *
 *   @param[in/out]
 *
 *    @return CTC_E_XXX
 *
 *    */
extern int32
ctc_goldengate_vlan_cli_init(void);

extern int32
ctc_goldengate_vlan_cli_deinit(void);

#ifdef __cplusplus
}
#endif

#endif  /* _CTC_GOLDENGATE_VLAN_CLI_H */

