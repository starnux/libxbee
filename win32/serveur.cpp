/*
 *
 *    win32 version of TCP2Serial Xbee Server
 *
 *    Neil Armstrong
 *    Projet Polyquest
 *
 *    http://dedale.eu.org/~polyquest
 *
 */

#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <stdlib.h>  /* exit */
#include <Winsock2.h>

#include "libxbee.h" /* libxbee */

/* Define DEBUG for Active Debugging for received packets */
/* #define _DEBUG_ 1 */

#define ERR(r, t) { if((r) == -1) { printf("%s: %d\n",t,WSAGetLastError()); system("pause"); exit(1); } }
#define COMMERR(r, t) { if((r) == -1) { printf("%s: %d\n",t,GetLastError()); system("pause"); exit(1); } }
#define QUIT(t) { printf("%s\n", t); system("pause"); exit(1);}
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

struct ThreadData
{
	SOCKET sock;
	HANDLE * fd;
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
#endif /* _DEBUG_ */

int writeComm(HANDLE * fd, uint8_t * buffer, size_t size)
{
	DWORD sizeWrite;
	if(WriteFile(*fd, buffer, (DWORD)size, &sizeWrite, NULL)==0)
		return -1;
	else
		return sizeWrite;
}

int readComm(HANDLE * fd, uint8_t * buffer, size_t size)
{
	DWORD sizeRead;
	if(ReadFile(*fd, buffer, (DWORD)size, &sizeRead, NULL)==0)
		return -1;
	else
		return sizeRead;
}

int availableComm(HANDLE * fd)
{
    struct _COMSTAT status;
    int             n;
    unsigned long   etat;

    n = 0;

    if (*fd!=INVALID_HANDLE_VALUE)
    {
        ClearCommError(*fd, &etat, &status);
        n = status.cbInQue;
    }
    return n;
}

void send_to_frame(HANDLE * fd, struct receive_tcp_frame * tcp_frame)
{
    struct api_frame frm;
    struct api_transmit_64 trans;
    char frame_buffer[BUF_SIZE];
    size_t frame_size;
	#ifdef _DEBUG_
    int i;
	#endif
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
    frame_size=make_frame(&frm, (uint8_t*)frame_buffer, BUF_SIZE);
    /* Send Frame */
    ERR(writeComm(fd, (uint8_t*)frame_buffer, frame_size), "write");
    #ifdef _DEBUG_
    fprintf(stderr, "DEBUG: Sent XBee frame [");
    for(i = 0 ; i < frame_size; ++i)
        fprintf(stderr, "%02X ", frame_buffer[i]);
    fprintf(stderr, "]\n");
    #endif
}

void send_scan(HANDLE * fd, struct receive_tcp_frame * tcp_frame)
{
    struct api_frame frm;
    struct api_at_command cmd;
    char frame_buffer[BUF_SIZE];
    size_t frame_size;
	#ifdef _DEBUG_
    int i;
	#endif
    /* Prepare Cmd */
    cmd.frame_id = tcp_frame->frame_id;
    cmd.command[0] = 'N';
    cmd.command[1] = 'D';
    cmd.size = 0;
    /* Prepare Frame */
    frm.cmdid = 0x08;
    /* Build Frame Data */
    make_frame_data_at_command(&cmd, &frm);
    frame_size=make_frame(&frm, (uint8_t*)frame_buffer, BUF_SIZE);
    /* Send Frame */
    ERR(writeComm(fd, (uint8_t*)frame_buffer, frame_size), "write");
    #ifdef _DEBUG_
    fprintf(stderr, "DEBUG: Sent XBee frame [");
    for(i = 0 ; i < frame_size; ++i)
        fprintf(stderr, "%02X ", frame_buffer[i]);
    fprintf(stderr, "]\n");
    #endif
}

void send_command(HANDLE * fd, struct receive_tcp_frame * tcp_frame)
{
    struct api_frame frm;
    struct api_at_command cmd;
    char frame_buffer[BUF_SIZE];
    size_t frame_size;
	#ifdef _DEBUG_
    int i;
	#endif
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
    frame_size=make_frame(&frm, (uint8_t*)frame_buffer, BUF_SIZE);
    /* Send Frame */
    ERR(writeComm(fd, (uint8_t*)frame_buffer, frame_size), "write");
    #ifdef _DEBUG_
    fprintf(stderr, "DEBUG: Sent XBee frame [");
    for(i = 0 ; i < frame_size; ++i)
        fprintf(stderr, "%02X ", frame_buffer[i]);
    fprintf(stderr, "]\n");
    #endif
}

void cut_tcp_frame(char * buffer, size_t size, struct receive_tcp_frame * tcp_frame)
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
            return (int)3;
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
            return (int)(11+tcp_frame->data_size);
        }break;
        case TCP_FRAME_COMMAND:
        {
            if(size < 3+tcp_frame->data_size)
                return -1;
            buffer[0] = TCP_FRAME_COMMAND;
            buffer[1] = tcp_frame->frame_id;
            buffer[2] = tcp_frame->status;
            memcpy(buffer+3, tcp_frame->data, tcp_frame->data_size);
            return (int)(3+tcp_frame->data_size);
        }break;
        case TCP_FRAME_SEND_RECEIVE:
        {
            if(size < 9+tcp_frame->data_size)
                return -1;
            buffer[0] = TCP_FRAME_SEND_RECEIVE;
            memcpy(buffer+1, tcp_frame->address, 8);
            memcpy(buffer+9, tcp_frame->data, tcp_frame->data_size);
            return (int)(9+tcp_frame->data_size);
        }break;
    }
    return -1;
}

void networkListen(SOCKET sd, HANDLE * fd)
{
    fd_set fdset;
    int ret = 1;
    int size;
    char buffer[BUF_SIZE];
    struct receive_tcp_frame tcp_frame;
	printf("networkListen Launched\n");
    while(ret > 0)
    {
        FD_ZERO(&fdset);
        FD_SET(sd, &fdset);
        ret = select(NULL, &fdset, NULL, NULL, NULL);
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

DWORD WINAPI xbeeListen(LPVOID lpParam )
{
	struct ThreadData * tData;
    char m_buffer[BUF_SIZE];
    size_t buf_size = 0;
    struct send_tcp_frame tcp_frm;
	SOCKET sd;
	HANDLE * fd;
	int size, sz;
	#ifdef _DEBUG_
	int i;
	#endif
	int pos;
    char buf[1024];
	tData = (struct ThreadData *)lpParam;
	sd = tData->sock;
	fd = tData->fd;
	printf("xBeeListen Launched\n");
    while(1)
    {
        ERR(size=readComm(fd, (uint8_t*)buf, 1024), "read");
		if(size <=0)
		{
			Sleep(10);
			continue;
		}
        #ifdef _DEBUG_
		fprintf(stderr, "DEBUG: Received Comm %i Bytes\n", size);
		#endif
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
        if((pos=has_frame((uint8_t*)m_buffer, buf_size)) >= 0)
        {
            struct api_frame frm;
            size_t pos2 = 0;
            sz= cut_frame((uint8_t*)(m_buffer+pos), buf_size-pos, &frm);
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
                size = make_tcp_frame((uint8_t*)buf, 1024, &tcp_frm);
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
    } 
}

void setAPI(HANDLE * fd)
{
    size_t size;
    char buf[1024];
    COMMERR(writeComm(fd, (uint8_t*)"+++", 3), "writeComm");
    COMMERR(size=readComm(fd, (uint8_t*)buf, 3), "readComm");
    if(strncmp(buf, "OK", 2)!=0){ QUIT("Module Not Responding\n");}
	buf[size] = 0;
    fprintf(stderr, "XBee Command Mode : %s\n", buf);
    /*ERR(write(fd, "ATMY FFFF\r", 10), "write");
    ERR(size=read(fd, buf, 1024), "read");
    if(strcmp(buf, "OK\r")!=0){ fprintf(stderr, "Module Not Responding\n"); exit(1);}
    fprintf(stderr, "XBee Disable 16Bit : %s\n", buf);*/
    COMMERR(writeComm(fd, (uint8_t*)"ATAP 1\r", 7), "writeComm");
    COMMERR(size=readComm(fd, (uint8_t*)buf, 3), "readComm");
    if(strncmp(buf, "OK", 2)!=0){ QUIT("Module Not Responding\n");}
	buf[size] = 0;
    fprintf(stderr, "XBee API Mode : %s\n", buf);
    COMMERR(writeComm(fd, (uint8_t*)"ATCN\r", 5), "writeComm");
    COMMERR(size=readComm(fd, (uint8_t*)buf, 3), "readComm");
    if(strncmp(buf, "OK", 2)!=0){ QUIT("Module Not Responding\n");}
	buf[size] = 0;
    fprintf(stderr, "XBee Exit Command Mode : %s\n", buf);
}

SOCKET openNetwork(int port)
{
    SOCKET socket_tcp;
    struct sockaddr_in addr_tcp;
	WSADATA wsaData;
	WSAStartup(MAKEWORD( 2, 2 ), &wsaData);
    ERR(socket_tcp = socket(PF_INET, SOCK_STREAM, 0), "socket");
    addr_tcp.sin_family = AF_INET;
    addr_tcp.sin_port = htons(port);
    addr_tcp.sin_addr.s_addr = INADDR_ANY;
	ERR(bind(socket_tcp, (struct sockaddr *)&addr_tcp, sizeof(struct sockaddr_in)), "bind");
	ERR(listen(socket_tcp, BACKLOG), "listen");
    fprintf(stderr, "Listening TCP Port %i\n", port);
    return socket_tcp;
}

void openXBee(char * device, HANDLE * fd)
{
    DCB  dcb;
	COMMTIMEOUTS cto = { 0, 0, 0, 0, 0 };
	memset(&dcb,NULL,sizeof(dcb));
	dcb.DCBlength       = sizeof(dcb); 
	dcb.BaudRate        = 9600;
	dcb.Parity			= NOPARITY;
    dcb.fParity			= 0;
	dcb.StopBits        = ONESTOPBIT;
    dcb.ByteSize        = 8;
	dcb.fOutxCtsFlow    = 0;
    dcb.fOutxDsrFlow    = 0;
    dcb.fDtrControl     = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = 0;
    dcb.fRtsControl     = RTS_CONTROL_DISABLE;
    dcb.fOutX           = 0;
    dcb.fInX            = 0;
	dcb.fErrorChar      = 0;
    dcb.fBinary         = 1;
    dcb.fNull           = 0;
    dcb.fAbortOnError   = 0;
    dcb.wReserved       = 0;
    dcb.XonLim          = 2;
    dcb.XoffLim         = 4;
    dcb.XonChar         = 0x13;
    dcb.XoffChar        = 0x19;
    dcb.EvtChar         = 0;
	*fd = CreateFile(device, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING,NULL,NULL);
	if (fd   == INVALID_HANDLE_VALUE)
	{
		printf("OPENCOMM Failed\n");
		COMMERR(-1, "CreateFile");
	}
	COMMERR((!SetCommMask(*fd, 0)?-1:0), "SetCommMask");
	COMMERR((!SetCommTimeouts(*fd,&cto)?-1:0), "SetCommTimeouts"); //Met en bloquant pour le passage en mode API
	COMMERR((!SetCommState(*fd,&dcb)?-1:0), "SetCommState");;
}

int main()
{
    char comm_port[100];
    int tcp_port, comm_num;
	COMMTIMEOUTS cto = { MAXDWORD, 0, 0, 0, 0 }; //NON BLOQUANT
	HANDLE fd, fork_id;
	SOCKET sd, sd_client;
	struct ThreadData tData;

	tcp_port = TCP_PORT;
	comm_num = 1;

	printf("Win32 TCP2Xbee");
	#ifdef _DEBUG_
	printf(" - Active DEBUG Activated");
	#endif
	printf("\n");

	printf("COM Port Number [1]:");
	if(scanf_s("%d", &comm_num)==0)
		comm_num = 1;

	printf("TCP Port [%i]:", tcp_port);
	if(scanf_s("%d", &tcp_port)==0)
		tcp_port = TCP_PORT;

    sprintf_s(comm_port, 100, "COM%d", comm_num);
	printf("Trying to Open %s\n", comm_port);
    openXBee(comm_port, &fd);
    sd=openNetwork(tcp_port);

    /* Switch to API Mode on the XBee device */
    setAPI(&fd);
	COMMERR((!SetCommTimeouts(fd,&cto)?-1:0), "SetCommTimeouts");

    /* Fork */
    while(1)
    {
        ERR(sd_client = accept(sd, NULL, 0), "accept");
        fprintf(stderr, "Client Connected\n");
		tData.sock = sd_client;
		tData.fd = &fd;
		fork_id = CreateThread( NULL, 0,xbeeListen, &tData, 0, NULL); //Crée un thread pour s'occuper des datas du Xbee
		networkListen(sd_client, &fd);
		CloseHandle(fork_id);
		printf("xBeeListen Finished\n");
        closesocket(sd_client);
        fprintf(stderr, "Client Disconnected\n");
    }

    //close(sd);
    closesocket(sd);
	CloseHandle(fd);
	system("pause");
    return 0;
}

