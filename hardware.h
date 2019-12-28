/**
 * @file hardware.h
 *
 -----------------------------------------------------------------------------
 This class provides access to various hardware and OS related information.
 It also handles the screen saver and system shutdown functions.
 -----------------------------------------------------------------------------
 */

#ifndef HARDWARE_H
#define HARDWARE_H

#include <time.h>

class Hardware {
public:
    // forbidden constructor, will fail
    Hardware ();
    // Constructor
    Hardware (bool withScreen);

    // Destructor
    ~Hardware ();

    /**
     * Shutdown and Reboot the system
     * @param reboot: false=halt true=reboot
     */
    int shutdown(bool reboot);

    /**
     * Read the CPU temperature in DegC
     */
    float read_cpu_temp(void);

    /**
     * Read operating system name
     * @param buffer: text storage buffer
     * @param maxlen: length of text storage buffer
     */
    int get_os_name(char *buffer, int maxlen);

    /**
     * Read hardware model name
     * @param buffer: text storage buffer
     * @param maxlen: length of text storage buffer
     */
    int get_model_name(char *buffer, int maxlen);

    /**
     * Read Kernel name and version
     * @param buffer: text storage buffer
     * @param maxlen: length of text storage buffer
     */
    int get_kernel_name(char *buffer, int maxlen);

    /**
     * Read active IP address
     * @param buffer: text storage buffer
     * @param maxlen: length of text storage buffer
     */
    int get_ip_address(char *buffer, int maxlen);

// ----- Display related methods

    /**
     * Process the screen saver
     * @param brightness: The current brightness setting when there screen saver is not active
     */
    void process_screen_saver(int brightness);

    /**
     * Set the display brightness
     * @param new_brightness: new brightness setting
     */
    bool set_brightness(int new_brightness);

    /**
     * Read the display brightness
     */
    int get_brightness(void);


private:
    bool _has_screen;            // true if screen is in use
    int touch_fd;               // file descriptor for touch input
    time_t screen_timout;       // screen saver timeout
    bool screen_saver_active;   //

};

#endif /* HARDWARE_H */