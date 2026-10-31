/* timer.h - host stand-in, advanced by sim_avr() */
#ifndef TIMER_H
#define TIMER_H
#include "global.h"
extern volatile u16 timer_100us;
extern volatile u32 time_stamp;
static inline void timer_hw_reset(void) {}
static inline u16  timer_hw_get(void) { return 0; }
static inline u16  timer_hw_calc_rate_kbs(u16 bytes, u16 delta) { return 0; }
#endif
