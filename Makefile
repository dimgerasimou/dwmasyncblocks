# dwmasyncblocks
# See LICENSE file for copyright and license details.

VERSION  ?= 1.0.0
CC       ?= cc

CFLAGS ?= -Os
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wpointer-arith -Wshadow -Wstrict-prototypes \
    -Wmissing-prototypes -Wold-style-definition -Wformat=2 -Wconversion -Wsign-conversion -Werror

CPPFLAGS += -MMD -MP -DVERSION=\"${VERSION}\" -I.
LDLIBS   ?= -lX11

DEBUG_CFLAGS  := -g3 -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
DEBUG_LDFLAGS := -fsanitize=address,leak,undefined

# -fanalyzer is GCC-only
ifneq (,$(findstring gcc,$(shell $(CC) --version 2>/dev/null)))
DEBUG_CFLAGS += -fanalyzer
endif

PREFIX      ?= /usr/local
MANPREFIX   ?= ${PREFIX}/share/man
BASHCOMPDIR ?= ${PREFIX}/share/bash-completion/completions
ZSHCOMPDIR  ?= ${PREFIX}/share/zsh/site-functions
FISHCOMPDIR ?= ${PREFIX}/share/fish/vendor_completions.d

SRCDIR   := src
BUILDDIR := build
BINDIR   := $(BUILDDIR)/bin
OBJDIR   := $(BUILDDIR)/obj
DOCDIR   := docs

BIN    := dwmblocks
SRCS   := dwmblocks.c
OBJS   := $(SRCS:%.c=$(OBJDIR)/%.o)
DEPS   := $(OBJS:.o=.d)

TARGET := $(BINDIR)/$(BIN)

COLOR  ?= 1
PRINTF ?= printf

ifeq ($(COLOR),0)
COLOR_RESET  :=
COLOR_GREEN  :=
COLOR_YELLOW :=
COLOR_BLUE   :=
COLOR_CYAN   :=
else
COLOR_RESET  := \033[0m
COLOR_GREEN  := \033[1;32m
COLOR_YELLOW := \033[1;33m
COLOR_BLUE   := \033[1;34m
COLOR_CYAN   := \033[1;36m
endif

all: $(TARGET)

debug: CFLAGS += $(DEBUG_CFLAGS)
debug: LDFLAGS += $(DEBUG_LDFLAGS)
debug: clean all

$(TARGET): $(OBJS) | $(BINDIR)
	@$(PRINTF) "$(COLOR_GREEN)Linking:$(COLOR_RESET) %s\n" "$@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c config.h | $(OBJDIR)
	@$(PRINTF) "$(COLOR_BLUE)Compiling:$(COLOR_RESET) %s\n" "$@"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

config.h:
	@cp config.def.h config.h

$(BINDIR) $(OBJDIR):
	@mkdir -p $@

clean:
	@$(PRINTF) "$(COLOR_YELLOW)Cleaning:$(COLOR_RESET) %s\n" "$(BUILDDIR)"
	@rm -rf $(BUILDDIR)

install: $(TARGET)
	@$(PRINTF) "$(COLOR_CYAN)Installing $(BIN) at:$(COLOR_RESET) %s\n" "$(DESTDIR)$(PREFIX)/bin/$(BIN)"
	@install -d $(DESTDIR)$(PREFIX)/bin
	@install -d $(DESTDIR)$(MANPREFIX)/man1
	@install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	@sed "s/VERSION/$(VERSION)/g" < $(DOCDIR)/$(BIN).1 > $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	@chmod 644 $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	@install -Dm644 completions/$(BIN).bash $(DESTDIR)$(BASHCOMPDIR)/$(BIN)
	@install -Dm644 completions/_$(BIN)     $(DESTDIR)$(ZSHCOMPDIR)/_$(BIN)
	@install -Dm644 completions/$(BIN).fish $(DESTDIR)$(FISHCOMPDIR)/$(BIN).fish

uninstall:
	@$(PRINTF) "$(COLOR_CYAN)Uninstalling $(BIN) from:$(COLOR_RESET) %s\n" "$(DESTDIR)$(PREFIX)/bin/$(BIN)"
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN) $(DESTDIR)$(MANPREFIX)/man1/$(BIN).1
	@rm -f $(DESTDIR)$(BASHCOMPDIR)/$(BIN) $(DESTDIR)$(ZSHCOMPDIR)/_$(BIN) \
	       $(DESTDIR)$(FISHCOMPDIR)/$(BIN).fish

-include $(DEPS)

.PHONY: all debug clean install uninstall
