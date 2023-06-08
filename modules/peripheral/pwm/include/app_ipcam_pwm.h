#ifndef __APP_IPCAM_PWM_H__
#define __APP_IPCAM_PWM_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

int app_ipcam_Pwm_Export(int grp, int chn);
int app_ipcam_Pwm_UnExport(int grp, int chn);
int app_ipcam_Pwm_Param_Set(int grp, int chn, int period, int duty_cycle);
int app_ipcam_Pwm_Enable(int grp, int chn);
int app_ipcam_Pwm_Disable(int grp, int chn);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif