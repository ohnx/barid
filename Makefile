INCLUDES+=-Iinclude/ -Idist/mbedtls/include -Idist/inih -Idist/libspf2/src/include
LIBS=-lpthread -Ldist/ -lmbedtls -lmbedcrypto -lmbedx509 -linih -lspf2 -l:libresolv.a
CFLAGS+=$(INCLUDES) -Wall -Werror -Wextra -std=c99 -pedantic -D_DEFAULT_SOURCE -D_GNU_SOURCE
CFLAGS+=-DUSE_PTHREADS

OBJ=objs/server.o objs/logger.o objs/networker.o objs/serworker.o objs/net.o
OBJ+=objs/smtp.o objs/mail.o
OUTPUT=barid

default: $(OUTPUT)

################################################################################
#                                SOURCES STUFF                                 #
################################################################################

dist/libmbedtls.a:
	-@git submodule update --init --recursive
	cd dist/mbedtls; scripts/config.pl set MBEDTLS_THREADING_C; scripts/config.pl set MBEDTLS_THREADING_PTHREAD;
	$(MAKE) lib -C dist/mbedtls
	cp dist/mbedtls/library/*.a dist/

dist/libspf2.a:
	-@git submodule update --init --recursive
	cd dist/libspf2; ./configure
	$(MAKE) -C dist/libspf2
	cp dist/libspf2/src/libspf2/.libs/libspf2.a dist/

dist/libinih.a:
	-@git submodule update --init --recursive
	cd dist/inih; $(CC) -c -o ini.o ini.c $(CFLAGS) $(EXTRA)
	ar rcs dist/libinih.a dist/inih/ini.o

objs/%.o: src/%.c
	@mkdir -p objs/
	$(CC) -c -o $@ $< $(CFLAGS) $(EXTRA)

$(OUTPUT): dist/libmbedtls.a dist/libspf2.a dist/libinih.a $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS) $(CFLAGS)

.PHONY: debug
debug: CFLAGS += -g -O0
debug: $(OUTPUT)
debug: CFLAGS+=-g -O0 -DUSE_PTHREADS -DDEBUG

.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f $(OUTPUT)
