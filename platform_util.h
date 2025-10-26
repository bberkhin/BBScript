#ifndef __PLATFORM_UTILS_H
#define __PLATFORM_UTILS_H
#include <stdbool.h>
#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <cstdarg>
#include <serial/serial.h>
#include <queue>
#include <mutex>



typedef std::pair<uint8_t,std::string> terminal_msg;
struct TerminalQuery
{    
    std::mutex mtx;
    std::queue<terminal_msg> messages;
};

extern TerminalQuery terminal_q; 

enum MSG_TYPE {
    MSG_TYPE_INFO = 0,
    MSG_TYPE_INFOOK = 1,
    MSG_TYPE_WARNING = 2, 
    MSG_TYPE_ERROR = 3, 
    MSG_TYPE_DEBUG = 4 
};

void init_main_thread_id();
bool is_main_thread();
void print_gui_terminal(uint8_t type, std::string &s);
void print_gui_terminal(uint8_t type, const char *s);
void print_terminal( uint8_t type, const char *fmt, ...);
int fatal_error(const char *fmt, ...);
void delay_ms( int time_ms );
unsigned long millis();


class SerialPort : public  serial::Serial
{
public:
    SerialPort(const std::string &port = "",
          uint32_t baudrate = 9600,
          serial::Timeout timeout = serial::Timeout(),
          serial::bytesize_t bytesize = serial::eightbits,
          serial::parity_t parity = serial::parity_none,
          serial::stopbits_t stopbits = serial::stopbits_one,
          serial::flowcontrol_t flowcontrol = serial::flowcontrol_none)
        : serial::Serial(port, baudrate, timeout, bytesize, parity, stopbits, flowcontrol)
    {
        
    }
    ~SerialPort() {
        if ( isOpen() )
            close();
    }
    static SerialPort* getGlobalPort();
//Can emulation
    void sendMsgBuf( uint32_t id, uint8_t rtrBit, uint8_t length, const uint8_t* data);
    bool readMsgBufID(uint32_t* ID, uint8_t* len, uint8_t* outb);
    bool isAvalible();   
};


#endif