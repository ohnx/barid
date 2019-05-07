INCLUDES+=-Iinclude/ -Idist/mbedtls/include -Idist/inih
LIBS=-lpthread -Ldist/ -lmbedtls -lmbedcrypto -lmbedx509 -linih
CFLAGS+=$(INCLUDES) -Wall -Werror -std=gnu99 -pedantic -D_DEFAULT_SOURCE -D_GNU_SOURCE

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

dist/libinih.a:
	-@git submodule update --init --recursive
	cd dist/inih; $(CC) -c -o ini.o ini.c $(CFLAGS) $(EXTRA)
	ar rcs dist/libinih.a dist/inih/ini.o

objs/%.o: src/%.c
	@mkdir -p objs/
	$(CC) -c -o $@ $< $(CFLAGS) $(EXTRA)

$(OUTPUT): dist/libmbedtls.a dist/libinih.a $(OBJ)
	$(CC) $^ -o $@ $(LIBS) $(LDFLAGS)

debug/hook_net.so: debug/hook_net.c
	$(MAKE) -C debug/

.PHONY: debug
debug: $(OUTPUT)
debug: CFLAGS+=-g -O0
	# valgrind --leak-check=full --show-leak-kinds=all ./$(OUTPUT) -p 2525 example.com example.org example.net -s

.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f $(OUTPUT)

