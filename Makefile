INCLUDES=-Iinclude/
LIBS=-lpthread
CFLAGS=$(INCLUDES) -Wall -Werror -pedantic -g -O0

OBJ=objs/smtp.o objs/mail.o objs/server.o
OUTPUT=mail

################################################################################
#                                SOURCES STUFF                                 #
################################################################################

objs/%.o: src/%.c
	@mkdir -p objs/
	$(CC) -c -o $@ $< $(CFLAGS) $(EXTRA)

$(OUTPUT): $(OBJ)
	$(CC) $^ -o $@ $(LIBS)

debug/hook_net.so: debug/hook_net.c
	$(MAKE) -C debug/

.PHONY: debugnet
debugnet: $(OUTPUT) debug/hook_net.so
	LD_PRELOAD=debug/hook_net.so ./$(OUTPUT)

.PHONY: debug
debug: $(OUTPUT)
	valgrind --leak-check=full --show-leak-kinds=all ./$(OUTPUT)

.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f $(OUTPUT)
	$(MAKE) clean -C debug/
