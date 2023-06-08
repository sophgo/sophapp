#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "cvi_type.h"
#include "app_ipcam_adc.h"
#include "app_ipcam_comm.h"

/* reference voltage is 1.5V */
#define REFERENCE_VOLTAGE	((float)1500)
#define ADC_RESOLUTION		(4096)
#define STEP_VOLTAGE		(REFERENCE_VOLTAGE/ADC_RESOLUTION)

#define SYSFS_ADC_NODE "/sys/class/cvi-saradc/cvi-saradc0/device/cv_saradc"

static int fd_adc = -1;

int app_ipcam_Adc_Init(char *node, char channel)
{
    APP_PROF_LOG_PRINT(LEVEL_INFO, "Init adc channel: %s %d failed!\n", node, channel);
    
    fd_adc = open(node, O_RDWR);
    if (fd_adc < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "open adc node: %s failed!\n", node);
        return CVI_FAILURE;
    }

    ssize_t w_cnt = 0;
    w_cnt = write(fd_adc, &channel, 1);
    if (w_cnt == -1) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "write adc channel: %s %d failed!\n", node, channel);
    }

    close(fd_adc);

    return CVI_SUCCESS;
}

int app_ipcam_AdcVal_Get(const char *node)
{
    fd_adc = open(node, O_RDWR);
    if (fd_adc < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "open adc node: %s failed!\n", node);
        return CVI_FAILURE;
    }

    ssize_t len = 0;
    char buf[5] = {0};
    len = read(fd_adc, buf, sizeof(buf));
    buf[len] = 0;

    APP_PROF_LOG_PRINT(LEVEL_TRACE, "get value(string) form driver: %s\n", buf);

    int val_pre = 0;
    val_pre = atoi(buf);

    close(fd_adc);

    int val = 0;
    val = (int)(val_pre * STEP_VOLTAGE);

    APP_PROF_LOG_PRINT(LEVEL_INFO, "Current voltage=%dmV\n", val);

    return val;
}

/* adc usage sample*/
/**********************************************************************************
 *  step 1: app_ipcam_Adc_Init(SYSFS_ADC_NODE, channel_id);  // channel id = 1,2,3
 * 
 *  step 2: app_ipcam_AdcVal_Get(SYSFS_ADC_NODE);   // return current voltage
 * *******************************************************************************/
