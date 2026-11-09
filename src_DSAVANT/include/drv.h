/* The video driver's state (shared/drv and vga/).  On DOS this was VGA.DRV,
   loaded at run time; its data lived in its code segment and
   load_video_driver copied DS's driver block table, palette pointer, delay
   hook and EMS state into it.  Linked in, the driver uses DS's own (dshw.h)
   under its old names. */
#include "dshw.h"
#include "dosmem.h"

/* What load_video_driver used to copy in. */
#define maze_mem_seg	maze_area_seg		/* 4180h paragraphs: pictures, mazes, screens, sounds, scores */
#define scratch_buf_seg	font_header_seg		/* 1400h-byte work buffer */
#define ems_para_limit	ems_allocated_paragraphs
#define ems_mapping_handle	ems_handle
#define mouse_init_enabled	mouse_button_count
#define MAZE_MEM_PARAS	0x4180

extern unsigned char vga_palette[];	/* palette_ptr: the game's 256 RGB entries */

/* Blocks in the maze area: {paragraph offset, paragraphs}, numbered from 1
   in allocation order and kept packed (release_maze_block moves the later
   ones down).  Every slot table below holds block numbers (0: empty). */
extern unsigned maze_blocks[];
extern unsigned maze_block_count, maze_paras_used;
extern unsigned picture_slots[];	/* 20 */
extern unsigned maze_map_slots[];	/* 10, load_maze */
extern unsigned screen_maze_slots[];	/* 5, save_screen/load_screen */
extern unsigned maze_slots[];		/* 16, cache_area mode 2 */
extern unsigned sound_slots[];		/* 20 x {block, length} */
extern unsigned score_slots[];		/* 5 x {block, payload offset} */
extern unsigned score_info[5];
extern unsigned maze_piece_table;	/* offset of a maze's piece table: 4 */
extern unsigned maze_piece_descriptors[10];	/* per maze: offset of its piece placements */

/* EMS blocks, the same way, in paragraphs from the start of the handle. */
extern unsigned ems_blocks[];
extern unsigned ems_block_count, ems_paras_used;
extern unsigned ems_slots[20];		/* cache_area modes 0, 1 */
extern unsigned cache_maze_slot_table[10];	/* cache_maze */
extern unsigned screen_ems_slots[10];	/* cache_screen_to_storage */

/* Text. */
extern int primary_text_color, secondary_text_color;
extern int text_pos_x, text_pos_y, text_opaque;
extern unsigned char text_state[];	/* draw_text's column masks, then its row masks */
#define text_state_words	text_state
#define text_state_bytes	(text_state + 40)
extern unsigned char intensity_limits[256];

/* Mouse. */
extern unsigned char mouse_glide_active, mouse_redraw_pending, mouse_present;
extern int mouse_drawn_x, mouse_drawn_y;
extern unsigned mouse_click_pending;
extern int mouse_click_x, mouse_click_y;

/* Palette. */
extern unsigned char rgb_to_palette[];	/* 3-bit r, g, b to a palette index; 0FFh none */

/* The host address of a paragraph in the maze area, and of one in EMS (the
   handle's pages are contiguous, so there is no page frame to map). */
#define MAZE_PTR(para, off)	DOS_PTR(maze_mem_seg + (para), off)
#define EMS_PTR(para)	(ems_page(ems_mapping_handle, 0) + ((long)(unsigned)(para) << 4))
#define SCREEN_BUF	DOS_PTR(screen_buf_seg, 0)
