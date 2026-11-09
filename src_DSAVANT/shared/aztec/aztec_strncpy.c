/* Copyright (C) 1983, 1985 by Manx Software Systems */
/* Copies at most len bytes; a shorter source is padded with nuls. */

char *
aztec_strncpy(str1, str2, len)
char *str1, *str2; unsigned len;
{
	register char *d = str1;

	for (; len && *str2; --len)
		*d++ = *str2++;
	for (; len; --len)
		*d++ = 0;
	return str1;
}
