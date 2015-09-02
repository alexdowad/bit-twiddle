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

static inline void
store_64_into_bnum(VALUE bnum, uint64_t int64)
{
  BDIGIT *dest = RBIGNUM_DIGITS(bnum);
  size_t  len  = RBIGNUM_LEN(bnum);

#if SIZEOF_BDIGIT == 8
  *dest = int64;
#else
  if (len > 1) {
    *dest     = int64;
    *(dest+1) = int64 >> 32;
  } else if (int64 & (0xFFFFFFFFULL << 32) == 0) {
    /* the high 4 bytes are zero anyways */
    *dest = int64;
  } else {
    rb_big_resize(bnum, 2);
    dest      = RBIGNUM_DIGITS(bnum); /* may have moved */
    *dest     = int64;
    *(dest+1) = int64 >> 32;
  }
#endif
}

static inline uint64_t
load_64_from_bignum(VALUE bnum)
{
  BDIGIT *src = RBIGNUM_DIGITS(bnum);
  size_t  len = RBIGNUM_LEN(bnum);
  uint64_t result = *src;

#if SIZEOF_BDIGIT == 4
  if (len > 1)
    result += ((unsigned long long)*(src+1)) << 32;
#endif

  return result;
}

static VALUE
fnum_popcount(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    value = -value;
  return LONG2FIX(__builtin_popcountl(value));
}

static VALUE
bnum_popcount(VALUE bnum)
{
  BDIGIT *digits = RBIGNUM_DIGITS(bnum);
  size_t  length = RBIGNUM_LEN(bnum);
  long    bits   = 0;

  while (length--) {
    bits += popcount_bdigit(*digits);
    digits++;
  }

  return LONG2FIX(bits);
}

static VALUE
fnum_lo_bit(VALUE fnum)
{
  return LONG2FIX(__builtin_ffsl(FIX2LONG(fnum)));
}

static VALUE
bnum_lo_bit(VALUE bnum)
{
  BDIGIT *digit = RBIGNUM_DIGITS(bnum);
  long    bits  = 0;

  while (!*digit) {
    digit++;
    bits += (sizeof(BDIGIT) * 8);
  }

  bits += ffs_bdigit(*digit);
  return LONG2FIX(bits);
}

static VALUE
fnum_hi_bit(VALUE fnum)
{
  long bits, value;
  if (fnum == fix_zero) return fix_zero;
  value = FIX2LONG(fnum);
  if (value < 0)
    value = -value;
  bits = __builtin_clzl(value);
  return LONG2FIX((sizeof(long) * 8) - bits);
}

static VALUE
bnum_hi_bit(VALUE bnum)
{
  BDIGIT *digit = RBIGNUM_DIGITS(bnum) + (RBIGNUM_LEN(bnum)-1);
  long    bits  = (sizeof(BDIGIT) * 8) * RBIGNUM_LEN(bnum);

  while (!*digit) {
    digit--;
    bits -= (sizeof(BDIGIT) * 8);
  }

  bits -= clz_bdigit(*digit);
  return LONG2FIX(bits);
}

static VALUE
fnum_bswap16(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFF) | __builtin_bswap16(value));
}

static VALUE
bnum_bswap16(VALUE bnum)
{
  VALUE  result = rb_big_clone(bnum);
  BDIGIT value  = *RBIGNUM_DIGITS(bnum);
  *RBIGNUM_DIGITS(result) = (value & ~0xFFFF) | __builtin_bswap16(value);
  return result;
}

static VALUE
fnum_bswap32(VALUE fnum)
{
  /* the size of a Fixnum is always the same as 'long'
   * and the C standard guarantees 'long' is at least 32 bits
   * but a couple bits are used for tagging, so the usable precision could
   * be less than 32 bits... */
#if SIZEOF_LONG == 4
  return LONG2NUM(__builtin_bswap32(FIX2LONG(fnum)));
#else
  return LONG2FIX(__builtin_bswap32(FIX2LONG(fnum)));
#endif
}

static VALUE
bnum_bswap32(VALUE bnum)
{
  VALUE result = rb_big_clone(bnum);

#if SIZEOF_BDIGIT < 4
#error "Help!!! This platform is seriously weird!! Bignum#bswap32 will not work!"
#endif

  *RBIGNUM_DIGITS(result) = __builtin_bswap32(*RBIGNUM_DIGITS(bnum));
  return result;
}

static VALUE
fnum_bswap64(VALUE fnum)
{
  return LL2NUM(__builtin_bswap64(FIX2LONG(fnum)));
}

static VALUE
bnum_bswap64(VALUE bnum)
{
  VALUE   result = rb_big_clone(bnum);
  uint64_t value = __builtin_bswap64(load_64_from_bignum(bnum));
  store_64_into_bnum(result, value);
  return result;
}

void Init_popcount(void)
{
  rb_define_method(rb_cFixnum, "popcount", fnum_popcount, 0);
  rb_define_method(rb_cBignum, "popcount", bnum_popcount, 0);

  rb_define_method(rb_cFixnum, "lo_bit",   fnum_lo_bit, 0);
  rb_define_method(rb_cBignum, "lo_bit",   bnum_lo_bit, 0);
  rb_define_method(rb_cFixnum, "hi_bit",   fnum_hi_bit, 0);
  rb_define_method(rb_cBignum, "hi_bit",   bnum_hi_bit, 0);

  rb_define_method(rb_cFixnum, "bswap16", fnum_bswap16, 0);
  rb_define_method(rb_cBignum, "bswap16", bnum_bswap16, 0);
  rb_define_method(rb_cFixnum, "bswap32", fnum_bswap32, 0);
  rb_define_method(rb_cBignum, "bswap32", bnum_bswap32, 0);
  rb_define_method(rb_cFixnum, "bswap64", fnum_bswap64, 0);
  rb_define_method(rb_cBignum, "bswap64", bnum_bswap64, 0);
}
