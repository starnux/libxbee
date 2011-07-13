#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#ifndef WIN32
#include <unistd.h>  /* UNIX standard function definitions */
#include <termios.h> /* POSIX terminal control definitions */
#endif
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <stdlib.h>  /* exit */

#ifndef WIN32
#include <sys/types.h> /* socket */
#include <sys/socket.h>  /* socket */
#include <netinet/in.h> /* socket ip */
#include <arpa/inet.h> /* inet_aton */
#else
#include <Winsock2.h>
#endif

#include <signal.h> /* kill */
#include "libxbee.h" /* libxbee */

/* Define DEBUG for Active Debugging for received packets */
/* #define _DEBUG_ 1 */

#define ERR(r, t) { if((r) == -1) { perror(t); exit(1); } }
#define BUF_SIZE 2048
#define BACKLOG 100
#define COMM_PORT "COM1"
#define TCP_PORT 5876

#define TCP_FRAME_ERROR         0xFF
#define TCP_FRAME_RECEIVE       '0'
#define TCP_FRAME_SCAN          '1'
#define TCP_FRAME_COMMAND       '2'
#define TCP_FRAME_SEND_STATUS   '0'
#define TCP_FRAME_SEND_SCAN     '1'
#define TCP_FRAME_SEND_RECEIVE  '3'

uint8_t broadcast_address_64[8] = { 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0xFF, 0xFF };

struct receive_tcp_frame
{
    uint8_t type;
    uint8_t address[8];
    uint8_t command[2];
    uint8_t frame_id;
    uint8_t data[BUF_SIZE];
    size_t data_size;
};

struct send_tcp_frame
{
    uint8_t type;
    uint8_t frame_id;
    uint8_t status;
    uint8_t address[8];
    uint8_t rssi;
    uint8_t data[BUF_SIZE];
    size_t data_size;
};

#ifdef _DEBUG_
void debug_xbee_frame(struct api_frame * frame, void * data)
{
    size_t i;
    fprintf(stderr, "DEBUG: Received XBee Frame ID %02X\n", frame->cmdid);
    switch(frame->cmdid)
    {
        case XBEE_CMD_AT_RESPONSE:
            fprintf(stderr, "DEBUG: XBEE_CMD_AT_RESPONSE ID %02X CMD %c%c STATUS %02X VALUE SIZE %i\n", 
                        ((struct api_at_response *)data)->frame_id, ((struct api_at_response *)data)->command[0], ((struct api_at_response *)data)->command[1],
                        ((struct api_at_response *)data)->status, ((struct api_at_response *)data)->size);
            break;
        case XBEE_CMD_TRANSMIT_STATUS:
            fprintf(stderr, "DEBUG: XBEE_CMD_TRANSMIT_STATUS ID %02X STATUS %02X\n", 
                        ((struct api_transmit_status *)data)->frame_id, ((struct api_transmit_status *)data)->status);
            break;
        case XBEE_CMD_RECEIVE_64:
            fprintf(stderr, "DEBUG: XBEE_CMD_RECEIVE_64 SOURCE ");
            for(i = 0 ; i < 8 ; ++i)
                fprintf(stderr, "%02X", ((struct api_receive_64 *)data)->source_address[i]);
            fprintf(stderr, " RSSI %02X OPTIONS %02X VALUE SIZE %i\n", 
                        ((struct api_receive_64 *)data)->RSSI, ((struct api_receive_64 *)data)->options, ((struct api_receive_64 *)data)->size);
            break;
        case XBEE_CMD_RECEIVE_16:
            fprintf(stderr, "DEBUG: XBEE_CMD_RECEIVE_16 SOURCE ");
            for(i = 0 ; i < 2 ; ++i)
                fprintf(stderr, "%02X", ((struct api_receive_16 *)data)->source_address[i]);
            fprintf(stderr, " RSSI %02X OPTIONS %02X VALUE SIZE %i\n", 
                        ((struct api_receive_16 *)data)->RSSI, ((struct api_receive_16 *)data)->options, ((struct api_receive_16 *)data)->size);
            break;
    };
}

void debug_tcp_frame(struct receive_tcp_frame * frame)
{
    size_t i;
    fprintf(stderr, "DEBUG: Received TCP Frame Type %02X\n", frame->type);
    switch(frame->type)
    {
        case TCP_FRAME_RECEIVE:
            fprintf(stderr, "DEBUG: TCP_FRAME_RECEIVE address ");
            for(i = 0 ; i < 8 ; ++i)
                fprintf(stderr, "%02X", frame->address[i]);
            fprintf(stderr, " frame_id %02X Data Size %i\n", frame->frame_id, frame->data_size);
            break;
        case TCP_FRAME_SCAN:
            fprintf(stderr, "DEBUG: TCP_FRAME_SCAN frame_id %02X\n", frame->frame_id);
            break;
        case TCP_FRAME_COMMAND:
            fprintf(stderr, "DEBUG: TCP_FRAME_COMMAND frame_id %02X COMMAND %c%c\n", frame->frame_id, frame->command[0], frame->command[1]);
            break;

    };
}

#endif

//WRITE
int writeComm(int fd, uint8_t * buffer, size_t size)
{
	return 0;
}

//WRITE
int readComm(int fd, uint8_t * buffer, size_t size)
{
	return 0;
}

//TEST
void send_to_frame(int fd, struct receive_tcp_frame * tcp_frame)
{
    struct api_frame frm;
    struct api_transmit_64 trans;
    uint8_t frame_buffer[BUF_SIZE];
    size_t frame_size;
    int i;
    /* Prepare Trans */
    memcpy(trans.dest_address, tcp_frame->address, 8);
    trans.frame_id = tcp_frame->frame_id;
    trans.options = 0;
    memcpy(trans.rf_data, tcp_frame->data, (tcp_frame->data_size>100?100:tcp_frame->data_size));
    trans.size = (tcp_frame->data_size>100?100:tcp_frame->data_size);
    /* Prepare Frame */
    frm.cmdid = 0x00;
    /* Build frame data */
    make_frame_data_transmit_64(&trans, &frm);
    frame_size=make_frame(&frm, frame_buffer, BUF_SIZE);
    /* Send Frame */
    ERR(writeComm(fd, frame_buffer, frame_size), "write");
    #ifdef _DEBUG_
    fprintf(stderr, "DEBUG: Sent XBee frame [");
    for(i = 0 ; i < frame_size; ++i)
        fprintf(stderr, "%02X ", frame_buffer[i]);
    fprintf(stderr, "]\n");
    #endif
}

//TEST
void send_scan(int fd, struct receive_tcp_frame * tcp_frame)
{
    struct api_frame frm;
    struct api_at_command cmd;
    uint8_t frame_buffer[BUF_SIZE];
    size_t frame_size;
    int i;
    /* Prepare Cmd */
    cmd.frame_id = tcp_frame->frame_id;
    cmd.command[0] = 'N';
    cmd.command[1] = 'D';
    cmd.size = 0;
    /* Prepare Frame */
    frm.cmdid = 0x08;
    /* Build Frame Data */
    make_frame_data_at_command(&cmd, &frm);
    frame_size=make_frame(&frm, frame_buffer, BUF_SIZE);
    /* Send Frame */
    ERR(writeComm(fd, frame_buffer, frame_size), "write");
    #ifdef _DEBUG_
    fprintf(stderr, "DEBUG: Sent XBee frame [");
    for(i = 0 ; i < frame_size; ++i)
        fprintf(stderr, "%02X ", frame_buffer[i]);
    fprintf(stderr, "]\n");
    #endif
}

//TEST
void send_command(int fd, struct receive_tcp_frame * tcp_frame)
{
    struct api_frame frm;
    struct api_at_command cmd;
    uint8_t frame_buffer[BUF_SIZE];
    size_t frame_size;
    int i;
    /* Prepare Cmd */
    cmd.frame_id = tcp_frame->frame_id;
    cmd.command[0] = tcp_frame->command[0];
    cmd.command[1] = tcp_frame->command[1];
    memcpy(cmd.parameter, tcp_frame->data, tcp_frame->data_size);
    cmd.size = tcp_frame->data_size;
    /* Prepare Frame */
    frm.cmdid = 0x08;
    /* Build Frame Data */
    make_frame_data_at_command(&cmd, &frm);
    frame_size=make_frame(&frm, frame_buffer, BUF_SIZE);
    /* Send Frame */
    ERR(writeComm(fd, frame_buffer, frame_size), "write");
    #ifdef _DEBUG_
    fprintf(stderr, "DEBUG: Sent XBee frame [");
    for(i = 0 ; i < frame_size; ++i)
        fprintf(stderr, "%02X ", frame_buffer[i]);
    fprintf(stderr, "]\n");
    #endif
}

//OK
void cut_tcp_frame(uint8_t * buffer, size_t size, struct receive_tcp_frame * tcp_frame)
{
    tcp_frame->type = TCP_FRAME_ERROR;
    switch(buffer[0])
    {
        case TCP_FRAME_RECEIVE:
        {
            if(size < 10)
                return;
            tcp_frame->type = buffer[0];
            tcp_frame->data_size = (size-10>BUF_SIZE?BUF_SIZE:size-10);
            memcpy(tcp_frame->address, buffer+1, 8);
            tcp_frame->frame_id = buffer[9];
            memcpy(tcp_frame->data, buffer+10, tcp_frame->data_size);
        }break;
        case TCP_FRAME_SCAN:
        {
            if(size < 2)
                return;
            tcp_frame->frame_id = buffer[1];
            tcp_frame->type = buffer[0];
        }break;
        case TCP_FRAME_COMMAND:
        {
            if(size < 4)
                return;
            tcp_frame->frame_id = buffer[1];
            tcp_frame->type = buffer[0];
            tcp_frame->command[0] = buffer[2];
            tcp_frame->command[1] = buffer[3];
            memcpy(tcp_frame->data, buffer+4, size-4);
            tcp_frame->data_size = size-4;
        }break;
    }
}

//OK
int make_tcp_frame(uint8_t * buffer, size_t size, struct send_tcp_frame * tcp_frame)
{
    switch(tcp_frame->type)
    {
        case TCP_FRAME_SEND_STATUS:
        {
            if(size < 3)
                return -1;
            buffer[0] = TCP_FRAME_SEND_STATUS;
            buffer[1] = tcp_frame->frame_id;
            buffer[2] = tcp_frame->status;
            return 3;
        }break;
        case TCP_FRAME_SEND_SCAN:
        {
            if(size < 11+tcp_frame->data_size)
                return -1;
            buffer[0] = TCP_FRAME_SEND_SCAN;
            buffer[1] = tcp_frame->frame_id;
            memcpy(buffer+2, tcp_frame->address, 8);
            buffer[10] = tcp_frame->rssi;
            memcpy(buffer+11, tcp_frame->data, tcp_frame->data_size);
            return 11+tcp_frame->data_size;
        }break;
        case TCP_FRAME_COMMAND:
        {
            if(size < 3+tcp_frame->data_size)
                return -1;
            buffer[0] = TCP_FRAME_COMMAND;
            buffer[1] = tcp_frame->frame_id;
            buffer[2] = tcp_frame->status;
            memcpy(buffer+3, tcp_frame->data, tcp_frame->data_size);
            return 3+tcp_frame->data_size;
        }break;
        case TCP_FRAME_SEND_RECEIVE:
        {
            if(size < 9+tcp_frame->data_size)
                return -1;
            buffer[0] = TCP_FRAME_SEND_RECEIVE;
            memcpy(buffer+1, tcp_frame->address, 8);
            memcpy(buffer+9, tcp_frame->data, tcp_frame->data_size);
            return 9+tcp_frame->data_size;
        }break;
    }
    return -1;
}

//TEST
void networkListen(int sd, int fd)
{
    fd_set fdset;
    int ret = 1;
    size_t size;
    uint8_t buffer[BUF_SIZE];
    struct receive_tcp_frame tcp_frame;
    while(ret > 0)
    {
        FD_ZERO(&fdset);
        FD_SET(sd, &fdset);
        ret = select(sd+1, &fdset, NULL, NULL, NULL);
        if(ret)
        {
            size = 0;
            size = recv(sd, buffer, BUF_SIZE, 0);
            if(size <= 0)
                return;
            buffer[size] = 0;
            cut_tcp_frame(buffer, size, &tcp_frame);
            #ifdef _DEBUG_
            debug_tcp_frame(&tcp_frame);
            #endif
            switch(tcp_frame.type)
            {
                case TCP_FRAME_RECEIVE:
                {
                    send_to_frame(fd, &tcp_frame);
                }break;
                case TCP_FRAME_SCAN:
                {
                    send_scan(fd, &tcp_frame);
                }break;
                case TCP_FRAME_COMMAND:
                {
                    send_command(fd, &tcp_frame);
                }break;
                default:
                    fprintf(stderr, "Unknown TCP Frame type!\n");
            }
        }
    }
}

void xbeeListen(int sd, int fd)
{
    uint8_t m_buffer[BUF_SIZE];
    size_t buf_size = 0;
    struct send_tcp_frame tcp_frm;
    while(1)
    {
        int retval, size, i, pos = 0, sz;
        unsigned char buf[1024];
        /*fd_set rfds;
        unsigned char buf[1024];
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        ERR(retval = select(fd+1, &rfds, NULL, NULL, NULL), "select");
        if (retval)
        {*/
            ERR(size=readComm(fd, buf, 1024), "read");
            buf[size] = 0;
            if(size+buf_size > BUF_SIZE)
            {
                memcpy((m_buffer+buf_size), buf, BUF_SIZE-buf_size);
                buf_size = BUF_SIZE;
                fprintf(stderr, "Buffer Overflow !\n");
            }
            else
            {
                memcpy((m_buffer+buf_size), buf, size);
                buf_size += size;
            }
            #ifdef _DEBUG_
            fprintf(stderr, "DEBUG: Buffer ");
            for(i=0; i < buf_size;++i)
                fprintf(stderr, "%02X ", m_buffer[i]);
            fprintf(stderr, "\n");
            #endif
            if((pos=has_frame(m_buffer, buf_size)) >= 0)
            {
                struct api_frame frm;
                size_t pos2 = 0;
                sz= cut_frame(m_buffer+pos, buf_size-pos, &frm);
                tcp_frm.type = TCP_FRAME_ERROR;
                switch(frm.cmdid)
                {
                    case XBEE_CMD_AT_RESPONSE:
                    {
                        struct api_at_response api;
                        cut_frame_data_at_response(&frm, &api);
                        if(api.command[0]=='N' && api.command[1]=='D' &&
                            api.status == 0x00 && api.size >= 12)
                        {
                            tcp_frm.type = TCP_FRAME_SEND_SCAN;
                            tcp_frm.frame_id = api.frame_id;
                            tcp_frm.rssi = api.value[10];
                            memcpy(tcp_frm.address, api.value+2, 8);
                            memcpy(tcp_frm.data, api.value+11, api.size-12);
                            tcp_frm.data_size = api.size-12;
                        }
                        else
                        {
                            tcp_frm.type = TCP_FRAME_COMMAND;
                            tcp_frm.frame_id = api.frame_id;
                            tcp_frm.status = api.status;
                            memcpy(tcp_frm.data, api.value, api.size);
                            tcp_frm.data_size = api.size;
                        }
                        #ifdef _DEBUG_
                        debug_xbee_frame(&frm, &api);
                        #endif
                    }break;
                    case XBEE_CMD_TRANSMIT_STATUS:
                    {
                        struct api_transmit_status api;
                        cut_frame_data_transmit_status(&frm, &api);
                        tcp_frm.type = TCP_FRAME_SEND_STATUS;
                        tcp_frm.status = api.status;
                        #ifdef _DEBUG_
                        debug_xbee_frame(&frm, &api);
                        #endif
                    }break;
                    case XBEE_CMD_RECEIVE_16:
                    {
                        struct api_receive_16 api;
                        cut_frame_data_receive_16(&frm, &api);
                        tcp_frm.type = TCP_FRAME_SEND_RECEIVE;
                        memcpy(tcp_frm.address, api.source_address, 2);
                        memset(tcp_frm.address+2, 0, 6);
                        memcpy(tcp_frm.data, api.rf_data, api.size);
                        tcp_frm.data_size = api.size;
                        #ifdef _DEBUG_
                        debug_xbee_frame(&frm, &api);
                        #endif
                    }break;
                    case XBEE_CMD_RECEIVE_64:
                    {
                        struct api_receive_64 api;
                        cut_frame_data_receive_64(&frm, &api);
                        tcp_frm.type = TCP_FRAME_SEND_RECEIVE;
                        memcpy(tcp_frm.address, api.source_address, 8);
                        memcpy(tcp_frm.data, api.rf_data, api.size);
                        tcp_frm.data_size = api.size;
                        #ifdef _DEBUG_
                        debug_xbee_frame(&frm, &api);
                        #endif
                    } break;
                    default:
                        #ifdef _DEBUG_
                        debug_xbee_frame(&frm, NULL);
                        #endif
                        fprintf(stderr, "Unknown API Frame\n");
                };
                buf_size -= pos + sz;
                for(pos2 = 0 ; pos < buf_size ; ++pos, ++pos2)
                    m_buffer[pos2] = m_buffer[pos];
                if(tcp_frm.type != TCP_FRAME_ERROR)
                {
                    size = make_tcp_frame(buf, 1024, &tcp_frm);
                    #ifdef _DEBUG_
                    fprintf(stderr, "DEBUG: Make TCP_FRAME Data:");
                    for(i=0; i < size; ++i)
                        fprintf(stderr, "%02X ", buf[i]);
                    fprintf(stderr, "\n");
                    #endif
                    if(size > 0)
                    {
                        #ifdef _DEBUG_
                        fprintf(stderr, "DEBUG: Sending TCP_FRAME\n");
                        #endif
                        ERR(send(sd, buf, size, 0), "write");
                        #ifdef _DEBUG_
                        fprintf(stderr, "DEBUG: Sent TCP_FRAME\n");
                        #endif
                    }
                }
            }
        //}
    } 
}

//OK
void setAPI(int fd)
{
    size_t size;
    char buf[1024];
    ERR(writeComm(fd, "+++", 3), "write");
    ERR(size=readComm(fd, buf, 1024), "read");
    if(strcmp(buf, "OK\r")!=0){ fprintf(stderr, "Module Not Responding\n"); exit(1);}
    fprintf(stderr, "XBee Command Mode : %s\n", buf);
    /*ERR(write(fd, "ATMY FFFF\r", 10), "write");
    ERR(size=read(fd, buf, 1024), "read");
    if(strcmp(buf, "OK\r")!=0){ fprintf(stderr, "Module Not Responding\n"); exit(1);}
    fprintf(stderr, "XBee Disable 16Bit : %s\n", buf);*/

    ERR(writeComm(fd, "ATAP 1\r", 7), "write");
    ERR(size=readComm(fd, buf, 1024), "read");
    if(strcmp(buf, "OK\r")!=0){ fprintf(stderr, "Module Not Responding\n"); exit(1);}
    fprintf(stderr, "XBee API Mode : %s\n", buf);
    ERR(writeComm(fd, "ATCN\r", 5), "write");
    ERR(size=readComm(fd, buf, 1024), "read");
    if(strcmp(buf, "OK\r")!=0){ fprintf(stderr, "Module Not Responding\n"); exit(1);}
    fprintf(stderr, "XBee Exit Command Mode : %s\n", buf);
}

int openNetwork(int port)
{
    int socket_tcp;
    /*struct sockaddr_in addr_tcp;
    ERR(socket_tcp = socket(PF_INET, SOCK_STREAM, 0), "socket");
    bzero(&addr_tcp, sizeof(struct sockaddr_in));
    addr_tcp.sin_family = AF_INET;
    addr_tcp.sin_port = htons(port);
    addr_tcp.sin_addr.s_addr = INADDR_ANY;
    ERR(bind(socket_tcp, (struct sockaddr *)&addr_tcp, sizeof(struct sockaddr_in)), "bind");
    ERR(listen(socket_tcp, BACKLOG), "listen");
    fprintf(stderr, "Listening TCP Port %i\n", port);*/
    return socket_tcp;
}

void setbaud(int fd)
{
    /*struct termios options;
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
    options.c_oflag &= ~OCRNL;
    options.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
    tcsetattr(fd, TCSANOW, &options);*/
}

int openXBee(char * device)
{
    int fd;
    /*ERR(fd = open(device, O_RDWR | O_NOCTTY), "open");
    setbaud(fd);
    fprintf(stderr, "Listening COMM Port %s\n", device);*/
    return fd;
}

int main2(int argc, char **argv)
{
    char comm_port[100], buf[1024], buf2[2048];
    int fd, sd, sd_client, size, i, tcp_port, fork_id;

    if(argc > 1)
        strcpy_s(comm_port,100, argv[1]);
    else
        strcpy_s(comm_port,100, COMM_PORT);

    if(strcmp(comm_port, "-h")==0)
    {
        fprintf(stderr, "Usage: %s [Serial Device] [TCP Port]\nDefaults:\n\tSerial\t: %s\n\tTCP\t: %i\n",
                argv[0], COMM_PORT, TCP_PORT);
        exit(1);
    }

    if(argc > 2)
        tcp_port = atoi(argv[2]);
    else
        tcp_port = TCP_PORT;

    fd=openXBee(comm_port);
    sd=openNetwork(tcp_port);

    /* Switch to API Mode on the XBee device */
    setAPI(fd);

    /* Fork */
    while(1)
    {
        ERR(sd_client = accept(sd, NULL, 0), "accept");
        fprintf(stderr, "Client Connected\n");
		/*if((fork_id=fork()))
            networkListen(sd_client, fd);
        else
            xbeeListen(sd_client, fd);*/
		/*CreateThread( 
            NULL,              // default security attributes
            0,                 // use default stack size  
            xbeeListen,        // thread function 
            pData,             // argument to thread function 
            0,                 // use default creation flags 
            &dwThreadId[i]);   // returns the thread identifier 
		*/
		networkListen(sd_client, fd);
		/* If Finishes, client disconnects */
        //kill(fork_id, SIGTERM);
		CloseHandle(fork_id);
        close(sd_client);
        fprintf(stderr, "Client Disconnected\n");*/
    }

    //close(sd);
    //close(fd);

    return 0;
}

