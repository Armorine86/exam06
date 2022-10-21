#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct s_client{
    int id;
    int fd;
    struct s_client *next;
} t_client;

t_client *clients = NULL;

int server_socket, id = 0;
fd_set read_set, write_set, actual_set;

char tmp[4096 * 42];

char read_buffer[4096 * 42], msg_buffer[4096 * 42 + 42], server_msg[50];

void print_error(char* msg) {
    write(2, msg, strlen(msg));
    exit(1);
}

int add_to_list(int client_fd) {
    t_client *head = clients;
    t_client *new_client;

    if (!(new_client = calloc(1, sizeof(t_client))))
        print_error("Fatal error\n");
    
    new_client->id = id++;
    new_client->fd = client_fd;
    new_client->next = NULL;

    if (!clients)
        clients = new_client;
    else {
        while (head->next)
            head = head->next;
        head->next = new_client;
    }
    return new_client->id;
}

int get_max_fd() {
    t_client *head = clients;
    int max = server_socket;

    while(head) {
        if (head->fd > max)
            max = head->fd;
        head = head->next;
    }
    return max;
}

void send_all(int fd, char *msg) {
    t_client *head = clients;

    while (head) {
        if (head->fd != fd && FD_ISSET(head->fd, &write_set))
            if (send(head->fd, msg, strlen(msg), 0) < 0)
                print_error("Fatal error\n");
        head = head->next;
    }
}

void accept_connection() {
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);

    int client_fd = accept(server_socket, (struct sockaddr *)&clientaddr, &len);

    if (client_fd < 0)
        print_error("Fatal error\n");

    int client_id = add_to_list(client_fd);
    bzero(server_msg, sizeof(server_msg));
    sprintf(server_msg, "server: client %d just arrived\n", client_id);
    send_all(client_fd, server_msg);
    FD_SET(client_fd, &actual_set);
}

int get_id(int fd) {
    t_client *head = clients;

    while (head) {
        if (head->fd == fd) {
            return head->id;
        }
        head = head->next;
    }
    return -1;
}

void disconnect_client(int fd) {
    t_client *head = clients;
    t_client *delete;

    int id = get_id(fd);

    if (head && head->fd == fd) {
        clients = head->next;
        free(head);
    } else {
        while (head && head->next && head->next->fd != fd)
            head = head->next;

        delete = head->next;
        head->next = head->next->next;

        free(delete);
    }

    FD_CLR(fd, &actual_set);
    close(fd);
    bzero(server_msg, sizeof(server_msg));
    sprintf(server_msg, "server: client %d just left\n", id);
    send_all(server_socket, server_msg);
}

void send_msg(int fd) {

    int i = 0;
    int j = 0;

    while (read_buffer[i]) {
        tmp[j] = read_buffer[i];
        j++;
        if (read_buffer[i] == '\n') {
            tmp[j] = '\0';
            sprintf(msg_buffer, "client %d: %s", get_id(fd), tmp);
            send_all(fd, msg_buffer);
            bzero(&tmp, sizeof(tmp));
            bzero(&msg_buffer, sizeof(msg_buffer));
            j = 0;
        }
        i++;
    }
    bzero(&read_buffer, sizeof(read_buffer));
}

void send_or_remove_client() {
    for (int fd = 3; fd <= get_max_fd(); fd++) {
        if (FD_ISSET(fd, &read_set)){
            size_t count = recv(fd, read_buffer, sizeof(read_buffer), 0);

            if (count <= 0) {
                disconnect_client(fd);
                break;
            } else {
                send_msg(fd);
            } 
        }
    }
}

void setup(char *port) {
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr)); 

    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
    servaddr.sin_port = htons(atoi(port));

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        print_error("Fatal error\n");
    if ((bind(server_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
        print_error("Fatal error\n");
    if (listen(server_socket, 10) != 0)
        print_error("Fatal error\n");
    
    FD_ZERO(&actual_set);
    FD_SET(server_socket, &actual_set);
    bzero(&read_buffer, sizeof(read_buffer));
    bzero(&msg_buffer, sizeof(msg_buffer));
    bzero(&server_msg, sizeof(server_msg));
}

int main(int argc, char** argv) {

    if (argc != 2)
        print_error("Wrong number of arguments\n");
    
    setup(argv[1]);

    while (42) {

        read_set = write_set = actual_set;
        if (select(get_max_fd() + 1, &read_set, &write_set, NULL, NULL) < 0)
            continue;

        if (FD_ISSET(server_socket, &read_set)) {
            accept_connection();
            continue;
        }
        else
            send_or_remove_client();
    }
}
