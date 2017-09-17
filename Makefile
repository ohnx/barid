INCLUDES=-Iinclude/
LIBS=-lpthread
CFLAGS=$(INCLUDES) -Wall -Werror -pedantic

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



debug/hook_send.so: debug/hook_send.c
	$(MAKE) -C debug/

.PHONY: debug
debug: $(OUTPUT) debug/hook_send.so
	LD_PRELOAD=debug/hook_send.so ./$(OUTPUT)

.PHONY: clean
clean:
	-rm -f $(OBJ)
	-rm -f $(OUTPUT)
	$(MAKE) clean -C debug/
