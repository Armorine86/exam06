#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct s_client {
    int fd;
    int id;
    struct s_client *next;
} t_client;

t_client *client = NULL;
fd_set read_fd, write_fd, active_sockets;
char serv_bcast[50];
char read_buf[42*4096], copy_buf[42*4096], write_buf[42*4096+42];

int server_socket, g_id = 0;

void print_error(char* msg) {
    write(2, msg, strlen(msg));
    exit(1);
}

int get_max_fd() {
    t_client *tmp = client;
    int max = server_socket;

    while(tmp) {
        if (tmp->fd > max)
            max = tmp->fd;
        tmp = tmp->next;
    }
    return max;
}

void send_all(int sender_fd, char* msg) {
    t_client *tmp = client;

    while(tmp) {
        if (FD_ISSET(tmp->fd, &write_fd) && tmp->fd != sender_fd)
            if (send(tmp->fd, msg, strlen(msg), 0) < 0)
                print_error("Fatal error\n");
        tmp = tmp->next;
    }
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

void accept_connection() {
    struct sockaddr_in cli;
    socklen_t len = sizeof(cli);

    int client_fd = accept(server_socket, (struct sockaddr *)&cli, &len);
    if (client_fd < 0)
        print_error("Fatal error\n");
    
    int client_id = add_to_list(client_fd);
    bzero(serv_bcast, sizeof(serv_bcast));
    sprintf(serv_bcast, "server: client %d just arrived\n", client_id);
    send_all(server_socket, serv_bcast);
    FD_SET(client_fd, &active_sockets);
}

int get_id(int fd) {
    t_client *tmp = client;

    while (tmp) {
        if (tmp->fd == fd)
            return tmp->id;
        tmp = tmp->next;
    }
    return -1;
}

void disconnect_client(int fd) {
    t_client *tmp = client;
    t_client *to_del = NULL;

    int id = get_id(fd);

    if (tmp && tmp->fd == fd) {
        client = tmp->next;
        free(tmp);
    } else {
        while (tmp && tmp->next && tmp->next->fd != fd)
            tmp = tmp->next;
        to_del = tmp->next;
        tmp->next = tmp->next->next;
        free(to_del);
    }
    bzero(&serv_bcast, sizeof(serv_bcast));
    sprintf(serv_bcast, "server: client %d just left\n", id);
    send_all(fd, serv_bcast);
    FD_CLR(fd, &active_sockets);
    close(fd);
}

void send_msg(int fd) {
    int i = 0;
    int j = 0;

    while (read_buf[i]) {
        copy_buf[j] = read_buf[i];
        j++;
        if (read_buf[i] == '\n') {
            copy_buf[j] = '\0';
            sprintf(write_buf, "client %d: %s", get_id(fd), copy_buf);
            send_all(fd, write_buf);
            bzero(&write_buf, sizeof(write_buf));
            bzero(&copy_buf, sizeof(copy_buf));
        }
        i++;
    }
    bzero(&read_buf, sizeof(read_buf));

}

void send_or_disconnect() {
    for (int fd = 3; fd <= get_max_fd(); fd++) {
        if (FD_ISSET(fd, &read_fd)) {
            size_t count = recv(fd, read_buf, sizeof(read_buf), 0);

            if (count <= 0) {
                disconnect_client(fd);
                break;
            } else {
                send_msg(fd);
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
    bzero(&copy_buf, sizeof(copy_buf));
    bzero(&serv_bcast, sizeof(serv_bcast));

    while (42) {
        read_fd = write_fd = active_sockets;
        if (select(get_max_fd() + 1, &read_fd, &write_fd, NULL, NULL) < 0)
            continue;
        
        if (FD_ISSET(server_socket, &read_fd)) {
            accept_connection();
            continue;
        } else {
            send_or_disconnect();
        }
    }
}
