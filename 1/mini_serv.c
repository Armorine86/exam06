#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client {
	int fd;
	int id;
	struct s_client *next;
}	t_client;

t_client *g_client = NULL;
int sock_fd, g_id = 0;
fd_set curr_sock, cpy_read, cpy_write;
char msg[42];
char str[42*4096], tmp[42*4096], buf[42*4096 + 42];

void ft_putstr_fd(int fd, char *str) {
	for (int i = 0; str[i]; i++)
		write(fd, &str[i], 1);
}

void fatal_error() {
	ft_putstr_fd(2, "Fatal error\n");
	close(sock_fd);
	exit(1);
}

int get_max_fd() {
	int max = sock_fd;

	for (t_client *head = g_client; head; head = head->next) {
		if (head->fd > max)
			max = head->fd;
	}
	return max;
}

int add_client_to_list(int fd) {
	t_client *head = g_client;
	t_client *new;

	if (!(new = calloc(1, sizeof(t_client))))
		fatal_error();
	
	new->id = g_id++;
	new->fd = fd;
	new->next = NULL;

	if (!g_client)
		g_client = new;
	else {
		while (head->next)
			head = head->next;
		head->next = new;
	}
	return new->id;
}

void add_client() {
	struct sockaddr_in client_addr;
	socklen_t len = sizeof(client_addr);
	int client_fd = 0;

	if ((client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &len)) < 0)
		fatal_error();
	sprintf(msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
}

int main(int argc, char **argv) {

	if (argc != 2) {
		ft_putstr_fd(2, "Wrong number of argumewnts\n");
		exit(1);
	}

	int server_fd;
	struct sockaddr_in servaddr;
	uint16_t port = atoi(argv[1]);
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 
		
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		fatal_error();
  	if ((bind(server_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) < 0)
		fatal_error();
	if (listen(server_fd, 10) < 0)
		fatal_error();
	
	FD_ZERO(&curr_sock);
	FD_SET(server_fd, &curr_sock);
	bzero(&tmp, sizeof(tmp));
	bzero(&buf, sizeof(buf));
	bzero(&str, sizeof(str));

	while(42){
		cpy_write = cpy_read = curr_sock;
		if (select(get_max_fd() + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
			continue;
		
		for (int fd = 0; fd <= get_max_fd(); fd++) {
			if (FD_ISSET(fd, &cpy_read)) {
				if (fd == server_fd) {
					bzero(&msg, sizeof(msg));
					add_client();
					break;
				}
			}
		}
	}
}
