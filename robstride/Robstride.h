#ifndef __RobStride_H__
#define __RobStride_H__

#ifdef __cplusplus
 extern "C" {
#endif

#ifdef __cplusplus
 }
#endif

#include "..\platform_util.h"
#include "..\canmotorimpl.h"

#define CAN_ID_STD                  0//(0x00000000U)  /*!< Standard Id */
#define CAN_ID_EXT                  1// (0x00000004U)  /*!< Extended Id */
#define CAN_RTR_DATA                0// (0x00000000U)  /*!< Data frame   */
#define CAN_RTR_REMOTE              1// (0x00000002U)  /*!< Remote frame */


 #define Set_mode 		 'j'				//Set working mode
 #define Set_parameter 'p'				//Set parameter
 //Control modes
 #define move_control_mode  0	//Torque mode
 #define Pos_control_mode   1	//Position (PP) mode
 #define Speed_control_mode 2 //Speed mode
 #define Elect_control_mode 3 //Current mode
 #define Set_Zero_mode      4 //Zero mode
 #define CSP_control_mode		5	//CSP position mode
 //Communication addresses
#define Communication_Type_Get_ID 0x00     							//Get device ID, 64-bit unique MCU identifier
#define Communication_Type_MotionControl 0x01 					//Torque mode command, supports motion instruction
#define Communication_Type_MotorRequest 0x02						//Query motor status
#define Communication_Type_MotorEnable 0x03							//Enable motor
#define Communication_Type_MotorStop 0x04								//Stop motor
#define Communication_Type_SetPosZero 0x06							//Set current position as zero
#define Communication_Type_Can_ID 0x07									//Get current CAN_ID
#define Communication_Type_Control_Mode 0x12						//Set control mode
#define Communication_Type_GetSingleParameter 0x11			//Get single parameter
#define Communication_Type_SetSingleParameter 0x12			//Set single parameter
#define Communication_Type_ErrorFeedback 0x15						//Error feedback frame
//When using MIT mode, check firmware version >= 0.13.0		
#define Communication_Type_MotorDataSave 0x16						//Motor data save frame
#define Communication_Type_BaudRateChange 0x17					//Baud rate modify frame, take effect after reboot
#define Communication_Type_ProactiveEscalationSet 0x18	//Proactive report settings
#define Communication_Type_MotorModeSet 0x19						//Protocol modify frame, take effect after reboot


class data_read_write_one
{
	public:
		uint16_t index;
		float data;
};
static const uint16_t Index_List[] = {0X7005, 0X7006, 0X700A, 0X700B, 0X7010, 0X7011, 0X7014, 0X7016, 0X7017, 0X7018, 0x7019, 0x701A, 0x701B, 0x701C, 0x701D};
//18 communication types - list of writable parameters
//Parameter description   Parameter address   Type   Byte length   Unit/Note
class data_read_write			//Writable parameters
{
	
	public:
		data_read_write_one run_mode;				//0: Torque mode, 1: Position mode, 2: Speed mode, 3: Current mode, 4: Zero mode uint8  1byte
		data_read_write_one iq_ref;					//Current mode Iq command  				float 	4byte 	-23~23A
		data_read_write_one spd_ref;				//Speed mode speed command 				float 	4byte 	-30~30rad/s 
		data_read_write_one imit_torque;		//Torque limit 								float 	4byte 	0~12Nm  
		data_read_write_one cur_kp;					//Current loop gain Kp 					float 	4byte 	Default 0.125  
		data_read_write_one cur_ki;					//Current loop gain Ki 					float 	4byte 	Default 0.0158  
		data_read_write_one cur_filt_gain;	//Current filter gain 	float 	4byte 	Default 0.1  
		data_read_write_one loc_ref;				//Position mode position command	float 	4byte 	rad  
		data_read_write_one limit_spd;			//Position mode speed limit				float 	4byte 	0~30rad/s  
		data_read_write_one limit_cur;			//Speed/position mode current limit 		float 	4byte 	0~23A
		//Read-only
		data_read_write_one mechPos;				//Actual mechanical position angle			float 	4byte 	rad
		data_read_write_one iqf;						//Filtered Iq value 							float 	4byte 	-23~23A
		data_read_write_one	mechVel;				//Actual rotation speed							float 	4byte 	-30~30rad/s 	
		data_read_write_one	VBUS;						//Bus voltage								float 	4byte 	V	
		data_read_write_one	rotation;				//Turn count 									int16 	2byte   turns
		data_read_write(const uint16_t *index_list=Index_List);
};
typedef struct
{
	float Angle;
	float Speed;
	float Torque;
	float Temp;
	int pattern; //Control mode: 0-position, 1-speed, 2-torque
}Motor_Pos_RobStride_Info;
typedef struct
{
	int set_motor_mode;
	float set_current;
	float set_speed;
	float set_acceleration;
	float set_Torque;
	float set_angle;
	float set_limit_cur;
	float set_limit_speed;
	float set_Kp;
	float set_Ki;
	float set_Kd;
}Motor_Set;

enum MIT_TYPE
{
    operationControl = 0,
    positionControl = 1,
    speedControl = 2
};
class RobStride_Motor : public CanMotorDriver
{
public:
    RobStride_Motor(uint8_t CAN_Id = 0, bool MIT_Mode = false);
	 ~RobStride_Motor() = default;

    void setPosition(float position, float velocity_feedforward) override;
    void setVelocity(float velocity) override;
    void stop()  override;


    float getPosition() const override;
    float getVelocity() const override;
    float getVoltage() const override;
	float getIqCurrent() const override;
	
    bool request(uint16_t what, uint16_t wait_ms = 0) override;

    //states
    void setCloseLoop()  override;
    void setStateIdle()  override;
    void setControlModeVelocity() override;
    void setControlModePosition()  override;
    //common
    void calibrate()  override;
    void clearErrors()  override;

	bool onCanMessage(uint32_t id, uint8_t len, uint8_t *DataFrame) override;
private:
	
	void RobStride_Get_CAN_ID();
	void RobStride_Get_Feedback();
	void Set_RobStride_Motor_parameter(uint16_t Index, float Value, char Value_mode);
	void Get_RobStride_Motor_parameter(uint16_t Index);
	bool RobStride_Motor_Analysis(uint8_t *DataFrame,uint32_t ID_ExtId);
	void RobStride_Motor_move_control(float Torque, float Angle, float Speed, float Kp, float Kd);
	void RobStride_Motor_Pos_control( float Speed, float Angle);
	void RobStride_Motor_CSP_control(float Angle, float limit_spd);
	void RobStride_Motor_Speed_control(float Speed, float limit_cur);
	void RobStride_Motor_current_control( float current);
	void RobStride_Motor_Set_Zero_control();
	void RobStride_Motor_MotorModeSet(uint8_t F_CMD);
	void Enable_Motor();
	void Disenable_Motor( uint8_t clear_error);
	void Set_CAN_ID(uint8_t Set_CAN_ID);
	void Set_ZeroPos();
	

	bool Get_MIT_Mode();
	MIT_TYPE get_MIT_Type();
	void RobStride_Motor_MIT_Control(float Angle, float Speed, float Kp, float Kd, float Torque);
	void RobStride_Motor_MIT_PositionControl(float position_rad, float speed_rad_per_s);
	void RobStride_Motor_MIT_SpeedControl(float speed_rad_per_s, float current_limit);
	void RobStride_Motor_MIT_Enable();
	void RobStride_Motor_MIT_Disable();
	void RobStride_Motor_MIT_SetZeroPos();
	void RobStride_Motor_MIT_ClearOrCheckError(uint8_t F_CMD);
	void RobStride_Motor_MIT_SetMotorType(uint8_t F_CMD);
	void RobStride_Motor_MIT_SetMotorId(uint8_t F_CMD);
	void RobStride_Motor_MotorDataSave();
	void RobStride_Motor_BaudRateChange(uint8_t F_CMD);
	void RobStride_Motor_ProactiveEscalationSet(uint8_t F_CMD);
	void RobStride_Motor_MIT_MotorModeSet(uint8_t F_CMD);

private:
	uint32_t getExtId( uint32_t type);	
	uint32_t getExtId( uint32_t type, uint16_t id2, uint8_t id1);
	void Set_MIT_Mode(bool MIT_Mode);
	void Set_MIT_Type(MIT_TYPE MIT_Type);	
private:
	Motor_Set Motor_Set_All;		//Setting value
	uint8_t error_code;
	float output;
	uint64_t Unique_ID = 0;					//64-bit unique MCU identifier
	Motor_Pos_RobStride_Info Pos_Info;		//Return value
	data_read_write drw;      						//Parameter structure
	
	bool MIT_Mode;			//MIT mode
	MIT_TYPE MIT_Type;		//MIT mode type
};

#endif
