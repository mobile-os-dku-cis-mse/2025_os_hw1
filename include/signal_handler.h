#ifndef INC_2025_OS_HW1_HANDLER_H
#define INC_2025_OS_HW1_HANDLER_H

#include <signal.h>

void setup_signal_handlers(void);
void reset_child_signals(void);
void check_signal_handler(int signum, const char *signame);

#endif