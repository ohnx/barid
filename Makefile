INCLUDES=-Iinclude/
LIBS=-lpthread
CFLAGS=$(INCLUDES) -Wall -Werror -pedantic

OBJ=objs/server.o
OUTPUT=mail

################################################################################
#                                SOURCES STUFF                                 #
################################################################################

objs/%.o: src/%.c
	@mkdir -p objs/
	$(CC) -c -o $@ $< $(CFLAGS) $(EXTRA)

$(OUTPUT): $(OBJ)
	$(CC) $< -o $@ $(LIBS)

.PHONY: clean
clean:
	-rm -f $(OBJ) objs/repl.o
	-rm -f $(OUTPUT)
	-rm -f leo
