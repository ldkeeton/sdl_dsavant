/* Active OPL routines from PLAY. Register numbers and byte overflow match
   the DOS code, including channel 0 reserved for digitized samples. */
#include "play.h"

uint8_t opl_register_shadow[256];
uint8_t opl_channel_index, opl_note_index, opl_note_velocity, opl_note_semitone;
uint16_t opl_pitch_value = 0x2000;
static const uint8_t operators[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};

void play_opl_write(uint8_t reg, uint8_t value)
{
    opl_register_shadow[reg] = value;
    host_opl_address(reg);
    host_opl_data(value);
    host_opl_address(0x40);
}

void play_opl_keys_off(void)
{
    uint8_t i, reg;
    for (i = 0; i <= 8; ++i) {
        reg = (play_device == 1 || play_device == 4 ? 0xb1 : 0xb0) + i;
        play_opl_write(reg, opl_register_shadow[reg] & 0xdf);
    }
}

void play_opl_init(void)
{
    play_opl_keys_off();
    play_opl_write(1, 0x20);
    play_opl_write(0xbd, 0);
    play_opl_write(4, 6);
}

static uint16_t patch_base(void)
{
    uint8_t patch = *DOS_PTR(music_patch_segment,
                            music_patch_offset + (opl_note_index >> 2));
    return music_patch_offset + 0x40 + patch * 11;
}

static uint8_t patch_byte(uint16_t base, uint8_t offset)
{
    return *DOS_PTR(music_patch_segment, base + offset);
}

void play_opl_load_patch(void)
{
    static const uint8_t pairs[][2] = {
        {0,0x20}, {2,0x60}, {3,0x80}, {4,0xe0},
        {5,0x23}, {7,0x63}, {8,0x83}, {9,0xe3}
    };
    uint16_t base = patch_base() + (opl_note_index & 3) * 11;
    uint8_t op = operators[opl_channel_index + 1];
    uint8_t i;
    play_opl_note_off();
    for (i = 0; i < 8; ++i)
        play_opl_write(pairs[i][1] + op, patch_byte(base, pairs[i][0]));
    play_opl_write(0xc1 + opl_channel_index, patch_byte(base, 10));
}

static void operator_level(uint8_t op, uint8_t attenuation)
{
    uint8_t scale = ((uint16_t)playback_volume * (uint8_t)(opl_note_velocity * 2 + 1)) >> 8;
    uint8_t level = ((uint16_t)((~attenuation) & 63) * scale) >> 8;
    play_opl_write(0x40 + op, ((~level) & 63) | (attenuation & 0xc0));
}

void play_opl_note_on(void)
{
    uint16_t base = patch_base();
    uint8_t op = operators[opl_channel_index + 1];
    int16_t pitch, fnum;
    int8_t block;
    uint16_t semitone;
    operator_level(op + 3, patch_byte(base, 6));
    if (patch_byte(base, 10) & 1)
        operator_level(op, patch_byte(base, 1));
    else
        play_opl_write(0x40 + op, patch_byte(base, 1));

    /* With the fixed zero pitch range, DOS's computed jump lands two
       bytes into add ah,cs:[semitone], decoding as ES CMP AL,0eh. It
       consequently skips the semitone addition for a noncentered bend.
       The shipped scores all keep the centered value. */
    pitch = (int16_t)(opl_pitch_value - 0x2000);
    if (pitch)
        pitch >>= 5;
    else
        pitch = (uint16_t)opl_note_semitone << 8;
    pitch = (int16_t)(pitch + 8) >> 4;
    if (pitch < 0) pitch = 0;
    if (pitch >= 0x5ff) pitch = 0x5ff;
    semitone = (uint16_t)pitch >> 4;
    fnum = opl_fnum_table[(semitone % 12) * 16 + (pitch & 15)];
    block = semitone / 12 - 1;
    if (fnum < 0) ++block;
    if (block < 0) {
        ++block;
        fnum >>= 1;
    }
    play_opl_write(0xa1 + opl_channel_index, (uint8_t)fnum);
    play_opl_write(0xb1 + opl_channel_index,
                   (((uint16_t)fnum >> 8) & 3) + (uint8_t)(block * 4) + 0x20);
}

void play_opl_note_off(void)
{
    uint8_t reg = 0xb1 + opl_channel_index;
    play_opl_write(reg, opl_register_shadow[reg] & 0xdf);
}

void play_opl_pulse(void)
{
    play_opl_write(0x20, 0x21);
    play_opl_write(0x60, 0xf0);
    play_opl_write(0x80, 0xf0);
    play_opl_write(0xc0, 1);
    play_opl_write(0xe0, 0);
    play_opl_write(0x43, 0x3f);
    play_opl_write(0xb0, 0);
    play_opl_write(0xa0, 0);
    play_opl_write(0xa0, 0x8f);
    play_opl_write(0xb0, 0x2e);
    /* PIT mode 3 counts down by two: 0952h counts is about 1 ms.
       Keep key-on set while freezing the waveform at F-number zero. */
    host_opl_advance(50);
    play_opl_write(0xb0, 0x20);
    play_opl_write(0xa0, 0);
    host_opl_address(0x40);
    host_opl_data(0x3f);
}
