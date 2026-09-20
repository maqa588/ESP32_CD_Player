#pragma once

#include <stdint.h>
#include <stdbool.h>

static inline void dacp_init(void) {}
static inline void dacp_set_session(const char *dacp_id, const char *active_remote, uint32_t client_ip) {
    (void)dacp_id; (void)active_remote; (void)client_ip;
}
static inline void dacp_clear_session(void) {}
static inline void dacp_send_playpause(void) {}
static inline void dacp_send_next(void) {}
static inline void dacp_send_prev(void) {}
static inline void dacp_send_volume_up(void) {}
static inline void dacp_send_volume_down(void) {}
static inline void dacp_send_volume(float volume_percent) { (void)volume_percent; }
static inline bool dacp_is_active(void) { return false; }
static inline bool dacp_probe_service(void) { return false; }
