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

#ifndef __LIBXBEE_H__
#define __LIBXBEE_H__

#include <string.h>
#include <stdint.h>

/*********** API Mode ***********/

/*  Command Types  */
#define XBEE_CMD_AT_COMMAND 0x08
#define XBEE_CMD_AT_RESPONSE 0x88
#define XBEE_CMD_TRANSMIT_64 0x00
#define XBEE_CMD_TRANSMIT_16 0x01
#define XBEE_CMD_TRANSMIT_STATUS 0x89
#define XBEE_CMD_RECEIVE_64 0x80
#define XBEE_CMD_RECEIVE_16 0x81

/* Structure */

struct api_frame
{
    uint16_t size;
    uint8_t cmdid;
    uint8_t data[1024];
    size_t data_size;
    uint8_t checksum;
};

/* 0x00 */
struct api_transmit_64
{
    uint8_t frame_id;
    uint8_t dest_address[8];
    uint8_t options;
    uint8_t rf_data[100];
    size_t size;
};

/* 0x08 */
struct api_at_command
{
    uint8_t frame_id;
    uint8_t command[2];
    uint8_t parameter[1024];
    size_t size;
};

/* 0x88 */
struct api_at_response
{
    uint8_t frame_id;
    uint8_t command[2];
    uint8_t status;
    uint8_t value[1024];
    size_t size;
};

/* 0x89 */
struct api_transmit_status
{
    uint8_t frame_id;
    uint8_t status;
};

/* 0x80 */
struct api_receive_64
{
    uint8_t source_address[8];
    uint8_t RSSI;
    uint8_t options;
    uint8_t rf_data[100];
    size_t size;
};

/* 0x81 */
struct api_receive_16
{
    uint8_t source_address[2];
    uint8_t RSSI;
    uint8_t options;
    uint8_t rf_data[100];
    size_t size;
};

/* Functions */
uint8_t calc_checksum(uint8_t buffer[], size_t frame_size);

size_t make_frame_data_transmit_64(struct api_transmit_64 * src, struct api_frame * dest);
/*size_t make_frame_data_transmit_16(struct api_transmit_16 * src, struct api_frame * dest);
*/
size_t make_frame_data_at_command(struct api_at_command * src, struct api_frame * dest);

size_t make_frame(struct api_frame * src, uint8_t buffer[], size_t buf_size);

int cut_frame(uint8_t buffer[], size_t buf_size, struct api_frame * dest);

void cut_frame_data_receive_16(struct api_frame * src, struct api_receive_16 * dest);
void cut_frame_data_receive_64(struct api_frame * src, struct api_receive_64 * dest);
void cut_frame_data_transmit_status(struct api_frame * src, struct api_transmit_status * dest);
void cut_frame_data_at_response(struct api_frame * src, struct api_at_response * dest);

int check_frame(uint8_t * data, size_t data_size);
int has_frame(uint8_t * data, size_t size);

#endif /* __LIBXBEE_H__ */
