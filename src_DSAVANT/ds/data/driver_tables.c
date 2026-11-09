/* Driver and table blocks (see dshw.h), the fonts loaded, EMS and mouse
   state.  load_video_driver copies the block table and the rest into the
   video driver. */
unsigned driver_block_table[16] = {
	0, 0,			/* where free_driver_blocks stops */
	0x4180, 0,		/* maze area */
	0x1000, 0,		/* screen buffer */
	0x0140, 0,		/* font header */
	0x0040, 0,		/* misc table */
	0x0530, 0,		/* VGA.DRV */
	0x01d0, 0,		/* PLAY.DRV */
	0, 0
};

unsigned font_seg_table[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xffff};

unsigned ems_allocated_paragraphs;
unsigned ems_handle;
unsigned ems_page_frame_segment;
unsigned char mouse_button_count;

unsigned char saved_video_mode, saved_equip_byte;
unsigned char video_is_ega, video_is_vga, video_is_cga, video_is_herc;
