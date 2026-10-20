#pragma once

#include "zanni_protocol.h"

typedef int (*zanni_recv_callback_t)(zanni_message_t *);
typedef void (*znc_ready_callback_t)(void);

void znc_set_ready_callback(znc_ready_callback_t callback);
int znc_start_server(int port, int listen_backlog, zanni_recv_callback_t callback);
void znc_stop_server(void);