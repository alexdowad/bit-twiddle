/* Fast C implementation of additional bitwise operations for Ruby
 * Hand-crafted with ♥ by Alex Dowad, using ONLY the finest 1s and 0s */

#include <ruby.h>
#include "bt_bignum.h"

#ifndef HAVE_TYPE_ULONG
typedef unsigned long ulong;
#endif
#ifndef HAVE_TYPE_UCHAR
typedef unsigned char uchar;
#endif

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
#error "Sorry, Integer#bswap32 and Integer#arith_rshift32 will not work if sizeof(BDIGIT) < 4. Please report this error."
#elif SIZEOF_BDIGIT > 8
#error "Sorry, several methods will not work if sizeof(BDIGIT) > 8. Please report this error."
#elif SIZEOF_LONG > 8
#error "Sorry, Integer#arith_rshift64 will not work if sizeof(long) > 8. Please report this error."
#endif

#if HAVE_BSWAP16 == 0
/* stupid bug in GCC 4.7 */
static inline uint16_t __builtin_bswap16(uint16_t value)
{
  return (value >> 8) | (value << 8);
}
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
value_to_shiftdist(VALUE shiftdist, unsigned int bits)
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
      return (ulong)rdist;
    } else if (BIGNUM_P(rotdist)) {
      rdist = *RBIGNUM_DIGITS(rotdist) & mask;
      if (RBIGNUM_NEGATIVE_P(rotdist))
        rdist = bits - rdist;
      return (ulong)rdist;
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

#if (SIZEOF_BDIGIT == 8)
    *dest = int64;
#else
    if (len > 1) {
      *dest     = (uint32_t)int64;
      *(dest+1) = (uint32_t)(int64 >> 32);
    } else if ((int64 & (0xFFFFFFFFULL << 32)) == 0) {
      /* the high 4 bytes are zero anyways */
      *dest = (uint32_t)int64;
    } else {
      rb_big_resize(bnum, 2);
      dest      = RBIGNUM_DIGITS(bnum); /* may have moved */
      *dest     = (uint32_t)int64;
      *(dest+1) = (uint32_t)(int64 >> 32);
    }
#endif
}

static uint64_t
load_64_from_bignum(VALUE bnum)
{
  BDIGIT *src = RBIGNUM_DIGITS(bnum);
  size_t  len = RBIGNUM_LEN(bnum);
  uint64_t result = *src;

  if (SIZEOF_BDIGIT == 4 && len > 1)
    result += ((uint64_t)*(src+1)) << 32;

  return result;
}

static VALUE
modify_lo8_in_bignum(VALUE bnum, uint8_t lo8)
{
  VALUE result;

  if (lo8 == (uint8_t)*RBIGNUM_DIGITS(bnum))
    return bnum;

  result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = (*RBIGNUM_DIGITS(bnum) & ~0xFFL) | lo8;
  return result;
}

static VALUE
modify_lo16_in_bignum(VALUE bnum, uint16_t lo16)
{
  VALUE result;

  if (lo16 == (uint16_t)*RBIGNUM_DIGITS(bnum))
    return bnum;

  result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = (*RBIGNUM_DIGITS(bnum) & ~0xFFFFL) | lo16;
  return result;
}

static VALUE
modify_lo32_in_bignum(VALUE bnum, uint32_t lo32)
{
  BDIGIT value;
  VALUE  result;

  if (lo32 == (uint32_t)*RBIGNUM_DIGITS(bnum))
    return bnum;

  if (SIZEOF_BDIGIT == 4)
    value = lo32;
  else
    value = (*RBIGNUM_DIGITS(bnum) & ~0xFFFFFFFFL) | lo32;

#if SIZEOF_LONG == 4
  /* if a 'long' is only 4 bytes, a 32-bit number could be promoted to Bignum
   * then modifying the low 32 bits could make it fixable again */
  if (RBIGNUM_LEN(bnum) == 1 && FIXABLE(value))
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

/* Now start defining implementations of actual Ruby methods
 * First two helper macros: */
#define def_int_method(name) \
  static VALUE int_ ## name(VALUE integer) { \
    if (FIXNUM_P(integer)) \
      return fnum_ ## name(integer); \
    else \
      return bnum_ ## name(integer); \
  }
#define def_int_method_with_arg(name) \
  static VALUE int_ ## name(VALUE integer, VALUE arg) { \
    if (FIXNUM_P(integer)) \
      return fnum_ ## name(integer, arg); \
    else \
      return bnum_ ## name(integer, arg); \
  }

static VALUE
fnum_popcount(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    rb_raise(rb_eRangeError, "can't take popcount of a negative number");
  return LONG2FIX(__builtin_popcountl((ulong)value));
}

static VALUE
bnum_popcount(VALUE bnum)
{
  BDIGIT *digits = RBIGNUM_DIGITS(bnum);
  size_t  length = RBIGNUM_LEN(bnum);
  long    bits   = 0;

  if (RBIGNUM_NEGATIVE_P(bnum))
    rb_raise(rb_eRangeError, "can't take popcount of a negative number");

  while (length--) {
    bits += popcount_bdigit(*digits);
    digits++;
  }

  return LONG2FIX(bits);
}

/* Document-method: Integer#popcount
 * Return the number of 1 bits in this integer.
 *
 * If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   7.popcount   # => 3
 *   255.popcount # => 8
 * @return [Integer]
 */
def_int_method(popcount);

/* Return the number of 1 bits in all the bytes of this `String`.
 * @example
 *   "abc".popcount # => 10
 * @return [Integer]
 */
static VALUE
str_popcount(VALUE str)
{
  uchar *p    = (uchar*)RSTRING_PTR(str);
  long length = RSTRING_LEN(str);
  long bits   = 0;

  /* This could be made faster by processing 4/8 bytes at a time */

  while (length--)
    bits += __builtin_popcount(*p++);

  return LONG2FIX(bits);
}

static VALUE
fnum_lo_bit(VALUE fnum)
{
  /* We raise an error on a negative number because the internal representation
   * used for negative numbers is different between Fixnum and Bignum, so the
   * results would not be consistent (running a program on a different computer,
   * or with a Ruby interpreter compiled by a different compiler, could yield
   * different results.)
   * The alternative would be to _pretend_ that both Fixnums/Bignums use 2's
   * complement notation and compute the answer accordingly.
   * I don't think it's worth the trouble! */
  long value = FIX2LONG(fnum);
  if (value < 0)
    rb_raise(rb_eRangeError, "can't find lowest 1 bit in a negative number");
  return LONG2FIX(__builtin_ffsl(value));
}

static VALUE
bnum_lo_bit(VALUE bnum)
{
  BDIGIT *digit = RBIGNUM_DIGITS(bnum);
  long    bits  = 0;

  if (RBIGNUM_NEGATIVE_P(bnum))
    rb_raise(rb_eRangeError, "can't find lowest 1 bit in a negative number");

  while (!*digit) {
    digit++;
    bits += (sizeof(BDIGIT) * 8);
  }

  bits += ffs_bdigit(*digit);
  return LONG2FIX(bits);
}

/* Document-method: Integer#lo_bit
 * Return the index of the lowest 1 bit, where the least-significant bit is index 1.
 * If the receiver is 0, return 0.
 *
 * If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   1.lo_bit   # => 1
 *   128.lo_bit # => 8
 * @return [Integer]
 */
def_int_method(lo_bit);

static VALUE
fnum_hi_bit(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value == 0)
    return fix_zero;
  else if (value < 0)
    rb_raise(rb_eRangeError, "can't find highest 1 bit in a negative number");
  return LONG2FIX((sizeof(long) * 8) - __builtin_clzl(value));
}

static VALUE
bnum_hi_bit(VALUE bnum)
{
  BDIGIT *digit = RBIGNUM_DIGITS(bnum) + (RBIGNUM_LEN(bnum)-1);
  ulong   bits  = (sizeof(BDIGIT) * 8) * RBIGNUM_LEN(bnum);

  if (RBIGNUM_NEGATIVE_P(bnum))
    rb_raise(rb_eRangeError, "can't find highest 1 bit in a negative number");

  while (!*digit) {
    digit--;
    bits -= (sizeof(BDIGIT) * 8);
  }

  bits -= clz_bdigit(*digit);
  return LONG2FIX(bits);
}

/* Document-method: Integer#hi_bit
 * Return the index of the highest 1 bit, where the least-significant bit is index 1.
 * If the receiver is 0, return 0.
 *
 * If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   1.hi_bit   # => 1
 *   255.hi_bit # => 8
 * @return [Integer]
 */
def_int_method(hi_bit);

static VALUE
fnum_bswap16(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    rb_raise(rb_eRangeError, "can't swap bytes in a negative number");
  return LONG2FIX(((ulong)value & ~0xFFFFUL) | __builtin_bswap16((uint16_t)value));
}

static VALUE
bnum_bswap16(VALUE bnum)
{
  if (RBIGNUM_POSITIVE_P(bnum))
    return modify_lo16_in_bignum(bnum, __builtin_bswap16((uint16_t)*RBIGNUM_DIGITS(bnum)));
  else
    rb_raise(rb_eRangeError, "can't swap bytes in a negative number");
}

/* Document-method: Integer#bswap16
 * Reverse the least-significant and second least-significant bytes of this integer.
 *
 * If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   0xFF00.bswap16 # => 255
 *   0x00FF.bswap16 # => 65280
 * @return [Integer]
 */
def_int_method(bswap16);

static VALUE
fnum_bswap32(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    rb_raise(rb_eRangeError, "can't swap bytes in a negative number");

  if (SIZEOF_LONG == 4)
    /* the size of a Fixnum is always the same as 'long'
     * and the C standard guarantees 'long' is at least 32 bits
     * but a couple bits are used for tagging, so the usable precision could
     * be less than 32 bits...
     * That is why we have to use a '2NUM' function, not '2FIX' */
    return ULONG2NUM(__builtin_bswap32(FIX2LONG(fnum)));
  else
    return LONG2FIX(((ulong)value & ~0xFFFFFFFFUL) | __builtin_bswap32((uint32_t)value));
}

static VALUE
bnum_bswap32(VALUE bnum)
{
  if (RBIGNUM_POSITIVE_P(bnum))
    return modify_lo32_in_bignum(bnum, __builtin_bswap32(*RBIGNUM_DIGITS(bnum)));
  else
    rb_raise(rb_eRangeError, "can't swap bytes in a negative number");
}

/* Document-method: Integer#bswap32
 * Reverse the least-significant 4 bytes of this integer.
 *
 * Does not reverse bits within each byte. This can be used to swap endianness
 * of a 32-bit integer. If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   0xaabbccdd.bswap32.to_s(16) # => "ddccbbaa"
 *
 * @return [Integer]
 */
def_int_method(bswap32);

static VALUE
fnum_bswap64(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    rb_raise(rb_eRangeError, "can't swap bytes in a negative number");
  return ULL2NUM(__builtin_bswap64((uint64_t)value));
}

static VALUE
bnum_bswap64(VALUE bnum)
{
  if (RBIGNUM_POSITIVE_P(bnum))
    return modify_lo64_in_bignum(bnum, __builtin_bswap64(load_64_from_bignum(bnum)));
  else
    rb_raise(rb_eRangeError, "can't swap bytes in a negative number");
}

/* Document-method: Integer#bswap64
 * Reverse the least-significant 8 bytes of this integer.
 *
 * Does not reverse bits within each byte. This can be used to swap endianness
 * of a 64-bit integer. If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   0xaabbccdd.bswap64.to_s(16) # => "ddccbbaa00000000"
 *
 * @return [Integer]
 */
def_int_method(bswap64);

#define def_rot_helpers(bits) \
  static inline uint##bits##_t rrot##bits(uint##bits##_t value, VALUE rotdist) { \
    ulong rotd = value_to_rotdist(rotdist, bits, bits-1); \
    return (value >> rotd) | (value << (-rotd & (bits-1))); \
  } \
  static inline uint##bits##_t lrot##bits(uint##bits##_t value, VALUE rotdist) { \
    ulong rotd = value_to_rotdist(rotdist, bits, bits-1); \
    return (value << rotd) | (value >> (-rotd & (bits-1))); \
  }

def_rot_helpers(8);
def_rot_helpers(16);
def_rot_helpers(32);
def_rot_helpers(64);

static VALUE
fnum_rrot8(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX(((ulong)value & ~0xFFUL) | rrot8((uint8_t)value, rotdist));
}

static VALUE
bnum_rrot8(VALUE bnum, VALUE rotdist)
{
  return modify_lo8_in_bignum(bnum, rrot8((uint8_t)*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Integer#rrot8
 * Right-rotation ("circular shift") of the low 8 bits in this integer.
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
def_int_method_with_arg(rrot8);

static VALUE
fnum_rrot16(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX(((ulong)value & ~0xFFFFUL) | rrot16((uint16_t)value, rotdist));
}

static VALUE
bnum_rrot16(VALUE bnum, VALUE rotdist)
{
  return modify_lo16_in_bignum(bnum, rrot16((uint16_t)*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Integer#rrot16
 * Right-rotation ("circular shift") of the low 16 bits in this integer.
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
def_int_method_with_arg(rrot16);

static VALUE
fnum_rrot32(VALUE fnum, VALUE rotdist)
{
  long     value  = FIX2LONG(fnum);
  if (SIZEOF_LONG == 8)
    return LONG2FIX(((ulong)value & ~0xFFFFFFFFUL) | rrot32((uint32_t)value, rotdist));
  else
    return ULONG2NUM(rrot32((uint32_t)value, rotdist));
}

static VALUE
bnum_rrot32(VALUE bnum, VALUE rotdist)
{
  return modify_lo32_in_bignum(bnum, rrot32((uint32_t)*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Integer#rrot32
 * Right-rotation ("circular shift") of the low 32 bits in this integer.
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
def_int_method_with_arg(rrot32);

static VALUE
fnum_rrot64(VALUE fnum, VALUE rotdist)
{
  return ULL2NUM(rrot64(FIX2ULONG(fnum), rotdist));
}

static VALUE
bnum_rrot64(VALUE bnum, VALUE rotdist)
{
  return modify_lo64_in_bignum(bnum, rrot64(load_64_from_bignum(bnum), rotdist));
}

/* Document-method: Integer#rrot64
 * Right-rotation ("circular shift") of the low 64 bits in this integer.
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
def_int_method_with_arg(rrot64);

static VALUE
fnum_lrot8(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX(((ulong)value & ~0xFFUL) | lrot8((uint8_t)value, rotdist));
}

static VALUE
bnum_lrot8(VALUE bnum, VALUE rotdist)
{
  return modify_lo8_in_bignum(bnum, lrot8((uint8_t)*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Integer#lrot8
 * Left-rotation ("circular shift") of the low 8 bits in this integer.
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
def_int_method_with_arg(lrot8);

static VALUE
fnum_lrot16(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX(((ulong)value & ~0xFFFFUL) | lrot16((uint16_t)value, rotdist));
}

static VALUE
bnum_lrot16(VALUE bnum, VALUE rotdist)
{
  return modify_lo16_in_bignum(bnum, lrot16((uint16_t)*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Integer#lrot16
 * Left-rotation ("circular shift") of the low 16 bits in this integer.
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
def_int_method_with_arg(lrot16);

static VALUE
fnum_lrot32(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX(((ulong)value & ~0xFFFFFFFFUL) | lrot32((uint32_t)value, rotdist));
}

static VALUE
bnum_lrot32(VALUE bnum, VALUE rotdist)
{
  return modify_lo32_in_bignum(bnum, lrot32((uint32_t)*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Integer#lrot32
 * Left-rotation ("circular shift") of the low 32 bits in this integer.
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
def_int_method_with_arg(lrot32);

static VALUE
fnum_lrot64(VALUE fnum, VALUE rotdist)
{
  return ULL2NUM(lrot64(FIX2ULONG(fnum), rotdist));
}

static VALUE
bnum_lrot64(VALUE bnum, VALUE rotdist)
{
  return modify_lo64_in_bignum(bnum, lrot64(load_64_from_bignum(bnum), rotdist));
}

/* Document-method: Integer#lrot64
 * Left-rotation ("circular shift") of the low 64 bits in this integer.
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
def_int_method_with_arg(lrot64);

#define def_shift_helpers(bits) \
  static uint##bits##_t lshift##bits(uint##bits##_t value, VALUE shiftdist) { \
    long sdist = value_to_shiftdist(shiftdist, bits); \
    if (sdist >= bits || sdist <= -bits) return 0; \
    else if (sdist < 0) return value >> ((uint)-sdist); \
    else return value << (uint)sdist; \
  } \
  static uint##bits##_t rshift##bits(uint##bits##_t value, VALUE shiftdist) { \
    long sdist = value_to_shiftdist(shiftdist, bits); \
    if (sdist >= bits || sdist <= -bits) return 0; \
    else if (sdist < 0) return value << ((uint)-sdist); \
    else return value >> (uint)sdist; \
  } \
  static uint##bits##_t arith_rshift##bits(uint##bits##_t value, VALUE shiftdist) { \
    long sdist = value_to_shiftdist(shiftdist, bits); \
    if (sdist >= bits) { \
      if ((((uint##bits##_t)1 << (bits - 1)) & value) != 0) \
        return ~0; \
      else \
        return 0; \
    } else if (sdist <= -bits) { \
      return 0; \
    } else if (sdist < 0) { \
      return value << ((uint)-sdist); \
    } else if (RSHIFT_IS_ARITH) { \
      return (int##bits##_t)value >> (int)sdist; \
    } else if ((((uint##bits##_t)1 << (bits - 1)) & value) != 0) { \
      return (value >> sdist) | ~(((uint##bits##_t)~0) >> sdist); \
    } else { \
      return value >> sdist; \
    } \
  }

def_shift_helpers(8);
def_shift_helpers(16);
def_shift_helpers(32);
def_shift_helpers(64);

static VALUE
fnum_lshift8(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFUL) | lshift8((uint8_t)value, shiftdist));
}

static VALUE
bnum_lshift8(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo8_in_bignum(bnum, lshift8((uint8_t)*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#lshift8
 * Left-shift of the low 8 bits in this integer.
 *
 * If the shift distance is negative, a right shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 7 or less than -7, the low 8 bits will all be zeroed.
 *
 * @example
 *   0x11223344.lshift8(1).to_s(16) # => "11223388"
 *   0x11223344.lshift8(2).to_s(16) # => "11223310"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(lshift8);

static VALUE
fnum_lshift16(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFFFUL) | lshift16((uint16_t)value, shiftdist));
}

static VALUE
bnum_lshift16(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo16_in_bignum(bnum, lshift16((uint16_t)*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#lshift16
 * Left-shift of the low 16 bits in this integer.
 *
 * If the shift distance is negative, a right shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 15 or less than -15, the low 16 bits will all be zeroed.
 *
 * @example
 *   0x11223344.lshift16(1).to_s(16) # => "11226688"
 *   0x11223344.lshift16(2).to_s(16) # => "1122cd10"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(lshift16);

static VALUE
fnum_lshift32(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFFFFFFFUL) | lshift32((uint32_t)value, shiftdist));
}

static VALUE
bnum_lshift32(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo32_in_bignum(bnum, lshift32((uint32_t)*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#lshift32
 * Left-shift of the low 32 bits in this integer.
 *
 * If the shift distance is negative, a right shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 31 or less than -31, the low 32 bits will all be zeroed.
 *
 * @example
 *   0x11223344.lshift32(1).to_s(16) # => "22446688"
 *   0x11223344.lshift32(2).to_s(16) # => "4488cd10"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(lshift32);

static VALUE
fnum_lshift64(VALUE fnum, VALUE shiftdist)
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
bnum_lshift64(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo64_in_bignum(bnum, lshift64(load_64_from_bignum(bnum), shiftdist));
}

/* Document-method: Integer#lshift64
 * Left-shift of the low 64 bits in this integer.
 *
 * If the shift distance is negative, a right shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 63 or less than -63, the low 64 bits will all be zeroed.
 *
 * @example
 *   0x1122334411223344.lshift64(1).to_s(16) # => "2244668822446688"
 *   0x1122334411223344.lshift64(2).to_s(16) # => "4488cd104488cd10"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(lshift64);

static VALUE
fnum_rshift8(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFUL) | rshift8((uint8_t)value, shiftdist));
}

static VALUE
bnum_rshift8(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo8_in_bignum(bnum, rshift8((uint8_t)*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#rshift8
 * Right-shift of the low 8 bits in this integer.
 *
 * If the shift distance is negative, a left shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 7 or less than -7, the low 8 bits will all be zeroed.
 *
 * @example
 *   0x11223344.rshift8(1).to_s(16) # => "11223322"
 *   0x11223344.rshift8(2).to_s(16) # => "11223311"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(rshift8);

static VALUE
fnum_rshift16(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFFFUL) | rshift16((uint16_t)value, shiftdist));
}

static VALUE
bnum_rshift16(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo16_in_bignum(bnum, rshift16((uint16_t)*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#rshift16
 * Right-shift of the low 16 bits in this integer.
 *
 * If the shift distance is negative, a left shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 15 or less than -15, the low 16 bits will all be zeroed.
 *
 * @example
 *   0x11223344.rshift16(1).to_s(16) # => "112219a2"
 *   0x11223344.rshift16(2).to_s(16) # => "11220cd1"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(rshift16);

static VALUE
fnum_rshift32(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFFFFFFFUL) | rshift32((uint32_t)value, shiftdist));
}

static VALUE
bnum_rshift32(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo32_in_bignum(bnum, rshift32(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#rshift32
 * Right-shift of the low 32 bits in this integer.
 *
 * If the shift distance is negative, a left shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 31 or less than -31, the low 32 bits will all be zeroed.
 *
 * @example
 *   0x11223344.rshift32(1).to_s(16) # => "89119a2"
 *   0x11223344.rshift32(2).to_s(16) # => "4488cd1"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(rshift32);

static VALUE
fnum_rshift64(VALUE fnum, VALUE shiftdist)
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
bnum_rshift64(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo64_in_bignum(bnum, rshift64(load_64_from_bignum(bnum), shiftdist));
}

/* Document-method: Integer#rshift64
 * Right-shift of the low 64 bits in this integer.
 *
 * If the shift distance is negative, a left shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 63 or less than -63, the low 64 bits will all be zeroed.
 *
 * @example
 *   0x1122334411223344.rshift64(1).to_s(16) # => "89119a2089119a2"
 *   0x1122334411223344.rshift64(2).to_s(16) # => "4488cd104488cd1"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(rshift64);

static VALUE
fnum_arith_rshift8(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFUL) | arith_rshift8((uint8_t)value, shiftdist));
}

static VALUE
bnum_arith_rshift8(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo8_in_bignum(bnum, arith_rshift8((uint8_t)*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#arith_rshift8
 * Arithmetic right-shift of the low 8 bits in this integer.
 *
 * If bit 8 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
 * they will be filled with 0s. Or, if the shift distance is negative, a left shift
 * will be performed instead, and the vacated bit positions will be filled with 0s.
 *
 * @example
 *   0xaabbccdd.arith_rshift8(1).to_s(16) # => "aabbccee"
 *   0xaabbccdd.arith_rshift8(2).to_s(16) # => "aabbccf7"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(arith_rshift8);

static VALUE
fnum_arith_rshift16(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFFFUL) | arith_rshift16((uint16_t)value, shiftdist));
}

static VALUE
bnum_arith_rshift16(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo16_in_bignum(bnum, arith_rshift16((uint16_t)*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#arith_rshift16
 * Arithmetic right-shift of the low 16 bits in this integer.
 *
 * If bit 16 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
 * they will be filled with 0s. Or, if the shift distance is negative, a left shift
 * will be performed instead, and the vacated bit positions will be filled with 0s.
 *
 * @example
 *   0xaabbccdd.arith_rshift16(1).to_s(16) # => "aabbe66e"
 *   0xaabbccdd.arith_rshift16(2).to_s(16) # => "aabbf337"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(arith_rshift16);

static VALUE
fnum_arith_rshift32(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX(((ulong)value & ~0xFFFFFFFFUL) | arith_rshift32((uint32_t)value, shiftdist));
}

static VALUE
bnum_arith_rshift32(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo32_in_bignum(bnum, arith_rshift32(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Integer#arith_rshift32
 * Arithmetic right-shift of the low 32 bits in this integer.
 *
 * If bit 32 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
 * they will be filled with 0s. Or, if the shift distance is negative, a left shift
 * will be performed instead, and the vacated bit positions will be filled with 0s.
 *
 * @example
 *   0xaabbccddaabbccdd.arith_rshift32(1).to_s(16) # => "d55de66e"
 *   0xaabbccddaabbccdd.arith_rshift32(2).to_s(16) # => "eaaef337"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(arith_rshift32);

static VALUE
fnum_arith_rshift64(VALUE fnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return fnum;
  else
    return ULONG2NUM(arith_rshift64((uint64_t)FIX2LONG(fnum), shiftdist));
}

static VALUE
bnum_arith_rshift64(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo64_in_bignum(bnum, arith_rshift64(load_64_from_bignum(bnum), shiftdist));
}

/* Document-method: Integer#arith_rshift64
 * Arithmetic right-shift of the low 64 bits in this integer.
 *
 * If bit 64 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
 * they will be filled with 0s. Or, if the shift distance is negative, a left shift
 * will be performed instead, and the vacated bit positions will be filled with 0s.
 *
 * @example
 *   0xaabbccddaabbccdd.arith_rshift64(1).to_s(16) # => "d55de66ed55de66e"
 *   0xaabbccddaabbccdd.arith_rshift64(2).to_s(16) # => "eaaef3376aaef337"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
def_int_method_with_arg(arith_rshift64);

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

static inline uint8_t reverse8(uint8_t value)
{
  if (SIZEOF_LONG == 8)
    /* 64-bit CPU
     * Thanks to the Bit Twiddling Hacks page:
     * http://graphics.stanford.edu/~seander/bithacks.html */
    return (uint8_t)((value * 0x0202020202UL & 0x010884422010UL) % 1023);
  else
    /* 32-bit CPU */
    return bitreverse_table[value];
}

static inline uint16_t reverse16(uint16_t value)
{
  return (uint16_t)(bitreverse_table[value & 0xFF] << 8) | bitreverse_table[value >> 8];
}

static inline uint32_t reverse32(uint32_t value)
{
  return ((uint32_t)reverse16((uint16_t)value) << 16) | reverse16(value >> 16);
}

static inline uint64_t reverse64(uint64_t value)
{
  return ((uint64_t)reverse32((uint32_t)value) << 32) | reverse32(value >> 32);
}

static VALUE
fnum_bitreverse8(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    rb_raise(rb_eRangeError, "can't reverse bits in a negative number");
  return LONG2FIX((value & ~0xFFL) | reverse8((uint8_t)value));
}

static VALUE
bnum_bitreverse8(VALUE bnum)
{
  if (RBIGNUM_NEGATIVE_P(bnum))
    rb_raise(rb_eRangeError, "can't reverse bits in a negative number");
  return modify_lo8_in_bignum(bnum, reverse8((uint8_t)*RBIGNUM_DIGITS(bnum)));
}

/* Document-method: Integer#bitreverse8
 * Reverse the low 8 bits in this integer.
 *
 * If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   0b01101011.bitreverse8.to_s(2) # => "11010110"
 *
 * @return [Integer]
 */
def_int_method(bitreverse8);

static VALUE
fnum_bitreverse16(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    rb_raise(rb_eRangeError, "can't reverse bits in a negative number");
  return LONG2FIX((value & ~0xFFFFL) | reverse16((uint16_t)value));
}

static VALUE
bnum_bitreverse16(VALUE bnum)
{
  if (RBIGNUM_NEGATIVE_P(bnum))
    rb_raise(rb_eRangeError, "can't reverse bits in a negative number");
  return modify_lo16_in_bignum(bnum, reverse16((uint16_t)*RBIGNUM_DIGITS(bnum)));
}

/* Document-method: Integer#bitreverse16
 * Reverse the low 16 bits in this integer.
 *
 * If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   0b0110101100001011.bitreverse16.to_s(2) # => "1101000011010110"
 *
 * @return [Integer]
 */
def_int_method(bitreverse16);

static VALUE
fnum_bitreverse32(VALUE fnum)
{
  long     value = FIX2LONG(fnum);
  uint32_t lo32  = (uint32_t)value;
  if (value < 0)
    rb_raise(rb_eRangeError, "can't reverse bits in a negative number");
  else if (SIZEOF_LONG == 4)
    return ULONG2NUM(reverse32(lo32));
  else
    return LONG2FIX((value & ~0xFFFFFFFFL) | reverse32(lo32));
}

static VALUE
bnum_bitreverse32(VALUE bnum)
{
  if (RBIGNUM_NEGATIVE_P(bnum))
    rb_raise(rb_eRangeError, "can't reverse bits in a negative number");
  return modify_lo32_in_bignum(bnum, reverse32(*RBIGNUM_DIGITS(bnum)));
}

/* Document-method: Integer#bitreverse32
 * Reverse the low 32 bits in this integer.
 *
 * If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   0x12341234.bitreverse32.to_s(16) # => "2c482c48"
 *
 * @return [Integer]
 */
def_int_method(bitreverse32);

static VALUE
fnum_bitreverse64(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    rb_raise(rb_eRangeError, "can't reverse bits in a negative number");
  return ULL2NUM(reverse64((uint64_t)value));
}

static VALUE
bnum_bitreverse64(VALUE bnum)
{
  if (RBIGNUM_NEGATIVE_P(bnum))
    rb_raise(rb_eRangeError, "can't reverse bits in a negative number");
  return modify_lo64_in_bignum(bnum, reverse64(load_64_from_bignum(bnum)));
}

/* Document-method: Integer#bitreverse64
 * Reverse the low 64 bits in this integer.
 *
 * If the receiver is negative, raise `RangeError`.
 *
 * @example
 *   0xabcd1234abcd1234.bitreverse64.to_s(16) # => "2c48b3d52c48b3d5"
 *
 * @return [Integer]
 */
def_int_method(bitreverse64);

/* Document-class: Integer
 * Ruby's good old Integer.
 *
 * `require "bit-twiddle/core_ext"` before trying to use any of the below methods.
 */
/* Document-class: String
 * Ruby's good old String.
 *
 * `require "bit-twiddle/core_ext"` before trying to use any of the below methods.
 */

/* Add all `bit-twiddle` methods directly to `Integer`. */
static void init_core_extensions()
{
  rb_define_method(rb_cInteger, "popcount", int_popcount, 0);
  rb_define_method(rb_cString, "popcount", str_popcount,  0);

  rb_define_method(rb_cInteger, "lo_bit",   int_lo_bit, 0);
  rb_define_method(rb_cInteger, "hi_bit",   int_hi_bit, 0);

  rb_define_method(rb_cInteger, "bswap16",  int_bswap16, 0);
  rb_define_method(rb_cInteger, "bswap32",  int_bswap32, 0);
  rb_define_method(rb_cInteger, "bswap64",  int_bswap64, 0);

  rb_define_method(rb_cInteger, "rrot8",    int_rrot8,  1);
  rb_define_method(rb_cInteger, "rrot16",   int_rrot16, 1);
  rb_define_method(rb_cInteger, "rrot32",   int_rrot32, 1);
  rb_define_method(rb_cInteger, "rrot64",   int_rrot64, 1);

  rb_define_method(rb_cInteger, "lrot8",    int_lrot8,  1);
  rb_define_method(rb_cInteger, "lrot16",   int_lrot16, 1);
  rb_define_method(rb_cInteger, "lrot32",   int_lrot32, 1);
  rb_define_method(rb_cInteger, "lrot64",   int_lrot64, 1);

  rb_define_method(rb_cInteger, "lshift8",   int_lshift8,  1);
  rb_define_method(rb_cInteger, "lshift16",  int_lshift16, 1);
  rb_define_method(rb_cInteger, "lshift32",  int_lshift32, 1);
  rb_define_method(rb_cInteger, "lshift64",  int_lshift64, 1);

  rb_define_method(rb_cInteger, "rshift8",   int_rshift8,  1);
  rb_define_method(rb_cInteger, "rshift16",  int_rshift16, 1);
  rb_define_method(rb_cInteger, "rshift32",  int_rshift32, 1);
  rb_define_method(rb_cInteger, "rshift64",  int_rshift64, 1);

  rb_define_method(rb_cInteger, "arith_rshift8",  int_arith_rshift8,  1);
  rb_define_method(rb_cInteger, "arith_rshift16", int_arith_rshift16, 1);
  rb_define_method(rb_cInteger, "arith_rshift32", int_arith_rshift32, 1);
  rb_define_method(rb_cInteger, "arith_rshift64", int_arith_rshift64, 1);

  rb_define_method(rb_cInteger, "bitreverse8",  int_bitreverse8,  0);
  rb_define_method(rb_cInteger, "bitreverse16", int_bitreverse16, 0);
  rb_define_method(rb_cInteger, "bitreverse32", int_bitreverse32, 0);
  rb_define_method(rb_cInteger, "bitreverse64", int_bitreverse64, 0);
}

static VALUE
bt_add_core_extensions(VALUE self)
{
  /* this is so Yardoc can find method definitions */
  init_core_extensions();
  return Qnil;
}

/* Wrapper functions are used for methods on BitTwiddle module */
#define def_wrapper(name) \
  static VALUE bt_ ## name(VALUE self, VALUE num) \
  { \
    retry: \
    switch (TYPE(num)) { \
    case T_FIXNUM: return fnum_ ## name(num); \
    case T_BIGNUM: return bnum_ ## name(num); \
    default: num = rb_to_int(num); goto retry; \
    } \
  }
#define def_wrapper_with_arg(name) \
  static VALUE bt_ ## name(VALUE self, VALUE num, VALUE arg) \
  { \
    retry: \
    switch (TYPE(num)) { \
    case T_FIXNUM: return fnum_ ## name(num, arg); \
    case T_BIGNUM: return bnum_ ## name(num, arg); \
    default: num = rb_to_int(num); goto retry; \
    } \
  }

def_wrapper(popcount);
def_wrapper(lo_bit);
def_wrapper(hi_bit);
def_wrapper(bswap16);
def_wrapper(bswap32);
def_wrapper(bswap64);
def_wrapper_with_arg(lrot8);
def_wrapper_with_arg(lrot16);
def_wrapper_with_arg(lrot32);
def_wrapper_with_arg(lrot64);
def_wrapper_with_arg(rrot8);
def_wrapper_with_arg(rrot16);
def_wrapper_with_arg(rrot32);
def_wrapper_with_arg(rrot64);
def_wrapper_with_arg(lshift8);
def_wrapper_with_arg(lshift16);
def_wrapper_with_arg(lshift32);
def_wrapper_with_arg(lshift64);
def_wrapper_with_arg(rshift8);
def_wrapper_with_arg(rshift16);
def_wrapper_with_arg(rshift32);
def_wrapper_with_arg(rshift64);
def_wrapper_with_arg(arith_rshift8);
def_wrapper_with_arg(arith_rshift16);
def_wrapper_with_arg(arith_rshift32);
def_wrapper_with_arg(arith_rshift64);
def_wrapper(bitreverse8);
def_wrapper(bitreverse16);
def_wrapper(bitreverse32);
def_wrapper(bitreverse64);

void Init_bit_twiddle(void)
{
  VALUE rb_mBitTwiddle = rb_define_module("BitTwiddle");

  rb_define_singleton_method(rb_mBitTwiddle, "add_core_extensions", bt_add_core_extensions, 0);

  /* Return the number of 1 bits in `int`.
   * @example
   *   BitTwiddle.popcount(7)   # => 3
   *   BitTwiddle.popcount(255) # => 8
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "popcount", bt_popcount, 1);
  /* Return the index of the lowest 1 bit, where the least-significant bit is index 1.
   * If this integer is 0, return 0.
   * @example
   *   BitTwiddle.lo_bit(1)   # => 1
   *   BitTwiddle.lo_bit(128) # => 8
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lo_bit",   bt_lo_bit,   1);
  /* Return the index of the highest 1 bit, where the least-significant bit is index 1.
   * If `int` is 0, return 0.
   * @example
   *   BitTwiddle.hi_bit(1)   # => 1
   *   BitTwiddle.hi_bit(255) # => 8
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "hi_bit",   bt_hi_bit,   1);
  /* Reverse the least-significant and second least-significant bytes of `int`.
   * @example
   *   BitTwiddle.bswap16(0xFF00) # => 255
   *   BitTwiddle.bswap16(0x00FF) # => 65280
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bswap16",  bt_bswap16,  1);
  /* Reverse the least-significant 4 bytes of `int`.
   *
   * Does not reverse bits within each byte. This can be used to swap endianness
   * of a 32-bit integer.
   *
   * @example
   *   BitTwiddle.bswap32(0xaabbccdd).to_s(16) # => "ddccbbaa"
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bswap32",  bt_bswap32,  1);
  /* Reverse the least-significant 8 bytes of `int`.
   *
   * Does not reverse bits within each byte. This can be used to swap endianness
   * of a 64-bit integer.
   *
   * @example
   *   BitTwiddle.bswap64(0xaabbccdd).to_s(16) # => "ddccbbaa00000000"
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bswap64",  bt_bswap64,  1);
  /* Left-rotation ("circular shift") of the low 8 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the right
   * instead.
   *
   * @example
   *   BitTwiddle.lrot8(0b01110001, 1).to_s(2) # => "11100010"
   *   BitTwiddle.lrot8(0b01110001, 3).to_s(2) # => "10001011"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lrot8",    bt_lrot8,    2);
  /* Left-rotation ("circular shift") of the low 16 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the right
   * instead.
   *
   * @example
   *   BitTwiddle.lrot16(0b0111000101110001, 1).to_s(2) # => "1110001011100010"
   *   BitTwiddle.lrot16(0b0111000101110001, 3).to_s(2) # => "1000101110001011"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lrot16",   bt_lrot16,   2);
  /* Left-rotation ("circular shift") of the low 32 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the right
   * instead.
   *
   * @example
   *   BitTwiddle.lrot32(0xaabbccdd, 4).to_s(16) # => "abbccdda"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lrot32",   bt_lrot32,   2);
  /* Left-rotation ("circular shift") of the low 64 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the right
   * instead.
   *
   * @example
   *   BitTwiddle.lrot64(0x11223344aabbccdd, 4).to_s(16) # => "1223344aabbccdd1"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lrot64",   bt_lrot64,   2);
  /* Right-rotation ("circular shift") of the low 8 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the left
   * instead.
   *
   * @example
   *   BitTwiddle.rrot8(0b01110001, 1).to_s(2) # => "10111000"
   *   BitTwiddle.rrot8(0b01110001, 3).to_s(2) # => "101110"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rrot8",    bt_rrot8,    2);
  /* Right-rotation ("circular shift") of the low 16 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the left
   * instead.
   *
   * @example
   *   BitTwiddle.rrot16(0b0111000101110001, 1).to_s(2) # => "1011100010111000"
   *   BitTwiddle.rrot16(0b0111000101110001, 3).to_s(2) # => "10111000101110"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rrot16",   bt_rrot16,   2);
  /* Right-rotation ("circular shift") of the low 32 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the left
   * instead.
   *
   * @example
   *   BitTwiddle.rrot32(0xaabbccdd, 4).to_s(16) # => "daabbccd"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rrot32",   bt_rrot32,   2);
  /* Right-rotation ("circular shift") of the low 64 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the left
   * instead.
   *
   * @example
   *   BitTwiddle.rrot64(0x11223344aabbccdd, 4).to_s(16) # => "d11223344aabbccd"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rrot64",   bt_rrot64,   2);
  /* Left-shift of the low 8 bits in `int`.
   *
   * If the shift distance is negative, a right shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 7 or less than -7, the low 8 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.lshift8(0x11223344, 1).to_s(16) # => "11223388"
   *   BitTwiddle.lshift8(0x11223344, 2).to_s(16) # => "11223310"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lshift8",  bt_lshift8,  2);
  /* Left-shift of the low 16 bits in `int`.
   *
   * If the shift distance is negative, a right shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 15 or less than -15, the low 16 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.lshift16(0x11223344, 1).to_s(16) # => "11226688"
   *   BitTwiddle.lshift16(0x11223344, 2).to_s(16) # => "1122cd10"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lshift16", bt_lshift16, 2);
  /* Left-shift of the low 32 bits in `int`.
   *
   * If the shift distance is negative, a right shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 31 or less than -31, the low 32 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.lshift32(0x11223344, 1).to_s(16) # => "22446688"
   *   BitTwiddle.lshift32(0x11223344, 2).to_s(16) # => "4488cd10"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lshift32", bt_lshift32, 2);
  /* Left-shift of the low 64 bits in `int`.
   *
   * If the shift distance is negative, a right shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 63 or less than -63, the low 64 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.lshift64(0x1122334411223344, 1).to_s(16) # => "2244668822446688"
   *   BitTwiddle.lshift64(0x1122334411223344, 2).to_s(16) # => "4488cd104488cd10"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lshift64", bt_lshift64, 2);
  /* Right-shift of the low 8 bits in `int`.
   *
   * If the shift distance is negative, a left shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 7 or less than -7, the low 8 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.rshift8(0x11223344, 1).to_s(16) # => "11223322"
   *   BitTwiddle.rshift8(0x11223344, 2).to_s(16) # => "11223311"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rshift8",  bt_rshift8,  2);
  /* Right-shift of the low 16 bits in `int`.
   *
   * If the shift distance is negative, a left shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 15 or less than -15, the low 16 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.rshift16(0x11223344, 1).to_s(16) # => "112219a2"
   *   BitTwiddle.rshift16(0x11223344, 2).to_s(16) # => "11220cd1"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rshift16", bt_rshift16, 2);
  /* Right-shift of the low 32 bits in `int`.
   *
   * If the shift distance is negative, a left shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 31 or less than -31, the low 32 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.rshift32(0x11223344, 1).to_s(16) # => "89119a2"
   *   BitTwiddle.rshift32(0x11223344, 2).to_s(16) # => "4488cd1"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rshift32", bt_rshift32, 2);
  /* Arithmetic right-shift of the low 64 bits in `int`.
   *
   * If bit 64 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift64(0xaabbccddaabbccdd, 1).to_s(16) # => "d55de66ed55de66e"
   *   BitTwiddle.arith_rshift64(0xaabbccddaabbccdd, 2).to_s(16) # => "eaaef3376aaef337"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rshift64", bt_rshift64, 2);
  /* Arithmetic right-shift of the low 8 bits in `int`.
   *
   * If bit 8 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift8(0xaabbccdd, 1).to_s(16) # => "aabbccee"
   *   BitTwiddle.arith_rshift8(0xaabbccdd, 2).to_s(16) # => "aabbccf7"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "arith_rshift8",  bt_arith_rshift8,  2);
  /* Arithmetic right-shift of the low 16 bits in `int`.
   *
   * If bit 16 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift16(0xaabbccdd, 1).to_s(16) # => "aabbe66e"
   *   BitTwiddle.arith_rshift16(0xaabbccdd, 2).to_s(16) # => "aabbf337"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "arith_rshift16", bt_arith_rshift16, 2);
  /* Arithmetic right-shift of the low 32 bits in `int`.
   *
   * If bit 32 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift32(0xaabbccddaabbccdd, 1).to_s(16) # => "d55de66e"
   *   BitTwiddle.arith_rshift32(0xaabbccddaabbccdd, 2).to_s(16) # => "eaaef337"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "arith_rshift32", bt_arith_rshift32, 2);
  /* Arithmetic right-shift of the low 64 bits in `int`.
   *
   * If bit 64 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift64(0xaabbccddaabbccdd, 1).to_s(16) # => "d55de66ed55de66e"
   *   BitTwiddle.arith_rshift64(0xaabbccddaabbccdd, 2).to_s(16) # => "eaaef3376aaef337"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "arith_rshift64", bt_arith_rshift64, 2);
  /* Reverse the low 8 bits in `int`.
   *
   * @example
   *   BitTwiddle.bitreverse8(0b01101011).to_s(2) # => "11010110"
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bitreverse8",  bt_bitreverse8,  1);
  /* Reverse the low 16 bits in `int`.
   *
   * @example
   *   BitTwiddle.bitreverse16(0b0110101100001011).to_s(2) # => "1101000011010110"
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bitreverse16", bt_bitreverse16, 1);
  /* Reverse the low 32 bits in `int`.
   *
   * @example
   *   BitTwiddle.bitreverse32(0x12341234).to_s(16) # => "2c482c48"
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bitreverse32", bt_bitreverse32, 1);
  /* Reverse the low 64 bits in `int`.
   *
   * @example
   *   BitTwiddle.bitreverse64(0xabcd1234abcd1234).to_s(16) # => "2c48b3d52c48b3d5"
   *
   * If `int` is negative, raise `RangeError`.
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bitreverse64", bt_bitreverse64, 1);
}
