#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <stdlib.h>  /* exit */
#include <sys/types.h> //socket
#include <sys/socket.h>  //socket
#include <netinet/in.h> //socket ip
#include <arpa/inet.h> //inet_aton
#include <fcntl.h> //fcntl

#define ERR(r, t) { if((r) == -1) { perror(t); exit(1); } }
#define PORT_TCP 4537
#define BACKLOG 100

void setbaud(int fd)
{
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    options.c_cflag |= CREAD;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag &= ~CLOCAL;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    //options.c_cc[VMIN]  = 0;
    //options.c_cc[VTIME] = 10;
    tcsetattr(fd, TCSANOW, &options);
}

void strrpl(const char * str, char what, char rpl)
{
    char * where = strchr(str, what);
    while(where != NULL)
    {
        *where = rpl;
        where = strchr(str, what);
    }
}

unsigned char calc_checksum(char * data, int n)
{
    unsigned short result = 0;
    char checksum = 0;
    int i;
    for(i = 3 ; i < n ; ++i)
        result += data[i];
    //result &= 0xFF;
    return 0xFF - result&0xFF;
    //return checksum;
}

void pere(int fd)
{
    int socket_tcp;
    struct sockaddr_in addr_tcp;
    ERR(socket_tcp = socket(PF_INET, SOCK_STREAM, 0), "socket");
    fcntl(socket_tcp, F_SETFL, O_NONBLOCK);
    bzero(&addr_tcp, sizeof(struct sockaddr_in));
    addr_tcp.sin_family = AF_INET;
    addr_tcp.sin_port = htons(PORT_TCP);
    addr_tcp.sin_addr.s_addr = INADDR_ANY;
    ERR(bind(socket_tcp, (struct sockaddr *)&addr_tcp, sizeof(struct sockaddr_in)), "bind");
    ERR(listen(socket_tcp, BACKLOG), "listen");
    printf("Ecoute Port %i\n", PORT_TCP);
    //ecoute le réseau et envoie
    while(1)
    {
        fd_set rfds;
        int retval;
        FD_ZERO(&rfds);
        FD_SET(socket_tcp, &rfds);
        ERR(retval = select(socket_tcp+1, &rfds, NULL, NULL, NULL), "select");
        if(retval)
        {
            printf("Client connecté\n");
            char buffer[1024];
            int sd, size, ret=0;
            sd = accept(socket_tcp, NULL, 0);
            while(ret >= 0)
            {
                fd_set fdset;
                FD_ZERO(&fdset);
                FD_SET(sd, &fdset);
                ret = select(sd+1, &fdset, NULL, NULL, NULL);
                if(ret)
                {
                    ERR(size = read(sd, buffer, 1024), "read");
                    buffer[size] = 0;
                    ERR(write(fd, buffer, size), "write");
                }
            }
            close(sd);
        }
    }
}

void fils(int fd)
{
    //Ecoute et affiche
    while(1)
    {
        int retval, size, i;
        fd_set rfds;
        unsigned char buf;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        ERR(retval = select(fd+1, &rfds, NULL, NULL, NULL), "select");
        if (retval)
        {
            ERR(size=read(fd, &buf, 1), "read");
            //printf("%c", buf);
            //for(i = 0; i < size; ++i)
                printf("%2X,", buf);
            //printf("\n");
            fflush(stdout);
        }
    }
}

int main(int argc, char **argv)
{
    char port[100], buf[1024], buf2[2048];
    int fd, size, i;

    if(argc > 1)
        strcpy(port, argv[1]);
    else
        strcpy(port, "/dev/ttyS0");

    ERR(fd = open(port, O_RDWR | O_NOCTTY), "open");
    setbaud(fd);

    if(fork())
        pere(fd);
    else
        fils(fd);

    return 0;
}
/*    
    sleep(1);

    ERR(write(fd, "+++", 3), "write");
    ERR(size=read(fd, buf, 1024), "read");
    sleep(1);
    ERR(write(fd, "ATAP 1\r", 7), "write");
    ERR(size=read(fd, buf, 1024), "read");
    sleep(1);
    ERR(write(fd, "ATCN\r", 5), "write");
    ERR(size=read(fd, buf, 1024), "read");
    sleep(1);
*/
/*
    ERR(write(fd, "+++", 3), "write");
    ERR(size=read(fd, buf, 1024), "read");
    buf[size] = 0;
    printf("+++ %s\n", buf);
    sleep(1);
    ERR(write(fd, "ATAP 1\r", 6), "write");
    sleep(1);
    ERR(size=read(fd, buf, 1024), "read");
    buf[size] = 0;
    printf("ATAP %s\n", buf);
    sleep(2);
    */
    /*ERR(write(fd, "ATCN \r", 6), "write");
    ERR(size=read(fd, buf, 1024), "read");
    buf[size] = 0;
    printf("ATCN %s\n", buf);*/
//    while(1)
//    {
        /*char * where = NULL, * dest = buf2;
        ERR(size=read(fd, buf, 1024), "read");
        buf[size] = 0;
        snprintf(buf2, 2048, "%s%s", buf, buf2);
        if( (where = strchr(buf2, '\r')) != NULL)
        {
            *where = 0;
            printf("%s\n", buf2);
            ++where;
            while(*where != 0)
                *(dest++) = *(where++);
            *dest = 0;
        }*/
        //envoie la meme trame a tt le monde tt les secondes avec ACK
/*        unsigned char trame[14] = {0x7E, 0, 0x4, 0x08, 0x52,
                        0x44, 0x4C,
                        0,
                        0x48, 0x45, 0x4C, 0x4C, 0x4F,
                        0};
        trame[7] = calc_checksum(trame, 7);
        printf("%2X\n", trame[7]);
        ERR(write(fd, buf, 8), "write");
        sleep(1);
        ERR(size=read(fd, buf, 1024), "read");
        for(i = 0; i < size; ++i)
            printf("%2X,", buf[i]);
        printf("\n");
        sleep(1);
    }

    close(fd);

    return 0;
}*/
