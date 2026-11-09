/* Copyright (C) 1983, 1985 by Manx Software Systems */
/* strncat copies at most len bytes of str2 and always ends with a nul. */

char *
aztec_strncat(str1, str2, len)
char *str1, *str2; unsigned len;
{
	register char *d = str1;

	while (*d)
		++d;
	for (; len && *str2; --len)
		*d++ = *str2++;
	*d = 0;
	return str1;
}

char *
aztec_strcat(a, b)
char *a, *b;
{
	return aztec_strncat(a, b, 0x7fff);
}
