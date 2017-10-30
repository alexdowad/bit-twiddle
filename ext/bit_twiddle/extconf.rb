require 'mkmf'

dir = File.dirname(__FILE__)

# allow 'CC' env variable to override the compiler used
if ENV['CC']
  RbConfig::MAKEFILE_CONFIG['CC'] = ENV['CC']
end

$CFLAGS << ' -Wall -Werror -O3 -march=native -mtune=native -std=c99 '

if RUBY_ENGINE == 'rbx'
  raise "bit-twiddle does not support Rubinius. Sorry!"
elsif RUBY_VERSION < '2.3.0'
  $CFLAGS << " -I#{File.join(dir, 'ruby22')} "
else
  $CFLAGS << " -I#{File.join(dir, 'ruby23')} "
end

check_sizeof 'short'
check_sizeof 'int'
check_sizeof 'long'
check_sizeof 'long long'

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