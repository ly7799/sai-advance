/**
 @file ctc_greatbelt_special_cli.h

 @date 2010-7-9

 @version v2.0

---file comments----
*/

#ifndef _CTC_GREATBELT_SPECIAL_CLI_H
#define _CTC_GREATBELT_SPECIAL_CLI_H
#ifdef __cplusplus
extern "C" {
#endif

#include "sal.h"
#include "ctc_cli.h"
#include "ctc_cli_common.h"

extern int32
ctc_greatbelt_chip_special_cli_init(void);

extern int32
ctc_greatbelt_chip_special_cli_deinit(void);

extern int32
ctc_greatbelt_chip_special_cli_callback_init(void);

#ifdef __cplusplus
}
#endif

#endif

