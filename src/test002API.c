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
#include "libxbee.h" //libxbee

#define ERR(r, t) { if((r) == -1) { perror(t); exit(1); } }
#define PORT_TCP 4538
#define BACKLOG 100
#define BUF_SIZE 2048

uint8_t broadcast_address_64[8] = { 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0xFF, 0xFF };

void print_frame_data(struct api_frame * frame)
{
    switch(frame->cmdid)
    {
        case XBEE_CMD_AT_RESPONSE:
            break;
        case XBEE_CMD_TRANSMIT_STATUS:
            break;
        case XBEE_CMD_RECEIVE_16:
            break;
        case XBEE_CMD_RECEIVE_64:
            break;
        default:
            printf("Unknown Data type !\n");
    };
}

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

void send_to_frame(int fd, uint8_t * data, size_t size)
{
    struct api_frame frm, frm2;
    struct api_transmit_64 trans;
    uint8_t frame_buffer[BUF_SIZE];
    size_t frame_size;
    int i;
    //Prepare Trans
    memcpy(trans.dest_address, broadcast_address_64, 8);
    trans.frame_id = 20;
    trans.options = 0;
    memcpy(trans.rf_data, data, (size>100?100:size));
    trans.size = (size>100?100:size);
    //Prepare Frame
    frm.cmdid = 0x00;
    //Build frame data
    make_frame_data_transmit_64(&trans, &frm);
    frame_size=make_frame(&frm, frame_buffer, BUF_SIZE);
    //Send Frame
    ERR(write(fd, frame_buffer, frame_size), "write");
    cut_frame(frame_buffer, frame_size, &frm2);
    printf("Frame sent rsize %i size %i id %2X checksum %2X\nData:", frame_size, frm2.size, frm2.cmdid, frm2.checksum);
    for(i = 0; i < frm2.data_size; ++i)
        printf("%2X,", frm2.data[i]);
    printf("\n");

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
                    send_to_frame(fd, buffer, size);
                }
            }
            close(sd);
        }
    }
}

void fils(int fd)
{
    uint8_t m_buffer[BUF_SIZE];
    size_t buf_size = 0;
    //Ecoute et affiche
    while(1)
    {
        int retval, size, i, pos = 0, sz;
        fd_set rfds;
        unsigned char buf[1024];
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        ERR(retval = select(fd+1, &rfds, NULL, NULL, NULL), "select");
        if (retval)
        {
            ERR(size=read(fd, buf, 1024), "read");
            buf[size] = 0;
            if(size+buf_size > BUF_SIZE)
            {
                memcpy((m_buffer+buf_size), buf, BUF_SIZE-buf_size);
                buf_size = BUF_SIZE;
                printf("Buffer Overflow !\n");
            }
            else
            {
                memcpy((m_buffer+buf_size), buf, size);
                buf_size += size;
            }
            printf("Buffer before : ");
            for(i = 0; i < buf_size; ++i)
                printf("%2X,", m_buffer[i]);
            printf("\n");
            if((pos=has_frame(m_buffer, buf_size)) >= 0)
            {
                struct api_frame frm;
                size_t pos2 = 0;
                uint8_t chk2;
                printf("Has Frame pos:%i\n", pos);
                sz= cut_frame(m_buffer+pos, buf_size-pos, &frm);
                chk2 = calc_checksum(m_buffer+pos, 3+frm.size);
                printf("Frame rsize %i size %i id %2X checksum %2X %2X\nData:", sz, frm.size, frm.cmdid, frm.checksum, chk2);
                for(i = 0; i < frm.data_size; ++i)
                    printf("%2X,", frm.data[i]);
                printf("\n");
                //Retire l'ancien et les déchets si pos > 0
                if(chk2 == frm.checksum)
                {
                    buf_size -= pos + sz;
                    for(pos2 = 0 ; pos < buf_size ; ++pos, ++pos2)
                        m_buffer[pos2] = m_buffer[pos];

                    printf("Buffer after : ");
                    for(i = 0; i < buf_size; ++i)
                        printf("%2X,", m_buffer[i]);
                    printf("\n");
                }
            }
        }
    }
}

void setAPI(int fd)
{
    size_t size;
    uint8_t buf[1024];
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
    setAPI(fd);

    if(fork())
        pere(fd);
    else
        fils(fd);

    return 0;
}

