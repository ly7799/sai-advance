/*sai include file*/
#include "sai.h"
#include "saitypes.h"
#include "saistatus.h"
#include "saitwamp.h"

/*ctc_sai include file*/
#include "ctc_sai.h"
#include "ctc_sai_oid.h"
#include "ctc_sai_db.h"
#include "ctc_sai_port.h"
#include "ctc_sai_next_hop.h"
#include "ctc_sai_twamp.h"

/*sdk include file*/
#include "ctcs_api.h"


static sai_object_id_t twamp_group_acl_group_oid = 0;
static uint32_t twamp_mep_index = 0;

uint8 twamp_pkt_ipv4_header[128] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x08, 0x00, 0x45, 0x00,
    0x00, 0x52, 0x00, 0x00, 0x00, 0x00, 0xFD, 0x11, 0x8F, 0x81, 0x0C, 0x0C, 0x0C, 0x02, 0x0B, 0x0B,
    0x0B, 0x01, 0x00, 0x3E, 0x00, 0x3F, 0x00, 0x31, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28,
};

uint8 twamp_pkt_ipv6_header[128] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x86, 0xDD, 0x60, 0x00,
    0x00, 0x00, 0x00, 0x5C, 0x11, 0x36, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xC0, 0x3C, 0x12, 0xB5, 0x00, 0x5C, 0x00, 0x00, 0x01, 0x02,
    0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x28,
};

static sai_status_t
_ctc_sai_twamp_test_ipv4_packet(ctc_sai_twamp_attr_t *p_twamp_attr, ctc_npm_cfg_t *p_npm)
{
    p_npm->pkt_format.pkt_header = (void*)twamp_pkt_ipv4_header;
    p_npm->pkt_format.header_len = 83;

    return SAI_STATUS_SUCCESS;
}

static sai_status_t
_ctc_sai_twamp_test_ipv6_packet(ctc_sai_twamp_attr_t *p_twamp_attr, ctc_npm_cfg_t *p_npm)
{
    p_npm->pkt_format.pkt_header = (void*)twamp_pkt_ipv6_header;
    p_npm->pkt_format.header_len = 103;

    return SAI_STATUS_SUCCESS;
}

static int32
_ctc_sai_twamp_build_maid_twamp(ctc_oam_maid_t* maid, char* md_name, char* ma_name)
{
    uint8 md_name_len = 0;
    uint8 ma_name_len = 0;
    uint8 maid_len = 0;

    md_name_len = sal_strlen(md_name);
    ma_name_len = sal_strlen(ma_name);
    if (md_name_len > 43)
    {
        return -1;
    }
    else
    {
        if (ma_name_len > (44 - md_name_len))
        {
            return -1;
        }
    }

    maid->maid[0] = 0x4;  /* MD name format */
    maid->maid[1] = md_name_len; /* MD name len */
    sal_memcpy(&maid->maid[2], md_name, md_name_len); /* copy md name string */

    maid->maid[2 + md_name_len] = 0x2; /* MA name format */
    maid->maid[3 + md_name_len] = ma_name_len; /* MA name len */
    sal_memcpy(&maid->maid[4 + md_name_len], ma_name, ma_name_len);

    maid_len = 4 + md_name_len + ma_name_len;
    maid->maid_len = maid_len;

    return 0;
}


static sai_status_t
_ctc_sai_twamp_create_oam_enable_maid_bridge_mac(ctc_sai_twamp_attr_t *p_twamp_attr, sai_object_id_t switch_id, uint32_t* p_mep_type)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    uint8           lchip = 0;
    int32           ret = 0;
    uint32          global_port = 0;
    uint8           enable = 0;
    ctc_direction_t dir = 0;
    ctc_oam_property_t      oam_prop;
    ctc_oam_y1731_prop_t    oam_eth_prop;

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);
    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(switch_id, &lchip));

    if (SAI_TWAMP_SESSION_SENDER == p_twamp_attr->direction)
    {
        //sai_twamp_session_direction_t
        dir = CTC_BOTH_DIRECTION;
    }

    if (TRUE == p_twamp_attr->trans_enable)
    {
        enable = 1;
    }

    if (p_twamp_attr->port_id)
    {
        CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_gport(p_twamp_attr->port_id, &global_port));
    }

    sal_memset(&oam_prop, 0, sizeof(ctc_oam_property_t));
    sal_memset(&oam_eth_prop, 0, sizeof(ctc_oam_y1731_prop_t));

    oam_eth_prop.cfg_type   = CTC_OAM_Y1731_CFG_TYPE_PORT_OAM_EN;
    oam_eth_prop.gport      = global_port;
    oam_eth_prop.dir        = dir;
    oam_eth_prop.value      = enable;
    sal_memcpy(&oam_prop.u.y1731, &oam_eth_prop, sizeof(ctc_oam_y1731_prop_t));
    ret = ctcs_oam_set_property(lchip, &oam_prop);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    // set bridge mac
    mac_addr_t          mac_addr;
    uint8               oam_bridge_mac[6] = {4,7,8,9};
    int32 rc = 0;

    sal_memset(&oam_prop, 0, sizeof(ctc_oam_property_t));
    sal_memset(&mac_addr, 0, sizeof(mac_addr_t));

    oam_prop.oam_pro_type = CTC_OAM_PROPERTY_TYPE_Y1731;
    oam_prop.u.y1731.cfg_type = CTC_OAM_Y1731_CFG_TYPE_BRIDGE_MAC;
    sal_memcpy(&mac_addr, oam_bridge_mac, sizeof(mac_addr_t));
    oam_prop.u.y1731.p_value = &mac_addr;

    ret = ctcs_oam_set_property(lchip, &oam_prop);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    ctc_oam_maid_t maid;
    char  md_name[] = "npm";
    char  ma_name[] = "twamp";

    sal_memset(&maid, 0, sizeof(maid));
    maid.mep_type = CTC_OAM_MEP_TYPE_ETH_1AG;

    /* build MAID string */
    rc = _ctc_sai_twamp_build_maid_twamp(&maid, md_name, ma_name);
    if (rc)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%%invalid md name or ma name \n");
    }

    ret = ctcs_oam_add_maid(lchip, &maid);
    if (CTC_E_EXIST == ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% twamp oam maid exist \n");
    }
    else if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% twamp add oam maid fail \n");
        status = ctc_sai_mapping_error_ctc(ret);
    }

    p_twamp_attr->mep_type = maid.mep_type;
    *p_mep_type = maid.mep_type;

    return status;
}

static sai_status_t
_ctc_sai_twamp_remove_oam_disable_maid_bridge_mac(ctc_sai_twamp_attr_t *p_twamp_attr, sai_object_id_t session_id)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    uint8           lchip = 0;
    int32           ret = 0;
    uint32          global_port = 0;
    uint8           enable = 0;
    ctc_direction_t dir = 0;
    ctc_oam_maid_t  maid;
    ctc_oam_property_t      oam_prop;
    ctc_oam_y1731_prop_t    oam_eth_prop;
    mac_addr_t              mac_addr;
    char  md_name[128] = {};
    char  ma_name[128] = {};

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);
    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(session_id, &lchip));

    // remove oam  maid


    sal_memset(&maid, 0, sizeof(maid));
    maid.mep_type = CTC_OAM_MEP_TYPE_ETH_1AG;
    sal_memcpy(md_name, p_twamp_attr->md_name, sizeof(p_twamp_attr->md_name));
    sal_memcpy(ma_name, p_twamp_attr->ma_name, sizeof(p_twamp_attr->ma_name));
    ret = _ctc_sai_twamp_build_maid_twamp(&maid, md_name, ma_name);
    /* build MAID string */
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%%invalid md name or ma name \n");
    }
    else
    {

    }

    ret = ctcs_oam_remove_maid(lchip, &maid);
    if (CTC_E_NOT_EXIST == ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% twamp oam maid exist \n");
    }
    else if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    // set bridge mac to zero
    sal_memset(&oam_prop, 0, sizeof(ctc_oam_property_t));
    sal_memset(&mac_addr, 0, sizeof(mac_addr_t));

    oam_prop.oam_pro_type = CTC_OAM_PROPERTY_TYPE_Y1731;
    oam_prop.u.y1731.cfg_type = CTC_OAM_Y1731_CFG_TYPE_BRIDGE_MAC;
    oam_prop.u.y1731.p_value  = &mac_addr;

    ret = ctcs_oam_set_property(lchip, &oam_prop);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    // diable oam cfm maid
    if (SAI_TWAMP_SESSION_SENDER == p_twamp_attr->direction)
    {
        dir = CTC_BOTH_DIRECTION;
    }

    if (FALSE == p_twamp_attr->trans_enable)
    {
        enable = 0;
    }

    if (p_twamp_attr->port_id)
    {
        CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_gport(p_twamp_attr->port_id, &global_port));
    }

    sal_memset(&oam_prop, 0, sizeof(ctc_oam_property_t));
    sal_memset(&oam_eth_prop, 0, sizeof(ctc_oam_y1731_prop_t));

    oam_eth_prop.cfg_type   = CTC_OAM_Y1731_CFG_TYPE_PORT_OAM_EN;
    oam_eth_prop.gport      = global_port;
    oam_eth_prop.dir        = dir;
    oam_eth_prop.value      = enable;

    sal_memcpy(&oam_prop.u.y1731, &oam_eth_prop, sizeof(ctc_oam_y1731_prop_t));
    ret = ctcs_oam_set_property(lchip, &oam_prop);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    return status;
}

static sai_status_t
_ctc_sai_twamp_create_oam_mep_index(ctc_sai_twamp_attr_t *p_twamp_attr, sai_object_id_t switch_id, uint32_t* mep_index)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    uint8           lchip = 0;
    int32           ret = 0;
    uint32          global_port = 0;
    ctc_oam_maid_t  maid;
    char            md_name[] = "npm";
    char            ma_name[] = "twamp";
    ctc_oam_lmep_t  lmep;
    ctc_oam_y1731_lmep_t* p_y1731_lmep  = &lmep.u.y1731_lmep;

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);

    sal_memset(&lmep, 0, sizeof(lmep));
    sal_memset(&maid, 0, sizeof(maid));

    // set oam lookup channel
    maid.mep_type = CTC_OAM_MEP_TYPE_ETH_1AG; // need to get from ctcs_oam_add_maid
    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(switch_id, &lchip));
    if (p_twamp_attr->port_id)
    {
        CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_gport(p_twamp_attr->port_id, &global_port));
        lmep.key.u.eth.gport = global_port; // diff with loop port
        lmep.key.u.eth.md_level = 3;        // make it fix
    }

    lmep.key.u.eth.vlan_id = TWAMP_ADD_MEP_KEY_VLAN_ID;

    // TODO, init md name, and save into db of p_twamp_attr
    ret = _ctc_sai_twamp_build_maid_twamp(&maid, md_name, ma_name);
    /* build MAID string */
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%%invalid md name or ma name \n");
    }
    else
    {
        sal_memcpy(&lmep.maid, &maid, sizeof(maid));
        sal_memcpy(p_twamp_attr->md_name, md_name, sizeof(md_name));
        sal_memcpy(p_twamp_attr->ma_name, ma_name, sizeof(ma_name));
    }

    p_y1731_lmep->tpid_index = CTC_PARSER_L2_TPID_SVLAN_TPID_0;
    p_y1731_lmep->ccm_interval = TWAMP_OAM_CCM_INTERVAL;
    p_y1731_lmep->mep_id = TWAMP_ADD_MEP_ID;                    // make it fix
    p_y1731_lmep->flag |= CTC_OAM_Y1731_LMEP_FLAG_MEP_EN;       /* FLAG */
    p_y1731_lmep->flag |= CTC_OAM_Y1731_LMEP_FLAG_TWAMP_EN;     /* TWAMP FLAG */
    if (SAI_TWAMP_MODE_TWAMP_LIGHT == p_twamp_attr->session_mode)
    {
        p_y1731_lmep->flag |= CTC_OAM_Y1731_LMEP_FLAG_TWAMP_REFLECT_EN;
    }

    ret = ctcs_oam_add_lmep(lchip, &lmep);
    if (CTC_E_EXIST == ret)
    {
        lmep.lmep_index = twamp_mep_index;
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "% md name or ma name have exist \n");
    }
    else if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%%invalid md name or ma name \n");
    }
    else
    {
        twamp_mep_index = lmep.lmep_index;
    }

    *mep_index = lmep.lmep_index;

    return status;
}

static sai_status_t
_ctc_sai_twamp_remove_oam_mep_index(ctc_sai_twamp_attr_t *p_twamp_attr, sai_object_id_t session_id)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    uint8           lchip = 0;
    int32           ret = 0;
    uint32          global_port = 0;
    ctc_oam_lmep_t  lmep;

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);

    sal_memset(&lmep, 0, sizeof(lmep));
    lmep.key.mep_type = CTC_OAM_MEP_TYPE_ETH_1AG;
    if (p_twamp_attr->port_id)
    {
        CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_gport(p_twamp_attr->port_id, &global_port));
        lmep.key.u.eth.gport = global_port; // diff with loop port
        lmep.key.u.eth.md_level = 3;        // make it fix
    }

    //lmep.u.y1731_lmep.master_gchip = p_oam_lmep->u.y1731_lmep.master_gchip;
    lmep.key.u.eth.md_level = 3;        // make it fix
    lmep.u.y1731_lmep.flag |= CTC_OAM_Y1731_LMEP_FLAG_TWAMP_EN;
    if (SAI_TWAMP_MODE_TWAMP_LIGHT == p_twamp_attr->session_mode)
    {
        lmep.u.y1731_lmep.flag |= CTC_OAM_Y1731_LMEP_FLAG_TWAMP_REFLECT_EN;
    }

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(session_id, &lchip));
    ret = ctcs_oam_remove_lmep(lchip, &lmep);
    if (ret < 0)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    return status;
}

static sai_status_t
_ctc_sai_twamp_update_oam_mep(ctc_sai_twamp_attr_t *p_twamp_attr, sai_object_id_t session_id)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    uint8           lchip = 0;
    int32           ret = 0;    
    uint32          oam_session_id = 0xff;
    uint32          global_port = 0;
    ctc_oam_update_t    lmep_update;

    // if node is twamp sender, local-mep npm level 3 session 0 enable
    sal_memset(&lmep_update, 0, sizeof(lmep_update));
    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(session_id, &lchip));
    lmep_update.is_local = 1;
    if (TRUE == p_twamp_attr->trans_enable)
    {
        oam_session_id = session_id;
    }
    else
    {
        oam_session_id = 0xff;
    }

    if (p_twamp_attr->port_id)
    {
        CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_gport(p_twamp_attr->port_id, &global_port));
        lmep_update.key.u.eth.gport = global_port; // diff with loop port
    }

    lmep_update.key.u.eth.vlan_id = TWAMP_ADD_MEP_KEY_VLAN_ID;
    lmep_update.key.u.eth.md_level = 3;        // make it fix
    lmep_update.update_type    = CTC_OAM_Y1731_LMEP_UPDATE_TYPE_NPM;
    lmep_update.update_value   = oam_session_id;
    ret = ctcs_oam_update_lmep(lchip, &lmep_update);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    return status;
}


sai_status_t
ctc_sai_twamp_acl_stats_create(uint32_t *stats_id)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    int32           rc = 0;
    uint8           lchip = 0;
    twamp_stats_param_t     param;
    ctc_stats_statsid_t     ctc_stats;

    CTC_SAI_PTR_VALID_CHECK(stats_id);

    sal_memset(&param, 0, sizeof(param));
    param.dir = CTC_INGRESS;
    param.type = CTC_STATS_STATSID_TYPE_ACL;

    sal_memcpy(&ctc_stats, &param, sizeof(param));
    rc = ctcs_stats_create_statsid(lchip, &ctc_stats);
    if (rc)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(rc));
        status = ctc_sai_mapping_error_ctc(rc);
    }

    *stats_id = ctc_stats.stats_id;
    return status;
}

sai_status_t 
ctc_sai_twamp_acl_stats_remove(uint32_t stats_id)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    int32           rc = 0;
    uint8           lchip = 0;

    rc = ctcs_stats_destroy_statsid(lchip, stats_id);
    if (rc)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(rc));
        status = ctc_sai_mapping_error_ctc(rc);
    }

    return status;
}

static sai_status_t
ctc_sai_twamp_acl_build_param(ctc_sai_twamp_attr_t *p_twamp_attr, twamp_acl_param_t *acl_entry)
{

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);
    CTC_SAI_PTR_VALID_CHECK(acl_entry);
    
    /* Mapping keys */
    if (SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->dst_ip.addr_family)
    {
        acl_entry->key.u.ipv4_key.l3_type = CTC_PARSER_L3_TYPE_IPV4;
        acl_entry->key.u.ipv4_key.l3_type_mask = 0xf;
        acl_entry->key.u.ipv4_key.eth_type = 0x0800;
        acl_entry->key.u.ipv4_key.eth_type_mask = 0xFFFF;
    }
    else
    {
        acl_entry->key.u.ipv4_key.l3_type = CTC_PARSER_L3_TYPE_IPV6;
        acl_entry->key.u.ipv4_key.l3_type_mask = 0xf;
        acl_entry->key.u.ipv4_key.eth_type = 0x86DD;
        acl_entry->key.u.ipv4_key.eth_type_mask = 0xFFFF;
    }

    CTC_SET_FLAG(acl_entry->key.u.ipv4_key.flag, CTC_ACL_IPV4_KEY_FLAG_L3_TYPE);

    if (SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->dst_ip.addr_family)
    {
        CTC_SET_FLAG(acl_entry->key.u.ipv4_key.flag, CTC_ACL_IPV4_KEY_FLAG_L3_TYPE);
        acl_entry->key.u.ipv4_key.l3_type = CTC_PARSER_L3_TYPE_IPV4;
        acl_entry->key.u.ipv4_key.l3_type_mask = 0xf;
        CTC_SET_FLAG(acl_entry->key.u.ipv4_key.flag, CTC_ACL_IPV4_KEY_FLAG_IP_SA);

        sal_memcpy(&acl_entry->key.u.ipv4_key.ip_sa, &p_twamp_attr->src_ip.addr.ip4, sizeof(sai_ip4_t));
        acl_entry->key.u.ipv4_key.ip_sa_mask = 0xFFFFFFFF;

        CTC_SET_FLAG(acl_entry->key.u.ipv4_key.flag, CTC_ACL_IPV4_KEY_FLAG_L3_TYPE);
        acl_entry->key.u.ipv4_key.l3_type = CTC_PARSER_L3_TYPE_IPV4;
        acl_entry->key.u.ipv4_key.l3_type_mask = 0xf;
        CTC_SET_FLAG(acl_entry->key.u.ipv4_key.flag, CTC_ACL_IPV4_KEY_FLAG_IP_DA);

        sal_memcpy(&acl_entry->key.u.ipv4_key.ip_da, &p_twamp_attr->dst_ip.addr.ip4, sizeof(sai_ip4_t));
        acl_entry->key.u.ipv4_key.ip_da_mask = 0xFFFFFFFF;

        CTC_SET_FLAG(acl_entry->key.u.ipv4_key.flag, CTC_ACL_IPV4_KEY_FLAG_L3_TYPE);
        acl_entry->key.u.ipv4_key.l3_type = CTC_PARSER_L3_TYPE_IPV4;
        acl_entry->key.u.ipv4_key.l3_type_mask = 0xf;
        CTC_SET_FLAG(acl_entry->key.u.ipv4_key.flag, CTC_ACL_IPV4_KEY_FLAG_L4_PROTOCOL);
        acl_entry->key.u.ipv4_key.l4_protocol = 6;
        acl_entry->key.u.ipv4_key.l4_protocol_mask = 0xFF;
    }
    else if (SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->dst_ip.addr_family)
    {

    }

    if (p_twamp_attr->priority)
    {
        acl_entry->key.u.ipv4_key.dscp = p_twamp_attr->priority;
        acl_entry->key.u.ipv4_key.dscp_mask = 0xFF;
    }

    if (p_twamp_attr->udp_dst_port)
    {
        CTC_SET_FLAG(acl_entry->key.u.ipv4_key.sub_flag, CTC_ACL_IPV4_KEY_SUB_FLAG_L4_DST_PORT);
        acl_entry->key.u.ipv4_key.l4_dst_port_use_mask = 1;
        acl_entry->key.u.ipv4_key.l4_dst_port_0 = p_twamp_attr->udp_dst_port;
        acl_entry->key.u.ipv4_key.l4_dst_port_1 = 0xFFFF;
    }

    if (p_twamp_attr->udp_src_port)
    {
        CTC_SET_FLAG(acl_entry->key.u.ipv4_key.sub_flag, CTC_ACL_IPV4_KEY_SUB_FLAG_L4_SRC_PORT);
        acl_entry->key.u.ipv4_key.l4_src_port_use_mask = 1;
        acl_entry->key.u.ipv4_key.l4_src_port_0 = p_twamp_attr->udp_src_port;
        acl_entry->key.u.ipv4_key.l4_src_port_1 = 0xFFFF;
    }


    if (SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->dst_ip.addr_family)
    {
        acl_entry->key.type = CTC_ACL_KEY_IPV4;
        acl_entry->key.u.ipv4_key.key_size = CTC_ACL_KEY_SIZE_DOUBLE;
    }
    else if (SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->dst_ip.addr_family)
    {

    }

    // TODO, need to remove the acl entry for delete twamp session

    uint32_t stats_id = 0;

    ctc_sai_twamp_acl_stats_create(&stats_id);
    acl_entry->action.stats_id = stats_id;
    CTC_SET_FLAG(acl_entry->action.flag, CTC_ACL_ACTION_FLAG_STATS);

    p_twamp_attr->acl_stats_id = stats_id;

    return SAI_STATUS_SUCCESS;
}

// need to check twamp loop in tm, diff with tm2
static sai_status_t
_ctc_sai_twamp_create_iloop_nexthop_for_hw_lookup(ctc_sai_twamp_attr_t *p_twamp_attr, sai_object_id_t switch_id)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    int32           ret = 0;
    uint32          nhid = 0;
    uint8           lchip = 0;
    uint8           gchip = 0;
    uint32          l3if_id = 0;
    uint32          global_port = 0;
    ctc_l3if_t      l3if;
    ctc_l3if_property_t l3if_prop;
    ctc_internal_port_assign_para_t port_assign;

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);

    sal_memset(&port_assign, 0, sizeof(port_assign));
    port_assign.type = CTC_INTERNAL_PORT_TYPE_ILOOP;
    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(switch_id, &lchip));
    if(CTC_CHIP_TSINGMA == ctcs_get_chip_type(lchip)) 
    {
        /*alloc global iloop port */
        CTC_SAI_CTC_ERROR_RETURN(ctcs_get_gchip_id(lchip, &gchip));
        port_assign.gchip = gchip;
    }

    ret = ctcs_alloc_internal_port(lchip, &port_assign);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%%The allocate internal port is %d\n", port_assign.inter_port);

    /*config inner l3if */
    sal_memset(&l3if, 0, sizeof(ctc_l3if_t));
    ret = ctc_sai_db_alloc_id(lchip, CTC_SAI_DB_ID_TYPE_L3IF, &l3if_id);

    l3if.l3if_type = CTC_L3IF_TYPE_PHY_IF;
    l3if.gport = port_assign.inter_port;
    ret = ctcs_l3if_create(lchip, l3if_id, &l3if);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    l3if_prop = CTC_L3IF_PROP_ROUTE_EN;
    ret = ctcs_l3if_set_property(lchip, l3if_id, l3if_prop, 1);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    l3if_prop = CTC_L3IF_PROP_IPV4_UCAST;
    ret = ctcs_l3if_set_property(lchip, l3if_id, l3if_prop, 1);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    l3if_prop = CTC_L3IF_PROP_IPV4_MCAST;
    ret = ctcs_l3if_set_property(lchip, l3if_id, l3if_prop, 1);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    l3if_prop = CTC_L3IF_PROP_IPV6_UCAST;
    ret = ctcs_l3if_set_property(lchip, l3if_id, l3if_prop, 1);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    l3if_prop = CTC_L3IF_PROP_ROUTE_ALL_PKT;
    ret = ctcs_l3if_set_property(lchip, l3if_id, l3if_prop, 1);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    l3if_prop = CTC_L3IF_PROP_VRF_EN;
    ret = ctcs_l3if_set_property(lchip, l3if_id, l3if_prop, 1);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    // for vpn instance
    if (p_twamp_attr->vrf_id)
    {
        l3if_prop = CTC_L3IF_PROP_VRF_ID;
        ret = ctcs_l3if_set_property(lchip, l3if_id, l3if_prop, p_twamp_attr->vrf_id);
        if (ret)
        {
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
            status = ctc_sai_mapping_error_ctc(ret);
        }
    }

    ret = ctcs_port_set_phy_if_en(lchip, port_assign.inter_port, 1);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    // save the l3if id
    p_twamp_attr->l3if_id = l3if_id;

    //  need to delete, inner alloc
    if (FALSE == p_twamp_attr->hw_lookup)
    {
        if (p_twamp_attr->port_id)
        {
            CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_gport(p_twamp_attr->port_id, &global_port));
        }
    }
    else
    {
        global_port = port_assign.inter_port;
    }

    if (SAI_TWAMP_SESSION_SENDER == p_twamp_attr->role)
    {
        ctc_loopback_nexthop_param_t iloop_nh;

        sal_memset(&iloop_nh, 0, sizeof(ctc_loopback_nexthop_param_t));

        ret = ctc_sai_db_alloc_id(lchip, CTC_SAI_DB_ID_TYPE_NEXTHOP, &nhid);
        iloop_nh.lpbk_lport = port_assign.inter_port;
        ret = ctcs_nh_add_iloop(lchip, nhid, &iloop_nh);
        if (ret)
        {
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
            status = ctc_sai_mapping_error_ctc(ret);
        }

        p_twamp_attr->iloop_port = port_assign.inter_port;
        p_twamp_attr->nexthop_id = nhid;
    }
    else if (SAI_TWAMP_SESSION_REFLECTOR == p_twamp_attr->role)
    {
        ctc_misc_nh_param_t nh_param;

        sal_memset(&nh_param, 0, sizeof(ctc_misc_nh_param_t));

        ret = ctc_sai_db_alloc_id(lchip, CTC_SAI_DB_ID_TYPE_NEXTHOP, &nhid);
        nh_param.gport = global_port;
        nh_param.misc_param.flex_edit.flag |= CTC_MISC_NH_FLEX_EDIT_REPLACE_L2_HDR;
        nh_param.misc_param.flex_edit.flag |= CTC_MISC_NH_FLEX_EDIT_SWAP_MACDA;

         /*ipv4*/
        nh_param.misc_param.flex_edit.flag |= CTC_MISC_NH_FLEX_EDIT_REPLACE_IP_HDR;
        nh_param.misc_param.flex_edit.flag |= CTC_MISC_NH_FLEX_EDIT_IPV4;
        nh_param.misc_param.flex_edit.flag |= CTC_MISC_NH_FLEX_EDIT_SWAP_IP;
        ret = ctcs_nh_add_misc(lchip, nhid, &nh_param);
        if (ret)
        {
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
            status = ctc_sai_mapping_error_ctc(ret);
        }

        p_twamp_attr->nexthop_id = nhid;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t
_ctc_sai_twamp_remove_iloop_nexthop_for_hw_lookup(ctc_sai_twamp_attr_t *p_twamp_attr, sai_object_id_t switch_id)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    int32           ret = 0;
    uint8           lchip = 0;
    uint8           gchip = 0;
    uint32          l3if_id = 0;
    uint32          global_port = 0;
    ctc_l3if_t      l3if;
    ctc_internal_port_assign_para_t port_assign;

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);

    sal_memset(&port_assign, 0, sizeof(port_assign));
    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(switch_id, &lchip));
    if(CTC_CHIP_TSINGMA == ctcs_get_chip_type(lchip)) 
    {
        CTC_SAI_CTC_ERROR_RETURN(ctcs_get_gchip_id(lchip, &gchip));
    }

    if (SAI_TWAMP_SESSION_SENDER == p_twamp_attr->role)
    {
        ret =  ctcs_nh_remove_iloop(lchip, p_twamp_attr->nexthop_id);
        if (ret)
        {
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
            status = ctc_sai_mapping_error_ctc(ret);
        }

        status = ctc_sai_db_free_id(lchip, CTC_SAI_DB_ID_TYPE_NEXTHOP, p_twamp_attr->nexthop_id);
    }
    else if (SAI_TWAMP_SESSION_REFLECTOR == p_twamp_attr->role)
    {
        ret = ctcs_nh_remove_misc(lchip, p_twamp_attr->nexthop_id);
        if (ret)
        {
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
            status = ctc_sai_mapping_error_ctc(ret);
        }

        ctc_sai_db_free_id(lchip, CTC_SAI_DB_ID_TYPE_NEXTHOP, p_twamp_attr->nexthop_id);
    }

    /*config inner l3if */
    sal_memset(&l3if, 0, sizeof(ctc_l3if_t));

    l3if.l3if_type = CTC_L3IF_TYPE_PHY_IF;
    l3if.gport = p_twamp_attr->iloop_port;
    l3if_id = p_twamp_attr->l3if_id;
    ret = ctcs_l3if_destory(lchip, l3if_id, &l3if);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    ret = ctcs_port_set_phy_if_en(lchip, port_assign.inter_port, 0);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    status = ctc_sai_db_free_id(lchip, CTC_SAI_DB_ID_TYPE_L3IF, l3if_id);

    if (FALSE == p_twamp_attr->hw_lookup)
    {
        if (p_twamp_attr->port_id)
        {
            CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_gport(p_twamp_attr->port_id, &global_port));
        }
    }

    port_assign.gchip = gchip;
    port_assign.type = CTC_INTERNAL_PORT_TYPE_ILOOP;
    port_assign.inter_port = p_twamp_attr->iloop_port;
    CTC_SAI_CTC_ERROR_RETURN(ctcs_free_internal_port(lchip, &port_assign));

    CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%%The dealloc internal port is %d\n", port_assign.inter_port);

    return SAI_STATUS_SUCCESS;
}

sai_status_t
ctc_sai_twamp_global_ipv4_acl_add(sai_object_id_t twamp_session_id, ctc_sai_twamp_attr_t *p_twamp_attr)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    int32           rc = 0;
    uint32_t        entry_id = 0;
    uint32_t        session_id = 0;
    uint8           lchip = 0;
    ctc_acl_entry_t acl_entry;
    ctc_acl_oam_t   twamp_acl_oam;
    twamp_acl_param_t param;
    ctc_acl_field_action_t act_field;
    uint32_t        group_index = 0;

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);

    sal_memset(&acl_entry, 0, sizeof(acl_entry));
    sal_memset(&param, 0, sizeof(param));

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_id, &lchip));
    status = ctc_sai_twamp_acl_build_param(p_twamp_attr, &param);
    sal_memcpy(&acl_entry, &param, sizeof(twamp_acl_param_t));

     // refer to session id, do not alloc
    ctc_sai_oid_get_twamp_session_id(twamp_session_id, &session_id);
    entry_id = session_id + TWAMP_ACL_ENTRY_ID_BASE_INDEX;
    acl_entry.entry_id = entry_id;

    // save the entry id
    p_twamp_attr->acl_entry_id = entry_id;
    if (twamp_group_acl_group_oid)
    {
        group_index = twamp_group_acl_group_oid & 0xFFFFFFFF; 
    }

    if ((rc = ctcs_acl_add_entry(lchip, group_index, &acl_entry)))
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "acl add entry failed: %d\n", ctc_get_error_desc(rc));
        return rc;
    }

    sal_memset(&act_field, 0, sizeof(ctc_acl_field_action_t));
    sal_memset(&twamp_acl_oam, 0, sizeof(ctc_acl_oam_t));

    act_field.type = CTC_ACL_FIELD_ACTION_OAM;
    twamp_acl_oam.oam_type = CTC_ACL_OAM_TYPE_TWAMP;
    twamp_acl_oam.lmep_index = p_twamp_attr->lmep_index;

    // TODO need to save the packet offset
    twamp_acl_oam.packet_offset = 42;  // depend on the twamp session IP version
    act_field.ext_data = &twamp_acl_oam;
    rc = ctcs_acl_add_action_field(lchip, entry_id, &act_field);

    if ((rc = ctcs_acl_install_entry(lchip, acl_entry.entry_id)))
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "acl install entry failed: %d\n", ctc_get_error_desc(rc));
        goto err3;
    }

    return status;

err3:
    rc = ctcs_acl_remove_entry(lchip, acl_entry.entry_id);

    return rc;
}

sai_status_t
ctc_sai_twamp_global_ipv4_acl_del(sai_object_id_t twamp_session_id)
{
    uint8       lchip = 0;
    uint32_t    entry_id = 0;
    uint32_t    session_id = 0;
    int32       rc = 0;
    sai_status_t status = SAI_STATUS_SUCCESS;

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_id, &lchip));

     // refer to session id, do not alloc
    ctc_sai_oid_get_twamp_session_id(twamp_session_id, &session_id);
    entry_id = session_id + TWAMP_ACL_ENTRY_ID_BASE_INDEX;
    rc = ctcs_acl_uninstall_entry(lchip, entry_id);
    if (rc)
    {
        return SAI_STATUS_FAILURE;
    }

    // uninstall the entry, when disable the twamp session
    rc = ctcs_acl_remove_entry(lchip, entry_id);
    if (rc)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(rc));
        status = ctc_sai_mapping_error_ctc(rc);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
ctc_sai_twamp_global_ipv4_acl_enable(sai_object_id_t twamp_session_id)
{
    uint8       lchip = 0;
    uint32_t    entry_id = 0;
    uint32_t    session_id = 0;
    int32       rc = 0;
    sai_status_t status = SAI_STATUS_SUCCESS;

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_id, &lchip));

    ctc_sai_oid_get_twamp_session_id(twamp_session_id, &session_id);
    entry_id = session_id + TWAMP_ACL_ENTRY_ID_BASE_INDEX;

    rc = ctcs_acl_install_entry(lchip, entry_id);
    if (rc)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(rc));
        status = ctc_sai_mapping_error_ctc(rc);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
ctc_sai_twamp_global_ipv4_acl_disable(sai_object_id_t twamp_session_id)
{
    uint8       lchip = 0;
    uint32_t    entry_id = 0;
    uint32_t    session_id = 0;
    int32       rc = 0;
    sai_status_t status = SAI_STATUS_SUCCESS;

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_id, &lchip));

    ctc_sai_oid_get_twamp_session_id(twamp_session_id, &session_id);
    entry_id = session_id + TWAMP_ACL_ENTRY_ID_BASE_INDEX;

    rc = ctcs_acl_uninstall_entry(lchip, entry_id);
    if (rc)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(rc));
        status = ctc_sai_mapping_error_ctc(rc);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
ctc_sai_twamp_global_acl_init(void)
{
    sai_status_t            status = SAI_STATUS_SUCCESS;
    ctc_acl_group_info_t    acl_group;
    uint32_t    group_id = 0;
    int32       rc = 0;
    uint8       lchip = 0;
    uint32_t    group_index = 0;

    /* init twamp test packet global acl entry*/
    CTC_SAI_ERROR_GOTO(ctc_sai_db_alloc_id(lchip, CTC_SAI_DB_ID_TYPE_ACL_GROUP_INDEX, &group_index), status, error0);

    // save the acl group id
    twamp_group_acl_group_oid = group_index;  
    sal_memset(&acl_group, 0, sizeof(acl_group));
    acl_group.type     = CTC_ACL_GROUP_TYPE_GLOBAL;
    acl_group.dir      = CTC_INGRESS;
    group_id           = group_index & 0xFFFFFFFF;
    rc = ctcs_acl_create_group(lchip, group_id, &acl_group);
    rc = ctcs_acl_install_group(lchip, group_id, &acl_group);
    if (rc)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(rc));
        status = ctc_sai_mapping_error_ctc(rc);
    }

error0:
    return SAI_STATUS_SUCCESS;
}

sai_status_t
ctc_sai_twamp_init_session_max(void)
{
    ctc_npm_global_cfg_t  npm_glb_config;
    uint8 lchip = 0;

    sal_memset(&npm_glb_config, 0, sizeof(ctc_npm_global_cfg_t));
    npm_glb_config.session_mode = CTC_NPM_SESSION_MODE_4;
    CTC_SAI_CTC_ERROR_RETURN(ctcs_npm_set_global_config(lchip, &npm_glb_config));
    return SAI_STATUS_SUCCESS;
}

static sai_status_t
_ctc_sai_twamp_build_db(uint8 lchip, sai_object_id_t session_id, ctc_sai_twamp_t** twamp_info)
{
    sai_status_t            status = SAI_STATUS_SUCCESS;
    ctc_sai_twamp_t         *pst_twamp = NULL;

    pst_twamp = ctc_sai_db_get_object_property(lchip, session_id);
    if (NULL != pst_twamp)
    {
        return SAI_STATUS_ITEM_ALREADY_EXISTS;
    }

    pst_twamp = mem_malloc(MEM_OAM_MODULE, sizeof(ctc_sai_twamp_t));
    if (NULL == pst_twamp)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "no memory\n");
        return SAI_STATUS_NO_MEMORY;
    }

    sal_memset(pst_twamp, 0, sizeof(ctc_sai_twamp_t));

    status = ctc_sai_db_add_object_property(lchip, session_id, pst_twamp);
    if (CTC_SAI_ERROR(status))
    {
        mem_free(pst_twamp);
    }

    *twamp_info = pst_twamp;

    return status;
}

static sai_status_t
_ctc_sai_twamp_remove_db(uint8 lchip, sai_object_id_t session_oid)
{
    sai_status_t            status = SAI_STATUS_SUCCESS;
    ctc_sai_twamp_t         *pst_twamp = NULL;

    pst_twamp = ctc_sai_db_get_object_property(lchip, session_oid);
    if (NULL == pst_twamp)
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    status = ctc_sai_db_remove_object_property(lchip, session_oid);
    if (CTC_SAI_ERROR(status))
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "_ctc_sai_twamp_remove_db error!\n");
        return status;
    }

    mem_free(pst_twamp);
    return SAI_STATUS_SUCCESS;
}

sai_status_t
_ctc_sai_twamp_mapping_npm_session(ctc_sai_twamp_attr_t *p_twamp_attr, ctc_npm_cfg_t *p_npm_cfg)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    uint32          global_port = 0;
    uint32_t        ipv4_addr_tmp = 0;
    sai_object_id_t switch_id = 0;

    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);
    CTC_SAI_PTR_VALID_CHECK(p_npm_cfg);

    if (SAI_TWAMP_SESSION_MODE_UNAUTHENTICATED == p_twamp_attr->auth_mode)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%%auth_mode is unauthenticated \n");
    }
    else if (SAI_TWAMP_SESSION_MODE_AUTHENTICATED == p_twamp_attr->auth_mode)
    {
        status = SAI_STATUS_NOT_SUPPORTED;
    }
    else if (SAI_TWAMP_SESSION_MODE_ENCRYPTED == p_twamp_attr->auth_mode)
    {
        status = SAI_STATUS_NOT_SUPPORTED;
    }

    if (SAI_TWAMP_ENCAPSULATION_TYPE_IP == p_twamp_attr->encap_type)
    {
        // no need, nexthop id, just dest port
        if (TRUE == p_twamp_attr->hw_lookup)
        {
            status = _ctc_sai_twamp_create_iloop_nexthop_for_hw_lookup(p_twamp_attr, switch_id);
        }
    }
    else if (SAI_TWAMP_ENCAPSULATION_TYPE_L3_MPLS_VPN == p_twamp_attr->encap_type)
    {
        // need to check hw lookup is vaild
        if (TRUE == p_twamp_attr->hw_lookup)
        {
            status = _ctc_sai_twamp_create_iloop_nexthop_for_hw_lookup(p_twamp_attr, switch_id);
        }
    }
    else
    {
        status = SAI_STATUS_INVALID_PARAMETER;
    }

    if (p_twamp_attr->padding_length)
    {
        //TODO set the npm tx mode repeat, ranodm, etc
        status = SAI_STATUS_NOT_SUPPORTED;
    }

    if (p_twamp_attr->port_id)
    {
        if (FALSE == p_twamp_attr->hw_lookup)
        {
            CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_gport(p_twamp_attr->port_id, &global_port));
        }

        p_npm_cfg->dest_gport = global_port;
    }

    if (SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->dst_ip.addr_family)
    {
        p_npm_cfg->flag |= CTC_NPM_CFG_FLAG_TS_EN;
        p_npm_cfg->pkt_format.ts_offset = 46;   // need to check if ipv6 twamp is enable

        p_npm_cfg->flag |= CTC_NPM_CFG_FLAG_SEQ_EN;
        p_npm_cfg->pkt_format.seq_num_offset = 42;
    }
    else if (SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->dst_ip.addr_family)
    {
        //TODO
    }

    p_npm_cfg->pkt_format.ipg = 20;

    if (SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->dst_ip.addr_family)
    {
        if (p_twamp_attr->udp_dst_port)
        {
             *((uint16*)(&twamp_pkt_ipv4_header[34])) = p_twamp_attr->udp_dst_port;
        }

        if (p_twamp_attr->udp_src_port)
        {
             *((uint16*)(&twamp_pkt_ipv4_header[36])) = p_twamp_attr->udp_src_port;
        }

        // for modify the packet src ip
        sal_memcpy(&ipv4_addr_tmp, &p_twamp_attr->src_ip.addr.ip4, sizeof(sai_ip4_t));
         *((uint32*)(&twamp_pkt_ipv4_header[26])) = ipv4_addr_tmp;

        // for modify the packet dst ip
        ipv4_addr_tmp = 0;
        sal_memcpy(&ipv4_addr_tmp, &p_twamp_attr->dst_ip.addr.ip4, sizeof(sai_ip4_t));
         *((uint32*)(&twamp_pkt_ipv4_header[30])) = ipv4_addr_tmp;
    }
    else if (SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->dst_ip.addr_family)
    {
        if (p_twamp_attr->udp_dst_port)
        {
             *((uint16*)(&twamp_pkt_ipv6_header[34])) = p_twamp_attr->udp_dst_port;
        }

        if (p_twamp_attr->udp_src_port)
        {
             *((uint16*)(&twamp_pkt_ipv6_header[36])) = p_twamp_attr->udp_src_port;
        }

        // for modify the packet src ip
        sal_memcpy(&ipv4_addr_tmp, &p_twamp_attr->src_ip.addr.ip4, sizeof(sai_ip4_t));
         *((uint32*)(&twamp_pkt_ipv6_header[26])) = ipv4_addr_tmp;

        // for modify the packet dst ip
        ipv4_addr_tmp = 0;
        sal_memcpy(&ipv4_addr_tmp, &p_twamp_attr->dst_ip.addr.ip4, sizeof(sai_ip4_t));
         *((uint32*)(&twamp_pkt_ipv6_header[30])) = ipv4_addr_tmp;
    }

    // for init the twamp test packet
    if (SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV4 == p_twamp_attr->dst_ip.addr_family)
    {
        status = _ctc_sai_twamp_test_ipv4_packet(p_twamp_attr, p_npm_cfg);
    }
    else if (SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->src_ip.addr_family 
                || SAI_IP_ADDR_FAMILY_IPV6 == p_twamp_attr->dst_ip.addr_family)
    {
        status = _ctc_sai_twamp_test_ipv6_packet(p_twamp_attr, p_npm_cfg);
    }

    //TODO check the twamp sender rate
    if (SAI_TWAMP_SESSION_SENDER == p_twamp_attr->role)
    {
        // need to set the acl match,  diff with IPv4 or IPv6
    }
    else if (SAI_TWAMP_SESSION_REFLECTOR == p_twamp_attr->role)
    {
    }
    else
    {
        status = SAI_STATUS_INVALID_PARAMETER;
    }

    if (p_twamp_attr->tx_rate)
    {
        p_npm_cfg->tx_mode = CTC_NPM_TX_MODE_CONTINUOUS;
        p_npm_cfg->rate = p_twamp_attr->tx_rate;
    }

    if (p_twamp_attr->pkt_duration)
    {
        p_npm_cfg->tx_mode = CTC_NPM_TX_MODE_CONTINUOUS;
        p_npm_cfg->timeout  = p_twamp_attr->pkt_duration;
    }
    else
    {
        p_npm_cfg->tx_mode = CTC_NPM_TX_MODE_CONTINUOUS;
        p_npm_cfg->timeout  = 0;
    }

    if (p_twamp_attr->pkt_cnt)
    {
        p_npm_cfg->tx_mode = CTC_NPM_TX_MODE_PACKET_NUM;
        p_npm_cfg->packet_num = p_twamp_attr->pkt_cnt;
    }

    if (p_twamp_attr->period)
    {
        p_npm_cfg->tx_mode = CTC_NPM_TX_MODE_PERIOD;
        p_npm_cfg->tx_period = p_twamp_attr->period;
    }

    p_npm_cfg->pkt_format.frame_size_mode = 0;  //TODO, fix frame size
    p_npm_cfg->pkt_format.frame_size = TWAMP_FIXED_FRAME_SIZE;

    // default to use the pattern 
    p_npm_cfg->pkt_format.pattern_type = CTC_NPM_PATTERN_TYPE_REPEAT;
    p_npm_cfg->pkt_format.repeat_pattern = 0xFF;

    // twamp no need to loop for edit, check for the rfc2544 and 1564
    //npm_cfg.flag |= CTC_NPM_CFG_FLAG_ILOOP;

    // twamp sender need to record the stats
    p_npm_cfg->dm_stats_mode = 1;

    // need to check the timestamp format
    if (p_twamp_attr->timestamp_format)
    {
        p_npm_cfg->flag |= CTC_NPM_CFG_FLAG_NTP_TS;  // TODO , add the sai twamp attr
    }

    // force hw lookup, tx packet header encap, udp lookup with  ipda, 
    // MPLS PW lookup with mpls label, to the nexthop,
    // how to create the nexthop, two type:

    // 1. packet format, and egress port  2.  packet info to create nexthop    
    // 

    return status;
}

static sai_status_t
ctc_sai_twamp_parser_session_attr(uint32_t attr_count, const sai_attribute_t *attr_list, ctc_sai_twamp_attr_t *p_twamp_attr)
{
    sai_status_t            status = SAI_STATUS_SUCCESS;
    uint32_t                index = 0;
    const sai_attribute_value_t     *attr_value = NULL;

    // Get attr and check the param
    CTC_SAI_PTR_VALID_CHECK(p_twamp_attr);

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_PORT, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->port_id = attr_value->oid;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_SESSION_ROLE, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->role = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_UDP_SRC_PORT, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->udp_src_port = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_UDP_DST_PORT, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->udp_dst_port = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_SRC_IP, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        sal_memcpy(&p_twamp_attr->src_ip, &attr_value->ipaddr, sizeof(p_twamp_attr->src_ip));
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_DST_IP, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        sal_memcpy(&p_twamp_attr->dst_ip, &attr_value->ipaddr, sizeof(p_twamp_attr->dst_ip));
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_TC, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->priority = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_VPN_VIRTUAL_ROUTE, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->vrf_id = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_TWAMP_ENCAPSULATION_TYPE, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->encap_type = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_HW_LOOKUP_VALID, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->hw_lookup = attr_value->booldata;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_PADDING_LENGTH, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->padding_length = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_STATE, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->session_state = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_AUTH_MODE, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->auth_mode = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_NEXT_HOP_ID, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->nexthop_id = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_TX_PKT_PERIOD, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->period = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_TX_RATE, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->tx_rate = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_TX_PKT_CNT, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->pkt_cnt = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_TX_PKT_DURATION, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        // 10ms, 100ms, 1s, 30s
        p_twamp_attr->pkt_duration = attr_value->u64;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_MODE, &attr_value, &index);
    if(SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->session_mode = attr_value->u32;
    }

    status = ctc_sai_find_attrib_in_list(attr_count, attr_list, SAI_TWAMP_SESSION_ATTR_SESSION_ENABLE_TRANSMIT, &attr_value, &index);
    if (SAI_STATUS_SUCCESS == status)
    {
        p_twamp_attr->trans_enable = attr_value->booldata;
    }

    return status;
}

static sai_status_t
ctc_sai_twamp_create_twamp_session(sai_object_id_t *twamp_session_id,  sai_object_id_t switch_id, 
                        uint32_t attr_count, const sai_attribute_t *attr_list)
{
    sai_status_t            status = SAI_STATUS_SUCCESS;
    int32                   ret = 0;
    uint8                   lchip = 0;
    uint32_t                mep_type = 0;
    uint32_t                twamp_id = 0;
    uint32_t                mep_index = 0;
    sai_object_id_t         twamp_tmp_oid;
    ctc_npm_cfg_t           npm_cfg;
    ctc_sai_twamp_attr_t    twamp_attr;
    ctc_sai_twamp_t         *p_twamp_info = NULL;
    ctc_sai_twamp_t         *p_db_twamp = NULL;

    // Get attr and check the param
    CTC_SAI_PTR_VALID_CHECK(twamp_session_id);
    CTC_SAI_PTR_VALID_CHECK(attr_list);

    CTC_SAI_ERROR_RETURN(ctc_sai_db_alloc_id(lchip, CTC_SAI_DB_ID_TYPE_TWAMP, &twamp_id));
    twamp_tmp_oid = ctc_sai_create_object_id(SAI_OBJECT_TYPE_TWAMP, lchip, 0, 0, twamp_id);

    ctc_sai_oid_get_lchip(switch_id, &lchip);
    p_twamp_info = ctc_sai_db_get_object_property(lchip, twamp_tmp_oid);
    if (NULL != p_twamp_info)
    {
        status = SAI_STATUS_ITEM_ALREADY_EXISTS;
        goto error2;
    }

    sal_memset(&twamp_attr, 0, sizeof(ctc_sai_twamp_attr_t));

    status = ctc_sai_twamp_parser_session_attr(attr_count, attr_list, &twamp_attr);
    if (CTC_SAI_ERROR(status))
    {
        status = SAI_STATUS_INVALID_PARAMETER;
        goto error2;
    }

    CTC_SAI_CTC_ERROR_GOTO(_ctc_sai_twamp_create_oam_enable_maid_bridge_mac(&twamp_attr, switch_id, &mep_type),status, error2);
    CTC_SAI_CTC_ERROR_GOTO(_ctc_sai_twamp_create_oam_mep_index(&twamp_attr, switch_id, &mep_index),status, error2);

    // need to check with receive, alloc the new auto gen ptr
    if (SAI_TWAMP_SESSION_SENDER == twamp_attr.role)
    {
        CTC_SAI_CTC_ERROR_GOTO(_ctc_sai_twamp_update_oam_mep(&twamp_attr, twamp_tmp_oid),status, error2);
    }
    else
    {
        if (SAI_TWAMP_MODE_TWAMP_FULL == twamp_attr.session_mode)
        {
            CTC_SAI_CTC_ERROR_GOTO(_ctc_sai_twamp_update_oam_mep(&twamp_attr, twamp_tmp_oid),status, error2);
        }
    }

    // save the mep_type after _ctc_sai_twamp_create_oam_enable_maid_bridge_mac
    twamp_attr.mep_type = mep_type;

    // save the mep_index after _ctc_sai_twamp_create_oam_mep_index
    twamp_attr.lmep_index = mep_index;

    //  add cfm dsethmep when TM enable timestamp -- diff with TM2
    // mapping the npm session, before sdk 

    sal_memset(&npm_cfg, 0, sizeof(npm_cfg));
    if (SAI_TWAMP_SESSION_SENDER == twamp_attr.role)
    {
        if (SAI_IP_ADDR_FAMILY_IPV4 == twamp_attr.src_ip.addr_family 
                    || SAI_IP_ADDR_FAMILY_IPV4 == twamp_attr.dst_ip.addr_family)
        {
            CTC_SAI_CTC_ERROR_GOTO(_ctc_sai_twamp_mapping_npm_session(&twamp_attr, &npm_cfg),status, error2);
            ret = ctcs_npm_set_config(lchip, &npm_cfg);
            if (ret)
            {
                CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
                status = ctc_sai_mapping_error_ctc(ret);
                goto error2;
            }

            ret = ctc_sai_twamp_global_ipv4_acl_add(twamp_tmp_oid, &twamp_attr);
            if (ret)
            {
                CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
                status = ctc_sai_mapping_error_ctc(ret);
                goto error2;
            }
        }
        else if (SAI_IP_ADDR_FAMILY_IPV6 == twamp_attr.src_ip.addr_family 
                    || SAI_IP_ADDR_FAMILY_IPV6 == twamp_attr.dst_ip.addr_family)
        {
            //TODO
        }
    }

    if (SAI_TWAMP_SESSION_REFLECTOR == twamp_attr.role)
    {
        if (SAI_IP_ADDR_FAMILY_IPV4 == twamp_attr.src_ip.addr_family 
                    || SAI_IP_ADDR_FAMILY_IPV4 == twamp_attr.dst_ip.addr_family)
        {
            ret = ctc_sai_twamp_global_ipv4_acl_add(twamp_tmp_oid, &twamp_attr);
            if (ret)
            {
                CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
                status = ctc_sai_mapping_error_ctc(ret);
                goto error2;
            }
        }
        else if (SAI_IP_ADDR_FAMILY_IPV6 == twamp_attr.src_ip.addr_family 
                    || SAI_IP_ADDR_FAMILY_IPV6 == twamp_attr.dst_ip.addr_family)
        {
            //TODO
        }
    }

    // save into DB, after install sdk success
    CTC_SAI_ERROR_GOTO(_ctc_sai_twamp_build_db(lchip, twamp_tmp_oid, &p_twamp_info), status, error2);
    p_db_twamp = ctc_sai_db_get_object_property(lchip, twamp_tmp_oid);
    if (NULL != p_twamp_info)
    {
        sal_memcpy(&p_db_twamp->session_attr, &twamp_attr, sizeof(ctc_sai_twamp_attr_t));
    }

    *twamp_session_id = twamp_tmp_oid;

    return status;

error2:
    ctc_sai_db_free_id(lchip, SAI_OBJECT_TYPE_TWAMP, twamp_id);

    if (SAI_STATUS_SUCCESS != status)
    {
        CTC_SAI_LOG_ERROR(SAI_API_VLAN, "Failed to create twamp session entry:%d\n", status);
    }

    return status;
}


static sai_status_t
ctc_sai_twamp_remove_twamp_session(sai_object_id_t twamp_session_oid)
{
    ctc_sai_twamp_t     *p_twamp_info = NULL;
    sai_status_t        status = SAI_STATUS_SUCCESS;
    uint8               lchip = 0;
    uint32              session_id = 0;
    uint8               twmap_enable = 0;
    int32               ret = 0;
    sai_object_id_t     switch_id = 0;

    CTC_SAI_LOG_ENTER(SAI_API_TWAMP);

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_oid, &lchip));
    p_twamp_info = ctc_sai_db_get_object_property(lchip, twamp_session_oid);
    if (NULL == p_twamp_info)
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    if (SAI_TWAMP_ENCAPSULATION_TYPE_IP == p_twamp_info->session_attr.encap_type)
    {
        if (TRUE == p_twamp_info->session_attr.hw_lookup)
        {
            _ctc_sai_twamp_remove_iloop_nexthop_for_hw_lookup(&p_twamp_info->session_attr, switch_id);
        }
    }
    else if (SAI_TWAMP_ENCAPSULATION_TYPE_L3_MPLS_VPN == p_twamp_info->session_attr.encap_type)
    {
        if (TRUE == p_twamp_info->session_attr.hw_lookup)
        {
            _ctc_sai_twamp_remove_iloop_nexthop_for_hw_lookup(&p_twamp_info->session_attr, switch_id);
        }
    }

    status = ctc_sai_twamp_acl_stats_remove(p_twamp_info->session_attr.acl_stats_id);

    // clear the sdk config, disable the tranmit 
    status = ctc_sai_oid_get_twamp_session_id(twamp_session_oid, &session_id);
    CTC_SAI_CTC_ERROR_RETURN(ctcs_npm_set_transmit_en(lchip, session_id, twmap_enable));

    ret = ctc_sai_twamp_global_ipv4_acl_del(twamp_session_oid);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
    }

    CTC_SAI_CTC_ERROR_GOTO(_ctc_sai_twamp_remove_oam_disable_maid_bridge_mac(&p_twamp_info->session_attr, twamp_session_oid),status, error2);
    CTC_SAI_CTC_ERROR_GOTO(_ctc_sai_twamp_remove_oam_mep_index(&p_twamp_info->session_attr, twamp_session_oid),status, error2);

    // remove sai db before clear the sdk 
    status = _ctc_sai_twamp_remove_db(lchip, twamp_session_oid);
    if (SAI_STATUS_SUCCESS != status)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "Failed to remove twamp session :%d\n", status);
    }

    status = ctc_sai_db_free_id(lchip, CTC_SAI_DB_ID_TYPE_TWAMP, session_id);

error2:
    return status;
}

sai_status_t
ctc_sai_twamp_get_twamp_session_attr(sai_object_key_t *key, sai_attribute_t* attr, uint32 attr_idx)
{
    sai_status_t        status = SAI_STATUS_SUCCESS;
    uint8               lchip = 0;
    ctc_sai_twamp_t     *p_twamp_info = NULL;

    CTC_SAI_LOG_ENTER(SAI_API_TWAMP);

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(key->key.object_id, &lchip));
    p_twamp_info = ctc_sai_db_get_object_property(lchip, key->key.object_id);
    if (NULL == p_twamp_info)
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    switch(attr->id)
    {
        case SAI_TWAMP_SESSION_ATTR_PORT:
            attr->value.oid = p_twamp_info->session_attr.port_id;
            break;

        case SAI_TWAMP_SESSION_ATTR_SESSION_ROLE:
            attr->value.u32 = p_twamp_info->session_attr.role;
            break;

        case SAI_TWAMP_SESSION_ATTR_UDP_SRC_PORT:
            attr->value.u32 = p_twamp_info->session_attr.udp_src_port;
            break;

        case SAI_TWAMP_SESSION_ATTR_UDP_DST_PORT:
            attr->value.u32 = p_twamp_info->session_attr.udp_dst_port;
            break;

        case SAI_TWAMP_SESSION_ATTR_SRC_IP:
            sal_memcpy(&attr->value.ipaddr, &p_twamp_info->session_attr.src_ip, sizeof(sai_ip_address_t));
            break;

        case SAI_TWAMP_SESSION_ATTR_DST_IP:
            sal_memcpy(&attr->value.ipaddr, &p_twamp_info->session_attr.dst_ip, sizeof(sai_ip_address_t));
            break;

        case SAI_TWAMP_SESSION_ATTR_TC:
            attr->value.u32 = p_twamp_info->session_attr.priority;
            break;

        case SAI_TWAMP_SESSION_ATTR_VPN_VIRTUAL_ROUTE:
            attr->value.u32 = p_twamp_info->session_attr.vrf_id;
            break;

        case SAI_TWAMP_SESSION_ATTR_TWAMP_ENCAPSULATION_TYPE:
            attr->value.u32 = p_twamp_info->session_attr.encap_type;
            break;

        case SAI_TWAMP_SESSION_ATTR_SESSION_ENABLE_TRANSMIT:
            attr->value.booldata = p_twamp_info->session_attr.trans_enable;
            break;

        case SAI_TWAMP_SESSION_ATTR_HW_LOOKUP_VALID:
            attr->value.booldata = p_twamp_info->session_attr.hw_lookup;
            break;

        case SAI_TWAMP_SESSION_ATTR_PADDING_LENGTH:
            attr->value.u32 = p_twamp_info->session_attr.padding_length;
            break;

        case SAI_TWAMP_SESSION_ATTR_STATE:
            attr->value.u32 = p_twamp_info->session_attr.session_state;
            break;

        case SAI_TWAMP_SESSION_ATTR_AUTH_MODE:
            attr->value.u32 = p_twamp_info->session_attr.auth_mode;
            break;

        case SAI_TWAMP_SESSION_ATTR_NEXT_HOP_ID:
            attr->value.u32 = p_twamp_info->session_attr.nexthop_id;
            break;

        case SAI_TWAMP_SESSION_ATTR_TX_PKT_PERIOD:
            attr->value.u32 = p_twamp_info->session_attr.period;
            break;

        case SAI_TWAMP_SESSION_ATTR_TX_RATE:
            attr->value.u32 = p_twamp_info->session_attr.tx_rate;
            break;

        case SAI_TWAMP_SESSION_ATTR_TX_PKT_CNT:
            attr->value.u32 = p_twamp_info->session_attr.pkt_cnt;
            break;

        case SAI_TWAMP_SESSION_ATTR_TX_PKT_DURATION:
            attr->value.u64 = p_twamp_info->session_attr.pkt_duration;
            break;

        case SAI_TWAMP_SESSION_ATTR_MODE:
            attr->value.u32 = p_twamp_info->session_attr.session_mode;
            break;

        default:
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "Get twamp attribute not implement\n");
            return  SAI_STATUS_NOT_IMPLEMENTED + attr_idx;
    }

    return status;
}

static sai_status_t 
ctc_sai_twamp_set_twamp_session_attr(sai_object_key_t *key, const sai_attribute_t* attr)
{
    sai_status_t        status = SAI_STATUS_SUCCESS;
    ctc_sai_twamp_t     *p_twamp_info = NULL;
    uint8               lchip = 0;
    uint32              session_id = 0;
    uint8               twmap_enable = 0;
    uint8               session_tmp_id = 0;

    CTC_SAI_LOG_ENTER(SAI_API_TWAMP);

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(key->key.object_id, &lchip));
    CTC_SAI_DB_LOCK(lchip);
    p_twamp_info = ctc_sai_db_get_object_property(lchip, key->key.object_id);
    CTC_SAI_DB_UNLOCK(lchip);

    if (NULL == p_twamp_info)
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    // check the twamp session is disable and modify the attr
    if (SAI_TWAMP_SESSION_ATTR_SESSION_ENABLE_TRANSMIT != attr->id)
    {
        if (TRUE == p_twamp_info->session_attr.trans_enable)
        {
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "Need to disable twamp attribute of session transmit first\n");
            return  SAI_STATUS_FAILURE;
        }
        else
        {
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "To support set attr, need to remove twamp session first for updating all the attr, due to SDK API\n");
            return  SAI_STATUS_NOT_SUPPORTED;
        }
    }
    else
    {
        if (attr->value.booldata == p_twamp_info->session_attr.trans_enable)
        {
            return  SAI_STATUS_SUCCESS;
        }
    }

    /* need to remove the session , and create it again, due to SDK not clear the twamp npm config */
    switch(attr->id)
    {
        case SAI_TWAMP_SESSION_ATTR_PORT:
            p_twamp_info->session_attr.port_id = attr->value.oid;
            break;

        case SAI_TWAMP_SESSION_ATTR_SESSION_ROLE:
            p_twamp_info->session_attr.role = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_UDP_SRC_PORT:
            p_twamp_info->session_attr.udp_src_port = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_UDP_DST_PORT:
            p_twamp_info->session_attr.udp_dst_port = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_SRC_IP:
            sal_memcpy(&p_twamp_info->session_attr.src_ip, &attr->value.ipaddr, sizeof(sai_ip_address_t));
            break;

        case SAI_TWAMP_SESSION_ATTR_DST_IP:
            sal_memcpy(&p_twamp_info->session_attr.dst_ip, &attr->value.ipaddr, sizeof(sai_ip_address_t));
            break;

        case SAI_TWAMP_SESSION_ATTR_TC:
            p_twamp_info->session_attr.priority = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_VPN_VIRTUAL_ROUTE:
            p_twamp_info->session_attr.vrf_id = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_TWAMP_ENCAPSULATION_TYPE:
            p_twamp_info->session_attr.encap_type = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_SESSION_ENABLE_TRANSMIT:
            p_twamp_info->session_attr.trans_enable = attr->value.booldata;
            break;

        case SAI_TWAMP_SESSION_ATTR_HW_LOOKUP_VALID:
            p_twamp_info->session_attr.hw_lookup = attr->value.booldata;
            break;

        case SAI_TWAMP_SESSION_ATTR_PADDING_LENGTH:
            p_twamp_info->session_attr.padding_length = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_STATE:
            p_twamp_info->session_attr.session_state = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_AUTH_MODE:
            p_twamp_info->session_attr.auth_mode = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_NEXT_HOP_ID:
            p_twamp_info->session_attr.nexthop_id = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_TX_PKT_PERIOD:
            p_twamp_info->session_attr.period = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_TX_RATE:
            p_twamp_info->session_attr.tx_rate = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_TX_PKT_CNT:
            p_twamp_info->session_attr.pkt_cnt = attr->value.u32;
            break;

        case SAI_TWAMP_SESSION_ATTR_TX_PKT_DURATION:
            p_twamp_info->session_attr.pkt_duration = attr->value.u64;
            break;

        case SAI_TWAMP_SESSION_ATTR_MODE:
            p_twamp_info->session_attr.session_mode = attr->value.u32;
            break;

        default:
            CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "Set twamp attribute not implement\n");
            return  SAI_STATUS_NOT_IMPLEMENTED;
    }

    // clear the sdk config, disable the tranmit 
    if (SAI_TWAMP_SESSION_ATTR_SESSION_ENABLE_TRANSMIT == attr->id)
    {
        status = ctc_sai_oid_get_twamp_session_id(key->key.object_id, &session_id);
        session_tmp_id = session_id;
        if (TRUE == attr->value.booldata)
        {
            twmap_enable = 1;
            CTC_SAI_CTC_ERROR_RETURN(ctc_sai_twamp_global_ipv4_acl_enable(session_id));
        }
        else
        {
            twmap_enable = 0;
            CTC_SAI_CTC_ERROR_RETURN(ctc_sai_twamp_global_ipv4_acl_disable(session_id));
        }

        CTC_SAI_CTC_ERROR_RETURN(ctcs_npm_set_transmit_en(lchip,  session_tmp_id, twmap_enable));
    }

    return SAI_STATUS_SUCCESS;
}

static ctc_sai_attr_fn_entry_t  twamp_attr_fn_entries[] =
{
    { SAI_TWAMP_SESSION_ATTR_PORT,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_SESSION_ROLE,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr 
    },
    { SAI_TWAMP_SESSION_ATTR_UDP_SRC_PORT,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_UDP_DST_PORT,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_SRC_IP,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_DST_IP,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_TC,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_VPN_VIRTUAL_ROUTE,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_TWAMP_ENCAPSULATION_TYPE,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_SESSION_ENABLE_TRANSMIT,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_HW_LOOKUP_VALID,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_PADDING_LENGTH,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_STATE,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_AUTH_MODE,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_NEXT_HOP_ID,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_TX_PKT_PERIOD,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_TX_RATE,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_TX_PKT_CNT,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_TX_PKT_DURATION,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    },
    { SAI_TWAMP_SESSION_ATTR_MODE,
      ctc_sai_twamp_get_twamp_session_attr,
      ctc_sai_twamp_set_twamp_session_attr
    }
};

sai_status_t
ctc_sai_twamp_set_twamp_attr(sai_object_id_t twamp_session_id, const sai_attribute_t *attr)
{
    sai_object_key_t key = { .key.object_id = twamp_session_id };
    uint8           lchip = 0;
    char            key_str[MAX_KEY_STR_LEN];
    sai_status_t    status = SAI_STATUS_SUCCESS;

    CTC_SAI_LOG_ENTER(SAI_API_TWAMP);

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_id, &lchip));
    status = ctc_sai_set_attribute(&key, key_str, SAI_OBJECT_TYPE_TWAMP, twamp_attr_fn_entries, attr);
    if (SAI_STATUS_SUCCESS != status)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "Failed to set stp attr:%d, status:%d\n", attr->id,status);
    }

    return status;
}

sai_status_t
ctc_sai_twamp_get_twamp_attr(sai_object_id_t twamp_session_id, uint32_t attr_count, sai_attribute_t *attr_list)
{
    sai_object_key_t key = { .key.object_id = twamp_session_id};
    sai_status_t    status = SAI_STATUS_SUCCESS;
    char            key_str[MAX_KEY_STR_LEN];
    uint32_t        loop_index = 0;
    uint8           lchip = 0;

    CTC_SAI_LOG_ENTER(SAI_API_TWAMP);

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_id, &lchip));

    while (loop_index < attr_count)
    {
        CTC_SAI_ERROR_GOTO(ctc_sai_get_attribute(&key, key_str, SAI_OBJECT_TYPE_TWAMP,
                                    loop_index, twamp_attr_fn_entries, &attr_list[loop_index]), status, out);
        loop_index ++;
    }

out:
    if (SAI_STATUS_SUCCESS != status)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "Failed to get twamp attr. status:%d, attr_id:%d\n", status, attr_list[loop_index].id);
    }

    return status;
}

int32
ctc_sai_twamp_time_delay_transfer(uint64 time, uint32* time_s, uint32* time_ms, uint32* time_us, uint32* time_ns)
{
    *time_s = time / 1000000000;
    *time_ms = (time - ((*time_s) * 1000000000)) / 1000000;
    *time_us = (time - ((*time_s) * 1000000000) - ((*time_ms) * 1000000)) / 1000;
    *time_ns = time - (*time_s) * 1000000000 - (*time_ms) * 1000000 - (*time_us) * 1000;

    return 0;
}

sai_status_t
ctc_sai_twmap_get_twamp_session_stats(sai_object_id_t twamp_session_id, uint32_t stats_count, sai_attribute_t *stats_list)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    int32   ret = 0;
    uint8   lchip = 0;
    uint8   session_id = 0;
    uint32  tmp_session_id = 0;
    uint32  time_s = 0;
    uint32  time_ms = 0;
    uint32  time_us = 0;
    uint32  time_ns = 0;
    uint64  duration_ts = 0;
    uint64  fl = 0;
    uint8   stats_index = 0;
    double  fl_ratio = 0;
    char    first_ts[64] = {0};
    char    last_ts[64] = {0};
    sal_time_t tv;
    sal_time_t tv1;
    ctc_npm_stats_t npm_stats;
    ctc_sai_twamp_t     *p_twamp_info = NULL;

    CTC_SAI_LOG_ENTER(SAI_API_TWAMP);

    CTC_SAI_PTR_VALID_CHECK(stats_list);

    sal_memset(&npm_stats, 0, sizeof(ctc_npm_stats_t));
    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_id, &lchip));
    p_twamp_info = ctc_sai_db_get_object_property(lchip, twamp_session_id);
    if (NULL == p_twamp_info)
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    status = ctc_sai_oid_get_twamp_session_id(twamp_session_id, &tmp_session_id);
    session_id = tmp_session_id;
    ret = ctcs_npm_get_stats(lchip, session_id, &npm_stats);

    ctc_sai_twamp_time_delay_transfer(npm_stats.total_delay, &time_s, &time_ms, &time_us, &time_ns);

    /*duration*/
    tv = npm_stats.first_ts / 1000000000;
    tv1 = npm_stats.last_ts / 1000000000;
    sal_strncpy(first_ts, sal_ctime(&tv), sal_strlen(sal_ctime(&tv))-1);
    sal_strncpy(last_ts, sal_ctime(&tv1), sal_strlen(sal_ctime(&tv1))-1);

    duration_ts = (npm_stats.last_ts >= npm_stats.first_ts) ? (npm_stats.last_ts - npm_stats.first_ts) : 0;
    ctc_sai_twamp_time_delay_transfer(duration_ts, &time_s, &time_ms, &time_us, &time_ns);

    /*FL*/
    if (0 == npm_stats.tx_en)
    {
        if (npm_stats.tx_pkts >= npm_stats.rx_pkts)
        {
            fl = npm_stats.tx_pkts - npm_stats.rx_pkts;
            fl_ratio = fl ? ((double)fl / npm_stats.tx_pkts) : 0;
        }
    }

    // save into sai twamp db
    p_twamp_info->session_stats.max_delay = npm_stats.max_delay;
    p_twamp_info->session_stats.min_delay = npm_stats.min_delay;
    p_twamp_info->session_stats.max_jitter = npm_stats.max_jitter;
    p_twamp_info->session_stats.min_jitter = npm_stats.min_jitter;

    for (stats_index = 0; stats_index < stats_count; stats_index ++)
    {
        stats_list[stats_index].id = SAI_TWAMP_SESSION_STATS_RX_PACKETS;
        stats_list[stats_index].value.u64 = npm_stats.rx_pkts;

        switch(stats_index)
        {
            case SAI_TWAMP_SESSION_STATS_RX_PACKETS:
                stats_list[stats_index].value.u64 = npm_stats.rx_pkts;
                break;

            case SAI_TWAMP_SESSION_STATS_RX_BYTE:
                stats_list[stats_index].value.u64 = npm_stats.rx_bytes;
                break;

            case SAI_TWAMP_SESSION_STATS_TX_PACKETS:
                stats_list[stats_index].value.u64 = npm_stats.tx_pkts;
                break;

            case SAI_TWAMP_SESSION_STATS_TX_BYTE:
                stats_list[stats_index].value.u64 = npm_stats.tx_bytes;
                break;

            case SAI_TWAMP_SESSION_STATS_DROP_PACKETS:
                stats_list[stats_index].value.u64 = fl;
                break;

            case SAI_TWAMP_SESSION_STATS_MAX_LATENCY:
                stats_list[stats_index].value.u64 = npm_stats.max_delay;
                break;

            case SAI_TWAMP_SESSION_STATS_MIN_LATENCY:
                stats_list[stats_index].value.u64 = npm_stats.min_delay;
                break;

            case SAI_TWAMP_SESSION_STATS_AVG_LATENCY:
                stats_list[stats_index].value.u64 = npm_stats.total_delay / npm_stats.tx_pkts;
                break;

            case SAI_TWAMP_SESSION_STATS_MAX_JITTER:
                stats_list[stats_index].value.u64 = npm_stats.max_jitter;
                break;

            case SAI_TWAMP_SESSION_STATS_MIN_JITTER:
                stats_list[stats_index].value.u64 = npm_stats.min_jitter;

                break;
            case SAI_TWAMP_SESSION_STATS_AVG_JITTER:
                stats_list[stats_index].value.u64 = npm_stats.total_jitter / npm_stats.tx_pkts;
                break;

            case SAI_TWAMP_SESSION_STATS_FIRST_TS:
                stats_list[stats_index].value.u64 = npm_stats.first_ts;
                break;

            case SAI_TWAMP_SESSION_STATS_LAST_TS:
                stats_list[stats_index].value.u64 = npm_stats.last_ts;
                break;

            case SAI_TWAMP_SESSION_STATS_DURATION_TS:
                stats_list[stats_index].value.u64 = duration_ts;
                break;

            default:
                CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "Get twamp stats not implement\n");
                return  SAI_STATUS_NOT_IMPLEMENTED;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
ctc_sai_twmap_clear_twamp_session_stats( sai_object_id_t twamp_session_id, uint32_t stats_count, const sai_attribute_t *stats_list)
{
    sai_status_t    status = SAI_STATUS_SUCCESS;
    int32   ret = 0;
    uint8   lchip = 0;
    uint8   session_id = 0;
    ctc_sai_twamp_t     *p_twamp_info = NULL;

    CTC_SAI_LOG_ENTER(SAI_API_TWAMP);

    CTC_SAI_ERROR_RETURN(ctc_sai_oid_get_lchip(twamp_session_id, &lchip));
    p_twamp_info = ctc_sai_db_get_object_property(lchip, twamp_session_id);
    if (NULL == p_twamp_info)
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    ret = ctcs_npm_clear_stats(lchip, session_id);
    if (ret)
    {
        CTC_SAI_LOG_ERROR(SAI_API_TWAMP, "%% %s \n", ctc_get_error_desc(ret));
        status = ctc_sai_mapping_error_ctc(ret);
        goto error2;
    }

    sal_memset(&p_twamp_info->session_stats, 0, sizeof(sai_twamp_common_stats_t));

    return status;

error2:
    return SAI_STATUS_SUCCESS;
}


const sai_twamp_api_t ctc_sai_twamp_api = {
    ctc_sai_twamp_create_twamp_session,
    ctc_sai_twamp_remove_twamp_session,
    ctc_sai_twamp_set_twamp_attr,
    ctc_sai_twamp_get_twamp_attr,
    ctc_sai_twmap_get_twamp_session_stats,
    ctc_sai_twmap_clear_twamp_session_stats,
};

sai_status_t
ctc_sai_twamp_api_init()
{
    ctc_sai_register_module_api(SAI_API_TWAMP, (void*)&ctc_sai_twamp_api);

    return SAI_STATUS_SUCCESS;
}

sai_status_t
ctc_sai_twamp_db_init(uint8 lchip)
{
    ctc_sai_db_wb_t wb_info;

    sal_memset(&wb_info, 0, sizeof(wb_info));
    wb_info.version = SYS_WB_VERSION_TWAMP;
    wb_info.data_len = sizeof(ctc_sai_twamp_t);
    wb_info.wb_sync_cb = NULL;
    wb_info.wb_reload_cb = NULL;
    ctc_sai_warmboot_register_cb(lchip, CTC_SAI_WB_TYPE_OID, SAI_OBJECT_TYPE_TWAMP, (void*)(&wb_info));

    CTC_SAI_WARMBOOT_STATUS_CHECK(lchip);

    ctc_sai_twamp_global_acl_init();
    ctc_sai_twamp_init_session_max();
    return SAI_STATUS_SUCCESS;
}

