/*
 *
 *    libXbee API Protocol Implementation
 *
 *    Neil Armstrong
 *    Projet Polyquest
 *
 *    http://dedale.eu.org/~polyquest
 *
 */
#include "libxbee.h"
#ifdef WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h> /* htons, ntohs */
#endif

/* Network to host : order big endian */
/*uint16_t ntohs(uint16_t value)
{
    uint16_t conv;
    uint8_t * tab = NULL;
    tab = (uint8_t *)&value;
    conv = tab[1] + tab[0]*0x100;
    return conv;
}*/

/* Host to Network */
/*uint16_t htons(uint16_t value)
{
    uint16_t conv;
    uint8_t * tab = NULL;
    tab = (uint8_t *)&conv;
    tab[1] = value&0xFF;
    tab[0] = (value << sizeof(uint8_t))&0xFF;
    return conv;
}*/

uint8_t calc_checksum(uint8_t buffer[], size_t frame_size) /* frame_size without the checksum */
{
    uint16_t result = 0;
    int i;
    for(i = 3 ; i < frame_size ; ++i)
        result += buffer[i];
    return 0xFF - result&0xFF;
}

size_t make_frame_data_transmit_64(struct api_transmit_64 * src, struct api_frame * dest)
{
    if(src->size+10 > 1024)
        return 0;
    dest->data[0] = src->frame_id;
    memcpy(dest->data+1, src->dest_address, 8);
    dest->data[9] = src->options;
    memcpy(dest->data+10, src->rf_data, src->size);
    dest->data_size = src->size+10;
    return dest->data_size;
}

size_t make_frame_data_at_command(struct api_at_command * src, struct api_frame * dest)
{
    if(src->size+3 > 1024)
        return 0;
    dest->data[0] = src->frame_id;
    memcpy(dest->data+1, src->command, 2);
    memcpy(dest->data+3, src->parameter, src->size);
    dest->data_size = src->size+3;
    return dest->data_size;
}

size_t make_frame(struct api_frame * src, uint8_t buffer[], size_t buf_size) /* htons */
{
    uint16_t size;
    uint8_t * size_tab = NULL;
    size_tab = (uint8_t *)&size;
    if(buf_size < src->data_size+5)
        return 0;
    buffer[0] = 0x7E;
    size = src->data_size+1;
    size = htons(size);
    buffer[1] = size_tab[0];
    buffer[2] = size_tab[1];
    buffer[3] = src->cmdid;
    memcpy(buffer+4, src->data, src->data_size);
    buffer[4+src->data_size] = calc_checksum(buffer, src->data_size+4);
    return 5+src->data_size;
}

int cut_frame(uint8_t buffer[], size_t buf_size, struct api_frame * dest) /* ntohs */
{
    /* Checker la taille !!! */
    uint16_t size;
    uint8_t * size_tab = NULL;
    size_tab = (uint8_t *)&size;
    if(buffer[0] != 0x7E)
        return -1;
    size_tab[0] = buffer[1];
    size_tab[1] = buffer[2];
    dest->size = ntohs(size);
    if(dest->size > 0)
    {
        dest->cmdid = buffer[3];
        dest->data_size = dest->size-1;
        memcpy(dest->data, buffer+4, dest->data_size);
    }
    dest->checksum = buffer[dest->size+3];
    return dest->size+4;
}

void cut_frame_data_receive_64(struct api_frame * src, struct api_receive_64 * dest)
{
    memcpy(dest->source_address, src->data, 8);
    dest->RSSI = src->data[8];
    dest->options = src->data[9];
    memcpy(dest->rf_data, src->data+10, src->data_size-10);
    dest->size = src->data_size-10;
}

void cut_frame_data_receive_16(struct api_frame * src, struct api_receive_16 * dest)
{
    dest->source_address[0] = src->data[0];
    dest->source_address[1] = src->data[1];
    dest->RSSI = src->data[2];
    dest->options = src->data[3];
    memcpy(dest->rf_data, src->data+4, src->data_size-4);
    dest->size = src->data_size-4;
}

void cut_frame_data_transmit_status(struct api_frame * src, struct api_transmit_status * dest)
{
    dest->frame_id = src->data[0];
    dest->status = src->data[1];
}

void cut_frame_data_at_response(struct api_frame * src, struct api_at_response * dest)
{
    dest->frame_id = src->data[0];
    dest->command[0] = src->data[1];
    dest->command[1] = src->data[2];
    dest->status = src->data[3];
    memcpy(dest->value, src->data+4, src->data_size-4);
    dest->size = src->data_size-4;
}

int check_frame(uint8_t * data, size_t data_size)
{
    uint16_t size;
    uint8_t * size_tab = NULL;
    uint8_t checksum = 0;
    size_tab = (uint8_t *)&size;
    if(data_size < 4)
        return -1;
    if(data[0] != 0x7E)
        return -1;
    size_tab[0] = data[1];
    size_tab[1] = data[2];
    size = ntohs(size);
    if(data_size < size + 4)
        return -1;
    checksum = data[size + 3];
    if(checksum != calc_checksum(data, size + 3))
        return -1;
    return 0;
}

int has_frame(uint8_t * data, size_t size)
{
    size_t pos = 0;
    while(pos < size)
    {
        if(data[pos] == 0x7E)
            if(check_frame(data+pos, size-pos) == 0)
                return pos;
        ++pos;
    }
    return -1;
}

