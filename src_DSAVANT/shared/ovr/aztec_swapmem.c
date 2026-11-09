/* Copyright (C) 1984 by Manx Software Systems */
/* Exchanges len bytes at str1 and str2. */

aztec_swapmem(str1, str2, len)
char *str1, *str2; unsigned len;
{
	register char t;

	if (str1 == str2)
		return;
	while (len--) {
		t = *str1;
		*str1++ = *str2;
		*str2++ = t;
	}
}
