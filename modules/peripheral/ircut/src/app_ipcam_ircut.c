#include <stdio.h>
#include <stdlib.h>
#include "cvi_type.h"
#include "cvi_isp.h"
#include "cvi_comm_isp.h"
#include "cvi_ae.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_vi.h"

#include "app_ipcam_ircut.h"

#define IR_CUT_SWITCH_TO_NORMAL 0
#define IR_CUT_SWITCH_TO_IR     1
#define IR_CUT_CLOSE            0
#define IR_CUT_OPEN             1
#define ENTER_NIGHT_LV_LEVEL    400
#define ENTER_DAY_LV_LEVEL      600
#define CHECK_COUNT             5

typedef struct APP_PARAM_IRCUT_STATUS_T {
    CVI_BOOL bAutoCtrl;
    CVI_BOOL bManualCtrl;
} APP_PARAM_IRCUT_STATUS_S;

typedef struct APP_PARAM_IRCUT_CFG_T {
    CVI_BOOL bInit;
    CVI_BOOL bDayMode;
    CVI_BOOL bNightMode;
    CVI_CHAR sDayBinPath[APP_IPCAM_MAX_STR_LEN];
    CVI_CHAR sNightBinPath[APP_IPCAM_MAX_STR_LEN];
} APP_PARAM_IRCUT_CFG_S;

static APP_PARAM_IRCUT_CFG_S g_stIrCutCfg = {
    .sDayBinPath = "/mnt/cfg/param/cvi_sdr_bin",
    .sNightBinPath = "/mnt/cfg/param/cvi_sdr_ir_bin",
};

APP_PARAM_IRCUT_CFG_S *g_pstIrCutCfg = &g_stIrCutCfg;
static pthread_t g_pthIRCut;
static CVI_BOOL g_bIRCut_Running;
static APP_PARAM_IRCUT_STATUS_S g_stIRCutCtrl = {0};
static CVI_GPIO_NUM_E IR_CUT_A, IR_CUT_B, IR_LED;

int app_ipcam_IRCut_Switch(CVI_GPIO_NUM_E IR_CUT_A, CVI_GPIO_NUM_E IR_CUT_B, CVI_BOOL bEnable)
{
    if (!g_pstIrCutCfg->bInit) {
        app_ipcam_Gpio_Export(IR_CUT_A);
        app_ipcam_Gpio_Dir_Set(IR_CUT_A, CVI_GPIO_DIR_OUT);

        app_ipcam_Gpio_Export(IR_CUT_B);
        app_ipcam_Gpio_Dir_Set(IR_CUT_B, CVI_GPIO_DIR_OUT);

        g_pstIrCutCfg->bInit = CVI_TRUE;
    }

    if (bEnable) {
        app_ipcam_Gpio_Value_Set(IR_CUT_A, CVI_GPIO_VALUE_H);
        app_ipcam_Gpio_Value_Set(IR_CUT_B, CVI_GPIO_VALUE_L);

        usleep(500*1000);
        app_ipcam_Gpio_Value_Set(IR_CUT_A, CVI_GPIO_VALUE_L);
    } else {
        app_ipcam_Gpio_Value_Set(IR_CUT_A, CVI_GPIO_VALUE_L);
        app_ipcam_Gpio_Value_Set(IR_CUT_B, CVI_GPIO_VALUE_H);

        usleep(500*1000);
        app_ipcam_Gpio_Value_Set(IR_CUT_B, CVI_GPIO_VALUE_L);
    }

    return CVI_SUCCESS;
}
#if 0
static CVI_S32 app_ipcam_IRCutMode_Switch(int mode, ISP_IR_AUTO_ATTR_S *pstIrAttr)
{
    if (mode == IR_CUT_SWITCH_TO_NORMAL) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "IRCut switch to RGB Mode\n");
        app_ipcam_PQBin_Load(PQ_BIN_SDR);
        #ifdef USING_VPSS_ADJUSTMENT
        CVI_VPSS_SetGrpParamfromBin(0, 0);//grop0 load scene0
        #endif
        /* close ir-cut */
        app_ipcam_IRCut_Switch(IR_CUT_A, IR_CUT_B, IR_CUT_CLOSE);
        /* close ir-led */
        app_ipcam_Gpio_Value_Set(IR_LED, CVI_GPIO_VALUE_L);
        /* update ir-status */
        pstIrAttr->enIrStatus = ISP_IR_STATUS_NORMAL;
    } else if (mode == IR_CUT_SWITCH_TO_IR) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "IRCut switch to IR Mode\n");
        /* open ir-cut */
        app_ipcam_IRCut_Switch(IR_CUT_A, IR_CUT_B, IR_CUT_OPEN);
        /* open ir-led */
        app_ipcam_Gpio_Value_Set(IR_LED, CVI_GPIO_VALUE_H);
        /* update ir-status */
        pstIrAttr->enIrStatus = ISP_IR_STATUS_IR;
        app_ipcam_PQBin_Load(PQ_BIN_IR);
        #ifdef USING_VPSS_ADJUSTMENT
        CVI_VPSS_SetGrpParamfromBin(0, 0);//grop0 load scene0
        #endif
    }

    return CVI_SUCCESS;
}

#endif

/*
 * @brief select ir-cut mode between auto and manual
 * value = CVI_TRUE; AutoCtl
 * value = CVI_FALSE; bManualCtrl
 */
void app_ipcam_IRCutMode_Select(CVI_S32 value)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "IRCut auto control (%d)\n", value);
    g_stIRCutCtrl.bAutoCtrl = value;
    if (g_stIRCutCtrl.bAutoCtrl == CVI_TRUE) {
        g_stIRCutCtrl.bManualCtrl = CVI_FALSE;
    }
}

void app_ipcam_IRCutMode_ManualCtrl(CVI_S32 value, CVI_S32 state)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "IRCut Manual control (%d) and state (%d)\n", value, state);

    g_stIRCutCtrl.bManualCtrl = value;

    if (g_stIRCutCtrl.bManualCtrl == CVI_TRUE) {
        g_stIRCutCtrl.bAutoCtrl = CVI_FALSE;
        if (state == IR_CUT_SWITCH_TO_NORMAL) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "IRCut Manual control and switch to normal mode\n");
            /* open ir filter*/
            app_ipcam_IRCut_Switch(IR_CUT_A, IR_CUT_B, IR_CUT_OPEN);
            /* close ir-led */
            app_ipcam_Gpio_Value_Set(IR_LED, CVI_GPIO_VALUE_L);
            sleep(1);
            /* load PQ parameter */
            app_ipcam_PQBin_Load(PQ_BIN_SDR);
            #ifdef USING_VPSS_ADJUSTMENT
            CVI_VPSS_SetGrpParamfromBin(0, 0);//grop0 load scene0
            #endif
            g_pstIrCutCfg->bDayMode = CVI_TRUE;
            g_pstIrCutCfg->bNightMode = CVI_FALSE;
        } if (state == IR_CUT_SWITCH_TO_IR) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "IRCut Manual control and switch to IR mode\n");
            /* load PQ parameter */
            app_ipcam_PQBin_Load(PQ_BIN_IR);
            #ifdef USING_VPSS_ADJUSTMENT
            CVI_VPSS_SetGrpParamfromBin(0, 0);//grop0 load scene0
            #endif
            sleep(2);
            /* close ir filter*/
            app_ipcam_IRCut_Switch(IR_CUT_A, IR_CUT_B, IR_CUT_CLOSE);
            /* open ir-led */
            app_ipcam_Gpio_Value_Set(IR_LED, CVI_GPIO_VALUE_H);
            g_pstIrCutCfg->bDayMode = CVI_FALSE;
            g_pstIrCutCfg->bNightMode = CVI_TRUE;
        }
    }
}

static CVI_VOID *ThreadIRCutAutoSwitch(void *arg)
{
    CVI_S16 lv;
    CVI_U8 checkDayCount = 0;
    CVI_U8 checkNightCount = 0;
    
    while(g_bIRCut_Running) {
        if (g_stIRCutCtrl.bAutoCtrl == CVI_TRUE) {
            CVI_ISP_GetCurrentLvX100(0, &lv);
            APP_PROF_LOG_PRINT(LEVEL_DEBUG, "IRCut Auto control lv is %d, Day_LV=%d Night_LV=%d\n", lv, ENTER_DAY_LV_LEVEL, ENTER_NIGHT_LV_LEVEL);
            if (lv > ENTER_DAY_LV_LEVEL) {
                if (checkDayCount < CHECK_COUNT) {
                    checkDayCount++;
                } else if(!g_pstIrCutCfg->bDayMode) {
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "IRCut Manual control and switch to normal mode\n");
                    /* open ir filter*/
                    app_ipcam_IRCut_Switch(IR_CUT_A, IR_CUT_B, IR_CUT_OPEN);
                    /* close ir-led */
                    app_ipcam_Gpio_Value_Set(IR_LED, CVI_GPIO_VALUE_L);
                    /* load PQ parameter */
                    app_ipcam_PQBin_Load(PQ_BIN_SDR);
                    #ifdef USING_VPSS_ADJUSTMENT
                    CVI_VPSS_SetGrpParamfromBin(0, 0);//grop0 load scene0
                    #endif
                    g_pstIrCutCfg->bDayMode = CVI_TRUE;
                    g_pstIrCutCfg->bNightMode = CVI_FALSE;
                    checkDayCount = 0;
                }
            } else if (lv < ENTER_NIGHT_LV_LEVEL) {
                if (checkNightCount < CHECK_COUNT) {
                    checkNightCount++;
                } else if(!g_pstIrCutCfg->bNightMode) {
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "IRCut Manual control and switch to IR mode\n");
                    /* load PQ parameter */
                    app_ipcam_PQBin_Load(PQ_BIN_IR);
                    #ifdef USING_VPSS_ADJUSTMENT
                    CVI_VPSS_SetGrpParamfromBin(0, 0);//grop0 load scene0
                    #endif
                    sleep(1);
                    /* close ir filter*/
                    app_ipcam_IRCut_Switch(IR_CUT_A, IR_CUT_B, IR_CUT_CLOSE);
                    /* open ir-led */
                    app_ipcam_Gpio_Value_Set(IR_LED, CVI_GPIO_VALUE_H);
                    g_pstIrCutCfg->bDayMode = CVI_FALSE;
                    g_pstIrCutCfg->bNightMode = CVI_TRUE;
                    checkNightCount = 0;
                }
            } else {
                checkDayCount = 0;
                checkNightCount = 0;
            }
        }
        sleep(1);
	}
    /*
    ISP_DEV	IspDev = 0;
    ISP_IR_AUTO_ATTR_S stIrAttr;
    stIrAttr.enIrStatus = ISP_IR_STATUS_NORMAL;
    stIrAttr.bEnable = 1;
    stIrAttr.u32Normal2IrIsoThr = 1400;//黑暗环境临界值
    stIrAttr.u32Ir2NormalIsoThr = 300;//正常环境临界值
    stIrAttr.u32RGMin = 150;
    stIrAttr.u32RGMax = 170;
    stIrAttr.u32BGMin = 155;
    stIrAttr.u32BGMax = 170;

    while (g_bIRCut_Running) {
        if (g_stIRCutCtrl.bAutoCtrl) {
            CVI_ISP_IrAutoRunOnce(IspDev, &stIrAttr);
            if ((stIrAttr.enIrSwitch == ISP_IR_SWITCH_TO_NORMAL) && (!g_pstIrCutCfg->bDayMode)) {
                app_ipcam_IRCutMode_Switch(IR_CUT_SWITCH_TO_NORMAL, &stIrAttr);
                g_pstIrCutCfg->bDayMode = CVI_TRUE;
                g_pstIrCutCfg->bNightMode = CVI_FALSE;
            } else if ((stIrAttr.enIrSwitch == ISP_IR_SWITCH_TO_IR) && (!g_pstIrCutCfg->bNightMode)) {
                app_ipcam_IRCutMode_Switch(IR_CUT_SWITCH_TO_IR, &stIrAttr);
                g_pstIrCutCfg->bDayMode = CVI_FALSE;
                g_pstIrCutCfg->bNightMode = CVI_TRUE;
            }
        }
        sleep(1);
    }
    */

    return NULL;
}

int app_ipcam_IRCut_Init(void)
{
    int s32Ret = CVI_SUCCESS;
    APP_PARAM_GPIO_CFG_S *psGpioCfg;

    psGpioCfg = app_ipcam_Gpio_Param_Get();
    IR_CUT_A = psGpioCfg->IR_CUT_A;
    IR_CUT_B = psGpioCfg->IR_CUT_B;
    IR_LED   = psGpioCfg->LED_IR;

    APP_PROF_LOG_PRINT(LEVEL_INFO, "gpio num ir_a=%d ir_b=%d ir_led=%d\n", IR_CUT_A, IR_CUT_B, IR_LED);

    app_ipcam_IRCut_Switch(IR_CUT_A, IR_CUT_B, IR_CUT_OPEN);

    g_bIRCut_Running = CVI_TRUE;
    s32Ret = pthread_create(&g_pthIRCut, NULL, ThreadIRCutAutoSwitch, NULL);
    if (s32Ret != 0) {
        printf("pthread_create failed!\n");
        return s32Ret;
    }

    return CVI_SUCCESS;
}

int app_ipcam_IrCut_DeInit(void)
{
    g_bIRCut_Running = CVI_FAILURE;

    if (g_pthIRCut) {
        pthread_cancel(g_pthIRCut);
        pthread_join(g_pthIRCut, NULL);
        g_pthIRCut = 0;
    }

    app_ipcam_Gpio_Value_Set(IR_CUT_A, CVI_GPIO_VALUE_L);
    app_ipcam_Gpio_Value_Set(IR_CUT_B, CVI_GPIO_VALUE_L);
    app_ipcam_Gpio_Value_Set(IR_LED, CVI_GPIO_VALUE_L);
    app_ipcam_Gpio_Unexport(IR_CUT_A);
    app_ipcam_Gpio_Unexport(IR_CUT_B);
    app_ipcam_Gpio_Unexport(IR_LED);

    g_pstIrCutCfg->bInit = CVI_FALSE;

    printf("state detect exit\n");

    return CVI_SUCCESS;
}
