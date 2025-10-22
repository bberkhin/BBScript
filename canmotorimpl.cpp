#include "canmotorimpl.h"
#include "pcanimpl.h"

CanMotorDriver::CanMotorDriver(int can_id, int master_id ) : 
    CAN_ID(can_id), Master_CAN_ID(master_id) 
{
    can_bus_ = dynamic_cast<ICanInterface *>(PCanImpl::getGlobalCan());
}
uint8_t CanMotorDriver::sendMsgBuf(uint32_t id, uint8_t ext, uint8_t rtrBit, uint8_t len, const uint8_t *buf, bool wait_sent )
{  
    if ( can_bus_ )  
        return can_bus_->sendMsgBuf( id,  ext, rtrBit, len, buf, wait_sent );			
    else 
        return 0;
}
 