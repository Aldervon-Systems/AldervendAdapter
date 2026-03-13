#ifndef HEARTBEAT_H
#define HEARTBEAT_H

typedef enum {
    HEARTBEAT_MODE_WIFI_CONFIG = 0,
    HEARTBEAT_MODE_NORMAL,
    HEARTBEAT_MODE_WORKING,
} heartbeat_mode_t;

void heartbeat_init(void);
void heartbeat_set_mode(heartbeat_mode_t mode);

#endif

