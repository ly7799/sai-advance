/**
 @file ctc_mpls_cli.c

 @date 2010-03-16

 @version v2.0

 The file apply clis of ipuc module
*/

#include "sal.h"
#include "ctc_cli.h"
#include "ctc_cli_common.h"
#include "ctc_debug.h"
#include "ctc_error.h"
#include "ctc_mpls_cli.h"
#include "ctc_api.h"
#include "ctcs_api.h"
#include "ctc_mpls.h"

extern int32
sys_goldengate_mpls_ilm_show(uint8 lchip, ctc_mpls_ilm_t* p_ilm_info, uint8  show_type);

extern int32
sys_goldengate_mpls_sqn_show(uint8 lchip, uint8 index);

extern int32
sys_goldengate_show_mpls_status(uint8 lchip);

#define CTC_CLI_MPLS_L2VPN_OLD_OPT \
    "| igmp-snooping-en | vsi-learning-disable\
        | mac-security-vsi-discard "
#define CTC_CLI_MPLS_POLICER_STR \
    "Set policer type after setting id,default type is flow policer"

#define CTC_CLI_MPLS_COMM_OPT \
    "stats STATS_ID |trust-outer-exp |qos-use-outer-info | acl-use-outer-info |"
#define CTC_CLI_MPLS_COMM_STR \
    "Statistics",\
    "1~0xFFFFFFFF",\
    "Use outer label's exp",\
    "Use the QOS information from outer header",\
    "Use the ACL information from outer header"
#define CTC_CLI_MPLS_SPACE_DESC "Label space id, <0-255>, space 0 is platform space"

CTC_CLI(ctc_cli_gg_mpls_add_pw,
        ctc_cli_gg_mpls_add_pw_cmd,
        "mpls l2vpn-pw add (space SPACEID |) LABEL (vpws NHID \
        | vpls FID {vpls-port-type "CTC_CLI_MPLS_L2VPN_OLD_OPT"| pwid PWID |})\
        { service-aclqos-en | encapsulation-mode (raw|tagged) | control-word \
        | oam (ID|) | svlan-tpid-index TPID_INDEX \
        | aps-select GROUP_ID (working-path|protection-path) \
        | service-id ID | service-policer-en| service-queue-en \
        | inner-pkt-raw | policer-id POLICERID (service-policer|)\
        | "CTC_CLI_MPLS_COMM_OPT"}",
        CTC_CLI_MPLS_M_STR,
        "L2vpn pw mapping",
        "Add vpls/vpws pw mapping",
        "Mpls label space",
        "Label space id, <0-255>, space 0 is platform space",
        "L2 vpn vc incoming label, <0-1048575>",
        "Virtual private wire services ",
        CTC_CLI_NH_ID_STR,
        "Virtual private lan services",
        CTC_CLI_FID_ID_DESC,
        "Vpls port type, means this vpls PW is not a H-VPLS tunnel",
        "enable igmp snooping ",
        "disable vsi learning",
        "vsi mac security discard",
        "Logic port",
        "0xffff means no used",
        "Enable service aclqos",
        "PW encapsulation mode",
        "Raw mode",
        "Tagged mode",
        "Control word used",
        "OAM Enable",
        "L2vpn oam id, equal to fid when VPLS",
        "Set svlan tpid index",
        "Index <0-3>, the corresponding svlan tpid is in EpeL2TpidCtl",
        "APS select ",
        CTC_CLI_APS_GROUP_ID_DESC,
        "Indicate working path",
        "Indicate protection path",
        "Service id",
        "Service id number",
        "Service policer enable",
        "Service queue enable",
        "Inner packet raw",
        "Set Policer ID",
        "Policer ID 1 - 16383",
        CTC_CLI_MPLS_POLICER_STR,
        CTC_CLI_MPLS_COMM_STR)
{
    int32 ret = CLI_SUCCESS;
    ctc_mpls_ilm_t ilm_info;
    int idx = 0;

    sal_memset(&ilm_info, 0, sizeof(ctc_mpls_ilm_t));

    idx = CTC_CLI_GET_ARGC_INDEX("space");
    if (0xFF != idx)
    {
        CTC_CLI_GET_UINT8_RANGE("space", ilm_info.spaceid, argv[idx + 1], 0, CTC_MAX_UINT8_VALUE);
        CTC_CLI_GET_UINT32("label", ilm_info.label, argv[idx + 2]);
    }
    else
    {
        ilm_info.spaceid = 0;
        CTC_CLI_GET_UINT32("label", ilm_info.label, argv[0]);
    }

    idx = CTC_CLI_GET_ARGC_INDEX("vpws");
    if (0xFF != idx)
    {
        CTC_CLI_GET_UINT32("nexthop", ilm_info.nh_id, argv[idx + 1]);
        ilm_info.type = CTC_MPLS_LABEL_TYPE_VPWS;
    }

    idx = CTC_CLI_GET_ARGC_INDEX("vpls");
    if (0xFF != idx)
    {
        CTC_CLI_GET_UINT16("vpls", ilm_info.fid, argv[idx + 1]);
        ilm_info.type = CTC_MPLS_LABEL_TYPE_VPLS;

        idx = CTC_CLI_GET_ARGC_INDEX("vpls-port-type");
        if (0xFF != idx)
        {
            ilm_info.logic_port_type = TRUE;
        }
    }

    idx  = CTC_CLI_GET_ARGC_INDEX("pwid");
    if (0xFF != idx)
    {
        CTC_CLI_GET_UINT16("pwid", ilm_info.pwid, argv[idx + 1]);
    }


    /* qos use outer info */
    idx  = CTC_CLI_GET_ARGC_INDEX("qos-use-outer-info");
    if (0xFF != idx)
    {
        ilm_info.qos_use_outer_info = TRUE;
    }

    idx = CTC_CLI_GET_ARGC_INDEX("acl-use-outer-info");
    if (0xFF != idx)
    {
        CTC_SET_FLAG(ilm_info.flag, CTC_MPLS_ILM_FLAG_ACL_USE_OUTER_INFO);
    }

    /* service aclqos en */
    idx  = CTC_CLI_GET_ARGC_INDEX("service-aclqos-en");
    if (0xFF != idx)
    {
        CTC_SET_FLAG(ilm_info.flag, CTC_MPLS_ILM_FLAG_SERVICE_ACL_EN);
    }

    /* encapsulation mode is RAW for default */
    ilm_info.pw_mode = CTC_MPLS_L2VPN_RAW;
    idx  = CTC_CLI_GET_ARGC_INDEX("encapsulation-mode");
    if (0xFF != idx)
    {
        idx  = CTC_CLI_GET_ARGC_INDEX("raw");
        if (0xFF != idx)
        {
            ilm_info.pw_mode = CTC_MPLS_L2VPN_RAW;
        }
        else
        {
            ilm_info.pw_mode = CTC_MPLS_L2VPN_TAGGED;
        }
    }

    idx  = CTC_CLI_GET_ARGC_INDEX("control-word");
    if (0xFF != idx)
    {
        ilm_info.cwen = TRUE;
    }

    idx  = CTC_CLI_GET_ARGC_INDEX("oam");
    if (0xFF != idx)
    {
         /*pw_info.oam_en = TRUE;*/
        CTC_CLI_GET_UINT16_RANGE("l2vpn-oam-id", ilm_info.l2vpn_oam_id, argv[idx + 1], 0, CTC_MAX_UINT16_VALUE);
        CTC_SET_FLAG(ilm_info.flag, CTC_MPLS_ILM_FLAG_L2VPN_OAM);
    }

    idx = CTC_CLI_GET_ARGC_INDEX("svlan-tpid-index");
    if (0xFF != idx)
    {
        CTC_CLI_GET_UINT8_RANGE("svlan-tpid-index", ilm_info.svlan_tpid_index, argv[idx + 1], 0, CTC_MAX_UINT8_VALUE);
    }

    idx  = CTC_CLI_GET_ARGC_INDEX("service-id");
    if (0xFF != idx)
    {
        CTC_CLI_GET_UINT16_RANGE("service-id ", ilm_info.flw_vrf_srv_aps.service_id, argv[idx + 1], 0, CTC_MAX_UINT16_VALUE);
        ilm_info.id_type |= CTC_MPLS_ID_SERVICE;
    }

    /* service policer en */
    idx  = CTC_CLI_GET_ARGC_INDEX("service-policer-en");
    if (0xFF != idx)
    {
        CTC_SET_FLAG(ilm_info.flag, CTC_MPLS_ILM_FLAG_SERVICE_ACL_EN);
    }

    /* service queue en */
    idx  = CTC_CLI_GET_ARGC_INDEX("service-queue-en");
    if (0xFF != idx)
    {
        CTC_SET_FLAG(ilm_info.flag, CTC_MPLS_ILM_FLAG_SERVICE_POLICER_EN);
    }


    idx = CTC_CLI_GET_ARGC_INDEX("aps-select");
    if (0xFF != idx)
    {
        ilm_info.id_type |= CTC_MPLS_ID_APS_SELECT;
        CTC_CLI_GET_UINT16_RANGE("aps-select ", ilm_info.aps_select_grp_id, argv[idx + 1], 0, CTC_MAX_UINT16_VALUE);

        if (CLI_CLI_STR_EQUAL("protection-path", idx + 2))
        {
            ilm_info.aps_select_protect_path = 1;
        }
        else
        {
            ilm_info.aps_select_protect_path = 0;
        }
    }

    idx = CTC_CLI_GET_ARGC_INDEX("stats");
    if (idx != 0xFF)
    {
        ilm_info.id_type |= CTC_MPLS_ID_STATS;
        CTC_CLI_GET_UINT32("stats-id", ilm_info.stats_id, argv[idx + 1]);
    }

    idx = CTC_CLI_GET_ARGC_INDEX("inner-pkt-raw");
    if (idx != 0xFF)
    {
        ilm_info.inner_pkt_type = CTC_MPLS_INNER_RAW;
    }

    idx = CTC_CLI_GET_ARGC_INDEX("policer-id");
    if (idx != 0xFF)
    {
        CTC_CLI_GET_UINT16("policer-id", ilm_info.policer_id, argv[idx + 1]);

        idx = CTC_CLI_GET_ARGC_INDEX("service-policer");
        if (idx != 0xFF)
        {
            CTC_SET_FLAG(ilm_info.flag, CTC_MPLS_ILM_FLAG_SERVICE_POLICER_EN);
        }
    }

    idx = CTC_CLI_GET_ARGC_INDEX("acl-use-outer-info");
    if (0xFF != idx )
    {
        CTC_SET_FLAG(ilm_info.flag, CTC_MPLS_ILM_FLAG_ACL_USE_OUTER_INFO);
    }

    idx = CTC_CLI_GET_ARGC_INDEX("trust-outer-exp");
    if (0xFF != idx )
    {
        ilm_info.trust_outer_exp = 1;
    }

    if(g_ctcs_api_en)
    {
        ret = ctcs_mpls_add_ilm(g_api_lchip, &ilm_info);
    }
    else
    {
        ret = ctc_mpls_add_ilm(&ilm_info);
    }
    if (ret < 0)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return ret;
}

CTC_CLI(ctc_cli_gg_mpls_remove_pw,
        ctc_cli_gg_mpls_remove_pw_cmd,
        "mpls l2vpn-pw remove (space SPACEID |) LABEL",
        CTC_CLI_MPLS_M_STR,
        "L2vpn pw mapping",
        "Remove l2vpn pw mapping",
        "Mpls label space",
        "Label space id, <0-255>, space 0 is platform space",
        "L2vpn vc incoming label, <0-1048575>")
{
    int32 ret = CLI_SUCCESS;
    uint8 idx = 0;
    ctc_mpls_ilm_t pw_info;

    sal_memset(&pw_info, 0, sizeof(pw_info));
    idx = CTC_CLI_GET_ARGC_INDEX("space");
    if (0xFF != idx)
    {
        CTC_CLI_GET_UINT8_RANGE("space", pw_info.spaceid, argv[idx + 1], 0, CTC_MAX_UINT8_VALUE);
        CTC_CLI_GET_UINT32("label", pw_info.label, argv[idx + 2]);
    }
    else
    {
        pw_info.spaceid = 0;
        CTC_CLI_GET_UINT32("label", pw_info.label, argv[0]);
    }

    if(g_ctcs_api_en)
    {
        ret = ctcs_mpls_del_ilm(g_api_lchip, &pw_info);
    }
    else
    {
        ret = ctc_mpls_del_ilm(&pw_info);
    }
    if (ret < 0)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_mpls_show_ilm,
        ctc_cli_gg_mpls_show_ilm_cmd,
        "show mpls ilm (SPACEID ((LABEL detail) |) |) (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_MPLS_M_STR,
        "Ilm entries",
        "space id <0-255>, 0 is platform space",
        "label <0-1048575>",
        "Key and associate-data offset info",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = 0;
    uint8 show_type = 0;
    ctc_mpls_ilm_t mpls_ilm;
    uint8 lchip = 0;
    uint8 index = 0;
    sal_memset(&mpls_ilm, 0, sizeof(ctc_mpls_ilm_t));



    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
        if (5 == argc)
        {
            CTC_CLI_GET_UINT8("space", mpls_ilm.spaceid, argv[0]);
            CTC_CLI_GET_UINT32("label", mpls_ilm.label, argv[1]);
            show_type = 1;
        }
        else if (3 == argc)
        {
            CTC_CLI_GET_UINT8("space", mpls_ilm.spaceid, argv[0]);
            show_type = 2;
        }
        else
        {
            show_type = 3;
        }
    }
    else
    {
        if (3 == argc)
        {
            CTC_CLI_GET_UINT8("space", mpls_ilm.spaceid, argv[0]);
            CTC_CLI_GET_UINT32("label", mpls_ilm.label, argv[1]);
            show_type = 1;
        }
        else if (1 == argc)
        {
            CTC_CLI_GET_UINT8("space", mpls_ilm.spaceid, argv[0]);
            show_type = 2;
        }
        else
        {
            show_type = 3;
        }
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_mpls_ilm_show(lchip, &mpls_ilm, show_type);
    if (ret < 0)
    {
        ctc_cli_out("%% %s \n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return ret;
}

CTC_CLI(ctc_cli_gg_mpls_show_sqn,
        ctc_cli_gg_mpls_show_sqn_cmd,
        "show mpls sequence-number SQN_INDEX (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_MPLS_M_STR,
        "Sequence counter",
        "Sequence counter index, <0-255>",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    uint8 index;
    uint8 sindex;
    int32 ret = 0;
    uint8 lchip = 0;

    CTC_CLI_GET_UINT8_RANGE("index", sindex, argv[0], 0, CTC_MAX_UINT8_VALUE);

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_mpls_sqn_show(lchip, sindex);
    if (ret < 0)
    {
        ctc_cli_out("%% %s \n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return ret;
}

CTC_CLI(ctc_cli_gg_mpls_show_mpls_status,
        ctc_cli_gg_mpls_show_mpls_status_cmd,
        "show mpls status (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_MPLS_M_STR,
        "MPLS Status",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{

    int32 ret  = CLI_SUCCESS;
    uint8 lchip = 0;
    uint8 index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_show_mpls_status(lchip);
    if (ret < 0)
    {
        ctc_cli_out("%% %s \n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return ret;

}

int32
ctc_goldengate_mpls_cli_init(void)
{
    install_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_show_ilm_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_show_sqn_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_show_mpls_status_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_add_pw_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_remove_pw_cmd);

    return CLI_SUCCESS;
}

int32
ctc_goldengate_mpls_cli_deinit(void)
{
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_show_ilm_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_show_sqn_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_show_mpls_status_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_add_pw_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_mpls_remove_pw_cmd);

    return CLI_SUCCESS;
}

