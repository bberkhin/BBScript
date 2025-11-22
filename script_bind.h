#ifndef __SCRIPT_BIND_H
#define __SCRIPT_BIND_H

#ifdef __cplusplus
extern "C" {
#endif

double mr_getvoltag(int id);
void mr_calibrate(int id);               // IMotorDriver::calibrate() override;
void mr_clearerrors(int id);       // IMotorDriver::clearErrors() override;
double mr_getpos(int id);          // IMotorDriver::getCurrentPosition() const override;
double mr_getvel(int id);          // IMotorDriver::getCurrentVelocity() const override;
void mr_closeloop(int id);         // IMotorDriver::setCloseLoop() override;
void mr_setidle(int id);           // IMotorDriver::setStateIdle() override;
void mr_setmodepos(int id);        // IMotorDriver::setControlModePosition() override;
void mr_setmodevel(int id);        // IMotorDriver::setControlModeVelocity() override;
void mr_setpos(int id, float pos);  // IMotorDriver::setTargetPosition(double position) override;
void mr_setvel(int id, float v);  // IMotorDriver::setTargetVelocity(double velocity) override;
void mr_stop();
void mr_movej( float *pos, float speed);
void mr_setzero(int id);
extern void mr_delay_ms(int );

#ifdef __cplusplus
};
#endif

#endif