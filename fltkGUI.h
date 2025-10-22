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
#include <vector>
#include <string>
#include <filesystem>
#include "mrobotimpl.h"
#include <condition_variable>
#include <atomic>
#include <map>



#define MAX_FILE_EDIT_SIZE 1024
#define MAX_FILE_LINE_SIZE 128
#define STATUS_H 120
#define BUTTON_H 30
#define SPACING_H 5
#define MAX_JOINTS 6

#define SL_HEIGHT 25
#define SL_WIDTH  350
#define SL_SPACING  12
#define SL_Y0 80
#define SL_X 80

namespace fs = std::filesystem;
typedef std::map<std::string, FeedbackJoint> FeedbackGUIData;




struct SyncExchange {
    std::mutex mtx;
    std::condition_variable cv_gui;
 //   std::condition_variable cv_gcmc;
    std::string str_prg;
    std::string fname;
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
    Fl_Tabs *tabs;
    std::vector<Fl_Hor_Value_Slider*> sliders;
    std::string selected_file;
    static SyncExchange *gui_run_data;
};

extern MRobot g_robot_;
extern FileManager *g_mainWnd;

