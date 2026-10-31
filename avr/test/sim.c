/* sim.c - host simulation of the plipbox parallel protocol.
 *
 * The firmware side is the real avr/src/pb_proto.c, built against stand-in
 * pin helpers (stub/par_low.h) that charge AVR cycles at 16 MHz.  The Amiga
 * side is a C model of the hwpar.asm loops, run as a coroutine.  Every CIA
 * access by the Amiga is aligned to the E clock and lasts one E cycle;
 * every access of CIA-A port B pulses /STROBE low for the following E cycle
 * (8520 PC) and sets the latch the firmware polls.
 *
 * Output: one key=value line per case, exit code 0 when all pass.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#include "global.h"
#include "pb_proto.h"

typedef unsigned long long simt;          /* picoseconds */
#define PS_AVR      62500ULL              /* 16 MHz */
#define PS_E        1409700ULL            /* 709.379 kHz */
#define PS_US       1000000ULL
#define PS_100US    (100 * PS_US)
#define PS_LIMIT    (3000ULL * 1000 * PS_US)  /* 3 s per case: hang */

/* ----- wire ----- */
int sim_sel, sim_req, sim_rak;
int sim_avr_out, sim_amiga_out;
u08 sim_avr_data, sim_amiga_data;
int sim_intf;
int sim_irq_off;
static simt strobe_from, strobe_until;
static int contention;

volatile u16 timer_100us;
volatile u32 time_stamp;
static simt now, last_tick;

/* ----- Amiga coroutine ----- */
static ucontext_t main_ctx, amiga_ctx;
static char amiga_stack[256 * 1024];
static simt am_t, am_next;
static int am_done = 1, am_rc;
static void (*am_func)(void);

/* Amiga CPU model: gap between CIA accesses, and interrupts */
static simt gap_min, gap_max;
static unsigned irq_permille;
static simt irq_len;

/* firmware model: interrupt service while not in cli() */
static unsigned avr_isr_permille;

static unsigned long long rng = 88172645463325252ULL;
static unsigned rnd(unsigned n)
{
  rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
  return (unsigned)(rng % n);
}

static void run_amiga(void)
{
  while(!am_done && am_next <= now)
    swapcontext(&main_ctx, &amiga_ctx);
}

int sim_strobe_level(void)
{
  return !(now >= strobe_from && now < strobe_until);
}

void sim_avr_drive(int on)
{
  sim_avr_out = on;
  if(sim_avr_out && sim_amiga_out) contention++;
}

void sim_avr(unsigned cycles)
{
  now += (simt)cycles * PS_AVR;
  if(!sim_irq_off && avr_isr_permille && rnd(1000) < avr_isr_permille)
    now += (3 + rnd(6)) * PS_US;
  while(now - last_tick >= PS_100US) {
    last_tick += PS_100US;
    timer_100us++;
    time_stamp++;
  }
  run_amiga();
}

/* one CIA access: wait for its E cycle, then back to the AVR until then */
static void cia_cycle(void)
{
  simt start = am_t + gap_min + (gap_max > gap_min ? rnd((unsigned)(gap_max - gap_min)) : 0);
  if(irq_permille && rnd(1000) < irq_permille)
    start += irq_len;
  am_t = (start / PS_E + 1) * PS_E + PS_E;
  am_next = am_t;
  swapcontext(&amiga_ctx, &main_ctx);
}

static void prb_pulse(void)
{
  strobe_from = am_t;
  strobe_until = am_t + PS_E;
  sim_intf = 1;
}

static int  cia_rak(void)        { cia_cycle(); return sim_rak; }
static u08  cia_read_prb(void)   { cia_cycle(); u08 d = sim_avr_out ? sim_avr_data : 0xff; prb_pulse(); return d; }
static void cia_write_prb(u08 d) { cia_cycle(); sim_amiga_data = d; prb_pulse(); }
static void cia_ddr(int out)     { cia_cycle(); sim_amiga_out = out; if(sim_avr_out && sim_amiga_out) contention++; }
static void cia_sel(int v)       { cia_cycle(); cia_cycle(); sim_sel = v; }  /* bset/bclr: read + write */
static void cia_req(int v)       { cia_cycle(); cia_cycle(); sim_req = v; }

/* ----- Amiga transfers (models of hwpar.asm) ----- */
static simt am_t0;
static u08 am_buf[2048];
static u16 am_len, am_max = 1514;

static int am_timeout(void) { return am_t - am_t0 > 500ULL * 1000 * PS_US; }
#define WAIT_RAK(v) do { while(cia_rak() != (v)) if(am_timeout()) goto err; } while(0)

static void am_strobe_recv(void)          /* _hwstroberecv */
{
  am_rc = 0;
  WAIT_RAK(0);
  cia_ddr(1);
  cia_write_prb(PBPROTO_CMD_RECV_STROBE);
  cia_sel(1);
  WAIT_RAK(1);
  cia_ddr(0);
  cia_req(1);
  WAIT_RAK(0); am_buf[0] = cia_read_prb();
  WAIT_RAK(1); am_buf[1] = cia_read_prb();
  am_len = (u16)(am_buf[0] << 8 | am_buf[1]);
  if(am_len > am_max) goto err;
  for(u16 i = 0; i < ((am_len + 1) & ~1); i += 2) {
    WAIT_RAK(0); am_buf[2 + i] = cia_read_prb();
    WAIT_RAK(1); am_buf[3 + i] = cia_read_prb();
  }
  am_rc = 1;
err:
  cia_req(0);
  cia_sel(0);
}

static void am_recv(void)                 /* _hwrecv */
{
  am_rc = 0;
  WAIT_RAK(0);
  cia_ddr(1);
  cia_write_prb(PBPROTO_CMD_RECV);
  cia_sel(1);
  WAIT_RAK(1);
  cia_ddr(0);
  cia_req(1);
  WAIT_RAK(0); am_buf[0] = cia_read_prb(); cia_req(0);
  WAIT_RAK(1); am_buf[1] = cia_read_prb(); cia_req(1);
  am_len = (u16)(am_buf[0] << 8 | am_buf[1]);
  if(am_len > am_max) goto err;
  for(u16 i = 0; i < ((am_len + 1) & ~1); i += 2) {
    WAIT_RAK(0); am_buf[2 + i] = cia_read_prb(); cia_cycle(); sim_req = 0;
    WAIT_RAK(1); am_buf[3 + i] = cia_read_prb(); cia_cycle(); sim_req = 1;
  }
  am_rc = 1;
err:
  cia_req(0);
  cia_sel(0);
}

static u08 tx_buf[2048];
static u16 tx_len;

static void am_send(void)                 /* _hwsend */
{
  am_rc = 0;
  WAIT_RAK(0);
  cia_ddr(1);
  cia_write_prb(PBPROTO_CMD_SEND);
  cia_sel(1);
  u16 words = (u16)((tx_len + 2 + 1) / 2);
  for(u16 i = 0; i < words; i++) {
    WAIT_RAK(1); cia_write_prb(tx_buf[2 * i]);     cia_req(1);
    WAIT_RAK(0); cia_write_prb(tx_buf[2 * i + 1]); cia_req(0);
  }
  WAIT_RAK(1);
  am_rc = 1;
err:
  cia_ddr(0);
  cia_sel(0);
}

static void amiga_entry(void)
{
  am_t0 = am_t;
  am_func();
  am_done = 1;
  am_next = (simt)-1;
  swapcontext(&amiga_ctx, &main_ctx);
}

/* ----- firmware callbacks ----- */
static u08 avr_buf[1600];
static u08 pattern[1600];
static u16 fill_len;
static u16 got_len;
static u08 got_buf[1600];

static u08 fill(u08 *buf, u16 max, u16 *size)
{
  memcpy(buf, pattern, fill_len);
  *size = fill_len;
  return PBPROTO_STATUS_OK;
}

static u08 proc(const u08 *buf, u16 size)
{
  memcpy(got_buf, buf, size);
  got_len = size;
  return PBPROTO_STATUS_OK;
}

/* ----- cases ----- */
struct cfg {
  const char *name;
  simt gap_min, gap_max;            /* Amiga CPU time between CIA accesses */
  unsigned irq_permille; simt irq_len;
  unsigned avr_isr_permille;
};

static const struct cfg cfgs[] = {
  { "fastcpu",   0,            0,             0,  0,              0  },
  { "pistorm",   2 * PS_US,    4 * PS_US,     2,  200 * PS_US,   20 },
  { "m68000",    1 * PS_US,    3 * PS_US,     5,  100 * PS_US,   20 },
  { "jitter",    0,            30 * PS_US,   20,  300 * PS_US,  100 },
};

/* returns firmware status; sets am_rc */
static u08 run_case(const struct cfg *c, void (*f)(void), simt *elapsed)
{
  sim_sel = sim_req = sim_rak = 0;
  sim_avr_out = sim_amiga_out = 0;
  sim_intf = 0; sim_irq_off = 0;
  strobe_from = strobe_until = 0;
  contention = 0;
  gap_min = c->gap_min; gap_max = c->gap_max;
  irq_permille = c->irq_permille; irq_len = c->irq_len;
  avr_isr_permille = c->avr_isr_permille;

  am_func = f; am_done = 0; am_rc = 0; am_t = now; am_next = now;
  getcontext(&amiga_ctx);
  amiga_ctx.uc_stack.ss_sp = amiga_stack;
  amiga_ctx.uc_stack.ss_size = sizeof(amiga_stack);
  amiga_ctx.uc_link = NULL;
  makecontext(&amiga_ctx, amiga_entry, 0);

  simt t0 = now;
  u08 st = PBPROTO_STATUS_IDLE;
  while(now - t0 < PS_LIMIT) {
    u08 r = pb_proto_handle();
    if(r != PBPROTO_STATUS_IDLE) st = r;
    if(am_done && st != PBPROTO_STATUS_IDLE && !sim_sel) break;
  }
  /* the firmware ends with RAK = 0 after SEL = 0 */
  sim_avr(100);
  *elapsed = now - t0;
  return st;
}

static int fails;
static int known_race;

static void report(const char *test, const struct cfg *c, u16 len, int ok,
                   u08 st, simt el, u16 bytes)
{
  double us = (double)el / PS_US;
  printf("test=%s cpu=%s len=%u ok=%d fw_status=0x%02x amiga_rc=%d contention=%d rak_end=%d us=%.0f",
         test, c->name, len, ok, st, am_rc, contention, sim_rak, us);
  if(bytes && us > 0)
    printf(" us_per_byte=%.2f", us / bytes);
  printf("\n");
  if(!ok) fails++;
}

int main(int argc, char **argv)
{
  /* old: pb_proto.c without CMD_RECV_STROBE; only the fallback case applies */
  int old_fw = argc > 1 && !strcmp(argv[1], "old");
  static const u16 lens[] = { 0, 1, 2, 14, 15, 60, 61, 1514 };

  for(unsigned i = 0; i < sizeof(pattern); i++) pattern[i] = (u08)(i * 37 + 11);
  pb_proto_init(fill, proc, avr_buf, sizeof(avr_buf));

  for(unsigned ci = 0; ci < sizeof(cfgs) / sizeof(cfgs[0]); ci++) {
    const struct cfg *c = &cfgs[ci];
    for(unsigned li = 0; li < sizeof(lens) / sizeof(lens[0]); li++) {
      u16 len = lens[li];
      simt el;
      u08 st;
      int ok;

      fill_len = len;
      memset(am_buf, 0xee, sizeof(am_buf));
      st = run_case(c, am_strobe_recv, &el);
      if(old_fw) {
        /* the driver must give up cleanly; firmware rejects the command */
        ok = am_done && !am_rc && st == PBPROTO_STATUS_INVALID_CMD && !contention && !sim_rak;
        report("strobe_vs_old_fw", c, len, ok, st, el, 0);
        continue;
      }
      ok = am_done && am_rc && st == PBPROTO_STATUS_OK && !contention && !sim_rak &&
           am_len == len && !memcmp(am_buf + 2, pattern, len);
      report("strobe_recv", c, len, ok, st, el, len + 2);

      memset(am_buf, 0xee, sizeof(am_buf));
      st = run_case(c, am_recv, &el);
      ok = am_done && am_rc && !contention && !sim_rak &&
           am_len == len && !memcmp(am_buf + 2, pattern, len);
      /* Unchanged since v0.6: the firmware polls the final REQ = 1, which
       * the Amiga drops again on exit.  An interrupt on the AVR there makes
       * it miss the level and report a lost select for a frame the Amiga
       * has; bridge.c then offers that frame again.  The strobe receive
       * latches its last edge instead. */
      if(ok && st == (PBPROTO_STATUS_LOST_SELECT | PBPROTO_STAGE_LAST_DATA))
        known_race++;
      else if(st != PBPROTO_STATUS_OK)
        ok = 0;
      report("legacy_recv", c, len, ok, st, el, len + 2);

      if(len >= 14) {
        tx_len = len;
        tx_buf[0] = (u08)(len >> 8); tx_buf[1] = (u08)len;
        memcpy(tx_buf + 2, pattern, len);
        got_len = 0;
        st = run_case(c, am_send, &el);
        ok = am_done && am_rc && st == PBPROTO_STATUS_OK && !contention &&
             got_len == ((len + 1) & ~1) && !memcmp(got_buf, pattern, len);
        report("legacy_send", c, len, ok, st, el, len + 2);
      }
    }
  }

  if(!old_fw) {
    /* Amiga gives up mid-frame (SEL dropped): firmware must not hang */
    const struct cfg *c = &cfgs[0];
    simt el;
    fill_len = 1514;
    am_max = 100;                        /* size check fails after size lo */
    u08 st = run_case(c, am_strobe_recv, &el);
    am_max = 1514;
    report("strobe_size_reject", c, 1514,
           am_done && !am_rc && st != PBPROTO_STATUS_OK && !contention && !sim_rak, st, el, 0);
  }

  printf("result=%s fails=%d legacy_final_req_race=%d\n", fails ? "fail" : "pass", fails, known_race);
  return fails ? 1 : 0;
}
