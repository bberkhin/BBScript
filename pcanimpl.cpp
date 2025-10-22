#include "pcanimpl.h"

PCanImpl *PCanImpl::getGlobalCan()
{
    static PCanImpl global_can;
    return &global_can;
}

/*    
   
    rc = slcan_open_channel(port);
    if (rc < 0) {
        fprintf(stderr, "+++ error: slcan_open_channel returnd %i (%i)\n", rc, errno);
        goto teardown;
    }
    while (running) {
        usleep(10);
    }
    fputc('\n', stdout);
    rc = slcan_close_channel(port);
    if (rc < 0) {
        fprintf(stderr, "+++ error: slcan_close_channel returnd %i (%i)\n", rc, errno);
        goto teardown;
    }
    rc = slcan_version_number(port, &hw, &sw);
    if (rc >= 0) {
        fprintf(stdout, "!!! hardware: %u.%u\n!!! firmware: %u.%u\n", (hw >> 4), (hw & 0x0F), (sw >> 4), (sw & 0x0F));
    }
    rc = slcan_serial_number(port, &sn);
    if (rc >= 0) {
        fprintf(stdout, "!!! serial number: %X\n", sn);
    }

}
    */

PCanImpl::PCanImpl() : is_open_(false)
{
    port = slcan_create(8U);
    if (!port) {
        fprintf(stderr, "+++ error: slcan_create returnd NULL (%i)\n", errno);
    }
}

PCanImpl::~PCanImpl()
{
    slcan_disconnect(port);
    slcan_destroy(port);
}

void PCanImpl::setPort(const char *port)
{
    port_name_ =  port;
}

bool PCanImpl::open()
 {
    int rc = slcan_connect(port, port_name_.c_str(), NULL);
    if (rc < 0) {
        fprintf(stderr, "+++ error: slcan_connect returnd %i (%i)\n", rc, errno);
        return false;
    }
    rc = slcan_set_ack(port, true);
    if (rc < 0) {
        fprintf(stderr, "+++ error: slcan_set_ack true returnd %i (%i)\n", rc, errno);
        return false;
    }
    rc = slcan_version_number(port, NULL, NULL);
    if (rc < 0) {
        rc = slcan_set_ack(port, false);
        if (rc < 0) {
            fprintf(stderr, "+++ error: slcan_set_ack false returnd %i (%i)\n", rc, errno);
            return false;
        }
        fprintf(stdout, "!!! Using CANable protocol (w/o ACK/NACK feedback)\n");
    }
    rc = slcan_setup_bitrate(port, CAN_1000K);
    if (rc < 0) {
        fprintf(stderr, "+++ error: slcan_setup_bitrate returnd %i (%i)\n", rc, errno);
        return false;
    }
    rc = slcan_open_channel(port);
    if (rc < 0) {
        fprintf(stderr, "+++ error: slcan_open_channel returnd %i (%i)\n", rc, errno);
        return false;
    }
    is_open_ = true;
    return true;
 }

 bool PCanImpl::isOpen()
{
    return is_open_;
}


bool PCanImpl::readMsgBufID(uint32_t *id, uint8_t *len, uint8_t *buf)
{
    static slcan_message_t msg;
    if ( slcan_read_message(port, &msg, 0) == 0 )
    {
        *id = msg.can_id;
        *len = msg.can_dlc;
        memcpy( buf, msg.data, msg.can_dlc );
        return true;
    }
    return false;
}

uint8_t PCanImpl::sendMsgBuf(uint32_t id, uint8_t ext, uint8_t rtrBit, uint8_t len, const uint8_t *buf, bool wait_sent)
{
    static slcan_message_t msg;
    msg.can_id = id;
    msg.can_dlc = len;  
    if ( ext )   {  msg.can_id |= CAN_XTD_FRAME;    }   
    if( rtrBit ) {  msg.can_id |= CAN_RTR_FRAME;    }

    if ( len > 0 )
        memcpy( msg.data, buf, len );
    slcan_write_message(port, &msg, 0);
    return 0;
}
