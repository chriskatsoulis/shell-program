#pragma once
extern int signal_init(void);
extern int signal_enable_interrupt(int sig);
extern int signal_ignore(int sig);
extern int signal_restore(void);
