#include "platform_util.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <thread>

std::thread::id main_thread_id;

void init_main_thread_id() {
    main_thread_id = std::this_thread::get_id();
}

bool is_main_thread() {
    return std::this_thread::get_id() == main_thread_id;
}

// Версия, принимающая va_list напрямую
std::string vprint_to_string(const char *fmt, va_list va, bool addcr)
{
    int size = vsnprintf(nullptr, 0, fmt, va);
    std::string result(size, '\0');
    vsnprintf(result.data(), result.size() + 1, fmt, va);
    if ( addcr )
        result += '\n';
    return result;
}

void messageToQuery(uint8_t type, std::string &s)
{
    std::lock_guard<std::mutex> lock(terminal_q.mtx);
    terminal_q.messages.push( std::make_pair(type, s));
}

// print_terminal вызывает vprint_to_string напрямую — без копирования аргументов
void print_terminal(uint8_t type, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    std::string s = vprint_to_string(fmt, va, false);
    va_end(va);
    if ( is_main_thread() )
        print_gui_terminal( type, s );
    else
        messageToQuery(type, s );
}


extern "C" void print_gram_error(const char *err_pfx, const char *fmt, va_list va)
{
    std::string s(err_pfx);
    s += vprint_to_string(fmt, va, false);
    if ( is_main_thread() )
        print_gui_terminal( MSG_TYPE_ERROR, s );
    else
        messageToQuery(MSG_TYPE_ERROR, s );
}



int fatal_error(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
    vfprintf(stderr, fmt, va);
	va_end(va);
	exit(1);
}


SerialPort *SerialPort::getGlobalPort()
{
    static SerialPort global_port;
    return &global_port;
 }


// Преобразование массива байт в HEX-строку
std::string bytesToHexString(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (uint8_t byte : data) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(byte);
    }
    return oss.str();
}

// Преобразование HEX-строки в массив байт 

void hexStringToBytes(uint8_t *out, int l, const char *buff) {
    int len = strlen(buff);
    if (len % 2 != 0) {
        fatal_error("HEX bust be devided by 2");
    }
    uint8_t *p = out;
    char byteString[3];
    byteString[2]=0;
    for (size_t i = 0; i < len; i += 2, p++) {
        memcpy(byteString, buff+i, 2);
        *p = static_cast<uint8_t>(std::stoi(byteString, nullptr, 16));
    }
}

void SerialPort::sendMsgBuf(uint32_t id, uint8_t rtrBit, uint8_t length, const uint8_t *data)
{
    static std::vector<uint8_t> in;
    static std::string str_data;
    in.clear();
    in.resize(sizeof(uint32_t) + 2*sizeof(uint8_t) + length);
    uint8_t *p = &in[0];
    memcpy(p, &id, sizeof(uint32_t) );
    p += sizeof(uint32_t);
    memcpy(p, &rtrBit, sizeof(uint8_t) );
    p += sizeof(uint8_t);
    memcpy(p, &length, sizeof(uint8_t) );
    p += sizeof(uint8_t);
    memcpy(p, data, length );
    str_data = bytesToHexString(in);

    SerialPort *com = getGlobalPort();
    com->write(str_data);
}

bool SerialPort::readMsgBufID(uint32_t *ID, uint8_t *len, uint8_t *outb)
 {
    SerialPort *com = getGlobalPort();
    static  char buffer[16];
    memset(buffer,0,sizeof(buffer)) ;
    size_t rs = com->read((uint8_t *)buffer, 16);
    if (rs < 6 )
        return false; 
    
     // Parse ID and LEN
    char *p = buffer;
    hexStringToBytes((uint8_t *)ID, sizeof(uint32_t), p );            
    p += sizeof(uint32_t);
    hexStringToBytes((uint8_t *)len, sizeof(uint8_t), p );        
    p += sizeof(uint8_t);
    if ( (*len) > 0 && (*len) <= 8 )
        hexStringToBytes(outb, *len, p );

    return true;
 }


 bool SerialPort::isAvalible()
 {
    SerialPort *com = getGlobalPort();
    return (com->available() > 0 );
 }

unsigned long millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

 void delay_ms( int time_ms ) {
    if (time_ms <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
 }