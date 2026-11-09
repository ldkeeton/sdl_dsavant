/* Copyright (C) 1983 by Manx Software Systems */

aztec_strlen(str1)
char *str1;
{
	register char *p = str1;

	while (*p)
		++p;
	return p - str1;
}
