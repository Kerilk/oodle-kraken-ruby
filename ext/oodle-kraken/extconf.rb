require 'mkmf'

dir_config("oodle_kraken_c")

$srcs = ["oodle_kraken_c.c", "ooz/bitknit.cpp", "ooz/lzna.cpp", "ooz/kraken.cpp"]

$INCFLAGS << " -I$(srcdir)/ooz"

$VPATH << "$(srcdir)/ooz"

create_makefile("oodle_kraken_c")
