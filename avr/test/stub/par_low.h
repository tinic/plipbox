/* par_low.h - host stand-in: every pin access advances simulated AVR time
 * (sim.c) and lets the simulated Amiga run up to that time. */
#ifndef PAR_LOW_H
#define PAR_LOW_H

#include "global.h"

#define PAR_TIMED_INLINE static inline

extern void sim_avr(unsigned cycles);
extern int  sim_strobe_level(void);
extern int  sim_intf;
extern int  sim_sel, sim_req, sim_rak;
extern int  sim_avr_out;
extern u08  sim_avr_data;
extern u08  sim_amiga_data;
extern void sim_avr_drive(int on);

static inline void par_low_init(void) {}
static inline void par_low_data_set_output(void) { sim_avr(4); sim_avr_drive(1); }
static inline void par_low_data_set_input(void)  { sim_avr(4); sim_avr_drive(0); }
PAR_TIMED_INLINE void par_low_data_out(u08 d)       { sim_avr(8); sim_avr_data = d; }
PAR_TIMED_INLINE void par_low_data_out_latch(u08 d) { sim_avr(8); sim_avr_data = d; }
PAR_TIMED_INLINE u08  par_low_data_in(void)          { sim_avr(4); return sim_amiga_data; }
PAR_TIMED_INLINE void par_low_set_busy_hi(void) { sim_avr(2); sim_rak = 1; }
PAR_TIMED_INLINE void par_low_set_busy_lo(void) { sim_avr(2); sim_rak = 0; }
PAR_TIMED_INLINE void par_low_set_ack_lo(void) { sim_avr(2); }
PAR_TIMED_INLINE void par_low_set_ack_hi(void) { sim_avr(2); }
static inline void par_low_pulse_ack(u08 delay) { sim_avr(4 + 3 * delay); }
PAR_TIMED_INLINE u08 par_low_get_strobe(void) { sim_avr(2); return (u08)sim_strobe_level(); }
PAR_TIMED_INLINE u08 par_low_get_select(void) { sim_avr(2); return (u08)sim_sel; }
PAR_TIMED_INLINE u08 par_low_get_pout(void)   { sim_avr(2); return (u08)sim_req; }
PAR_TIMED_INLINE void par_low_strobe_clear(void) { sim_avr(2); sim_intf = 0; }
PAR_TIMED_INLINE u08 par_low_strobe_seen(void)   { sim_avr(2); return (u08)sim_intf; }

#endif
