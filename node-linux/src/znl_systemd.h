#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Check if running under systemd with notification socket support.
 * @return true if NOTIFY_SOCKET environment variable is set and non-empty.
 */
bool znl_systemd_is_available(void);

/**
 * @brief Low-level notify function sending state string to systemd notify socket.
 * Compatible with standard sd_notify protocol.
 * If running outside systemd (no NOTIFY_SOCKET), this is a harmless no-op returning 0.
 *
 * @param state State notification string (e.g. "READY=1", "WATCHDOG=1").
 * @return 0 on success or if not running under systemd, negative on error.
 */
int znl_systemd_notify(const char *state);

/**
 * @brief Formatted systemd notification.
 * @param fmt Printf-like format string.
 * @return 0 on success or if not running under systemd, negative on error.
 */
int znl_systemd_notifyf(const char *fmt, ...);

/**
 * @brief Send READY=1 notification to systemd, with optional status message.
 * @param status_msg Optional status message to accompany READY=1 (can be NULL).
 * @return 0 on success or no-op, negative on error.
 */
int znl_systemd_notify_ready(const char *status_msg);

/**
 * @brief Update systemd service status message (STATUS=...).
 * @param fmt Printf format string for the status message.
 * @return 0 on success or no-op, negative on error.
 */
int znl_systemd_notify_status(const char *fmt, ...);

/**
 * @brief Send STOPPING=1 notification to systemd, with optional status message.
 * @param status_msg Optional status message (can be NULL).
 * @return 0 on success or no-op, negative on error.
 */
int znl_systemd_notify_stopping(const char *status_msg);

/**
 * @brief Send a single WATCHDOG=1 keepalive ping to systemd.
 * @return 0 on success or no-op, negative on error.
 */
int znl_systemd_notify_watchdog(void);

/**
 * @brief Check whether systemd watchdog is enabled for this service.
 * Inspects WATCHDOG_USEC and WATCHDOG_PID environment variables.
 *
 * @param out_usec Output pointer for watchdog timeout in microseconds (optional).
 * @return true if watchdog is enabled, false otherwise.
 */
bool znl_systemd_watchdog_is_enabled(uint64_t *out_usec);

/**
 * @brief Start automatic background watchdog keepalive thread if enabled by systemd.
 * If WATCHDOG_USEC is set, spawns a thread that sends WATCHDOG=1 at interval = WATCHDOG_USEC / 2.
 * If watchdog is not configured by systemd, logs info and returns 0 without starting a thread.
 *
 * @return 0 on success, negative on error.
 */
int znl_systemd_watchdog_start(void);

/**
 * @brief Stop the background watchdog thread cleanly.
 */
void znl_systemd_watchdog_stop(void);

/**
 * @brief Record activity timestamp / kick watchdog on demand.
 */
void znl_systemd_watchdog_touch(void);
