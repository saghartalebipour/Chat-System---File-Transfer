#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//#define PORT "9034" // the port client will be connecting to 
//#define listenport "3000"
#define MAXDATASIZE 100 // max number of bytes we can get at once 
#define MAXBUFLEN 100
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void add_to_set(int socket, fd_set *set, int* max){
    FD_SET(socket,set);
    if(socket> (*max)){
        *max = socket;
    }

}



int make_tcp_socket(char* port){
    printf("making tcp 1 for port : %s.\n",port);
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     
    printf("making tcp 2\n");
    if ((rv = getaddrinfo(NULL ,port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    printf("making tcp 3\n");
    for(p = servinfo; p != NULL; p = p->ai_next){
         printf("making tcp 4\n");
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        printf("making tcp 5\n");
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            //close(sockfd);
            perror("client: connect");
            continue;
        }
        printf("making tcp 6\n");
        break;
    }
   
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
    s, sizeof s);
    printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo); 
    return sockfd;
}

int make_listener_socket(char* port){

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    int listener; 

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }


        return listener;
        
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }
}

int UDP_connection(char* Nport){
    int sockfd;
    struct addrinfo hints, *servinfo, *p; 
    int rv;
    int numbytes;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL,Nport, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        int yes = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(servinfo);
    printf("listener: waiting to recvfrom...\n");
   
    return sockfd;
    //return 0;
}

int main(int argc, char *argv[])
{
    char* Nport=argv[1];
    char* listenport =argv[2];
    int has_tracker = 0 ;
    int user_has_logged_in = 0;
    int i;
    fd_set master_read;   // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    //fd_set master_write;
    //fd_set write_fds;
    int fdmax;        // maximum file descriptor number

    char* online_user = (char*)malloc(100*sizeof(char));


    //char buf[MAXBUFLEN];
    char s[INET6_ADDRSTRLEN];   
    char remoteIP[INET6_ADDRSTRLEN];
    socklen_t addr_len;                  // --*
    struct sockaddr_storage their_addr;  // --*
    addr_len = sizeof their_addr;
    struct sockaddr_storage remoteaddr;
    
    int listener = make_listener_socket(listenport);     
    int serversocket;
    int UDPsocket;
    int newfd;     
    int sockfd;   

    FD_ZERO(&master_read);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    // FD_ZERO(&master_write);
    // FD_ZERO(&writ_fds);

    FD_SET(STDIN_FILENO, &master_read);
    FD_SET(listener, &master_read);

    char first_command[128];
    char command[128];
    int first_command_length;
    
    first_command_length = read(0, first_command, 128);
    printf("first_command : %s\n",first_command);
    first_command[first_command_length - 1] = '\0';
    strncpy(command, first_command,5);
    printf("command :%s\n", command);
    if(strcmp(command,"login") == 0){
        char* user = (char*)malloc(100*sizeof(char));
        strncpy(user, first_command+5,first_command_length-5);
        strcpy(online_user,user);
        printf("login e aval shod!\n");
        char* serverport = (char*)malloc(5);
        UDPsocket=UDP_connection(Nport);
        add_to_set(UDPsocket, &master_read, &fdmax);
        for(;;){
            if((has_tracker==1)&&(user_has_logged_in==0)){
                printf("miaim too login send konim \n");
                strcat(first_command," ");
                strcat(first_command,listenport);
                printf("hala shod : %s\n",first_command );
                printf("serverport:-%s-\n", serverport);
                serversocket = make_tcp_socket(serverport);
                printf("seeend mikoniiiim !\n");
                if (send(serversocket, first_command, 258, 0) == -1)
                    perror("send");
                //printf("bade sende login\n\n\n\n");
                user_has_logged_in = 1;
            }
            read_fds = master_read;
            //write_fds = master_write;
            if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
            }
            for(i = 0; i <= fdmax; i++){
                if(FD_ISSET(i, &read_fds)){
                    if(i == listener){
                        printf("listening : \n");
                        socklen_t addrlen;
                        addrlen = sizeof remoteaddr;
                        newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);
                        if (newfd == -1) {
                            perror("accept");
                        } 
                        else {
                            FD_SET(newfd, &master_read); // add to master set
                            if (newfd > fdmax) {    // keep track of the max
                                fdmax = newfd;
                            }
                            printf("selectserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                        }
                    }
                    else if(i == UDPsocket){
                        printf(" oomad too udp ! \n" );
                        int numbytes;
                        char buf[MAXBUFLEN];
                        if ((numbytes = recvfrom(UDPsocket, buf, MAXBUFLEN-1 , 0,
                        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
                        perror("recvfrom");
                        exit(1);
                        }
                        printf("listener: got packet from %s\n",
                        inet_ntop(their_addr.ss_family,
                        get_in_addr((struct sockaddr *)&their_addr),
                        s, sizeof s));
                        printf("listener: packet is %d bytes long\n", numbytes);
                        buf[numbytes] = '\0';
                        printf("listener: packet contains \"%s\"\n", buf);
                        strcpy((serverport),buf);
                        printf("serverport : %s\n", serverport);
                        has_tracker=1;
                        close(UDPsocket);
                    }
                    else if(i == 0){
                        printf("oomad too input : %s.\n", command);
                        char cmd[128];
                        int cmd_len;
                        char *command =(char*)malloc(100*sizeof(char));
                        char *username =(char*)malloc(100*sizeof(char));
                        char *message =(char*)malloc(100*sizeof(char));
                        char *token = (char*)malloc(100*sizeof(char));
                        cmd_len= read(0, cmd, 128);
                        const char s[2] = " ";
                        char *temp = (char*)malloc(100*sizeof(char));
                        strcpy(temp, cmd);
                        command = strtok(temp,s);
                        if(strcmp(command ,"send") == 0){
                            token = strtok(NULL, " ");
                            strcpy(username, token);
                            strcat(command, " ");
                            strcat(command, username);
                            token = strtok(NULL, "\n");
                            strcpy(message, token);
                            send(serversocket, command,100,0);
                            printf("message : %s\n",message);
                            char *new_client_port = (char*)malloc(5*sizeof(char));
                            recv(serversocket, new_client_port,5,0);
                            printf("new_client_port : %s.\n",new_client_port );
                            sockfd = make_tcp_socket(new_client_port);
                            char* final_message = (char*)malloc((201)*sizeof(char));
                            strcpy(final_message,online_user);
                            strcat(final_message, " : ");
                            strcat(final_message,message);
                            if (send(sockfd, final_message, 50, 0) == -1)
                                perror("send");                    

                        }
                    }
                    else{
                        printf("oomad message ro begire\n");
                        char buf[MAXBUFLEN];
                        int nbytes;
                        if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                            if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                            } else {
                                 perror("recv");
                            }
                        //close(i); // bye!
                        FD_CLR(i, &master_read); // remove from master set
                        }
                        else{
                            printf("message : %s\n", buf);
                        }
                    }
                }
            }
        }    
    }

}