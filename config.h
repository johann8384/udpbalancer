/*
 * config.h, 2005.07.18, SJ
 */

#define PROGNAME "udpbalancer"
#define VERSION "0.1"

#define LISTEN_ADDR "0.0.0.0"

#define GGID 98
#define UUID 99

#define LOG_OPTION   LOG_DEBUG
#define LOG_FACILITY LOG_LOCAL4

#define TIMEOUT 1
#define MAX_LOST_PACKET 5

#define PACKETLEN 4096
#define TMPLEN 64

#define MAXHOSTS 4
#define IPADDRLEN 16

#define BONUS 4
#define REFRESH_INTERVAL 300

#define M_ROUNDROBIN 0
#define M_LOADBALANCE 1
#define M_VOLUMEBALANCE 2

#define M_DEFAULT M_ROUNDROBIN

#define FORMAT1 "%s\n"
#define FORMAT2 "%s: %d\n"
#define FORMAT3 "%s: %s\n"

struct onehost {
   char host[IPADDRLEN];
   unsigned short int avail;
   unsigned long rtt;
   unsigned long bogomips;
   unsigned long counter;
   unsigned long lost;
   unsigned long s_addr;
};
