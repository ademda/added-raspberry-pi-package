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

int main(){
    char message[1024];
    struct addrinfo hints, *servinfo;
    int status, client_sock_fd;
    char buf[1024];
    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    printf("Client part\n");
    if((status = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) !=0){
        perror("addressing problem\n");
        printf("addressing problem\n");
        exit(1);
    }

    if ((client_sock_fd=socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket problem\n");
        printf("socket problem\n");
        freeaddrinfo(servinfo);
        exit(1);
    }
    printf("socketing\n");

    if (connect(client_sock_fd,servinfo->ai_addr,servinfo->ai_addrlen) < 0 ){
        perror("connect problem\n");
        printf("connect problem\n");
        freeaddrinfo(servinfo);
        exit(1);
    }
    printf("connect with success \n");
    freeaddrinfo(servinfo);
    while (1){
        printf("Enter message (or 'quit' to exit): ");
        fgets(message, sizeof(message), stdin);
        message[strlen(message)-1]='\0';
        printf("message read:%s",message);
        if (send(client_sock_fd, message, strlen(message), 0) == -1) {
            perror("send");
            printf("sending problem\n");
            close(client_sock_fd);
            exit(1);
        }
        printf("client: sent message '%s' to server\n", message);
        
        int num_bytes = recv(client_sock_fd, buf, sizeof buf - 1, 0);
        if (num_bytes == -1) {
            perror("recv");
            printf("recieving problem\n");
            close(client_sock_fd);
            exit(1);
        } else if (num_bytes == 0) {
            printf("client: server closed the connection\n");
        } else {
            buf[num_bytes] = '\0';
            printf("client: received '%s' from server\n", buf);
        }
    }
    close(client_sock_fd);
    printf("client: connection closed\n");
    return 0; 
}