/* host stand-in */
extern int sim_irq_off;
#define cli() (sim_irq_off = 1)
#define sei() (sim_irq_off = 0)
