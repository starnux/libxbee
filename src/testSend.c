#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <stdlib.h>  /* exit */

#define ERR(r, t) { if((r) == -1) { perror(t); exit(1); } }

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
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 10;
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

    sleep(1);

    ERR(write(fd, "+++", 3), "write");
    ERR(size=read(fd, buf, 1024), "read");
//    buf[size] = 0;
//    printf("+++ %s\n", buf);
    sleep(1);
    ERR(write(fd, "ATAP 1\r", 7), "write");
    ERR(size=read(fd, buf, 1024), "read");
    sleep(1);
//    buf[size] = 0;
//    printf("ATAP %s\n", buf);
    ERR(write(fd, "ATCN\r", 6), "write");
    ERR(size=read(fd, buf, 1024), "read");
    sleep(1);
    //buf[size] = 0;
    //strrpl(buf, '\r', '\n');
    //printf("%s\n", buf);
    while(1)
    {
        sleep(1);
        ERR(size=read(fd, buf, 1024), "read");
        for(i = 0; i < size; ++i)
            printf("%2X,", buf[i]);
        printf("\n");
        sleep(1);
    }

    close(fd);

    return 0;
}
