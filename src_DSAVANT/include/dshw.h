/* State the DS hardware layer (ds/library) shares: the driver memory
   blocks, the loaded fonts, EMS and the mouse.  On DOS these were words
   in DS's code segment (ds/data/driver_tables.c). */

/* {paragraphs, segment} entries, between two zero entries; allocated by
   allocate_driver_blocks, in this order. */
extern unsigned driver_block_table[];
#define DRV_BLOCK_FIRST	2	/* word index of the first entry */
#define DRV_BLOCK_END	14	/* word index of the closing zero entry */
#define maze_area_seg	driver_block_table[3]	/* 4180h paragraphs */
#define screen_buf_seg	driver_block_table[5]	/* 64000-byte screen buffer */
#define font_header_seg	driver_block_table[7]	/* font headers are read here */
#define misc_table_seg	driver_block_table[9]	/* the 400h-byte misc table */
#define video_drv_seg	driver_block_table[11]	/* where VGA.DRV was loaded */
#define play_drv_seg	driver_block_table[13]	/* where PLAY.DRV was loaded */

/* The segment of each loaded font (0: none), then 0FFFFh. */
extern unsigned font_seg_table[];

extern unsigned ems_allocated_paragraphs, ems_handle, ems_page_frame_segment;
extern unsigned char mouse_button_count;

extern unsigned char saved_video_mode, saved_equip_byte;
extern unsigned char video_is_ega, video_is_vga, video_is_cga, video_is_herc;
