/* Exercise the real, rewritten treasure chooser with scripted UI input.
   Long captions must survive returning through the stack protector. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern short item_or_loot_dialog(int, int, int, int, int, char *);
short dialog_selection, dialog_button, show_area_bottom;
short party_redraw_interval, space_key_hit, hotspot_hit, hotspot_rects[80];
char treasure_item_dialog_temp_file[] = "test.tmp";
static int title, chosen_button, polls, labels, restored;
static const char *expected[3];

short aztec_strcpy(char *dest, char *src) { strcpy(dest, src); return 0; }
short load_message_string(int number, char *dest)
{
    assert(number >= title && number <= title+2);
    strcpy(dest, expected[number-title]);
    return 0;
}
short draw_button(int x, int y, int w, int h, int boxed, char *label)
{
    assert(labels < 3 && strcmp(label, expected[labels]) == 0);
    ++labels;
    return 0;
}
short poll_hotspots(short *rects)
{
    assert(rects == hotspot_rects && polls < 2);
    hotspot_hit = 1;
    /* Select the first row, then Take/Leave or Use/Discard/Cancel. */
    return polls++ == 0 ? 7 : chosen_button+4;
}
short save_screen_area(int x, int y, int right, int bottom, char *file) { return 7; }
short restore_saved_screen(int id, int mode)
{
    assert(id == 7 && mode == 1);
    ++restored;
    return 0;
}
short draw_dialog_box(int a, int b, int c, int d) { return 0; }
short draw_inset_list_rows(int a, int b, int c, int d, int e, int f, int g) { return 0; }
short draw_scrollbar(int a, int b, int c, int d, int e) { return 0; }
short set_hotspot(int a, int b, int c, int d, int e, int f, int g) { return 0; }
short call_show_area(int a, int b, int c, int d) { return 0; }
short tick_treasure_animation(void) { return 0; }
short call_get_mouse(short *x, short *y) { *x = *y = 0; return 0; }
short clamp_between(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}
short flash_area(int a, int b, int c, int d, int e) { return 0; }
short flash_button(int a, int b, int c, int d, int e) { return 0; }

int main(void)
{
    for (int use_item = 0; use_item <= 1; ++use_item) {
        title = use_item ? 2030 : 5100;
        expected[0] = use_item ? "USE" : "DIVVY LOOT";
        expected[1] = use_item ? "DISCARD" : "TAKE";
        expected[2] = use_item ? "CANCEL" : "LEAVE";
        for (chosen_button = use_item ? 0 : 1; chosen_button < 3; ++chosen_button) {
            for (int rows = 1; rows <= 6; ++rows) {
                polls = labels = restored = 0;
                assert(item_or_loot_dialog(title, use_item, 1, 1, rows, "DIVVY LOOT") == chosen_button);
                assert(dialog_selection == 0 && dialog_button == chosen_button);
                assert(labels == 3 && restored == 1 && show_area_bottom == 143);
            }
        }
    }
    puts("treasure dialogs: Take/Leave and Use/Discard/Cancel passed (1..6 rows)");
    return 0;
}
