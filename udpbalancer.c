/*
 * udpbalancer.c, 2005.07.18, SJ
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <stdarg.h>
#include "misc.h"
#include "config.h"

#define USAGE "usage: " PROGNAME " -l <listen address> -m <method> -p <port>, -x <host1:bogo1,host2:bogo2,..,hostN:bogoN>, ..."

#if defined (__FreeBSD__)
   #define INT_MAX 2147483647
#endif

extern char *optarg;
extern int optind;

int sd, lsd, v=0, timeout=TIMEOUT, hostnum=0;
unsigned long counter;
struct timezone tz;
struct timeval senttime, now;
struct onehost hosts[MAXHOSTS];
int bogomips[MAXHOSTS];

int fatal(const char *fmt, ...){
   va_list ap;

   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);

   #ifdef USESYSLOG
      vsyslog(LOG_OPTION, fmt, ap);
   #endif

   exit(1);
}

void clean_exit(){
   close(sd);
   close(lsd);

   #ifdef USESYSLOG
      syslog(LOG_OPTION, "terminated");
   #endif

   exit(1);
}

int choose_round_robin_host(int chost){
   int i;

   if(chost > hostnum)
      i=0;
 
   for(i=chost; i<hostnum; i++){
      if(hosts[i].avail == 1)
         return i;
   }

   for(i=0; i<chost; i++){
      if(hosts[i].avail == 1)
         return i;
   }

   return -1;
}

int choose_volume_host(){
   int i, x=0, maxbogo=0;

   // determine max. bogomips value

   for(i=0; i<hostnum; i++){
      if(hosts[i].avail == 1 && bogomips[i] > maxbogo)
         maxbogo = bogomips[i];
   }

   // choose random number

   x = rand() % maxbogo + 1;
   //fprintf(stderr, "rand: %d ** ", x);

   // choose one

   for(i=0; i<hostnum; i++){
      if(hosts[i].avail == 1 && bogomips[i] >= x)
          return i;
   }

   return -1;
}

int choose_balance_host(){
   int i, x, ix=-1, minrtt=timeout*1E6, maxbogo=0, cbogo;

   // determine which host respondes the fastest

   for(i=0; i<hostnum; i++){
      if(hosts[i].avail == 1 && hosts[i].rtt > 0 && hosts[i].rtt / hosts[i].counter < minrtt){
         minrtt = hosts[i].rtt / hosts[i].counter;
         ix = i;
      }
   }

   // determine max. bogomips value

   for(i=0; i<hostnum; i++){
      cbogo = bogomips[i];
      if(i == ix)
         cbogo += BONUS; // bonus weight for the fastest

      if(hosts[i].avail == 1 && cbogo > maxbogo)
         maxbogo = cbogo;
   }

   // choose random number

   x = rand() % maxbogo + 1;
   //fprintf(stderr, "rand: %d fastest: %s ** ", x, hosts[ix].host);

   // choose one

   for(i=0; i<hostnum; i++){
      cbogo = bogomips[i];
      if(i == ix)
         cbogo += BONUS;

      if(hosts[i].avail == 1 && cbogo >= x)
          return i;
   }

   return -1;
}

void refresh(){
   int i;

   for(i=0; i<hostnum; i++){
      hosts[i].rtt = 0;
      hosts[i].counter = 1;
      hosts[i].avail = 1;
      hosts[i].bogomips = 0;
      hosts[i].lost = 0;
   }

   alarm(REFRESH_INTERVAL);

   //fprintf(stderr, "FLUSHED\n");
}

int main(int argc, char **argv){
   int i, n, nexthost, port=0, method=M_DEFAULT, addr_len;
   char *p, *pl, *pl2, *listen_addr=LISTEN_ADDR, tmpbuf[TMPLEN];
   unsigned char packet[PACKETLEN];
   unsigned long listenhost;
   struct sockaddr_in my_addr, send_addr, client_addr;

   while((i = getopt(argc, argv, "x:l:m:p:v")) > 0){

      switch(i){
         case 'x' :
                    pl = optarg;

                    do{
                       memset((char *)&hosts[hostnum], 0, sizeof(struct onehost));

                       p = _extract(pl, ',', &tmpbuf[0], TMPLEN-1);

                       if(p){
                          p++;
                          pl = p;
                       }
                       else {
                          strncpy(&tmpbuf[0], pl, TMPLEN-1);
                       }

                       pl2 = strchr(tmpbuf, ':');
                       if(pl2){
                          *pl2 = '\0';
                          pl2++;
                          bogomips[hostnum] = atoi(pl2);
                          if(bogomips[hostnum] == 0)
                             fatal(FORMAT2, "bogomips value must be >= 1", bogomips[hostnum]);
                       }
                       else {
                          bogomips[hostnum] = 0;
                       }

                       strncpy((char *)&hosts[hostnum].host[0], &tmpbuf[0], IPADDRLEN-1);

                       hosts[hostnum].s_addr = resolve_host(hosts[hostnum].host);
                       if(hosts[hostnum].s_addr == 0)
                          fatal(FORMAT3, "invalid host", hosts[hostnum].host);

                       hostnum++;

                       if(hostnum > MAXHOSTS)
                          fatal(FORMAT2, "too many host. max:", MAXHOSTS);

                    } while(p);

                    break;

         case 'l' :
                    listen_addr = optarg;
                    break;

         case 'm' :
                    if(!strcmp(optarg, "roundrobin"))
                       method = M_ROUNDROBIN;

                    if(!strcmp(optarg, "loadbalance"))
                       method = M_LOADBALANCE;

                    if(!strcmp(optarg, "volumebalance"))
                       method = M_VOLUMEBALANCE;

                    break;

         case 'p' :
                    port = atoi(optarg);
                    if(port == 0 || port > 65535)
                       fatal(FORMAT2, "invalid port", port);

                    break;

         case 'v' :
                    v++;
                    break;

         default : 
                    fatal(FORMAT1, USAGE);
                    break;
      }
   }

   if(port == 0)
      fatal(FORMAT2, "invalid port", port);

   listenhost = resolve_host(listen_addr);

   #ifdef USESYSLOG
      (void) openlog(PROGNAME, LOG_PID, LOG_FACILITY);
      syslog(LOG_OPTION, "%s %s is starting on %s:%d", PROGNAME, VERSION, listen_addr, port);
   #endif

   signal(SIGINT, clean_exit);
   signal(SIGQUIT, clean_exit);
   signal(SIGKILL, clean_exit);
   signal(SIGTERM, clean_exit);
   signal(SIGALRM, refresh);

   /* create listener socket */

   if((lsd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
      fatal(FORMAT1, "cannot open listener socket");

   my_addr.sin_family = AF_INET;
   my_addr.sin_port = htons(port);
   my_addr.sin_addr.s_addr = listenhost;
   bzero(&(my_addr.sin_zero), 8);

   if(bind(lsd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1)
      fatal(FORMAT2, "cannot bind to port:", port);

   /* drop root privs if we have */

   if(getuid() == 0 || geteuid() == 0){
      fprintf(stderr, "dropped\n");
      drop_root();
   }

   /* create sender socket */

   if((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
      fatal(FORMAT1, "cannot open socket");

   bzero((char *)&send_addr, sizeof(struct sockaddr));
   send_addr.sin_family = AF_INET;
   send_addr.sin_port = htons(port);


   nexthost = -1;
   addr_len = sizeof(struct sockaddr);

   srand(getpid());

   refresh();

   for(;;){

      /* read request from clients */

      if((n = recvfrom(lsd,  &packet[0], PACKETLEN, 0, (struct sockaddr*)&client_addr, &addr_len)) > 0){
         //fprintf(stderr, "%d %s\n", n, inet_ntoa(client_addr.sin_addr));

         /* determine next host to forward request */

         switch(method){
            case M_ROUNDROBIN:
                               nexthost++;
                               nexthost = choose_round_robin_host(nexthost);
                               break;

            case M_VOLUMEBALANCE:
                               nexthost = choose_volume_host();
                               break;

            case M_LOADBALANCE:
                               nexthost = choose_balance_host();
                               break;
         }

         if(nexthost < 0){
            #ifdef USESYSLOG
               syslog(LOG_OPTION, "no available hosts! Refreshing.");
            #endif

            fprintf(stderr, "no available hosts! Refreshing.\n");
            refresh();

            continue;
         }

         /* send request to selected host */

         send_addr.sin_addr.s_addr = hosts[nexthost].s_addr;

         gettimeofday(&senttime, &tz);

         sendto(sd, (char *)&packet, n, 0, (struct sockaddr *)&send_addr, sizeof(struct sockaddr));

         /*
          * read answer 
          * we do not retry instead let the client repeat the request
          */

         n = readudppacket(sd, &packet[0], PACKETLEN, timeout, (struct sockaddr*)&client_addr);

         gettimeofday(&now, &tz);

         if(n > 0){
            hosts[nexthost].counter++;
            hosts[nexthost].rtt += tvdiff(now, senttime);

            if(hosts[nexthost].rtt < 0)
               hosts[nexthost].rtt = INT_MAX - 100;

            // print debug info
            if(v > 2)
               fprintf(stderr, "%s => %s:%d - %d bytes in %ld [usec]\n", inet_ntoa(client_addr.sin_addr), hosts[nexthost].host, port, n, tvdiff(now, senttime));
         }
         else {
            /* one lost packet should not discard our next node */

            hosts[nexthost].lost++;
            if(hosts[nexthost].lost > MAX_LOST_PACKET){
               hosts[nexthost].avail = 0;

               #ifdef USESYSLOG
                  syslog(LOG_OPTION, "%s-(%d) been discarded", hosts[nexthost].host, port);
               #endif

               if(v >= 1)
                  fprintf(stderr, "%s-(%d) been discarded\n", hosts[nexthost].host, port);
            }
         }

         /* send response back to your client */

         sendto(lsd, (char *)&packet, n, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr));
      }
   }

   return 0;
}
