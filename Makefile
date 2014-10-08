# Which OS is this? Can be overriden by setting it first
# eg to compile Windows edition on another platform use: $ make OS_NAME=Windows
ifeq ($(OS_NAME),)
override OS_NAME=$(shell uname -s)
endif

ifeq ($(OS_NAME),Linux)
CFLAGS += -D_LINUX
else
ifeq ($(OS_NAME),Darwin)
CFLAGS += -D_MACOSX
else
$(error "Unknown/unsupported Operating System: $(OS_NAME)")
endif
endif

# Make sure we get all warnings needed for a compile that is compatible everywhere
CFLAGS += -W -Wall -Wshadow -Wpointer-arith -Wwrite-strings
CFLAGS += -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -Wmissing-declarations -Wredundant-decls -Wnested-externs
CFLAGS += -Winline -Wbad-function-cast -Wunused -Winit-self -Wextra
CFLAGS += -Wno-long-long -Wmissing-include-dirs
CFLAGS += -Wno-packed -pedantic -Wno-variadic-macros -Wswitch-default
CFLAGS += -Wformat=2 -Wformat-security -Wmissing-format-attribute
CFLAGS += -fshort-enums -fstrict-aliasing -fno-common
CFLAGS += -D_REENTRANT -D_THREAD_SAFE -pipe

all: fwparser goprom fwunpacker h3-wifi-address h4-section-patch

h3-wifi-address: crc32.o

h4-section-patch: crc32.o

clean:
	rm -f fwparser goprom fwunpacker h3-wifi-address *~

