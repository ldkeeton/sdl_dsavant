/* The random generator: three 16-bit seeds, each with its own recurrence
   (stir_random_seeds, run on every key poll as well), combined into 15
   bits.  The arithmetic is the original's 16-bit signed and unsigned
   division and multiplication. */
#include "bios.h"

static int random_seed_a = 0x0bb8;
static int random_seed_b = 0x0001;
static int random_seed_c = 0x752f;

/* seed = k1 * (seed % d) - k2 * (seed / d), plus m when that is negative */
static int
stir(seed, d, k1, k2, m)
int seed, d, k1, k2; unsigned m;
{
	int r = seed % d, q = seed / d;
	int v = (int)(k1 * r) - (int)(k2 * q);

	if (v < 0)
		v += m;
	return v;
}

stir_random_seeds()
{
	random_seed_a = stir(random_seed_a, 177, 171, 2, 0x763d);
	random_seed_b = stir(random_seed_b, 176, 172, 35, 0x7663);
	random_seed_c = stir(random_seed_c, 178, 170, 63, 0x7673);
}

/* seed % m scaled to 0..9999, as unsigned 16-bit values */
static unsigned
scaled(seed, m)
unsigned seed, m;
{
	return (unsigned)((unsigned long)(seed % m) * 10000 / m) & 0x7fff;
}

random_15bit()
{
	unsigned sum;

	stir_random_seeds();
	sum = scaled(random_seed_a, 0x763d);
	sum = (sum + scaled(random_seed_b, 0x7663)) & 0x7fff;
	return (sum + scaled(random_seed_c, 0x7673)) & 0x7fff;
}

/* Seeds b from the BIOS tick (init_system_hardware). */
seed_random_from_timer()
{
	random_seed_b = (int)BIOS_TICKS + 2;
}
