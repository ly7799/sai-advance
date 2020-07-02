/**
 @file ctc_app_flow_recorder.h

 @date 2019-8-30

*/

#ifndef _CTC_APP_FLOW_RECORDER_H
#define _CTC_APP_FLOW_RECORDER_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef CTC_APP_FLOW_RECORDER
struct ctc_app_flow_recorder_init_param_s
{
    uint8 enable_ipfix_level;
    uint8 queue_drop_stats_en;
    uint8 resolve_conflict_en;
    uint8 resolve_conflict_level;
};
typedef struct ctc_app_flow_recorder_init_param_s ctc_app_flow_recorder_init_param_t;

extern int32 ctc_app_flow_recorder_init(ctc_app_flow_recorder_init_param_t *p_param);
extern int32 ctc_app_flow_recorder_deinit(void);
extern int32 ctc_app_flow_recorder_show_status(void);
#endif
#ifdef __cplusplus
}
#endif

#endif

