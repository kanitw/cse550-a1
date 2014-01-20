CC=/usr/bin/gcc
CC_OPTS=-g3
CC_LIBS=
CC_DEFINES=
CC_INCLUDES=
CC_ARGS=${CC_OPTS} ${CC_LIBS} ${CC_DEFINES} ${CC_INCLUDES}

# clean is not a file
.PHONY=clean

#target "all" depends on all others
all: client server

# client C depends on source file client.c, if that changes, make client will 
# rebuild the binary
client: client.c
	@${CC} ${CC_ARGS} -o client client.c

server: server.c
	@${CC} ${CC_ARGS} -o server server.c -pthread

clean:
	@rm -f server client *.o
