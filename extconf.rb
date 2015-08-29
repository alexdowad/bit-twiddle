require 'mkmf'
$CFLAGS << ' -Wall -Werror -march=native -mtune=native '
create_makefile 'popcount'