#define _GNU_SOURCE

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef HAVE_LINUX_SECCOMP_H
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#define ALLOW(syscall) \
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, SYS_##syscall, 0, 1), \
	    BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW)

#if defined(__x86_64__)
#define MY_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__i386__)
#define MY_AUDIT_ARCH AUDIT_ARCH_I386
#elif defined(__riscv) && __riscv_xlen == 64
#define MY_AUDIT_ARCH AUDIT_ARCH_RISCV64
#elif defined(__riscv) && __riscv_xlen == 32
#define MY_AUDIT_ARCH AUDIT_ARCH_RISCV32
#elif defined(__arm__)
#define MY_AUDIT_ARCH AUDIT_ARCH_ARM
#elif defined(__aarch64__)
#define MY_AUDIT_ARCH AUDIT_ARCH_AARCH64
#else
#error unknown architecture, file a bug or turn off seccomp
#endif

static int setup_seccomp() {
	struct sock_filter filter[] = {
	    /* check if architecture is the same as what we
	     * were compiled with */
	    BPF_STMT(BPF_LD + BPF_W + BPF_ABS,
		     offsetof(struct seccomp_data, arch)),
	    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, MY_AUDIT_ARCH, 1, 0),
	    BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL_PROCESS),
	    /* check syscalls */
	    BPF_STMT(BPF_LD + BPF_W + BPF_ABS,
		     offsetof(struct seccomp_data, nr)),

	    ALLOW(close),
	    ALLOW(exit),
	    ALLOW(madvise),
	    ALLOW(munmap),
#ifdef __NR_recv
	    ALLOW(recv),
#endif
	    ALLOW(recvfrom),
	    ALLOW(rt_sigprocmask),
	    ALLOW(write),
	    ALLOW(writev),
	    /* otherwise kill the process */
	    BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL_PROCESS),
	};
	struct sock_fprog prog = {.len = sizeof(filter) / sizeof(*filter),
				  .filter = filter};

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		perror("error enabling seccomp");
		return 1;
	}
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
		perror("error enabling seccomp");
		return 2;
	}
	return 0;
}
#endif

int fbfd, fb_width, fb_height, fb_length, fb_bytes, fb_hexbytes;

#define DECLARE_FBDATA2(size) uint##size##_t
#define DECLARE_FBDATA(size) DECLARE_FBDATA2(size)
#define FBDATA_T DECLARE_FBDATA(PIXELSIZE)
FBDATA_T *fbdata;

char *safestrtok(char *str, const char *delim, char **strtokptr) {
	char *result = strtok_r(str, delim, strtokptr);
	if (result == NULL)
		result = "";
	return result;
}

void *handle_connection(void *socket_desc) {
#ifdef HAVE_LINUX_SECCOMP_H
	setup_seccomp();
#endif
	pthread_detach(pthread_self());

	int sock = *(int *)socket_desc;
	int read_size = 0, message_len;
	char message[37], client_message[37] = "";
	char *offset = client_message;
	char *end_message = offset + 36;

	while (strchr(client_message, '\n') ||
	       (read_size = recv(sock, offset, end_message - offset, 0)) > 0) {
		char *newline, *command, *strtokptr;

		offset += read_size;
		*offset = '\0';
		newline = strchr(client_message, '\n');

		if (newline) {
			*newline = '\0';
		} else if (offset < end_message) {
			read_size = 0;
			continue;
		}

		command = safestrtok(client_message, " \n", &strtokptr);

		if (!strcmp("PX", command)) {
			int xpos = atoi(safestrtok(NULL, " \n", &strtokptr));
			int ypos = atoi(safestrtok(NULL, " \n", &strtokptr));
			char *colorcode = strtok_r(NULL, " \n", &strtokptr);

			if (xpos >= 0 && ypos >= 0 && xpos < fb_width &&
			    ypos < fb_height) {
				if (colorcode == NULL) {
					message_len = sprintf(
					    message, "PX %i %i %06X\n", xpos,
					    ypos,
					    fbdata[ypos * fb_length + xpos]);
					write(sock, message, message_len);
					goto reset_line;
				}
				/* TODO: re-implement ALPHA_AT_END */

				fbdata[ypos * fb_length + xpos] =
				    (FBDATA_T)strtol(colorcode, NULL, 16);
			}
			goto reset_line;
		}
		if (!strcmp("SIZE", command)) {
			message_len = sprintf(message, "SIZE %i %i\n", fb_width,
					      fb_height);
			write(sock, message, message_len);
			goto reset_line;
		}
		if (!strcmp("HELP", command)) {
			write(sock, "HELP\nSIZE\nPX x y [color]\n", 25);
			goto reset_line;
		}

	reset_line:
		read_size = 0;
		if (newline) {
			memmove(client_message, newline + 1,
				offset - newline - 1);
			offset = offset - newline - 1 + client_message;
		} else {
			offset = client_message;
		}
		*offset = '\0';
	}

	close(sock);
	return 0;
}

int main(int argc, const char *argv[]) {
	int port;

	signal(SIGPIPE, SIG_IGN);

	port = argc > 1 ? atoi(argv[1]) : 0;
	if (!port)
		port = 1234;

	/* FIXME: probably should not hard-code this,
	 * there may be more than one framebuffer */
	fbfd = open("/dev/fb0", O_RDWR);

	if (fbfd < 0) {
		perror("failed to open framebuffer");
		return 2;
	}

	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

	ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
	ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);

	fb_width = vinfo.xres;
	fb_height = vinfo.yres;
	fb_bytes = vinfo.bits_per_pixel / 8;
	fb_length = finfo.line_length / fb_bytes;
	fb_hexbytes = fb_bytes * 2;

	if (fb_bytes != sizeof *fbdata) {
		printf("compiled with -DPIXELSIZE=%li bits per pixel, "
		       "but framebuffer wants %i\n",
		       8 * sizeof(*fbdata), vinfo.bits_per_pixel);
		return 12;
	}

	/* turn off cursor blinking in the tty */
	printf("\033[?17;127cwidth: %i, height: %i, bpp: %i\n", fb_width,
	       fb_height, fb_bytes);

	int fb_data_size = fb_length * fb_height * fb_bytes;

	fbdata = mmap(0, fb_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd,
		      (off_t)0);

	/* clear the screen */
	memset(fbdata, 0, fb_data_size);

	int socket_desc, client_sock, c;

#ifdef LECACY_IP_ONLY
	struct sockaddr_in server, client;

	socket_desc = socket(AF_INET, SOCK_STREAM, 0);

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port);
#else
	struct sockaddr_in6 server, client;

	socket_desc = socket(AF_INET6, SOCK_STREAM, 0);

	server.sin6_family = AF_INET6;
	server.sin6_addr = in6addr_any;
	server.sin6_port = htons(port);
#endif

	{
		/* disconnect idle clients */
		struct timeval tv;
		tv.tv_sec = 60;
		tv.tv_usec = 0;
		setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO,
			   (const char *)&tv, sizeof tv);
	}

	if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
		perror("failed to bind");
		return 15;
	}

	listen(socket_desc, 100);
	c = sizeof(struct sockaddr_in);

	while ((client_sock = accept(socket_desc, (struct sockaddr *)&client,
				     (socklen_t *)&c))) {
		pthread_t thread_id;

		pthread_create(&thread_id, NULL, handle_connection,
			       (void *)&client_sock);
	}

	return 0;
}
