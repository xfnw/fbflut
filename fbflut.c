
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <linux/fb.h>
#include <unistd.h>

int fbfd, fb_width, fb_height, fb_length, fb_bytes;
uint32_t *fbdata;

char *safestrtok(char *str, const char *delim) {
	char *result = strtok(str, delim);
	if (result == NULL)
		result="";
	return result;
}

void *handle_connection(void *socket_desc) {
	int sock = *(int*)socket_desc;
	int read_size, read_to;
	char *command, message[36], client_message[36];

	while ((read_size = recv(sock, client_message, 36, MSG_PEEK)) > 0) {
		client_message[read_size] = '\0';
		read_to = (char *)strchrnul(client_message, '\n')-client_message;
		memset(client_message, 0, 36);
		read_size = recv(sock, client_message, read_to+1, 0);

		command = safestrtok(client_message, " \n");

		if (!strcmp("PX", command)) {
			int xpos = atoi(safestrtok(NULL, " \n"));
			int ypos = atoi(safestrtok(NULL, " \n"));
			char colorcode[8];
			strncpy(colorcode, safestrtok(NULL, " \n"), 8);

			if (xpos >= 0 && ypos >= 0 && xpos < fb_width && ypos < fb_height) {
				if (colorcode[0] == '\0') {
					sprintf(message, "PX %i %i %X\n",xpos,ypos,fbdata[ypos*fb_length+xpos]);
					write(sock, message, strlen(message));
					continue;
				} else {
					//if (strlen(colorcode) < 2*fb_bytes)
					//	strcat(colorcode, "00");
					int color = (int)strtol(colorcode, NULL, 16);
					fbdata[ypos*fb_length+xpos] = color;
				}
			}
			continue;
		}
		if (!strcmp("SIZE", command)) {
			sprintf(message, "SIZE %i %i\n\0", fb_width, fb_height);
			write(sock, message, strlen(message));
			continue;
		}
		if (!strcmp("HELP", command)) {
			memcpy(message, "HELP\nSIZE\nPX x y [color]\n", 26);
			write(sock, message, strlen(message));
			continue;
		}

		memset(client_message, 0, 36);
	}

	close(sock);
	return 0;
}

int main(int argc, char *argv[]) {
	int port;

	port = argc > 1 ? atoi(argv[1]) : 0;
	if (!port)
		port = 1234;

	fbfd = open("/dev/fb0", O_RDWR);

	printf("\e[?25l");

	if (fbfd >= 0) {
		struct fb_var_screeninfo vinfo;
		struct fb_fix_screeninfo finfo;

		ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
		ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);

		fb_width = vinfo.xres;
		fb_height = vinfo.yres;
		fb_bytes = vinfo.bits_per_pixel / 8;
		fb_length = finfo.line_length / fb_bytes;

		printf("width: %i, height: %i, bpp: %i\n", fb_width, fb_height, fb_bytes);

		int fb_data_size = fb_length * fb_height * fb_bytes;

		fbdata = mmap(0, fb_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, (off_t)0);

		memset(fbdata, 0, fb_data_size); // clear the screen

		
		int socket_desc, client_sock, c;
		struct sockaddr_in server, client;

		socket_desc = socket(AF_INET, SOCK_STREAM, 0);

		server.sin_family = AF_INET;
		server.sin_addr.s_addr = INADDR_ANY;
		server.sin_port = htons(port);

		struct timeval tv;
		tv.tv_sec = 60;
		tv.tv_usec = 0;
		setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

		if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
			return 15;

		listen(socket_desc, 100);
		c = sizeof(struct sockaddr_in);
		pthread_t thread_id;

		while (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) {

			pthread_create(&thread_id, NULL, handle_connection, (void*) &client_sock);

		}

		return 0;
	}

}

