#include "fltkGUI.h"
#include "mrobotimpl.h"
#include "platform_util.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
#include "pcanimpl.h"


MRobot g_robot_;


std::atomic<bool> keep_running = true;
std::mutex mtx;
FeedbackGUIData  com_map;




const char *xmlBuffer =
"<robot name=\"MyRobot\">"
	"<joint name=\"joint1\" motor=\"uart_odrv\" />"
//	"<joint name=\"joint2\" motor=\"can_odrv\" />"
"</robot>";


extern "C" {
	int parser_intit();
	int parser_parse(const char *input_buf);
	int parser_execute();
	void parser_clean();
}


void run_programm(const std::string &input )
{
    try
    {		
        if ( input.empty() )
            return;
        int res = parser_parse( input.c_str() );	
        if ( res != 0 ) {
            print_terminal("Syntax error");
            return;
        }
        int exec_res = parser_execute();
        if ( exec_res != 0 ) {
            print_terminal("Execution error");            
        }
    } catch(const std::exception& e)
    {
        print_terminal("Catch exception: ");
        print_terminal( e.what() );
    }
}

void programm_thread_runner(SyncExchange& exchange)
{
    while (true)       
    {
        std::unique_lock<std::mutex> lock(exchange.mtx);
        exchange.cv_gui.wait(lock, [&]{ return (exchange.do_run || exchange.stop); });
        if ( exchange.stop )
            break;
       // std::cout << "Consumer: принял строку: " << exchange.message << '\n';       
        run_programm( exchange.str_prg );
        exchange.done = true;
        exchange.do_run = false; // clear flag
        //exchange.cv_gcmc.notify_one();
    }
    return;
}


void feedback_reader_thread() 
{
    int index = 1;
    IJointController* jctrl = g_robot_.getJointController();    
    PCanImpl *can = PCanImpl::getGlobalCan();
    double pos = 0, vel = 0;
    while (keep_running) {       
        if ( jctrl )
        {
            if ( can->isOpen() )
                 g_robot_.pumpEvents(can);
            
            jctrl->requestAll(FB_POSITION, 0);  
        }
        // Чтобы уменьшить нагрузку
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void timer_callback(void*) {
    std::lock_guard<std::mutex> lock(mtx);
    if (g_mainWnd) {
         g_mainWnd->setFeedback(com_map);
    }   
    Fl::repeat_timeout(0.05, timer_callback); // каждые 50 мс
}

void get_feedback_callback(IMotorDriver *motor,  uint16_t type)
{ 
    IJointController* jctrl = g_robot_.getJointController();   
    JointPtr jnt = jctrl->getJointByMotor(motor);
    if ( jnt && (type&FB_POSITION) )
    {
        FeedbackJoint data;
        jnt->getFeedback(&data);      
        std::lock_guard<std::mutex> lock(mtx);
        com_map[ jnt->getName() ] = data;

    }
}


#define DEFAULT_COM_PORT "COM5"
#define DEFAULT_COM_BAUNDRATE 115200
bool init_config()
{
    try
	{        
        rapidxml::file<> xmlFile("C:/Projects/BBRobot/BBScript/config.xml");    
        // Создаем и парсим документ
        rapidxml::xml_document<> doc;
        doc.parse<0>(xmlFile.data());
        rapidxml::xml_node<>* root = doc.first_node("app");
        if (!root) {
            fatal_error( "Root <app> not found in config.xml");
            return false;
        }
           
            //open can  <can port="COM5"/>
        rapidxml::xml_node<>* nd_can = root->first_node("can");
        if ( nd_can )
        {
            PCanImpl *can = PCanImpl::getGlobalCan();
            rapidxml::xml_attribute<> *attr = nd_can->first_attribute("port");
            const char* s = nullptr;
            if ( attr )
                s = attr->value();  
            can->setPort( s ? s : DEFAULT_COM_PORT );    		
            can->open();
	    	if ( can->isOpen() )  {
			    print_terminal("CAN opened successfully");
		    }
            else
               print_terminal("CAN not opened: %s", s ? s : DEFAULT_COM_PORT );

        }
        else
          print_terminal("tag <CAN> not found in config.xml so can not opend");

        //open COM  <com port="COM5",bound="115200"/>
        rapidxml::xml_node<>* nd_com = root->first_node("com");
        if ( nd_com )
        {
            //init port
            SerialPort *com = SerialPort::getGlobalPort();
            rapidxml::xml_attribute<> *attr = nd_com->first_attribute("port");
            const char* s = nullptr;
            if ( attr )
                s = attr->value();  
            else
                print_terminal("<port> not found in <com> in  config.xml");
            com->setPort( s ? s : DEFAULT_COM_PORT );
            attr = nd_com->first_attribute("bound");
            if ( attr ) {
                s = attr->value();  
            } else {
                s = nullptr;
                print_terminal("<bound> not found in <com> in  config.xml");
            }
            com->setBaudrate( s ? atoi(s) : DEFAULT_COM_BAUNDRATE );
    		com->open();
	    	if ( com->isOpen() )  {
			    print_terminal("COM port opened successfully\n");
		    }
        }
        else
          print_terminal("tag <COM> not found in config.xml so com port not opend");
        // init robot like URDF format 
        rapidxml::xml_node<>* nd_robot = root->first_node("robot");
        if ( !nd_robot )
        {
            fatal_error( "tag <robot> not found in config.xml");
            return false;
        }
        if ( !g_robot_.Load( nd_robot ) ) {
            fatal_error("Failed to load robot from URDF");
            return false;
        }
        IMotorDriver::setFeedbackCallback( get_feedback_callback );
    }
    catch (const rapidxml::parse_error &e) 
    {
        print_terminal( e.what() );
    }
    catch(const std::exception& e)
	{
		print_terminal( e.what() );
	}
    return true;
}


int main(int argc, char **argv) {


    init_config();  	
	// Initialize parser
	if ( parser_intit() != 0 ) {
    	fatal_error("Failed to initialize parser\n");
		return 1;
	}

    SyncExchange gui_run_data;
    FileManager win(640, 520,  &gui_run_data,  "File manager (FLTK)" );
    g_mainWnd = &win;
    win.show(argc, argv);

    std::thread programm_thread( programm_thread_runner, std::ref(gui_run_data) ); 


    //feedback thread
    std::thread reader(feedback_reader_thread);  
    Fl::add_timeout(0.05, timer_callback); // Пуск таймера

    int err = Fl::run();
    
    keep_running = false;
    reader.join();  

    {
        std::unique_lock<std::mutex> lock(gui_run_data.mtx);
        gui_run_data.stop = true;
        gui_run_data.cv_gui.notify_one();
    }

    programm_thread.join();

    
 	parser_clean();
    return err;
}

