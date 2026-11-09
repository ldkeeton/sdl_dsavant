/* Copyright (C) Manx Software Systems, Inc.  1987 */
/* Host version of Aztec's printf engine: calls putsub for each character
   of fmt with the conversions done, and returns how many it emitted.  The
   arguments come from a va_list (a game int arrives promoted, as a host
   int: a game long).  Conversions: d i u o x X c s %, with the flags
   - + space # 0, a width and a precision (either may be *), and l or h.
   No floating point, as in the DOS build (which was not linked with -lm). */

static char digits[] = "0123456789abcdef";

static int count;
static int (*put)();

static
emit(c)
{
	(*put)(c & 255);
	++count;
}

static
pad(c, n)
{
	while (n-- > 0)
		emit(c);
}

aztec_format(int (*putsub)(), char *fmt, __builtin_va_list *ap)
{
	char buffer[40], *s, *p, sign;
	int width, precision, left, alt, islong, len, zeros;
	char fill, pos_sign;
	unsigned long v;
	unsigned base;
	int c, upper;

	put = putsub;
	count = 0;
	while ((c = *fmt++) != 0) {
		if (c != '%') {
			emit(c);
			continue;
		}
		left = alt = islong = 0;
		fill = ' ';
		pos_sign = 0;
		for (;; ++fmt) {
			if (*fmt == '-')
				left = 1;
			else if (*fmt == '+')
				pos_sign = '+';
			else if (*fmt == ' ') {
				if (!pos_sign)
					pos_sign = ' ';
			} else if (*fmt == '#')
				alt = 1;
			else if (*fmt == '0')
				fill = '0';
			else
				break;
		}
		width = 0;
		if (*fmt == '*') {
			width = __builtin_va_arg(*ap, long);
			++fmt;
		} else
			while (*fmt >= '0' && *fmt <= '9')
				width = width * 10 + *fmt++ - '0';
		precision = -1;
		if (*fmt == '.') {
			precision = 0;
			if (*++fmt == '*') {
				precision = __builtin_va_arg(*ap, long);
				++fmt;
			} else
				while (*fmt >= '0' && *fmt <= '9')
					precision = precision * 10 + *fmt++ - '0';
		}
		for (;; ++fmt) {
			if (*fmt == 'l' || *fmt == 'L')
				islong = 1;
			else if (*fmt != 'h' && *fmt != 'F' && *fmt != 'N')
				break;
		}
		sign = 0;
		upper = 0;
		switch (c = *fmt++) {
		case 0:
			--fmt;
			continue;
		case 'c':
			buffer[0] = __builtin_va_arg(*ap, long);
			s = buffer;
			len = 1;
			goto output;
		case 's':
			s = __builtin_va_arg(*ap, char *);
			if (s == 0)
				s = "(null)";
			for (len = 0; s[len] && (precision < 0 || len < precision); ++len)
				;
			goto output;
		case 'd':
		case 'i':
			v = __builtin_va_arg(*ap, long);
			if (!islong)
				v = (int)v;
			if ((long)v < 0) {
				sign = '-';
				v = -(long)v;
			} else
				sign = pos_sign;
			base = 10;
			break;
		case 'u':
			base = 10;
			goto get_unsigned;
		case 'o':
			base = 8;
			goto get_unsigned;
		case 'X':
			upper = 1;
			/* fall through */
		case 'x':
			base = 16;
		get_unsigned:
			v = __builtin_va_arg(*ap, long);
			if (!islong)
				v = (unsigned)v;
			break;
		default:
			emit(c);
			continue;
		}
		/* a number: v in base */
		p = buffer + sizeof buffer;
		while (v) {
			c = digits[v % base];
			*--p = upper && c >= 'a' ? c - 'a' + 'A' : c;
			v /= base;
		}
		len = buffer + sizeof buffer - p;
		zeros = precision > len ? precision - len : 0;
		if (precision < 0 && len == 0)
			zeros = 1;
		if (alt && base == 8 && zeros == 0 && (len == 0 || *p != '0'))
			zeros = 1;
		s = p;
		if (precision >= 0)
			fill = ' ';
		width -= (sign != 0) + (alt && base == 16 && len ? 2 : 0) + zeros;
		if (!left && fill == ' ')
			pad(' ', width - len);
		if (sign)
			emit(sign);
		if (alt && base == 16 && len) {
			emit('0');
			emit(upper ? 'X' : 'x');
		}
		if (!left && fill == '0')
			pad('0', width - len);
		pad('0', zeros);
		while (len-- > 0)
			emit(*s++);
		if (left)
			pad(' ', width - (s - p));
		continue;
	output:
		width -= len;
		if (!left)
			pad(' ', width);
		while (len-- > 0)
			emit(*s++);
		if (left)
			pad(' ', width);
	}
	return count;
}
