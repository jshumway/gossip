CFLAGS = -Wall -Wextra -Werror -pipe -ftrapv

all:
	$(CC) $(CFLAGS) -o gossip gossiper.c

clean:
	$(RM) gossip
