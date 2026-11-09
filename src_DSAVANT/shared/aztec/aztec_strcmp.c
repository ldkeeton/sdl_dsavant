/* Copyright (C) 1983, 1985 by Manx Software Systems */
/* strcmp and strncmp: -1, 0 or 1, comparing bytes as unsigned. */

aztec_strcmp(left, right)
char *left, *right;
{
	return aztec_strncmp(left, right, 0xffff);
}

aztec_strncmp(left, right, limit)
char *left, *right; unsigned limit;
{
	register unsigned char *l = (unsigned char *)left;
	register unsigned char *r = (unsigned char *)right;

	for (; limit; --limit, ++l, ++r) {
		if (*l != *r)
			return *l < *r ? -1 : 1;
		if (*l == 0)
			break;
	}
	return 0;
}
