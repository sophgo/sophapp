#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>

#include "app_ipcam_gpio.h"

static APP_PARAM_GPIO_CFG_S stGpioCfg;
static APP_PARAM_GPIO_CFG_S *pstGpioCfg = &stGpioCfg;
#define EVB_GPIO(x) (x<32)?(480+x):((x<64)?(448+x-32):((x<96)?(416+x-64):((x<=108)?(404+x-96):(-1))))
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64
#define CHECK_GPIO_NUMBER(x)                                                                                       \
        do {                                                                                                       \
            if ((x < CVI_GPIO_MIN) || (x > CVI_GPIO_MAX)) {                                                        \
                printf("\033[0;31m GPIO %d is invalid at %s: LINE: %d!\033[0;39m\n", x, __func__, __LINE__);       \
                return -1;                                                                                         \
            }                                                                                                      \
        } while (0)

APP_PARAM_GPIO_CFG_S *app_ipcam_Gpio_Param_Get(void)
{
    return pstGpioCfg;
}

static int GpioExport(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
    if (fd < 0) {
        printf("gpio %d export error\n", gpio);
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    if (-1 == write(fd, buf, len)) {
        printf("write gpio export failed\n");
    }
    close(fd);

    return 0;
}

static int GpioUnexport(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
    if (fd < 0) {
        printf("gpio %d unexport error\n", gpio);
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    if (-1 == write(fd, buf, len)) {
        printf("write gpio unexport failed\n");
    }
    close(fd);

    return 0;
}

static int GpioDirectionSet(unsigned int gpio, CVI_GPIO_DIRECTION_E dir)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR"/gpio%d/direction", gpio);
    if(access(buf, F_OK) == -1) {
        GpioExport(gpio);
    }

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        printf("gpio %d open error\n", gpio);
        return fd;
    }

    printf("mark %d , %s \n", dir, buf);
    if (dir) {
        if (-1 == write(fd, "out", 4)) {
            printf("write gpio dir out failed\n");
        }
    } else {
        if (-1 == write(fd, "in", 3)) {
            printf("write gpio dir in failed\n");
        }
    }

    close(fd);

    return 0;
}

static int GpioDirectionGet(unsigned int gpio, CVI_GPIO_DIRECTION_E *dir)
{
    int fd;
    char buf[MAX_BUF] = "\0";
    char ch;

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR"/gpio%d/direction", gpio);
    if(access(buf, F_OK) == -1) {
        printf("gpio_%d not export!\n", gpio);
        return -1;
        //GpioExport(gpio);
    }

    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        printf("gpio %d open error\n", gpio);
        return fd;
    }

    if (-1 == read(fd, &ch, 1)) {
        printf("read gpio dir failed\n");
    }
    if (ch == 'i') {
        *dir = CVI_GPIO_DIR_IN;
    } else if (ch == 'o') {
        *dir = CVI_GPIO_DIR_OUT;
    } else {
        printf("get dir fail!\n");
        return -1;
    }

    close(fd);

    return 0;
}

static int GpioValueSet(unsigned int gpio, unsigned int value)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf),SYSFS_GPIO_DIR"/gpio%d/value", gpio);
    if(access(buf, F_OK) == -1) {
        GpioExport(gpio);
        GpioDirectionSet(gpio, CVI_GPIO_DIR_OUT);
    }

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        printf("gpio %d open error\n", gpio);
        return fd;
    }

    if (value != 0) {
        if (-1 == write(fd, "1", 2)) {
            printf("write gpio dir in failed\n");
        }
    } else {
        if (-1 == write(fd, "0", 2)) {
            printf("write gpio dir in failed\n");
        }
    }

    close(fd);

    return 0;
}

static int GpioValueGet(unsigned int gpio, unsigned int *value)
{
    int fd;
    char buf[MAX_BUF];
    char ch;

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    if(access(buf, F_OK) == -1) {
        printf("gpio_%d not export!\n", gpio);
        return -1;
        //GpioExport(gpio);
    }

    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        printf("gpio %d open error\n", gpio);
        return fd;
    }

    if (-1 == read(fd, &ch, 1)) {
        printf("read gpio value failed\n");
    }
    //printf(" GpioValueGet = %c \n", ch);

    if (ch != '0') {
        *value = 1;
    } else {
        *value = 0;
    }

    close(fd);
    return 0;
}

static int GpioEdgeSet(unsigned int gpio, unsigned int edge_flag)
{
    int fd;
    char buf[MAX_BUF];
    ssize_t w_cnt = 0;

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR"/gpio%d/edge", gpio);
    if(access(buf, F_OK) == -1) {
        GpioExport(gpio);
    }

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        printf("gpio %d open error\n", gpio);
        return fd;
    }

    //printf("mark %d , %s \n",edge_flag, buf);
    switch(edge_flag)
    {
        case CVI_GPIO_NONE:
            w_cnt = write(fd, "none", 5);
            break;
        case CVI_GPIO_RISING:
            w_cnt = write(fd, "rising", 7);
            break;
        case CVI_GPIO_FALLING:
            w_cnt = write(fd, "falling", 8);
            break;
        case CVI_GPIO_BOTH:
            w_cnt = write(fd, "both", 5);
            break;
        default:
            w_cnt = write(fd, "none", 5);
            break;
    }
    if (w_cnt == -1) {
        printf("write gpio edge failed\n");
    }
    close(fd);
    return 0;
}

static int GpioPoll(unsigned int gpio, unsigned int event, FunType Fp)
{
    struct pollfd pfd;
    int fd;
    char value;
    char buf[MAX_BUF];
    
    snprintf(buf, sizeof(buf),SYSFS_GPIO_DIR"/gpio%d/value", gpio);
    fd = open(buf, O_RDWR);
    if (fd < 0) {
        printf("gpio %d open error\n", gpio);
        return -1;
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
        printf("gpio %d lseek error\n", gpio);
        return -1;
    }
    if (read(fd, &value, 1) == -1) {
        printf("gpio %d read error\n", gpio);
        return -1;
    }

    pfd.fd = fd;
    pfd.events = event;
    
    while (1) {
        if (poll(&pfd, 1, 1000) == -1) {
            printf("gpio %d poll error\n", gpio);
            return -1;
        }

        if (pfd.revents & POLLPRI) {
            if (lseek(fd, 0, SEEK_SET) == -1) {
                printf("gpio %d lseek error\n", gpio);
                return -1;
            }
            if (read(fd, &value, 1) == -1) {
                printf("gpio %d read error\n", gpio);
                return -1;
            }
            printf("poll value:%c\n", value);
            if (Fp) {
                Fp();
            }
        }
        if (pfd.revents & POLLERR) {
            printf("gpio %d POLLERR\n", gpio);
        }

        usleep(500 * 1000);
    }
    
    close(fd);
    
    return 0;
}

int app_ipcam_Gpio_Export(CVI_GPIO_NUM_E gpio)
{
    int ret = 0;
    CHECK_GPIO_NUMBER(gpio);
    ret = GpioExport(gpio);

    return ret;
}

int app_ipcam_Gpio_Unexport(CVI_GPIO_NUM_E gpio)
{
    int ret = 0;
    CHECK_GPIO_NUMBER(gpio);
    ret = GpioUnexport(gpio);

    return ret;
}

int app_ipcam_Gpio_Dir_Set(CVI_GPIO_NUM_E gpio, CVI_GPIO_DIRECTION_E dir)
{
    int ret = 0;
    CHECK_GPIO_NUMBER(gpio);
    ret = GpioDirectionSet(gpio, dir);

    return ret;
}

int app_ipcam_Gpio_Dir_Get(CVI_GPIO_NUM_E gpio, CVI_GPIO_DIRECTION_E *dir)
{
    int ret = 0;
    CHECK_GPIO_NUMBER(gpio);
    ret = GpioDirectionGet(gpio, dir);

    return ret;
}

int app_ipcam_Gpio_Value_Set(CVI_GPIO_NUM_E gpio, CVI_GPIO_VALUE_E value)
{
    int ret = 0;
    CHECK_GPIO_NUMBER(gpio);
    ret = GpioValueSet(gpio, value);

    return ret;
}

int app_ipcam_Gpio_Value_Get(CVI_GPIO_NUM_E gpio, CVI_GPIO_VALUE_E *value)
{
    int ret = 0;
    CHECK_GPIO_NUMBER(gpio);
    ret = GpioValueGet(gpio, value);

    return ret;
}

int app_ipcam_Gpio_Polling(CVI_GPIO_NUM_E gpio, CVI_GPIO_EDGE_E edge, FunType Fp)
{
    int ret = 0;
    CHECK_GPIO_NUMBER(gpio);

    // open gpio
    GpioExport(gpio);

    // set input mode
    ret = GpioDirectionSet(gpio, CVI_GPIO_DIR_IN);

    // set interrupt trigger
    ret |= GpioEdgeSet(gpio, edge);

    ret |= GpioPoll(gpio, POLLPRI | POLLERR, Fp);

    // operated and released
    ret |= GpioUnexport(gpio);
    
    return ret;
}

