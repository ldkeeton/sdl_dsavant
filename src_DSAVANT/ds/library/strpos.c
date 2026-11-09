/* The offset of c in str, or -1. */
strpos(str, c)
char *str;
{
	register int i;

	for (i = 0; str[i]; i++)
		if (str[i] == (char)c)
			return i;
	return -1;
}
