/**
 @file ctc_ipuc_cli.h

 @date 2009-12-30

 @version v2.0

---file comments----
*/

#ifndef _CTC_GREATBELT_IPUC_CLI_H
#define _CTC_GREATBELT_IPUC_CLI_H
#ifdef __cplusplus
extern "C" {
#endif

#include "sal.h"
#include "ctc_cli.h"
#include "ctc_cli_common.h"

/**
 @brief  Initialize sdk ipuc module CLIs

 @param[in/out]   cli_tree     CLI tree

 @return CTC_E_XXX

*/
extern int32
ctc_greatbelt_ipuc_cli_init(void);

extern int32
ctc_greatbelt_ipuc_cli_deinit(void);

#ifdef __cplusplus
}
#endif

#endif  /* _CTC_IPUC_CLI_H */
