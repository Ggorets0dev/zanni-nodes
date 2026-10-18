#pragma once

#include <stdbool.h>
#include "znc_types.h"

char* znc_execute_toggle(const znc_commands_toggle_t* cmds,
                         bool state,
                         int *exit_code);