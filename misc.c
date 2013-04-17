/*
 * misc.c, 2005.07.18, SJ
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "config.h"

void drop_root(){
   setgid(GGID);
   setuid(UUID);
}

long tvdiff(struct timeval a, struct timeval b){
   double res;

   res = (a.tv_sec * 1000000 + a.tv_usec) - (b.tv_sec * 1000000 + b.tv_usec);
   return (long) res;
}

char *_extract(char *row, int ch, char *s, int size){
   char *r;
   int len;

   if(row == NULL) return NULL;

   if((r = strchr(row,ch)) == NULL) return NULL;
   if(s != NULL){
       len = strlen(row) - strlen(r);
       if(len > size) len = size;
       strncpy(s, row, len);
       s[len] = '\0';
   }

   return r;
}

unsigned long resolve_host(char *h){
   struct hostent *host;
   struct in_addr addr;

   if((addr.s_addr = inet_addr(h)) == -1){
       if((host = gethostbyname(h)) == NULL){
          return 0;
       }
       else return *(unsigned long*)host->h_addr;
   }
   else return addr.s_addr;
}

int readudppacket(int sd, char *buf, int size, int timeout, struct sockaddr *client_addr){
   struct timeval tv;
   fd_set rfds;
   int retval, n, size_of;

   size_of = sizeof(struct sockaddr);

   FD_ZERO(&rfds);
   FD_SET(sd, &rfds);

   tv.tv_sec = timeout;
   tv.tv_usec = 0;

   retval = select(sd+1, &rfds, NULL, NULL, &tv);

   /* 0: timeout, -1: error */

   if(retval <= 0)
      return 0;

   n = recvfrom(sd, buf, size, 0, (struct sockaddr *)&client_addr, &size_of);

   return n;
}

