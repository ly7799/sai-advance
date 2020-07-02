/**
 @file ctc_goldengate_monitor_cli.h

 @date 2010-06-09

 @version v2.0

 This file declares extern function for ptp cli module

*/

#ifndef _CTC_GOLDENGATE_MONITOR_CLI_H
#define _CTC_GOLDENGATE_MONITOR_CLI_H
#ifdef __cplusplus
extern "C" {
#endif
#include "sal.h"
#include "ctc_cli.h"

/**
 @brief  Initialize sdk monitor module CLIs
*/
extern int32
ctc_goldengate_monitor_cli_init(void);

extern int32
ctc_goldengate_monitor_cli_deinit(void);

#ifdef __cplusplus
}
#endif

#endif  /* _CTC_GOLDENGATE_MONITOR_CLI_H */
