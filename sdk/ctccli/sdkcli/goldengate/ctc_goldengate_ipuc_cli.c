/**
 @file ctc_ipuc_cli.c

 @date 2009-12-30

 @version v2.0

 The file apply clis of ipuc module
*/

#include "sal.h"
#include "ctc_cli.h"
#include "ctc_cli_common.h"
#include "ctc_debug.h"
#include "ctc_error.h"
#include "ctc_ipuc_cli.h"
#include "ctc_api.h"
#include "ctcs_api.h"
#include "ctc_ipuc.h"

extern int32
sys_goldengate_ipuc_db_show(uint8 lchip, ctc_ip_ver_t ip_ver, uint32* p_ipuc_data,uint32 detail);
extern int32
sys_goldengate_ipuc_offset_show(uint8 lchip, uint8 flag, uint8 blockid, uint8 detail);
extern int32
sys_goldengate_ipuc_state_show(uint8 lchip);
extern int32
sys_goldengate_ipuc_db_show_count(uint8 lchip);
extern int32
_sys_goldengate_ipuc_rpf_check_port(uint8 lchip, uint8 enable);

extern int32
sys_goldengate_lpm_enable_dma(uint8 lchip, uint8 enable);
extern int32
sys_goldengate_ipuc_reinit(uint8 lchip, uint8 use_hash8);
extern int32
sys_goldengate_ipuc_show_debug_info(uint8 lchip, ctc_ipuc_param_t* p_ipuc_param);

extern int32
sys_goldengate_ipuc_default_entry_en(uint8 lchip, uint32 enable);

extern int32
sys_goldengate_get_ipuc_prefix_mode(uint8 lchip);

extern int32
sys_goldengate_ipuc_show_nat_sa_db(uint8 lchip, ctc_ip_ver_t ip_ver, uint32 detail);

extern int32
sys_goldengate_ipuc_set_host_route_mode(uint8 lchip, uint8 host_route_mode);

extern int32
sys_goldengate_lpm_db_show(uint8 lchip, ctc_ipuc_param_t* p_ipuc_param, uint32 detail);

CTC_CLI(ctc_cli_gg_ipuc_show_ipv4,
        ctc_cli_gg_ipuc_show_ipv4_cmd,
        "show ipuc (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint8 lchip = 0;
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_db_show(lchip, CTC_IP_VER_4, NULL,FALSE);

    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_show_ipv6,
        ctc_cli_gg_ipuc_show_ipv6_cmd,
        "show ipuc ipv6 (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        CTC_CLI_IPV6_STR,
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint8 lchip = 0;
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_db_show(lchip, CTC_IP_VER_6, NULL,FALSE);

    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_show_count,
        ctc_cli_gg_ipuc_show_count_cmd,
        "show ipuc count (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "Show ipuc route number",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint8 lchip = 0;
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_db_show_count(lchip);

    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_show_state,
        ctc_cli_gg_ipuc_show_state_cmd,
        "show ipuc status (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "IPUC configure and state",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint8 lchip = 0;
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_state_show(lchip);

    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_offset_show,
        ctc_cli_gg_ipuc_offset_show_cmd,
        "show ipuc offset (ipv6 | ) (block-id INDEX|) (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "IPUC offset",
        CTC_CLI_IPV6_STR,
        "Block id",
        "Block index",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32  ret = CLI_ERROR;
    uint32 index = 0;
    uint8 block_id = 0;
    uint32 ip_ver = MAX_CTC_IP_VER;
    uint8 lchip = 0;
    uint8 detail = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }


    if (0 == sal_memcmp("ipv6", argv[0], 4))
    {
        ip_ver = CTC_IP_VER_6;
    }
    else
    {
        ip_ver = CTC_IP_VER_4;
    }


    index = CTC_CLI_GET_ARGC_INDEX("block-id");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("block id", block_id, argv[index + 1]);
        detail = 1;
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_offset_show(lchip, ip_ver, block_id, detail);

    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_arrange_fragment,
        ctc_cli_gg_ipuc_arrange_fragment_cmd,
        "ipuc arrange fragment",
        CTC_CLI_IPUC_M_STR,
        "Arrange lpm fragment",
        "Arrange lpm fragment")
{
    int32 ret = CLI_ERROR;

    if(g_ctcs_api_en)
    {
       ret = ctcs_ipuc_arrange_fragment(g_api_lchip, NULL);
    }
    else
    {
        ret = ctc_ipuc_arrange_fragment(NULL);
    }

    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_show_ipv4_info,
        ctc_cli_gg_ipuc_show_ipv4_info_cmd,
        "show ipuc VRFID A.B.C.D MASK_LEN (l4-dst-port VALUE (tcp-port|)|) (lchip LCHIP|)",
        CTC_CLI_IPUC_M_STR,
        "Show ipuc ipv4 route",
        CTC_CLI_VRFID_ID_DESC,
        CTC_CLI_IPV4_FORMAT,
        CTC_CLI_IPV4_MASK_LEN_FORMAT,
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint32 vrfid = 0;
    uint32 masklen = 0;
    ctc_ipuc_param_t ipuc_info = {0};
    uint8  index = 0;
    uint16 l4_dst_port = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    CTC_CLI_GET_UINT16_RANGE("vrfid", vrfid, argv[0], 0, CTC_MAX_UINT16_VALUE);
    ipuc_info.vrf_id = vrfid;

    CTC_CLI_GET_IPV4_ADDRESS("ip", ipuc_info.ip.ipv4, argv[1]);

    CTC_CLI_GET_UINT8_RANGE("mask", masklen, argv[2], 0, CTC_MAX_UINT8_VALUE);
    ipuc_info.masklen = masklen;

    index = CTC_CLI_GET_ARGC_INDEX("l4-dst-port");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT16("l4-dst-port", l4_dst_port, argv[index + 1]);
    }
    ipuc_info.l4_dst_port = l4_dst_port;

    index = CTC_CLI_GET_ARGC_INDEX("tcp-port");
    if (0xFF != index)
    {
        ipuc_info.is_tcp_port = 1;
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_show_debug_info(lchip, &ipuc_info);

    if (ret < 0)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_show_ipv6_info,
        ctc_cli_gg_ipuc_show_ipv6_info_cmd,
        "show ipuc ipv6 VRFID X:X::X:X MASK_LEN (lchip LCHIP|)",
        CTC_CLI_IPUC_M_STR,
        CTC_CLI_IPV6_STR,
        "show ipuc ipv6 route",
        CTC_CLI_VRFID_ID_DESC,
        CTC_CLI_IPV6_FORMAT,
        CTC_CLI_IPV6_MASK_LEN_FORMAT,
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint32 vrfid = 0;
    uint32 masklen = 0;
    ipv6_addr_t ipv6_address;
    ctc_ipuc_param_t ipuc_info = {0};
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    ipuc_info.ip_ver = CTC_IP_VER_6;

    CTC_CLI_GET_UINT16_RANGE("vrfid", vrfid, argv[0], 0, CTC_MAX_UINT16_VALUE);
    ipuc_info.vrf_id = vrfid;

    CTC_CLI_GET_IPV6_ADDRESS("ipv6", ipv6_address, argv[1]);

    /* adjust endian */
    ipuc_info.ip.ipv6[0] = sal_htonl(ipv6_address[0]);
    ipuc_info.ip.ipv6[1] = sal_htonl(ipv6_address[1]);
    ipuc_info.ip.ipv6[2] = sal_htonl(ipv6_address[2]);
    ipuc_info.ip.ipv6[3] = sal_htonl(ipv6_address[3]);

    CTC_CLI_GET_UINT8_RANGE("mask", masklen, argv[2], 0, CTC_MAX_UINT8_VALUE);
    ipuc_info.masklen = masklen;

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_show_debug_info(lchip, &ipuc_info);

    if (ret < 0)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_write_lpm_by_dma,
        ctc_cli_gg_ipuc_write_lpm_by_dma_cmd,
        "ipuc dma (enable|disable) (lchip LCHIP|)",
        CTC_CLI_IPUC_M_STR,
        "Dma",
        "Enable",
        "Disable",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint32 index;
    bool dma_enable;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    index = CTC_CLI_GET_ARGC_INDEX("enable");
    if (index != 0xFF)
    {
        dma_enable = TRUE;
    }
    else
    {
        dma_enable = FALSE;
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_lpm_enable_dma(lchip, dma_enable);
    if (ret < 0)
    {
        ctc_cli_out("%% %s \n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_rpf_check_port,
        ctc_cli_gg_ipuc_rpf_check_port_cmd,
        "ipuc rpf-check-port (enable|disable) (lchip LCHIP|)",
        CTC_CLI_IPUC_M_STR,
        "Rpf check by port",
        "Enable",
        "Disable",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint32 index;
    uint8 enable;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    index = CTC_CLI_GET_ARGC_INDEX("enable");
    if (index != 0xFF)
    {
        enable = TRUE;
    }
    else
    {
        enable = FALSE;
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = _sys_goldengate_ipuc_rpf_check_port(lchip, enable);
    if (ret < 0)
    {
        ctc_cli_out("%% %s \n", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_show_prefix_mode,
        ctc_cli_gg_ipuc_show_prefix_mode_cmd,
        "show ipuc prefix mode (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "prefix",
        "mode",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = 0;
    uint8 lchip = 0;
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
    	CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_get_ipuc_prefix_mode(lchip);

    ctc_cli_out("LPM Prefix Mode : %d\n",ret);

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_default_entry,
        ctc_cli_gg_ipuc_default_entry_cmd,
        "ipuc default-entry (enable | disable ) (lchip LCHIP|)",
        CTC_CLI_IPUC_M_STR,
        "Default entry",
        "Enable",
        "Disable",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint32 index;
    uint32 enable = 0;
    uint8 lchip = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
    	CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    index = CTC_CLI_GET_ARGC_INDEX("enable");
    if (index != 0xFF)
    {
        enable = 1;
    }
    else
    {
        enable = 0;
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_default_entry_en(lchip, enable);

    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}


CTC_CLI(ctc_cli_gg_ipuc_show_ipv4_nat_sa,
        ctc_cli_gg_ipuc_show_ipv4_nat_sa_cmd,
        "show ipuc nat-sa (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "Nat sa",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint8 lchip = 0;
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_show_nat_sa_db(lchip, CTC_IP_VER_4, FALSE);

    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_set_tcam,
        ctc_cli_gg_ipuc_set_tcam_cmd,
        "ipuc host-route-mode (hash | tcam) (lchip LCHIP|)",
        "IPUC",
        "Set host key mode",
        "Add host key to hash",
        "Add host key to tcam",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    uint8 index     = 0;
    int32 ret       = CLI_SUCCESS;
    uint8 host_route_mode   = 0;
    uint8 lchip     = 0;

    index = CTC_CLI_GET_ARGC_INDEX("tcam");
    if (index != 0xFF)
    {
        host_route_mode = 1;
    }
    else
    {
        host_route_mode = 0;
    }

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_ipuc_set_host_route_mode(lchip, host_route_mode);
    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }
    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_show_ipv4_lpm_info,
        ctc_cli_gg_ipuc_show_ipv4_lpm_info_cmd,
        "show ipuc lpm VRFID (all |prefix A.B.C.D MASK_LEN) (detail|node|) (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "Show ipuc lpm prefix route info",
        CTC_CLI_VRFID_ID_DESC", all vrf is CTC_MAX_VRFID_ID+1",
        "All ipv4 lpm route",
        "Ipv4 prefix, support prefix or prefix+8",
        CTC_CLI_IPV4_FORMAT,
        CTC_CLI_IPV4_MASK_LEN_FORMAT,
        "show detail info",
        "show detail and node info",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint32 vrfid = 0;
    uint32 detail = 0;
    ctc_ipuc_param_t ipuc_info = {0};
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    index = CTC_CLI_GET_ARGC_INDEX("detail");
    if (0xFF != index)
    {
        detail = 1;
    }

    index = CTC_CLI_GET_ARGC_INDEX("node");
    if (0xFF != index)
    {
        detail = 2;
    }

    ipuc_info.ip_ver = CTC_IP_VER_4;

    CTC_CLI_GET_UINT16_RANGE("vrfid", vrfid, argv[0], 0, CTC_MAX_UINT16_VALUE);
    ipuc_info.vrf_id = vrfid;

    index = CTC_CLI_GET_ARGC_INDEX("all");
    if (0xFF == index)
    {
        index = CTC_CLI_GET_ARGC_INDEX("prefix");
        if (0xFF != index)
        {
            CTC_CLI_GET_IPV4_ADDRESS("ip", ipuc_info.ip.ipv4, argv[index+1]);
            CTC_CLI_GET_UINT8_RANGE("mask", ipuc_info.masklen, argv[index+2], 0, CTC_MAX_UINT8_VALUE);
        }
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_lpm_db_show(lchip, &ipuc_info, detail);

    if (ret < 0)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_usw_ipuc_show_ipv6_lpm_info,
        ctc_cli_gg_ipuc_show_ipv6_lpm_info_cmd,
        "show ipuc lpm ipv6 VRFID (all | prefix X:X::X:X MASK_LEN |) (detail|node|) (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "Show ipuc lpm prefix route info",
        "Ip version 6",
        CTC_CLI_VRFID_ID_DESC", all vrf is CTC_MAX_VRFID_ID+1",
        "All ipv6 lpm route",
        "Ipv6 prefix, support 48 or 56",
        CTC_CLI_IPV6_FORMAT,
        CTC_CLI_IPV6_MASK_LEN_FORMAT,
        "show detail info",
        "show detail and node info",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint8 lchip = 0;
    uint32 vrfid = 0;
    uint32 detail = 0;
    ipv6_addr_t ipv6_address;
    ctc_ipuc_param_t ipuc_info = {0};
    uint8  index = 0;

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }

    index = CTC_CLI_GET_ARGC_INDEX("detail");
    if (0xFF != index)
    {
        detail = 1;
    }

    index = CTC_CLI_GET_ARGC_INDEX("node");
    if (0xFF != index)
    {
        detail = 1;
    }

    ipuc_info.ip_ver = CTC_IP_VER_6;

    CTC_CLI_GET_UINT16_RANGE("vrfid", vrfid, argv[0], 0, CTC_MAX_UINT16_VALUE);
    ipuc_info.vrf_id = vrfid;

    index = CTC_CLI_GET_ARGC_INDEX("all");
    if (0xFF == index)
    {
        index = CTC_CLI_GET_ARGC_INDEX("prefix");
        if (0xFF != index)
        {
            CTC_CLI_GET_IPV6_ADDRESS("ipv6", ipv6_address, argv[index+1]);

            /* adjust endian */
            ipuc_info.ip.ipv6[0] = sal_htonl(ipv6_address[0]);
            ipuc_info.ip.ipv6[1] = sal_htonl(ipv6_address[1]);
            ipuc_info.ip.ipv6[2] = sal_htonl(ipv6_address[2]);
            ipuc_info.ip.ipv6[3] = sal_htonl(ipv6_address[3]);
            CTC_CLI_GET_UINT8_RANGE("mask", ipuc_info.masklen, argv[index+2], 0, CTC_MAX_UINT8_VALUE);
        }
    }

    lchip = g_ctcs_api_en?g_api_lchip:lchip;
    ret = sys_goldengate_lpm_db_show(lchip, &ipuc_info, detail);

    if (ret < 0)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_get_nat_da_hit,
        ctc_cli_gg_ipuc_get_nat_da_hit_cmd,
        "show ipuc nat-da VRFID A.B.C.D (l4-dst-port L4PORT (tcp-port|)|) hit (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "Nat da",
        CTC_CLI_VRFID_ID_DESC,
        CTC_CLI_IPV4_FORMAT,
        "Layer4 source port for NAPT",
        CTC_CLI_L4_PORT_VALUE,
        "Indicate l4-dst-port is tcp port or not. If set is TCP port else is UDP port",
        "Entry hit status",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint8 hit = 0;
    uint8 lchip = 0;
    uint32 vrfid = 0;
    uint32 index = 0;
    ctc_ipuc_param_t ipuc_info = {0};
    sal_memset(&ipuc_info, 0, sizeof(ctc_ipuc_param_t));
    ipuc_info.ip_ver = CTC_IP_VER_4;

    CTC_CLI_GET_UINT16_RANGE("vrfid", vrfid, argv[0], 0, CTC_MAX_UINT16_VALUE);
    ipuc_info.vrf_id = vrfid;

    CTC_CLI_GET_IPV4_ADDRESS("ip", ipuc_info.ip.ipv4, argv[1]);

    ipuc_info.masklen = 32;

    index = CTC_CLI_GET_ARGC_INDEX_ENHANCE("l4-dst-port");
    if (index != 0xFF)
    {
        CTC_CLI_GET_UINT16_RANGE("l4-dst-port", ipuc_info.l4_dst_port, argv[index + 1], 0, CTC_MAX_UINT16_VALUE);
        index = CTC_CLI_GET_ARGC_INDEX_ENHANCE("tcp-port");
        if (index != 0xFF)
        {
            ipuc_info.is_tcp_port |= 1;
        }
    }
    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }
    lchip = g_ctcs_api_en?g_api_lchip:lchip;

    if(g_ctcs_api_en)
    {
        ret = ctcs_ipuc_get_entry_hit(lchip, &ipuc_info, &hit);
    }
    else
    {
        ret = ctc_ipuc_get_entry_hit(&ipuc_info, &hit);
    }
    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }
    ctc_cli_out("hit value:%d \n\r", hit);
    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_set_nat_da_hit,
        ctc_cli_gg_ipuc_set_nat_da_hit_cmd,
        "ipuc nat-da VRFID A.B.C.D (l4-dst-port L4PORT (tcp-port|)|) (hit VALUE) (lchip LCHIP|)",
        CTC_CLI_IPUC_M_STR,
        "Nat da",
        CTC_CLI_VRFID_ID_DESC,
        CTC_CLI_IPV4_FORMAT,
        "Layer4 source port for NAPT",
        CTC_CLI_L4_PORT_VALUE,
        "Indicate l4-dst-port is tcp port or not. If set is TCP port else is UDP port",
        "Entry hit status",
        "HIT VALUE",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_SUCCESS;
    uint32 vrfid = 0;
    uint8 index = 0;
    ctc_ipuc_param_t nat_info = {0};
    uint8 hit  = 0;
    uint8 lchip = 0;

    CTC_CLI_GET_UINT16_RANGE("vrfid", vrfid, argv[0], 0, CTC_MAX_UINT16_VALUE);
    nat_info.vrf_id = vrfid;

    CTC_CLI_GET_IPV4_ADDRESS("ip", nat_info.ip.ipv4, argv[1]);
    nat_info.masklen = 32;

    index = CTC_CLI_GET_ARGC_INDEX_ENHANCE("l4-dst-port");
    if (index != 0xFF)
    {
        CTC_CLI_GET_UINT16_RANGE("l4-dst-port", nat_info.l4_dst_port, argv[index + 1], 0, CTC_MAX_UINT16_VALUE);
        index = CTC_CLI_GET_ARGC_INDEX_ENHANCE("tcp-port");
        if (index != 0xFF)
        {
            nat_info.is_tcp_port |= 1;
        }
    }
    else
    {
        return CLI_ERROR;
    }

    index = CTC_CLI_GET_ARGC_INDEX("hit");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("hit", hit, argv[index + 1]);
        hit = hit ? 1 : 0;
    }

    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }
    lchip = g_ctcs_api_en?g_api_lchip:lchip;

    if(g_ctcs_api_en)
    {
        ret = ctcs_ipuc_set_entry_hit(lchip, &nat_info, hit);
    }
    else
    {
        ret = ctc_ipuc_set_entry_hit(&nat_info, hit);
    }

    if (ret < 0)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }

    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_get_nat_sa_hit,
        ctc_cli_gg_ipuc_get_nat_sa_hit_cmd,
        "show ipuc nat-sa VRFID A.B.C.D (l4-src-port L4PORT (tcp-port|)|) hit (lchip LCHIP|)",
        CTC_CLI_SHOW_STR,
        CTC_CLI_IPUC_M_STR,
        "Nat sa",
        CTC_CLI_VRFID_ID_DESC,
        CTC_CLI_IPV4_FORMAT,
        "Layer4 source port for NAPT",
        CTC_CLI_L4_PORT_VALUE,
        "Indicate l4-dst-port is tcp port or not. If set is TCP port else is UDP port",
        "Entry hit status",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint8 hit = 0;
    uint8 lchip = 0;
    uint16 vrfid = 0;
    uint32 index = 0;
    ctc_ipuc_nat_sa_param_t ipuc_info = {0};
    sal_memset(&ipuc_info, 0, sizeof(ctc_ipuc_nat_sa_param_t));
    ipuc_info.ip_ver = CTC_IP_VER_4;

    CTC_CLI_GET_UINT16_RANGE("vrfid", vrfid, argv[0], 0, CTC_MAX_UINT16_VALUE);
    ipuc_info.vrf_id = vrfid;

    CTC_CLI_GET_IPV4_ADDRESS("ip", ipuc_info.ipsa.ipv4, argv[1]);

    index = CTC_CLI_GET_ARGC_INDEX_ENHANCE("l4-src-port");
    if (index != 0xFF)
    {
        CTC_CLI_GET_UINT16_RANGE("l4-src-port", ipuc_info.l4_src_port, argv[index + 1], 0, CTC_MAX_UINT16_VALUE);
        index = CTC_CLI_GET_ARGC_INDEX_ENHANCE("tcp-port");
        if (index != 0xFF)
        {
            ipuc_info.flag|= CTC_IPUC_NAT_FLAG_USE_TCP_PORT;
        }
    }
    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }
    lchip = g_ctcs_api_en?g_api_lchip:lchip;

    if(g_ctcs_api_en)
    {
        ret = ctcs_ipuc_get_natsa_entry_hit(lchip, &ipuc_info, &hit);
    }
    else
    {
        ret = ctc_ipuc_get_natsa_entry_hit(&ipuc_info, &hit);
    }
    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }
    ctc_cli_out("hit value:%d \n\r", hit);
    return CLI_SUCCESS;
}

CTC_CLI(ctc_cli_gg_ipuc_set_nat_sa_hit,
        ctc_cli_gg_ipuc_set_nat_sa_hit_cmd,
        "ipuc nat-sa VRFID A.B.C.D (l4-src-port L4PORT (tcp-port|)|) (hit VALUE) (lchip LCHIP|)",
        CTC_CLI_IPUC_M_STR,
        "Nat sa",
        CTC_CLI_VRFID_ID_DESC,
        CTC_CLI_IPV4_FORMAT,
        "Layer4 source port for NAPT",
        CTC_CLI_L4_PORT_VALUE,
        "Indicate l4-dst-port is tcp port or not. If set is TCP port else is UDP port",
        "Entry hit status",
        CTC_CLI_LCHIP_ID_STR,
        CTC_CLI_LCHIP_ID_VALUE)
{
    int32 ret = CLI_ERROR;
    uint8 hit = 0;
    uint8 lchip = 0;
    uint16 vrfid = 0;
    uint32 index = 0;
    ctc_ipuc_nat_sa_param_t ipuc_info = {0};
    sal_memset(&ipuc_info, 0, sizeof(ctc_ipuc_nat_sa_param_t));
    ipuc_info.ip_ver = CTC_IP_VER_4;

    CTC_CLI_GET_UINT16_RANGE("vrfid", vrfid, argv[0], 0, CTC_MAX_UINT16_VALUE);
    ipuc_info.vrf_id = vrfid;

    CTC_CLI_GET_IPV4_ADDRESS("ip", ipuc_info.ipsa.ipv4, argv[1]);

    index = CTC_CLI_GET_ARGC_INDEX_ENHANCE("l4-src-port");
    if (index != 0xFF)
    {
        CTC_CLI_GET_UINT16_RANGE("l4-src-port", ipuc_info.l4_src_port, argv[index + 1], 0, CTC_MAX_UINT16_VALUE);
        index = CTC_CLI_GET_ARGC_INDEX_ENHANCE("tcp-port");
        if (index != 0xFF)
        {
            ipuc_info.flag|= CTC_IPUC_NAT_FLAG_USE_TCP_PORT;
        }
    }
    index = CTC_CLI_GET_ARGC_INDEX("lchip");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("lchip", lchip, argv[index + 1]);
    }
    lchip = g_ctcs_api_en?g_api_lchip:lchip;

    index = CTC_CLI_GET_ARGC_INDEX("hit");
    if (0xFF != index)
    {
        CTC_CLI_GET_UINT8("hit", hit, argv[index + 1]);
        hit = hit ? 1 : 0;
    }

    if(g_ctcs_api_en)
    {
        ret = ctcs_ipuc_set_natsa_entry_hit(lchip, &ipuc_info, hit);
    }
    else
    {
        ret = ctc_ipuc_set_natsa_entry_hit(&ipuc_info, hit);
    }
    if (ret)
    {
        ctc_cli_out("%% %s \n\r", ctc_get_error_desc(ret));
        return CLI_ERROR;
    }
    return CLI_SUCCESS;
}

int32
ctc_goldengate_ipuc_cli_init(void)
{

#ifdef SDK_INTERNAL_CLI_SHOW
    install_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_offset_show_cmd);
    install_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_write_lpm_by_dma_cmd);
    install_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_rpf_check_port_cmd);
    install_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_show_prefix_mode_cmd);
    install_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_default_entry_cmd);
    install_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_set_tcam_cmd);
    install_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_show_ipv4_lpm_info_cmd);
    install_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_show_ipv6_lpm_info_cmd);
#endif

    install_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_arrange_fragment_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv6_cmd);
 /*    install_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_count_cmd);*/
    install_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_state_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv4_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv4_info_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv6_info_cmd);
    install_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv4_nat_sa_cmd);
    install_element(CTC_SDK_MODE,  &ctc_cli_gg_ipuc_get_nat_da_hit_cmd);
    install_element(CTC_SDK_MODE,  &ctc_cli_gg_ipuc_set_nat_da_hit_cmd);
    install_element(CTC_SDK_MODE,  &ctc_cli_gg_ipuc_get_nat_sa_hit_cmd);
    install_element(CTC_SDK_MODE,  &ctc_cli_gg_ipuc_set_nat_sa_hit_cmd);

    return CLI_SUCCESS;
}

int32
ctc_goldengate_ipuc_cli_deinit(void)
{

#ifdef SDK_INTERNAL_CLI_SHOW
    uninstall_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_offset_show_cmd);
    uninstall_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_write_lpm_by_dma_cmd);
    uninstall_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_rpf_check_port_cmd);
    uninstall_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_show_prefix_mode_cmd);
    uninstall_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_default_entry_cmd);
    uninstall_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_show_ipv4_lpm_info_cmd);
    uninstall_element(CTC_INTERNAL_MODE, &ctc_cli_gg_ipuc_show_ipv6_lpm_info_cmd);
#endif

    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_arrange_fragment_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv6_cmd);
 /*    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_count_cmd);*/
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_state_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv4_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv4_info_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv6_info_cmd);
    uninstall_element(CTC_SDK_MODE, &ctc_cli_gg_ipuc_show_ipv4_nat_sa_cmd);

    return CLI_SUCCESS;
}

