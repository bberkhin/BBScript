
#pragma once
#include <stdbool.h>
#include "minterfaces.h"
#include "slcan.h"
#include <string>




class PCanImpl : public ICanInterface
{
public:
    PCanImpl();
    ~PCanImpl();
    void setPort( const char *port );
    bool open();
    bool isOpen() override;
    bool readMsgBufID(uint32_t *id, uint8_t *len, uint8_t *buf) override;
	uint8_t sendMsgBuf(uint32_t id, uint8_t ext, uint8_t rtrBit, uint8_t len, const uint8_t *buf, bool wait_sent = true) override;

public:
    static PCanImpl *getGlobalCan();
    std::string port_name_;
    slcan_port_t port;
    bool is_open_;
};

