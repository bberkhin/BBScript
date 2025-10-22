#pragma once
#include "minterfaces.h"

class CanMotorDriver : public IMotorDriver
{
public:
    CanMotorDriver(int can_id = 0, int master_id = 0xFD );
   // uint8_t getId() const override { return CAN_ID; }
	virtual bool onCanMessage(uint32_t id, uint8_t len, uint8_t *DataFrame)  = 0;
protected:
	uint8_t sendMsgBuf(uint32_t id, uint8_t ext, uint8_t rtrBit, uint8_t len, const uint8_t *buf, bool wait_sent = true);
protected:
    ICanInterface *can_bus_;
    uint8_t CAN_ID;						//CAN ID   (Default 127(0x7f for broadcast, set to 1 for direct communication)	
    uint8_t Master_CAN_ID = 0xFD;
}; 
