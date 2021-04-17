require 'mkmf'

dir = File.dirname(__FILE__)

# allow 'CC' env variable to override the compiler used
if ENV['CC']
  RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC']
end

$CFLAGS << ' -Wall ' # turn on all warnings for more thorough code checking
# for clang; generated Makefile contains GCC-specific -W options which clang doesn't understand
$CFLAGS << ' -Wno-unknown-warning-option '
# for clang; ruby.h contains __error__ and __deprecated__, which clang chokes on
$CFLAGS << ' -Wno-unknown-attributes -Wno-ignored-attributes '
$CFLAGS << ' -Werror ' # convert all warnings to errors so we can't ignore them
$CFLAGS << ' -O3 -march=native -mtune=native ' # full optimization
$CFLAGS << ' -std=c99 ' # use a modern version of the C standard

if RUBY_ENGINE == 'rbx'
  raise "bit-twiddle does not support Rubinius. Sorry!"
elsif RUBY_VERSION < '2.3.0'
  $CFLAGS << " -I#{File.join(dir, 'ruby22')} "
elsif RUBY_VERSION < '3.0.0'
  $CFLAGS << " -I#{File.join(dir, 'ruby23')} "
else
  $CFLAGS << " -I#{File.join(dir, 'ruby30')} "
end

check_sizeof 'short'
check_sizeof 'int'
check_sizeof 'long'
check_sizeof 'long long'

# if we already have ulong, HAVE_TYPE_ULONG will be defined as a macro
have_type 'ulong'
# likewise for uchar
have_type 'uchar'

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
