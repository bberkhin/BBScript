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
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Terminal.H>
#include <FL/Fl_Text_Buffer.H>
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
typedef std::map<std::string, FeedbackJoint> FeedbackGUIData;




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
    void set_lines(const std::vector<std::string>& new_lines);
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
private:
    Fl_Group *CreateEditTab(int x, int y,int W, int H);
    Fl_Group *CreateProgramsTab(int x, int y,int W, int H);
    Fl_Group *CreateMoveTab(int x, int y, int W, int H);
public:
    void save_file();
    void run_edit();
    void run_file();
    void new_file();
    void edit_file();
    void setFeedback(const FeedbackGUIData  &map_data);
    void UpdateStatusLine();
    void show_editor(const std::string& filename, bool is_new);
    void print_terminal(uint8_t type, const char *s);
    void clear_terminal();
    static void slider_cb(Fl_Widget* w, void* data);
    static void browser_cb(Fl_Widget *w, void *data);
    static void limit_text_cb(int pos, int nInserted, int nDeleted, int nRestyled, const char *deletedText, void *cbArg);


private:
    MyFileBrowser *browser;
    //Fl_Text_Display *terminal;
    Fl_Terminal *terminal;
    //Fl_Text_Buffer *tbuf;
    //Fl_Text_Buffer *sbuf;
    StatusBar *feedback_bar;
    std::vector<Fl_Hor_Value_Slider*> sliders;
    std::string selected_file;
    static SyncExchange *gui_run_data;
    Fl_Tabs *tabs;
    Fl_Group *edit_tab;
    Fl_Multiline_Input *editor;
    Fl_Box *statusline;
        
};

/*
class FileManager : public Fl_Window 
{
    enum Mode { modeLIST, modeEDIT } mode = modeLIST;
public:
    FileManager(int W, int H, SyncExchange *g_r_d, const char*L = 0);
    void setMode(Mode md);
    void UpdateStatus(int ln);
    void setStatusText(int i, const char *s);
    void update_file_list();
    void resize(int X, int Y, int W, int H) override;
    void show_editor(const std::string& filename, bool is_new = false);
    void hide_editor();
    void save_file();
    void setFeedback(const FeedbackGUIData  &data);

    Fl_Group *CreateProgrammTab(int x, int y,int H, int W);
    Fl_Group *CreatMoveTab(int x, int y, int H, int W);

    static void browser_cb(Fl_Widget *w, void *data);
    static void run_cb(Fl_Widget*, void *data);
    static void edit_cb(Fl_Widget*, void *data);
    static void close_cb(Fl_Widget *, void *data);
    static void new_cb(Fl_Widget*, void *data);
    static void save_cb(Fl_Widget*, void *data);
    static void slider_cb(Fl_Widget* w, void* data);


private:
    MyFileBrowser *browser;
    Fl_Button *run_btn, *close_btn, *edit_btn, *new_btn, *save_btn;
    Fl_Multiline_Input *editor;
    StatusBar *status_bar;
    StatusBar *feedback_bar;
    Fl_Tabs *tabs;
    std::vector<Fl_Hor_Value_Slider*> sliders;
    std::string selected_file;
    static SyncExchange *gui_run_data;
};
*/
extern MRobot g_robot_;
extern MainWnd *g_mainWnd;

