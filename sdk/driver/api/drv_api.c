/**
 @file drv_api.c

 @author  Copyright (C) 2011 Centec Networks Inc.  All rights reserved.

 @date 2015-10-09

 @version v1.0

 The file contains all chip APIs of Driver layer
*/
#include "sal.h"
#include "drv_api.h"
#include "usw/include/drv_common.h"
#include "usw/include/drv_error.h"
#include "dal.h"
#include "usw/include/drv_chip_ctrl.h"

extern int32 drv_chip_common_init(uint8 lchip);
extern int32 drv_model_common_init(uint8 lchip);
extern int32 dal_get_chip_number(uint8* p_num);

/* use for testing cpu endian */
struct endian_test_e
{
    uint32 test1    : 1;
    uint32 test2    : 31;
};
typedef struct endian_test_e endian_test_t;
drv_master_t g_drv_master[DRV_MAX_CHIP_NUM];
drv_master_t* p_drv_master[DRV_MAX_CHIP_NUM] = {NULL};


#define __DRV_INTERNAL_FUNCTION_

#ifdef EMULATION_ENV
uint32 g_drv_pcs[] ={ QsgmiiPcsAneg0Cfg0_t,QsgmiiPcsAneg0Cfg1_t,QsgmiiPcsAneg0Cfg2_t,QsgmiiPcsAneg0Cfg3_t,QsgmiiPcsAneg0Cfg4_t,QsgmiiPcsAneg0Cfg5_t,QsgmiiPcsAneg0Cfg6_t,QsgmiiPcsAneg0Cfg7_t,QsgmiiPcsAneg0Cfg8_t,QsgmiiPcsAneg0Cfg9_t,QsgmiiPcsAneg0Cfg10_t,QsgmiiPcsAneg0Cfg11_t,QsgmiiPcsAneg1Cfg0_t,QsgmiiPcsAneg1Cfg1_t,QsgmiiPcsAneg1Cfg2_t,QsgmiiPcsAneg1Cfg3_t,QsgmiiPcsAneg1Cfg4_t,QsgmiiPcsAneg1Cfg5_t,QsgmiiPcsAneg1Cfg6_t,QsgmiiPcsAneg1Cfg7_t,QsgmiiPcsAneg1Cfg8_t,QsgmiiPcsAneg1Cfg9_t,QsgmiiPcsAneg1Cfg10_t,QsgmiiPcsAneg1Cfg11_t,QsgmiiPcsAneg2Cfg0_t,QsgmiiPcsAneg2Cfg1_t,QsgmiiPcsAneg2Cfg2_t,QsgmiiPcsAneg2Cfg3_t,QsgmiiPcsAneg2Cfg4_t,QsgmiiPcsAneg2Cfg5_t,QsgmiiPcsAneg2Cfg6_t,QsgmiiPcsAneg2Cfg7_t,QsgmiiPcsAneg2Cfg8_t,QsgmiiPcsAneg2Cfg9_t,QsgmiiPcsAneg2Cfg10_t,QsgmiiPcsAneg2Cfg11_t,QsgmiiPcsAneg3Cfg0_t,QsgmiiPcsAneg3Cfg1_t,QsgmiiPcsAneg3Cfg2_t,QsgmiiPcsAneg3Cfg3_t,QsgmiiPcsAneg3Cfg4_t,QsgmiiPcsAneg3Cfg5_t,QsgmiiPcsAneg3Cfg6_t,QsgmiiPcsAneg3Cfg7_t,QsgmiiPcsAneg3Cfg8_t,QsgmiiPcsAneg3Cfg9_t,QsgmiiPcsAneg3Cfg10_t,QsgmiiPcsAneg3Cfg11_t,QsgmiiPcsCfg0_t,QsgmiiPcsCfg1_t,QsgmiiPcsCfg2_t,QsgmiiPcsCfg3_t,QsgmiiPcsCfg4_t,QsgmiiPcsCfg5_t,QsgmiiPcsCfg6_t,QsgmiiPcsCfg7_t,QsgmiiPcsCfg8_t,QsgmiiPcsCfg9_t,QsgmiiPcsCfg10_t,QsgmiiPcsCfg11_t,QsgmiiPcsCodeErrCnt0_t,QsgmiiPcsCodeErrCnt1_t,QsgmiiPcsCodeErrCnt2_t,QsgmiiPcsCodeErrCnt3_t,QsgmiiPcsCodeErrCnt4_t,QsgmiiPcsCodeErrCnt5_t,QsgmiiPcsCodeErrCnt6_t,QsgmiiPcsCodeErrCnt7_t,QsgmiiPcsCodeErrCnt8_t,QsgmiiPcsCodeErrCnt9_t,QsgmiiPcsCodeErrCnt10_t,QsgmiiPcsCodeErrCnt11_t,QsgmiiPcsInterruptNormal0_t,QsgmiiPcsInterruptNormal1_t,QsgmiiPcsInterruptNormal2_t,QsgmiiPcsInterruptNormal3_t,QsgmiiPcsInterruptNormal4_t,QsgmiiPcsInterruptNormal5_t,QsgmiiPcsInterruptNormal6_t,QsgmiiPcsInterruptNormal7_t,QsgmiiPcsInterruptNormal8_t,QsgmiiPcsInterruptNormal9_t,QsgmiiPcsInterruptNormal10_t,QsgmiiPcsInterruptNormal11_t,QsgmiiPcsK281PositionDbg0_t,QsgmiiPcsK281PositionDbg1_t,QsgmiiPcsK281PositionDbg2_t,QsgmiiPcsK281PositionDbg3_t,QsgmiiPcsK281PositionDbg4_t,QsgmiiPcsK281PositionDbg5_t,QsgmiiPcsK281PositionDbg6_t,QsgmiiPcsK281PositionDbg7_t,QsgmiiPcsK281PositionDbg8_t,QsgmiiPcsK281PositionDbg9_t,QsgmiiPcsK281PositionDbg10_t,QsgmiiPcsK281PositionDbg11_t,QsgmiiPcsReserved0_t,QsgmiiPcsReserved1_t,QsgmiiPcsReserved2_t,QsgmiiPcsReserved3_t,QsgmiiPcsReserved4_t,QsgmiiPcsReserved5_t,QsgmiiPcsReserved6_t,QsgmiiPcsReserved7_t,QsgmiiPcsReserved8_t,QsgmiiPcsReserved9_t,QsgmiiPcsReserved10_t,QsgmiiPcsReserved11_t,QsgmiiPcsSoftRst0_t,QsgmiiPcsSoftRst1_t,QsgmiiPcsSoftRst2_t,QsgmiiPcsSoftRst3_t,QsgmiiPcsSoftRst4_t,QsgmiiPcsSoftRst5_t,QsgmiiPcsSoftRst6_t,QsgmiiPcsSoftRst7_t,QsgmiiPcsSoftRst8_t,QsgmiiPcsSoftRst9_t,QsgmiiPcsSoftRst10_t,QsgmiiPcsSoftRst11_t,RefDivCsPcsEeePulse_t,RefDivCsPcsLinkPulse_t,RefDivHsPcsEeePulse_t,RefDivHsPcsLinkPulse_t,RefDivPcsEeePulse_t,RefDivPcsLinkPulse_t,SharedPcsCfg0_t,SharedPcsCfg1_t,SharedPcsCfg2_t,SharedPcsCfg3_t,SharedPcsCfg4_t,SharedPcsCfg5_t,SharedPcsCfg6_t,SharedPcsCfg7_t,SharedPcsCfg8_t,SharedPcsCfg9_t,SharedPcsDsfCfg0_t,SharedPcsDsfCfg1_t,SharedPcsDsfCfg2_t,SharedPcsDsfCfg3_t,SharedPcsDsfCfg4_t,SharedPcsDsfCfg5_t,SharedPcsDsfCfg6_t,SharedPcsDsfCfg7_t,SharedPcsDsfCfg8_t,SharedPcsDsfCfg9_t,SharedPcsEeeCtl00_t,SharedPcsEeeCtl01_t,SharedPcsEeeCtl02_t,SharedPcsEeeCtl03_t,SharedPcsEeeCtl04_t,SharedPcsEeeCtl05_t,SharedPcsEeeCtl06_t,SharedPcsEeeCtl07_t,SharedPcsEeeCtl08_t,SharedPcsEeeCtl09_t,SharedPcsEeeCtl10_t,SharedPcsEeeCtl11_t,SharedPcsEeeCtl12_t,SharedPcsEeeCtl13_t,SharedPcsEeeCtl14_t,SharedPcsEeeCtl15_t,SharedPcsEeeCtl16_t,SharedPcsEeeCtl17_t,SharedPcsEeeCtl18_t,SharedPcsEeeCtl19_t,SharedPcsEeeCtl20_t,SharedPcsEeeCtl21_t,SharedPcsEeeCtl22_t,SharedPcsEeeCtl23_t,SharedPcsEeeCtl24_t,SharedPcsEeeCtl25_t,SharedPcsEeeCtl26_t,SharedPcsEeeCtl27_t,SharedPcsEeeCtl28_t,SharedPcsEeeCtl29_t,SharedPcsEeeCtl30_t,SharedPcsEeeCtl31_t,SharedPcsEeeCtl32_t,SharedPcsEeeCtl33_t,SharedPcsEeeCtl34_t,SharedPcsEeeCtl35_t,SharedPcsEeeCtl36_t,SharedPcsEeeCtl37_t,SharedPcsEeeCtl38_t,SharedPcsEeeCtl39_t,SharedPcsFecCfg0_t,SharedPcsFecCfg1_t,SharedPcsFecCfg2_t,SharedPcsFecCfg3_t,SharedPcsFecCfg4_t,
SharedPcsFecCfg5_t,SharedPcsFecCfg6_t,SharedPcsFecCfg7_t,SharedPcsFecCfg8_t,SharedPcsFecCfg9_t,SharedPcsFx0Cfg0_t,SharedPcsFx0Cfg1_t,SharedPcsFx0Cfg2_t,SharedPcsFx0Cfg3_t,SharedPcsFx0Cfg4_t,SharedPcsFx0Cfg5_t,SharedPcsFx0Cfg6_t,SharedPcsFx0Cfg7_t,SharedPcsFx0Cfg8_t,SharedPcsFx0Cfg9_t,SharedPcsFx1Cfg0_t,SharedPcsFx1Cfg1_t,SharedPcsFx1Cfg2_t,SharedPcsFx1Cfg3_t,SharedPcsFx1Cfg4_t,SharedPcsFx1Cfg5_t,SharedPcsFx1Cfg6_t,SharedPcsFx1Cfg7_t,SharedPcsFx1Cfg8_t,SharedPcsFx1Cfg9_t,SharedPcsFx2Cfg0_t,SharedPcsFx2Cfg1_t,SharedPcsFx2Cfg2_t,SharedPcsFx2Cfg3_t,SharedPcsFx2Cfg4_t,SharedPcsFx2Cfg5_t,SharedPcsFx2Cfg6_t,SharedPcsFx2Cfg7_t,SharedPcsFx2Cfg8_t,SharedPcsFx2Cfg9_t,SharedPcsFx3Cfg0_t,SharedPcsFx3Cfg1_t,SharedPcsFx3Cfg2_t,SharedPcsFx3Cfg3_t,SharedPcsFx3Cfg4_t,SharedPcsFx3Cfg5_t,SharedPcsFx3Cfg6_t,SharedPcsFx3Cfg7_t,SharedPcsFx3Cfg8_t,SharedPcsFx3Cfg9_t,SharedPcsInterruptNormal0_t,SharedPcsInterruptNormal1_t,SharedPcsInterruptNormal2_t,SharedPcsInterruptNormal3_t,SharedPcsInterruptNormal4_t,SharedPcsInterruptNormal5_t,SharedPcsInterruptNormal6_t,SharedPcsInterruptNormal7_t,SharedPcsInterruptNormal8_t,SharedPcsInterruptNormal9_t,SharedPcsLgCfg0_t,SharedPcsLgCfg1_t,SharedPcsLgCfg2_t,SharedPcsLgCfg3_t,SharedPcsLgCfg4_t,SharedPcsLgCfg5_t,SharedPcsLgCfg6_t,SharedPcsLgCfg7_t,SharedPcsLgCfg8_t,SharedPcsLgCfg9_t,SharedPcsReserved0_t,SharedPcsReserved1_t,SharedPcsReserved2_t,SharedPcsReserved3_t,SharedPcsReserved4_t,SharedPcsReserved5_t,SharedPcsReserved6_t,SharedPcsReserved7_t,SharedPcsReserved8_t,SharedPcsReserved9_t,SharedPcsSerdes0Cfg0_t,SharedPcsSerdes0Cfg1_t,SharedPcsSerdes0Cfg2_t,SharedPcsSerdes0Cfg3_t,SharedPcsSerdes0Cfg4_t,SharedPcsSerdes0Cfg5_t,SharedPcsSerdes0Cfg6_t,SharedPcsSerdes0Cfg7_t,SharedPcsSerdes0Cfg8_t,SharedPcsSerdes0Cfg9_t,SharedPcsSerdes1Cfg0_t,SharedPcsSerdes1Cfg1_t,SharedPcsSerdes1Cfg2_t,SharedPcsSerdes1Cfg3_t,SharedPcsSerdes1Cfg4_t,SharedPcsSerdes1Cfg5_t,SharedPcsSerdes1Cfg6_t,SharedPcsSerdes1Cfg7_t,SharedPcsSerdes1Cfg8_t,SharedPcsSerdes1Cfg9_t,SharedPcsSerdes2Cfg0_t,SharedPcsSerdes2Cfg1_t,SharedPcsSerdes2Cfg2_t,SharedPcsSerdes2Cfg3_t,SharedPcsSerdes2Cfg4_t,SharedPcsSerdes2Cfg5_t,SharedPcsSerdes2Cfg6_t,SharedPcsSerdes2Cfg7_t,SharedPcsSerdes2Cfg8_t,SharedPcsSerdes2Cfg9_t,SharedPcsSerdes3Cfg0_t,SharedPcsSerdes3Cfg1_t,SharedPcsSerdes3Cfg2_t,SharedPcsSerdes3Cfg3_t,SharedPcsSerdes3Cfg4_t,SharedPcsSerdes3Cfg5_t,SharedPcsSerdes3Cfg6_t,SharedPcsSerdes3Cfg7_t,SharedPcsSerdes3Cfg8_t,SharedPcsSerdes3Cfg9_t,SharedPcsSgmii0Cfg0_t,SharedPcsSgmii0Cfg1_t,SharedPcsSgmii0Cfg2_t,SharedPcsSgmii0Cfg3_t,SharedPcsSgmii0Cfg4_t,SharedPcsSgmii0Cfg5_t,SharedPcsSgmii0Cfg6_t,SharedPcsSgmii0Cfg7_t,SharedPcsSgmii0Cfg8_t,SharedPcsSgmii0Cfg9_t,SharedPcsSgmii1Cfg0_t,SharedPcsSgmii1Cfg1_t,SharedPcsSgmii1Cfg2_t,SharedPcsSgmii1Cfg3_t,SharedPcsSgmii1Cfg4_t,SharedPcsSgmii1Cfg5_t,SharedPcsSgmii1Cfg6_t,SharedPcsSgmii1Cfg7_t,SharedPcsSgmii1Cfg8_t,SharedPcsSgmii1Cfg9_t,SharedPcsSgmii2Cfg0_t,SharedPcsSgmii2Cfg1_t,SharedPcsSgmii2Cfg2_t,SharedPcsSgmii2Cfg3_t,SharedPcsSgmii2Cfg4_t,SharedPcsSgmii2Cfg5_t,SharedPcsSgmii2Cfg6_t,SharedPcsSgmii2Cfg7_t,SharedPcsSgmii2Cfg8_t,SharedPcsSgmii2Cfg9_t,SharedPcsSgmii3Cfg0_t,SharedPcsSgmii3Cfg1_t,SharedPcsSgmii3Cfg2_t,SharedPcsSgmii3Cfg3_t,SharedPcsSgmii3Cfg4_t,SharedPcsSgmii3Cfg5_t,SharedPcsSgmii3Cfg6_t,SharedPcsSgmii3Cfg7_t,SharedPcsSgmii3Cfg8_t,SharedPcsSgmii3Cfg9_t,SharedPcsSoftRst0_t,SharedPcsSoftRst1_t,SharedPcsSoftRst2_t,SharedPcsSoftRst3_t,SharedPcsSoftRst4_t,SharedPcsSoftRst5_t,SharedPcsSoftRst6_t,SharedPcsSoftRst7_t,SharedPcsSoftRst8_t,SharedPcsSoftRst9_t,SharedPcsXauiCfg0_t,SharedPcsXauiCfg1_t,SharedPcsXauiCfg2_t,SharedPcsXauiCfg3_t,SharedPcsXauiCfg4_t,SharedPcsXauiCfg5_t,SharedPcsXauiCfg6_t,SharedPcsXauiCfg7_t,SharedPcsXauiCfg8_t,SharedPcsXauiCfg9_t,SharedPcsXfi0Cfg0_t,SharedPcsXfi0Cfg1_t,SharedPcsXfi0Cfg2_t,SharedPcsXfi0Cfg3_t,SharedPcsXfi0Cfg4_t,SharedPcsXfi0Cfg5_t,SharedPcsXfi0Cfg6_t,SharedPcsXfi0Cfg7_t,SharedPcsXfi0Cfg8_t,SharedPcsXfi0Cfg9_t,SharedPcsXfi1Cfg0_t,SharedPcsXfi1Cfg1_t,SharedPcsXfi1Cfg2_t,SharedPcsXfi1Cfg3_t,SharedPcsXfi1Cfg4_t,SharedPcsXfi1Cfg5_t,SharedPcsXfi1Cfg6_t,SharedPcsXfi1Cfg7_t,
SharedPcsXfi1Cfg8_t,SharedPcsXfi1Cfg9_t,SharedPcsXfi2Cfg0_t,SharedPcsXfi2Cfg1_t,SharedPcsXfi2Cfg2_t,SharedPcsXfi2Cfg3_t,SharedPcsXfi2Cfg4_t,SharedPcsXfi2Cfg5_t,SharedPcsXfi2Cfg6_t,SharedPcsXfi2Cfg7_t,SharedPcsXfi2Cfg8_t,SharedPcsXfi2Cfg9_t,SharedPcsXfi3Cfg0_t,SharedPcsXfi3Cfg1_t,SharedPcsXfi3Cfg2_t,SharedPcsXfi3Cfg3_t,SharedPcsXfi3Cfg4_t,SharedPcsXfi3Cfg5_t,SharedPcsXfi3Cfg6_t,SharedPcsXfi3Cfg7_t,SharedPcsXfi3Cfg8_t,SharedPcsXfi3Cfg9_t,SharedPcsXgFecCfg0_t,SharedPcsXgFecCfg1_t,SharedPcsXgFecCfg2_t,SharedPcsXgFecCfg3_t,SharedPcsXgFecCfg4_t,SharedPcsXgFecCfg5_t,SharedPcsXgFecCfg6_t,SharedPcsXgFecCfg7_t,SharedPcsXgFecMon0_t,SharedPcsXgFecMon1_t,SharedPcsXgFecMon2_t,SharedPcsXgFecMon3_t,SharedPcsXgFecMon4_t,SharedPcsXgFecMon5_t,SharedPcsXgFecMon6_t,SharedPcsXgFecMon7_t,SharedPcsXlgCfg0_t,SharedPcsXlgCfg1_t,SharedPcsXlgCfg2_t,SharedPcsXlgCfg3_t,SharedPcsXlgCfg4_t,SharedPcsXlgCfg5_t,SharedPcsXlgCfg6_t,SharedPcsXlgCfg7_t,SharedPcsXlgCfg8_t,SharedPcsXlgCfg9_t,UsxgmiiPcsAmCtl0_t,UsxgmiiPcsAmCtl1_t,UsxgmiiPcsAmCtl2_t,UsxgmiiPcsAmCtl3_t,UsxgmiiPcsAmCtl4_t,UsxgmiiPcsAmCtl5_t,UsxgmiiPcsAmCtl6_t,UsxgmiiPcsAmCtl7_t,UsxgmiiPcsAmCtl8_t,UsxgmiiPcsAmCtl9_t,UsxgmiiPcsAmCtl10_t,UsxgmiiPcsAmCtl11_t,UsxgmiiPcsAneg0Cfg0_t,UsxgmiiPcsAneg0Cfg1_t,UsxgmiiPcsAneg0Cfg2_t,UsxgmiiPcsAneg0Cfg3_t,UsxgmiiPcsAneg0Cfg4_t,UsxgmiiPcsAneg0Cfg5_t,UsxgmiiPcsAneg0Cfg6_t,UsxgmiiPcsAneg0Cfg7_t,UsxgmiiPcsAneg0Cfg8_t,UsxgmiiPcsAneg0Cfg9_t,UsxgmiiPcsAneg0Cfg10_t,UsxgmiiPcsAneg0Cfg11_t,UsxgmiiPcsAneg1Cfg0_t,UsxgmiiPcsAneg1Cfg1_t,UsxgmiiPcsAneg1Cfg2_t,UsxgmiiPcsAneg1Cfg3_t,UsxgmiiPcsAneg1Cfg4_t,UsxgmiiPcsAneg1Cfg5_t,UsxgmiiPcsAneg1Cfg6_t,UsxgmiiPcsAneg1Cfg7_t,UsxgmiiPcsAneg1Cfg8_t,UsxgmiiPcsAneg1Cfg9_t,UsxgmiiPcsAneg1Cfg10_t,UsxgmiiPcsAneg1Cfg11_t,UsxgmiiPcsAneg2Cfg0_t,UsxgmiiPcsAneg2Cfg1_t,UsxgmiiPcsAneg2Cfg2_t,UsxgmiiPcsAneg2Cfg3_t,UsxgmiiPcsAneg2Cfg4_t,UsxgmiiPcsAneg2Cfg5_t,UsxgmiiPcsAneg2Cfg6_t,UsxgmiiPcsAneg2Cfg7_t,UsxgmiiPcsAneg2Cfg8_t,UsxgmiiPcsAneg2Cfg9_t,UsxgmiiPcsAneg2Cfg10_t,UsxgmiiPcsAneg2Cfg11_t,UsxgmiiPcsAneg3Cfg0_t,UsxgmiiPcsAneg3Cfg1_t,UsxgmiiPcsAneg3Cfg2_t,UsxgmiiPcsAneg3Cfg3_t,UsxgmiiPcsAneg3Cfg4_t,UsxgmiiPcsAneg3Cfg5_t,UsxgmiiPcsAneg3Cfg6_t,UsxgmiiPcsAneg3Cfg7_t,UsxgmiiPcsAneg3Cfg8_t,UsxgmiiPcsAneg3Cfg9_t,UsxgmiiPcsAneg3Cfg10_t,UsxgmiiPcsAneg3Cfg11_t,UsxgmiiPcsCfg0_t,UsxgmiiPcsCfg1_t,UsxgmiiPcsCfg2_t,UsxgmiiPcsCfg3_t,UsxgmiiPcsCfg4_t,UsxgmiiPcsCfg5_t,UsxgmiiPcsCfg6_t,UsxgmiiPcsCfg7_t,UsxgmiiPcsCfg8_t,UsxgmiiPcsCfg9_t,UsxgmiiPcsCfg10_t,UsxgmiiPcsCfg11_t,UsxgmiiPcsEeeCtl00_t,UsxgmiiPcsEeeCtl01_t,UsxgmiiPcsEeeCtl02_t,UsxgmiiPcsEeeCtl03_t,UsxgmiiPcsEeeCtl04_t,UsxgmiiPcsEeeCtl05_t,UsxgmiiPcsEeeCtl06_t,UsxgmiiPcsEeeCtl07_t,UsxgmiiPcsEeeCtl08_t,UsxgmiiPcsEeeCtl09_t,UsxgmiiPcsEeeCtl10_t,UsxgmiiPcsEeeCtl11_t,UsxgmiiPcsEeeCtl12_t,UsxgmiiPcsEeeCtl13_t,UsxgmiiPcsEeeCtl14_t,UsxgmiiPcsEeeCtl15_t,UsxgmiiPcsEeeCtl16_t,UsxgmiiPcsEeeCtl17_t,UsxgmiiPcsEeeCtl18_t,UsxgmiiPcsEeeCtl19_t,UsxgmiiPcsEeeCtl20_t,UsxgmiiPcsEeeCtl21_t,UsxgmiiPcsEeeCtl22_t,UsxgmiiPcsEeeCtl23_t,UsxgmiiPcsEeeCtl24_t,UsxgmiiPcsEeeCtl25_t,UsxgmiiPcsEeeCtl26_t,UsxgmiiPcsEeeCtl27_t,UsxgmiiPcsEeeCtl28_t,UsxgmiiPcsEeeCtl29_t,UsxgmiiPcsEeeCtl30_t,UsxgmiiPcsEeeCtl31_t,UsxgmiiPcsEeeCtl32_t,UsxgmiiPcsEeeCtl33_t,UsxgmiiPcsEeeCtl34_t,UsxgmiiPcsEeeCtl35_t,UsxgmiiPcsEeeCtl36_t,UsxgmiiPcsEeeCtl37_t,UsxgmiiPcsEeeCtl38_t,UsxgmiiPcsEeeCtl39_t,UsxgmiiPcsEeeCtl010_t,UsxgmiiPcsEeeCtl011_t,UsxgmiiPcsEeeCtl110_t,UsxgmiiPcsEeeCtl111_t,UsxgmiiPcsEeeCtl210_t,UsxgmiiPcsEeeCtl211_t,UsxgmiiPcsEeeCtl310_t,UsxgmiiPcsEeeCtl311_t,UsxgmiiPcsInterruptNormal0_t,UsxgmiiPcsInterruptNormal1_t,UsxgmiiPcsInterruptNormal2_t,UsxgmiiPcsInterruptNormal3_t,UsxgmiiPcsInterruptNormal4_t,UsxgmiiPcsInterruptNormal5_t,UsxgmiiPcsInterruptNormal6_t,UsxgmiiPcsInterruptNormal7_t,UsxgmiiPcsInterruptNormal8_t,UsxgmiiPcsInterruptNormal9_t,UsxgmiiPcsInterruptNormal10_t,UsxgmiiPcsInterruptNormal11_t,UsxgmiiPcsPortNumCtl0_t,UsxgmiiPcsPortNumCtl1_t,UsxgmiiPcsPortNumCtl2_t,UsxgmiiPcsPortNumCtl3_t,UsxgmiiPcsPortNumCtl4_t,UsxgmiiPcsPortNumCtl5_t,UsxgmiiPcsPortNumCtl6_t,UsxgmiiPcsPortNumCtl7_t,UsxgmiiPcsPortNumCtl8_t,UsxgmiiPcsPortNumCtl9_t,
UsxgmiiPcsPortNumCtl10_t,UsxgmiiPcsPortNumCtl11_t,UsxgmiiPcsReserved0_t,UsxgmiiPcsReserved1_t,UsxgmiiPcsReserved2_t,UsxgmiiPcsReserved3_t,UsxgmiiPcsReserved4_t,UsxgmiiPcsReserved5_t,UsxgmiiPcsReserved6_t,UsxgmiiPcsReserved7_t,UsxgmiiPcsReserved8_t,UsxgmiiPcsReserved9_t,UsxgmiiPcsReserved10_t,UsxgmiiPcsReserved11_t,UsxgmiiPcsRxPortRemapCtl0_t,UsxgmiiPcsRxPortRemapCtl1_t,UsxgmiiPcsRxPortRemapCtl2_t,UsxgmiiPcsRxPortRemapCtl3_t,UsxgmiiPcsRxPortRemapCtl4_t,UsxgmiiPcsRxPortRemapCtl5_t,UsxgmiiPcsRxPortRemapCtl6_t,UsxgmiiPcsRxPortRemapCtl7_t,UsxgmiiPcsRxPortRemapCtl8_t,UsxgmiiPcsRxPortRemapCtl9_t,UsxgmiiPcsRxPortRemapCtl10_t,UsxgmiiPcsRxPortRemapCtl11_t,UsxgmiiPcsSerdesCfg0_t,UsxgmiiPcsSerdesCfg1_t,UsxgmiiPcsSerdesCfg2_t,UsxgmiiPcsSerdesCfg3_t,UsxgmiiPcsSerdesCfg4_t,UsxgmiiPcsSerdesCfg5_t,UsxgmiiPcsSerdesCfg6_t,UsxgmiiPcsSerdesCfg7_t,UsxgmiiPcsSerdesCfg8_t,UsxgmiiPcsSerdesCfg9_t,UsxgmiiPcsSerdesCfg10_t,UsxgmiiPcsSerdesCfg11_t,UsxgmiiPcsSoftRst0_t,UsxgmiiPcsSoftRst1_t,UsxgmiiPcsSoftRst2_t,UsxgmiiPcsSoftRst3_t,UsxgmiiPcsSoftRst4_t,UsxgmiiPcsSoftRst5_t,UsxgmiiPcsSoftRst6_t,UsxgmiiPcsSoftRst7_t,UsxgmiiPcsSoftRst8_t,UsxgmiiPcsSoftRst9_t,UsxgmiiPcsSoftRst10_t,UsxgmiiPcsSoftRst11_t,UsxgmiiPcsTxPortRemapCtl0_t,UsxgmiiPcsTxPortRemapCtl1_t,UsxgmiiPcsTxPortRemapCtl2_t,UsxgmiiPcsTxPortRemapCtl3_t,UsxgmiiPcsTxPortRemapCtl4_t,UsxgmiiPcsTxPortRemapCtl5_t,UsxgmiiPcsTxPortRemapCtl6_t,UsxgmiiPcsTxPortRemapCtl7_t,UsxgmiiPcsTxPortRemapCtl8_t,UsxgmiiPcsTxPortRemapCtl9_t,UsxgmiiPcsTxPortRemapCtl10_t,UsxgmiiPcsTxPortRemapCtl11_t,UsxgmiiPcsXfi0Cfg0_t,UsxgmiiPcsXfi0Cfg1_t,UsxgmiiPcsXfi0Cfg2_t,UsxgmiiPcsXfi0Cfg3_t,UsxgmiiPcsXfi0Cfg4_t,UsxgmiiPcsXfi0Cfg5_t,UsxgmiiPcsXfi0Cfg6_t,UsxgmiiPcsXfi0Cfg7_t,UsxgmiiPcsXfi0Cfg8_t,UsxgmiiPcsXfi0Cfg9_t,UsxgmiiPcsXfi0Cfg10_t,UsxgmiiPcsXfi0Cfg11_t,UsxgmiiPcsXfi1Cfg0_t,UsxgmiiPcsXfi1Cfg1_t,UsxgmiiPcsXfi1Cfg2_t,UsxgmiiPcsXfi1Cfg3_t,UsxgmiiPcsXfi1Cfg4_t,UsxgmiiPcsXfi1Cfg5_t,UsxgmiiPcsXfi1Cfg6_t,UsxgmiiPcsXfi1Cfg7_t,UsxgmiiPcsXfi1Cfg8_t,UsxgmiiPcsXfi1Cfg9_t,UsxgmiiPcsXfi1Cfg10_t,UsxgmiiPcsXfi1Cfg11_t,UsxgmiiPcsXfi2Cfg0_t,UsxgmiiPcsXfi2Cfg1_t,UsxgmiiPcsXfi2Cfg2_t,UsxgmiiPcsXfi2Cfg3_t,UsxgmiiPcsXfi2Cfg4_t,UsxgmiiPcsXfi2Cfg5_t,UsxgmiiPcsXfi2Cfg6_t,UsxgmiiPcsXfi2Cfg7_t,UsxgmiiPcsXfi2Cfg8_t,UsxgmiiPcsXfi2Cfg9_t,UsxgmiiPcsXfi2Cfg10_t,UsxgmiiPcsXfi2Cfg11_t,UsxgmiiPcsXfi3Cfg0_t,UsxgmiiPcsXfi3Cfg1_t,UsxgmiiPcsXfi3Cfg2_t,UsxgmiiPcsXfi3Cfg3_t,UsxgmiiPcsXfi3Cfg4_t,UsxgmiiPcsXfi3Cfg5_t,UsxgmiiPcsXfi3Cfg6_t,UsxgmiiPcsXfi3Cfg7_t,UsxgmiiPcsXfi3Cfg8_t,UsxgmiiPcsXfi3Cfg9_t,UsxgmiiPcsXfi3Cfg10_t,UsxgmiiPcsXfi3Cfg11_t,QsgmiiPcsAneg0Cfg0_t,QsgmiiPcsAneg0Cfg1_t,QsgmiiPcsAneg0Cfg2_t,QsgmiiPcsAneg0Cfg3_t,QsgmiiPcsAneg0Cfg4_t,QsgmiiPcsAneg0Cfg5_t,QsgmiiPcsAneg0Cfg6_t,QsgmiiPcsAneg0Cfg7_t,QsgmiiPcsAneg0Cfg8_t,QsgmiiPcsAneg0Cfg9_t,QsgmiiPcsAneg0Cfg10_t,QsgmiiPcsAneg0Cfg11_t,QsgmiiPcsAneg1Cfg0_t,QsgmiiPcsAneg1Cfg1_t,QsgmiiPcsAneg1Cfg2_t,QsgmiiPcsAneg1Cfg3_t,QsgmiiPcsAneg1Cfg4_t,QsgmiiPcsAneg1Cfg5_t,QsgmiiPcsAneg1Cfg6_t,QsgmiiPcsAneg1Cfg7_t,QsgmiiPcsAneg1Cfg8_t,QsgmiiPcsAneg1Cfg9_t,QsgmiiPcsAneg1Cfg10_t,QsgmiiPcsAneg1Cfg11_t,QsgmiiPcsAneg2Cfg0_t,QsgmiiPcsAneg2Cfg1_t,QsgmiiPcsAneg2Cfg2_t,QsgmiiPcsAneg2Cfg3_t,QsgmiiPcsAneg2Cfg4_t,QsgmiiPcsAneg2Cfg5_t,QsgmiiPcsAneg2Cfg6_t,QsgmiiPcsAneg2Cfg7_t,QsgmiiPcsAneg2Cfg8_t,QsgmiiPcsAneg2Cfg9_t,QsgmiiPcsAneg2Cfg10_t,QsgmiiPcsAneg2Cfg11_t,QsgmiiPcsAneg3Cfg0_t,QsgmiiPcsAneg3Cfg1_t,QsgmiiPcsAneg3Cfg2_t,QsgmiiPcsAneg3Cfg3_t,QsgmiiPcsAneg3Cfg4_t,QsgmiiPcsAneg3Cfg5_t,QsgmiiPcsAneg3Cfg6_t,QsgmiiPcsAneg3Cfg7_t,QsgmiiPcsAneg3Cfg8_t,QsgmiiPcsAneg3Cfg9_t,QsgmiiPcsAneg3Cfg10_t,QsgmiiPcsAneg3Cfg11_t,QsgmiiPcsCfg0_t,QsgmiiPcsCfg1_t,QsgmiiPcsCfg2_t,QsgmiiPcsCfg3_t,QsgmiiPcsCfg4_t,QsgmiiPcsCfg5_t,QsgmiiPcsCfg6_t,QsgmiiPcsCfg7_t,QsgmiiPcsCfg8_t,QsgmiiPcsCfg9_t,QsgmiiPcsCfg10_t,QsgmiiPcsCfg11_t,QsgmiiPcsCodeErrCnt0_t,QsgmiiPcsCodeErrCnt1_t,QsgmiiPcsCodeErrCnt2_t,QsgmiiPcsCodeErrCnt3_t,QsgmiiPcsCodeErrCnt4_t,QsgmiiPcsCodeErrCnt5_t,QsgmiiPcsCodeErrCnt6_t,QsgmiiPcsCodeErrCnt7_t,QsgmiiPcsCodeErrCnt8_t,QsgmiiPcsCodeErrCnt9_t,QsgmiiPcsCodeErrCnt10_t,QsgmiiPcsCodeErrCnt11_t,QsgmiiPcsInterruptNormal0_t,QsgmiiPcsInterruptNormal1_t,
QsgmiiPcsInterruptNormal2_t,QsgmiiPcsInterruptNormal3_t,QsgmiiPcsInterruptNormal4_t,QsgmiiPcsInterruptNormal5_t,QsgmiiPcsInterruptNormal6_t,QsgmiiPcsInterruptNormal7_t,QsgmiiPcsInterruptNormal8_t,QsgmiiPcsInterruptNormal9_t,QsgmiiPcsInterruptNormal10_t,QsgmiiPcsInterruptNormal11_t,QsgmiiPcsK281PositionDbg0_t,QsgmiiPcsK281PositionDbg1_t,QsgmiiPcsK281PositionDbg2_t,QsgmiiPcsK281PositionDbg3_t,QsgmiiPcsK281PositionDbg4_t,QsgmiiPcsK281PositionDbg5_t,QsgmiiPcsK281PositionDbg6_t,QsgmiiPcsK281PositionDbg7_t,QsgmiiPcsK281PositionDbg8_t,QsgmiiPcsK281PositionDbg9_t,QsgmiiPcsK281PositionDbg10_t,QsgmiiPcsK281PositionDbg11_t,QsgmiiPcsSoftRst0_t,QsgmiiPcsSoftRst1_t,QsgmiiPcsSoftRst2_t,QsgmiiPcsSoftRst3_t,QsgmiiPcsSoftRst4_t,QsgmiiPcsSoftRst5_t,QsgmiiPcsSoftRst6_t,QsgmiiPcsSoftRst7_t,QsgmiiPcsSoftRst8_t,QsgmiiPcsSoftRst9_t,QsgmiiPcsSoftRst10_t,QsgmiiPcsSoftRst11_t,RefDivCsPcsEeePulse_t,RefDivCsPcsLinkPulse_t,RefDivHsPcsEeePulse_t,RefDivHsPcsLinkPulse_t,RefDivPcsEeePulse_t,RefDivPcsLinkPulse_t,SharedPcsCfg0_t,SharedPcsCfg1_t,SharedPcsCfg2_t,SharedPcsCfg3_t,SharedPcsCfg4_t,SharedPcsCfg5_t,SharedPcsCfg6_t,SharedPcsCfg7_t,SharedPcsCfg8_t,SharedPcsCfg9_t,SharedPcsDsfCfg0_t,SharedPcsDsfCfg1_t,SharedPcsDsfCfg2_t,SharedPcsDsfCfg3_t,SharedPcsDsfCfg4_t,SharedPcsDsfCfg5_t,SharedPcsDsfCfg6_t,SharedPcsDsfCfg7_t,SharedPcsDsfCfg8_t,SharedPcsDsfCfg9_t,SharedPcsEeeCtl00_t,SharedPcsEeeCtl01_t,SharedPcsEeeCtl02_t,SharedPcsEeeCtl03_t,SharedPcsEeeCtl04_t,SharedPcsEeeCtl05_t,SharedPcsEeeCtl06_t,SharedPcsEeeCtl07_t,SharedPcsEeeCtl08_t,SharedPcsEeeCtl09_t,SharedPcsEeeCtl10_t,SharedPcsEeeCtl11_t,SharedPcsEeeCtl12_t,SharedPcsEeeCtl13_t,SharedPcsEeeCtl14_t,SharedPcsEeeCtl15_t,SharedPcsEeeCtl16_t,SharedPcsEeeCtl17_t,SharedPcsEeeCtl18_t,SharedPcsEeeCtl19_t,SharedPcsEeeCtl20_t,SharedPcsEeeCtl21_t,SharedPcsEeeCtl22_t,SharedPcsEeeCtl23_t,SharedPcsEeeCtl24_t,SharedPcsEeeCtl25_t,SharedPcsEeeCtl26_t,SharedPcsEeeCtl27_t,SharedPcsEeeCtl28_t,SharedPcsEeeCtl29_t,SharedPcsEeeCtl30_t,SharedPcsEeeCtl31_t,SharedPcsEeeCtl32_t,SharedPcsEeeCtl33_t,SharedPcsEeeCtl34_t,SharedPcsEeeCtl35_t,SharedPcsEeeCtl36_t,SharedPcsEeeCtl37_t,SharedPcsEeeCtl38_t,SharedPcsEeeCtl39_t,SharedPcsFecCfg0_t,SharedPcsFecCfg1_t,SharedPcsFecCfg2_t,SharedPcsFecCfg3_t,SharedPcsFecCfg4_t,SharedPcsFecCfg5_t,SharedPcsFecCfg6_t,SharedPcsFecCfg7_t,SharedPcsFecCfg8_t,SharedPcsFecCfg9_t,SharedPcsFx0Cfg0_t,SharedPcsFx0Cfg1_t,SharedPcsFx0Cfg2_t,SharedPcsFx0Cfg3_t,SharedPcsFx0Cfg4_t,SharedPcsFx0Cfg5_t,SharedPcsFx0Cfg6_t,SharedPcsFx0Cfg7_t,SharedPcsFx0Cfg8_t,SharedPcsFx0Cfg9_t,SharedPcsFx1Cfg0_t,SharedPcsFx1Cfg1_t,SharedPcsFx1Cfg2_t,SharedPcsFx1Cfg3_t,SharedPcsFx1Cfg4_t,SharedPcsFx1Cfg5_t,SharedPcsFx1Cfg6_t,SharedPcsFx1Cfg7_t,SharedPcsFx1Cfg8_t,SharedPcsFx1Cfg9_t,SharedPcsFx2Cfg0_t,SharedPcsFx2Cfg1_t,SharedPcsFx2Cfg2_t,SharedPcsFx2Cfg3_t,SharedPcsFx2Cfg4_t,SharedPcsFx2Cfg5_t,SharedPcsFx2Cfg6_t,SharedPcsFx2Cfg7_t,SharedPcsFx2Cfg8_t,SharedPcsFx2Cfg9_t,SharedPcsFx3Cfg0_t,SharedPcsFx3Cfg1_t,SharedPcsFx3Cfg2_t,SharedPcsFx3Cfg3_t,SharedPcsFx3Cfg4_t,SharedPcsFx3Cfg5_t,SharedPcsFx3Cfg6_t,SharedPcsFx3Cfg7_t,SharedPcsFx3Cfg8_t,SharedPcsFx3Cfg9_t,SharedPcsInterruptNormal0_t,SharedPcsInterruptNormal1_t,SharedPcsInterruptNormal2_t,SharedPcsInterruptNormal3_t,SharedPcsInterruptNormal4_t,SharedPcsInterruptNormal5_t,SharedPcsInterruptNormal6_t,SharedPcsInterruptNormal7_t,SharedPcsInterruptNormal8_t,SharedPcsInterruptNormal9_t,SharedPcsLgCfg0_t,SharedPcsLgCfg1_t,SharedPcsLgCfg2_t,SharedPcsLgCfg3_t,SharedPcsLgCfg4_t,SharedPcsLgCfg5_t,SharedPcsLgCfg6_t,SharedPcsLgCfg7_t,SharedPcsLgCfg8_t,SharedPcsLgCfg9_t,SharedPcsReserved0_t,SharedPcsReserved1_t,SharedPcsReserved2_t,SharedPcsReserved3_t,SharedPcsReserved4_t,SharedPcsReserved5_t,SharedPcsReserved6_t,SharedPcsReserved7_t,SharedPcsReserved8_t,SharedPcsReserved9_t,SharedPcsSerdes0Cfg0_t,SharedPcsSerdes0Cfg1_t,SharedPcsSerdes0Cfg2_t,SharedPcsSerdes0Cfg3_t,SharedPcsSerdes0Cfg4_t,SharedPcsSerdes0Cfg5_t,SharedPcsSerdes0Cfg6_t,SharedPcsSerdes0Cfg7_t,SharedPcsSerdes0Cfg8_t,SharedPcsSerdes0Cfg9_t,SharedPcsSerdes1Cfg0_t,SharedPcsSerdes1Cfg1_t,SharedPcsSerdes1Cfg2_t,SharedPcsSerdes1Cfg3_t,
SharedPcsSerdes1Cfg4_t,SharedPcsSerdes1Cfg5_t,SharedPcsSerdes1Cfg6_t,SharedPcsSerdes1Cfg7_t,SharedPcsSerdes1Cfg8_t,SharedPcsSerdes1Cfg9_t,SharedPcsSerdes2Cfg0_t,SharedPcsSerdes2Cfg1_t,SharedPcsSerdes2Cfg2_t,SharedPcsSerdes2Cfg3_t,SharedPcsSerdes2Cfg4_t,SharedPcsSerdes2Cfg5_t,SharedPcsSerdes2Cfg6_t,SharedPcsSerdes2Cfg7_t,SharedPcsSerdes2Cfg8_t,SharedPcsSerdes2Cfg9_t,SharedPcsSerdes3Cfg0_t,SharedPcsSerdes3Cfg1_t,SharedPcsSerdes3Cfg2_t,SharedPcsSerdes3Cfg3_t,SharedPcsSerdes3Cfg4_t,SharedPcsSerdes3Cfg5_t,SharedPcsSerdes3Cfg6_t,SharedPcsSerdes3Cfg7_t,SharedPcsSerdes3Cfg8_t,SharedPcsSerdes3Cfg9_t,SharedPcsSgmii0Cfg0_t,SharedPcsSgmii0Cfg1_t,SharedPcsSgmii0Cfg2_t,SharedPcsSgmii0Cfg3_t,SharedPcsSgmii0Cfg4_t,SharedPcsSgmii0Cfg5_t,SharedPcsSgmii0Cfg6_t,SharedPcsSgmii0Cfg7_t,SharedPcsSgmii0Cfg8_t,SharedPcsSgmii0Cfg9_t,SharedPcsSgmii1Cfg0_t,SharedPcsSgmii1Cfg1_t,SharedPcsSgmii1Cfg2_t,SharedPcsSgmii1Cfg3_t,SharedPcsSgmii1Cfg4_t,SharedPcsSgmii1Cfg5_t,SharedPcsSgmii1Cfg6_t,SharedPcsSgmii1Cfg7_t,SharedPcsSgmii1Cfg8_t,SharedPcsSgmii1Cfg9_t,SharedPcsSgmii2Cfg0_t,SharedPcsSgmii2Cfg1_t,SharedPcsSgmii2Cfg2_t,SharedPcsSgmii2Cfg3_t,SharedPcsSgmii2Cfg4_t,SharedPcsSgmii2Cfg5_t,SharedPcsSgmii2Cfg6_t,SharedPcsSgmii2Cfg7_t,SharedPcsSgmii2Cfg8_t,SharedPcsSgmii2Cfg9_t,SharedPcsSgmii3Cfg0_t,SharedPcsSgmii3Cfg1_t,SharedPcsSgmii3Cfg2_t,SharedPcsSgmii3Cfg3_t,SharedPcsSgmii3Cfg4_t,SharedPcsSgmii3Cfg5_t,SharedPcsSgmii3Cfg6_t,SharedPcsSgmii3Cfg7_t,SharedPcsSgmii3Cfg8_t,SharedPcsSgmii3Cfg9_t,SharedPcsSoftRst0_t,SharedPcsSoftRst1_t,SharedPcsSoftRst2_t,SharedPcsSoftRst3_t,SharedPcsSoftRst4_t,SharedPcsSoftRst5_t,SharedPcsSoftRst6_t,SharedPcsSoftRst7_t,SharedPcsSoftRst8_t,SharedPcsSoftRst9_t,SharedPcsXauiCfg0_t,SharedPcsXauiCfg1_t,SharedPcsXauiCfg2_t,SharedPcsXauiCfg3_t,SharedPcsXauiCfg4_t,SharedPcsXauiCfg5_t,SharedPcsXauiCfg6_t,SharedPcsXauiCfg7_t,SharedPcsXauiCfg8_t,SharedPcsXauiCfg9_t,SharedPcsXfi0Cfg0_t,SharedPcsXfi0Cfg1_t,SharedPcsXfi0Cfg2_t,SharedPcsXfi0Cfg3_t,SharedPcsXfi0Cfg4_t,SharedPcsXfi0Cfg5_t,SharedPcsXfi0Cfg6_t,SharedPcsXfi0Cfg7_t,SharedPcsXfi0Cfg8_t,SharedPcsXfi0Cfg9_t,SharedPcsXfi1Cfg0_t,SharedPcsXfi1Cfg1_t,SharedPcsXfi1Cfg2_t,SharedPcsXfi1Cfg3_t,SharedPcsXfi1Cfg4_t,SharedPcsXfi1Cfg5_t,SharedPcsXfi1Cfg6_t,SharedPcsXfi1Cfg7_t,SharedPcsXfi1Cfg8_t,SharedPcsXfi1Cfg9_t,SharedPcsXfi2Cfg0_t,SharedPcsXfi2Cfg1_t,SharedPcsXfi2Cfg2_t,SharedPcsXfi2Cfg3_t,SharedPcsXfi2Cfg4_t,SharedPcsXfi2Cfg5_t,SharedPcsXfi2Cfg6_t,SharedPcsXfi2Cfg7_t,SharedPcsXfi2Cfg8_t,SharedPcsXfi2Cfg9_t,SharedPcsXfi3Cfg0_t,SharedPcsXfi3Cfg1_t,SharedPcsXfi3Cfg2_t,SharedPcsXfi3Cfg3_t,SharedPcsXfi3Cfg4_t,SharedPcsXfi3Cfg5_t,SharedPcsXfi3Cfg6_t,SharedPcsXfi3Cfg7_t,SharedPcsXfi3Cfg8_t,SharedPcsXfi3Cfg9_t,SharedPcsXgFecCfg0_t,SharedPcsXgFecCfg1_t,SharedPcsXgFecCfg2_t,SharedPcsXgFecCfg3_t,SharedPcsXgFecCfg4_t,SharedPcsXgFecCfg5_t,SharedPcsXgFecCfg6_t,SharedPcsXgFecCfg7_t,SharedPcsXgFecMon0_t,SharedPcsXgFecMon1_t,SharedPcsXgFecMon2_t,SharedPcsXgFecMon3_t,SharedPcsXgFecMon4_t,SharedPcsXgFecMon5_t,SharedPcsXgFecMon6_t,SharedPcsXgFecMon7_t,SharedPcsXlgCfg0_t,SharedPcsXlgCfg1_t,SharedPcsXlgCfg2_t,SharedPcsXlgCfg3_t,SharedPcsXlgCfg4_t,SharedPcsXlgCfg5_t,SharedPcsXlgCfg6_t,SharedPcsXlgCfg7_t,SharedPcsXlgCfg8_t,SharedPcsXlgCfg9_t,UsxgmiiPcsAneg0Cfg0_t,UsxgmiiPcsAneg0Cfg1_t,UsxgmiiPcsAneg0Cfg2_t,UsxgmiiPcsAneg0Cfg3_t,UsxgmiiPcsAneg0Cfg4_t,UsxgmiiPcsAneg0Cfg5_t,UsxgmiiPcsAneg0Cfg6_t,UsxgmiiPcsAneg0Cfg7_t,UsxgmiiPcsAneg0Cfg8_t,UsxgmiiPcsAneg0Cfg9_t,UsxgmiiPcsAneg0Cfg10_t,UsxgmiiPcsAneg0Cfg11_t,UsxgmiiPcsAneg1Cfg0_t,UsxgmiiPcsAneg1Cfg1_t,UsxgmiiPcsAneg1Cfg2_t,UsxgmiiPcsAneg1Cfg3_t,UsxgmiiPcsAneg1Cfg4_t,UsxgmiiPcsAneg1Cfg5_t,UsxgmiiPcsAneg1Cfg6_t,UsxgmiiPcsAneg1Cfg7_t,UsxgmiiPcsAneg1Cfg8_t,UsxgmiiPcsAneg1Cfg9_t,UsxgmiiPcsAneg1Cfg10_t,UsxgmiiPcsAneg1Cfg11_t,UsxgmiiPcsAneg2Cfg0_t,UsxgmiiPcsAneg2Cfg1_t,UsxgmiiPcsAneg2Cfg2_t,UsxgmiiPcsAneg2Cfg3_t,UsxgmiiPcsAneg2Cfg4_t,UsxgmiiPcsAneg2Cfg5_t,UsxgmiiPcsAneg2Cfg6_t,UsxgmiiPcsAneg2Cfg7_t,UsxgmiiPcsAneg2Cfg8_t,UsxgmiiPcsAneg2Cfg9_t,UsxgmiiPcsAneg2Cfg10_t,UsxgmiiPcsAneg2Cfg11_t,UsxgmiiPcsAneg3Cfg0_t,UsxgmiiPcsAneg3Cfg1_t,UsxgmiiPcsAneg3Cfg2_t,UsxgmiiPcsAneg3Cfg3_t,
UsxgmiiPcsAneg3Cfg4_t,UsxgmiiPcsAneg3Cfg5_t,UsxgmiiPcsAneg3Cfg6_t,UsxgmiiPcsAneg3Cfg7_t,UsxgmiiPcsAneg3Cfg8_t,UsxgmiiPcsAneg3Cfg9_t,UsxgmiiPcsAneg3Cfg10_t,UsxgmiiPcsAneg3Cfg11_t,UsxgmiiPcsEeeCtl00_t,UsxgmiiPcsEeeCtl01_t,UsxgmiiPcsEeeCtl02_t,UsxgmiiPcsEeeCtl03_t,UsxgmiiPcsEeeCtl04_t,UsxgmiiPcsEeeCtl05_t,UsxgmiiPcsEeeCtl06_t,UsxgmiiPcsEeeCtl07_t,UsxgmiiPcsEeeCtl08_t,UsxgmiiPcsEeeCtl09_t,UsxgmiiPcsEeeCtl10_t,UsxgmiiPcsEeeCtl11_t,UsxgmiiPcsEeeCtl12_t,UsxgmiiPcsEeeCtl13_t,UsxgmiiPcsEeeCtl14_t,UsxgmiiPcsEeeCtl15_t,UsxgmiiPcsEeeCtl16_t,UsxgmiiPcsEeeCtl17_t,UsxgmiiPcsEeeCtl18_t,UsxgmiiPcsEeeCtl19_t,UsxgmiiPcsEeeCtl20_t,UsxgmiiPcsEeeCtl21_t,UsxgmiiPcsEeeCtl22_t,UsxgmiiPcsEeeCtl23_t,UsxgmiiPcsEeeCtl24_t,UsxgmiiPcsEeeCtl25_t,UsxgmiiPcsEeeCtl26_t,UsxgmiiPcsEeeCtl27_t,UsxgmiiPcsEeeCtl28_t,UsxgmiiPcsEeeCtl29_t,UsxgmiiPcsEeeCtl30_t,UsxgmiiPcsEeeCtl31_t,UsxgmiiPcsEeeCtl32_t,UsxgmiiPcsEeeCtl33_t,UsxgmiiPcsEeeCtl34_t,UsxgmiiPcsEeeCtl35_t,UsxgmiiPcsEeeCtl36_t,UsxgmiiPcsEeeCtl37_t,UsxgmiiPcsEeeCtl38_t,UsxgmiiPcsEeeCtl39_t,UsxgmiiPcsEeeCtl010_t,UsxgmiiPcsEeeCtl011_t,UsxgmiiPcsEeeCtl110_t,UsxgmiiPcsEeeCtl111_t,UsxgmiiPcsEeeCtl210_t,UsxgmiiPcsEeeCtl211_t,UsxgmiiPcsEeeCtl310_t,UsxgmiiPcsEeeCtl311_t,UsxgmiiPcsInterruptNormal0_t,UsxgmiiPcsInterruptNormal1_t,UsxgmiiPcsInterruptNormal2_t,UsxgmiiPcsInterruptNormal3_t,UsxgmiiPcsInterruptNormal4_t,UsxgmiiPcsInterruptNormal5_t,UsxgmiiPcsInterruptNormal6_t,UsxgmiiPcsInterruptNormal7_t,UsxgmiiPcsInterruptNormal8_t,UsxgmiiPcsInterruptNormal9_t,UsxgmiiPcsInterruptNormal10_t,UsxgmiiPcsInterruptNormal11_t,UsxgmiiPcsPortNumCtl0_t,UsxgmiiPcsPortNumCtl1_t,UsxgmiiPcsPortNumCtl2_t,UsxgmiiPcsPortNumCtl3_t,UsxgmiiPcsPortNumCtl4_t,UsxgmiiPcsPortNumCtl5_t,UsxgmiiPcsPortNumCtl6_t,UsxgmiiPcsPortNumCtl7_t,UsxgmiiPcsPortNumCtl8_t,UsxgmiiPcsPortNumCtl9_t,UsxgmiiPcsPortNumCtl10_t,UsxgmiiPcsPortNumCtl11_t,UsxgmiiPcsSerdesCfg0_t,UsxgmiiPcsSerdesCfg1_t,UsxgmiiPcsSerdesCfg2_t,UsxgmiiPcsSerdesCfg3_t,UsxgmiiPcsSerdesCfg4_t,UsxgmiiPcsSerdesCfg5_t,UsxgmiiPcsSerdesCfg6_t,UsxgmiiPcsSerdesCfg7_t,UsxgmiiPcsSerdesCfg8_t,UsxgmiiPcsSerdesCfg9_t,UsxgmiiPcsSerdesCfg10_t,UsxgmiiPcsSerdesCfg11_t,UsxgmiiPcsSoftRst0_t,UsxgmiiPcsSoftRst1_t,UsxgmiiPcsSoftRst2_t,UsxgmiiPcsSoftRst3_t,UsxgmiiPcsSoftRst4_t,UsxgmiiPcsSoftRst5_t,UsxgmiiPcsSoftRst6_t,UsxgmiiPcsSoftRst7_t,UsxgmiiPcsSoftRst8_t,UsxgmiiPcsSoftRst9_t,UsxgmiiPcsSoftRst10_t,UsxgmiiPcsSoftRst11_t,UsxgmiiPcsXfi0Cfg0_t,UsxgmiiPcsXfi0Cfg1_t,UsxgmiiPcsXfi0Cfg2_t,UsxgmiiPcsXfi0Cfg3_t,UsxgmiiPcsXfi0Cfg4_t,UsxgmiiPcsXfi0Cfg5_t,UsxgmiiPcsXfi0Cfg6_t,UsxgmiiPcsXfi0Cfg7_t,UsxgmiiPcsXfi0Cfg8_t,UsxgmiiPcsXfi0Cfg9_t,UsxgmiiPcsXfi0Cfg10_t,UsxgmiiPcsXfi0Cfg11_t,UsxgmiiPcsXfi1Cfg0_t,UsxgmiiPcsXfi1Cfg1_t,UsxgmiiPcsXfi1Cfg2_t,UsxgmiiPcsXfi1Cfg3_t,UsxgmiiPcsXfi1Cfg4_t,UsxgmiiPcsXfi1Cfg5_t,UsxgmiiPcsXfi1Cfg6_t,UsxgmiiPcsXfi1Cfg7_t,UsxgmiiPcsXfi1Cfg8_t,UsxgmiiPcsXfi1Cfg9_t,UsxgmiiPcsXfi1Cfg10_t,UsxgmiiPcsXfi1Cfg11_t,UsxgmiiPcsXfi2Cfg0_t,UsxgmiiPcsXfi2Cfg1_t,UsxgmiiPcsXfi2Cfg2_t,UsxgmiiPcsXfi2Cfg3_t,UsxgmiiPcsXfi2Cfg4_t,UsxgmiiPcsXfi2Cfg5_t,UsxgmiiPcsXfi2Cfg6_t,UsxgmiiPcsXfi2Cfg7_t,UsxgmiiPcsXfi2Cfg8_t,UsxgmiiPcsXfi2Cfg9_t,UsxgmiiPcsXfi2Cfg10_t,UsxgmiiPcsXfi2Cfg11_t,UsxgmiiPcsXfi3Cfg0_t,UsxgmiiPcsXfi3Cfg1_t,UsxgmiiPcsXfi3Cfg2_t,UsxgmiiPcsXfi3Cfg3_t,UsxgmiiPcsXfi3Cfg4_t,UsxgmiiPcsXfi3Cfg5_t,UsxgmiiPcsXfi3Cfg6_t,UsxgmiiPcsXfi3Cfg7_t,UsxgmiiPcsXfi3Cfg8_t,UsxgmiiPcsXfi3Cfg9_t,UsxgmiiPcsXfi3Cfg10_t,UsxgmiiPcsXfi3Cfg11_t,PllCoreCfg_t,PllCoreMon_t,SupDskCfg_t,SupMiscCfg_t,MdioInit_t,MdioInitDone_t,McuMutex0CpuCtl0_t,GlobalCtlSharedFec0_t,GlobalCtlSharedFec1_t,I2CMasterCfg0_t,I2CMasterCfg1_t,I2CMasterInit0_t,I2CMasterInit1_t,I2CMasterInitDone0_t,I2CMasterInitDone1_t,MdioScanCtl0_t,MdioScanCtl1_t,MdioCfg0_t,MdioCfg1_t,MdioUserPhy_t,MdioChanMap_t,PbCtlInterruptFatal_t,AnethStatus0_t,AnethStatus1_t,AnethStatus2_t,AnethStatus3_t,AnethStatus4_t,AnethStatus5_t,AnethStatus6_t,AnethStatus7_t,AnethStatus8_t,AnethStatus9_t,AnethStatus10_t,AnethStatus11_t,AnethStatus12_t,AnethStatus13_t,AnethStatus14_t,AnethStatus15_t,AnethStatus16_t,AnethStatus16_t,AnethStatus16_t,AnethStatus17_t,AnethStatus18_t,AnethStatus19_t,AnethStatus20_t,
AnethStatus21_t,AnethStatus22_t,AnethStatus23_t,AnethStatus24_t,AnethStatus25_t,AnethStatus26_t,AnethStatus27_t,AnethStatus28_t,AnethStatus29_t,AnethStatus30_t,AnethStatus31_t,    0xffff};
#endif


/**
 @brief get host type automatically. Little-Endian or Big-Endian.
*/
STATIC void
_drv_get_host_type(uint8 lchip)
{
    endian_test_t test;
    uint32* p_test = NULL;

    p_test = (uint32 *)&test;
    *p_test = 0;
    test.test1 = 1;

    if (*p_test == 0x01)
    {
        p_drv_master[lchip]->host_type = HOST_LE;
    }
    else
    {
        p_drv_master[lchip]->host_type = HOST_BE;
    }

    return;
}


/*mep table need mask cfg, TBD*/
STATIC int32
_drv_ioctl(uint8 lchip, int32 index, uint32 cmd, void* val)
{
    int32 action;
    tbls_id_t tbl_id;
    uint16 field_id;
    uint32* p_data_entry = NULL;
    uint32* p_mask_entry = NULL;
    tbl_entry_t entry;
    drv_io_callback_fun_t* drv_io_api;
    int32 ret = DRV_E_NONE;
    uint32 tbl_type = 0;

   #ifndef SDK_IN_KERNEL
    uint32  data_entry[MAX_ENTRY_WORD];
    uint32  mask_entry[MAX_ENTRY_WORD] ;
   #endif

    DRV_PTR_VALID_CHECK(val);

    action = DRV_IOC_OP(cmd);
    tbl_id = DRV_IOC_MEMID(cmd);
    field_id = DRV_IOC_FIELDID(cmd);

    DRV_TBL_ID_VALID_CHECK(lchip, tbl_id);

    if (action == DRV_IOC_READ || DRV_ENTRY_FLAG != field_id)
    {
#ifdef SDK_IN_KERNEL
        p_data_entry = (uint32*)mem_malloc(MEM_SYSTEM_MODULE, sizeof(uint32)*MAX_ENTRY_WORD);
        p_mask_entry = (uint32*)mem_malloc(MEM_SYSTEM_MODULE, sizeof(uint32)*MAX_ENTRY_WORD);
        if (!p_data_entry || !p_mask_entry)
        {
            return DRV_E_NO_MEMORY;
        }
#else
        p_data_entry = data_entry;
        p_mask_entry = mask_entry;
#endif
        entry.data_entry = p_data_entry;
        entry.mask_entry = p_mask_entry;
    }

    drv_io_api= &(p_drv_master[lchip]->drv_io_api);
    tbl_type = drv_usw_get_table_type(lchip, tbl_id);

    switch (action)
    {
    case DRV_IOC_WRITE:
        if ((tbl_type != DRV_TABLE_TYPE_TCAM)
            && (tbl_type != DRV_TABLE_TYPE_LPM_TCAM_IP)
            && (tbl_type != DRV_TABLE_TYPE_LPM_TCAM_NAT)
            && (tbl_type != DRV_TABLE_TYPE_STATIC_TCAM_KEY))
        {
            /* Write Sram one field of the entry */
            if ((drv_io_api->drv_sram_tbl_read) && (DRV_ENTRY_FLAG != field_id))
            {
                DRV_IF_ERROR_GOTO(drv_io_api->drv_sram_tbl_read(lchip, tbl_id, index, p_data_entry), ret, out);
                DRV_IF_ERROR_GOTO(drv_set_field(lchip, tbl_id, field_id, p_data_entry, (uint32*)val), ret, out);
                DRV_IF_ERROR_GOTO(drv_io_api->drv_sram_tbl_write(lchip, tbl_id, index, p_data_entry), ret, out);
            }
            else
            {
                DRV_IF_ERROR_GOTO(drv_io_api->drv_sram_tbl_write(lchip, tbl_id, index, (uint32*)val), ret, out);
            }
        }
        else
        {
            if ((drv_io_api->drv_tcam_tbl_read) && (DRV_ENTRY_FLAG != field_id))
            {
                DRV_IF_ERROR_GOTO(drv_io_api->drv_tcam_tbl_read(lchip, tbl_id, index, &entry), ret, out);
                DRV_IF_ERROR_GOTO(drv_set_field(lchip, tbl_id, field_id, entry.data_entry, (uint32 *)((tbl_entry_t*)val)->data_entry), ret, out);
                DRV_IF_ERROR_GOTO(drv_set_field(lchip, tbl_id, field_id, entry.mask_entry, (uint32 *)((tbl_entry_t*)val)->mask_entry), ret, out);
                DRV_IF_ERROR_GOTO(drv_io_api->drv_tcam_tbl_write(lchip, tbl_id, index, &entry), ret, out);
            }
            else
            {
                DRV_IF_ERROR_GOTO(drv_io_api->drv_tcam_tbl_write(lchip, tbl_id, index, (tbl_entry_t*)val), ret, out);
            }
        }
        break;
    case DRV_IOC_READ:
        if ((tbl_type != DRV_TABLE_TYPE_TCAM)
            && (tbl_type != DRV_TABLE_TYPE_LPM_TCAM_IP)
            && (tbl_type != DRV_TABLE_TYPE_LPM_TCAM_NAT)
            && (tbl_type != DRV_TABLE_TYPE_STATIC_TCAM_KEY))
        {
            /* Read Sram one field of the entry */
            DRV_IF_ERROR_GOTO(drv_io_api->drv_sram_tbl_read(lchip, tbl_id, index, p_data_entry), ret, out);
            if (DRV_ENTRY_FLAG != field_id)
            {
                DRV_IF_ERROR_GOTO(drv_get_field(lchip, tbl_id, field_id, p_data_entry, (uint32*)val), ret, out);
            }
            else
            {
                sal_memcpy((uint32*)val, p_data_entry, TABLE_ENTRY_SIZE(lchip, tbl_id));
            }
        }
        else
        {
            /* Read Tcam one field of the entry */
            /* For Tcam entry, we could only operate its data's filed, app should not opreate tcam field */
            DRV_IF_ERROR_GOTO(drv_io_api->drv_tcam_tbl_read(lchip, tbl_id, index, &entry), ret, out);

            if (DRV_ENTRY_FLAG != field_id)
            {
                DRV_IF_ERROR_GOTO(drv_get_field(lchip, tbl_id, field_id, (entry.data_entry), (uint32 *)((tbl_entry_t*)val)->data_entry), ret, out);
                DRV_IF_ERROR_GOTO(drv_get_field(lchip, tbl_id, field_id, (entry.mask_entry), (uint32 *)((tbl_entry_t*)val)->mask_entry), ret, out);
            }
            else
            {
                sal_memcpy((uint32 *)((tbl_entry_t*)val)->data_entry, entry.data_entry, TABLE_ENTRY_SIZE(lchip, tbl_id));
                sal_memcpy((uint32 *)((tbl_entry_t*)val)->mask_entry, entry.mask_entry, TABLE_ENTRY_SIZE(lchip, tbl_id));
            }
        }
        break;
    case DRV_IOC_REMOVE:
    /*just for tcam remove*/
        if ((tbl_type != DRV_TABLE_TYPE_TCAM)
            && (tbl_type != DRV_TABLE_TYPE_LPM_TCAM_IP)
            && (tbl_type != DRV_TABLE_TYPE_LPM_TCAM_NAT)
            && (tbl_type != DRV_TABLE_TYPE_STATIC_TCAM_KEY))
        {
            ret = DRV_E_INVALID_TCAM_INDEX;
            goto out;
        }

        if (drv_io_api->drv_tcam_tbl_remove)
        {
            DRV_IF_ERROR_GOTO(drv_io_api->drv_tcam_tbl_remove(lchip, tbl_id, index), ret, out);
        }

    default:
        break;
    }

out:

 #ifdef SDK_IN_KERNEL
    if (p_data_entry)
    {
        mem_free(p_data_entry);
    }

    if (p_mask_entry)
    {
        mem_free(p_mask_entry);
    }
#endif

    return ret;
}


STATIC int32
_drv_oam_mask_ioctl(uint8 lchip, int32 index, uint32 cmd, void* val)
{
#if(SDK_WORK_PLATFORM == 0)
    uint32 cmd_oam = 0;
#endif
    uint32* value = NULL;

#if(SDK_WORK_PLATFORM == 0)
    /*write mask*/
    value = (uint32 *)(((tbl_entry_t*)val)->mask_entry);
    cmd_oam = DRV_IOW(OamDsMpDataMask_t, DRV_ENTRY_FLAG);
    DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, 0, cmd_oam, value));
#endif

    /*write data*/
    value = (uint32 *)(((tbl_entry_t*)val)->data_entry);
    DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, index, cmd, value));

    return DRV_E_NONE;
}

#if(SDK_WORK_PLATFORM == 0)
STATIC int32
_drv_aps_mask_ioctl(uint8 lchip, int32 index, uint32 cmd, void* val)
{
    uint32 cmd_aps = 0;
    DsApsBridge_m data_mask;
    uint32 field_id = 0;
    uint32 mask_value = 0;

    field_id = DRV_IOC_FIELDID(cmd);

    if (DRV_ENTRY_FLAG == field_id)
    {
        /*per entry write, mask should be all 0*/
        sal_memset(&data_mask, 0, sizeof(DsApsBridge_m));
    }
    else
    {
        sal_memset(&data_mask, 0xff, sizeof(DsApsBridge_m));
        DRV_IOW_FIELD(lchip, DsApsBridge_t, field_id, &mask_value, &data_mask);
    }

    if (DRV_IS_DUET2(lchip))
    {
        /*write mask*/
        cmd_aps = DRV_IOW(DsApsBridgeMask_t, DRV_ENTRY_FLAG);
        DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, 0, cmd_aps, &data_mask));

        /*mapping data*/
        cmd = DRV_IOW(DsApsBridgeRtl_t, field_id);

        /*write data*/
        DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, index, cmd, val));
    }
    else
    {
        uint32 field_val = 0;
        /*write mask*/
        cmd_aps = DRV_IOW(MetFifoDsApsMask_t, MetFifoDsApsMask_dsApsMemMask_f);
        DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, 0, cmd_aps, &data_mask));

        field_val = 1;
        cmd_aps = DRV_IOW(MetFifoDsApsMask_t, MetFifoDsApsMask_dsApsMemMaskEn_f);
        DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, 0, cmd_aps, &field_val));

        DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, index, cmd, val));
    }

    return DRV_E_NONE;
}

STATIC int32
drv_ioctl_aps_api(uint8 lchip, int32 index, uint32 cmd, void* val)
{
    uint32 action;
    action = DRV_IOC_OP(cmd);
    if (DRV_IOC_WRITE == action)
    {
        DRV_IF_ERROR_RETURN(_drv_aps_mask_ioctl(lchip, index, cmd, val));
    }
    else
    {
        uint32 cmd_aps = 0;
        uint32 field_id = 0;
        field_id = DRV_IOC_FIELDID(cmd);
        if (DRV_IS_DUET2(lchip))
        {
            cmd_aps = DRV_IOR(DsApsBridgeRtl_t, field_id);
        }
        else
        {
            cmd_aps = DRV_IOR(DsApsBridge_t, field_id);
        }
        DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, index, cmd_aps, val));
    }
    return DRV_E_NONE;
}

STATIC int32
drv_ioctl_drain_dis_api(uint8 lchip, int32 index, uint32 cmd, void* val)
{
    FibAccDrainEnable_m fib_drain_en;
    int32 ret = DRV_E_NONE;
    uint32 cmd_tmp = 0;

    SetFibAccDrainEnable(V, agingReqDrainEnable_f, &fib_drain_en, 0);
    SetFibAccDrainEnable(V, learnReqDrainEnable_f, &fib_drain_en, 0);

    FIB_ACC_LOCK(lchip);
    cmd_tmp = DRV_IOW(FibAccDrainEnable_t, DRV_ENTRY_FLAG);
    _drv_ioctl(lchip, 0, cmd_tmp, &fib_drain_en);

    ret = _drv_ioctl(lchip, index, cmd, val);

    SetFibAccDrainEnable(V, agingReqDrainEnable_f, &fib_drain_en, 1);
    SetFibAccDrainEnable(V, learnReqDrainEnable_f, &fib_drain_en, 1);
    _drv_ioctl(lchip, 0, cmd_tmp, &fib_drain_en);
    FIB_ACC_UNLOCK(lchip);
    return ret;
}

#endif
#define __DRV_API__

/**
  @driver set gchip id for lchip
*/
int32
drv_set_gchip_id(uint8 lchip, uint8 gchip_id)
{
    DRV_INIT_CHECK(lchip);
    p_drv_master[lchip]->g_lchip = gchip_id;

    return DRV_E_NONE;
}

/**
  @driver get gchip id for lchip
*/
int32
drv_get_gchip_id(uint8 lchip, uint8* gchip_id)
{
    DRV_INIT_CHECK(lchip);
    *gchip_id = p_drv_master[lchip]->g_lchip;
    return DRV_E_NONE;
}

/**
  @driver get environmnet type interface
*/
int32
drv_get_platform_type(uint8 lchip, drv_work_platform_type_t *plaform_type)
{
    DRV_INIT_CHECK(lchip);

    if (p_drv_master[lchip])
    {
       *plaform_type = p_drv_master[lchip]->plaform_type;
        return DRV_E_NONE;
    }
    else
    {
        DRV_DBG_INFO("@@@ERROR, Driver is not initialized!\n");
        return  DRV_E_NOT_INIT;
    }
}

/**
  @driver get host type interface
*/
int32
drv_get_host_type (uint8 lchip)
{
    DRV_INIT_CHECK(lchip);

    return p_drv_master[lchip]->host_type;
}

int32 drv_get_field_name_by_bit(uint8 lchip, tbls_id_t tbl_id, uint32 bit_offset, char* field_name)
{
    uint16 loop = 0;
    uint16 loop1 = 0;
    fields_t* p_field = NULL;
    segs_t* p_seg = NULL;
    uint8 words = bit_offset / 32;
    uint8 bits = bit_offset%32;

    DRV_INIT_CHECK(lchip);
    p_field = TABLE_FIELD_INFO_PTR(lchip, tbl_id);
    for(loop=0; loop < TABLE_FIELD_NUM(lchip, tbl_id); loop++)
    {
        p_field = TABLE_FIELD_INFO_PTR(lchip, tbl_id)+loop;
        p_seg = p_field->ptr_seg;
        for(loop1 = 0; loop1 < p_field->seg_num; loop1++)
        {
            p_seg += loop1;
            if((p_seg->word_offset == words) && (bits >= p_seg->start) && (bits < (p_seg->start+p_seg->bits)))
            {
                sal_strcpy(field_name, p_field->ptr_field_name);
                return DRV_E_NONE;
            }
        }
    }

    return DRV_E_NOT_FOUND;
}
/**
  @driver get table property
*/
int32
drv_get_table_property(uint8 lchip, uint8 type, tbls_id_t tbl_id, uint32 index, void* value)
{
    int32 ret = 0;

    if (!value)
    {
        return DRV_E_INVALID_ADDR;
    }

    DRV_TBL_ID_VALID_CHECK(lchip, tbl_id);
    DRV_INIT_CHECK(lchip);

    switch(type)
    {
        case DRV_TABLE_PROP_TYPE:
            *((uint32*)value) = drv_usw_get_table_type(lchip, tbl_id);

            break;
        case DRV_TABLE_PROP_HW_ADDR:
            ret = drv_usw_table_get_hw_addr(lchip, tbl_id, index, (uint32*)value, 0);
            break;

        case DRV_TABLE_PROP_GET_NAME:
            ret = drv_usw_get_tbl_string_by_id(lchip, tbl_id, (char*)value);
            break;

        case DRV_TABLE_PROP_BITMAP:
            *((uint32*)value) = DYNAMIC_BITMAP(lchip, tbl_id);
            break;

        case DRV_TABLE_PROP_FIELD_NUM:
            *((uint32*)value) = TABLE_FIELD_NUM(lchip, tbl_id);
            break;

        default:
            ret = DRV_E_INVALID_TBL;
            break;
    }

    return ret;
}

/**
  @driver set chip access type
*/
int32
drv_set_access_type(uint8 lchip, drv_access_type_t access_type)
{
    DRV_INIT_CHECK(lchip);
    if (access_type == DRV_PCI_ACCESS)
    {
        p_drv_master[lchip]->drv_chip_read = drv_usw_pci_read_chip;
        p_drv_master[lchip]->drv_chip_write = drv_usw_pci_write_chip;
    }
    else if (access_type == DRV_I2C_ACCESS)
    {
        p_drv_master[lchip]->drv_chip_read = drv_usw_i2c_read_chip;
        p_drv_master[lchip]->drv_chip_write = drv_usw_i2c_write_chip;
    }
    p_drv_master[lchip]->access_type = access_type;

    return DRV_E_NONE;
}

/**
  @driver get chip access type
*/
int32
drv_get_access_type(uint8 lchip, drv_access_type_t* p_access_type)
{
    DRV_INIT_CHECK(lchip);
    *p_access_type = p_drv_master[lchip]->access_type;

    return DRV_E_NONE;
}
/**
  @driver install drv io/app interface
*/
int32
drv_install_api(uint8 lchip, drv_io_callback_fun_t* cb)
{

    uint8 chip_num = 0;

    dal_get_chip_number(&chip_num);

    if (lchip >= chip_num)
    {
        return DRV_E_NOT_INIT;
    }

    if (NULL == cb)
    {
        return DRV_E_NOT_INIT;
    }

    p_drv_master[lchip]->drv_io_api.drv_sram_tbl_read = cb->drv_sram_tbl_read;
    p_drv_master[lchip]->drv_io_api.drv_sram_tbl_write = cb->drv_sram_tbl_write;
    p_drv_master[lchip]->drv_io_api.drv_tcam_tbl_read = cb->drv_tcam_tbl_read;
    p_drv_master[lchip]->drv_io_api.drv_tcam_tbl_write = cb->drv_tcam_tbl_write;
    p_drv_master[lchip]->drv_io_api.drv_tcam_tbl_remove = cb->drv_tcam_tbl_remove;

    return DRV_E_NONE;
}


int32  drv_field_support_check(uint8 lchip, tbls_id_t tbl_id, fld_id_t field_id)
{
    fields_t* field = NULL;
    DRV_TBL_ID_VALID_CHECK(lchip, tbl_id);

    field = TABLE_FIELD_INFO_PTR(lchip, tbl_id) + field_id;
    if (field_id >= TABLE_FIELD_NUM(lchip, tbl_id) || 0 == field->bits)
    {
        return DRV_E_INVALID_FLD;
    }

    return DRV_E_NONE;
}

/**
 @brief Get a field of a memory data entry
*/
int32
drv_get_field(uint8 lchip, tbls_id_t tbl_id, fld_id_t field_id,
                void* ds, uint32* value)
{
    fields_t* field = NULL;
    int32 seg_id;
    segs_t* seg = NULL;
    uint8 uint32_idx  = 0;
    uint16 remain_len  = 0;
    uint16 cross_word  = 0;
    uint32 remain_value = 0;
    uint32 seg_value = 0;
    uint32* entry  = NULL;

    field = TABLE_FIELD_INFO_PTR(lchip, tbl_id) + field_id;

    if (field_id >= TABLE_FIELD_NUM(lchip, tbl_id) || 0 == field->bits)
    {
        return DRV_E_NONE;
    }

    entry = ds;
    seg_id = (field->seg_num - 1);

    do
    {
        seg = &(field->ptr_seg[seg_id]);
        seg_value = (entry[seg->word_offset] >> seg->start) & SHIFT_LEFT_MINUS_ONE(seg->bits);

        value[uint32_idx] = (seg_value << remain_len) | remain_value;
        if ((seg->bits + remain_len) == 32)
        {
            remain_value = 0;
            cross_word = 0;
            uint32_idx ++;
        }
        else if ((seg->bits + remain_len) > 32)
        {
            /*create new remain_value */
            remain_value = seg_value >> (32 - remain_len);
            cross_word = 1;
            uint32_idx ++;
        }
        else
        {
            /*create new remain_value */
            remain_value = (seg_value << remain_len) | remain_value;
            cross_word = 0;
        }
        /*create new remain_len */
        remain_len   = (seg->bits + remain_len) & 0x1F; /*rule out bits that exceeds 32*/
        seg_id --;
    }
    while(seg_id>=0);


    if(cross_word) /* last seg cross word */
    {
        value[uint32_idx] = remain_value;
    }

    return DRV_E_NONE;
}

/**
 @brief Set a field of a memory data entry
*/
int32
drv_set_field(uint8 lchip, tbls_id_t tbl_id, fld_id_t field_id,
                        void* ds, uint32 *value)
{
    fields_t* field = NULL;
    segs_t* seg = NULL;
    int32 seg_id = 0;
    uint8 cut_len =  0;
    uint8 array_idx = 0;
    uint32 seg_value = 0;
    uint32 value_a = 0;
    uint32 value_b = 0;
    uint32* entry  = NULL;

    field = TABLE_FIELD_INFO_PTR(lchip, tbl_id) + field_id;

    if (field_id >= TABLE_FIELD_NUM(lchip, tbl_id) || 0 == field->bits)
    {
        return DRV_E_NONE;
    }

    entry = ds;
    seg_id = (field->seg_num-1);

    do
    {
        seg = &(field->ptr_seg[seg_id]);

        if ((cut_len + seg->bits) >= 32)
        {
            value_b = (cut_len + seg->bits) == 32? 0 : (value[array_idx + 1 ] & SHIFT_LEFT_MINUS_ONE((cut_len + seg->bits - 32)));
            value_a = (value[array_idx] >> cut_len) & SHIFT_LEFT_MINUS_ONE((32- cut_len));
            seg_value = (value_b << (32 - cut_len)) | value_a;

            array_idx++;
        }
        else
        {
            value_a = (value[array_idx] >> cut_len) & SHIFT_LEFT_MINUS_ONE(seg->bits);
            seg_value =  value_a;
        }

        entry[seg->word_offset] &= ~ (SHIFT_LEFT_MINUS_ONE(seg->bits)  << seg->start);
        entry[seg->word_offset] |= (seg_value << seg->start);

        cut_len = (cut_len + seg->bits) & 0x1F; /*rule out bits that exceeds 32*/
        seg_id --;
    }
    while(seg_id>=0);

    return DRV_E_NONE;
}

int32
drv_set_field32(uint8 lchip, tbls_id_t tbl_id, fld_id_t field_id, uint32* entry, uint32 value)
{
    fields_t* field = NULL;
    segs_t* seg = NULL;

    field = TABLE_FIELD_INFO_PTR(lchip, tbl_id) + field_id;

    if (field_id >= TABLE_FIELD_NUM(lchip, tbl_id) || 0 == field->bits)
    {
        return DRV_E_NONE;
    }

    if (field->seg_num == 1)
    {
        seg = &(field->ptr_seg[0]);
        entry[seg->word_offset] &= ~(SHIFT_LEFT_MINUS_ONE(seg->bits)  << seg->start);
        entry[seg->word_offset] |= ((value & SHIFT_LEFT_MINUS_ONE(seg->bits)) << seg->start);
    }
    else if(field->seg_num == 2)
    {
        segs_t* seg1 = NULL;

        seg = &(field->ptr_seg[1]);
        entry[seg->word_offset] &= ~(SHIFT_LEFT_MINUS_ONE(seg->bits)  << seg->start);
        entry[seg->word_offset] |= ((value & SHIFT_LEFT_MINUS_ONE(seg->bits)) << seg->start);
        seg1 = &(field->ptr_seg[0]);
        entry[seg1->word_offset] &= ~(SHIFT_LEFT_MINUS_ONE(seg1->bits)  << seg1->start);
        entry[seg1->word_offset] |= (((value>>seg->bits) & SHIFT_LEFT_MINUS_ONE(seg1->bits)) << seg1->start);
    }
    else
    {
        drv_set_field(lchip, tbl_id, field_id, entry, &value);
    }


    return DRV_E_NONE;
}



uint32
drv_get_field32(uint8 lchip, tbls_id_t tbl_id, fld_id_t field_id, void* entry)
{
    fields_t* field = NULL;
    segs_t* seg = NULL;
    uint32 value = 0;

    field = TABLE_FIELD_INFO_PTR(lchip, tbl_id) + field_id;

    if (field_id >= TABLE_FIELD_NUM(lchip, tbl_id) || 0 == field->bits)
    {
        return DRV_E_NONE;
    }

    if (field->seg_num == 1)
    {
        seg = &(field->ptr_seg[0]);
        value = (((uint32*)entry)[seg->word_offset]>>seg->start) & SHIFT_LEFT_MINUS_ONE(seg->bits);
    }
    else if(field->seg_num == 2)
    {
        segs_t* seg1 = NULL;
        seg = &(field->ptr_seg[1]);
        value = (((uint32*)entry)[seg->word_offset]>>seg->start) & SHIFT_LEFT_MINUS_ONE(seg->bits);
        seg1 = &(field->ptr_seg[0]);
        value |= ((((uint32*)entry)[seg1->word_offset]>>seg1->start) & SHIFT_LEFT_MINUS_ONE(seg1->bits) )<< seg->bits;
    }
    else
    {
         drv_get_field(lchip, tbl_id, field_id, entry, &value);
    }

    return value;
}

uint32
drv_get_chip_type(uint8 lchip)
{
    DRV_INIT_CHECK(lchip);
    return p_drv_master[lchip]->dev_type;
}


STATIC int32
drv_ioctl_oam_api(uint8 lchip, int32 index, uint32 cmd, void* val)
{
    uint32 action;
    action = DRV_IOC_OP(cmd);

    if (DRV_IOC_WRITE == action)
    {
        DRV_IF_ERROR_RETURN(_drv_oam_mask_ioctl(lchip, index, cmd, val));
    }
    else
    {
        DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, index, cmd, val));
    }
    return DRV_E_NONE;
}


/**
  @driver Driver IO Api
*/
int32
drv_ioctl_api(uint8 lchip, int32 index, uint32 cmd, void* val)
{
    tbls_id_t tbl_id;
    uint32 action;
    action = DRV_IOC_OP(cmd);
    tbl_id = DRV_IOC_MEMID(cmd);

    DRV_INIT_CHECK(lchip);

    if (0 == TABLE_FIELD_NUM(lchip, tbl_id) || TABLE_MAX_INDEX(lchip, tbl_id) == 0)
    {
          return 0;
    }

#ifdef EMULATION_ENV
uint16 i = 0;
for (i = 0; ; i++)
{
    if (g_drv_pcs[i] == 0xffff)
    {
        break;
    }

    if (g_drv_pcs[i] == tbl_id)
    {
        return DRV_E_NONE;
    }
}
#endif

    if(action == DRV_IOC_WRITE && (p_drv_master[lchip]->wb_status == DRV_WB_STATUS_RELOADING))
    {
        return DRV_E_NONE;
    }

    DRV_IF_ERROR_RETURN(p_drv_master[lchip]->drv_ioctl_cb[TABLE_IOCTL_TYPE(lchip, tbl_id)](lchip, index, cmd, val));

    return DRV_E_NONE;
}

int32
drv_mcu_ioctl_api(uint8 lchip, int32 index, uint32 cmd, void* val)
{
#if(SDK_WORK_PLATFORM == 0)
    tbls_id_t tbl_id;
    uint32 action;
    action = DRV_IOC_OP(cmd);
    tbl_id = DRV_IOC_MEMID(cmd);
#endif

    DRV_INIT_CHECK(lchip);

#if(SDK_WORK_PLATFORM == 0)
    if (DRV_IOC_WRITE == action)
    {
        DRV_IF_ERROR_RETURN(drv_usw_mcu_tbl_lock(lchip, tbl_id, action));
    }
#endif
        DRV_IF_ERROR_RETURN(_drv_ioctl(lchip, index, cmd, val));
#if(SDK_WORK_PLATFORM == 0)
    if (DRV_IOC_WRITE == action)
    {
        DRV_IF_ERROR_RETURN(drv_usw_mcu_tbl_unlock(lchip, tbl_id, action));
    }
#endif
    return DRV_E_NONE;
}

/**
  @driver Driver Acc Api
*/
int32
drv_acc_api(uint8 lchip, void* in, void* out)
{
    DRV_INIT_CHECK(lchip);

    DRV_IF_ERROR_RETURN(drv_usw_acc(lchip, in, out));

    return DRV_E_NONE;
}

/**
  @driver Driver Set warmboot status
*/
int32
drv_set_warmboot_status(uint8 lchip, uint32 wb_status)
{
    DRV_INIT_CHECK(lchip);

    p_drv_master[lchip]->wb_status = wb_status;

    return DRV_E_NONE;
}


#ifdef DUET2
extern int32
drv_tbls_list_init_duet2(uint8 lchip);
int32
drv_tbls_list_deinit_duet2(uint8 lchip);
#endif

#ifdef TSINGMA
extern int32
drv_tbls_list_init_tsingma(uint8 lchip);
int32
drv_tbls_list_deinit_tsingma(uint8 lchip);
#endif


int32
drv_tbls_info_init(uint8 lchip)
{
    uint32 dev_id = 0;

    dal_get_chip_dev_id(0, &dev_id);

#ifdef DUET2
    if (dev_id == DAL_DUET2_DEVICE_ID)
    {
        drv_tbls_list_init_duet2(lchip);
    }
#endif


#ifdef TSINGMA
    if (dev_id == DAL_TSINGMA_DEVICE_ID)
    {
        drv_tbls_list_init_tsingma(lchip);
    }
#endif

    return 0;
}

int32
drv_tbls_info_deinit(uint8 lchip)
{

#ifdef DUET2
    if (DRV_IS_DUET2(lchip))
    {
        drv_tbls_list_deinit_duet2(lchip);
    }
#endif


#ifdef TSINGMA

    if (DRV_IS_TSINGMA(lchip))
    {
        drv_tbls_list_deinit_tsingma(lchip);
    }
#endif

    return 0;
}

/**
  @driver Driver Init Api,per chip init TBD
*/
int32
drv_init(uint8 lchip, uint8 base)
{
    uint8 chip_num = 0;
    uint8 cnt = 0;
    uint32 dev_id = 0;

    dal_get_chip_number(&chip_num);
    if(p_drv_master[lchip] == NULL)
    {
        uint16 lchip_idx = 0;
        for(lchip_idx=0; lchip_idx < DRV_MAX_CHIP_NUM; lchip_idx++)
        {
            p_drv_master[lchip_idx] = &(g_drv_master[lchip_idx]);
        }
    }
    sal_memset(p_drv_master[lchip], 0, sizeof(drv_master_t));

    p_drv_master[lchip]->plaform_type = SDK_WORK_PLATFORM;
    p_drv_master[lchip]->workenv_type = SDK_WORK_ENV;
    p_drv_master[lchip]->access_type = DRV_PCI_ACCESS;
    if (p_drv_master[lchip]->access_type == DRV_PCI_ACCESS)
    {
        p_drv_master[lchip]->drv_chip_read = drv_usw_pci_read_chip;
        p_drv_master[lchip]->drv_chip_write = drv_usw_pci_write_chip;
    }
    else if (p_drv_master[lchip]->access_type == DRV_I2C_ACCESS)
    {
        p_drv_master[lchip]->drv_chip_read = drv_usw_i2c_read_chip;
        p_drv_master[lchip]->drv_chip_write = drv_usw_i2c_write_chip;
    }

    drv_tbls_info_init(lchip);

    _drv_get_host_type(lchip);

    if (0 == SDK_WORK_PLATFORM)
    {
        drv_chip_common_init(lchip);
    }
    else
    {
        drv_model_common_init(lchip);
    }
    dal_get_chip_dev_id(lchip, &(dev_id));
    if (dev_id == DAL_DUET2_DEVICE_ID)
    {
        p_drv_master[lchip]->dev_type = DRV_DUET2;
    }
    if (dev_id == DAL_TSINGMA_DEVICE_ID)
    {
        p_drv_master[lchip]->dev_type = DRV_TSINGMA;
    }

    TABLE_IOCTL_TYPE(lchip, DsEthMep_t) = DRV_TBL_TYPE_OAM;
    TABLE_IOCTL_TYPE(lchip, DsEthRmep_t) = DRV_TBL_TYPE_OAM;
    TABLE_IOCTL_TYPE(lchip, DsBfdMep_t) = DRV_TBL_TYPE_OAM;
    TABLE_IOCTL_TYPE(lchip, DsBfdRmep_t) = DRV_TBL_TYPE_OAM;
    TABLE_IOCTL_TYPE(lchip, SharedPcsSoftRst0_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, SharedPcsSoftRst1_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst0_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst1_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst2_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst3_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst4_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst5_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst6_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst7_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst0_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst1_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst2_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst3_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst4_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst5_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst6_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst7_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, SharedPcsSoftRst2_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, SharedPcsSoftRst3_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst8_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst9_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst10_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, QsgmiiPcsSoftRst11_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst8_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst9_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst10_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst11_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, SharedPcsSoftRst4_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, SharedPcsSoftRst5_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst6_t) = DRV_TBL_TYPE_MCU;
    TABLE_IOCTL_TYPE(lchip, UsxgmiiPcsSoftRst7_t) = DRV_TBL_TYPE_MCU;
#if (SDK_WORK_PLATFORM == 0)
    TABLE_IOCTL_TYPE(lchip, DsApsBridge_t) = DRV_TBL_TYPE_APS_BRIDGE;
    if (dev_id == DAL_DUET2_DEVICE_ID)
    {
        TABLE_IOCTL_TYPE(lchip, DsMac_t) = DRV_TBL_TYPE_MAC;
    }
    else
    {
        TABLE_IOCTL_TYPE(lchip, DsMac_t) = DRV_TBL_TYPE_NORMAL;
    }
    if (dev_id == DAL_TSINGMA_DEVICE_ID)
    {
        TABLE_IOCTL_TYPE(lchip, DsFibHost0FcoeHashKey_t) = DRV_TBL_TYPE_MAC;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv4HashKey_t) = DRV_TBL_TYPE_MAC;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv6McastHashKey_t) = DRV_TBL_TYPE_MAC;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv6UcastHashKey_t) = DRV_TBL_TYPE_MAC;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0MacIpv6McastHashKey_t) = DRV_TBL_TYPE_MAC;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0TrillHashKey_t) = DRV_TBL_TYPE_MAC;
    }
    else
    {
        TABLE_IOCTL_TYPE(lchip, DsFibHost0FcoeHashKey_t) = DRV_TBL_TYPE_NORMAL;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv4HashKey_t) = DRV_TBL_TYPE_NORMAL;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv6McastHashKey_t) = DRV_TBL_TYPE_NORMAL;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv6UcastHashKey_t) = DRV_TBL_TYPE_NORMAL;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0MacIpv6McastHashKey_t) = DRV_TBL_TYPE_NORMAL;
        TABLE_IOCTL_TYPE(lchip, DsFibHost0TrillHashKey_t) = DRV_TBL_TYPE_NORMAL;
    }
#else
    TABLE_IOCTL_TYPE(lchip, DsApsBridge_t) = DRV_TBL_TYPE_NORMAL;
    TABLE_IOCTL_TYPE(lchip, DsMac_t) = DRV_TBL_TYPE_NORMAL;
    TABLE_IOCTL_TYPE(lchip, DsFibHost0FcoeHashKey_t) = DRV_TBL_TYPE_NORMAL;
    TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv4HashKey_t) = DRV_TBL_TYPE_NORMAL;
    TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv6McastHashKey_t) = DRV_TBL_TYPE_NORMAL;
    TABLE_IOCTL_TYPE(lchip, DsFibHost0Ipv6UcastHashKey_t) = DRV_TBL_TYPE_NORMAL;
    TABLE_IOCTL_TYPE(lchip, DsFibHost0MacIpv6McastHashKey_t) = DRV_TBL_TYPE_NORMAL;
    TABLE_IOCTL_TYPE(lchip, DsFibHost0TrillHashKey_t) = DRV_TBL_TYPE_NORMAL;
#endif

    for (cnt = 0; cnt < DRV_TBL_TYPE_MAX; cnt++)
    {
        p_drv_master[lchip]->drv_ioctl_cb[cnt] = NULL;
    }
    p_drv_master[lchip]->drv_ioctl_cb[DRV_TBL_TYPE_NORMAL] = _drv_ioctl;
    p_drv_master[lchip]->drv_ioctl_cb[DRV_TBL_TYPE_OAM] = drv_ioctl_oam_api;
#if (SDK_WORK_PLATFORM == 0)
    p_drv_master[lchip]->drv_ioctl_cb[DRV_TBL_TYPE_APS_BRIDGE] = drv_ioctl_aps_api;
    p_drv_master[lchip]->drv_ioctl_cb[DRV_TBL_TYPE_MAC] = drv_ioctl_drain_dis_api;
#endif
    p_drv_master[lchip]->drv_ioctl_cb[DRV_TBL_TYPE_MCU] = drv_mcu_ioctl_api;

    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_FDB] = drv_usw_acc_fdb_cb;
    if (dev_id == DAL_DUET2_DEVICE_ID)
    {
        p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_FIB_HOST0] = drv_usw_acc_host0;
    }
    else if (dev_id == DAL_TSINGMA_DEVICE_ID)
    {
        p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_FIB_HOST0] = drv_usw_acc_hash;
    }
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_FIB_HOST1] = drv_usw_acc_hash;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_EGRESS_XC] = drv_usw_acc_hash;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_FLOW] = drv_usw_acc_hash;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_USERID] = drv_usw_acc_hash;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_MAC_LIMIT] = drv_usw_acc_mac_limit;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_IPFIX] = drv_usw_acc_ipfix;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_CID] = drv_usw_acc_cid;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_AGING] = drv_usw_acc_aging;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_MPLS_HASH] = drv_usw_acc_mpls;
    p_drv_master[lchip]->drv_acc_cb[DRV_ACC_HASH_MODULE_GEMPORT_HASH] = drv_usw_acc_gemport;

    p_drv_master[lchip]->init_done = 1;


    return DRV_E_NONE;
}

int32
drv_deinit(uint8 lchip, uint8 base)
{
    drv_ser_deinit(lchip);
    drv_tbls_info_deinit(lchip);

    p_drv_master[lchip]->p_flow_tcam_mutex ? sal_mutex_destroy(p_drv_master[lchip]->p_flow_tcam_mutex):0;
    p_drv_master[lchip]->p_lpm_ip_tcam_mutex ? sal_mutex_destroy(p_drv_master[lchip]->p_lpm_ip_tcam_mutex):0;
    p_drv_master[lchip]->p_lpm_nat_tcam_mutex ? sal_mutex_destroy(p_drv_master[lchip]->p_lpm_nat_tcam_mutex):0;
    p_drv_master[lchip]->p_entry_mutex ? sal_mutex_destroy(p_drv_master[lchip]->p_entry_mutex):0;
    p_drv_master[lchip]->fib_acc_mutex ? sal_mutex_destroy(p_drv_master[lchip]->fib_acc_mutex):0;
    p_drv_master[lchip]->cpu_acc_mutex ? sal_mutex_destroy(p_drv_master[lchip]->cpu_acc_mutex):0;
    p_drv_master[lchip]->ipfix_acc_mutex ? sal_mutex_destroy(p_drv_master[lchip]->ipfix_acc_mutex):0;
    p_drv_master[lchip]->cid_acc_mutex ? sal_mutex_destroy(p_drv_master[lchip]->cid_acc_mutex):0;
#ifndef PACKET_TX_USE_SPINLOCK
    p_drv_master[lchip]->p_pci_mutex ? sal_mutex_destroy(p_drv_master[lchip]->p_pci_mutex):0;
    p_drv_master[lchip]->p_i2c_mutex ? sal_mutex_destroy(p_drv_master[lchip]->p_i2c_mutex):0;
#else
    p_drv_master[lchip]->p_pci_mutex ? sal_spinlock_destroy((sal_spinlock_t*)p_drv_master[lchip]->p_pci_mutex):0;
    p_drv_master[lchip]->p_i2c_mutex ? sal_spinlock_destroy((sal_spinlock_t*)p_drv_master[lchip]->p_i2c_mutex):0;
#endif
    p_drv_master[lchip]->p_hss_mutex ? sal_mutex_destroy(p_drv_master[lchip]->p_hss_mutex):0;
    p_drv_master[lchip]->mpls_acc_mutex ? sal_mutex_destroy(p_drv_master[lchip]->mpls_acc_mutex):0;
    p_drv_master[lchip]->gemport_acc_mutex ? sal_mutex_destroy(p_drv_master[lchip]->gemport_acc_mutex):0;
    p_drv_master[lchip]->p_mep_mutex ? sal_mutex_destroy(p_drv_master[lchip]->p_mep_mutex):0;
    sal_memset(&(g_drv_master[lchip]), 0, sizeof(drv_master_t));
    return 0;
}
