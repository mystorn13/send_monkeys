include $(ARUBA_SRC)/mk/Makefile.top
CC:=$(ARUBA_TOOLS_PREFIX)gcc
CFLAGS := -lpthread

all:
	$(CC) -g3 $(CFLAGS)\
		send_monkeys.c -o send_monkeys


arubainstall:
	$(INSTALL) send_monkeys $(DESTDIR)/bin
