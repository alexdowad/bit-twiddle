#include <ruby.h>

static VALUE
fnum_popcount(VALUE fnum)
{
  long bits = __builtin_popcountl(FIX2LONG(fnum));
  return LONG2FIX(bits);
}

static VALUE
bnum_popcount(VALUE bnum)
{
  BDIGIT *digits = RBIGNUM_DIGITS(bnum);
  size_t  length = RBIGNUM_LEN(bnum);
  long    bits = 0;

  while (length--) {
    bits += __builtin_popcountl((long)*digits);
    digits++;
  }

  return LONG2FIX(bits);
}

void Init_popcount(void)
{
  rb_define_method(rb_cFixnum, "popcount", fnum_popcount, 0);
  rb_define_method(rb_cBignum, "popcount", bnum_popcount, 0);
}