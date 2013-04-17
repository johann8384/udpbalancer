/*
 * misc.h, 2005.07.18, SJ
 */

void drop_root();
long tvdiff(struct timeval a, struct timeval b);
char *_extract(char *row, int ch, char *s, int size);
unsigned long resolve_host(char *h);
int readudppacket(int sd, char *buf, int size, int timeout, struct sockaddr *client_addr);


