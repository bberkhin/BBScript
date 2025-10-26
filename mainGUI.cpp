#include "mainwnd.h"
#include "mrobotimpl.h"
#include "platform_util.h"
#include <thread>
#include <atomic>
#include <map>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
#include "pcanimpl.h"


MRobot g_robot_;


std::atomic<bool> keep_running = true;
std::mutex mtx;
FeedbackGUIData  com_map;


TerminalQuery terminal_q; 

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
        print_terminal(MSG_TYPE_INFO, "Start checking...");
        if ( input.empty() )
            return;
        int res = parser_parse( input.c_str() );	
        if ( res != 0 ) {
            print_terminal(MSG_TYPE_INFO, "Stop: Syntax error");
            return;
        }
        int exec_res = parser_execute();
        if ( exec_res != 0 ) {
            print_terminal(MSG_TYPE_INFO, "Stop: Execution error");            
        }
        else
            print_terminal(MSG_TYPE_INFOOK, "Program completed successfully");            

    } catch(const std::exception& e)
    {
        print_terminal(MSG_TYPE_ERROR, "Catch exception: %s", e.what() );
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

void update_display_cb(void*) {
    std::queue<terminal_msg> local_queue;
    
    // Копируем все сообщения под защитой мьютекса
    {
        std::lock_guard<std::mutex> lock(mtx);
        local_queue.swap(terminal_q.messages);
    }
    
    // Обновляем GUI без удержания мьютекса
    while (!local_queue.empty()) {
        print_gui_terminal( local_queue.front().first, local_queue.front().second );        
        local_queue.pop();
    }
  //  g_mainWnd->redraw();    
   // Fl::flush();  // применяет обновления немедленно
}

void timer_callback(void*) 
{
    //check feedback
    {
        std::lock_guard<std::mutex> lock(mtx);
        g_mainWnd->setFeedback(com_map);
    }
    //check terminal
    {
        std::lock_guard<std::mutex> lock(terminal_q.mtx);
        if ( !terminal_q.messages.empty() )
            Fl::awake(update_display_cb, nullptr); // Потокобезопасный вызов            
    }
    Fl::repeat_timeout(0.05, timer_callback); // каждые 50 мс
}

void init_feedback_data()
{
    FeedbackJoint data;
    IJointController* jctrl = g_robot_.getJointController();     
    std::lock_guard<std::mutex> lock(mtx);    
    for( data.index = 0; data.index < jctrl->getJointsCount(); data.index++) {
        JointPtr jnt = jctrl->getJointByIndex(data.index);
        com_map[ jnt->getName() ] = data;
    }    
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
        std::string cfg_file = "C:/Projects/BBRobot/BBScript/config.xml";
        std::string msg("Parse Config: ");
        msg += cfg_file;
        print_terminal(MSG_TYPE_INFO, msg.c_str() );
        rapidxml::file<> xmlFile( cfg_file.c_str() );
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
			    print_terminal(MSG_TYPE_INFOOK, "CAN opened successfully");
		    }
            else
               print_terminal(MSG_TYPE_ERROR, "CAN not opened: %s", s ? s : DEFAULT_COM_PORT );

        }
        else
          print_terminal(MSG_TYPE_WARNING, "tag <CAN> not found in config.xml so can not opend");

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
                print_terminal(MSG_TYPE_WARNING, "<port> not found in <com> in  config.xml");
            com->setPort( s ? s : DEFAULT_COM_PORT );
            attr = nd_com->first_attribute("bound");
            if ( attr ) {
                s = attr->value();  
            } else {
                s = nullptr;
                print_terminal(MSG_TYPE_WARNING, "<bound> not found in <com> in  config.xml");
            }
            com->setBaudrate( s ? atoi(s) : DEFAULT_COM_BAUNDRATE );
    		com->open();
	    	if ( com->isOpen() )  {
			    print_terminal(MSG_TYPE_INFOOK, "COM port opened successfully");
		    }
        }
        else
          print_terminal(MSG_TYPE_WARNING,"tag <COM> not found in config.xml so com port not opend");
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
        print_terminal(MSG_TYPE_ERROR, e.what() );
    }
    catch(const std::exception& e)
	{
		print_terminal(MSG_TYPE_ERROR, e.what() );
	}
    return true;
}

extern int Ww, Wh;
int main(int argc, char **argv) {

    init_main_thread_id();

    SyncExchange gui_run_data;
    Fl_Window *window = new Fl_Window(Ww, Wh, "BBRobot UI");
    window->color(FL_WHITE);
    g_mainWnd = new MainWnd(0, 0, Ww, Wh,&gui_run_data);
    window->end();
    window->resizable(g_mainWnd);
    window->size_range(Ww, Wh);
    window->show(argc, argv);
    window->size(Ww + Ww/2, Wh+Wh);


    init_config();  	
	// Initialize parser
	if ( parser_intit() != 0 ) {
    	fatal_error("Failed to initialize parser\n");
		return 1;
	}
    init_feedback_data();
    g_mainWnd->configUpdated();

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
    delete window;
    return err;
}

