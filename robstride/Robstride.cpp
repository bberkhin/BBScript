#include "Robstride.h"
#include "string.h"

#define P_MIN -12.5f
#define P_MAX 12.5f
#define V_MIN -44.0f 
#define V_MAX 44.0f
#define KP_MIN 0.0f
#define KP_MAX 500.0f
#define KD_MIN 0.0f
#define KD_MAX 5.0f
#define T_MIN -17.0f
#define T_MAX 17.0f


/*******************************************************************************
* @brief      : RobStride Motor class constructor
* @param      : CAN ID
* @return     : void
* @note       : Initializes device ID
*******************************************************************************/
RobStride_Motor::RobStride_Motor(uint8_t CAN_Id, bool MIT_mode )
	: CanMotorDriver(CAN_Id, 0xFD)
{
	Motor_Set_All.set_motor_mode = move_control_mode;
	MIT_Mode = MIT_mode;
	MIT_Type = operationControl;
}


/*******************************************************************************
* @brief      : Convert uint16_t to float and with offset
* @param1     : Value to convert
* @param2     : x axis minimum
* @param3     : x axis maximum
* @param4     : Conversion bit width
* @return     : Converted float value
* @note       : None
*******************************************************************************/
float uint16_to_float(uint16_t x,float x_min,float x_max,int bits){
    //uint32_t span = (1 << bits) - 1;
	uint32_t span = 1;
	span <<= bits;
	span = span - 1;
	x &= span; 
    float offset = x_max - x_min;
    return offset * x / span + x_min;
}
/*******************************************************************************
* @brief      : Convert float to uint integer
* @param1     : Value to convert
* @param2     : x axis minimum
* @param3     : x axis maximum
* @param4     : Conversion bit width
* @return     : Converted int value
* @note       : None
*******************************************************************************/
int float_to_uint(float x,float x_min,float x_max,int bits)
{
	float span = x_max - x_min;
	float offset = x_min;
	if(x > x_max) x = x_max;
	else if(x < x_min) x = x_min;
	return (int) ((x - offset)*((float)((1<<bits)-1))/span);
}
/*******************************************************************************
* @brief      : Convert uint8_t array to float
* @param      : Array to convert
* @return     : Converted float value
* @note       : None
*******************************************************************************/
float Byte_to_float(uint8_t* bytedata)  
{  
	//uint32_t data  = bytedata[7]<<24|bytedata[6]<<16|bytedata[5]<<8|bytedata[4];
	uint32_t data  = bytedata[7];
	data <<= 8;
	data |= bytedata[6];
	data <<= 8;
	data |= bytedata[5];
	data <<= 8;
	data |= bytedata[4];
 	
	float data_float;// = *static_cast<float*>( &data );
	memcpy(&data_float, &data, 4);
    return data_float;  
}  
/*******************************************************************************
* @brief      : Convert MIT fault mask to private mode fault mask
* @param      : MIT fault mask
* @return     : uint8_t
* @note       : None
*******************************************************************************/
uint8_t mapFaults(uint16_t fault16) {
    uint8_t fault8 = 0;

    if (fault16 & (1 << 14)) fault8 |= (1 << 4); // Overvoltage fault
    if (fault16 & (1 << 7))  fault8 |= (1 << 5); // Not positioned
    if (fault16 & (1 << 3))  fault8 |= (1 << 3); // Encoder fault
    if (fault16 & (1 << 2))  fault8 |= (1 << 0); // Undervoltage fault
    if (fault16 & (1 << 1))  fault8 |= (1 << 1); // Overcurrent fault
    if (fault16 & (1 << 0))  fault8 |= (1 << 2); // Overtemperature fault

    return fault8;
}

/*******************************************************************************
* @brief      : Parse received data frame. For communication type 2 and 17, and type 0 response frame.
* @param1     : Received data frame
* @param2     : Received CANID
* @return     : None
* @note       : drw only supports getting values through communication type 17
*******************************************************************************/
bool RobStride_Motor::RobStride_Motor_Analysis(uint8_t *DataFrame,uint32_t ID_ExtId)
{
	bool result = false;
	if(MIT_Mode)
	{
		if((ID_ExtId & 0xFF) == 0XFD)
		{
			if(DataFrame[3] == 0x00 && DataFrame[4] == 0x00 && DataFrame[5] == 0x00 && DataFrame[6] == 0x00 && DataFrame[7] == 0x00)
			{
				uint16_t fault16 = 0;
				memcpy(&fault16, &DataFrame[1], 2);
				error_code = mapFaults(fault16);
			}
			else
			{
				Pos_Info.Angle =  uint16_to_float((DataFrame[1]<<8) | (DataFrame[2]),P_MIN,P_MAX,16);
				Pos_Info.Speed =  uint16_to_float((DataFrame[3]<<4) | (DataFrame[4]>>4),V_MIN,V_MAX,12);
				Pos_Info.Torque = uint16_to_float((DataFrame[4]<<8) | (DataFrame[5]),T_MIN,T_MAX,12);
				Pos_Info.Temp = ((DataFrame[6]<<8) | DataFrame[7])*0.1;
			}
		}
		else
		{
		    memcpy(&Unique_ID, DataFrame, 8);
		}
		result = true;
	}
	else 
	{     
		if (uint8_t((ID_ExtId&0xFF00)>>8) == CAN_ID)
		{		
			if (int((ID_ExtId&0x3F000000)>>24) == 2)
			{
				Pos_Info.Angle =  uint16_to_float(DataFrame[0]<<8|DataFrame[1],P_MIN,P_MAX,16);
				Pos_Info.Speed =  uint16_to_float(DataFrame[2]<<8|DataFrame[3],V_MIN,V_MAX,16);			
				Pos_Info.Torque = uint16_to_float(DataFrame[4]<<8|DataFrame[5],T_MIN,T_MAX,16);				
				Pos_Info.Temp = (DataFrame[6]<<8|DataFrame[7])*0.1;
				error_code = uint8_t((ID_ExtId&0x3F0000)>>16);
				Pos_Info.pattern = uint8_t((ID_ExtId&0xC00000)>>22);
				result = true;
				if (feedback_callback_	)
					feedback_callback_( this, FB_POSITION | FB_TEMPERATURE | FB_TORQUE );
			}
			else if (int((ID_ExtId&0x3F000000)>>24) == 17)
			{
				uint16_t cur_idx = (DataFrame[1]<<8|DataFrame[0]);
				for (int index_num = 0; index_num <= 13; index_num++)
				{					
					if (cur_idx == Index_List[index_num]){
						result = true;
						switch(index_num)
						{
							case 0:
								drw.run_mode.data = uint8_t(DataFrame[4]);
								break;
							case 1:
								drw.iq_ref.data = Byte_to_float(DataFrame);
								break;
							case 2:
								drw.spd_ref.data = Byte_to_float(DataFrame);
								break;
							case 3:
								drw.imit_torque.data = Byte_to_float(DataFrame);
								break;
							case 4:
								drw.cur_kp.data = Byte_to_float(DataFrame);
								break;
							case 5:
								drw.cur_ki.data = Byte_to_float(DataFrame);
								break;
							case 6:
								drw.cur_filt_gain.data = Byte_to_float(DataFrame);
								break;
							case 7:
								drw.loc_ref.data = Byte_to_float(DataFrame);
								break;
							case 8:
								drw.limit_spd.data = Byte_to_float(DataFrame);
								break;
							case 9:
								drw.limit_cur.data = Byte_to_float(DataFrame);
								break;	
							case 10:
								drw.mechPos.data = Byte_to_float(DataFrame);
								break;	
							case 11:
								drw.iqf.data = Byte_to_float(DataFrame);
								if (feedback_callback_	)
									feedback_callback_( this, FB_IQCURRENT );
								break;	
							case 12:
								drw.mechVel.data =Byte_to_float(DataFrame);
								break;	
							case 13:
								drw.VBUS.data = Byte_to_float(DataFrame);
								if (feedback_callback_	)
									feedback_callback_( this, FB_VBUS );
								break;	
						}
					}
				}
				if ( !result && cur_idx == 0X7024 )
				{
					limit_speed_ = Byte_to_float(DataFrame);
					result = true;
				}
				if ( !result && cur_idx == 0X7024 )
				{
					acceleration_ = Byte_to_float(DataFrame);
					result = true;
				}
				
			}
			else if ((uint8_t)((ID_ExtId & 0xFF)) == 0xFE)
			{
				CAN_ID = uint8_t((ID_ExtId & 0xFF00)>>8);	
				memcpy(&Unique_ID, DataFrame, 8);
				result = true;
			}
		}
	}
	return result;
}
/*******************************************************************************
* @brief      : RobStride get device CAN_ID, MCU communication type 0
* @param      : None
* @return     : void
* @note       : None
*******************************************************************************/
uint32_t RobStride_Motor::getExtId( uint32_t type)
{
	return getExtId(type, Master_CAN_ID, CAN_ID);
}

uint32_t RobStride_Motor::getExtId( uint32_t type, uint16_t id2, uint8_t id1)
{
	uint32_t id = type;
	id  <<= 16;
	id  |=  id2;
	id  <<= 8;
	id  |=  id1;
	return id;
}


void RobStride_Motor::RobStride_Get_CAN_ID()
{
		uint8_t txdata[8] = {0};						// Send data buffer
		sendMsgBuf( getExtId(Communication_Type_Get_ID),CAN_ID_EXT,CAN_RTR_DATA,8,txdata );

}

void RobStride_Motor::RobStride_Get_Feedback()
{
		uint8_t txdata[8] = {0};						// Send data buffer
		sendMsgBuf( getExtId(Communication_Type_MotorRequest),CAN_ID_EXT,CAN_RTR_DATA,8,txdata );

}



/*******************************************************************************
* @brief      : RobStride torque control mode, communication type 1
* @param1     : Torque (-4Nm~4Nm)
* @param2     : Target angle (-4~4 rad)
* @param3     : Target speed (-30rad/s~30rad/s)
* @param4     : Kp (0.0~500.0)
* @param5     : Kd (0.0~5.0)
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::RobStride_Motor_move_control(float Torque, float Angle, float Speed, float Kp, float Kd)
{
	uint8_t txdata[8] = {0};						// Send data buffer
	Motor_Set_All.set_Torque = Torque;
	Motor_Set_All.set_angle = Angle;	
	Motor_Set_All.set_speed = Speed;
	Motor_Set_All.set_Kp = Kp;
	Motor_Set_All.set_Kd = Kd;
	if (drw.run_mode.data != 0)
	{
		Set_RobStride_Motor_parameter(0X7005, move_control_mode, Set_mode);		// Set control mode
		Get_RobStride_Motor_parameter(0x7005);
		Enable_Motor();
		Motor_Set_All.set_motor_mode = move_control_mode;
	}
	if(Pos_Info.pattern != 2)
	{
		Enable_Motor();
	}

	txdata[0] = float_to_uint(Motor_Set_All.set_angle, P_MIN,P_MAX, 16)>>8; 
	txdata[1] = float_to_uint(Motor_Set_All.set_angle, P_MIN,P_MAX, 16); 
	txdata[2] = float_to_uint(Motor_Set_All.set_speed, V_MIN,V_MAX, 16)>>8; 
	txdata[3] = float_to_uint(Motor_Set_All.set_speed, V_MIN,V_MAX, 16); 
	txdata[4] = float_to_uint(Motor_Set_All.set_Kp,KP_MIN, KP_MAX, 16)>>8; 
	txdata[5] = float_to_uint(Motor_Set_All.set_Kp,KP_MIN, KP_MAX, 16); 
	txdata[6] = float_to_uint(Motor_Set_All.set_Kd,KD_MIN, KD_MAX, 16)>>8; 
	txdata[7] = float_to_uint(Motor_Set_All.set_Kd,KD_MIN, KD_MAX, 16); 
	uint32_t extId = getExtId( Communication_Type_MotionControl, float_to_uint(Motor_Set_All.set_Torque,T_MIN,T_MAX,16), CAN_ID);
	sendMsgBuf( extId,CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
}
//MIT mode enable
void RobStride_Motor::RobStride_Motor_MIT_Enable()
{
	uint8_t txdata[8] = {0}; 	// Data to send (all FF except last)
	txdata[0] = 0xFF;
	txdata[1] = 0xFF;
	txdata[2] = 0xFF;
	txdata[3] = 0xFF;
	txdata[4] = 0xFF;
	txdata[5] = 0xFF;
	txdata[6] = 0xFF;
	txdata[7] = 0xFC;
	sendMsgBuf( CAN_ID, CAN_ID_STD,CAN_RTR_DATA,8,txdata );
}

//MIT mode disable
void RobStride_Motor::RobStride_Motor_MIT_Disable()
{
	uint8_t txdata[8] = {0}; 	// Data to send (all FF except last)
	txdata[0] = 0xFF;
	txdata[1] = 0xFF;
	txdata[2] = 0xFF;
	txdata[3] = 0xFF;
	txdata[4] = 0xFF;
	txdata[5] = 0xFF;
	txdata[6] = 0xFF;
	txdata[7] = 0xFD;
	sendMsgBuf( CAN_ID, CAN_ID_STD,CAN_RTR_DATA,8,txdata );
}

//MIT mode clear or check error
void RobStride_Motor::RobStride_Motor_MIT_ClearOrCheckError(uint8_t F_CMD)
{
	uint8_t txdata[8] = {0}; 	// Data to send (all FF except 6th bit and FB)
	txdata[0] = 0xFF;
	txdata[1] = 0xFF;
	txdata[2] = 0xFF;
	txdata[3] = 0xFF;
	txdata[4] = 0xFF;
	txdata[5] = 0xFF;
	txdata[6] = F_CMD;
	txdata[7] = 0xFB;
	sendMsgBuf( CAN_ID, CAN_ID_STD,CAN_RTR_DATA,8,txdata );
}

//MIT set motor type
void RobStride_Motor::RobStride_Motor_MIT_SetMotorType(uint8_t F_CMD)
{
	uint8_t txdata[8] = {0}; 	// Data to send (all FF except 6th bit)
	txdata[0] = 0xFF;
	txdata[1] = 0xFF;
	txdata[2] = 0xFF;
	txdata[3] = 0xFF;
	txdata[4] = 0xFF;
	txdata[5] = 0xFF;
	txdata[6] = F_CMD;
	txdata[7] = 0xFC;
	sendMsgBuf( CAN_ID, CAN_ID_STD,CAN_RTR_DATA,8,txdata );

}

//MIT set motor ID
void RobStride_Motor::RobStride_Motor_MIT_SetMotorId(uint8_t F_CMD)
{
    uint8_t txdata[8] = {0}; 	// Data to send
	txdata[0] = 0xFF;
	txdata[1] = 0xFF;
	txdata[2] = 0xFF;
	txdata[3] = 0xFF;
	txdata[4] = 0xFF;
	txdata[5] = 0xFF;
	txdata[6] = F_CMD;
	txdata[7] = 0x01;
	sendMsgBuf( CAN_ID, CAN_ID_STD,CAN_RTR_DATA,8,txdata );
}



//MIT control mode
void RobStride_Motor::RobStride_Motor_MIT_Control(float Angle, float Speed, float Kp, float Kd, float Torque)
{
	uint8_t txdata[8] = {0}; 	// Data to send
	txdata[0] = float_to_uint(Angle, P_MIN,P_MAX, 16)>>8;
	txdata[1] = float_to_uint(Angle, P_MIN,P_MAX, 16);
	txdata[2] = float_to_uint(Speed, V_MIN,V_MAX, 12)>>4;
	txdata[3] = float_to_uint(Speed, V_MIN,V_MAX, 12)<<4 | float_to_uint(Kp, KP_MIN, KP_MAX, 12)>>8;
	txdata[4] = float_to_uint(Kp, KP_MIN, KP_MAX, 12);
	txdata[5] = float_to_uint(Kd, KD_MIN, KD_MAX, 12)>>4;
	txdata[6] = float_to_uint(Kd, KD_MIN, KD_MAX, 12)<<4 | float_to_uint(Torque, T_MIN, T_MAX, 12)>>8;
	txdata[7] = float_to_uint(Torque, T_MIN, T_MAX, 12);
	sendMsgBuf( CAN_ID, CAN_ID_STD,CAN_RTR_DATA,8,txdata );
}

//MIT position mode
void RobStride_Motor::RobStride_Motor_MIT_PositionControl(float position_rad, float speed_rad_per_s)
{
	uint8_t txdata[8] = {0}; 	// Data to send
	memcpy(&txdata[0], &position_rad, 4); 	// Copy position data to first 4 bytes of txdata
	memcpy(&txdata[4], &speed_rad_per_s, 4); 	// Copy speed data to last 4 bytes of txdata
	uint32_t id = (1 << 8) | CAN_ID; 	// Set standard ID
	sendMsgBuf( id, CAN_ID_STD,CAN_RTR_DATA,8,txdata );
}
// MIT mode speed control implementation
void RobStride_Motor::RobStride_Motor_MIT_SpeedControl(float speed_rad_per_s, float current_limit)
{
	uint8_t txdata[8] = {0}; 	// Data to send
	memcpy(&txdata[0], &speed_rad_per_s, 4);
	memcpy(&txdata[4], &current_limit, 4);
	uint32_t id = (2 << 8) | CAN_ID; 	// Set standard ID
	sendMsgBuf( id, CAN_ID_STD,CAN_RTR_DATA,8,txdata );

}

//MIT set zero position
void RobStride_Motor::RobStride_Motor_MIT_SetZeroPos()
{
	uint8_t txdata[8] = {0}; 	// Data to send (all FF except last byte)
	txdata[0] = 0xFF;
	txdata[1] = 0xFF;
	txdata[2] = 0xFF;
	txdata[3] = 0xFF;
	txdata[4] = 0xFF;
	txdata[5] = 0xFF;
	txdata[6] = 0xFF;
	txdata[7] = 0xFE;
	uint32_t id = CAN_ID; 	// Set standard ID
	sendMsgBuf( id, CAN_ID_STD,CAN_RTR_DATA,8,txdata );

}

/*******************************************************************************
* @brief      : RobStride position control mode (PP absolute position mode)
* @param1     : Target speed (-30rad/s~30rad/s)
* @param2     : Target angle (-4~4 rad)
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::RobStride_Motor_Pos_control(float Speed, float Angle)
{
		Motor_Set_All.set_speed = Speed;
		Motor_Set_All.set_angle = Angle;
		Motor_Set_All.set_limit_speed = limit_speed_;
		Motor_Set_All.set_acceleration = acceleration_;

	
//		if (drw.run_mode.data != 1 && Pos_Info.pattern == 2)
		if (drw.run_mode.data != 1)
		{
			Set_RobStride_Motor_parameter(0X7005, Pos_control_mode, Set_mode);		// Set control mode
			Get_RobStride_Motor_parameter(0x7005);
			Motor_Set_All.set_motor_mode = Pos_control_mode;
			Enable_Motor();
			Set_RobStride_Motor_parameter(0X7024, Motor_Set_All.set_limit_speed, Set_parameter);
			Set_RobStride_Motor_parameter(0X7025, Motor_Set_All.set_acceleration, Set_parameter);
		}	
		delay_ms(1);
		Set_RobStride_Motor_parameter(0X7016, Motor_Set_All.set_angle, Set_parameter);
}
/*******************************************************************************
* @brief      : RobStride position control mode (CSP position mode)
* @param1     : Target angle (-4~4 rad)
* @param2     : Target speed (0rad/s~44rad/s)
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::RobStride_Motor_CSP_control(float Angle, float limit_spd)
{
	if(MIT_Mode){
		RobStride_Motor_MIT_PositionControl(Angle, limit_spd);
	}
	else{
		Motor_Set_All.set_angle = Angle;
		Motor_Set_All.set_limit_speed = limit_spd;
		if (drw.run_mode.data != 1)
		{
			Set_RobStride_Motor_parameter(0X7005, CSP_control_mode, Set_mode);
			Get_RobStride_Motor_parameter(0x7005);
			Enable_Motor();
			Set_RobStride_Motor_parameter(0X7017, Motor_Set_All.set_limit_speed, Set_parameter);
		}
		delay_ms(1);
		Set_RobStride_Motor_parameter(0X7016, Motor_Set_All.set_angle, Set_parameter);
	}
}

/*******************************************************************************
* @brief      : RobStride speed control mode 
* @param1     : Target speed (-30rad/s~30rad/s)
* @param2     : Target current limit (0~23A)
* @return     : void
* @note       : None
*******************************************************************************/
uint8_t count_set_motor_mode_Speed = 0;
void RobStride_Motor::RobStride_Motor_Speed_control(float Speed, float limit_cur)
{
	Motor_Set_All.set_speed = Speed;
	Motor_Set_All.set_limit_cur = limit_cur;
	if (drw.run_mode.data != 2)
	{
		Set_RobStride_Motor_parameter(0X7005, Speed_control_mode, Set_mode);		// Set control mode
		Get_RobStride_Motor_parameter(0x7005);
		Enable_Motor();
		Motor_Set_All.set_motor_mode = Speed_control_mode;
		Set_RobStride_Motor_parameter(0X7018, Motor_Set_All.set_limit_cur, Set_parameter);
		Set_RobStride_Motor_parameter(0X7022, 10, Set_parameter);	
//		Set_RobStride_Motor_parameter(0X7022, Motor_Set_All.set_acceleration, Set_parameter);	
	}
	Set_RobStride_Motor_parameter(0X700A, Motor_Set_All.set_speed, Set_parameter);
}
/*******************************************************************************
* @brief      : RobStride current control mode
* @param      : Target current (-23~23A)
* @return     : void
* @note       : None
*******************************************************************************/
uint8_t count_set_motor_mode = 0;
void RobStride_Motor::RobStride_Motor_current_control(float current)
{
	Motor_Set_All.set_current = current;
	output = Motor_Set_All.set_current;
	if (Motor_Set_All.set_motor_mode != 3)
	{
		Set_RobStride_Motor_parameter(0X7005, Elect_control_mode, Set_mode);		// Set control mode
		Get_RobStride_Motor_parameter(0x7005);
		Motor_Set_All.set_motor_mode = Elect_control_mode;
		Enable_Motor();
	}
	Set_RobStride_Motor_parameter(0X7006, Motor_Set_All.set_current, Set_parameter);
}
/*******************************************************************************
* @brief      : RobStride zero mode (set current position as zero)
* @param      : None
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::RobStride_Motor_Set_Zero_control()
{
	Set_RobStride_Motor_parameter(0X7005, Set_Zero_mode, Set_mode);					// Set control mode
}
/*******************************************************************************
* @brief      : RobStride motor enable, communication type 3
* @param      : None
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::Enable_Motor()
{
	if (MIT_Mode)
	{
		RobStride_Motor_MIT_Enable();
	}
	else
	{
		uint8_t txdata[8] = {0};				// Send data buffer
		sendMsgBuf( getExtId(Communication_Type_MotorEnable), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );

	}
}
/*******************************************************************************
* @brief      : RobStride motor disable, communication type 4
* @param      : Clear error: 0 - no, 1 - yes
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::Disenable_Motor(uint8_t clear_error)
{
	if (MIT_Mode)
	{
		RobStride_Motor_MIT_Disable();
	}
	else
	{
		uint8_t txdata[8] = {0};						// Send data buffer
		txdata[0] = clear_error;
		sendMsgBuf( getExtId(Communication_Type_MotorStop), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
		Set_RobStride_Motor_parameter(0X7005, move_control_mode, Set_mode);
	}

}
/*******************************************************************************
* @brief      : RobStride write parameter, communication type 18
* @param1     : Parameter address
* @param2     : Parameter value
* @param3     : Choose whether writing as mode or parameter. Use Set_mode for working mode, Set_parameter for parameter setting.
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::Set_RobStride_Motor_parameter(uint16_t Index, float Value, char Value_mode)
{
	uint8_t txdata[8] = {0};						// Send data buffer
	txdata[0] = Index;
	txdata[1] = Index>>8;
	txdata[2] = 0x00;
	txdata[3] = 0x00;	
	if (Value_mode == 'p')
	{
		memcpy(&txdata[4],&Value,4);
	}
	else if (Value_mode == 'j')
	{
		Motor_Set_All.set_motor_mode = int(Value);
		txdata[4] = (uint8_t)Value;
		txdata[5] = 0x00;	
		txdata[6] = 0x00;	
		txdata[7] = 0x00;	
	}
	sendMsgBuf( getExtId(Communication_Type_SetSingleParameter), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
	
	
}
/*******************************************************************************
* @brief      : RobStride read parameter, communication type 17
* @param      : Parameter address
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::Get_RobStride_Motor_parameter(uint16_t Index)
{
	uint8_t txdata[8] = {0};						// Send data buffer
	txdata[0] = Index;
	txdata[1] = Index>>8;
	sendMsgBuf( getExtId(Communication_Type_GetSingleParameter), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
	
}
/*******************************************************************************
* @brief      : RobStride set CAN_ID, communication type 7
* @param      : Set and preset CANID
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::Set_CAN_ID(uint8_t Set_CAN_ID)
{
	Disenable_Motor(0);
	uint8_t txdata[8] = {0};						// Send data buffer
	uint16_t new_id = Set_CAN_ID;
	new_id <<= 8;
	new_id |= Master_CAN_ID;

	uint32_t ExtId = getExtId( Communication_Type_Can_ID, new_id, CAN_ID) ;
	sendMsgBuf( ExtId, CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
	
}
/*******************************************************************************
* @brief      : RobStride set zero position, communication type 6
* @param      : None
* @return     : void
* @note       : Sets current mechanical position as encoder zero; disables, then enables motor
*******************************************************************************/

//41543007e80c0801000000000000000d0a

//remove:4154 ... 0d0a
//3007e80c 08 01 00 00 00 00 00 00 00

//3007e80 c data size  08 data: 01 00 00 00 00 00 00 00

void RobStride_Motor::Set_ZeroPos()
{
	Disenable_Motor(0);							// Disable motor
	uint8_t txdata[8] = {0};						// Send data buffer
	txdata[0] = 1;
	sendMsgBuf( getExtId(Communication_Type_SetPosZero), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
	
	Enable_Motor();
}

/*******************************************************************************
* @brief      : RobStride motor data save, communication type 22
* @param      : None
* @return     : void
* @note       : Write current operating parameters to motor memory as defaults; user specified parameters are used for command
*******************************************************************************/
void RobStride_Motor::RobStride_Motor_MotorDataSave()
{
	uint8_t txdata[8] = {0};						// Send data buffer
	txdata[0] = 0x01;
	txdata[1] = 0x02;
	txdata[2] = 0x03;
	txdata[3] = 0x04;
	txdata[4] = 0x05;
	txdata[5] = 0x06;
	txdata[6] = 0x07;
	txdata[7] = 0x08;
    sendMsgBuf( getExtId(Communication_Type_MotorDataSave), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
	
	
}

/*******************************************************************************
* @brief      : RobStride baud rate modification, communication type 23
* @param      : Baud rate mode:    01-1M
*                                       02-500K
*                                       03-250K
*                                       04-125K
* @return     : void
* @note       : Set as corresponding value; eg. set as 01 to change to 1M baud rate
*******************************************************************************/
void RobStride_Motor::RobStride_Motor_BaudRateChange(uint8_t F_CMD)
{
	uint8_t txdata[8] = {0};						// Send data buffer
	txdata[0] = 0x01;
	txdata[1] = 0x02;
	txdata[2] = 0x03;
	txdata[3] = 0x04;
	txdata[4] = 0x05;
	txdata[5] = 0x06;
	txdata[6] = F_CMD;
	txdata[7] = 0x08;	// The meaning of the last byte is unknown, use 0x08
    sendMsgBuf( getExtId(Communication_Type_BaudRateChange), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
	
}

/*******************************************************************************
* @brief      : RobStride proactive reporting set, communication type 24
* @param      : Reporting mode:    00-disable
*                                   01-enable
* @return     : void
* @note       : Enable/disable proactive reporting, default reporting period is 10ms
*******************************************************************************/
void RobStride_Motor::RobStride_Motor_ProactiveEscalationSet(uint8_t F_CMD)
{
	uint8_t txdata[8] = {0};						// Send data buffer
	txdata[0] = 0x01;
	txdata[1] = 0x02;
	txdata[2] = 0x03;
	txdata[3] = 0x04;
	txdata[4] = 0x05;
	txdata[5] = 0x06;
	txdata[6] = F_CMD;
	txdata[7] = 0x08;	// The meaning of the last byte is unknown, use 0x08
    sendMsgBuf( getExtId(Communication_Type_ProactiveEscalationSet), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );

}

/*******************************************************************************
* @brief      : RobStride protocol modification, communication type 25
* @param      : Protocol type:        00-private protocol
*                                   01-Canopen
*                                   02-MIT protocol
* @return     : void
* @note       : None
*******************************************************************************/
void RobStride_Motor::RobStride_Motor_MIT_MotorModeSet(uint8_t F_CMD)
{
	uint8_t txdata[8] = {0};						// Send data buffer
	txdata[0] = 0xFF;
	txdata[1] = 0xFF;
	txdata[2] = 0xFF;
	txdata[3] = 0xFF;
	txdata[4] = 0xFF;
	txdata[5] = 0xFF;
	txdata[6] = F_CMD;
	txdata[7] = 0xFD;	// The meaning of the last byte is unknown, use 0xFD
    sendMsgBuf( CAN_ID, CAN_ID_STD,CAN_RTR_DATA,8,txdata );
}


/*******************************************************************************
* @brief      : Initialize addresses of data_read_write parameters
* @param      : Parameter address array
* @return     : void
* @note       : Automatically assigns addresses during construction
*******************************************************************************/
data_read_write::data_read_write(const uint16_t *index_list)
{
	run_mode.index = index_list[0];
	iq_ref.index = index_list[1];
	spd_ref.index = index_list[2];
	imit_torque.index = index_list[3];
	cur_kp.index = index_list[4];
	cur_ki.index = index_list[5];
	cur_filt_gain.index = index_list[6];
	loc_ref.index = index_list[7];
	limit_spd.index = index_list[8];
	limit_cur.index = index_list[9];
	mechPos.index = index_list[10];
	iqf.index = index_list[11];
	mechVel.index = index_list[12];
	VBUS.index = index_list[13];	
	rotation.index = index_list[14];
}

void RobStride_Motor::RobStride_Motor_MotorModeSet(uint8_t F_CMD)
{
	uint8_t txdata[8] = {0};						// Send data buffer
	txdata[0] = 0x01;
	txdata[1] = 0x02;
	txdata[2] = 0x03;
	txdata[3] = 0x04;
	txdata[4] = 0x05;
	txdata[5] = 0x06;
	txdata[6] = F_CMD;
	txdata[7] = 0x08;	// The last byte is the command, here use 0x08	
  	sendMsgBuf( getExtId(Communication_Type_MotorModeSet), CAN_ID_EXT,CAN_RTR_DATA,8,txdata );
}


bool RobStride_Motor::onCanMessage(uint32_t id, uint8_t , uint8_t *DataFrame)
{
	if (uint8_t((id&0xFF00)>>8) == CAN_ID)
	{
		return RobStride_Motor_Analysis(DataFrame, id);
	}
	return false;
}


  
void RobStride_Motor::setPosition(float position, float velocity_feedforward)
{
	if(MIT_Mode)
	{
		RobStride_Motor_MIT_PositionControl(position, velocity_feedforward);
	}
	else
	{
		RobStride_Motor_Pos_control(velocity_feedforward, position);
	}
}

bool RobStride_Motor::request_by_index(uint16_t index, uint16_t wait_ms )
{
	Get_RobStride_Motor_parameter( index );
	return wait_answer(1, wait_ms);
}

bool RobStride_Motor::request(uint16_t what, uint16_t wait_ms )
{
	int nufeedbacks = 0;
	if ( (what & FB_POSITION) || (what & FB_TEMPERATURE) || (what & FB_TORQUE) )	
	{
		RobStride_Get_Feedback();
		nufeedbacks++;
	}
	if ( what & FB_VBUS)
	{
		Get_RobStride_Motor_parameter( drw.VBUS.index );
		nufeedbacks++;
	}
	if ( what & FB_IQCURRENT)	
	{
		Get_RobStride_Motor_parameter( drw.iqf.index );
		nufeedbacks++;
	}
	return wait_answer(nufeedbacks, wait_ms);
}

bool RobStride_Motor::wait_answer(int nufeedbacks, int16_t wait_ms )
{
	if ( nufeedbacks && wait_ms && can_bus_)
	{
		uint32_t id;
		uint8_t len;
		uint8_t buffer[8];
		unsigned long startTime = millis();
		while ( nufeedbacks && ((millis() - startTime) < wait_ms) )
		{		
			if ( can_bus_->readMsgBufID(&id, &len, buffer) )
			{				
				if( onCanMessage(id, len, buffer) )
					nufeedbacks--;
			}
		}
	}
  	return true;
}

void RobStride_Motor::setVelocity(float velocity)
{
	this->RobStride_Motor_Speed_control(velocity, 20.0);
}

void RobStride_Motor::stop()
{
	Disenable_Motor(0);
}

void RobStride_Motor::setPosToZero() 
{
	Set_ZeroPos();
}


float RobStride_Motor::getPosition() const 
{
	return Pos_Info.Angle;
}

float RobStride_Motor::getVelocity() const 
{
	return Pos_Info.Speed;	
}


float RobStride_Motor::getParameter( JOINT_MOTOR_PARAM type )
{
	switch(type)
	{
		case JOINT_MOTOR_SPEEDLIMIT:			
			request_by_index( 0X7024,  100 );
			return limit_speed_;
		case JOINT_MOTOR_ACCLIMIT:
			request_by_index( 0X7025,  100 );
			return acceleration_;
		case JOINT_MOTOR_CURLIMIT:
			request_by_index( 0X7018,  100 );
			return drw.limit_cur.data;
		case JOINT_MOTOR_VOLTAGE:
		    request( FB_VBUS,  100 );
			return drw.VBUS.data;
		case JOINT_MOTOR_CURRENT:			
			request( FB_IQCURRENT,  100 );
			return drw.iqf.data;			
	}
	return 0.f;
}

// TODO Speed limit 0X7024 or drw.limit_spd.index==0X7017
void  RobStride_Motor::setParameter( JOINT_MOTOR_PARAM type, float param )
{
	switch(type)
	{
		case JOINT_MOTOR_SPEEDLIMIT:		
			limit_speed_ = param;
			Set_RobStride_Motor_parameter(0X7024, limit_speed_, Set_parameter);
			break;	
		case JOINT_MOTOR_ACCLIMIT:
			acceleration_ = param;
			Set_RobStride_Motor_parameter(0X7025, acceleration_, Set_parameter);
			break;	
		case JOINT_MOTOR_CURLIMIT:
			Set_RobStride_Motor_parameter(0X7018, param, Set_parameter);			
			break;				
	}
}

void RobStride_Motor::setCloseLoop()
{
	if(MIT_Mode)
	{
		RobStride_Motor_MIT_Enable();
	}
	else
	{
		Enable_Motor();
	}
}

void RobStride_Motor::setStateIdle() 
{
	if(MIT_Mode)
	{
		RobStride_Motor_MIT_Disable();
	}
	else
	{
		Disenable_Motor(0);
	}
}

void RobStride_Motor::setControlModeVelocity()
{
	if(MIT_Mode)
	{
		RobStride_Motor_MIT_Control(0, 0, 0, 0, 0);
	}
	else
	{
		Set_RobStride_Motor_parameter(0X7005, Speed_control_mode, Set_mode);		// Set control mode
		Get_RobStride_Motor_parameter(0x7005);
		Enable_Motor();
		Motor_Set_All.set_motor_mode = Speed_control_mode;
	}
}

void RobStride_Motor::setControlModePosition()
{
	if(MIT_Mode)
	{
		RobStride_Motor_MIT_Control(0, 0, 0, 0, 0);
	}
	else
	{
		Set_RobStride_Motor_parameter(0X7005, Pos_control_mode, Set_mode);		// Set control mode
		Get_RobStride_Motor_parameter(0x7005);
		Enable_Motor();
		Motor_Set_All.set_motor_mode = Pos_control_mode;
	}
}
//common
void RobStride_Motor::calibrate()
{
	RobStride_Motor_Set_Zero_control();
}
void RobStride_Motor::clearErrors()
{
	if(MIT_Mode)	
	{
		RobStride_Motor_MIT_ClearOrCheckError(1);	
	}
	else
	{
		Disenable_Motor(1);
		Enable_Motor();
	}
}
