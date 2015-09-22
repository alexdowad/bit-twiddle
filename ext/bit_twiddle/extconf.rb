require 'mkmf'

$CFLAGS << ' -Wall -Werror -O3 -march=native -mtune=native '
check_sizeof 'BDIGIT'
check_sizeof 'int'
check_sizeof 'long'

checking_for("whether >> on a signed long is arithmetic shift or logical shift", "%s") do
  is_arith = try_static_assert("(-1L >> (sizeof(long)/8)) == -1L")
  $defs.push("-DRSHIFT_IS_ARITH=#{is_arith ? '1' : '0'}")
  is_arith ? "arithmetic" : "logical"
end

checking_for("presence of __builtin_bswap16", "%s") do
  have_bswap16 = try_static_assert("__builtin_bswap16(0xAABB) == 0xBBAA")
  $defs.push("-DHAVE_BSWAP16=#{have_bswap16 ? '1' : '0'}")
  have_bswap16 ? "oh yeah" : "nope...but we can sure fix that"
end

create_makefile 'bit_twiddle'