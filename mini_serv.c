#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct s_client {
    int fd;
    int id;
    struct s_client *next;
} t_client;

t_client *client = NULL;
int server_socket, g_id = 0;
char server_broadcast[50];
char read_buf[42*4096], cpy_buf[42*4096], write_buf[42*4096+42];
fd_set read_fd, write_fd, active_sockets;

void print_error(char *msg) {
    write(2, msg, strlen(msg));
    exit(1);
}

int get_max_fd() {
    int max = server_socket;

    for (t_client *tmp = client; tmp; tmp = tmp->next) {
        if (tmp->fd > max)
            max = tmp->fd;
    }
    return max;
}

int add_to_list(int client_fd) {
    t_client *tmp = client;
    t_client *new = NULL;

    if (!(new = calloc(1, sizeof(t_client))))
        print_error("Fatal error\n");
    
    new->fd = client_fd;
    new->id = g_id++;
    new->next = NULL;

    if (!client)
        client = new;
    else {
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = new;
    }
    return new->id;
}

void send_all(int sender_fd, char* msg) {
    for (t_client *tmp = client; tmp; tmp = tmp->next) {
        if (FD_ISSET(tmp->fd, &write_fd) && tmp->fd != sender_fd)
            if (send(tmp->fd, msg, strlen(msg), 0) < 0)
                print_error("Fatal error\n");
    }
}

void accept_connection() {
    struct sockaddr_in cli;
    socklen_t len = sizeof(cli);

    int client_fd = accept(server_socket, (struct sockaddr *)&cli, &len);
    if (client_fd < 0)
        print_error("Fatal error\n");
    
    bzero(&server_broadcast, sizeof(server_broadcast));
    sprintf(server_broadcast, "server: client %d just arrived\n", add_to_list(client_fd));
    send_all(server_socket, server_broadcast);
    FD_SET(client_fd, &active_sockets);
}

int get_client_id(int fd) {
    for (t_client *tmp = client; tmp; tmp = tmp->next) {
        if (tmp->fd == fd)
            return tmp->id;
    }
    return -1;
}

void disconnect_client(int client_fd) {
    t_client *tmp = client;
    t_client *to_del = NULL;

    int client_id = get_client_id(client_fd);

    if (tmp && tmp->fd == client_fd) {
        client = tmp->next;
        free(tmp);
    } else {
        while (tmp && tmp->next && tmp->next->fd != client_fd)
            tmp = tmp->next;
        to_del = tmp->next;
        tmp->next = tmp->next->next;
        free(to_del);
    }

    bzero(&server_broadcast, sizeof(server_broadcast));
    sprintf(server_broadcast, "server: client %d just left\n", client_id);
    send_all(server_socket, server_broadcast);
    FD_CLR(client_fd, &active_sockets);
    close(client_fd);
}

void send_message(int client_fd) {
    int i = 0;
    int j = 0;
    while (read_buf[i]) {
        cpy_buf[j] = read_buf[i];
        j++;
        if (read_buf[i] == '\n') {
            bzero(&write_buf, sizeof(write_buf));
            sprintf(write_buf, "client %d: %s", get_client_id(client_fd), cpy_buf);
            send_all(client_fd, write_buf);
            bzero(&write_buf, sizeof(write_buf));
            bzero(&cpy_buf, sizeof(cpy_buf));
            j = 0;
        }
        i++;
    }
    bzero(&read_buf, sizeof(read_buf));
}

void send_or_disconnect_client() {
    for (int fd = 3; fd <= get_max_fd(); fd++) {
        if (FD_ISSET(fd, &read_fd)) {
            ssize_t count = recv(fd, &read_buf, sizeof(read_buf), 0);
            if (count <= 0) {
                disconnect_client(fd);
                break;
            } else {
                send_message(fd);
            }
        }
    }
}

int main(int ac, char** av) {

    if (ac != 2)
        print_error("Wrong number of arguments\n");

    struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 
  
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        print_error("Fatal error\n");
	if ((bind(server_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        print_error("Fatal error\n");
	if (listen(server_socket, 10) != 0)
        print_error("Fatal error\n");
    
    FD_ZERO(&active_sockets);
    FD_SET(server_socket, &active_sockets);
    bzero(&read_buf, sizeof(read_buf));
    bzero(&write_buf, sizeof(write_buf));
    bzero(&cpy_buf, sizeof(cpy_buf));
    bzero(&server_broadcast, sizeof(server_broadcast));
    
    while (42) {
        read_fd = write_fd = active_sockets;
        if (select(get_max_fd() + 1, &read_fd, &write_fd, NULL, NULL) < 0)
            continue;
        if (FD_ISSET(server_socket, &read_fd)) {
            accept_connection();
            continue;
        } else {
            send_or_disconnect_client();
        }
    }
}
