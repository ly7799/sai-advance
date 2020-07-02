/**
 @file ctc_greatbel_mirror_cli.c

 @date 2012-8-14

 @version v2.0

---file comments----
*/

/****************************************************************
 *
 * Header Files
 *
 ***************************************************************/
#include "ctc_mirror_cli.h"
#include "ctc_cli.h"
#include "ctc_error.h"
#include "ctc_api.h"
#include "ctcs_api.h"
#include "ctc_debug.h"
#include "ctc_cli_common.h"
#include "sys_greatbelt_mirror.h"

CTC_CLI(ctc_cli_gb_mirror_show_mirror_info,
        ctc_cli_gb_mirror_show_mirror_info_cmd,
        "show mirror info (port GPHYPORT_ID | vlan VLAN_ID) (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_MIRROR_M_STR,
        "Information",
        CTC_CLI_GPORT_DESC,
        CTC_CLI_GPHYPORT_ID_DESC,
        CTC_CLI_VLAN_M_STR,
        CTC_CLI_VLAN_RANGE_DESC,
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    uint8 index = 0;
    uint32 gport = 0;
    uint16 vlan_id = 0;
    int32 ret = 0;
    uint8 lchip = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;

    index = CTC_CLI_GET_ARGC_INDEX("port");
    if (index != 0xFF)
    {
        CTC_CLI_GET_UINT32_RANGE("gport", gport, argv[index + 1],
                                 0, CTC_MAX_UINT32_VALUE);
        ret = sys_greatbelt_mirror_show_port_info(lchip,gport);
        if (ret < 0)
        {
            ctc_cli_out("%% ret = %d, %s \n", ret, ctc_get_error_desc(ret));
            return CLI_ERROR;
        }
    }

    index = 0xFF;
    index = CTC_CLI_GET_ARGC_INDEX("vlan");
    if (index != 0xFF)
    {
        CTC_CLI_GET_UINT16_RANGE("vlan", vlan_id, argv[index + 1],
                                 0, CTC_MAX_UINT16_VALUE);
        ret = sys_greatbelt_mirror_show_vlan_info(lchip, vlan_id);
        if (ret < 0)
        {
            ctc_cli_out("%% ret = %d, %s \n", ret, ctc_get_error_desc(ret));
            return CLI_ERROR;
        }
    }

    return CLI_SUCCESS;
}

int32
ctc_greatbelt_mirror_cli_init(void)
{

    install_element(CTC_SDK_MODE, &ctc_cli_gb_mirror_show_mirror_info_cmd);

    return CLI_SUCCESS;
}

int32
ctc_greatbelt_mirror_cli_deinit(void)
{

    uninstall_element(CTC_SDK_MODE, &ctc_cli_gb_mirror_show_mirror_info_cmd);

    return CLI_SUCCESS;
}
