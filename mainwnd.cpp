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


// window, simplex and arrow dimensions
int TLx = 5, TRx = 310, TLw = 300, Ww = 615;
int TLy = 5, LGy = 155, TLh = 145, LGh = 120, LAh = 35, Wh = 280;
int h_button = 25;// 25+2+2
int btn_w = 100;



MainWnd *g_mainWnd = nullptr;
SyncExchange *MainWnd::gui_run_data = nullptr;
void run_programm(const std::string &input );
extern MRobot g_robot_;





Fl_Group *MainWnd::CreateEditTab(int x, int y, int W, int H )
{
    Fl_Group *tab1 = new Fl_Group(x,  y, W, H, "New");
    //need button /internal tab
    Fl_Group *tI = new Fl_Group(tab1->x() , tab1->y() ,  tab1->w(), tab1->h());//);
    tI->begin();
    editor = new Fl_Multiline_Input(tI->x(), tI->y(), tI->w(), tI->h() - h_button-4 ); 
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


Fl_Group *MainWnd::CreateMoveTab(int x, int y,int W, int H)
{
     //tI->resizable(dummy);
    Fl_Group *move_tab = new Fl_Group(x,y,W,H, "Move");

    //Fl_Box* dummy = new Fl_Box(W-1,W - 31,1,1);
    //move_tab->resizable(dummy);

    // Группа для слайдеров с жёстко фиксированной позицией/размером (НЕ resizable!)
    //int grp_y = 40, grp_h = MAX_JOINTS * SL_HEIGHT + (MAX_JOINTS - 1) * SL_SPACING + 2 * SL_Y0;
    //Fl_Group *slider_group = new Fl_Group(10, grp_y, SL_WIDTH, grp_h);
    Fl_Group *slider_group = new Fl_Group(move_tab->x() , move_tab->y() ,  move_tab->w(), move_tab->h());//);

    IJointController *cntrl = g_robot_.getJointController();
   
    for (int i = 0; i < MAX_JOINTS; ++i) {
        JointPtr jnt = cntrl->getJointByIndex(i);
        if ( !jnt )
            break;
        int sy = SL_Y0 + i * (SL_HEIGHT + SL_SPACING);
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
    }
    slider_group->end();
    slider_group->resizable(0);
    move_tab->end();
    return move_tab;
}


//selection changed
void MainWnd::browser_cb(Fl_Widget *w, void *data) {
    MainWnd *fm = static_cast<MainWnd*>(data);
    MyFileBrowser* b = static_cast<MyFileBrowser*>(w);
    int idx = b->value();
    if ( idx >= 0 )
    {
        //printf( "Selected UKNOWN\n" );
        std::string item = b->text(idx);                                            
        b->selectFileName(item);
        return;
    }   
/*             
    if (Fl::event_clicks()) {
        //printf( "Double Click %s\n", item.c_str() );
        b->processItem(idx);
    }
  */      
}



void MainWnd::slider_cb(Fl_Widget* w, void* data) {
    int idx = (int)(intptr_t)data;    
    double pos = ((Fl_Hor_Value_Slider*)w)->value();
    mr_setpos(idx, pos);  // IMotorDriver::setTargetPosition(double position) override;
   // printf("Joint #%d: %.5f\n", idx, ((Fl_Hor_Value_Slider*)w)->value());
}

void MainWnd::UpdateStatusLine()
{
    std::string status = browser->getBrowseFile();
    if ( status.empty() )
    {
        status = browser->getDir().string();
    }
    else
    {   
        status += " Path: ";
        status += browser->getDir().string();        
    }
    
    statusline->label(status.c_str());
    statusline->parent()->redraw();
    statusline->redraw();
    Fl::flush();  // применяет обновления немедленно
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

 Fl_Text_Display::Style_Table_Entry stable[] = {
        { FL_BLACK,      FL_HELVETICA, FL_NORMAL_SIZE }, // 'A'   MSG_TYPE_INFO = 0,
        { FL_DARK_GREEN, FL_HELVETICA, FL_NORMAL_SIZE }, // 'B' — MSG_TYPE_INFOOK = 1,
        { FL_DARK_YELLOW,FL_HELVETICA, FL_NORMAL_SIZE }, // 'C' — MSG_TYPE_WARNING = 2, 
        { FL_RED,        FL_HELVETICA, FL_NORMAL_SIZE },  // 'D' — MSG_TYPE_ERROR = 3, 
        { FL_DARK_BLUE,  FL_HELVETICA, FL_NORMAL_SIZE }  // 'E' — MSG_TYPE_DEBUG = 4, 
    };

MainWnd::MainWnd(int X, int Y, int W, int H, SyncExchange *gui_rd)
  : Fl_Group(X, Y, W, H) {
    gui_run_data = gui_rd;
    int hTab = 25;

    Fl_Group *winTab = new Fl_Group(X + TLx, Y + TLy, TLw, TLh);//);
    tabs = new Fl_Tabs(X + TLx, Y + TLy, TLw, TLh);
// insert Tab Edit
    edit_tab = CreateEditTab(X + TLx, Y + TLy+hTab, TLw, TLh-hTab);

    CreateProgramsTab(X + TLx, Y + TLy+hTab, TLw, TLh-hTab);

  //  CreateMoveTab(X + TLx, Y + TLy+hTab, TLw, TLh-hTab);
   

    tabs->end();
    // Важно: фиксируем вкладки, делаем ресайз только содержимого
    tabs->resizable(edit_tab);  // Можно указать любую существующую вкладку
    winTab->resizable(tabs);

    //right side
    Fl_Group *TR = new Fl_Group(X + TRx, Y + TLy, TLw, TLh); 
    Fl_Box *title = new Fl_Box(X + TRx, Y + TLy, TLw, hTab, "Joints Positions");
     
    Fl_Group *mg = new Fl_Group(TR->x(), TR->y()+hTab, TR->w(), TR->h()-hTab);
    feedback_bar = new StatusBar(mg->x()+2, mg->y()+2, mg->w()-4, mg->h()-4, 6);
    feedback_bar->set_line(0, "Hello word");
    feedback_bar->set_line(1, "Hello word2");
    mg->end();

    TR->resizable(mg);
    TR->end();

    // terminal
    Fl_Group *LG = new Fl_Group(X + TLx, Y + LGy,  TRx + TLw - TLx, LGh);
    LG->box(FL_BORDER_FRAME);
    LG->color(FL_GREEN);

    //tbuf = new Fl_Text_Buffer();  
    //sbuf = new Fl_Text_Buffer(); // стили
   
    //terminal = new Fl_Text_Display(LG->x(), LG->y(), LG->w()-30, LG->h()-20);
    //terminal->buffer(tbuf);      
    //int stable_size = sizeof(stable) / sizeof(stable[0]);
    //terminal->highlight_data(sbuf, stable, stable_size, 'A', 0, 0);
    //tbuf->text("Start terminal...\nlekfjlef\nfekf\n\n\n\n\rlktlkg\n");
    //sbuf->text("BBBBBBBBBBBBBBBBB\nBBBBBBBB\nBBBB\n\n\n\n\nBBBBBB\n");
    terminal = new Fl_Terminal(LG->x(), LG->y(), LG->w()-30, LG->h()-20);  
    terminal->append("Start terminal...\nlekfjlef\nfekf\n\n\n\n\rlktlkg\n");

    terminal->activate(); 


  //  tbuf->add_modify_callback(limit_text_cb, this);
    LG->resizable(terminal );
    statusline = new Fl_Box(LG->x(), LG->y() + LG->h() - 20, LG->w(), 20);
   // statusline->color(FL_MAGENTA);
    statusline->box(FL_DOWN_BOX);
    statusline->align( FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
    statusline->label("");
    statusline->redraw();

    LG->end();

    this->resizable(winTab);
    this->end();
}


void MainWnd::setFeedback(const FeedbackGUIData  &map_data)
{
    int index = 0;
    std::string data;
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
}

/*
const int MAX_TEXT_SIZE = 1024; // Максимум символов

// Callback, вызывается при изменении текста
void MainWnd::limit_text_cb(int pos, int nInserted, int nDeleted, int nRestyled,
                   const char *deletedText, void *cbArg) {

    MainWnd *wnd = (MainWnd*)cbArg;                
    if (!wnd)
        return;
    Fl_Text_Display *disp = wnd->terminal;
    Fl_Text_Buffer *buffer = wnd->tbuf;     
    Fl_Text_Buffer *sbuf = wnd->sbuf;     
    
    int len = buffer->length();
    
    if (len > MAX_TEXT_SIZE) {
        // Вычисляем, сколько нужно удалить от начала
        int excess = len - MAX_TEXT_SIZE;        
        // Удаляем излишек с начала буфера
        buffer->remove(0, excess);       
    }
}
*/
void MainWnd::clear_terminal()
{
    /*
   tbuf->text("");
   sbuf->text("");
   terminal->insert_position(tbuf->length());
   terminal->show_insert_position();
   */
  terminal->clear();
}

void MainWnd::print_terminal( uint8_t type, const char *s)
{   
    terminal->append(s);
    /*
    int size = strlen(s); 
    if ( size  <= 0 )
        return;
    
    tbuf->append(s);
    char Ch = 'A' + type; 
    char *p = new char[size +1];
    memset(p, Ch, size );
    p[size] = 0;
    if ( s[size-1] == '\n')
        p[size-1] = '\n';
    sbuf->append(p);    
    delete [] p;  
           // Опционально: прокрутка вниз к последнему тексту        
    terminal->insert_position(tbuf->length());
    terminal->show_insert_position();
    
    terminal->parent()->redraw();
    terminal->redraw();    
    Fl::flush();  // применяет обновления немедленно
    */
}
/*
int main(int argc, char **argv) {
  Fl_Window *window = new Fl_Window(Ww, Wh, "BBRobot UI");
  window->color(FL_WHITE);
  g_mainWnd = new MainWnd(0, 0, Ww, Wh);
  window->end();
  window->resizable(g_mainWnd);
  window->size_range(Ww, Wh);
  window->show(argc, argv);
  //window->size(Ww + 90, Wh);
  return Fl::run();
}
*/

//Status Bar



StatusBar::StatusBar(int X, int Y, int W, int H, int num_lines)
    : Fl_Box(X, Y, W, H), lines(num_lines) {}

void StatusBar::set_line(size_t idx, const std::string& text) {
    if (idx < lines.size()) {
        lines[idx] = text;
        redraw();
    }
}

void StatusBar::set_lines(const std::vector<std::string>& new_lines) {
    lines = new_lines;
    redraw();
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
            printf("ENTER: %s\n", fname.c_str());
        }
    }
    if (event == FL_PUSH && Fl::event_clicks()) {
        int idx = value();
        if (idx > 0) {
            std::string fname = text(idx);
            processItem(idx);
            printf("double click: %s\n", fname.c_str());
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
    printf("processItem: %s\n", fname.c_str());
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
//    printf("selectFileName: Dir: %s  File %s\n",
 //           current_dir.string().c_str(), current_br_file.empty() ? "[None]" : current_br_file.c_str());
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

void print_gui_terminal(uint8_t type, std::string &s)
{
    if ( !g_mainWnd )
    {
        printf(s.c_str());
        return;
    }
    g_mainWnd->print_terminal(type, s.c_str());
}
