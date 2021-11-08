/*
** selectserver.c -- a cheezy multiperson chat server
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "9090"   // port we're listening on
#define LISTENPORT "9090"
struct user{
    char *username;
    int socket;
    char* IP_address;
    char* port;
};  

struct database{
    struct user *users;
    int size;
};

char* Nport;

void heartbeat(){
    int sockfd;
    struct sockaddr_in their_addr; // connector's address information
    struct hostent *he;
    int numbytes;
    int broadcast = 1;
    //char broadcast = '1'; // if that doesn't work, try this
    if ((he=gethostbyname("255.255.255.255")) == NULL) {  // get the host info
        perror("gethostbyname");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // this call is what allows broadcast packets to be sent:
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast,
        sizeof broadcast) == -1) {
        perror("setsockopt (SO_BROADCAST)");
        exit(1);
    }

    their_addr.sin_family = AF_INET;     // host byte order
    their_addr.sin_port = htons(atoi(Nport)); // short, network byte order
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);

    if ((numbytes=sendto(sockfd,LISTENPORT, 5, 0,
             (struct sockaddr *)&their_addr, sizeof their_addr)) == -1) {
        perror("sendto");
        exit(1);
    }

    printf("sent %d bytes to %s\n", numbytes,
        inet_ntoa(their_addr.sin_addr));

    close(sockfd);


    signal(SIGALRM, heartbeat);
    alarm(1);    
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
void print_user(struct user u){
    printf("name :%s \n socket:%d \n port:%s \n IP:%s \n",u.username,u.socket,u.port,u.IP_address );
}

void add_user(struct  database* db, struct user new_user){
    printf("mikhaym shooroo konim be add kardan  !");
    print_user(new_user);
    db->users[db->size] = (new_user);
    db->size++;
    printf("useretoono add kardim !");
}

void parse_command(char* buf,char** command){
   const char s[2] = " ";
   char *temp = (char*)malloc(100);
   strcpy(temp, buf);
   *command = strtok(temp, s);
}

int logged_in_before(char* username,struct database* db){
    for(int i=0;i<(db->size);i++){
        if(strcmp((db->users[i].username),username)==0){
            return 0;
        }
    }
    return 1;
}

void find_user_port(char *username,struct database* db,char** port){
    printf("db size : %d \n",db->size );
    for(int i=0;i<db->size;i++){
        if(strcmp(db->users[i].username,username)==0){
            printf(" peyda shod !\n");
            strcpy(*port,db->users[i].port);
        }
    }
}

int main(int argc, char *argv[])
{
    struct database* db;
    (db)=(struct database*)malloc(sizeof(struct database));
    db->users = (struct user*)malloc(sizeof (struct user*)*100);
    db->size=0;
    Nport=argv[1];
    heartbeat();
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;


    char buf[256];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
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

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            // perror("select");
            // exit(4);
            continue;
        }
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++){
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener){
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
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
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, 300, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        //close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        printf("data e jadid ferestad !\n");
                        printf("avvale avval : %s\n", buf);
                        char *command = (char*) malloc(100);
                        parse_command(buf,&command);
                        printf("my command:%s\n",command);
                        if(strcmp(command,"login") == 0){
                            char* username = (char*) malloc(100);
                            char *port = (char*) malloc(5);
                            char s[2] = " ";
                            command = strtok(buf,s);
                            command = strtok(NULL,s);
                            strcpy(username, command);
                            printf("username:%s.\n", username);
                            command = strtok(NULL, s);
                            strcpy(port, command);
                            printf("port:%s.\n", port);
                            printf("username:%s\n", username);
                            if(logged_in_before(username,db)==0){
                                printf("shodeee ghablan !\n");
                            }
                            else{
                                printf("nashoooode ghablan  55555!\n");
                                struct user new_user;
                                new_user.username = (char *) malloc(sizeof(char)*20);
                                new_user.IP_address = (char *) malloc(sizeof(char)*20);
                                new_user.port = (char *) malloc(sizeof(char)*20);
                                new_user.username = username;
                                new_user.socket = newfd;
                                strcpy(new_user.IP_address ,inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN));
                                strcpy(new_user.port ,port);
                                add_user(db,new_user);
                            }
                        }
                        else if(strcmp(command , "send") == 0){
                            printf("server fahmid send e !\n");
                            char* user = (char*) malloc(100);
                            char *port = (char*) malloc(5);
                            char s[2] = " ";
                            printf("cmd 1 :%s\n",buf );
                            command = strtok(buf,s);
                            printf("cmd 2 :%s\n",command );
                            command = strtok(NULL,s);
                            printf("cmd 3 :%s\n",command );
                            strcpy(user, command);
                            printf("user : %s\n",user );
                            find_user_port(user,db,&port);
                            printf("user port : %s.\n",port );
                            if (send(i, port, 5, 0) == -1)
                                perror("send");       
                        }
                        else{
                            continue;
                        }

                    }
                        // if(/*user_has_connected_before()*/0){

                        // printf("server: received : username : %s\n",buf);
                        // // we got some data from a client
                        // for(j = 0; j <= fdmax; j++) {
                        //     // send to everyone!
                        //     if (FD_ISSET(j, &master)) {
                        //         // except the listener and ourselves
                        //         if (j != listener && j != i) {
                        //             if (send(j, buf, nbytes, 0) == -1) {
                        //                 perror("send");
                        //             }
                        //         }
                        //     }
                        // }
                        // }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}