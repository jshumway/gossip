CFLAGS = -Wall -Wextra -Werror -pipe -fcatch-undefined-behavior -ftrapv -g

all:
	$(CC) $(CFLAGS) -o gossip gossiper.c

clean:
	$(RM) gossip
