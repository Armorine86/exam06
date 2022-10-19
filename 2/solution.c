#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "stdlib.h"
#include "stdio.h"

int		max_fd, id = 0, arr_id[5000];
fd_set	actual_set, read_set, write_set;
char	buff_send[32], buff_read[1001], *arr_str[5000];

int	extract_message(char **buf, char **msg) {
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i]) {
		if ((*buf)[i] == '\n') {
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

char	*str_join(char *buf, char *add) {
    char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = calloc(1, sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	ft_exit_error(char *str) {
	write(2, str, strlen(str));
	exit(1);
}

void	ft_send_message(int sender_fd, char *msg) {
	for (int receiver_fd = 3; receiver_fd <= max_fd; receiver_fd++) {
		if (FD_ISSET(receiver_fd, &write_set) && receiver_fd != sender_fd) {
			send(receiver_fd, msg, strlen(msg), 0);
		}
	}
}

void	ft_register_client(int connfd) {
	max_fd = connfd > max_fd ? connfd : max_fd;
	arr_id[connfd] = id++;
	arr_str[connfd] = NULL;
	FD_SET(connfd, &actual_set);
	sprintf(buff_send, "server: client %d just arrived\n", arr_id[connfd]);
	ft_send_message(connfd, buff_send);
}

void	ft_disconnect_client(int fd) {
	sprintf(buff_send, "server: client %d just left\n", arr_id[fd]);
	ft_send_message(fd, buff_send);
	FD_CLR(fd, &actual_set);
	free(arr_str[fd]);
	close(fd);
}

void	ft_disconnect_or_send(int sockfd) {
	char	*msg;

	for (int fd = 3; fd <= max_fd; fd++) {
		if (FD_ISSET(fd, &read_set) && fd != sockfd) {

			// check connection
			size_t count = recv(fd, buff_read, 1000, 0);

			if (count <= 0) {
				ft_disconnect_client(fd);
				break;
			}
			buff_read[count] = '\0';
			arr_str[fd] = str_join(arr_str[fd], buff_read);

			// message = arr_str[fd] before the first \n
			// arr_str[fd] = arr_str[fd] after the first \n
			while (extract_message(&arr_str[fd], &msg)) {
				sprintf(buff_send, "client %d: ", arr_id[fd]);
				ft_send_message(fd, buff_send);
				ft_send_message(fd, msg);
				free(msg);
			}
          }
	}
}

int main(int argc, char **argv) {
	int	sockfd, connfd;
	socklen_t	len;
	struct	sockaddr_in servaddr, cli;

	if (argc != 2) {
		ft_exit_error("Wrong number of arguments\n");
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		ft_exit_error("Fatal error\n");
	}
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1]));

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
		ft_exit_error("Fatal error\n");
	}
	if (listen(sockfd, 10) != 0) {
		ft_exit_error("Fatal error\n");
	}
	len = sizeof(cli);
	max_fd = sockfd;
	FD_ZERO(&actual_set);
	FD_SET(sockfd, &actual_set);

	// server run
	while (1) {
		read_set = write_set = actual_set;

		// wait and add new connection
		if (select(max_fd + 1, &read_set, &write_set, NULL, NULL) < 0) {
			continue ;
		}

		// connect new client
		if (FD_ISSET(sockfd, &read_set)) {

			// wait and extract new connection
			connfd = accept(sockfd, (struct sockaddr *) &cli, &len);
			if (connfd < 0) {
				ft_exit_error("Fatal error\n");
			}
			ft_register_client(connfd);
			continue ;
		}
		ft_disconnect_or_send(sockfd);
	}
}