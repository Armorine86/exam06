#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client {
	int id;
	int fd;
	struct s_client *next;
}	t_client;

t_client *g_clients = NULL;

int max_fd, id = 0/*, arr_id[5000]*/;
fd_set read_set, write_set, actual_set;
char server_msg[42], read_buffer[4096], *arr_str[5000];

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void print_error(char *str) {
	write(2, str, strlen(str));
	exit(1);
}

void send_all(int not_you, char *msg) {
	for (int receiver_fd = 3; receiver_fd <= max_fd; receiver_fd++) {
		if (FD_ISSET(receiver_fd, &write_set) && receiver_fd != not_you)
			send(receiver_fd, msg, strlen(msg), 0);
	}
}

int add_client_to_list(int client_fd) {
	// max_fd = client_fd > max_fd ? client_fd : max_fd;
	// arr_id[client_fd] = id++;
	// arr_str[client_fd] = NULL;
	// FD_SET(client_fd, &actual_set);
	// sprintf(server_msg, "server: client %d just arrived\n", arr_id[client_fd]);
	// send_all(client_fd, server_msg);
	t_client *temp = g_clients;
    t_client *new;

    if (!(new = calloc(1, sizeof(t_client))))
        print_error("Fatal error\n");
    new->id = id++;
    new->fd = client_fd;
    new->next = NULL;
    if (!g_clients)
    {
        g_clients = new;
    }
    else
    {
        while (temp->next)
            temp = temp->next;
        temp->next = new;
    }
    return (new->id);
}

void accept_connection() {
	struct sockaddr_in clientaddr;
	socklen_t len = sizeof(clientaddr);

	int client_fd;

	if ((client_fd = accept()))
}

void remove_client(int fd) {
	sprintf(server_msg, "server: client %d just left\n", arr_id[fd]);
	send_all(fd, server_msg);
	FD_CLR(fd, &actual_set);
	free(arr_str[fd]);
	close(fd);
}

void disconnect_or_send(int not_you) {
	char *msg = NULL;

	for (int fd = 3; fd <= max_fd; fd++) {
		if (FD_ISSET(fd, &read_set) && fd != not_you) {
			size_t count = recv(fd, read_buffer, 1000, 0);

			if (count <= 0) {
				remove_client(fd);
				break;
			}
			read_buffer[count] = '\0';
			arr_str[fd] = str_join(arr_str[fd], read_buffer);

			while (extract_message(&arr_str[fd], &msg)) {
				sprintf(server_msg, "client %d: ", arr_id[fd]);
				send_all(fd, server_msg);
				send_all(fd, msg);
				free(msg);
			}
		}
	}
}

int main(int argc, char** argv) {

	if (argc != 2)
		print_error("Wrong number of arguments\n");
	socklen_t len;
	int client_fd, server_socket;
	struct sockaddr_in servaddr, cli; 

	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 

	// socket create and verification 
	server_socket = socket(AF_INET, SOCK_STREAM, 0); 
	max_fd = server_socket;

	if (server_socket == -1)
		print_error("Fatal error\n");
  
	// Binding newly created socket to given IP and verification 
	if ((bind(server_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		print_error("Fatal error\n");
	if (listen(server_socket, 10) != 0) 
		print_error("Fatal error\n");
		
	len = sizeof(cli);
	FD_ZERO(&actual_set);
	FD_SET(server_socket, &actual_set);

	while (42) {
		read_set = write_set = actual_set;

		if (select(max_fd + 1, &read_set, &write_set, 0, 0) < 0)
			continue;
		
		if (FD_ISSET(server_socket, &read_set)) {
			client_fd = accept(server_socket, (struct sockaddr *)&cli, &len);
			if (client_fd < 0) 
				print_error("Fatal error\n");
			
			add_client(client_fd);
			continue;
		}
		disconnect_or_send(server_socket);
	}
}
