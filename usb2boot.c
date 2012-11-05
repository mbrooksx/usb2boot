/* https://github.com/robertinant/usb2boot

  usb2boot.c

  Copyright (C) 2012 Robert Wessels
  Based on 
	- netboot.c from Ivan Tikhonov
	- fastboot package from the Android Development Team

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include <pwd.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <stdint.h>


#include "usb.h"

int match_usbboot(usb_ifc_info *info)
{
	if(!(info->dev_vendor == 0x451 && info->dev_product == 0x6141)) return 0;
	printf("vid %x pid %x\n", info->dev_vendor, info->dev_product); 

	return 1;
}

int wait_for_device(void)
{
	static open = -1;
	int announce = 1;

	for(;;) {
		open = usb_find(match_usbboot);
		if(open != 0)
			return 0;

		if(announce) {
			announce = 0;
			fprintf(stderr,"< waiting for device >\n");
		}

		sleep(1);
	}
}


int tftpd(int s) {
	static int f;
	static int blksize = 512;
	static int last_sent = 0;
	static int was_last = 0;
	{
		unsigned char p[1500];
		struct sockaddr_in peer;
		int peerl = sizeof(peer);
		int n = recvfrom(s,p,1500,0,(struct sockaddr *)&peer,&peerl);
		if(p[1] == 1) {
			unsigned char o[1500];
			printf("request %s\n", p+2);

			f = open(p+2, O_RDONLY);
			if(f == -1) {
				o[0] = 0; o[1] = 5;
				o[2] = 0; o[3] = 1;
				strcpy(o+4,"Not found");
				o[13] = 0;
				sendto(s,o,14,0,(struct sockaddr *)&peer,peerl);
				printf("error\n");
				return;
			}
		}

		int no = 0;
		if(p[1] == 4) {
			/* byte 3 and 4 indicate block number acked */
			no = (p[2]<<8)|p[3];
		}

		if(was_last) { was_last = 0; blksize = 512; last_sent = 0; close(f); f = -1; }
		if(no++ == last_sent) {
			int n;
			unsigned char o[10240] = {0,3,(no>>8)&0xff,no&0xff};
			lseek(f,(no-1)*blksize,SEEK_SET);
			n = read(f,o+4,blksize);
			sendto(s,o,n+4,0,(struct sockaddr *)&peer,peerl);
			last_sent = no;
			if(n<blksize) { 
				was_last = 1;
				printf("boot end %d packets send\n", no);
			}
		}
	}
	return was_last;
}

int main(int argc, char *argv[]) {
	int s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	int r = -1;
	int t = -1;
	int u = 0;

	unsigned char macpat[6]; int macskip = 0;

	in_addr_t bc = 0;
	in_addr_t sc = 0;
	in_addr_t cc = 0;

	if(argc > 1 && argc < 5) {
		fprintf(stderr, "Usage  : ./usb2boot <broadcast ip> <interface ip> <ip to assign> <mac>\n");
		fprintf(stderr, "Example: ./usb2boot 192.168.0.255 192.168.0.1 192.168.0.55 00:11:ee:ff:66:ef\n");
		fprintf(stderr, "Example: ./usb2boot 192.168.0.255 192.168.0.1 192.168.0.55 66-ef-\n");
		fprintf(stderr, "\nTo find out who requesting boot run: ./netboot\n");
		exit(1);
	}

	if(argc == 5) {
 		bc = inet_addr(argv[1]);
 		sc = inet_addr(argv[2]);
		cc = inet_addr(argv[3]);

		{ char *p = argv[4]; while(*p) {
			int d;
			if(p[0]>='0'&&p[0]<='9') { d = (p[0]-'0')<<4; }
			else if(p[0]>='a'&&p[0]<='f') { d = (p[0]-'a'+0xa)<<4; }
			else if(p[0]>='A'&&p[0]<='F') { d = (p[0]-'A'+0xa)<<4; }
			else { p++; continue; }
			if(p[1]>='0'&&p[1]<='9') { d = d|(p[1]-'0'); }
			else if(p[1]>='a'&&p[1]<='f') { d = d|(p[1]-'a'+0xa); }
			else if(p[1]>='A'&&p[1]<='F') { d = d|(p[1]-'A'+0xa); }

			macpat[macskip] = d;
			macskip++;
			p+=2;
		}}
	}

	wait_for_device();

	if(cc) {
		fprintf(stderr, "mac pattern:");
		{ int i = 0;
		  while(i<(macskip)) { fprintf(stderr, "%02x ",macpat[i]); i++; }
		  while(i++ < 6) { fprintf(stderr,"?? "); }
		}
		fprintf(stderr, "\n");
	}

	{ struct sockaddr_in b = { AF_INET, htons(67), {0xffffffff} };
	  if(bind(s, (struct sockaddr *)&b, sizeof(b)) != 0) {
		fprintf(stderr, "Can not bind broadcast address. DHCP will not work! Try run it as root?\n");
	  }
	}

	if(cc)
	{ struct sockaddr_in b = { AF_INET, htons(67), {sc} };
	  r = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	  if(bind(r, (struct sockaddr *)&b, sizeof(b)) != 0) {
		fprintf(stderr, "Can not bind server address. DHCP WILL NOT WORK! Try run it as root?\n");
	  }
	}

	if(cc)
	{ struct sockaddr_in b = { AF_INET, htons(69), {sc} };
 	  t = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	  if(bind(t, (struct sockaddr *)&b, sizeof(b)) != 0) {
		fprintf(stderr, "Can not bind tftp server address. TFTP WILL NOT WORK! Try run it as root?\n");
	  }
	}

	{ int v = 1;
	  setsockopt(r,SOL_SOCKET,SO_BROADCAST,&v,sizeof(v));
	}

	{
		struct passwd *nobody;
		nobody = getpwnam("nobody");
		if (nobody && nobody->pw_uid) setuid(nobody->pw_uid);
	}


	for(;;) {
		fd_set rset;
		int n;

		FD_ZERO(&rset);
		FD_SET(s,&rset);
		if(t != -1) FD_SET(t,&rset);
		n = select((t>s?t:s)+1,&rset,0,0,0);

		if(cc&&FD_ISSET(t,&rset)) {
			if(tftpd(t) && u == 1) {
				/* Done booting. Exit */
				exit(EXIT_SUCCESS);
			}
		}

		if(FD_ISSET(s,&rset)) {
			int type, pos97 = -1, pos12=-1;
			unsigned char p[1500] = {0xff};
			int l = recv(s,p,1500,0);

			type = p[0];

			/* Boot reply. Skip */
			if(type == 2) continue;
			//printf("type::%u\n", type);

			/* Options. Start after magic cookie */
			{ int i = 240;
			  for(;i<l;) {
				/* options field ends with 0xff */
				if(p[i] == 0xff) break;
				if(p[i] == 0x0) { i++; continue; }
				/* t=6 vendor class id */
				if(p[i] == 0x3c) {
					if(strncmp(&p[i+2], "AM335x ROM", 10) == 0) {
						printf("am335x ROM boot request\n"); 
						u = 0;
					}
					if(strncmp(&p[i+2], "AM335x U-Boot SPL", 17) == 0) {
						printf("am335x U-Boot request\n"); 
						u = 1;
					}
				}
				
				i += p[i+1] + 2;
			  }
			}

			/* Print MAC address and host name if any */
			printf("request %u from %02x-%02x-%02x-%02x-%02x-%02x ",type,p[28],p[29],p[30],p[31],p[32],p[33]);
			if(pos12!=-1) { printf("(%.*s)\n", p[pos12],p+pos12+1);
			} else { printf("(%s)\n", p+44); }

			/* Only listening for requests */
			if(!cc) continue;

			/* Mac address matches? */
			if(memcmp(p+28,macpat,(macskip))) continue;

			printf("matched\n");

			if(type == 1 || type == 3) {
				int i,m;
				/* Boot reply */
				p[0] = 2;
				
				p[12] = 0; p[13] = 0; p[14] = 0; p[15] = 0;
				/* Set your address (UIAddr) and server addres (SIAddr) */
				*(uint32_t*)(p+16)=(uint32_t)cc;
				*(uint32_t*)(p+20)=(uint32_t)sc;

				i = 108;
				if(u) {
					p[i++] = 'u'; p[i++] = '-'; p[i++] = 'b'; p[i++] = 'o'; p[i++] = 'o'; p[i++] = 't'; p[i++] = '.'; p[i++] = 'i'; p[i++] = 'm'; p[i++] = 'g';
				}  else {
					p[i++] = 'M'; p[i++] = 'L'; p[i++] = 'O';
				}

				i = 240;

				p[i++] = 51; p[i++] = 4;
				p[i++] = 255; p[i++] = 255; p[i++] = 255; p[i++] = 255;

				p[i++] = 54; p[i++] = 4;
				*(uint32_t*)(p+i)=(uint32_t)sc; i+=4;

				p[i++] = 60; p[i++] = 17;
				p[i++] = 'T'; p[i++] = 'e'; p[i++] = 'x'; p[i++] = 'a'; p[i++] = 's'; p[i++] = ' '; p[i++] = 'I'; p[i++] = 'n'; p[i++] = 's'; p[i++] = 't'; p[i++] = 'r'; p[i++] = 'u'; p[i++] = 'm'; p[i++] = 'e'; p[i++] = 'n'; p[i++] = 't'; p[i++] = 's';

				p[i++] = 255;

				{ struct sockaddr_in a = { AF_INET, htons(68), {bc} };
				  sendto(r,p,i,0,(struct sockaddr *)&a,sizeof(a));
				}
			}
		}
	}
}
