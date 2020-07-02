#ifndef _CTC_DKIT_GOLDENGATE_DISCARD_H_
#define _CTC_DKIT_GOLDENGATE_DISCARD_H_


#ifdef __cplusplus
extern "C" {
#endif


#include "sal.h"
#include "ctc_goldengate_dkit_discard_type.h"

#define CTC_DKIT_IPE_DISCARD_TYPE_NUM     64
#define CTC_DKIT_EPE_DISCARD_TYPE_NUM     64
#define CTC_DKIT_INVALID_PORT              0xFFFF
#define CTC_DKIT_REASON_LEN                            64
#define CTC_DKIT_CHANNEL_NUM                         128
#define CTC_DKIT_SLICE_NUM                             2

struct ctc_dkit_discard_stats_s
{
    uint16 stats[CTC_DKIT_SLICE_NUM][CTC_DKIT_DISCARD_MAX];
    uint16 bsr_stats[CTC_DKIT_CHANNEL_NUM][CTC_DKIT_BSR_PKTERR_ABORT_DATA_ERROR - CTC_DKIT_BSR_PKTERR_SILENT_RESRC_FAIL + 1];
    uint16 netrx_stats[CTC_DKIT_CHANNEL_NUM][CTC_DKIT_NETRX_FULL_DROP - CTC_DKIT_NETRX_LEN_ERROR + 1];
    uint8 match_port;
    uint8 rsv;
    uint16 gport;
};
typedef struct ctc_dkit_discard_stats_s ctc_dkit_discard_stats_t;

enum ctc_dkit_discard_flag_e
{
    CTC_DKIT_DISCARD_FLAG_NONE = 0x00,
    CTC_DKIT_DISCARD_FLAG_IPE=0x01,
    CTC_DKIT_DISCARD_FLAG_EPE=0x02,
    CTC_DKIT_DISCARD_FLAG_BSR=0x04,
    CTC_DKIT_DISCARD_FLAG_OAM=0x08,
    CTC_DKIT_DISCARD_FLAG_NET_RX=0x10,
    CTC_DKIT_DISCARD_FLAG_NET_TX=0x20
} ;
typedef enum ctc_dkit_discard_flag_e ctc_dkit_discard_flag_t;


struct ctc_dkit_discard_reason_s
{
    uint8 module;/*indicate which module discard packet, only one module valid, refer to ctc_dkit_discard_flag_t*/
    uint8 slice;
    uint16 port;
    uint32 reason_id;
    uint16 stats;
};
typedef struct ctc_dkit_discard_reason_s ctc_dkit_discard_reason_t;


enum ctc_dkit_discard_value_type_e
{
    CTC_DKIT_DISCARD_VALUE_TYPE_NONE = 0,
    CTC_DKIT_DISCARD_VALUE_TYPE_1,    /*1 valid value*/
    CTC_DKIT_DISCARD_VALUE_TYPE_2,    /*2 valid value*/
    CTC_DKIT_DISCARD_VALUE_TYPE_3,    /*3 valid value*/

} ;
typedef enum ctc_dkit_discard_value_type_e ctc_dkit_discard_value_type_t;

struct ctc_dkit_discard_reason_bus_s
{
    uint32 reason_id;

    uint16 gport;/*ctc global port*/
    uint8 value_type;  /*ctc_dkit_discard_value_type_t*/
    uint8 valid;

    uint32 value[4];/*some value used for show reason*/
};
typedef struct ctc_dkit_discard_reason_bus_s ctc_dkit_discard_reason_bus_t;


struct ctc_dkit_discard_reason_captured_s
{
    uint32 reason_id;

    uint8 value_type;  /*ctc_dkit_discard_value_type_t*/
    uint8 rsv[3];

    uint32 value[4];/*some value used for show reason*/
};
typedef struct ctc_dkit_discard_reason_captured_s ctc_dkit_discard_reason_captured_t;


#define CTC_DKIT_DISCARD_REASON_NUM 64
struct ctc_dkit_discard_info_s
{
      char systime_str[50];

      uint8 discard_flag; /*indicate which module discard packet, refer to ctc_dkit_discard_flag_t*/
      uint8 discard_count; /*if discard_count>1, can not use bus info*/
      uint8 slice_ipe; /*only for read bus*/
      uint8 slice_epe;
      uint8 lchip;

      ctc_dkit_discard_reason_t discard_reason[CTC_DKIT_DISCARD_REASON_NUM];
      ctc_dkit_discard_reason_bus_t discard_reason_bus;
      ctc_dkit_discard_reason_captured_t discard_reason_captured;
};
typedef struct ctc_dkit_discard_info_s ctc_dkit_discard_info_t;

#define CTC_DKIT_DISCARD_HISTORY_NUM  8
struct ctc_dkit_discard_history_s
{
    ctc_dkit_discard_info_t discard[CTC_DKIT_DISCARD_HISTORY_NUM];
    uint8 front;
    uint8 rear;/*the position is empty*/
};
typedef struct ctc_dkit_discard_history_s ctc_dkit_discard_history_t;


extern int32
ctc_goldengate_dkit_discard_process(void* p_para);

extern int32
ctc_goldengate_dkit_discard_show_type(void* p_para);

extern int32
ctc_goldengate_dkit_discard_show_stats(void* p_para);

extern int32
ctc_goldengate_dkit_discard_to_cpu_en(void* p_para);

extern int32
ctc_goldengate_dkit_discard_display_en(void* p_para);

#ifdef __cplusplus
}
#endif

#endif



