#include <ruby.h>

static VALUE
fnum_popcount(VALUE fnum)
{
  int bits = __builtin_popcount(FIX2INT(fnum));
  return INT2FIX(bits);
}

static VALUE
bnum_popcount(VALUE bnum)
{
  BDIGIT *digits = RBIGNUM_DIGITS(bnum);
  size_t  length = RBIGNUM_LEN(bnum);
  int     bits = 0;

  while (length--)
    bits += __builtin_popcount(*digits++);

  return INT2FIX(bits);
}

void Init_popcount(void)
{
  rb_define_method(rb_cFixnum, "popcount", fnum_popcount, 0);
  rb_define_method(rb_cBignum, "popcount", bnum_popcount, 0);
}