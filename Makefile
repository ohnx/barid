INCLUDES+=-Iinclude/ -Idist/mbedtls/include -Idist/inih -Idist/libspf2/src/include
INCLUDES+=-Idist/jansson/src
LIBS=-lpthread -Ldist/ -lmbedtls -lmbedcrypto -lmbedx509 -linih -lspf2
LIBS+=-l:libresolv.a -lcurl -ljansson
CFLAGS+=$(INCLUDES) -Wall -Werror -Wextra -std=c99 -pedantic -D_DEFAULT_SOURCE -D_GNU_SOURCE
CFLAGS+=-DUSE_PTHREADS

OBJ=objs/server.o objs/logger.o objs/networker.o objs/serworker.o objs/net.o
OBJ+=objs/smtp.o objs/mail.o objs/mail_serialize.o objs/gotodo.o
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

JANSSON_VERSION=2.14
dist/libjansson.a:
	-@git submodule update --init --recursive
	cd dist/jansson; autoreconf -i
	cd dist/jansson; ./configure
	$(MAKE) -C dist/jansson
	cp dist/jansson/src/.libs/libjansson.a dist/

objs/%.o: src/%.c
	@mkdir -p objs/
	$(CC) -c -o $@ $< $(CFLAGS) $(EXTRA)

$(OUTPUT): dist/libmbedtls.a dist/libspf2.a dist/libinih.a dist/libjansson.a $(OBJ)
	$(CC) $(OBJ) -o $@ $(LIBS) $(CFLAGS)

gotodo_test: CFLAGS += -g -O0 -DGOTODO_TEST
gotodo_test: dist/libjansson.a objs/gotodo.o objs/mail.o objs/logger.o
	$(CC) objs/gotodo.o objs/mail.o objs/logger.o -o $@ $(LIBS) $(CFLAGS)

.PHONY: libs
libs: dist/libmbedtls.a dist/libspf2.a dist/libinih.a dist/libjansson.a

.PHONY: debug
debug: CFLAGS += -g -O0
debug: $(OUTPUT)
debug: CFLAGS+=-g -O0 -DUSE_PTHREADS -DDEBUG

.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f $(OUTPUT) gotodo_test
