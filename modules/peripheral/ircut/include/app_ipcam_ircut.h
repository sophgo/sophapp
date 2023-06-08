#ifndef __APP_IPCAM_IRCUT_H__
#define __APP_IPCAM_IRCUT_H__

#include "app_ipcam_gpio.h"

#ifdef __cplusplus
extern "C"
{
#endif

int app_ipcam_IRCut_Init(void);
int app_ipcam_IrCut_DeInit(void);
void app_ipcam_IRCutMode_Select(CVI_S32 value);
int app_ipcam_IRCut_Switch(CVI_GPIO_NUM_E IR_CUT_A, CVI_GPIO_NUM_E IR_CUT_B, CVI_BOOL bEnable);
void app_ipcam_IRCutMode_ManualCtrl(CVI_S32 value, CVI_S32 state);

#ifdef __cplusplus
}
#endif

#endif
