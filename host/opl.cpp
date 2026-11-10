/* The only C++ boundary: ymfm's OPL2 core, called under sound_lock().
   PLAY does not use OPL timers or IRQs, so ymfm's default interface suffices. */
#include "sound.h"
#include "ymfm_opl.h"

static ymfm::ymfm_interface interface;
static ymfm::ym3812 chip(interface);

extern "C" void host_opl_reset(void) { chip.reset(); }
extern "C" void host_opl_address(uint8_t reg) { chip.write_address(reg); }
extern "C" void host_opl_data(uint8_t value) { chip.write_data(value); }
extern "C" int32_t host_opl_sample(void)
{
    ymfm::ym3812::output_data output;
    chip.generate(&output);
    return output.data[0];
}
extern "C" void host_opl_advance(uint32_t samples)
{
    while (samples--)
        host_opl_sample();
}
