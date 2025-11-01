//
// Resize example for use in the Fast Light Tool Kit (FLTK) documentation.
//
//     See Article #415: How does resizing work?
//     https://www.fltk.org/articles.php?L415
//
// Copyright 1998-2020 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

#include "mainwnd.h"
#include "mrobotimpl.h"
#include <fstream>
#include <cstdio>
#include "script_bind.h"
#include "platform_util.h"
#include "macroses.h"


// window, simplex and arrow dimensions
int TLx = 5, TRx = 310, TLw = 300, Ww = 615;
int TLy = 5, LGy = 155, TLh = 145, LGh = 120, LAh = 35, Wh = 280;
int h_button = 25;// 25+2+2
int btn_w = 100;
int hTab = 25;
extern FeedbackGUIData  com_map;


/*
 Fl_Text_Display::Style_Table_Entry stable[] = {
        { FL_BLACK,      FL_HELVETICA, FL_NORMAL_SIZE }, // 'A'   MSG_TYPE_INFO = 0,
        { FL_DARK_GREEN, FL_HELVETICA, FL_NORMAL_SIZE }, // 'B' — MSG_TYPE_INFOOK = 1,
        { FL_DARK_YELLOW,FL_HELVETICA, FL_NORMAL_SIZE }, // 'C' — MSG_TYPE_WARNING = 2, 
        { FL_RED,        FL_HELVETICA, FL_NORMAL_SIZE },  // 'D' — MSG_TYPE_ERROR = 3, 
        { FL_DARK_BLUE,  FL_HELVETICA, FL_NORMAL_SIZE }  // 'E' — MSG_TYPE_DEBUG = 4, 
    };
*/
static const char* ansi_reset = "\033[0m";
static const char* ansi_black = "\033[30m";
static const char* ansi_green = "\033[32m";
static const char* ansi_orange = "\033[33m";
static const char* ansi_red = "\033[31m";
static const char* ansi_gray = "\033[90m";


MainWnd *g_mainWnd = nullptr;
SyncExchange *MainWnd::gui_run_data = nullptr;
void run_programm(const std::string &input );

Fl_Group *MainWnd::CreateEditTab(int x, int y, int W, int H )
{
    Fl_Group *tab1 = new Fl_Group(x,  y, W, H, "New");
    //need button /internal tab
    Fl_Group *tI = new Fl_Group(tab1->x() , tab1->y() ,  tab1->w(), tab1->h());//);
    tI->begin();

    macro_choice = new Fl_Choice(x, y, W, h_button,  "Macro:");
    editor = new Fl_Multiline_Input(tI->x(), tI->y() + h_button, tI->w(), tI->h() - 2*h_button-4 ); 
    int yb = tI->y() + tI->h() - h_button - 2;

    Fl_Group *th = new Fl_Group(tI->x(), yb, 4*btn_w, h_button );//);
    Fl_Button *run_btn    = new Fl_Button(tI->x() + 2, yb, btn_w, h_button, "Run");
    Fl_Button *save_btn   = new Fl_Button(tI->x() + 2 + btn_w + 2, yb, btn_w, h_button, "Save");  
    th->clip_children(0);
    th->end();
    th->resizable(0);
    tI->end();
    tab1->end();
    tab1->resizable(tI);
    tI->resizable(editor); 

    macro_choice->callback([](Fl_Widget* w, void* data) {
        Fl_Choice *choice = (Fl_Choice *)w;
        int index = choice->value();
        MainWnd* self = static_cast<MainWnd*>(data);
        const auto& macros = Macroses::getInstance().GetMacros();
        if (index >= 0 && index < (int)macros.size()) {
            self->editor->value(macros[index].code.c_str());
        }
        self->editor->take_focus();

        }, this);
    run_btn->callback([](Fl_Widget* w, void* data) {
            MainWnd* self = static_cast<MainWnd*>(data);
            self->run_edit();
            }, this);
            
    save_btn->callback( [](Fl_Widget* w, void* data) {
            MainWnd* self = static_cast<MainWnd*>(data);
            self->save_file();
            }, this);
    return tab1;
}


Fl_Group *MainWnd::CreateProgramsTab(int x, int y,int W, int H)
{
   Fl_Group *tab1 = new Fl_Group(x,  y, W, H, "Files");
    //need button /internal tab
    Fl_Group *tI = new Fl_Group(tab1->x() , tab1->y() ,  tab1->w(), tab1->h());//);
    tI->begin();
    browser = new MyFileBrowser(tI->x(), tI->y(), tI->w(), tI->h() - h_button-4 ); 
    int yb = tI->y() + tI->h() - h_button - 2;

    Fl_Group *th = new Fl_Group(tI->x(), yb, 4*btn_w, h_button );//);
    Fl_Button *run_btn    = new Fl_Button(tI->x() + 2, yb, btn_w, h_button, "Run");
    Fl_Button *edit_btn   = new Fl_Button(tI->x() + 2 + btn_w + 2, yb, btn_w, h_button, "Edit");  
    Fl_Button *new_btn   = new Fl_Button(tI->x() + 2 + btn_w + 2, yb, btn_w, h_button, "New");  
    th->clip_children(0);
    th->end();
    th->resizable(0);
    tI->end();
    tab1->end();
    tab1->resizable(tI);
    tI->resizable(browser); 

    run_btn->callback([](Fl_Widget* w, void* data) {
            MainWnd* self = static_cast<MainWnd*>(data);
            self->run_file();
            }, this);
    edit_btn->callback([](Fl_Widget* w, void* data) {
            MainWnd* self = static_cast<MainWnd*>(data);
            self->edit_file();
            }, this);
    new_btn->callback([](Fl_Widget* w, void* data) {
            MainWnd* self = static_cast<MainWnd*>(data);
            self->new_file();
            }, this);


    browser->when(FL_WHEN_CHANGED);
    browser->callback(browser_cb, this);
    browser->update_files_list();
    browser->type(FL_HOLD_BROWSER);

    return tab1;
        

}

#define SL_HEIGHT 25
#define SL_WIDTH  350
#define SL_SPACING  12
#define SL_Y0 80
#define SL_X 80

void MainWnd::addMoveButtons(int x, int y,int W, int H, int index)
{
    int btnlr_w = 30;
    Fl_Button *btn_left    = new Fl_Button(x, y, btnlr_w, H, "<<");
    Fl_Button *btn_right     = new Fl_Button(x + btnlr_w +2, y, btnlr_w, H, ">>");
    Fl_Float_Input *step     = new Fl_Float_Input(x + 3*(btnlr_w +2) + 5, y, 50, H, "Step:");
    step->align(FL_ALIGN_LEFT);    
    JointPtr jnt = g_robot_.getJointController()->getJointByIndex(index);
    step->value( jnt ? (double)jnt->getStep() : 0.1);
    btn_left->callback([](Fl_Widget* w, void* data) {
            int idx = (int)(intptr_t)data;    
            JointPtr jnt = g_robot_.getJointController()->getJointByIndex(idx);
            if ( jnt )
                jnt->moveSteps(-1);
            }, (void*)(intptr_t)index);
    btn_left->callback([](Fl_Widget* w, void* data) {
            int idx = (int)(intptr_t)data;    
            JointPtr jnt = g_robot_.getJointController()->getJointByIndex(idx);
            if ( jnt )
                jnt->moveSteps(1);
            }, (void*)(intptr_t)index);
    
    step->callback([](Fl_Widget* w, void* data) {
            int idx = (int)(intptr_t)data;  
            Fl_Float_Input *step = (Fl_Float_Input*)w;
            // Получаем строку из поля ввода
            const char *value_str = step->value();
            JointPtr jnt = g_robot_.getJointController()->getJointByIndex(idx);
            if ( jnt && value_str && *value_str )
            {
                float value = atof(value_str);
                if ( value != 0.f)
                    jnt->setStep(value);

            }}, (void*)(intptr_t)index);
    step->when(FL_WHEN_CHANGED); 
}


Fl_Group *MainWnd::CreateMoveTab(int x, int y,int W, int H)
{
     //tI->resizable(dummy);
    Fl_Group *m_tab = new Fl_Group(x,y,W,H, "Move");
    Fl_Group *slider_group = new Fl_Group(m_tab->x() , m_tab->y() ,  m_tab->w(), m_tab->h());//);

    IJointController *cntrl = g_robot_.getJointController();
   
    for (int i = 0; i < MAX_JOINTS; ++i) {
        JointPtr jnt = cntrl->getJointByIndex(i);
        if ( !jnt )
            break;
        int sy = slider_group->y() + 20 + i * (SL_HEIGHT + SL_SPACING);
        Fl_Hor_Value_Slider* slider = new Fl_Hor_Value_Slider(SL_X, sy, SL_WIDTH, SL_HEIGHT, jnt->getName().c_str() );
        slider->type(FL_HOR_NICE_SLIDER);
        const JointLimit& limits = jnt->getLimits();
        if ( limits.pos_min_ != limits.pos_max_ )
           slider->bounds(limits.pos_min_, limits.pos_max_);
        else
            slider->bounds(-1, 1);
        slider->user_data((void*)(intptr_t)i);
        slider->value(0);
        slider->align(FL_ALIGN_LEFT);

        slider->callback(slider_cb, (void*)(intptr_t)i);
        
        sliders.push_back(slider);
        addMoveButtons(SL_X + SL_WIDTH +5, sy, W - SL_X - SL_WIDTH -5, SL_HEIGHT, i ); // <<>> step
    }
    slider_group->end();
    slider_group->resizable(0);
    m_tab->end();
    return m_tab;
}
#include "tracerecoder.h"

Fl_Group *MainWnd::CreateRecordTab(int x, int y, int W, int H)
{
       //tI->resizable(dummy);
    Fl_Group *m_tab = new Fl_Group(x,y,W,H, "Record");
    Fl_Group *ti = new Fl_Group(m_tab->x() , m_tab->y() ,  m_tab->w(), m_tab->h());//);

    int btnlr_hw = 100;
    
    Fl_Button *btn_start    = new Fl_Button(x, y, btnlr_hw, btnlr_hw, "Start");
    Fl_Button *btn_stop     = new Fl_Button(x + btnlr_hw +2, y, btnlr_hw, btnlr_hw, "Stop");
    Fl_Button *btn_play     = new Fl_Button(x + 2*(btnlr_hw +2), y, btnlr_hw, btnlr_hw, "Play");
   
    ti->end();
    ti->resizable(0);
    m_tab->end();

    btn_start->callback([](Fl_Widget *w, void *data) {
        Fl_Button *btnstart  = (Fl_Button *)w;
        Fl_Button *btnstp  = (Fl_Button *)data;
        TraceRecorder *tr = TraceRecorder::getInstance();
        tr->startRecording(); 
        if ( tr->isRunning() )
        {
            btnstart->deactivate();
            btnstp->activate();
        } }, btn_stop);

    btn_stop->callback([](Fl_Widget *w, void *data) {
        Fl_Button *btnstop  = (Fl_Button *)w;
        Fl_Button *btnstart  = (Fl_Button *)data;
        TraceRecorder *tr = TraceRecorder::getInstance();
        tr->stopRecording();         
        if ( !tr->isRunning() )
        {
            btnstart->activate();
            btnstop->deactivate();
        }
        }, btn_start);
    
    btn_play->callback([](Fl_Widget *) {
        TraceRecorder *tr = TraceRecorder::getInstance();
        tr->playback();         
        });


    return m_tab;
}


bool MainWnd::restoreJointParam(int idx)
{
    JointPtr j = g_robot_.getJointController()->getJointByIndex(idx);
    float val =  j ? j->getParameter(JOINT_MOTOR_SPEEDLIMIT) : 0.f;
    speedLimit->value(std::to_string(val).c_str());
    val =  j ? j->getParameter(JOINT_MOTOR_ACCLIMIT) : 0.f;
    accLimit->value(std::to_string(val).c_str());
    val =  j ? j->getParameter(JOINT_MOTOR_CURLIMIT) : 0.f;
    currentLimit->value(std::to_string(val).c_str());
    motorModified(false);
    val =  j ? j->getParameter(JOINT_MOTOR_VOLTAGE) : 0.f;
    voltage->value(std::to_string(val).c_str());   
    last_motor_choice_idx_ = idx;     
    return true;
}

bool MainWnd::saveJointOneParam(JointPtr j, Fl_Float_Input *input, JOINT_MOTOR_PARAM type )
{
    float val =  atof ( input->value() );
    if ( j->isAcceptable( type, val ) )
    {
        j->setParameter(type, val );
    }
    else
    {
        const JointLimit& l = j->getLimits();
        switch( type)
        {
            case JOINT_MOTOR_SPEEDLIMIT: fl_alert( "Invalid speed limit %f\nShould be >= 0.1 and <= %f!",val, l.vel_max_ ); break;
            case JOINT_MOTOR_ACCLIMIT: fl_alert( "Invalid acceleration limit %f\nShould be >= 0.1 and <= %f!",val, l.acc_max_ ); break;
            case JOINT_MOTOR_CURLIMIT: fl_alert( "Invalid current limit %f\nShould be >= 0.1 and <= %f!",val, l.cur_max_ ); break;
        }
        return false;
    }
    return true;
}

bool MainWnd::saveJointParam(int idx)
{
    JointPtr j = g_robot_.getJointController()->getJointByIndex(idx);
    if ( !j )
        return true;
    
    if ( !saveJointOneParam( j, speedLimit, JOINT_MOTOR_SPEEDLIMIT) )
        return false;
    if ( !saveJointOneParam( j, accLimit, JOINT_MOTOR_ACCLIMIT) )
        return false;
    if ( !saveJointOneParam( j, currentLimit, JOINT_MOTOR_CURLIMIT) )
        return false;

    g_mainWnd->motorModified(false);
    return true;
}

Fl_Group* MainWnd::CreateMotorTab(int xs, int ys, int Ws, int Hs) {
    Fl_Group *motorTab = new Fl_Group(xs, ys, Ws, Hs, "Motor");
    Fl_Group *ti = new Fl_Group(motorTab->x() , motorTab->y() ,  motorTab->w(), motorTab->h());//);


    // ComboBox with available motors
    int x = xs + 130;
    int W = Ws - 135;
    int y = ys + 20;
    motor_choice = new Fl_Choice(x, y, W, h_button, "Joints:");

    IJointController *cntrl = g_robot_.getJointController();
    for (int i = 0; i < MAX_JOINTS; ++i) {
        JointPtr jnt = cntrl->getJointByIndex(i);
        if ( !jnt )
            break;
        std::string fullname = jnt->getName();
        IMotorDriver *m = jnt->getMotorDriver();
        if ( m )
        {
            fullname += " ";
            fullname += jnt->getMotorDriver()->getType();
        }                     
        motor_choice->add( fullname.c_str() );
    }
    
    // Speed Limit Input
    y += (h_button +10);
    speedLimit = new Fl_Float_Input(x, y, 100, h_button, "Speed Limit:");
    speedLimit->align(FL_ALIGN_LEFT);

    // Acceleration Limit Input
    y += (h_button +10);
    accLimit = new Fl_Float_Input(x, y, 100, h_button, "Acceleration Limit:");
    accLimit->align(FL_ALIGN_LEFT);

    // Current Limit Input
    y += (h_button +10);
    currentLimit = new Fl_Float_Input(x, y, 100, h_button, "Current Limit:");
    currentLimit->align(FL_ALIGN_LEFT);
    
     // Current Limit Input
    y += (h_button +10);
    voltage = new Fl_Float_Input(x, y, 100, h_button, "Voltage:");
    voltage->align(FL_ALIGN_LEFT);
    voltage->deactivate();
    

    W = 100;
    x = ti->x()+1;
    y += (h_button +50);
    //y = ti->y() + ti->h() - h_button - 2;
    // Buttons
    Fl_Button *saveBtn = new Fl_Button(x, y, W, h_button, "Save");
    Fl_Button *restoreBtn = new Fl_Button(x + 10 + W, y, W, h_button, "Restore");
    

    ti->end();
    ti->resizable(0);
    motorTab->end();

    // State for tracking modifications
    
    motorModified(false);

    // Callback for motor selection
    //motor_choice->when(FL_WHEN_CHANGED);
    motor_choice->callback([](Fl_Widget *w) {      
        auto m_choice = (Fl_Choice *)w;
        auto new_idx = static_cast<Fl_Choice*>(w)->value();

        if ( g_mainWnd->motorModified() ) {
            int result = fl_choice("Parameters have changed. Save changes?", "Cancel", "Don't Save", "Save");
            if ( result == 2 ) { // Save
                if ( !g_mainWnd->saveJointParam( g_mainWnd->lastMotorChoice() ) )
                {
                    m_choice->value( g_mainWnd->lastMotorChoice());
                    return;
                }
            } else if (result == 0) { // Cancel, do not switch motor
                m_choice->value(g_mainWnd->lastMotorChoice());
                return;
            }
        // If Don't Save, just continue
        }      
        g_mainWnd->restoreJointParam(new_idx);

    });

    // Callbacks for data modification (any field edit)
    auto changed_cb = [](Fl_Widget* w, void* data) {
        g_mainWnd->motorModified(true);
    };
    speedLimit->callback(changed_cb);
    accLimit->callback(changed_cb);
    currentLimit->callback(changed_cb);

    // Restore button callback
    restoreBtn->callback([](Fl_Widget* w, void* data) {
        int idx = g_mainWnd->motor_choice->value();
        g_mainWnd->restoreJointParam(idx);
    });

    // Save button callback
    saveBtn->callback([](Fl_Widget* w, void* data) {
        int idx = g_mainWnd->motor_choice->value();
        g_mainWnd->saveJointParam(idx);
    });

    return motorTab;
}



//selection changed
void MainWnd::browser_cb(Fl_Widget *w, void *data) {
    MainWnd *fm = static_cast<MainWnd*>(data);
    MyFileBrowser* b = static_cast<MyFileBrowser*>(w);
    int idx = b->value();
    if ( idx >= 0 )
    {
        //printf( "Selected UKNOWN\n" );
        const char *s = b->text(idx);                                            
        if ( s )
        {
            std::string item(s);
            b->selectFileName(item);
        }        
        return;
    }      
}


void MainWnd::stop_cb(Fl_Widget* w, void* ) {
    g_robot_.stop();
}

void MainWnd::slider_cb(Fl_Widget* w, void* data) {
    int idx = (int)(intptr_t)data;    
    double pos = ((Fl_Hor_Value_Slider*)w)->value();
    mr_setpos(idx, pos);  // IMotorDriver::setTargetPosition(double position) override;
   // printf("Joint #%d: %.5f\n", idx, ((Fl_Hor_Value_Slider*)w)->value());
}

void MainWnd::UpdateStatusLine()
{
    if ( !browser )
        return;
    std::string status;
    if ( tabs->value() == browser_tab )
    {
        status = browser->getBrowseFile();
        if ( status.empty() )
        {
            status = browser->getDir().string();
        }
        else
        {   
            status += " Path: ";
            status += browser->getDir().string();        
        }
    }
    else if ( tabs->value() == edit_tab )
    {
        status = "Write your programm...";
    }
    else
        status = "...";

    statusline->copy_label(status.c_str());
    statusline->redraw();
}

void MainWnd::save_file() 
{
    if (!selected_file.empty()) {
        std::ofstream f((browser->getDir() / selected_file).string());
        if (f)
            f << editor->value();
    }
}



void MainWnd::run_edit() 
{        
    clear_terminal();
    std::unique_lock<std::mutex> lock(gui_run_data->mtx);
    gui_run_data->str_prg = editor->value();
    gui_run_data->fname.clear();
    gui_run_data->do_run = true;
    gui_run_data->cv_gui.notify_one();
}

void MainWnd::run_file()
{
    clear_terminal();
    std::string fname = browser->getBrowseFile();
    if ( fname.empty() )
        return;
    std::unique_lock<std::mutex> lock(gui_run_data->mtx);
    gui_run_data->str_prg.clear();
    gui_run_data->fname = fname;
    gui_run_data->do_run = true;
    gui_run_data->cv_gui.notify_one();
}

void MainWnd::new_file()
{
    show_editor("newfile.txt", true);

}

void MainWnd::edit_file()
{
    std::string fname = browser->getBrowseFile();
    if (!fname.empty())
        show_editor(fname,false);
}

void MainWnd::show_editor(const std::string& filename, bool is_new) 
{
    fs::path p = browser->getDir();
    fs::path full_path = p / filename;
    editor->value("");
    if (!is_new) {
        std::ifstream fin(full_path);
        if (fin)
        {
            std::string line;
            size_t total_len = 0;
            while (std::getline(fin, line)) {
                if (line.size() > MAX_FILE_LINE_SIZE) continue;
                line += "\n";
                if (total_len + line.size() > MAX_FILE_EDIT_SIZE) break;
                editor->append( line.c_str() );
                total_len += line.size();
            }
        }
    }       
    selected_file = filename;
    tabs->value(edit_tab);
}

void MainWnd::tabs_cb(Fl_Widget* w, void *data) {
    MainWnd *wnd = static_cast<MainWnd*>(data);
    if ( wnd )
        wnd->UpdateStatusLine();
}
  
MainWnd::MainWnd(int X, int Y, int W, int H, SyncExchange *gui_rd)
  : Fl_Group(X, Y, W, H) {
    gui_run_data = gui_rd;

    Fl_Group *winTab = new Fl_Group(X + TLx, Y + TLy, TLw, TLh);//);

    tabs = new Fl_Tabs(X + TLx, Y + TLy, TLw, TLh);
// insert Tab Edit
    edit_tab = CreateEditTab(X + TLx, Y + TLy+hTab, TLw, TLh-hTab);

    browser_tab = CreateProgramsTab(X + TLx, Y + TLy+hTab, TLw, TLh-hTab);
    CreateRecordTab(X + TLx, Y + TLy+hTab, TLw, TLh-hTab);
    
   
    tabs->end();
    tabs->callback(tabs_cb, this);
    tabs->when(FL_WHEN_CHANGED);

    // Важно: фиксируем вкладки, делаем ресайз только содержимого
    tabs->resizable(edit_tab);  // Можно указать любую существующую вкладку
    winTab->resizable(tabs);
    winTab->end();
    this->resizable(winTab);

    //right side
    Fl_Group *TR = new Fl_Group(X + TRx, Y + TLy, TLw, TLh); 
    Fl_Box *title = new Fl_Box(X + TRx, Y + TLy, TLw, hTab, "Joints Positions");
     
    Fl_Group *mg = new Fl_Group(TR->x(), TR->y()+hTab, TR->w(), TR->h()-hTab);
    feedback_bar = new StatusBar(mg->x()+2, mg->y()+2, mg->w()-4, mg->h()-4, 6);
    mg->end();

    TR->resizable(mg);
    TR->end();

    // terminal
    Fl_Group *LG = new Fl_Group(X + TLx, Y + LGy,  TRx + TLw - TLx, LGh);
    LG->box(FL_BORDER_FRAME);
    LG->color(FL_GREEN);
    int hTer =  LG->h()- 20;
    terminal = new Fl_Terminal(LG->x(), LG->y(), LG->w() - hTer - 1 ,hTer);  
    terminal->ansi(true);                 // Разрешить ANSI цвета
    terminal->color(FL_WHITE);          // Установить белый фон
    terminal->textcolor(FL_BLACK);   
    //terminal->activate();
    LG->resizable(terminal );
    //Stop button
    Fl_Button *stop_btn    = new Fl_Button(terminal->x() + terminal->w() + 1 , terminal->y(), hTer, hTer, "STOP");
    stop_btn->callback(stop_cb, this);
    stop_btn->color(FL_DARK_RED);
    stop_btn->labelcolor(FL_WHITE);

    //status line
    statusline = new Fl_Box(LG->x(), LG->y() + LG->h() - 20, LG->w(), 20);
    statusline->box(FL_DOWN_BOX);
    statusline->align( FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    statusline->label("Hello Robot");
    statusline->redraw();
    LG->end();
    this->end();

    std::string hello_msg("Start terminal...");
    this->print_terminal(MSG_TYPE_INFOOK, hello_msg);
    //this->UpdateStatusLine();

}

void MainWnd::configUpdated()
{
    move_tab = CreateMoveTab(tabs->x(), tabs->y() + hTab, tabs->w(), tabs->w()-hTab);
    tabs->add(move_tab);    
    setFeedback(com_map);

    motor_tab = CreateMotorTab(tabs->x(), tabs->y() + hTab, tabs->w(), tabs->w()-hTab);
    tabs->add(motor_tab);

       // Заполняем комбобокс именами макросов
    const auto& macroses = Macroses::getInstance().GetMacros();
    for (const auto& macro : macroses) {
        macro_choice->add(macro.name.c_str());
    }
    
}

void MainWnd::setFeedback(const FeedbackGUIData  &map_data)
{
    int index = 0;
    std::string data;
    if ( !feedback_bar )
        return;

    for (const auto& kv : map_data) 
    {
        const std::string& key = kv.first;
        const FeedbackJoint& fb = kv.second;
        data = key;
        data += ": Pos: ";
        data += std::to_string(fb.pos);
        data += " Vel: ";
        data += std::to_string(fb.vel);
        feedback_bar->set_line(index++, data.c_str() );  
          
        if ( fb.index >= 0 && fb.index < sliders.size() )
        {
            Fl_Hor_Value_Slider *sl = sliders.at(fb.index );
            sl->value(fb.pos);
        }
            
    }
    //feedback_bar->parent()->redraw();
    feedback_bar->redraw();
    //Fl::flush();  // применяет обновления немедленно
}

void MainWnd::clear_terminal()
{
    if  (!terminal )
        return;

    terminal->reset_terminal();
    
    terminal->ansi(true);                 // Разрешить ANSI цвета
    terminal->color(FL_WHITE);          // Установить белый фон
    terminal->textcolor(FL_BLACK); 
    terminal->redraw();

}

void MainWnd::print_terminal( uint8_t type, std::string &s)
{   
    if  (!terminal )
        return;

    const char* color = ansi_black;

    switch (type) {
        case MSG_TYPE_INFO:    color = ansi_black;  break;
        case MSG_TYPE_INFOOK:  color = ansi_green;  break;
        case MSG_TYPE_WARNING: color = ansi_orange; break;
        case MSG_TYPE_ERROR:   color = ansi_red;    break;
        case MSG_TYPE_DEBUG:   color = ansi_gray;   break;
    }
    std::string msg = std::string(color) + s + ansi_reset + "\n";
    terminal->append(msg.c_str());
    /*
    terminal->parent()->redraw();
    terminal->redraw();    
    Fl::flush();  // применяет обновления немедленно
    */
}





//Status Bar
StatusBar::StatusBar(int X, int Y, int W, int H, int num_lines)
    : Fl_Box(X, Y, W, H), lines(num_lines) {}

void StatusBar::set_line(size_t idx, const std::string& text) {
    if (idx < lines.size()) {
        lines[idx] = text;
    }
}

size_t StatusBar::line_count() const {
    return lines.size();
}

void StatusBar::draw() {
    fl_push_clip(x(), y(), w(), h());
    fl_color(bg_clr);
    fl_rectf(x(), y(), w(), h());
    fl_color(txt_clr);
    fl_font(FL_HELVETICA, 14);
    int base_y = y() + 18;
    int line_height = 20;
    for (size_t i = 0; i < lines.size(); ++i) {
        fl_draw(lines[i].c_str(), x() + 5, base_y + i * line_height);
    }
    fl_pop_clip();
}


// --- MyFileBrowser ---

MyFileBrowser::MyFileBrowser(int X, int Y, int W, int H, const char *L)
    : Fl_File_Browser(X, Y, W, H, L)
{
    current_dir = fs::current_path();
}

int MyFileBrowser::handle(int event) 
{
    if (event == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
        int idx = value();
        if (idx > 0) {
            std::string fname = text(idx);
            processItem(idx);
        }
    }
    if (event == FL_PUSH && Fl::event_clicks()) {
        int idx = value();
        if (idx > 0) {
            std::string fname = text(idx);
            processItem(idx);
        }
    }
    return Fl_File_Browser::handle(event);    
}

void MyFileBrowser::update_files_list(const char *path)
{
    if (path) {
        current_dir = path;
        current_dir = fs::weakly_canonical(current_dir);
    }
    this->load(current_dir.string().c_str());
    UpdateStatusLine();
}

const std::string &MyFileBrowser::getBrowseFile() const {
    return current_br_file;
}

void MyFileBrowser::processItem(int idx)
{
    std::string fname = text(idx);    
    fs::path p = current_dir / fname;
    if (fs::is_directory(p)) {
        update_files_list(p.string().c_str());
    } else {
        g_mainWnd->show_editor(fname, false);
    }
    selectFileName(fname);
}

void MyFileBrowser::selectFileName(const std::string &fname)
{
    fs::path p = current_dir / fname;
    if (!fs::is_directory(p)) {
        current_br_file = fname;
    } else {
        current_br_file.clear();
    }
    UpdateStatusLine();
}

fs::path MyFileBrowser::getDir() const {
    return current_dir;
}



// --- Status line update function ---
void UpdateStatusLine()
{
    if( g_mainWnd )
        g_mainWnd->UpdateStatusLine();
}

void print_gui_terminal(uint8_t type, const char *s)
{
    if ( !g_mainWnd )
    {
        printf(s );
        return;
    }
    std::string str(s);
    g_mainWnd->print_terminal(type, str);
}


void print_gui_terminal(uint8_t type, std::string &s)
{
    if ( !g_mainWnd )
    {
        printf(s.c_str());
        return;
    }
    g_mainWnd->print_terminal(type, s);
}
