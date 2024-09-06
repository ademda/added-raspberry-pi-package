#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>


#define PORT "9000" 
#define BACKLOG 10
int main()
{
    const char *message = "server sending data";
    int status,serv_sock_fd,bind_stat,client_sock_fd;
    struct sockaddr_storage client_addr;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    int num_bytes;
    char client_ip[INET6_ADDRSTRLEN];
    char buf[1024];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    printf("Server part\n");
    if((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) !=0){
        perror("addressing problem\n");
        printf("addressing problem\n");
        exit(1);
    }
    if((serv_sock_fd = socket(PF_INET,SOCK_STREAM,0)) < 0){
        perror("socket problem\n");
        printf("socket problem\n");
        freeaddrinfo(servinfo);
        exit(1);
    } 
    printf("socketing\n");
    
    if((bind_stat = bind(serv_sock_fd, servinfo->ai_addr, servinfo->ai_addrlen)) != 0){
        perror("binding problem\n");
        printf("binding problem\n");
        close(serv_sock_fd);
        freeaddrinfo(servinfo);
        exit(1);
    }
    printf("binding\n");
    
    
    
    if (listen(serv_sock_fd,BACKLOG) !=0){
        perror("listening problem\n");
        printf("listening problem\n");
        close(serv_sock_fd);
        freeaddrinfo(servinfo);
        exit(1);
    }
    printf("listening\n");
    socklen_t addr_size = sizeof client_addr;
    if ((client_sock_fd=accept(serv_sock_fd, (struct sockaddr *)&client_addr, &addr_size)) < 0) {
        perror("accepting problem\n");
        printf("accepting problem\n");
        close(serv_sock_fd);
        freeaddrinfo(servinfo);
        exit(1);
    }

    struct sockaddr_in *s = (struct sockaddr_in *)&client_addr;
    inet_ntop(AF_INET, &s->sin_addr, client_ip, sizeof client_ip);
    printf("Accepted connection from %s\n", client_ip);
    while (1){
        while ((num_bytes = recv(client_sock_fd, buf, sizeof buf - 1, 0)) > 0){
            buf[num_bytes] = '\0';
            int i=strcmp(buf, "send data");
            if(i==0){
                //reading data from the sensors and sending them to the client 
                send(client_sock_fd, message, strlen(message), 0);
                printf("sending data\n");
            }
            else {
                perror("msg sent problem\n");
                printf("msg sent problem\n");
                send(client_sock_fd, "wrong argument", strlen("wrong argument"), 0);
            }
            
        }
        if (num_bytes == 0){
            printf("client chose to close \n");
        }
        else if (num_bytes < 0){
            perror("error recieving from client\n");
            printf("error recieving from client\n");
        }
    }
    close(client_sock_fd);
    printf("closing client file\n");
    close(serv_sock_fd);
    printf("closing server file\n");
    freeaddrinfo(servinfo);
    printf("freeing server info\n");
    return 0;
}