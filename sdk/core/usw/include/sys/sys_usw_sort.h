/**
 @file sys_usw_sort.h

 @date 2009-12-18

 @version v2.0

*/
#ifndef _SYS_USW_SORT_H
#define _SYS_USW_SORT_H
#ifdef __cplusplus
extern "C" {
#endif
/***************************************************************
 *
 * Header Files
 *
 ***************************************************************/
#include "sal.h"

#include "ctc_ipuc.h"
#include "ctc_const.h"
#include "sys_usw_opf.h"
/***************************************************************
 *
 *  Defines and Macros
 *
 ***************************************************************/
typedef  void*  skinfo_t;
typedef  skinfo_t*  skinfo_a;
typedef  skinfo_a*  skinfo_2a;

#define SORT_KEY_OFFSET_ARRAY_MAX_NUM 48
#define SORT_KEY_OFFSET_ARRAY_UNIT 8192 /*8*1024*/
#define SORT_KEY_GET_OFFSET_PTR(offset_s, index) \
    ((offset_s)[(index) / SORT_KEY_OFFSET_ARRAY_UNIT][(index) % SORT_KEY_OFFSET_ARRAY_UNIT])

enum sys_sort_block_dir_e
{
    SYS_SORT_BLOCK_DIR_UP = -1,
    SYS_SORT_BLOCK_DIR_DOWN = 1
};
typedef enum sys_sort_block_dir_e sys_sort_block_dir_t;

struct sys_sort_block_s
{
    uint32      all_left_b; /* left unused boundary */
    uint32      all_right_b;    /* right unused boundary */
    uint32      init_left_b;    /* init left boundary*/
    uint32      init_right_b;   /* init right boundary */

    uint32      all_of_num;   /*the number of all offset */
    uint32      used_of_num; /*the number of all used offset */

    /* statistics */
    uint32      used_key_offset_num; /*TBD*/

    /* management */
    uint8       is_block_can_shrink;
    uint8       multiple;
    uint8       rsv[2];

    sys_sort_block_dir_t       preferred_dir;
    sys_sort_block_dir_t       adj_dir;
};
typedef struct sys_sort_block_s sys_sort_block_t;

typedef int32 (* sort_key_syn_key)(uint8 lchip, uint32 new_offset, uint32 old_offset,void *pdata);

struct sys_sort_key_info_s
{
    sys_sort_block_t*  block;
    uint32                  max_block_num;
    uint16                  block_id;
    uint16                  opf_type;
    void*                   pdata;
    uint8       adj_size;
    /*synchronize the soft-table and hard-table*/
    sort_key_syn_key  sync_fn;
};
typedef struct sys_sort_key_info_s sys_sort_key_info_t;

struct sys_sort_block_init_info_s
{
    sys_sort_block_t* block;
    uint32 boundary_l;     /*the left of offset's boundary*/
    uint32 boundary_r;    /*the right of offset's boundary*/
    void* user_data_of;  /*the user data for creating offset avl tree*/
    bool is_block_can_shrink;
    sys_sort_block_dir_t dir;
    sys_usw_opf_t* opf;
    uint32 max_offset_num;
    uint8 is_empty;
    uint8 multiple;
    uint8 rsv[2];
};
typedef struct sys_sort_block_init_info_s sys_sort_block_init_info_t;

struct sort_key_offset_array_s
{
    uint32* offset_array[SORT_KEY_OFFSET_ARRAY_MAX_NUM];
    uint8 max_offset_num;
};
typedef struct sort_key_offset_array_s sort_key_offset_array_t;

#define SYS_SORT_CHECK_KEY_INFO(key_info)  \
    {                                          \
        CTC_PTR_VALID_CHECK((key_info)->block);    \
        CTC_MAX_VALUE_CHECK((key_info)->block_id, (key_info)->max_block_num); \
    }

#define SYS_SORT_OPPO_DIR(dir)   (SYS_SORT_BLOCK_DIR_UP == (dir) ? SYS_SORT_BLOCK_DIR_DOWN : SYS_SORT_BLOCK_DIR_UP)

#define SYS_SORT_CHECK_KEY_BLOCK_EXIST(key_info, dir)       \
    {                                                           \
        int32 tmp_block_id = (key_info)->block_id + dir;        \
        if ((tmp_block_id < 0) || (tmp_block_id >= (key_info)->max_block_num)) \
        {                                                       \
            return CTC_E_NO_RESOURCE;                       \
        }                                                       \
    }

/***************************************************************
 *
 *  Functions
 *
 ***************************************************************/

extern int32
sys_usw_sort_key_alloc_offset(uint8 lchip, sys_sort_key_info_t* key_info, uint32* p_offset);

extern int32
sys_usw_sort_key_alloc_offset_from_position(uint8 lchip, sys_sort_key_info_t* key_info, uint32 offset);

extern int32
sys_usw_sort_key_free_offset(uint8 lchip, sys_sort_key_info_t* key_info, uint32 p_offset);

extern int32
sys_usw_sort_key_init_offset_array(uint8 lchip, void**** pp_offset_array, uint32 max_offset_num);

extern int32
sys_usw_sort_key_init_block(uint8 lchip, sys_sort_block_init_info_t* init_info);

#ifdef __cplusplus
}
#endif

#endif

