### Makefile
## simple makefile for dev purpose


TIME		:= $(shell date '+%D_%H:%M'| sed 's/\//\\\//g')

CC	:= gcc

CFLAGS		+= 	   -std=gnu99 -D_GNU_SOURCE\
			   -Wall -Wextra -Werror -Wbad-function-cast -Wshadow \
			   -Wcast-qual -Wold-style-definition -Wmissing-noreturn \
			   -Wstrict-prototypes -Waggregate-return -Wformat=2 \
			   -Wundef -Wbad-function-cast -Wunused-parameter -Wnonnull 
LDFLAGS		+= -lrt -Wall

CFLAGS		+= -DPATH=\"$(shell pwd)\"

ifdef DEBUG
	CFLAGS	+= -g -ggdb -DDEBUG
	LDFLAGS	+= 
else
	CFLAGS += #-Os -fomit-frame-pointer -DNDEBUG
endif

# select C-files
sources := $(wildcard *.c)
headers := $(wildcard *.h)

# Get objects from C-files
objects := $(sources:.c=.o)

uvrrpd: $(objects)

.PHONY: all
all: uvrrpd


INDENT_ARGS := -kr -i8 -c8 -nprs -nce -l80 -cp1 
.PHONY: indent
indent:
	@echo "indent $(INDENT_ARGS)"
	@indent $(INDENT_ARGS) $(sources) > /dev/null
	@indent $(INDENT_ARGS) $(headers) > /dev/null
	@find -name "*~" -delete

.PHONY: clean
clean:
	find -name "*.o" -delete
	@rm -f uvrrpd
