/* Filtered key queue (get_filtered_key, poll_filtered_key, flush_keyboard):
   a count, then the keys, oldest first. */
struct {
	unsigned count;
	unsigned char keys[22];
} key_queue;
