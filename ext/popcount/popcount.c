#include <ruby.h>

#define fix_zero LONG2FIX(0L)
#define BIGNUM_P(x) RB_TYPE_P((x), T_BIGNUM)

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

#if SIZEOF_BDIGIT < 4
#error "Sorry, Bignum#bswap32 and Bignum#sar32 will not work with sizeof(BDIGIT) < 4. Please report this error."
#elif SIZEOF_BDIGIT > 8
#error "Sorry, several methods will not work if sizeof(BDIGIT) > 8. Please report this error."
#elif SIZEOF_LONG > 8
#error "Sorry, Fixnum#sar64 will not work with sizeof(long) > 8. Please report this error."
#endif

static inline VALUE
bigfixize(VALUE x)
{
  size_t n   = RBIGNUM_LEN(x);
  BDIGIT *ds = RBIGNUM_DIGITS(x);
  ulong u;

  while (n && ds[n-1] == 0)
    n--;

  if (n == 0) {
    return INT2FIX(0);
  } else if (n == 1) {
    u = ds[0];
  } else if (SIZEOF_BDIGIT == 4 && SIZEOF_LONG == 8 && n == 2) {
    u = ds[0] + ((ulong)ds[1] << 32);
  } else {
    goto return_big;
  }

  if (RBIGNUM_POSITIVE_P(x)) {
      if (POSFIXABLE(u))
        return LONG2FIX((long)u);
  } else if (u <= -FIXNUM_MIN) {
    return LONG2FIX(-(long)u);
  }

return_big:
  rb_big_resize(x, n);
  return x;
}

static inline int
bnum_greater(VALUE bnum, BDIGIT value)
{
  BDIGIT *digits = RBIGNUM_DIGITS(bnum);
  size_t  len    = RBIGNUM_LEN(bnum);
  if (*digits > value)
    return 1;
  while (--len)
    if (*++digits > 0)
      return 1;
  return 0;
}

static inline long
value_to_shiftdist(VALUE shiftdist, long bits)
{
  for (;;) {
    if (BIGNUM_P(shiftdist)) {
      long sdist;
      if (bnum_greater(shiftdist, bits-1))
        sdist = bits;
      else
        sdist = *RBIGNUM_DIGITS(shiftdist);
      if (RBIGNUM_NEGATIVE_P(shiftdist))
        sdist = -sdist;
      return sdist;
    } else if (FIXNUM_P(shiftdist)) {
      return FIX2LONG(shiftdist);
    } else {
      shiftdist = rb_to_int(shiftdist);
    }
  }
}

/* 'mask' is 0x7 for 8, 0xF for 16, 0x1F for 32, 0x3F for 64
 * return value is always positive! */
static inline ulong
value_to_rotdist(VALUE rotdist, long bits, long mask)
{
  for (;;) {
    long rdist;
    if (BIGNUM_P(rotdist)) {
      rdist = *RBIGNUM_DIGITS(rotdist) & mask;
      if (RBIGNUM_NEGATIVE_P(rotdist))
        rdist = bits - rdist;
      return rdist;
    } else if (FIXNUM_P(rotdist)) {
      rdist = FIX2LONG(rotdist) % bits;
      if (rdist < 0)
        rdist += bits;
      return rdist;
    } else {
      rotdist = rb_to_int(rotdist);
    }
  }
}

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

static inline BDIGIT
modify_lo32_in_bdigit(BDIGIT digit, uint32_t lo32)
{
#if SIZEOF_BDIGIT == 32
  return lo32;
#else
  return (digit & ~0xFFFFFFFF) | lo32;
#endif
}

static inline VALUE
modify_lo32_in_bignum(VALUE bnum, uint32_t lo32)
{
  BDIGIT *digit = RBIGNUM_DIGITS(bnum);
  BDIGIT  value = modify_lo32_in_bdigit(*digit, lo32);
  VALUE   result;

/* if a 'long' is only 4 bytes, a 32-bit number could be promoted to Bignum
 * then modifying the low 32 bits could make it fixable again */
#if SIZEOF_LONG == 4
  if (FIXABLE(value))
    return LONG2FIX(value);
#endif

  result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = value;
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
#elif SIZEOF_LONG >= 8
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFFFFFF) | __builtin_bswap32(value));
#endif
}

static VALUE
bnum_bswap32(VALUE bnum)
{
  return modify_lo32_in_bignum(bnum, __builtin_bswap32(*RBIGNUM_DIGITS(bnum)));
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

static VALUE
fnum_rrot8(VALUE fnum, VALUE rotdist)
{
  long  value  = FIX2LONG(fnum);
  ulong rotd   = value_to_rotdist(rotdist, 8, 0x7);
  ulong lobyte = value & 0xFF;
  lobyte = ((lobyte >> rotd) | (lobyte << (-rotd & 7))) & 0xFF;
  return LONG2FIX((value & ~0xFF) | lobyte);
}

static VALUE
bnum_rrot8(VALUE bnum, VALUE rotdist)
{
  VALUE   result = rb_big_clone(bnum);
  BDIGIT *src    = RBIGNUM_DIGITS(bnum);
  BDIGIT *dest   = RBIGNUM_DIGITS(result);
  BDIGIT  value  = *src;
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  ulong   lobyte = value & 0xFF;
  lobyte = ((lobyte >> rotd) | (lobyte << (-rotd & 7))) & 0xFF;
  *dest = ((value & ~0xFF) | lobyte);
  return result;
}

static VALUE
fnum_rrot16(VALUE fnum, VALUE rotdist)
{
  long  value  = FIX2LONG(fnum);
  ulong rotd   = value_to_rotdist(rotdist, 16, 0xF);
  ulong loword = value & 0xFFFF;
  loword = ((loword >> rotd) | (loword << (-rotd & 15))) & 0xFFFF;
  return LONG2FIX((value & ~0xFFFF) | loword);
}

static VALUE
bnum_rrot16(VALUE bnum, VALUE rotdist)
{
  VALUE   result = rb_big_clone(bnum);
  BDIGIT *src    = RBIGNUM_DIGITS(bnum);
  BDIGIT *dest   = RBIGNUM_DIGITS(result);
  BDIGIT  value  = *src;
  ulong   rotd   = value_to_rotdist(rotdist, 16, 0xF);
  ulong   loword = value & 0xFFFF;
  loword = ((loword >> rotd) | (loword << (-rotd & 15))) & 0xFFFF;
  *dest = ((value & ~0xFFFF) | loword);
  return result;
}

static VALUE
fnum_rrot32(VALUE fnum, VALUE rotdist)
{
  long     value  = FIX2LONG(fnum);
  ulong    rotd   = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32   = value;
  lo32 = (lo32 >> rotd) | (lo32 << (-rotd & 31));
#if SIZEOF_LONG >= 8
  return LONG2FIX((value & ~0xFFFFFFFF) | lo32);
#elif SIZEOF_LONG == 4
  return LONG2NUM(lo32);
#endif
}

static VALUE
bnum_rrot32(VALUE bnum, VALUE rotdist)
{
  ulong    rotd = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  lo32 = (lo32 >> rotd) | (lo32 << (-rotd & 31));
  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_rrot64(VALUE fnum, VALUE rotdist)
{
  ulong    rotd  = value_to_rotdist(rotdist, 64, 0x3F);
  uint64_t value = FIX2ULONG(fnum);
  return ULL2NUM((value >> rotd) | (value << (-rotd & 63)));
}

static VALUE
bnum_rrot64(VALUE bnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 64, 0x3F);
  VALUE    result = rb_big_clone(bnum);
  uint64_t value  = load_64_from_bignum(bnum);
  value = ((value >> rotd) | (value << (-rotd & 63)));
  store_64_into_bnum(result, value);
  return bigfixize(result);
}

static VALUE
fnum_lrot8(VALUE fnum, VALUE rotdist)
{
  ulong rotd   = value_to_rotdist(rotdist, 8, 0x7);
  long  value  = FIX2LONG(fnum);
  ulong lobyte = value & 0xFF;
  lobyte = ((lobyte << rotd) | (lobyte >> (-rotd & 7))) & 0xFF;
  return LONG2FIX((value & ~0xFF) | lobyte);
}

static VALUE
bnum_lrot8(VALUE bnum, VALUE rotdist)
{
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  VALUE   result = rb_big_clone(bnum);
  BDIGIT *src    = RBIGNUM_DIGITS(bnum);
  BDIGIT *dest   = RBIGNUM_DIGITS(result);
  BDIGIT  value  = *src;
  ulong   lobyte = value & 0xFF;
  lobyte = ((lobyte << rotd) | (lobyte >> (-rotd & 7))) & 0xFF;
  *dest  = (value & ~0xFF) | lobyte;
  return result;
}

static VALUE
fnum_lrot16(VALUE fnum, VALUE rotdist)
{
  ulong rotd   = value_to_rotdist(rotdist, 16, 0xF);
  long  value  = FIX2LONG(fnum);
  ulong loword = value & 0xFFFF;
  loword = ((loword << rotd) | (loword >> (-rotd & 15))) & 0xFFFF;
  return LONG2FIX((value & ~0xFFFF) | loword);
}

static VALUE
bnum_lrot16(VALUE bnum, VALUE rotdist)
{
  ulong   rotd   = value_to_rotdist(rotdist, 16, 0xF);
  VALUE   result = rb_big_clone(bnum);
  BDIGIT *src    = RBIGNUM_DIGITS(bnum);
  BDIGIT *dest   = RBIGNUM_DIGITS(result);
  BDIGIT  value  = *src;
  ulong   loword = value & 0xFFFF;
  loword = ((loword << rotd) | (loword >> (-rotd & 15))) & 0xFFFF;
  *dest  = (value & ~0xFFFF) | loword;
  return result;
}

static VALUE
fnum_lrot32(VALUE fnum, VALUE rotdist)
{
  long     value = FIX2LONG(fnum);
  ulong    rotd  = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32  = value;
  lo32 = (lo32 << rotd) | (lo32 >> (-rotd & 31));
  return LONG2FIX((value & ~0xFFFFFFFF) | lo32);
}

static VALUE
bnum_lrot32(VALUE bnum, VALUE rotdist)
{
  ulong    rotd = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  lo32  = (lo32 << rotd) | (lo32 >> (-rotd & 31));
  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_lrot64(VALUE fnum, VALUE rotdist)
{
  ulong    rotd = value_to_rotdist(rotdist, 64, 0x3F);
  uint64_t val  = FIX2ULONG(fnum);
  return ULL2NUM((val << rotd) | (val >> (-rotd & 63)));
}

static VALUE
bnum_lrot64(VALUE bnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 64, 0x3F);
  VALUE    result = rb_big_clone(bnum);
  uint64_t value  = load_64_from_bignum(bnum);
  value = ((value << rotd) | (value >> (-rotd & 63)));
  store_64_into_bnum(result, value);
  return bigfixize(result);
}

static VALUE
fnum_shl32(VALUE fnum, VALUE shiftdist)
{
  long    value = FIX2LONG(fnum);
  long    sdist = value_to_shiftdist(shiftdist, 32);
  uint32_t lo32 = value;

  if (sdist >= 32 || sdist <= -32)
    return LONG2FIX(value & ~0xFFFFFFFFUL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFFFFFFFUL) | (lo32 >> ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFFFFFFFUL) | (lo32 << ((ulong)sdist)));
}

static VALUE
bnum_shl32(VALUE bnum, VALUE shiftdist)
{
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  long    sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist >= 32 || sdist <= -32)
    lo32 = 0;
  else if (sdist < 0)
    lo32 = lo32 >> ((ulong)-sdist);
  else
    lo32 = lo32 << ((ulong)sdist);

  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_shr32(VALUE fnum, VALUE shiftdist)
{
  long    value = FIX2LONG(fnum);
  uint32_t lo32 = value;
  long    sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist >= 32 || sdist <= -32)
    return LONG2FIX(value & ~0xFFFFFFFFUL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFFFFFFFUL) | (lo32 << ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFFFFFFFUL) | (lo32 >> ((ulong)sdist)));
}

static VALUE
bnum_shr32(VALUE bnum, VALUE shiftdist)
{
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  long    sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist >= 32 || sdist <= -32)
    lo32 = 0;
  else if (sdist < 0)
    lo32 = lo32 << ((ulong)-sdist);
  else
    lo32 = lo32 >> ((ulong)sdist);

  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_sar32(VALUE fnum, VALUE shiftdist)
{
#if SIZEOF_LONG == 4
  /* Not enough precision for 32nd bit to be a 1 */
  return fnum_shr32(fnum, shiftdist);
#else
  long    value = FIX2LONG(fnum);
  uint32_t lo32 = value;
  long    sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist < 0)
    return fnum_shl32(fnum, LONG2FIX(-sdist));

  if ((0x80000000UL & lo32) != 0) {
    if (sdist < 32)
      lo32 = (lo32 >> sdist) | ~(0xFFFFFFFFUL >> sdist);
    else
      lo32 = 0xFFFFFFFFUL;
  } else {
    if (sdist < 32)
      lo32 = lo32 >> sdist;
    else
      lo32 = 0;
  }

  return LONG2FIX((value & ~0xFFFFFFFFUL) | lo32);
#endif
}

static VALUE
bnum_sar32(VALUE bnum, VALUE shiftdist)
{
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  long    sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist < 0)
    return bnum_shl32(bnum, LONG2FIX(-sdist));

  if ((0x80000000UL & lo32) != 0) {
    if (sdist < 32 && sdist > -32)
      lo32 = (lo32 >> sdist) | ~(~0UL >> sdist);
    else
      lo32 = 0xFFFFFFFFUL;
  } else {
    if (sdist < 32 && sdist > -32)
      lo32 = lo32 >> sdist;
    else
      lo32 = 0;
  }

  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_shl64(VALUE fnum, VALUE shiftdist)
{
  uint64_t val  = FIX2ULONG(fnum);
  long    sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist >= 64 || sdist <= -64)
    return fix_zero;
  else if (sdist < 0)
    return ULL2NUM(val >> ((ulong)-sdist));
  else
    return ULL2NUM(val << ((ulong)sdist));
}

static VALUE
bnum_shl64(VALUE bnum, VALUE shiftdist)
{
  VALUE   result = rb_big_clone(bnum);
  uint64_t val   = load_64_from_bignum(bnum);
  long    sdist  = value_to_shiftdist(shiftdist, 64);

  if (sdist >= 64 || sdist <= -64)
    store_64_into_bnum(result, 0ULL);
  else if (sdist < 0)
    store_64_into_bnum(result, val >> ((ulong)-sdist));
  else
    store_64_into_bnum(result, val << ((ulong)sdist));

  return bigfixize(result);
}

static VALUE
fnum_shr64(VALUE fnum, VALUE shiftdist)
{
  uint64_t val  = FIX2ULONG(fnum);
  long    sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist >= 64 || sdist <= -64)
    return fix_zero;
  else if (sdist < 0)
    return ULL2NUM(val << ((ulong)-sdist));
  else
    return ULL2NUM(val >> ((ulong)sdist));
}

static VALUE
bnum_shr64(VALUE bnum, VALUE shiftdist)
{
  VALUE   result = rb_big_clone(bnum);
  uint64_t val   = load_64_from_bignum(bnum);
  long    sdist  = value_to_shiftdist(shiftdist, 64);

  if (sdist >= 64 || sdist <= -64)
    store_64_into_bnum(result, 0ULL);
  else if (sdist < 0)
    store_64_into_bnum(result, val << ((ulong)-sdist));
  else
    store_64_into_bnum(result, val >> ((ulong)sdist));

  return bigfixize(result);
}

static VALUE
fnum_sar64(VALUE fnum, VALUE shiftdist)
{
  /* Fixnum doesn't have enough precision that the 64th bit could be a 1
   * Or if it does, our preprocessor directives above make sure this won't compile */
  return fnum_shr64(fnum, shiftdist);
}

static VALUE
bnum_sar64(VALUE bnum, VALUE shiftdist)
{
  VALUE   result = rb_big_clone(bnum);
  uint64_t val   = load_64_from_bignum(bnum);
  long    sdist  = value_to_shiftdist(shiftdist, 64);

  if (sdist < 0)
    return bnum_shl64(bnum, LONG2FIX(-sdist));

  if ((0x8000000000000000ULL & val) != 0) {
    if (sdist < 64)
      store_64_into_bnum(result, (val >> sdist) | ~(~0ULL >> sdist));
    else
      store_64_into_bnum(result, ~0ULL);
  } else {
    if (sdist < 64)
      store_64_into_bnum(result, val >> sdist);
    else
      store_64_into_bnum(result, 0);
  }

  return bigfixize(result);
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

  rb_define_method(rb_cFixnum, "rrot8",  fnum_rrot8,  1);
  rb_define_method(rb_cBignum, "rrot8",  bnum_rrot8,  1);
  rb_define_method(rb_cFixnum, "rrot16", fnum_rrot16, 1);
  rb_define_method(rb_cBignum, "rrot16", bnum_rrot16, 1);
  rb_define_method(rb_cFixnum, "rrot32", fnum_rrot32, 1);
  rb_define_method(rb_cBignum, "rrot32", bnum_rrot32, 1);
  rb_define_method(rb_cFixnum, "rrot64", fnum_rrot64, 1);
  rb_define_method(rb_cBignum, "rrot64", bnum_rrot64, 1);

  rb_define_method(rb_cFixnum, "lrot8",  fnum_lrot8,  1);
  rb_define_method(rb_cBignum, "lrot8",  bnum_lrot8,  1);
  rb_define_method(rb_cFixnum, "lrot16", fnum_lrot16, 1);
  rb_define_method(rb_cBignum, "lrot16", bnum_lrot16, 1);
  rb_define_method(rb_cFixnum, "lrot32", fnum_lrot32, 1);
  rb_define_method(rb_cBignum, "lrot32", bnum_lrot32, 1);
  rb_define_method(rb_cFixnum, "lrot64", fnum_lrot64, 1);
  rb_define_method(rb_cBignum, "lrot64", bnum_lrot64, 1);

  rb_define_method(rb_cFixnum, "shl32",  fnum_shl32, 1);
  rb_define_method(rb_cBignum, "shl32",  bnum_shl32, 1);
  rb_define_method(rb_cFixnum, "shl64",  fnum_shl64, 1);
  rb_define_method(rb_cBignum, "shl64",  bnum_shl64, 1);
  rb_define_method(rb_cFixnum, "shr32",  fnum_shr32, 1);
  rb_define_method(rb_cBignum, "shr32",  bnum_shr32, 1);
  rb_define_method(rb_cFixnum, "shr64",  fnum_shr64, 1);
  rb_define_method(rb_cBignum, "shr64",  bnum_shr64, 1);
  rb_define_method(rb_cFixnum, "sar32",  fnum_sar32, 1);
  rb_define_method(rb_cBignum, "sar32",  bnum_sar32, 1);
  rb_define_method(rb_cFixnum, "sar64",  fnum_sar64, 1);
  rb_define_method(rb_cBignum, "sar64",  bnum_sar64, 1);
}
