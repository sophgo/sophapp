#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <stdbool.h>

#include "cvi_type.h"
#include "app_ipcam_pwm.h"
#include "app_ipcam_comm.h"

#define MAX_BUF 64

#define SYSFS_PWM_DIR "/sys/class/pwm/pwmchip"

#define PWM_PORT_CHECK(GRP, CHN)    do {                                                        \
    if ((GRP != 0) && (GRP != 4) && (GRP != 8) && (GRP != 12)) {                                \
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM group invalid and Optional ID is [0/4/8/12]\n");   \
        return -1;                                                                              \
    }                                                                                           \
    if (!((CHN >= 0) && (CHN <= 3))) {                                                          \
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM channel invalid and Optional ID is [0~3]\n");      \
        return -1;                                                                              \
    }                                                                                           \
} while(0)

int app_ipcam_Pwm_Export(int grp, int chn)
{
    int fd;
    char buf[MAX_BUF];
    ssize_t w_cnt = 0;

    PWM_PORT_CHECK(grp, chn);

    snprintf(buf, sizeof(buf), SYSFS_PWM_DIR"%d/pwm%d", grp, chn);

    if ((access(buf, F_OK)) == -1) {
        snprintf(buf, sizeof(buf), SYSFS_PWM_DIR"%d/export", grp);
        fd = open(buf, O_WRONLY);
        if (fd < 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM port export:%s failed\n", buf);
            return -1;
        }
        if (chn == 0)
            w_cnt = write(fd, "0", strlen("0"));
        else if (chn == 1)
            w_cnt = write(fd, "1", strlen("1"));
        else if (chn == 2)
            w_cnt = write(fd, "2", strlen("2"));
        else if (chn == 3)
            w_cnt = write(fd, "3", strlen("3"));

        if (w_cnt == -1) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM port write:%s failed\n", buf);
        }
        close(fd);
        return -1;
    }

    return 0;
}

int app_ipcam_Pwm_UnExport(int grp, int chn)
{
    int fd;
    char buf[MAX_BUF];
    ssize_t w_cnt = 0;

    PWM_PORT_CHECK(grp, chn);

    snprintf(buf, sizeof(buf), SYSFS_PWM_DIR"%d/pwm%d", grp, chn);

    if ((access(buf, F_OK)) != -1) {
        snprintf(buf, sizeof(buf), SYSFS_PWM_DIR"%d/unexport", grp);
        fd = open(buf, O_WRONLY);
        if (fd < 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM port unexport:%s failed\n", buf);
            return -1;
        }
        if (chn == 0)
            w_cnt = write(fd, "0", strlen("0"));
        else if (chn == 1)
            w_cnt = write(fd, "1", strlen("1"));
        else if (chn == 2)
            w_cnt = write(fd, "2", strlen("2"));
        else if (chn == 3)
            w_cnt = write(fd, "3", strlen("3"));

        if (w_cnt == -1) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM port write:%s failed\n", buf);
        }
        close(fd);
        return -1;

    }
    return 0;
}

int app_ipcam_Pwm_Param_Set(int grp, int chn, int period, int duty_cycle)
{
    int fd;
    char buf[MAX_BUF], buf_parameter[MAX_BUF];

    PWM_PORT_CHECK(grp, chn);

    snprintf(buf, sizeof(buf), SYSFS_PWM_DIR"%d/pwm%d/period", grp, chn);
    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM period open:%s failed\n", buf);
        return -1;
    }
    snprintf(buf_parameter, sizeof(buf_parameter), "%d", period);
    if( -1 == write(fd, buf_parameter, sizeof(buf_parameter)) ) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM port %s period write failed\n", buf);
        return -1;
    }
    close(fd);

    snprintf(buf, sizeof(buf), SYSFS_PWM_DIR"%d/pwm%d/duty_cycle", grp, chn);
    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM duty_cycle open:%s failed\n", buf);
        return -1;
    }
    snprintf(buf_parameter, sizeof(buf_parameter), "%d", duty_cycle);
    if ( -1 == write(fd, buf_parameter, sizeof(buf_parameter)) ) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM port %s duty_cycle write failed\n", buf);
        return -1;
    }
    close(fd);

    return 0;
}

int app_ipcam_Pwm_Enable(int grp, int chn)
{
    int fd;
    char buf[MAX_BUF];

    PWM_PORT_CHECK(grp, chn);

    snprintf(buf, sizeof(buf), SYSFS_PWM_DIR"%d/pwm%d/enable", grp, chn);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM period open:%s failed\n", buf);
        return -1;
    }

    if ( -1 == write(fd, "1", strlen("1")) ) {
        printf("write pwm enable failed\n");
        APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM port %s Enable failed\n", buf);
        return -1;
    }

    close(fd);
    return 0;
}

int app_ipcam_Pwm_Disable(int grp, int chn)
{
    int fd;
    char buf[MAX_BUF];

    PWM_PORT_CHECK(grp, chn);

    snprintf(buf, sizeof(buf), SYSFS_PWM_DIR"%d/pwm%d/enable", grp, chn);

    if ((access(buf, F_OK)) != -1) {
        fd = open(buf, O_WRONLY);
        if (fd < 0) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM period open:%s failed\n", buf);
            return -1;
        }

        if ( -1 == write(fd, "0", strlen("0")) ) {
            APP_PROF_LOG_PRINT(LEVEL_ERROR, "PWM port %s Disable failed\n", buf);
            return -1;
        }
        close(fd);
    }

    return 0;
}
