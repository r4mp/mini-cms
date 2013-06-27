#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

/*
#include <winsock.h>

{
	WSADATA wsaData;   // if this doesn't work
    	//WSAData wsaData; // then try this instead
    	// MAKEWORD(1,1) for Winsock 1.1, MAKEWORD(2,0) for Winsock 2.0:
	if (WSAStartup(MAKEWORD(1,1), &wsaData) != 0) 
    {
    	fprintf(stderr, "WSAStartup failed.\n");
		exit(1);
	}
}
*/

#define PORT "3490"  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold

int generate_menu(void)
{
    FILE *f = fopen("content/menu.html", "w");

    if (f == NULL) {
        return -1;
    }

    fprintf(f, "<ul id=\"menu\">");
    // TODO: fprintf(f, 
    fprintf(f, "</ul>");

    fclose(f);
}

int ae_load_file_to_memory(const char *filename, char **result) 
{ 
    int size = 0;
    FILE *f = fopen(filename, "rb");
    
    if (f == NULL) { 
        *result = NULL;
        return -1; // -1 means file opening fail 
    } 

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *result = (char *)malloc(size+1);

    if (size != fread(*result, sizeof(char), size, f)) { 
        free(*result);
        return -2; // -2 means file reading fail 
    }
 
    fclose(f);
    (*result)[size] = 0;

    return size;
}

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue; 
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1); 
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
         }

        break; 
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
 
        if (new_fd == -1) {
            perror("accept");
            continue; 
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
      
        printf("server: got connection from %s\n", s);


        char *content = malloc(0);
        char *content_res = "HTTP/1.1 200 OK\r\nServer: self\r\nContent-Type: text/html\r\n\r\n";
        int size;
        
        /* --- content/header.html --- */
        
        size = ae_load_file_to_memory("content/header.html", &content);
        
        if (size < 0) { 
            perror("Error loading content/header.html");
            return 1;
        }

        //do { 
        //    putchar(content[size-1]);
        //    size--;
        //} while(size > 0);

        //puts(content);
        //printf("%zu\n", strlen(content_res));
        content_res = malloc(strlen(content_res) + strlen(content) + 1);
        //printf("%i\n", sizeof(content_res));
        //printf("%zu\n", strlen(content));
        strncat(content_res, content, strlen(content));
        //strcat(content, content_res);
        //printf("%zu\n", strlen(content_res));
        puts(content_res);

        /* --- content/header.html --- */

        /* --- content/footer.html --- */

        size = ae_load_file_to_memory("content/footer.html", &content);
        
        if (size < 0) { 
            perror("Error loading content/footer.html");
            return 1;
        }

        //puts(content);
        content_res = malloc(strlen(content_res) + strlen(content) + 1);
        strncat(content_res, content, strlen(content));
        puts(content_res); 
        
        /* --- content/footer.html --- */

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener

            //puts(strlen(content_res));        

            if (send(new_fd, content_res, strlen(content_res), 0) == -1) {
            //if (send(new_fd, "HTTP/1.1 200 OK\r\nServer: self\r\nContent-Type: text/html\r\n\r\nab", 60, 0) == -1) {
            //if (send(new_fd, "HTTP/1.1 500 Internal Server Error\r\n\r\nab", 40, 0) == -1) {
                perror("send");
            }

            close(new_fd);
            exit(0); 
        }

        //free(content_res);
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}

