/* host stand-in: 3 cycles per count */
extern void sim_avr(unsigned cycles);
static inline void _delay_loop_1(unsigned char n) { sim_avr(3u * n); }
