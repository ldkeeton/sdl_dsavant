/* Copyright (C) 1983, 1985 by Manx Software Systems */

char *
aztec_strcpy(str1, str2)
char *str1, *str2;
{
	register char *d = str1;

	while ((*d++ = *str2++) != 0)
		;
	return str1;
}
