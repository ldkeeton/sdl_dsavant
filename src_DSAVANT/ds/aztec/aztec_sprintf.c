/* Copyright (C) 1982 by Manx Software Systems */
/* Host version: the arguments are a va_list, not the stack after fmt. */
static char *buff;
static aztec_spsub();

aztec_sprintf(char *str, char *fmt, ...)
{
	register int i;
	__builtin_va_list ap;

	buff = str;
	__builtin_va_start(ap, fmt);
	i = aztec_format(aztec_spsub, fmt, &ap);
	__builtin_va_end(ap);
	*buff = 0;
	return i;
}

static
/* Appends one formatted character to the output buffer. */
aztec_spsub(c)
{
	return (*buff++ = c)&255;
}
