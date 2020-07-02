/**
 @file ctc_goldengate_interrupt_cli.c

 @author  Copyright (C) 2012 Centec Networks Inc.  All rights reserved.

 @date 2012-10-23

 @version v2.0

 This file define interrupt CLI functions

*/

#include "sal.h"
#include "ctc_api.h"
#include "ctcs_api.h"
#include "ctc_cli.h"
#include "ctc_cli_common.h"
#include "ctc_error.h"
#include "ctc_debug.h"
#include "ctc_interrupt.h"
#include "ctc_goldengate_interrupt.h"

extern int32
sys_goldengate_interrupt_dump_db(uint8 lchip);
extern int32
sys_goldengate_interrupt_dump_reg(uint8 lchip, uint32 intr);

extern int32
sys_goldengate_interrupt_parser_type(uint8 lchip, ctc_intr_type_t type);
extern int32
sys_goldengate_interrupt_show_status(uint8 lchip);

CTC_CLI(ctc_cli_gg_interrupt_show_interrupt_sdk,
        ctc_cli_gg_interrupt_show_interrupt_sdk_cmd,
        "show interrupt sdk (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_INTR_M_STR,
        "Infomation in SDK",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint8 index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_interrupt_dump_db(lchip);
    if (ret < 0)
    {
        ctc_cli_out("%% ret = %d, %s \n", ret, ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return ret;
}

CTC_CLI(ctc_cli_gg_interrupt_show_interrupt_chip,
        ctc_cli_gg_interrupt_show_interrupt_chip_cmd,
        "show interrupt LCHIP register (group | chip-fatal | chip-normal | func | pcie | dma-func | all)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_INTR_M_STR,
        CTC_CLI_LCHIP_ID_VALUE,
        "Interrupt register",
        "Group/PIN level",
        "Chip fatal",
        "Chip normal",
        "Function",
        "PCIe",
        "DMA function",
        "All")
{
    uint8 lchip = 0;
    uint32 intr = 0;
    int32 ret = CLI_SUCCESS;

    CTC_CLI_GET_UINT8("local-lchip", lchip, argv[0]);

    if (0 == sal_strcmp(argv[1], "group"))
    {
        /* SYS_INTR_GG_MAX */
        CTC_BIT_SET(intr, 31);
    }
    else if (0 == sal_strcmp(argv[1], "chip-fatal"))
    {
        /* SYS_INTR_GG_CHIP_FATAL */
        CTC_BIT_SET(intr, 0);
    }
    else if (0 == sal_strcmp(argv[1], "chip-normal"))
    {
        /* SYS_INTR_GG_CHIP_NORMAL*/
        CTC_BIT_SET(intr, 1);
    }
    else if (0 == sal_strcmp(argv[1], "func"))
    {
        /* SYS_INTR_GG_PCS_LINK_31_0 */
        CTC_BIT_SET(intr, 2);
    }
    else if (0 == sal_strcmp(argv[1], "pcie"))
    {
        /* SYS_INTR_GG_PCIE_BURST_DONE */
        CTC_BIT_SET(intr, 28);
    }
    else if (0 == sal_strcmp(argv[1], "dma-func"))
    {
        /* SYS_INTR_GG_DMA_0 */
        CTC_BIT_SET(intr, 29);
    }
    else if (0 == sal_strcmp(argv[1], "all"))
    {
        /* SYS_INTR_GG_CHIP_FATAL */
        CTC_BIT_SET(intr, 0);
        /* SYS_INTR_GG_CHIP_NORMAL */
        CTC_BIT_SET(intr, 1);
        /* SYS_INTR_GG_PCS_LINK_31_0 */
        CTC_BIT_SET(intr, 2);
        /* SYS_INTR_GG_PCIE_BURST_DONE */
        CTC_BIT_SET(intr, 28);
        /* SYS_INTR_GG_DMA_0 */
        CTC_BIT_SET(intr, 29);
        /* SYS_INTR_GG_MAX */
        CTC_BIT_SET(intr, 31);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_interrupt_dump_reg(lchip, intr);
    if (ret < 0)
    {
        ctc_cli_out("%% ret = %d, %s \n", ret, ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return ret;
}

/*just used for debug */
CTC_CLI(ctc_cli_gg_interrupt_show_type,
        ctc_cli_gg_interrupt_show_type_cmd,
        "show interrupt LCHIP type TYPE sub-type TYPE low-type TYPE",
        CTC_CLI_SHOW_STR,
        CTC_CLI_INTR_M_STR,
        CTC_CLI_LCHIP_ID_VALUE,
        CTC_CLI_INTR_TYPE_STR,
        CTC_CLI_INTR_TYPE_VALUE,
        CTC_CLI_INTR_SUB_TYPE_STR,
        CTC_CLI_INTR_TYPE_VALUE,
        "Interrupt sub-level type",
        CTC_CLI_INTR_TYPE_VALUE)
{
    uint8 lchip = 0;
    ctc_intr_type_t type;
    int32 ret = CLI_SUCCESS;

    CTC_CLI_GET_UINT32("local-lchip", lchip, argv[0]);
    CTC_CLI_GET_UINT32("type", type.intr, argv[1]);
    CTC_CLI_GET_UINT32("sub-type", type.sub_intr, argv[2]);
    CTC_CLI_GET_UINT32("low-type", type.low_intr, argv[3]);

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_interrupt_parser_type(lchip, type);
    if (ret < 0)
    {
        ctc_cli_out("%% ret = %d, %s \n", ret, ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_interrupt_show_status,
        ctc_cli_gg_interrupt_show_status_cmd,
        "show interrupt status (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_INTR_M_STR,
        "Interrupt global status",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint8  index = 0 ;

	index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_interrupt_show_status(lchip);
    if (ret < 0)
    {
        ctc_cli_out("%% ret = %d, %s \n", ret, ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

int32
ctc_goldengate_interrupt_cli_init(void)
{
    install_element(CTC_SDK_MODE, &ctc_cli_gg_interrupt_show_interrupt_sdk_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_interrupt_show_interrupt_chip_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_interrupt_show_type_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_interrupt_show_status_cmd);

    return CLI_SUCCESS;
}

int32
ctc_goldengate_interrupt_cli_deinit(void)
{
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_interrupt_show_interrupt_sdk_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_interrupt_show_interrupt_chip_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_interrupt_show_type_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_interrupt_show_status_cmd);

    return CLI_SUCCESS;
}
