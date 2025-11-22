#pragma once

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_File_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/fl_message.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Hor_Value_Slider.H>
#include <FL/Fl_Terminal.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Float_Input.H>
#include <vector>
#include <string>
#include <filesystem>
#include <condition_variable>
#include <atomic>
#include <map>
#include "mrobotimpl.h"



#define MAX_FILE_EDIT_SIZE 1024
#define MAX_FILE_LINE_SIZE 128
#define MAX_JOINTS 6

namespace fs = std::filesystem;





struct SyncExchange {
    std::mutex mtx;
    std::condition_variable cv_gui;
 //   std::condition_variable cv_gcmc;
    std::string str_prg;
    std::string fname;
    std::string err_msg;
    bool do_run = false;
    bool done = false;
    std::atomic<bool> stop = false;

};



void UpdateStatusLine();

class StatusBar : public Fl_Box {
    std::vector<std::string> lines;
public:
    StatusBar(int X, int Y, int W, int H, int num_lines);

    void set_line(size_t idx, const std::string& text);
    size_t line_count() const;
    void draw() override;
    void setColor( Fl_Color _txt_clr, Fl_Color _bg_clr) {  txt_clr = _txt_clr; bg_clr = _bg_clr; }
private:
    Fl_Color txt_clr = FL_BLACK;
    Fl_Color bg_clr = FL_LIGHT2;
};

class MyFileBrowser : public Fl_File_Browser {
public:
    using Fl_File_Browser::Fl_File_Browser;
    MyFileBrowser(int X, int Y, int W, int H, const char *L = 0);
    int handle(int event) override;
    void update_files_list(const char *path = 0);
    const std::string &getBrowseFile() const;
    void processItem(int idx);
    void selectFileName(const std::string &fname);
    fs::path getDir() const;
private:
    fs::path current_dir;
    std::string current_br_file;
};

class MainWnd : public Fl_Group 
{
public:
    MainWnd(int X, int Y, int W, int H, SyncExchange *gui_rd );
    void configUpdated(); // cal when congfig file loaded and robot configured
private:
    Fl_Group *CreateEditTab(int x, int y,int W, int H);
    Fl_Group *CreateProgramsTab(int x, int y,int W, int H);
    Fl_Group *CreateMoveTab(int x, int y, int W, int H);
    Fl_Group *CreateRecordTab(int x, int y, int W, int H);
    Fl_Group *CreateMotorTab(int x, int y, int W, int H);
    void addMoveButtons(int x, int y,int W, int H, int index);
public:
    void save_file();
    void run_edit();
    void run_file();
    void new_file();
    void edit_file();
    void updateFeedback();
    void UpdateStatusLine();
    void show_editor(const std::string& filename, bool is_new);
    void print_terminal(uint8_t type, std::string &s);
    void clear_terminal();
    static void slider_cb(Fl_Widget* w, void* data);
    static void browser_cb(Fl_Widget *w, void *data);
    static void tabs_cb(Fl_Widget* w, void *data);
    static void stop_cb(Fl_Widget* w, void *data);
    void motorModified(bool b ) { modified_ = b; }
    bool motorModified() { return modified_; }
    bool restoreJointParam(int idx);
    bool saveJointParam(int idx);
    int lastMotorChoice() { return last_motor_choice_idx_; }
    bool saveJointOneParam(JointPtr j, Fl_Float_Input *input, JOINT_MOTOR_PARAM type );

private:
    MyFileBrowser *browser = nullptr;
    Fl_Terminal *terminal = nullptr;
    StatusBar *feedback_bar = nullptr;
    Fl_Tabs *tabs = nullptr;
    Fl_Group *edit_tab = nullptr;
    Fl_Group *browser_tab = nullptr;
    Fl_Group *move_tab = nullptr;
    Fl_Group *motor_tab = nullptr;
    Fl_Multiline_Input *editor = nullptr;
    Fl_Choice *macro_choice= nullptr;
    Fl_Choice *motor_choice= nullptr;
    Fl_Box *statusline = nullptr;
    Fl_Float_Input *speedLimit = nullptr;
    Fl_Float_Input *accLimit   = nullptr;
    Fl_Float_Input *currentLimit = nullptr;
    Fl_Float_Input *voltage = nullptr;

    std::vector<Fl_Hor_Value_Slider*> sliders;
    std::string selected_file;
    bool modified_ = false;
    int last_motor_choice_idx_ = -1;
    
    static SyncExchange *gui_run_data;
        
};

extern MRobot g_robot_;
extern MainWnd *g_mainWnd;

