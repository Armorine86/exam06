#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

int max_fd, clients_number = 0;
int id_by_socket[65536], message[65536];
fd_set read_set, write_set, active_socket;

char read_buf[4096 * 42], write_buf[4096 * 42 + 42], msg_buf[4096 * 42];

void zero_buffers() {
	bzero(read_buf, sizeof(read_buf));
	bzero(write_buf, sizeof(write_buf));
	bzero(msg_buf, sizeof(msg_buf));
}

void send_all(int not_you) {
	for (int socket = 0; socket <= max_fd; socket++){
		if (FD_ISSET(socket, &read_set) && socket != not_you)
			send(socket, write_buf, strlen(write_buf), 0);
	}
}

void print_error(char* msg) {
	write(2, &msg, strlen(msg));
	exit(1);
}

int setup_connection(char* ip) {
	int server_socket;
	struct sockaddr_in servaddr; 

	uint16_t port = atoi(ip);
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 

	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		print_error("Fatal error\n");
	if ((bind(server_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		print_error("Fatal error\n");
	if (listen(server_socket, 10) != 0) 
		print_error("Fatal error\n");

	bzero(id_by_socket, sizeof(id_by_socket));
	FD_ZERO(&active_socket);
	FD_SET(server_socket, &active_socket);
	max_fd = server_socket;

	return server_socket;
}

void add_client(int client_socket) {
	FD_SET(client_socket, &active_socket);
	id_by_socket[client_socket] = clients_number++;
	message[client_socket] = 0;
	if (max_fd < client_socket)
		max_fd = client_socket;
}

int accept_client(int socket) {

	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);
	int client_socket = accept(socket, (struct sockaddr *)&client_socket, &len);
	if (client_socket == -1)
		return -1;
	return client_socket;
}

int main(int argc, char** argv){

	if (argc != 2)
		print_error("Wrong number of arguments\n");
	
	zero_buffers();
	
	int server_socket = setup_connection(argv[1]);

	while (42) {
		write_set = read_set = active_socket;
		if (select(max_fd + 1, &read_set, &write_set, 0, 0) <= 0)
			continue;
		
		for (int socket = 0; socket <= max_fd; socket++) {
			if (FD_ISSET(socket, &read_set)) {
				if (socket == server_socket){
					int client_socket = accept_client(socket);
					if (client_socket == -1)
						continue;
					add_client(client_socket);

					sprintf(write_buf, "server: client %d just arrived\n", id_by_socket[client_socket]);
					send_all(server_socket);
					break;
				}
			} else {

				int ret = recv(socket, read_buf, 4096 * 42, 0);

				if (ret == 0) {
					sprintf(write_buf, "server: client %d just left\n", id_by_socket[socket]);
					send_all(server_socket);

					FD_CLR(socket, &active_socket);
					close(socket);
					break;
				} else {
					for (int i = 0 , j = 0; i < ret; i++, j++) {
						msg_buf[j] = read_buf[i];

						if (msg_buf[j + 1] == '\n') {
							msg_buf[j + 1] = '\0';

							if (message[socket])
								sprintf(write_buf, "%s", msg_buf);
							else
								sprintf(write_buf, "client %d: %s", id_by_socket[socket], msg_buf);
							message[socket] = 1;
							send_all(server_socket);
							j = -1;
						} else if (i == (ret - 1)) {
							msg_buf[j+1] = '\0';
							if (message[socket])
								sprintf(msg_buf, "%s", msg_buf);
							else
								sprintf(write_buf, "client %d: %s", id_by_socket[socket], msg_buf);
							message[socket] = 1;
							send_all(server_socket);
							break;
						}
					}
				}
			}
		}
	}
	return 0;
}
