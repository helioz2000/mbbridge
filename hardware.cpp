/**
 * @file hardware.cpp
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <sys/utsname.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hardware.h"
#include "datatag.h"

#include <stdexcept>
#include <iostream>

/*********************
 *      DEFINES
 *********************/
#define BRIGTHNESS_CONTROL "/sys/class/backlight/rpi_backlight/brightness"
#define CPU_TEMP "/sys/class/thermal/thermal_zone0/temp"
#define OS_NAME_PATH "/etc/os-release"
#define MODEL_NAME_PATH "/proc/device-tree/model"
#define TOUCH_INPUT_PATH "/dev/input/event0"
#define SCREEN_SAVER_TIME 8      // in s

using namespace std;

int touch_fd = -1;
time_t screen_timout;
bool screen_saver_active = false;

Hardware::Hardware() {
    printf("%s\n", __func__);
    throw runtime_error("Class Hardware - forbidden constructor");
}

Hardware::Hardware(bool withScreen)
{
    //fprintf(stderr, "%s: Constructor called\n", __func__);
    if (withScreen) {
        _has_screen = true;
        touch_fd = -1;
        screen_saver_active = false;
        // preset screen saver timeout
        time_t now = time(NULL);
        screen_timout = now + SCREEN_SAVER_TIME;

        // open touch screen input for screen saver
        touch_fd = open(TOUCH_INPUT_PATH, O_RDONLY | O_NONBLOCK);
        if (touch_fd == -1) {
            printf("Failed to open Touch Input\n");
        }
    } else {
        _has_screen = false;
    }
}

Hardware::~Hardware() {
  //fprintf(stderr, "%s: Destructor called\n", __func__);
}

int Hardware::shutdown(bool reboot)
{
    char cmdbuf[50];
    if (reboot) {
        sprintf(cmdbuf, "shutdown -r now");
    } else {
        sprintf(cmdbuf, "shutdown -h now");
    }
    int retval = system(cmdbuf);
    return retval;
}

/**
 * Read CPU temperature
 */
float Hardware::read_cpu_temp(void)
{
    char buffer[80];
    float retval = 0.0;
    int fd = open(CPU_TEMP, O_RDONLY | O_NONBLOCK);
    if (fd != -1) {
        int length = read(fd, buffer, sizeof(buffer));
        buffer[length] = 0;
        sscanf(buffer, "%f", &retval);
        retval = retval / 1000.0;
        close(fd);
    }

    return retval;
}

int Hardware::get_ip_address(char *buffer, int maxlen)
{
    char buf[40];
    char data[128];
    FILE *pf;
    int length = -1;

    // Read IP address (IPV4 + IPV6)
    sprintf(buf, "hostname -I");
    pf = popen(buf,"r");
    if (pf != NULL) {
        fgets(data, sizeof(data) , pf);
        pclose(pf);
        length = strlen(data);
        data[length-1] = 0;
        if (strlen(data) <= (unsigned)maxlen) {
            strcpy(buffer, data);
        } else {
            strncpy(buffer, data, maxlen);
            buffer[maxlen-1] = 0;
            length = maxlen;
        }
    }
    return length;
}

int Hardware::get_model_name(char *buffer, int maxlen)
{
    char buf[80];
    int length = -1;
    int fd = open(MODEL_NAME_PATH, O_RDONLY | O_NONBLOCK);
    if (fd != -1) {
        length = read(fd, buf, maxlen-1);
        buf[length] = 0;
        close(fd);
        strcpy(buffer, buf);
    }
    return length;
}

int Hardware::get_os_name(char *buffer, int maxlen)
{
    char buf[maxlen];
    int length = -1;
    int fd = open(OS_NAME_PATH, O_RDONLY | O_NONBLOCK);
    if (fd != -1) {
        length = read(fd, buf, maxlen-1);
        buf[length] = 0;
        close(fd);
        // extract PRETTY_NAME parameter from first line
        char *start = strchr(buf, '"') + 1;
        char *end = strchr(start, '"');
        int count = end-start;
        buf[end-buf] = 0;
        strncpy(buffer, start, count+1);
        length = count;
    }
    return length;
}

int Hardware::get_kernel_name(char *buffer, int maxlen)
{
    char buf[255];
    struct utsname kernel;
    int retval = uname(&kernel);
    if (retval == 0) {
        sprintf(buf,"%s Kernel %s", kernel.sysname, kernel.release);
    }
    if (strlen(buf) <= (unsigned)maxlen) {
        strcpy(buffer, buf);
    } else {
        strncpy(buffer, buf, maxlen);
    }
    return strlen(buffer);
}

void Hardware::process_screen_saver(int brightness)
{
    char buffer[256];
    if (!_has_screen) return;
    if (touch_fd <= 0) return;
    int length = read(touch_fd, buffer, sizeof(buffer));
    time_t now = time(NULL);
    if (length > 0) {   // Touch detected
        screen_timout = now + SCREEN_SAVER_TIME;
        if (screen_saver_active) {      // disable screen saver
            set_brightness(brightness);
            screen_saver_active = false;
        }
    } else {    // No Touch
        if (!screen_saver_active) {
            if (now > screen_timout) {
                set_brightness(0);
                screen_saver_active = true;
            }
        }
    }
}

/**
 * Set screen brightness
 */
bool Hardware::set_brightness(int new_brightness)
{
    char buffer[80];
    int brightness_fd;

    if (!_has_screen) return false;

    brightness_fd = open(BRIGTHNESS_CONTROL, O_RDWR);
    if (brightness_fd != -1) {
        sprintf(buffer,"%d", new_brightness);
        write(brightness_fd, buffer, strlen(buffer));
        close(brightness_fd);
    } else {
        return false;
    }
    //printf("Brightness: %d\n", new_brightness);
    return true;
}

/**
 * get current screen brightness
 * returns -1 on failure
 */
int Hardware::get_brightness(void)
{
    char buffer[80];
    int fd;
    int length;
    int value = -1;
    if (!_has_screen) return -1;
    fd = open(BRIGTHNESS_CONTROL, O_RDONLY | O_NONBLOCK);
    if (fd != -1) {
        length = read(fd, buffer, sizeof(buffer));
        buffer[length] = 0;
        sscanf(buffer, "%d", &value);
        close(fd);
    }
    return value;
}