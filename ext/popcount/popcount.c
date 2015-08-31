#include <ruby.h>

#define fix_zero LONG2FIX(0L)

#if SIZEOF_BDIGIT == SIZEOF_LONG

#define popcount_bdigit __builtin_popcountl
#define ffs_bdigit      __builtin_ffsl
#define clz_bdigit      __builtin_clzl

#elif SIZEOF_BDIGIT == SIZEOF_INT

#define popcount_bdigit __builtin_popcount
#define ffs_bdigit      __builtin_ffs
#define clz_bdigit      __builtin_clz

#else
#error "What is the size of a Ruby Bignum digit on this platform???"
#endif

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
    bits += popcount_bdigit(*digits);
    digits++;
  }

  return LONG2FIX(bits);
}

void Init_popcount(void)
{
  rb_define_method(rb_cFixnum, "popcount", fnum_popcount, 0);
  rb_define_method(rb_cBignum, "popcount", bnum_popcount, 0);
}