#ifndef COMMANDS_H
#define COMMANDS_H

#include "ktypes.h"

void commands_init(void);
void commands_execute(const char *line);
const char *commands_cwd(void);
uint32_t commands_shell_task_id(void);
void commands_set_shell_task_id(uint32_t id);

#endif
