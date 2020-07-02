/**
 @file ctc_usw_ftm_cli.h

 @date 2009-11-23

 @version v2.0

---file comments----
*/

#ifndef _CTC_USW_FTM_CLI_H
#define _CTC_USW_FTM_CLI_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 @brief

 @param[in/out]   cli_tree     CLI tree

 @return CTC_E_XXX

*/

extern int32
ctc_usw_ftm_cli_init(void);

extern int32
ctc_usw_ftm_cli_deinit(void);

#ifdef __cplusplus
}
#endif

#endif  /* _CTC_USW_FTM_CLI_H */
