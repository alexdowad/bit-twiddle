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
#error "Sorry, Bignum#bswap32 and Bignum#sar32 will not work if sizeof(BDIGIT) < 4. Please report this error."
#elif SIZEOF_BDIGIT > 8
#error "Sorry, several methods will not work if sizeof(BDIGIT) > 8. Please report this error."
#elif SIZEOF_LONG > 8
#error "Sorry, Fixnum#sar64 will not work if sizeof(long) > 8. Please report this error."
#endif

static int
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

static long
value_to_shiftdist(VALUE shiftdist, long bits)
{
  for (;;) {
    if (FIXNUM_P(shiftdist)) {
      return FIX2LONG(shiftdist);
    } else if (BIGNUM_P(shiftdist)) {
      long sdist;
      if (bnum_greater(shiftdist, bits-1))
        sdist = bits;
      else
        sdist = *RBIGNUM_DIGITS(shiftdist);
      if (RBIGNUM_NEGATIVE_P(shiftdist))
        sdist = -sdist;
      return sdist;
    } else {
      shiftdist = rb_to_int(shiftdist);
    }
  }
}

/* 'mask' is 0x7 for 8, 0xF for 16, 0x1F for 32, 0x3F for 64
 * return value is always positive! */
static ulong
value_to_rotdist(VALUE rotdist, long bits, long mask)
{
  for (;;) {
    long rdist;
    if (FIXNUM_P(rotdist)) {
      rdist = FIX2LONG(rotdist) % bits;
      if (rdist < 0)
        rdist += bits;
      return rdist;
    } else if (BIGNUM_P(rotdist)) {
      rdist = *RBIGNUM_DIGITS(rotdist) & mask;
      if (RBIGNUM_NEGATIVE_P(rotdist))
        rdist = bits - rdist;
      return rdist;
    } else {
      rotdist = rb_to_int(rotdist);
    }
  }
}

static void
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

static uint64_t
load_64_from_bignum(VALUE bnum)
{
  BDIGIT *src = RBIGNUM_DIGITS(bnum);
  size_t  len = RBIGNUM_LEN(bnum);
  uint64_t result = *src;

#if SIZEOF_BDIGIT == 4
  if (len > 1)
    result += ((uint64_t)*(src+1)) << 32;
#endif

  return result;
}

static VALUE
modify_lo8_in_bignum(VALUE bnum, uint8_t lo8)
{
  VALUE result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = (*RBIGNUM_DIGITS(bnum) & ~0xFFL) | lo8;
  return result;
}

static VALUE
modify_lo16_in_bignum(VALUE bnum, uint16_t lo16)
{
  VALUE result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = (*RBIGNUM_DIGITS(bnum) & ~0xFFFFL) | lo16;
  return result;
}

static VALUE
modify_lo32_in_bignum(VALUE bnum, uint32_t lo32)
{
#if SIZEOF_BDIGIT == 4
  BDIGIT value = lo32;
#else
  BDIGIT value = (*RBIGNUM_DIGITS(bnum) & ~0xFFFFFFFFL) | lo32;
#endif
  VALUE  result;

#if SIZEOF_LONG == 4
  /* if a 'long' is only 4 bytes, a 32-bit number could be promoted to Bignum
   * then modifying the low 32 bits could make it fixable again */
  if (RBIGNUM_LEN() == 1 && FIXABLE(value))
    return LONG2FIX(value);
#endif

  result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = value;
  return result;
}

static VALUE
modify_lo64_in_bignum(VALUE bnum, uint64_t lo64)
{
  VALUE result;

  if (RBIGNUM_LEN(bnum) <= (8/SIZEOF_BDIGIT)) {
    if (RBIGNUM_POSITIVE_P(bnum)) {
        if (POSFIXABLE(lo64))
          return LONG2FIX((long)lo64);
    } else if (lo64 <= -FIXNUM_MIN) {
      return LONG2FIX(-(long)lo64);
    }
  }

  result = rb_big_clone(bnum);
  store_64_into_bnum(result, lo64);
  return result;
}

/* Return the number of 1 bits in this integer.
 * @example
 *   7.popcount   # => 3
 *   255.popcount # => 8
 * @return [Fixnum]
 */
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

/* Return the number of 1 bits in all the bytes of this `String`.
 * @example
 *   "abc".popcount # => 10
 * @return [Fixnum]
 */
static VALUE
str_popcount(VALUE str)
{
  char *p     = RSTRING_PTR(str);
  int  length = RSTRING_LEN(str);
  long bits   = 0;

  /* This could be made faster by processing 4/8 bytes at a time */

  while (length--)
    bits += __builtin_popcount(*p++);

  return LONG2FIX(bits);
}

/* Return the index of the lowest 1 bit, where the least-significant bit is index 1.
 * If this integer is 0, return 0.
 * @example
 *   1.lo_bit   # => 1
 *   128.lo_bit # => 8
 * @return [Fixnum]
 */
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

/* Return the index of the highest 1 bit, where the least-significant bit is index 1.
 * If this integer is 0, return 0.
 * @example
 *   1.hi_bit   # => 1
 *   255.hi_bit # => 8
 * @return [Fixnum]
 */
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

/* Reverse the least-significant and second least-significant bytes of this integer.
 * @example
 *   0xFF00.bswap16 # => 255
 *   0x00FF.bswap16 # => 65280
 * @return [Integer]
 */
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

/* Reverse the least-significant 4 bytes of this integer.
 *
 * Does not reverse bits within each byte. This can be used to swap endianness
 * of a 32-bit integer.
 *
 * @example
 *   0xaabbccdd.bswap32.to_s(16) # => "ddccbbaa"
 *
 * @return [Integer]
 */
static VALUE
fnum_bswap32(VALUE fnum)
{
#if SIZEOF_LONG == 4
  /* the size of a Fixnum is always the same as 'long'
   * and the C standard guarantees 'long' is at least 32 bits
   * but a couple bits are used for tagging, so the usable precision could
   * be less than 32 bits...
   * That is why we have to use a '2NUM' function, not '2FIX' */
  return ULONG2NUM(__builtin_bswap32(FIX2LONG(fnum)));
#elif SIZEOF_LONG >= 8
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFFFFFFL) | __builtin_bswap32(value));
#endif
}

static VALUE
bnum_bswap32(VALUE bnum)
{
  return modify_lo32_in_bignum(bnum, __builtin_bswap32(*RBIGNUM_DIGITS(bnum)));
}

/* Reverse the least-significant 8 bytes of this integer.
 *
 * Does not reverse bits within each byte. This can be used to swap endianness
 * of a 64-bit integer.
 *
 * @example
 *   0xaabbccdd.bswap64.to_s(16) # => "ddccbbaa00000000"
 *
 * @return [Integer]
 */
static VALUE
fnum_bswap64(VALUE fnum)
{
  return ULL2NUM(__builtin_bswap64(FIX2LONG(fnum)));
}

static VALUE
bnum_bswap64(VALUE bnum)
{
  uint64_t value = __builtin_bswap64(load_64_from_bignum(bnum));
  return modify_lo64_in_bignum(bnum, value);
}

/* Right-rotation ("circular shift") of the low 8 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the left
 * instead.
 *
 * @example
 *   0b01110001.rrot8(1).to_s(2) # => "10111000"
 *   0b01110001.rrot8(3).to_s(2) # => "101110"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_rrot8(VALUE fnum, VALUE rotdist)
{
  long    value  = FIX2LONG(fnum);
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  uint8_t lobyte = value;
  lobyte = ((lobyte >> rotd) | (lobyte << (-rotd & 7)));
  return LONG2FIX((value & ~0xFFL) | lobyte);
}

static VALUE
bnum_rrot8(VALUE bnum, VALUE rotdist)
{
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  uint8_t lobyte = *RBIGNUM_DIGITS(bnum);
  lobyte = ((lobyte >> rotd) | (lobyte << (-rotd & 7)));
  return modify_lo8_in_bignum(bnum, lobyte);
}

/* Right-rotation ("circular shift") of the low 16 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the left
 * instead.
 *
 * @example
 *   0b0111000101110001.rrot16(1).to_s(2) # => "1011100010111000"
 *   0b0111000101110001.rrot16(3).to_s(2) # => "10111000101110"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_rrot16(VALUE fnum, VALUE rotdist)
{
  long     value  = FIX2LONG(fnum);
  ulong    rotd   = value_to_rotdist(rotdist, 16, 0xF);
  uint16_t loword = value;
  loword = (loword >> rotd) | (loword << (-rotd & 15));
  return LONG2FIX((value & ~0xFFFFL) | loword);
}

static VALUE
bnum_rrot16(VALUE bnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 16, 0xF);
  uint16_t loword = *RBIGNUM_DIGITS(bnum);
  loword = (loword >> rotd) | (loword << (-rotd & 15));
  return modify_lo16_in_bignum(bnum, loword);
}

/* Right-rotation ("circular shift") of the low 32 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the left
 * instead.
 *
 * @example
 *   0xaabbccdd.rrot32(4).to_s(16) # => "daabbccd"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_rrot32(VALUE fnum, VALUE rotdist)
{
  long     value  = FIX2LONG(fnum);
  ulong    rotd   = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32   = value;
  lo32 = (lo32 >> rotd) | (lo32 << (-rotd & 31));
#if SIZEOF_LONG == 8
  return LONG2FIX((value & ~0xFFFFFFFFL) | lo32);
#else
  return ULONG2NUM(lo32);
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

/* Right-rotation ("circular shift") of the low 64 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the left
 * instead.
 *
 * @example
 *   0x11223344aabbccdd.rrot64(4).to_s(16) # => "d11223344aabbccd"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
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
  uint64_t value  = load_64_from_bignum(bnum);
  value = ((value >> rotd) | (value << (-rotd & 63)));
  return modify_lo64_in_bignum(bnum, value);
}

/* Left-rotation ("circular shift") of the low 8 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the right
 * instead.
 *
 * @example
 *   0b01110001.lrot8(1).to_s(2) # => "11100010"
 *   0b01110001.lrot8(3).to_s(2) # => "10001011"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_lrot8(VALUE fnum, VALUE rotdist)
{
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  long    value  = FIX2LONG(fnum);
  uint8_t lobyte = value;
  lobyte = ((lobyte << rotd) | (lobyte >> (-rotd & 7)));
  return LONG2FIX((value & ~0xFFL) | lobyte);
}

static VALUE
bnum_lrot8(VALUE bnum, VALUE rotdist)
{
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  uint8_t lobyte = *RBIGNUM_DIGITS(bnum);
  lobyte = ((lobyte << rotd) | (lobyte >> (-rotd & 7))) & 0xFF;
  return modify_lo8_in_bignum(bnum, lobyte);
}

/* Left-rotation ("circular shift") of the low 16 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the right
 * instead.
 *
 * @example
 *   0b0111000101110001.lrot16(1).to_s(2) # => "1110001011100010"
 *   0b0111000101110001.lrot16(3).to_s(2) # => "1000101110001011"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_lrot16(VALUE fnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 16, 0xF);
  long     value  = FIX2LONG(fnum);
  uint16_t loword = value;
  loword = (loword << rotd) | (loword >> (-rotd & 15));
  return LONG2FIX((value & ~0xFFFFL) | loword);
}

static VALUE
bnum_lrot16(VALUE bnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 16, 0xF);
  uint16_t loword = *RBIGNUM_DIGITS(bnum);
  loword = (loword << rotd) | (loword >> (-rotd & 15));
  return modify_lo16_in_bignum(bnum, loword);
}

/* Left-rotation ("circular shift") of the low 32 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the right
 * instead.
 *
 * @example
 *   0xaabbccdd.lrot32(4).to_s(16) # => "abbccdda"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_lrot32(VALUE fnum, VALUE rotdist)
{
  long     value = FIX2LONG(fnum);
  ulong    rotd  = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32  = value;
  lo32 = (lo32 << rotd) | (lo32 >> (-rotd & 31));
  return LONG2FIX((value & ~0xFFFFFFFFL) | lo32);
}

static VALUE
bnum_lrot32(VALUE bnum, VALUE rotdist)
{
  ulong    rotd = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  lo32  = (lo32 << rotd) | (lo32 >> (-rotd & 31));
  return modify_lo32_in_bignum(bnum, lo32);
}

/* Left-rotation ("circular shift") of the low 64 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the right
 * instead.
 *
 * @example
 *   0x11223344aabbccdd.lrot64(4).to_s(16) # => "1223344aabbccdd1"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
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
  uint64_t value  = load_64_from_bignum(bnum);
  value = ((value << rotd) | (value >> (-rotd & 63)));
  return modify_lo64_in_bignum(bnum, value);
}

static VALUE
fnum_shl8(VALUE fnum, VALUE shiftdist)
{
  long    value = FIX2LONG(fnum);
  long    sdist = value_to_shiftdist(shiftdist, 8);
  uint8_t lo8   = value;

  if (sdist == 0)
    return fnum;
  else if (sdist >= 8 || sdist <= -8)
    return LONG2FIX(value & ~0xFFL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFL) | (lo8 >> ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFL) | (lo8 << ((ulong)sdist)));  
}

static VALUE
bnum_shl8(VALUE bnum, VALUE shiftdist)
{
  uint16_t lo8   = *RBIGNUM_DIGITS(bnum);
  long     sdist = value_to_shiftdist(shiftdist, 8);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 8 || sdist <= -8)
    lo8 = 0;
  else if (sdist < 0)
    lo8 = lo8 >> ((ulong)-sdist);
  else
    lo8 = lo8 << ((ulong)sdist);

  return modify_lo8_in_bignum(bnum, lo8);
}

static VALUE
fnum_shl16(VALUE fnum, VALUE shiftdist)
{
  long    value = FIX2LONG(fnum);
  long    sdist = value_to_shiftdist(shiftdist, 16);
  uint16_t lo16 = value;

  if (sdist == 0)
    return fnum;
  else if (sdist >= 16 || sdist <= -16)
    return LONG2FIX(value & ~0xFFFFL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFFFL) | (lo16 >> ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFFFL) | (lo16 << ((ulong)sdist)));  
}

static VALUE
bnum_shl16(VALUE bnum, VALUE shiftdist)
{
  uint16_t lo16  = *RBIGNUM_DIGITS(bnum);
  long     sdist = value_to_shiftdist(shiftdist, 16);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 16 || sdist <= -16)
    lo16 = 0;
  else if (sdist < 0)
    lo16 = lo16 >> ((ulong)-sdist);
  else
    lo16 = lo16 << ((ulong)sdist);

  return modify_lo16_in_bignum(bnum, lo16);
}

static VALUE
fnum_shl32(VALUE fnum, VALUE shiftdist)
{
  long     value = FIX2LONG(fnum);
  long     sdist = value_to_shiftdist(shiftdist, 32);
  uint32_t lo32  = value;

  if (sdist == 0)
    return fnum;
  else if (sdist >= 32 || sdist <= -32)
    return LONG2FIX(value & ~0xFFFFFFFFL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFFFFFFFL) | (lo32 >> ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFFFFFFFL) | (lo32 << ((ulong)sdist)));
}

static VALUE
bnum_shl32(VALUE bnum, VALUE shiftdist)
{
  uint32_t lo32  = *RBIGNUM_DIGITS(bnum);
  long     sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 32 || sdist <= -32)
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

  if (sdist == 0)
    return fnum;
  else if (sdist >= 32 || sdist <= -32)
    return LONG2FIX(value & ~0xFFFFFFFFUL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFFFFFFFUL) | (lo32 << ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFFFFFFFUL) | (lo32 >> ((ulong)sdist)));
}

static VALUE
bnum_shr32(VALUE bnum, VALUE shiftdist)
{
  uint32_t lo32;
  long     sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 32 || sdist <= -32)
    lo32 = 0;
  else if (sdist < 0)
    lo32 = *RBIGNUM_DIGITS(bnum) << ((ulong)-sdist);
  else
    lo32 = *RBIGNUM_DIGITS(bnum) >> ((ulong)sdist);

  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_sar32(VALUE fnum, VALUE shiftdist)
{
#if SIZEOF_LONG == 4
  /* Not enough precision for 32nd bit to be a 1 */
  return fnum_shr32(fnum, shiftdist);
#else
  long     value;
  uint32_t lo32;
  long     sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return fnum;
  if (sdist < 0)
    return fnum_shl32(fnum, LONG2FIX(-sdist));
  
  value = FIX2LONG(fnum);
  lo32  = value;
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
  uint32_t lo32;
  long     sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return bnum;
  else if (sdist < 0)
    return bnum_shl32(bnum, LONG2FIX(-sdist));
  
  lo32 = *RBIGNUM_DIGITS(bnum);
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
  long sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return fnum;
  else if (sdist >= 64 || sdist <= -64)
    return fix_zero;
  else if (sdist < 0)
    return LONG2FIX(FIX2ULONG(fnum) >> ((ulong)-sdist));
  else
    return ULL2NUM(FIX2ULONG(fnum) << ((ulong)sdist));
}

static VALUE
bnum_shl64(VALUE bnum, VALUE shiftdist)
{
  uint64_t val;
  long     sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 64 || sdist <= -64)
    val = 0ULL;
  else if (sdist < 0)
    val = load_64_from_bignum(bnum) >> ((ulong)-sdist);
  else
    val = load_64_from_bignum(bnum) << ((ulong)sdist);

  return modify_lo64_in_bignum(bnum, val);
}

static VALUE
fnum_shr64(VALUE fnum, VALUE shiftdist)
{
  long sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return fnum;
  else if (sdist >= 64 || sdist <= -64)
    return fix_zero;
  else if (sdist < 0)
    return ULL2NUM(FIX2ULONG(fnum) << ((ulong)-sdist));
  else
    return LONG2FIX(FIX2ULONG(fnum) >> ((ulong)sdist));
}

static VALUE
bnum_shr64(VALUE bnum, VALUE shiftdist)
{
  uint64_t val;
  long     sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 64 || sdist <= -64)
    val = 0ULL;
  else if (sdist < 0)
    val = load_64_from_bignum(bnum) << ((ulong)-sdist);
  else
    val = load_64_from_bignum(bnum) >> ((ulong)sdist);

  return modify_lo64_in_bignum(bnum, val);
}

static VALUE
bnum_sar64(VALUE bnum, VALUE shiftdist)
{
  uint64_t val;
  long     sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return bnum;
  else if (sdist < 0)
    return bnum_shl64(bnum, LONG2FIX(-sdist));
  
  val = load_64_from_bignum(bnum);
  if ((0x8000000000000000ULL & val) != 0) {
    if (sdist < 64 && sdist > -64)
      val = (val >> sdist) | ~(~0ULL >> sdist);
    else
      val = ~0ULL;
  } else {
    if (sdist < 64 && sdist > -64)
      val = val >> sdist;
    else
      val = 0;
  }

  return modify_lo64_in_bignum(bnum, val);
}

static const uint8_t bitreverse_table[] =
{
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

static inline uint8_t
reverse8(uint8_t value)
{
  #if SIZEOF_LONG == 8
    /* 64-bit CPU
     * Thanks to the Bit Twiddling Hacks page:
     * http://graphics.stanford.edu/~seander/bithacks.html */
    return (value * 0x0202020202UL & 0x010884422010UL) % 1023;
  #else
    /* 32-bit CPU */
    return bitreverse_table[value];
  #endif
}

static inline uint16_t
reverse16(uint16_t value)
{
  return (bitreverse_table[value & 0xFF] << 8) | bitreverse_table[value >> 8];
}

static inline uint32_t
reverse32(uint32_t value)
{
  return ((uint32_t)reverse16(value) << 16) | reverse16(value >> 16);
}

static inline uint64_t
reverse64(uint64_t value)
{
  return ((uint64_t)reverse32(value) << 32) | reverse32(value >> 32);
}

static VALUE
fnum_bitreverse8(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFL) | reverse8(value));
}

static VALUE
bnum_bitreverse8(VALUE bnum)
{
  uint8_t lo8 = *RBIGNUM_DIGITS(bnum);
  return modify_lo8_in_bignum(bnum, reverse8(lo8));
}

static VALUE
fnum_bitreverse16(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFFL) | reverse16(value));
}

static VALUE
bnum_bitreverse16(VALUE bnum)
{
  uint16_t lo16 = *RBIGNUM_DIGITS(bnum);
  return modify_lo16_in_bignum(bnum, reverse16(lo16));
}

static VALUE
fnum_bitreverse32(VALUE fnum)
{
  long     value = FIX2LONG(fnum);
  uint32_t lo32  = value;
#if SIZEOF_LONG == 4
  return ULONG2NUM(reverse32(lo32));
#else
  return LONG2FIX((value & ~0xFFFFFFFFL) | reverse32(lo32));
#endif
}

static VALUE
bnum_bitreverse32(VALUE bnum)
{
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  return modify_lo32_in_bignum(bnum, reverse32(lo32));
}

static VALUE
fnum_bitreverse64(VALUE fnum)
{
  /* on a 32-bit system, do we want sign extension of a negative 32-bit value into 64 bits??? */
  uint64_t lo64 = FIX2ULONG(fnum);
  return ULL2NUM(reverse64(lo64));
}

static VALUE
bnum_bitreverse64(VALUE bnum)
{
  return modify_lo64_in_bignum(bnum, reverse64(load_64_from_bignum(bnum)));
}

/* Document-class: Fixnum
 * Ruby's good old Fixnum.
 */
/* Document-class: Bignum
 * Ruby's good old Bignum.
 */
/* Document-class: String
 * Ruby's good old String.
 */
void Init_bit_twiddle(void)
{
  rb_define_method(rb_cFixnum, "popcount", fnum_popcount, 0);
  rb_define_method(rb_cBignum, "popcount", bnum_popcount, 0);
  rb_define_method(rb_cString, "popcount", str_popcount,  0);

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

  rb_define_method(rb_cFixnum, "shl8",   fnum_shl8,  1);
  rb_define_method(rb_cBignum, "shl8",   bnum_shl8,  1);
  rb_define_method(rb_cFixnum, "shl16",  fnum_shl16, 1);
  rb_define_method(rb_cBignum, "shl16",  bnum_shl16, 1);
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

  /* Fixnum doesn't have enough precision that the 64th bit could be a 1
   * (Or if it does, our preprocessor directives above will make sure this won't compile) */
  rb_define_method(rb_cFixnum, "sar64",  fnum_shr64, 1);
  rb_define_method(rb_cBignum, "sar64",  bnum_sar64, 1);

  rb_define_method(rb_cFixnum, "bitreverse8",  fnum_bitreverse8,  0);
  rb_define_method(rb_cBignum, "bitreverse8",  bnum_bitreverse8,  0);
  rb_define_method(rb_cFixnum, "bitreverse16", fnum_bitreverse16, 0);
  rb_define_method(rb_cBignum, "bitreverse16", bnum_bitreverse16, 0);
  rb_define_method(rb_cFixnum, "bitreverse32", fnum_bitreverse32, 0);
  rb_define_method(rb_cBignum, "bitreverse32", bnum_bitreverse32, 0);
  rb_define_method(rb_cFixnum, "bitreverse64", fnum_bitreverse64, 0);
  rb_define_method(rb_cBignum, "bitreverse64", bnum_bitreverse64, 0);
}
