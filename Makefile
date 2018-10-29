INCLUDES=-Iinclude/
LIBS=-lpthread -Ldist/ -lmbedtls
CFLAGS=$(INCLUDES) -Wall -Werror -std=gnu99 -pedantic -g -O0

OBJ=objs/smtp.o objs/mail.o objs/server.o objs/mail_serialize.o
OUTPUT=mail

default: $(OUTPUT)

################################################################################
#                                SOURCES STUFF                                 #
################################################################################

dist/libmbedtls.a:
	-@git submodule update --init --recursive
	$(MAKE) no_test -C dist/mbedtls
	cp dist/mbedtls/library/*.a dist/

objs/%.o: src/%.c
	@mkdir -p objs/
	$(CC) -c -o $@ $< $(CFLAGS) $(EXTRA)

$(OUTPUT): dist/libmbedtls.a $(OBJ)
	$(CC) $^ -o $@ $(LIBS)

debug/hook_net.so: debug/hook_net.c
	$(MAKE) -C debug/

.PHONY: debugnet
debugnet: $(OUTPUT) debug/hook_net.so
	LD_PRELOAD=debug/hook_net.so ./$(OUTPUT)

.PHONY: debug
debug: $(OUTPUT)
	valgrind --leak-check=full --show-leak-kinds=all ./$(OUTPUT) -p 2525 example.com example.org example.net -s

.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f $(OUTPUT)
	$(MAKE) clean -C debug/
	$(MAKE) clean -C dist/mbedtls
