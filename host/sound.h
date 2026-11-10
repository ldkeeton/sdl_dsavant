/* Host sound hardware. Safe for the rewritten game C; no SDL types here.
   The recursive lock stands in for cli/sti around PLAY's shared state. */
#ifndef SOUND_H
#define SOUND_H
#include "hoststd.h"
void sound_init(void);
void sound_shutdown(void);
void sound_lock(void);
void sound_unlock(void);
/* PIT channel 0: divisor 0 means 65536; NULL restores the BIOS handler.
   BIOS time itself is maintained independently by bios.c. */
void sound_timer(uint16_t divisor, void (*handler)(void));
void sound_pcm(float level);

#ifdef __cplusplus
extern "C" {
#endif
void host_opl_reset(void);
void host_opl_address(uint8_t reg);
void host_opl_data(uint8_t value);
int32_t host_opl_sample(void);
void host_opl_advance(uint32_t samples);
#ifdef __cplusplus
}
#endif
#endif
