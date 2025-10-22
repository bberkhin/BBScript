#include <stdio.h>
#include <string>
#include <stdbool.h>
#include <stdlib.h>
#include <iostream>
#include <getopt.h>
#include "platform_util.h"
#include "mrobotimpl.h"
#include <cstdarg>

extern "C" {
	int parser_intit();
	int parser_parse(const char *input_buf);
	int parser_execute();
	void parser_clean();
}

static const char usage_str[] =
	"bbrobot [options] [inputfile]\n"
	"Options:\n"
	"  -c|--com com port (ex: COM2)       Set comport\n"
	"  -h|--help               This message\n"
	;


static const struct option lopts[] = {
	{ "com",		required_argument,	NULL,	'c' },
	{ NULL,			0,			NULL,	 0 },
};


const char *xmlBuffer =
"<robot name=\"MyRobot\">"
	"<joint name=\"joint1\" motor=\"uart_odrv\" />"
//	"<joint name=\"joint2\" motor=\"can_odrv\" />"
"</robot>";

MRobot g_robot_;

int main(int argc, char *argv[]) 
{
	//open COM2 
	//setup COM2 115200 8N1
	//how to open com port in c
	std::string port_name("\\\\.\\COM5");
	int optc;
	int oidx = 0;
	int baudrate = 115200;
   

	while(EOF != (optc = getopt_long(argc, argv, "c", lopts, &oidx))) {
		switch(optc) {
			case 'C':
			case 'c':
				port_name = optarg;
				break;
			case 'h':
				fatal_error("%s", usage_str);
				break;
		}
	}

	if ( port_name.empty() ) {
		fatal_error("Please specify COM port name with -c option, e.g. -c \\\\.\\COM3\n");
		return 1;
	}
//init com port
	SerialPort *com = SerialPort::getGlobalPort();
	try
	{
		com->setPort(port_name);
		com->setBaudrate(baudrate);
		com->open();
		if ( !com->isOpen() )  {
			fatal_error("Failed to open COM port\n");
			return 1;
		}
		else {
			print_terminal("COM port opened successfully\n");
		}
	}
	catch(const std::exception& e)
	{
		print_terminal( e.what() );
	}

	
	// Initialize parser
	if ( parser_intit() != 0 ) {
		com->close();
		fatal_error("Failed to initialize parser\n");
		return 1;
	}

	size_t bytes_written, bytes_read;
	//print_terminal("Enter any key to start...\n");
	//getchar();
	//print_terminal("\n");
 
	std::string input;
	
	if ( !g_robot_.Load(xmlBuffer) ) {
		fatal_error("Failed to load robot from URDF");
		return 1;
	}
	
	while( true )
	{
		try
		{		
			print_prompt();
			std::getline(std::cin, input);
			if ( input == "exit" || input == "quit" )
				break;
			if ( input.length() == 0 )
				continue;
			//input += "\n"; // add newline to simulate enter key
			int res = parser_parse( input.c_str() );	
			if ( res != 0 ) {
				print_terminal("Syntax error");
				continue;
			}
			int exec_res = parser_execute();
			if ( exec_res != 0 ) {
				print_terminal("Execution error");
				continue;
			}
		} catch(const std::exception& e)
		{
        	print_terminal("Catch exception: ");
			print_terminal( e.what() );
		}
	}	
 	com->close();
 	parser_clean();
	return 0;
}

