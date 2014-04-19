#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// Broadcast out existence every so often.
#define WHISPER_DELAY_SEC 10
#define GOSSIP_NAME_LEN 20
#define PORT "1337"
#define PORT_NUM 1337

static char *gossip_name = NULL;

typedef struct __attribute__((__packed__)) {
   uint8_t name[GOSSIP_NAME_LEN];
} GossipPacket;

// Broadcast a gossip message.
void whisper(int sockfd)
{
   GossipPacket packet;
   int rv;
   struct sockaddr_in s;

   // The broadcast address.
   memset(&s, 0, sizeof(struct sockaddr_in));
   s.sin_family = AF_INET;
   s.sin_port = htons(PORT_NUM);
   s.sin_addr.s_addr = htonl(INADDR_BROADCAST);

   // The packet with our name.
   memset(&packet, 0, sizeof packet);
   strncpy((char *)packet.name, gossip_name, GOSSIP_NAME_LEN);

   rv = sendto(sockfd, &packet, sizeof packet, 0, (struct sockaddr *) &s,
               sizeof(struct sockaddr_in));

   if (rv == -1)
      perror("sendto");
   fprintf(stderr, "broadcast message\n");
}

void hear_whisper(int sockfd)
{
   GossipPacket packet;
   int rv;
   socklen_t addrlen;
   struct sockaddr_in s;
   static int count = 0;

   // The broadcast address.
   memset(&s, 0, sizeof(struct sockaddr_in));
   s.sin_family = AF_INET;
   s.sin_port = htons(PORT_NUM);
   s.sin_addr.s_addr = htonl(INADDR_BROADCAST);

   rv = recvfrom(sockfd, &packet, sizeof packet, 0, (struct sockaddr *) &s,
                 &addrlen);

   if (rv == -1)
      perror("recvfrom");
   else {
      if (strncmp(gossip_name, (char *)packet.name, strlen(gossip_name)) != 0)
         printf("[%d] whisper from %s\n", count++, packet.name);
      else
         fprintf(stderr, "ignoring packet from myself\n");
   }
}

// Run the gossip protocol.
void gossip(int sockfd)
{
   fd_set readfds;
   int rv, shutdown = 0;
   struct timeval timeout;
   time_t last_whisper = 0;
   time_t diff;

   while (!shutdown) {
      FD_ZERO(&readfds);
      FD_SET(sockfd, &readfds);
      FD_SET(STDIN_FILENO, &readfds);

      diff = time(NULL) - last_whisper;
      if (diff >= WHISPER_DELAY_SEC) {
         // Timeout instantly. Or just whisper now...
         timeout.tv_sec = 0;
         timeout.tv_usec = 0;
      } else {
         timeout.tv_sec = diff > WHISPER_DELAY_SEC - diff;
         timeout.tv_sec = 3;
         timeout.tv_usec = 0;
      }

      rv = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

      if (rv == -1)
         perror("select");
      else if (rv == 0) {
         whisper(sockfd);
         last_whisper = time(NULL);
      }

      if (FD_ISSET(sockfd, &readfds))
         hear_whisper(sockfd);
      if (FD_ISSET(STDIN_FILENO, &readfds))
         shutdown = 1;
   }
}

int main(int argc, char *argv[])
{
   int sockfd, rv, allow_broadcast = 1;
   struct addrinfo hints, *servinfo, *p;

   if (argc != 2) {
      printf("usage: gossiper <name>\n");
      return 1;
   }

   if (strlen(argv[1]) > GOSSIP_NAME_LEN) {
      printf("usage: gossiper <name>\n");
      return 2;
   }

   gossip_name = argv[1];

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags = AI_PASSIVE;

   if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }

   for (p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
         perror("socket");
         continue;
      }

      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
         close(sockfd);
         perror("bind");
         continue;
      }

      break;
   }

   if (p == NULL) {
      fprintf(stderr, "failed to bind\n");
      return 2;
   }

   freeaddrinfo(servinfo);

   // Setup our socket to allow broadcasting.
   setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &allow_broadcast,
              sizeof allow_broadcast);

   gossip(sockfd);
   close(sockfd);

   return 0;
}
