/*
 * Copyright (c) 2012, Thingsquare, http://www.thingsquare.com/.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>

#ifdef linux
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#define DEVTAP "/dev/net/tun"
#else  /* linux */
#define DEVTAP "/dev/tap0"
#endif /* linux */

#include "contiki-net.h"

static int fd;

/*---------------------------------------------------------------------------*/
static void
remove_route(void)
{
  char buf[1024];
  snprintf(buf, sizeof(buf), "route delete -net 172.16.0.0");
  system(buf);
  printf("%s\n", buf);
}
/*---------------------------------------------------------------------------*/
void
ip64_tap_init(void)
{
  char buf[1024];

  fd = open(DEVTAP, O_RDWR);
  if(fd == -1) {
    perror("tapdev: tapdev_init: open");
    return;
  }

#ifdef linux
  {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if(ioctl(fd, TUNSETIFF, (void *) &ifr) < 0) {
      perror(buf);
      exit(1);
    }
  }
#endif /* Linux */

  snprintf(buf, sizeof(buf), "ifconfig tap0 inet 172.16.0.2 172.16.0.1");
  system(buf);
  printf("%s\n", buf);

#ifdef linux
  /* route add for linux */
  snprintf(buf, sizeof(buf), "route add -net 172.16.0.0/16 gw 172.16.0.1");
#else /* linux */
  /* route add for freebsd */
  snprintf(buf, sizeof(buf), "route add -net 172.16.0.0/16 172.16.0.1");
#endif /* linux */
  
  system(buf);
  printf("%s\n", buf);
  atexit(remove_route);

}
/*---------------------------------------------------------------------------*/
uint16_t
ip64_tap_poll(uint8_t *buf, const uint16_t buflen)
{
  fd_set fdset;
  struct timeval tv;
  int ret;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&fdset);
  if(fd > 0) {
    FD_SET(fd, &fdset);
  }

  ret = select(fd + 1, &fdset, NULL, NULL, &tv);

  if(ret == 0) {
    return 0;
  }
  ret = read(fd, buf, buflen);

  if(ret == -1) {
    perror("ip64_tap_poll: read");
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
void
ip64_tap_send(uint8_t *packet, uint16_t len)
{
  int ret;

  if(fd <= 0) {
    return;
  }

  printf("ip64_tap_send: sending %d bytes\n", len);

  ret = write(fd, packet, len);

  if(ret == -1) {
    perror("ip64_tap error, write:");
    exit(1);
  }
}
/*---------------------------------------------------------------------------*/
