/**
 @file sys_goldengate_oam_bfd.c

 @date 2011-12-11

 @version v2.0

  This file contains oam sys layer function implementation
*/

/****************************************************************************
 *
* Header Files
*
****************************************************************************/
#include "ctc_oam.h"
#include "ctc_error.h"
#include "ctc_common.h"

#include "sys_goldengate_chip.h"
#include "sys_goldengate_oam_com.h"
#include "sys_goldengate_oam_bfd.h"
#include "sys_goldengate_oam_bfd_db.h"
#include "sys_goldengate_oam_debug.h"


#include "goldengate/include/drv_lib.h"
/* #include "goldengate/include/drv_enum.h" --never--*/
#include "ctc_packet.h"
#include "sys_goldengate_packet.h"
/****************************************************************************
*
* Defines and Macros
*
*****************************************************************************/

extern sys_oam_master_t* g_gg_oam_master[CTC_MAX_LOCAL_CHIP_NUM];

sal_timer_t* sys_goldengate_bfd_pf_timer = NULL;

/****************************************************************************
*
* Function
*
***************************************************************************/

STATIC int32
_sys_goldengate_bfd_key_check_param(uint8 lchip, ctc_oam_key_t* key)
{

    if ((CTC_OAM_MEP_TYPE_IP_BFD == key->mep_type) || (CTC_OAM_MEP_TYPE_MPLS_BFD == key->mep_type))
    {
        SYS_OAM_DBG_PARAM("BFD KEY: discr->0x%x, flag->0x%x!!\n", key->u.bfd.discr, key->flag);
    }
    else if (CTC_OAM_MEP_TYPE_MPLS_TP_BFD == key->mep_type)
    {
        SYS_OAM_DBG_PARAM("BFD KEY: label->0x%x, gport_l3ifid->%d, flag->0x%x!!\n",
                          key->u.tp.label, key->u.tp.gport_or_l3if_id, key->flag);

        if (CTC_FLAG_ISSET(key->flag, CTC_OAM_KEY_FLAG_LINK_SECTION_OAM))
        {
            if (!P_COMMON_OAM_MASTER_GLB(lchip).tp_section_oam_based_l3if)
            {
                CTC_GLOBAL_PORT_CHECK(key->u.tp.gport_or_l3if_id);

                if (CTC_IS_LINKAGG_PORT(key->u.tp.gport_or_l3if_id))
                {
                    return CTC_E_INVALID_GLOBAL_PORT;
                }

            }
            else
            {
                if ((key->u.tp.gport_or_l3if_id < SYS_GG_MIN_CTC_L3IF_ID)
                    || (key->u.tp.gport_or_l3if_id > SYS_GG_MAX_CTC_L3IF_ID))
                {
                    return CTC_E_L3IF_INVALID_IF_ID;
                }
            }
        }

    }

    return CTC_E_NONE;

}


STATIC int32
_sys_goldengate_tp_bfd_mepid_no_check_param(uint8 lchip, void* p_mep_param, bool is_add)
{
    return CTC_E_NONE;
}



STATIC int32
_sys_goldengate_tp_bfd_mepid_check_param(uint8 lchip, void* p_mep_param, bool is_add, bool is_local)
{
    ctc_oam_bfd_lmep_t* p_bfd_lmep = NULL;
    ctc_oam_bfd_rmep_t* p_bfd_rmep = NULL;

    SYS_OAM_DBG_FUNC();

    CTC_PTR_VALID_CHECK(p_mep_param);

    if(is_local)
    {
        p_bfd_lmep = &(((ctc_oam_lmep_t*)p_mep_param)->u.bfd_lmep);
    }
    else
    {
        p_bfd_rmep = &(((ctc_oam_rmep_t*)p_mep_param)->u.bfd_rmep);
    }

    if (is_add)
    {
        /*check mep id tlv*/
        if (is_local && CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_TP_WITH_MEP_ID))
        {
            SYS_OAM_DBG_PARAM("Lmep: MEP ID TLV->'%s', len: 0x%x !!\n",
                      p_bfd_lmep->mep_id, p_bfd_lmep->mep_id_len);
            if ((p_bfd_lmep->mep_id_len == 0) || (p_bfd_lmep->mep_id_len > CTC_OAM_BFD_CV_MEP_ID_MAX_LEN))
            {
                return CTC_E_OAM_MAID_LENGTH_INVALID;
            }
        }
        else if(!is_local && CTC_FLAG_ISSET(p_bfd_rmep->flag, CTC_OAM_BFD_RMEP_FLAG_TP_WITH_MEP_ID))
        {
            SYS_OAM_DBG_PARAM("Lmep: RMEP ID TLV->'%s', len: 0x%x !!\n",
            p_bfd_rmep->mep_id, p_bfd_rmep->mep_id_len);
            if ((p_bfd_rmep->mep_id_len == 0) || (p_bfd_rmep->mep_id_len > CTC_OAM_BFD_CV_MEP_ID_MAX_LEN))
            {
                return CTC_E_OAM_MAID_LENGTH_INVALID;
            }
        }
    }

    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_bfd_lmep_check_param(uint8 lchip, ctc_oam_lmep_t* p_lmep_param, bool is_add)
{
    ctc_oam_bfd_lmep_t* p_bfd_lmep = NULL;
    uint32 max_mep_index = 0;

    SYS_OAM_DBG_FUNC();

    CTC_PTR_VALID_CHECK(p_lmep_param);

    p_bfd_lmep = &(p_lmep_param->u.bfd_lmep);

    CTC_ERROR_RETURN(_sys_goldengate_bfd_key_check_param(lchip, &p_lmep_param->key));

    if (is_add)
    {
        SYS_OAM_DBG_PARAM("Lmep: My discr->%d, min tx interval->%d, detect mult->%d, diag->%d, state->%d, master gchip->%d!!\n",
                          p_bfd_lmep->local_discr,
                          p_bfd_lmep->desired_min_tx_interval, p_bfd_lmep->local_detect_mult,
                          p_bfd_lmep->local_diag, p_bfd_lmep->local_state, p_bfd_lmep->master_gchip);

        SYS_GLOBAL_CHIPID_CHECK(p_bfd_lmep->master_gchip);

        if (CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_MEP_ON_CPU))
        {
            return CTC_E_NONE;
        }

        CTC_ERROR_RETURN(_sys_goldengate_oam_check_nexthop_type(lchip, p_bfd_lmep->nhid, CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_PROTECTION_PATH), p_lmep_param->key.mep_type));

        if ((CTC_OAM_MEP_TYPE_IP_BFD == p_lmep_param->key.mep_type)
            || (CTC_OAM_MEP_TYPE_MPLS_BFD == p_lmep_param->key.mep_type))
        {
            /* check udp port*/
            if ((!(CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_MPLS_PW_VCCV)
                    ||CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_MPLS_PW_VCCV_IPV4)
                    ||CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_MPLS_PW_VCCV_IPV6)))
                &&(p_bfd_lmep->bfd_src_port < SYS_OAM_MIN_BFD_UDP_PORT))
            {
                return CTC_E_OAM_BFD_INVALID_UDP_PORT;
            }

            /* check actual & min tx interval */
            if ((p_bfd_lmep->desired_min_tx_interval < SYS_OAM_MIN_BFD_INTERVAL
                || p_bfd_lmep->desired_min_tx_interval > SYS_OAM_MAX_BFD_INTERVAL))
            {
                return CTC_E_OAM_BFD_INVALID_INTERVAL;
            }
        }
        else if (CTC_OAM_MEP_TYPE_MPLS_TP_BFD == p_lmep_param->key.mep_type)
        {
            /* check actual & min tx interval */
            if ((p_bfd_lmep->desired_min_tx_interval < SYS_OAM_MIN_BFD_INTERVAL
                || p_bfd_lmep->desired_min_tx_interval > SYS_OAM_MAX_BFD_INTERVAL))
            {
                return CTC_E_OAM_BFD_INVALID_INTERVAL;
            }
        }

        /* check local_detect_mult */
        if (p_bfd_lmep->local_detect_mult < SYS_OAM_MIN_BFD_DETECT_MULT
            || p_bfd_lmep->local_detect_mult > SYS_OAM_MAX_BFD_DETECT_MULT)
        {
            return CTC_E_OAM_BFD_INVALID_DETECT_MULT;
        }

        /* check diag*/
        if (p_bfd_lmep->local_diag > CTC_OAM_BFD_DIAG_MAX)
        {
            return CTC_E_OAM_BFD_INVALID_DIAG;
        }

        /* check state*/
        if (p_bfd_lmep->local_state > CTC_OAM_BFD_STATE_UP)
        {
            return CTC_E_OAM_BFD_INVALID_STATE;
        }

        /* check lm value*/
        if (CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_LM_EN))
        {
            if(p_bfd_lmep->lm_cos_type >= CTC_OAM_LM_COS_TYPE_MAX)
            {
                return CTC_E_OAM_INVALID_LM_COS_TYPE;
            }

            CTC_COS_RANGE_CHECK(p_bfd_lmep->lm_cos);
        }

        /* check csf value*/
        if (CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_TX_CSF_EN))
        {
            if(p_bfd_lmep->tx_csf_type >= CTC_OAM_CSF_TYPE_MAX)
            {
                return CTC_E_OAM_INVALID_CSF_TYPE;
            }
        }

        /* check mep index*/
        if (!P_COMMON_OAM_MASTER_GLB(lchip).mep_index_alloc_by_sdk)
        {
            max_mep_index = _sys_goldengate_oam_get_mep_entry_num(lchip);
            if (max_mep_index&&((p_lmep_param->lmep_index < SYS_OAM_MIN_MEP_INDEX)
                || (p_lmep_param->lmep_index >= max_mep_index)))
            {
                return CTC_E_OAM_INVALID_MEP_INDEX;
            }
        }
    }

    return CTC_E_NONE;
}


STATIC int32
_sys_goldengate_bfd_rmep_check_param(uint8 lchip, ctc_oam_rmep_t* p_rmep_param, bool is_add)
{
    ctc_oam_bfd_rmep_t* p_bfd_rmep = NULL;
    uint32 max_mep_index = 0;

    SYS_OAM_DBG_FUNC();

    CTC_PTR_VALID_CHECK(p_rmep_param);

    p_bfd_rmep = &(p_rmep_param->u.bfd_rmep);

    CTC_ERROR_RETURN(_sys_goldengate_bfd_key_check_param(lchip, &p_rmep_param->key));

    if (is_add)
    {
        SYS_OAM_DBG_PARAM("Rmep: Your discr->%d !!\n", p_bfd_rmep->remote_discr);

        if ((CTC_OAM_MEP_TYPE_IP_BFD == p_rmep_param->key.mep_type)
            || (CTC_OAM_MEP_TYPE_MPLS_BFD == p_rmep_param->key.mep_type))
        {
            /* check actual & min rx interval */
            if ((p_bfd_rmep->required_min_rx_interval < SYS_OAM_MIN_BFD_INTERVAL
                || p_bfd_rmep->required_min_rx_interval > SYS_OAM_MAX_BFD_INTERVAL))
            {
                return CTC_E_OAM_BFD_INVALID_INTERVAL;
            }
        }
        else if (CTC_OAM_MEP_TYPE_MPLS_TP_BFD == p_rmep_param->key.mep_type)
        {
            /* check actual & min tx interval */
            if ((p_bfd_rmep->required_min_rx_interval < SYS_OAM_MIN_BFD_INTERVAL
                || p_bfd_rmep->required_min_rx_interval > SYS_OAM_MAX_BFD_INTERVAL))
            {
                return CTC_E_OAM_BFD_INVALID_INTERVAL;
            }
        }

        /* check remote_detect_mult */
        if (p_bfd_rmep->remote_detect_mult < SYS_OAM_MIN_BFD_DETECT_MULT
            || p_bfd_rmep->remote_detect_mult > SYS_OAM_MAX_BFD_DETECT_MULT)
        {
            return CTC_E_OAM_BFD_INVALID_DETECT_MULT;
        }

        /* check diag*/
        if (p_bfd_rmep->remote_diag > CTC_OAM_BFD_DIAG_MAX)
        {
            return CTC_E_OAM_BFD_INVALID_DIAG;
        }

        /* check state*/
        if (p_bfd_rmep->remote_state > CTC_OAM_BFD_STATE_UP)
        {
            return CTC_E_OAM_BFD_INVALID_STATE;
        }

        /* check mep index*/
        if (!P_COMMON_OAM_MASTER_GLB(lchip).mep_index_alloc_by_sdk)
        {
            max_mep_index = _sys_goldengate_oam_get_mep_entry_num(lchip);
            if ((p_rmep_param->rmep_index < SYS_OAM_MIN_MEP_INDEX)
                || (p_rmep_param->rmep_index >= max_mep_index))
            {
                return CTC_E_OAM_INVALID_MEP_INDEX;
            }
        }
    }

    return CTC_E_NONE;
}

STATIC int32
_sys_goldengate_bfd_check_update_param(uint8 lchip, ctc_oam_update_t* p_update_param, bool is_local)
{
    uint8 mep_type = CTC_OAM_MEP_TYPE_MAX;
    int32 ret      = CTC_E_NONE;
    ctc_oam_bfd_timer_t *p_timer = NULL;
    ctc_oam_bfd_timer_t *p_tx_rx_timer = NULL;
    CTC_PTR_VALID_CHECK(p_update_param);

    /* check is local or remote*/
    if (p_update_param->is_local != is_local)
    {
        return CTC_E_INVALID_PARAM;
    }

    CTC_ERROR_RETURN(_sys_goldengate_bfd_key_check_param(lchip, &p_update_param->key));

    mep_type = p_update_param->key.mep_type;

    if (is_local)
    {
        if ((CTC_OAM_MEP_TYPE_IP_BFD == mep_type)
            || (CTC_OAM_MEP_TYPE_MPLS_BFD == mep_type))
        {
            switch(p_update_param->update_type)
            {
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_CV_EN:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_CSF_EN:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_TX_CSF_EN:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_TX_CSF_TYPE:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_DCSF:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_EN:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_COS_TYPE:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_COS:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_DISCR:
                    ret = CTC_E_OAM_INVALID_CFG_IN_MEP_TYPE;
                    break;
                default:
                    break;
            }
        }
        else if (CTC_OAM_MEP_TYPE_MPLS_TP_BFD == mep_type)
        {
            switch(p_update_param->update_type)
            {
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_CV_EN:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_CSF_EN:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_TX_CSF_EN:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_EN:
                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_DCSF:
                    if(p_update_param->update_value > 1)
                    {
                        ret = CTC_E_INVALID_PARAM;
                    }
                    break;

                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_TX_CSF_TYPE:
                    if(p_update_param->update_value >= CTC_OAM_CSF_TYPE_MAX)
                    {
                        ret = CTC_E_OAM_INVALID_CSF_TYPE;
                    }
                    break;

                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_COS_TYPE:
                    if(p_update_param->update_value >= CTC_OAM_LM_COS_TYPE_MAX)
                    {
                        ret = CTC_E_OAM_INVALID_LM_COS_TYPE;
                    }
                    break;

                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_COS:
                    CTC_COS_RANGE_CHECK(p_update_param->update_value);
                    break;

                case CTC_OAM_BFD_LMEP_UPDATE_TYPE_DISCR:

                    break;
                default:
                    break;
            }
        }

        switch(p_update_param->update_type)
        {
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_MEP_EN:
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_CC_EN:
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_LMEP_SF_STATE:
                if(p_update_param->update_value > 1)
                {
                    ret = CTC_E_INVALID_PARAM;
                }
                break;

            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_TX_COS_EXP:
                CTC_COS_RANGE_CHECK(p_update_param->update_value);
                break;

            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_TX_RX_TIMER:
                p_tx_rx_timer = (ctc_oam_bfd_timer_t*)p_update_param->p_update_value;
                if ((p_tx_rx_timer[1].detect_mult < SYS_OAM_MIN_BFD_DETECT_MULT)
                    || (p_tx_rx_timer[1].detect_mult > SYS_OAM_MAX_BFD_DETECT_MULT))
                {
                    ret = CTC_E_OAM_BFD_INVALID_DETECT_MULT;
                }
                if ((p_tx_rx_timer[1].min_interval < SYS_OAM_MIN_BFD_INTERVAL)
                    || (p_tx_rx_timer[1].min_interval > SYS_OAM_MAX_BFD_INTERVAL))
                {
                    ret = CTC_E_OAM_BFD_INVALID_INTERVAL;
                }
				/*check tx timer start, cannot break*/
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_TX_TIMER:
                p_timer = (ctc_oam_bfd_timer_t*)p_update_param->p_update_value;
                if (p_timer->detect_mult < SYS_OAM_MIN_BFD_DETECT_MULT
                    || p_timer->detect_mult > SYS_OAM_MAX_BFD_DETECT_MULT)
                {
                    ret = CTC_E_OAM_BFD_INVALID_DETECT_MULT;
                }

                if((p_timer->min_interval < SYS_OAM_MIN_BFD_INTERVAL
                    || p_timer->min_interval > SYS_OAM_MAX_BFD_INTERVAL))
                {
                    ret = CTC_E_OAM_BFD_INVALID_INTERVAL;
                }

                break;
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_ACTUAL_TX_TIMER:
                p_timer = (ctc_oam_bfd_timer_t*)p_update_param->p_update_value;

                if ((p_timer->min_interval < SYS_OAM_MIN_BFD_INTERVAL
                    || p_timer->min_interval > SYS_OAM_MAX_BFD_INTERVAL))
                {
                    ret = CTC_E_OAM_BFD_INVALID_INTERVAL;
                }

                break;
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_DIAG:
                if (p_update_param->update_value > CTC_OAM_BFD_DIAG_MAX)
                {
                    ret = CTC_E_OAM_BFD_INVALID_DIAG;
                }
                break;
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_STATE:
                if (p_update_param->update_value > CTC_OAM_BFD_STATE_UP)
                {
                    ret = CTC_E_OAM_BFD_INVALID_STATE;
                }
                break;
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_NHOP:
                ret = _sys_goldengate_oam_check_nexthop_type(lchip, p_update_param->update_value, 0, mep_type);
                break;
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_MASTER_GCHIP:
                SYS_GLOBAL_CHIPID_CHECK(p_update_param->update_value);
                break;
            case CTC_OAM_BFD_LMEP_UPDATE_TYPE_TTL:
                if ((p_update_param->update_value > 255) || (0 == p_update_param->update_value))
                {
                    SYS_OAM_DBG_OUT(CTC_DEBUG_LEVEL_ERROR, " BFD ttl is invalid \n");
                    ret = CTC_E_INVALID_PARAM;
                }
                break;
            default:
                break;
        }
    }
    else
    {
        switch(p_update_param->update_type)
        {
            case CTC_OAM_BFD_RMEP_UPDATE_TYPE_MEP_EN:
            case CTC_OAM_BFD_RMEP_UPDATE_TYPE_RMEP_SF_STATE:
                if(p_update_param->update_value > 1)
                {
                    ret = CTC_E_INVALID_PARAM;
                }
                break;

            case CTC_OAM_BFD_RMEP_UPDATE_TYPE_RX_TIMER:
                p_timer = (ctc_oam_bfd_timer_t*)p_update_param->p_update_value;
                if((p_timer->min_interval < SYS_OAM_MIN_BFD_INTERVAL
                    || p_timer->min_interval > SYS_OAM_MAX_BFD_INTERVAL))
                {
                    ret = CTC_E_OAM_BFD_INVALID_INTERVAL;
                }
                break;
            case CTC_OAM_BFD_RMEP_UPDATE_TYPE_ACTUAL_RX_TIMER:
                p_timer = (ctc_oam_bfd_timer_t*)p_update_param->p_update_value;
                if((p_timer->min_interval < SYS_OAM_MIN_BFD_INTERVAL
                    || p_timer->min_interval > SYS_OAM_MAX_BFD_INTERVAL))
                {
                    ret = CTC_E_OAM_BFD_INVALID_INTERVAL;
                }
                break;
            case CTC_OAM_BFD_RMEP_UPDATE_TYPE_DIAG:
                if (p_update_param->update_value > CTC_OAM_BFD_DIAG_MAX)
                {
                    ret = CTC_E_OAM_BFD_INVALID_DIAG;
                }
                break;
            case CTC_OAM_BFD_RMEP_UPDATE_TYPE_STATE:
                if (p_update_param->update_value > CTC_OAM_BFD_STATE_UP)
                {
                    ret = CTC_E_OAM_BFD_INVALID_STATE;
                }
                break;
            case CTC_OAM_BFD_RMEP_UPDATE_TYPE_DISCR:
                break;
            default:
                break;
        }
    }

    return ret;
}


STATIC int32
_sys_goldengate_bfd_add_lmep(uint8 lchip, void* p_lmep)
{
    ctc_oam_bfd_lmep_t* p_bfd_lmep  = NULL;
    ctc_oam_key_t*      p_bfd_key   = NULL;

    uint8 mep_type                  = CTC_OAM_MEP_TYPE_MAX;

    sys_oam_chan_com_t* p_sys_chan_com  = NULL;
    sys_oam_maid_com_t* p_sys_maid      = NULL;
    sys_oam_lmep_bfd_t* p_sys_lmep_bfd  = NULL;

    int32 ret       = CTC_E_NONE;
    uint32 session_type = 0;
    ctc_oam_maid_t mep_id;

    CTC_PTR_VALID_CHECK(p_lmep);

    p_bfd_lmep  = &(((ctc_oam_lmep_t *)p_lmep)->u.bfd_lmep);
    p_bfd_key   = &(((ctc_oam_lmep_t *)p_lmep)->key);
    mep_type    = p_bfd_key->mep_type;

    /*1. check lmep param */
    CTC_ERROR_RETURN(_sys_goldengate_bfd_lmep_check_param(lchip, p_lmep, TRUE));

    /* check source mep id tlv param */
    if((CTC_OAM_MEP_TYPE_MPLS_TP_BFD == mep_type)
        && CTC_FLAG_ISSET(p_bfd_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_TP_WITH_MEP_ID)
        &&(!g_gg_oam_master[lchip]->no_mep_resource))
    {
        sal_memset(&mep_id, 0 , sizeof(ctc_oam_maid_t));
        mep_id.mep_type = mep_type;
        mep_id.maid_len = p_bfd_lmep->mep_id_len;
        sal_memcpy(&mep_id.maid, &p_bfd_lmep->mep_id, p_bfd_lmep->mep_id_len);

        CTC_ERROR_RETURN(_sys_goldengate_tp_bfd_mepid_check_param(lchip, p_lmep,TRUE,TRUE));
        ret = _sys_goldengate_oam_com_add_maid(lchip, &mep_id);
        if ((CTC_E_NONE != ret)&&(CTC_E_OAM_MAID_ENTRY_EXIST != ret))
        {
            return ret;
        }
        p_sys_maid = _sys_goldengate_oam_maid_lkup(lchip, &mep_id);
        if (NULL == p_sys_maid)
        {
            return CTC_E_NOT_EXIST;
        }
        else
        {
            p_sys_maid->ref_cnt++;
        }
    }

    OAM_LOCK(lchip);
    /*2. check channel exist or add chan*/
    ret = _sys_goldengate_bfd_add_chan(lchip, p_lmep, (void**)&p_sys_chan_com);
    if (CTC_E_NONE != ret)
    {
        goto STEP_1;
    }

    /*5. lookup lmep and check exist*/
    p_sys_lmep_bfd = _sys_goldengate_bfd_lmep_lkup(lchip, p_sys_chan_com);
    if (NULL != p_sys_lmep_bfd)
    {
        ret = CTC_E_OAM_CHAN_LMEP_EXIST;
        goto STEP_4;
    }

    /*6. build lmep sys node*/
    p_sys_lmep_bfd = _sys_goldengate_bfd_lmep_build_node(lchip, p_lmep);
    if (NULL == p_sys_lmep_bfd)
    {
        SYS_OAM_DBG_ERROR("Lmep: build node fail!!\n");
        ret = CTC_E_NO_MEMORY;
        goto STEP_4;
    }

    p_sys_lmep_bfd->com.p_sys_maid = p_sys_maid;
    p_sys_lmep_bfd->com.p_sys_chan = p_sys_chan_com;

    /*7. build lmep index */
    ret = _sys_goldengate_bfd_lmep_build_index(lchip, p_sys_lmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: build index fail!!\n");
        goto STEP_5;
    }

    /*8. add lmep to db */
    ret = _sys_goldengate_bfd_lmep_add_to_db(lchip, p_sys_lmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: add db fail!!\n");
        goto STEP_6;
    }

    /*9. write lmep asic*/
    ret = _sys_goldengate_bfd_lmep_add_to_asic(lchip, p_sys_lmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: add asic fail!!\n");
        goto STEP_7;
    }

    /*10. update chan*/
    ret = _sys_goldengate_bfd_update_chan(lchip, p_sys_lmep_bfd, TRUE);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: update chan fail!!\n");
        goto STEP_8;
    }

    sys_goldengate_oam_get_session_type(lchip, (sys_oam_lmep_com_t*)p_sys_lmep_bfd, &session_type);
    SYS_OAM_SESSION_CNT(lchip, session_type, 1);
    /*11. copy mep index*/
    ((ctc_oam_lmep_t*)p_lmep)->lmep_index = p_sys_lmep_bfd->com.lmep_index;

    OAM_UNLOCK(lchip);
    return ret;

STEP_8:
    _sys_goldengate_bfd_lmep_del_from_asic(lchip, p_sys_lmep_bfd);
STEP_7:
    _sys_goldengate_bfd_lmep_del_from_db(lchip, p_sys_lmep_bfd);
STEP_6:
    _sys_goldengate_bfd_lmep_free_index(lchip, p_sys_lmep_bfd);
STEP_5:
    _sys_goldengate_bfd_lmep_free_node(lchip, p_sys_lmep_bfd);
STEP_4:
     _sys_goldengate_bfd_remove_chan(lchip, p_lmep);

STEP_1:
    OAM_UNLOCK(lchip);
    if (NULL != p_sys_maid)
    {
        p_sys_maid->ref_cnt--;
        sal_memset(&mep_id, 0, sizeof(ctc_oam_maid_t));
        mep_id.mep_type = p_bfd_key->mep_type;
        mep_id.maid_len = p_sys_maid->maid_len;
        sal_memcpy(&mep_id.maid, p_sys_maid->maid, p_sys_maid->maid_len);

        _sys_goldengate_oam_com_remove_maid(lchip, &mep_id);
    }
    return ret;
}


STATIC int32
_sys_goldengate_bfd_remove_lmep(uint8 lchip, void* p_lmep)
{
    ctc_oam_lmep_t* p_lmep_param            = NULL;
    sys_oam_chan_com_t* p_sys_chan_com      = NULL;
    sys_oam_lmep_bfd_t* p_sys_lmep_bfd      = NULL;
    sys_oam_maid_com_t* p_sys_maid          = NULL;
    int32            ret = CTC_E_NONE;
    uint32 session_type = 0;
    ctc_oam_maid_t mep_id;

    SYS_OAM_DBG_FUNC();
    p_lmep_param = (ctc_oam_lmep_t*)p_lmep;

    /*1. check param valid (lmep)*/
    CTC_ERROR_RETURN(_sys_goldengate_bfd_lmep_check_param(lchip, p_lmep_param, FALSE));

    OAM_LOCK(lchip);

    /*2. lookup chan and check exist*/
    p_sys_chan_com = _sys_goldengate_bfd_chan_lkup(lchip, &p_lmep_param->key);
    if (NULL == p_sys_chan_com)
    {
        OAM_UNLOCK(lchip);
        return CTC_E_OAM_CHAN_ENTRY_NOT_FOUND;
    }
#if 0
    /*check master is remote or not*/
    if (CTC_E_NONE != sys_goldengate_oam_check_mep_lchip(lchip, p_sys_chan_com->master_chipid, &lchip))
    {
        /*only update chan*/
        ret = _sys_goldengate_bfd_chan_update_reomte(lchip, p_lmep_param, (sys_oam_chan_tp_t*)p_sys_chan_com, FALSE);
        goto STEP_3;
    }
#endif
    /*3. lookup lmep and check exist*/
    p_sys_lmep_bfd = _sys_goldengate_bfd_lmep_lkup(lchip, p_sys_chan_com);
    if (NULL == p_sys_lmep_bfd)
    {
        OAM_UNLOCK(lchip);
        return CTC_E_OAM_CHAN_LMEP_NOT_FOUND;
    }

    /*4 check if rmep list */
    if (CTC_SLISTCOUNT(p_sys_lmep_bfd->com.rmep_list) > 0)
    {
        OAM_UNLOCK(lchip);
        return CTC_E_OAM_RMEP_EXIST;
    }

    sys_goldengate_oam_get_session_type(lchip, (sys_oam_lmep_com_t*)p_sys_lmep_bfd, &session_type);

    p_sys_maid = p_sys_lmep_bfd->com.p_sys_maid;

    /*5. update chan*/
    ret = _sys_goldengate_bfd_update_chan(lchip, p_sys_lmep_bfd, FALSE);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: update chan fail!!\n");
        goto RETURN;
    }

    /*6. remove lmep from asic*/
    ret = _sys_goldengate_bfd_lmep_del_from_asic(lchip, p_sys_lmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: del asic fail!!\n");
        goto RETURN;
    }

    /*7. remove lmep from db*/
    ret = _sys_goldengate_bfd_lmep_del_from_db(lchip, p_sys_lmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: del db fail!!\n");
        goto RETURN;
    }

    /*8. free lmep index*/
    ret = _sys_goldengate_bfd_lmep_free_index(lchip, p_sys_lmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: free index fail!!\n");
        goto RETURN;
    }

    /*9. free lmep node*/
    ret = _sys_goldengate_bfd_lmep_free_node(lchip, p_sys_lmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Lmep: free node fail!!\n");
        goto RETURN;
    }

    /*10. update chan exist or remove chan */
    ret = _sys_goldengate_bfd_remove_chan(lchip, p_lmep_param);
    if ((CTC_E_OAM_CHAN_LMEP_EXIST == ret) || (CTC_E_OAM_CHAN_ENTRY_NOT_FOUND == ret))
    {
        ret = CTC_E_NONE;
    }
    SYS_OAM_SESSION_CNT(lchip, session_type, 0);

RETURN:
    OAM_UNLOCK(lchip);
    if (NULL != p_sys_maid)
    {
        p_sys_maid->ref_cnt--;
        sal_memset(&mep_id, 0, sizeof(ctc_oam_maid_t));
        mep_id.mep_type = p_lmep_param->key.mep_type;
        mep_id.maid_len = p_sys_maid->maid_len;
        sal_memcpy(&mep_id.maid, p_sys_maid->maid, p_sys_maid->maid_len);
        _sys_goldengate_oam_com_remove_maid(lchip, &mep_id);
    }
    return ret;
}


STATIC int32
_sys_goldengate_bfd_update_lmep(uint8 lchip, void* p_lmep)
{
    ctc_oam_update_t* p_lmep_update = (ctc_oam_update_t*)p_lmep;

    sys_oam_chan_com_t*     p_sys_chan      = NULL;
    sys_oam_lmep_bfd_t*     p_sys_lmep_bfd  = NULL;
    int32   ret = CTC_E_NONE;

    SYS_OAM_DBG_FUNC();

    /*1. check param valid (lmep update)*/
    CTC_ERROR_RETURN(_sys_goldengate_bfd_check_update_param(lchip, p_lmep_update, TRUE));

    /*2. lookup chan and check exist*/
    p_sys_chan = _sys_goldengate_bfd_chan_lkup(lchip, &p_lmep_update->key);
    if (NULL == p_sys_chan)
    {
        return CTC_E_OAM_CHAN_ENTRY_NOT_FOUND;
    }

    /*3. lookup lmep and check exist*/
    p_sys_lmep_bfd = _sys_goldengate_bfd_lmep_lkup(lchip, p_sys_chan);
    if (NULL == p_sys_lmep_bfd)
    {
        return CTC_E_OAM_CHAN_LMEP_NOT_FOUND;
    }

    if (CTC_FLAG_ISSET(p_sys_lmep_bfd->flag, CTC_OAM_BFD_LMEP_FLAG_MEP_ON_CPU))
    {
        return CTC_E_OAM_INVALID_OPERATION_ON_CPU_MEP;
    }

    /*4. update lmep db and asic*/
    if (p_lmep_update->is_local)
    {
        OAM_LOCK(lchip);
        if ((CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_COS_TYPE == p_lmep_update->update_type)
                ||(CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_COS == p_lmep_update->update_type)
                ||(CTC_OAM_BFD_LMEP_UPDATE_TYPE_LM_EN == p_lmep_update->update_type)
                ||(CTC_OAM_BFD_LMEP_UPDATE_TYPE_LOCK == p_lmep_update->update_type))
        {
            ret = _sys_goldengate_bfd_update_lmep_lm(lchip, p_lmep_update, p_sys_lmep_bfd);
        }
        else
        {
            ret = _sys_goldengate_bfd_lmep_update_asic(lchip, p_lmep_update, p_sys_lmep_bfd);
        }
        OAM_UNLOCK(lchip);
    }
    else
    {
        ret = CTC_E_INVALID_PARAM;
    }

    return ret;
}

STATIC int32
_sys_goldengate_bfd_add_rmep(uint8 lchip, void* p_rmep)
{
    ctc_oam_rmep_t* p_rmep_param = (ctc_oam_rmep_t*)p_rmep;
    ctc_oam_bfd_rmep_t*     p_bfd_rmep     = NULL;
    sys_oam_maid_com_t*     p_sys_maid     = NULL;
    sys_oam_chan_com_t*     p_sys_chan_com = NULL;
    sys_oam_lmep_bfd_t*     p_sys_lmep_bfd = NULL;
    sys_oam_rmep_bfd_t*     p_sys_rmep_bfd = NULL;
    int32 ret               = CTC_E_NONE;
    ctc_oam_maid_t mep_id;

    SYS_OAM_DBG_FUNC();

    p_bfd_rmep  = &(p_rmep_param->u.bfd_rmep);

    /*1. check param valid (rmep)*/
    CTC_ERROR_RETURN(_sys_goldengate_bfd_rmep_check_param(lchip, p_rmep, TRUE));

    OAM_LOCK(lchip);

    /*2. lookup chan and check exist*/
    p_sys_chan_com = _sys_goldengate_bfd_chan_lkup(lchip, &p_rmep_param->key);
    if (NULL == p_sys_chan_com)
    {
        ret = CTC_E_OAM_CHAN_ENTRY_NOT_FOUND;
        goto RETURN;
    }

    /*3. lookup lmep and check exist*/
    p_sys_lmep_bfd = _sys_goldengate_bfd_lmep_lkup(lchip, p_sys_chan_com);
    if (NULL == p_sys_lmep_bfd)
    {
        ret = CTC_E_OAM_CHAN_LMEP_NOT_FOUND;
        goto RETURN;
    }

    if (CTC_FLAG_ISSET(p_sys_lmep_bfd->flag, CTC_OAM_BFD_LMEP_FLAG_MEP_ON_CPU))
    {
        ret = CTC_E_OAM_INVALID_OPERATION_ON_CPU_MEP;
        goto RETURN;
    }

    /*4. check source mep id tlv param */
    if((CTC_OAM_MEP_TYPE_MPLS_TP_BFD == p_rmep_param->key.mep_type)
        && CTC_FLAG_ISSET(p_bfd_rmep->flag, CTC_OAM_BFD_RMEP_FLAG_TP_WITH_MEP_ID))
    {
        sal_memset(&mep_id, 0 , sizeof(ctc_oam_maid_t));
        mep_id.mep_type = p_rmep_param->key.mep_type;
        mep_id.maid_len = p_bfd_rmep->mep_id_len;
        sal_memcpy(&mep_id.maid, &p_bfd_rmep->mep_id, p_bfd_rmep->mep_id_len);
        ret = _sys_goldengate_tp_bfd_mepid_check_param(lchip, p_rmep,TRUE,FALSE);
        if (CTC_E_NONE != ret)
        {
            goto RETURN;
        }
        OAM_UNLOCK(lchip);
        ret = _sys_goldengate_oam_com_add_maid(lchip, &mep_id);
        if ((CTC_E_NONE != ret)&&(CTC_E_OAM_MAID_ENTRY_EXIST != ret))
        {
            goto RETURN1;
        }
        p_sys_maid = _sys_goldengate_oam_maid_lkup(lchip, &mep_id);
        if (NULL == p_sys_maid)
        {
            ret = CTC_E_NOT_EXIST;
            goto RETURN1;
        }
        OAM_LOCK(lchip);
    }


    /*4. lookup rmep and check exist*/
    p_sys_rmep_bfd = _sys_goldengate_bfd_rmep_lkup(lchip, p_sys_lmep_bfd);
    if (NULL != p_sys_rmep_bfd)
    {
        ret = CTC_E_OAM_RMEP_EXIST;
        goto RETURN;
    }

    /*5. build rmep sys node*/
    p_sys_rmep_bfd = _sys_goldengate_bfd_rmep_build_node(lchip, p_rmep_param);
    if (NULL == p_sys_rmep_bfd)
    {
        SYS_OAM_DBG_ERROR("Rmep: build node fail!!\n");
        ret = CTC_E_NO_MEMORY;
        goto RETURN;
    }

    p_sys_rmep_bfd->com.p_sys_maid = p_sys_maid;
    p_sys_rmep_bfd->com.p_sys_lmep = &p_sys_lmep_bfd->com;

    /*6. build rmep index */
    ret = _sys_goldengate_bfd_rmep_build_index(lchip, p_sys_rmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Rmep: build index fail!!\n");
        goto STEP_5;
    }

    /*7. add rmep to db */
    ret = _sys_goldengate_bfd_rmep_add_to_db(lchip, p_sys_rmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Rmep: add db fail!!\n");
        goto STEP_6;
    }

    /*8. write rmep asic*/
    ret = _sys_goldengate_bfd_rmep_add_to_asic(lchip, p_sys_rmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Rmep: add asic fail!!\n");
        goto STEP_7;
    }

    /*9. copy mep index*/
    p_rmep_param->rmep_index = p_sys_rmep_bfd->com.rmep_index;

    goto RETURN;

STEP_7:
    _sys_goldengate_bfd_rmep_del_from_db(lchip, p_sys_rmep_bfd);
STEP_6:
    _sys_goldengate_bfd_rmep_free_index(lchip, p_sys_rmep_bfd);
STEP_5:
    _sys_goldengate_bfd_rmep_free_node(lchip, p_sys_rmep_bfd);

RETURN:
    OAM_UNLOCK(lchip);
RETURN1:
    return ret;
}

STATIC int32
_sys_goldengate_bfd_remove_rmep(uint8 lchip, void* p_rmep)
{
    ctc_oam_rmep_t* p_rmep_param = (ctc_oam_rmep_t*)p_rmep;
    sys_oam_chan_com_t* p_sys_chan_com = NULL;
    sys_oam_lmep_bfd_t* p_sys_lmep_bfd = NULL;
    sys_oam_rmep_bfd_t* p_sys_rmep_bfd = NULL;
    int32 ret           = CTC_E_NONE;

    SYS_OAM_DBG_FUNC();

    /*1. check param valid (rmep)*/
    CTC_ERROR_RETURN(_sys_goldengate_bfd_rmep_check_param(lchip, p_rmep, FALSE));

    OAM_LOCK(lchip);

    /*2. lookup chan and check exist*/
    p_sys_chan_com = _sys_goldengate_bfd_chan_lkup(lchip, &p_rmep_param->key);
    if (NULL == p_sys_chan_com)
    {
        ret = CTC_E_OAM_CHAN_ENTRY_NOT_FOUND;
        goto RETURN;
    }

    /*3. lookup lmep and check exist*/
    p_sys_lmep_bfd = _sys_goldengate_bfd_lmep_lkup(lchip, p_sys_chan_com);
    if (NULL == p_sys_lmep_bfd)
    {
        ret = CTC_E_OAM_CHAN_LMEP_NOT_FOUND;
        goto RETURN;
    }

    /*4. lookup rmep and check exist*/
    p_sys_rmep_bfd = _sys_goldengate_bfd_rmep_lkup(lchip, p_sys_lmep_bfd);
    if (NULL == p_sys_rmep_bfd)
    {
        ret = CTC_E_OAM_RMEP_NOT_FOUND;
        goto RETURN;
    }

    /*5. remove rmep from asic*/
    ret = _sys_goldengate_bfd_rmep_del_from_asic(lchip, p_sys_rmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Rmep: del asic fail!!\n");
        goto RETURN;
    }

    /*6. remove rmep from db*/
    ret = _sys_goldengate_bfd_rmep_del_from_db(lchip, p_sys_rmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Rmep: del db fail!!\n");
        goto RETURN;
    }

    /*7. free rmep index*/
    ret = _sys_goldengate_bfd_rmep_free_index(lchip, p_sys_rmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Rmep: free index fail!!\n");
        goto RETURN;
    }

    /*8. free rmep node*/
    ret = _sys_goldengate_bfd_rmep_free_node(lchip, p_sys_rmep_bfd);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Rmep: free node fail!!\n");
        goto RETURN;
    }

RETURN:
    OAM_UNLOCK(lchip);
    return ret;
}


STATIC int32
_sys_goldengate_bfd_update_rmep(uint8 lchip, void* p_rmep)
{
    ctc_oam_update_t* p_rmep_param = (ctc_oam_update_t*)p_rmep;

    sys_oam_chan_com_t*     p_sys_chan  = NULL;
    sys_oam_lmep_bfd_t*     p_sys_lmep  = NULL;
    sys_oam_rmep_bfd_t*     p_sys_rmep  = NULL;
    int32 ret = CTC_E_NONE;

    SYS_OAM_DBG_FUNC();

    /*1. check param valid (rmep update)*/
    CTC_ERROR_RETURN(_sys_goldengate_bfd_check_update_param(lchip, p_rmep_param, FALSE));

    OAM_LOCK(lchip);

    /*2. lookup chan and check exist*/
    p_sys_chan = _sys_goldengate_bfd_chan_lkup(lchip, &p_rmep_param->key);
    if (NULL == p_sys_chan)
    {
        ret = CTC_E_OAM_CHAN_ENTRY_NOT_FOUND;
        goto RETURN;
    }

    /*3. lookup lmep and check exist*/
    p_sys_lmep = _sys_goldengate_bfd_lmep_lkup(lchip, p_sys_chan);
    if (NULL == p_sys_lmep)
    {
        ret = CTC_E_OAM_CHAN_LMEP_NOT_FOUND;
        goto RETURN;
    }

    if (CTC_FLAG_ISSET(p_sys_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_MEP_ON_CPU))
    {
        ret = CTC_E_OAM_INVALID_OPERATION_ON_CPU_MEP;
        goto RETURN;
    }

    /*4. lookup rmep and check exist*/
    p_sys_rmep = _sys_goldengate_bfd_rmep_lkup(lchip, p_sys_lmep);
    if (NULL == p_sys_rmep)
    {
        ret = CTC_E_OAM_RMEP_NOT_FOUND;
        goto RETURN;
    }

    /*5. update lmep db and asic*/
    ret = _sys_goldengate_bfd_rmep_update_asic(lchip, p_rmep_param, p_sys_rmep);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Rmep: update asic fail!!\n");
        goto RETURN;
    }

RETURN:
    OAM_UNLOCK(lchip);
    return ret;
}

STATIC int32
_sys_goldengate_bfd_set_property(uint8 lchip, void* p_prop)
{
    ctc_oam_property_t* p_prop_param = (ctc_oam_property_t*)p_prop;
    ctc_oam_bfd_prop_t* p_bfd_prop   = NULL;

    SYS_OAM_DBG_FUNC();

    CTC_PTR_VALID_CHECK(p_prop);
    p_bfd_prop= &p_prop_param->u.bfd;
    switch (p_bfd_prop->cfg_type)
    {
    case CTC_OAM_BFD_CFG_TYPE_SLOW_INTERVAL:
        CTC_ERROR_RETURN(
        _sys_goldengate_bfd_set_slow_interval(lchip, p_bfd_prop->value));
        break;
    case CTC_OAM_BFD_CFG_TYPE_TIMER_NEG_INTERVAL:
        CTC_ERROR_RETURN(
        _sys_goldengate_bfd_set_timer_neg_interval(lchip, p_bfd_prop->p_value));
        break;
    default:
        break;

        return CTC_E_NOT_SUPPORT;
    }

    return CTC_E_NONE;
}


STATIC int32
_sys_goldengate_bfd_get_mep_info(uint8 lchip, void* p_mep_info)
{
    ctc_oam_mep_info_with_key_t* p_mep_param = (ctc_oam_mep_info_with_key_t*)p_mep_info;

    sys_oam_chan_com_t*     p_sys_chan  = NULL;
    sys_oam_lmep_bfd_t*     p_sys_lmep  = NULL;
    sys_oam_rmep_bfd_t*     p_sys_rmep  = NULL;
    int32 ret = CTC_E_NONE;
    ctc_oam_mep_info_t mep_info;
    sal_memset(&mep_info, 0, sizeof(ctc_oam_mep_info_t));

    SYS_OAM_DBG_FUNC();

    /*1. check param valid (rmep update)*/
     /*CTC_ERROR_RETURN(_sys_goldengate_bfd_check_update_param(lchip, p_rmep_param, FALSE));*/

    OAM_LOCK(lchip);

    /*2. lookup chan and check exist*/
    p_sys_chan = _sys_goldengate_bfd_chan_lkup(lchip, &p_mep_param->key);
    if (NULL == p_sys_chan)
    {
        ret = CTC_E_OAM_CHAN_ENTRY_NOT_FOUND;
        goto RETURN;
    }

    if(SYS_OAM_NO_MEP_OPERATION(lchip, p_sys_chan->master_chipid))
    {
        ret = CTC_E_NONE;
        goto RETURN;
    }

    /*3. lookup lmep and check exist*/
    p_sys_lmep = _sys_goldengate_bfd_lmep_lkup(lchip, p_sys_chan);
    if (NULL == p_sys_lmep)
    {
        ret = CTC_E_OAM_CHAN_LMEP_NOT_FOUND;
        goto RETURN;
    }

    if (CTC_FLAG_ISSET(p_sys_lmep->flag, CTC_OAM_BFD_LMEP_FLAG_MEP_ON_CPU))
    {
        ret = CTC_E_OAM_INVALID_OPERATION_ON_CPU_MEP;
        goto RETURN;
    }

    /*4. lookup rmep and check exist*/
    p_sys_rmep = _sys_goldengate_bfd_rmep_lkup(lchip, p_sys_lmep);
    if (NULL == p_sys_rmep)
    {
        ret = CTC_E_OAM_RMEP_NOT_FOUND;
        goto RETURN;
    }

    mep_info.mep_index = p_sys_rmep->com.rmep_index;
    ret = _sys_goldengate_oam_get_mep_info(lchip, &mep_info);
    if (CTC_E_NONE != ret)
    {
        SYS_OAM_DBG_ERROR("Get mep info fail!!\n");
        goto RETURN;
    }
    sal_memcpy(&p_mep_param->lmep, &mep_info.lmep, sizeof(ctc_oam_lmep_info_t));
    sal_memcpy(&p_mep_param->rmep, &mep_info.rmep, sizeof(ctc_oam_rmep_info_t));

RETURN:
    OAM_UNLOCK(lchip);
    return ret;
}

int32
_sys_goldengate_bfd_learning_process(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{
    uint32 oam_type         = 0;
    uint32 payload_offset   = 0;
    uint16 lmep_index       = 0;
    uint16 rmep_index       = 0;
    uint8* bfd_payload      = NULL;
    uint32 my_disc = 0;
    uint32 remote_disc = 0;
    uint32 active = 0;
    uint32 cmd = 0;
    int32 ret = CTC_E_NONE;
    tbl_entry_t  tbl_entry;
    DsBfdRmep_m ds_bfd_rmep;
    DsBfdRmep_m ds_bfd_rmep_mask;

    sys_oam_lmep_bfd_t* p_sys_lmep_bfd = NULL;
    sys_oam_rmep_bfd_t* p_sys_rmep_bfd = NULL;

    if (0x1FFF == p_pkt_rx->rx_info.oam.mep_index)
    {
        return CTC_E_NONE;
    }

    OAM_LOCK(lchip);

    oam_type        = p_pkt_rx->rx_info.oam.type;
    payload_offset  = p_pkt_rx->rx_info.payload_offset;
    lmep_index      = p_pkt_rx->rx_info.oam.mep_index;
    rmep_index      = lmep_index + 1;

    cmd  = DRV_IOR(DsBfdRmep_t, DRV_ENTRY_FLAG);
    CTC_ERROR_GOTO(DRV_IOCTL(lchip, rmep_index, cmd, &ds_bfd_rmep), ret, End);
    active = GetDsEthRmep(V, active_f, &ds_bfd_rmep);
    if (!active)
    {
        goto End;
    }

    if (CTC_OAM_TYPE_ACH == oam_type)
    {
        payload_offset = payload_offset + 4;
    }
    bfd_payload = p_pkt_rx->pkt_buf->data + payload_offset + ((p_pkt_rx)->eth_hdr_len + (p_pkt_rx)->pkt_hdr_len);
    /*field in pkt*/
    my_disc = CTC_MAKE_UINT32(bfd_payload[4], bfd_payload[5], bfd_payload[6], bfd_payload[7]);

    p_sys_lmep_bfd = (sys_oam_lmep_bfd_t*)ctc_vector_get(g_gg_oam_master[lchip]->mep_vec, lmep_index);
    p_sys_rmep_bfd = (sys_oam_rmep_bfd_t*)ctc_vector_get(g_gg_oam_master[lchip]->mep_vec, rmep_index);
    if ((NULL == p_sys_lmep_bfd) || (NULL == p_sys_rmep_bfd))
    {
        goto End;
    }

    sal_memset(&ds_bfd_rmep, 0, sizeof(ds_bfd_rmep));
    sal_memset(&ds_bfd_rmep_mask, 0xFF, sizeof(ds_bfd_rmep_mask));
    cmd  = DRV_IOR(DsBfdRmep_t, DRV_ENTRY_FLAG);
    CTC_ERROR_GOTO(DRV_IOCTL(lchip, rmep_index, cmd, &ds_bfd_rmep), ret, End);

    /*update remote disc for learning*/
    remote_disc = GetDsBfdRmep(V, remoteDisc_f, &ds_bfd_rmep);
    if (0 == remote_disc)
    {
        SetDsBfdRmep(V, remoteDisc_f, &ds_bfd_rmep, my_disc);
        SetDsBfdRmep(V, remoteDisc_f, &ds_bfd_rmep_mask, 0);
    }

    cmd = DRV_IOW(DsBfdRmep_t, DRV_ENTRY_FLAG);
    tbl_entry.data_entry = (uint32*)&ds_bfd_rmep;
    tbl_entry.mask_entry = (uint32*)&ds_bfd_rmep_mask;
    CTC_ERROR_GOTO(DRV_IOCTL(lchip, rmep_index, cmd, &tbl_entry), ret, End);

End:
    OAM_UNLOCK(lchip);
    return ret;
}

int32
_sys_goldengate_bfd_pf_process(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{
    uint32 oam_type         = 0;
    uint32 payload_offset   = 0;
    uint16 lmep_index       = 0;
    uint16 rmep_index       = 0;
    int32 ret = 0;
    uint8* bfd_payload      = NULL;
    uint8   pbit    = 0;
    uint8   fbit    = 0;
    uint8   detect_mult = 0;
    uint32 desired_min_tx_interval  = 0;
    uint32 required_min_rx_interval = 0;
    uint32 actual_min_tx_interval = 0;
    uint32 actual_rx_interval = 0;
    uint32 old_actual_min_tx_interval = 0;
    uint32 active = 0;
    uint32 cmd = 0;
    uint32 min_interval = 100*1000;  /*100ms*/
    tbl_entry_t tbl_entry;
    DsBfdMep_m ds_bfd_mep;
    DsBfdRmep_m ds_bfd_rmep;
    DsBfdMep_m ds_bfd_mep_mask;
    DsBfdRmep_m ds_bfd_rmep_mask;

    sys_oam_lmep_bfd_t* p_sys_lmep_bfd = NULL;
    sys_oam_rmep_bfd_t* p_sys_rmep_bfd = NULL;
    sal_systime_t tv;
    oam_type        = p_pkt_rx->rx_info.oam.type;
    payload_offset  = p_pkt_rx->rx_info.payload_offset;
    lmep_index      = p_pkt_rx->rx_info.oam.mep_index;
    rmep_index      = lmep_index + 1;

    OAM_LOCK(lchip);
    cmd  = DRV_IOR(DsBfdRmep_t, DRV_ENTRY_FLAG);
    CTC_ERROR_GOTO(DRV_IOCTL(lchip, rmep_index, cmd, &ds_bfd_rmep), ret, End);
    active = GetDsEthRmep(V, active_f, &ds_bfd_rmep);
    if (!active)
    {
        OAM_UNLOCK(lchip);
        return CTC_E_NONE;
    }

    if (CTC_OAM_TYPE_ACH == oam_type)
    {
        payload_offset = payload_offset + 4;
    }
    bfd_payload = p_pkt_rx->pkt_buf->data + payload_offset + ((p_pkt_rx)->eth_hdr_len + (p_pkt_rx)->pkt_hdr_len);
    pbit = CTC_IS_BIT_SET(bfd_payload[1], 5);
    fbit = CTC_IS_BIT_SET(bfd_payload[1], 4);
    detect_mult = bfd_payload[2];
    desired_min_tx_interval  = CTC_MAKE_UINT32(bfd_payload[12], bfd_payload[13], bfd_payload[14], bfd_payload[15]);
    required_min_rx_interval = CTC_MAKE_UINT32(bfd_payload[16], bfd_payload[17], bfd_payload[18], bfd_payload[19]);

    if (pbit || fbit)
    {
        p_sys_lmep_bfd = (sys_oam_lmep_bfd_t*)ctc_vector_get(g_gg_oam_master[lchip]->mep_vec, lmep_index);
        p_sys_rmep_bfd = (sys_oam_rmep_bfd_t*)ctc_vector_get(g_gg_oam_master[lchip]->mep_vec, rmep_index);
    }

    if ((NULL == p_sys_lmep_bfd)
        || (NULL == p_sys_rmep_bfd))
    {
        OAM_UNLOCK(lchip);
        return CTC_E_NONE;
    }
    sal_gettime(&tv);
    if (pbit)
    {
        if (((tv.tv_sec - p_sys_lmep_bfd->tv_p.tv_sec)*1000000 + (tv.tv_usec - p_sys_lmep_bfd->tv_p.tv_usec)) < min_interval)
        {
            OAM_UNLOCK(lchip);
            return CTC_E_NONE;
        }
        sal_gettime(&p_sys_lmep_bfd->tv_p);
    }
    if (fbit)
    {
        if (((tv.tv_sec - p_sys_lmep_bfd->tv_f.tv_sec)*1000000 + (tv.tv_usec - p_sys_lmep_bfd->tv_f.tv_usec)) < min_interval)
        {
            OAM_UNLOCK(lchip);
            return CTC_E_NONE;
        }
        sal_gettime(&p_sys_lmep_bfd->tv_f);
    }
    SYS_OAM_DBG_INFO("BFD P/F process:\n");
    SYS_OAM_DBG_INFO("p = %d, f = %d, desired_min_tx_interval = %d, required_min_rx_interval = %d, detect_mult = %d\n",
                           pbit, fbit, desired_min_tx_interval, required_min_rx_interval, detect_mult);

    sal_memset(&ds_bfd_rmep_mask, 0xFF, sizeof(ds_bfd_rmep_t));
    sal_memset(&ds_bfd_mep_mask, 0xFF, sizeof(ds_bfd_mep_t));
    cmd = DRV_IOR(DsBfdRmep_t, DRV_ENTRY_FLAG);
    ret = (DRV_IOCTL(lchip, rmep_index, cmd, &ds_bfd_rmep));
    cmd = DRV_IOR(DsBfdMep_t, DRV_ENTRY_FLAG);
    ret = (DRV_IOCTL(lchip, lmep_index, cmd, &ds_bfd_mep));
    old_actual_min_tx_interval = GetDsBfdMep(V, actualMinTxInterval_f, &ds_bfd_mep);
    if (!GetDsBfdRmep(V, pbit_f, &ds_bfd_rmep) && fbit)
    {
        OAM_UNLOCK(lchip);
        return CTC_E_NONE;
    }
    if ((required_min_rx_interval / 1000) >= p_sys_lmep_bfd->desired_min_tx_interval)
    {
        actual_min_tx_interval = required_min_rx_interval / 1000;
    }
    else
    {
        actual_min_tx_interval =  p_sys_lmep_bfd->desired_min_tx_interval;
    }

    if ((desired_min_tx_interval / 1000) >= p_sys_rmep_bfd->required_min_rx_interval)
    {
        actual_rx_interval  = (desired_min_tx_interval / 1000);
    }
    else
    {
        actual_rx_interval = p_sys_rmep_bfd->required_min_rx_interval;
    }

    if (3300 == required_min_rx_interval)/*CCM timer*/
    {
        actual_min_tx_interval = 4;
        SetDsBfdMep(V, ccTxMode_f, &ds_bfd_mep, 1);
        SetDsBfdMep(V, ccTxMode_f, &ds_bfd_rmep_mask, 0);
    }
    else
    {
        SetDsBfdMep(V, ccTxMode_f, &ds_bfd_mep, 0);
        SetDsBfdMep(V, ccTxMode_f, &ds_bfd_rmep_mask, 0);
    }

    SetDsBfdRmep(V, defectMult_f, &ds_bfd_rmep, detect_mult);
    SetDsBfdRmep(V, defectMult_f, &ds_bfd_rmep_mask, 0);
    SetDsBfdRmep(V, defectWhile_f, &ds_bfd_rmep, actual_rx_interval*detect_mult);
    SetDsBfdRmep(V, defectWhile_f, &ds_bfd_rmep_mask, 0);
    SetDsBfdRmep(V, actualRxInterval_f, &ds_bfd_rmep, actual_rx_interval);
    SetDsBfdRmep(V, actualRxInterval_f, &ds_bfd_rmep_mask, 0);
    SetDsBfdMep(V, actualMinTxInterval_f, &ds_bfd_mep, actual_min_tx_interval);
    SetDsBfdMep(V, actualMinTxInterval_f, &ds_bfd_mep_mask, 0);
    SetDsBfdMep(V, helloWhile_f, &ds_bfd_mep, 0);
    SetDsBfdMep(V, helloWhile_f, &ds_bfd_mep_mask, 0);
    SetDsBfdRmep(V, fbit_f, &ds_bfd_rmep, pbit? 1 : 0);
    SetDsBfdRmep(V, fbit_f, &ds_bfd_rmep_mask, 0);
    SetDsBfdRmep(V, pbit_f, &ds_bfd_rmep, 0);
    SetDsBfdRmep(V, pbit_f, &ds_bfd_rmep_mask, 0);

    tbl_entry.data_entry = (uint32*)&ds_bfd_rmep;
    tbl_entry.mask_entry = (uint32*)&ds_bfd_rmep_mask;
    cmd = DRV_IOW(DsBfdRmep_t, DRV_ENTRY_FLAG);
    ret = (DRV_IOCTL(lchip, rmep_index, cmd, &tbl_entry));
    if (old_actual_min_tx_interval != actual_min_tx_interval)
    {
        tbl_entry.data_entry = (uint32*)&ds_bfd_mep;
        tbl_entry.mask_entry = (uint32*)&ds_bfd_mep_mask;
        cmd = DRV_IOW(DsBfdMep_t, DRV_ENTRY_FLAG);
        ret = (DRV_IOCTL(lchip, lmep_index, cmd, &tbl_entry));
    }

    SYS_OAM_DBG_INFO("P/F result: actual_min_tx_interval = %d, actual_rx_interval = %d\n",
                           actual_min_tx_interval, actual_rx_interval);

    End:
    OAM_UNLOCK(lchip);
    return ret;
}

int32
_sys_goldengate_bfd_process(uint8 lchip, ctc_pkt_rx_t* p_pkt_rx)
{
    CTC_PTR_VALID_CHECK(p_pkt_rx);
    SYS_OAM_INIT_CHECK(lchip);
    if (CTC_OAM_EXCP_LEARNING_BFD_TO_CPU == (p_pkt_rx->rx_info.reason - CTC_PKT_CPU_REASON_OAM))
    {
       CTC_ERROR_RETURN(_sys_goldengate_bfd_learning_process(lchip, p_pkt_rx));
    }
    else if(CTC_OAM_EXCP_BFD_TIMER_NEGOTIATION == (p_pkt_rx->rx_info.reason - CTC_PKT_CPU_REASON_OAM))
    {
        CTC_ERROR_RETURN(_sys_goldengate_bfd_pf_process(lchip, p_pkt_rx));
    }
    return CTC_E_NONE;
}


int32
sys_goldengate_oam_tp_bfd_init(uint8 lchip, ctc_oam_global_t* p_oam_glb)
{
    uint8 oam_type = CTC_OAM_MEP_TYPE_MPLS_TP_BFD;

    /*************************************************
    *init global param
    *************************************************/
    if (NULL == g_gg_oam_master[lchip])
    {
        return CTC_E_NONE;
    }

    CTC_ERROR_RETURN(_sys_goldengate_bfd_mpls_register_init(lchip));

    /*************************************************
    *init callback fun
    *************************************************/
    /* check for tp, don't use this check,use _sys_goldengate_tp_bfd_mepid_check_param*/
    SYS_OAM_CHECK_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHECK_MAID)     = _sys_goldengate_tp_bfd_mepid_no_check_param;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_ADD)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_DEL)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_UPDATE) = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_CMP)    = NULL;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_ADD)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_DEL)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_UPDATE) = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_CMP)    = _sys_goldengate_tp_chan_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_ADD)    = _sys_goldengate_bfd_add_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_DEL)    = _sys_goldengate_bfd_remove_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_UPDATE) = _sys_goldengate_bfd_update_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_CMP)    = _sys_goldengate_bfd_lmep_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_ADD)    = _sys_goldengate_bfd_add_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_DEL)    = _sys_goldengate_bfd_remove_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_UPDATE) = _sys_goldengate_bfd_update_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_CMP)    = _sys_goldengate_bfd_rmep_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MIP, SYS_OAM_ADD)      = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MIP, SYS_OAM_DEL)      = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_GLOB, SYS_OAM_GET_INFO)    = _sys_goldengate_bfd_get_mep_info;

    return CTC_E_NONE;

}


int32
sys_goldengate_oam_ip_bfd_init(uint8 lchip, ctc_oam_global_t* p_oam_glb)
{
    uint8 oam_type = CTC_OAM_MEP_TYPE_IP_BFD;

    /*************************************************
    *init global param
    *************************************************/
    if (NULL == g_gg_oam_master[lchip])
    {
        return CTC_E_NONE;
    }

    CTC_ERROR_RETURN(_sys_goldengate_bfd_ip_register_init(lchip));

    /*************************************************
    *init callback fun
    *************************************************/

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_ADD)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_DEL)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_UPDATE) = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_CMP)    = NULL;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_ADD)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_DEL)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_UPDATE) = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_CMP)    = _sys_goldengate_ip_bfd_chan_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_ADD)    = _sys_goldengate_bfd_add_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_DEL)    = _sys_goldengate_bfd_remove_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_UPDATE) = _sys_goldengate_bfd_update_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_CMP)    = _sys_goldengate_bfd_lmep_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_ADD)    = _sys_goldengate_bfd_add_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_DEL)    = _sys_goldengate_bfd_remove_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_UPDATE) = _sys_goldengate_bfd_update_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_CMP)    = _sys_goldengate_bfd_rmep_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MIP, SYS_OAM_ADD)      = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MIP, SYS_OAM_DEL)      = NULL;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_GLOB, SYS_OAM_GET_INFO)    = _sys_goldengate_bfd_get_mep_info;
     /*SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_STATS, SYS_OAM_CONFIG) = _sys_goldengate_bfd_get_stats_info;*/

    SYS_OAM_FUNC_TABLE(lchip, CTC_OAM_PROPERTY_TYPE_BFD, SYS_OAM_GLOB, SYS_OAM_CONFIG)  = _sys_goldengate_bfd_set_property;
    sys_goldengate_packet_register_internal_rx_cb(lchip, (SYS_PKT_RX_CALLBACK)_sys_goldengate_bfd_process);

    return CTC_E_NONE;

}


int32
sys_goldengate_oam_mpls_bfd_init(uint8 lchip, ctc_oam_global_t* p_oam_glb)
{
    uint8 oam_type = CTC_OAM_MEP_TYPE_MPLS_BFD;

    /*************************************************
    *init global param
    *************************************************/
    if (NULL == g_gg_oam_master[lchip])
    {
        return CTC_E_NONE;
    }

    CTC_ERROR_RETURN(_sys_goldengate_bfd_mpls_register_init(lchip));
    /*************************************************
    *init callback fun
    *************************************************/

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_ADD)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_DEL)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_UPDATE) = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MAID, SYS_OAM_CMP)    = NULL;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_ADD)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_DEL)    = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_UPDATE) = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_CHAN, SYS_OAM_CMP)    = _sys_goldengate_ip_bfd_chan_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_ADD)    = _sys_goldengate_bfd_add_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_DEL)    = _sys_goldengate_bfd_remove_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_UPDATE) = _sys_goldengate_bfd_update_lmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_LMEP, SYS_OAM_CMP)    = _sys_goldengate_bfd_lmep_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_ADD)    = _sys_goldengate_bfd_add_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_DEL)    = _sys_goldengate_bfd_remove_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_UPDATE) = _sys_goldengate_bfd_update_rmep;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_RMEP, SYS_OAM_CMP)    = _sys_goldengate_bfd_rmep_lkup_cmp;

    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MIP, SYS_OAM_ADD)      = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_MIP, SYS_OAM_DEL)      = NULL;
    SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_GLOB, SYS_OAM_GET_INFO)    = _sys_goldengate_bfd_get_mep_info;
     /*SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_STATS, SYS_OAM_CONFIG) = _sys_goldengate_bfd_get_stats_info;*/
     /*SYS_OAM_FUNC_TABLE(lchip, oam_type, SYS_OAM_GLOB, SYS_OAM_CONFIG)  = _sys_goldengate_bfd_set_property;*/

    return CTC_E_NONE;

}


