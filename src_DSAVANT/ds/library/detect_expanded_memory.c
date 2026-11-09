/* Takes up to 24 pages (384 KB) of EMS and records the handle, the size in
   paragraphs and the page frame.  The host's EMS is always present. */
#include "dosmem.h"
#include "dshw.h"

#define MAX_PAGES	0x18

detect_expanded_memory()
{
	unsigned total, pages;
	long handle;

	ems_page_counts(&total, &pages);
	if (pages > MAX_PAGES)
		pages = MAX_PAGES;
	ems_allocated_paragraphs = pages;
	handle = pages ? ems_alloc(pages) : -1;
	if (handle >= 0) {
		ems_handle = handle;
		ems_allocated_paragraphs = pages << 10;
		ems_page_frame_segment = EMS_FRAME_SEGMENT;
	}
}

/* Gives the pages back (on exit). */
release_expanded_memory()
{
	if (ems_handle)
		ems_free(ems_handle);
}
