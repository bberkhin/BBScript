#include "fltkGUI.h"
#include <fstream>
#include <cstdio>
#include "script_bind.h"
#include "mrobotimpl.h"


// Глобальные переменные

FileManager *g_mainWnd = nullptr;
SyncExchange *FileManager::gui_run_data = nullptr;
void run_programm(const std::string &input );
extern MRobot g_robot_;


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

int MyFileBrowser::handle(int event) {
    if (event == FL_KEYDOWN && Fl::event_key() == FL_Enter) {
        int idx = value();
        if (idx > 0) {
            std::string fname = text(idx);
            processItem(idx);
            printf("ENTER: %s\n", fname.c_str());
        }
        return 1;
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
        g_mainWnd->show_editor(fname);
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
    printf("selectFileName: Dir: %s  File %s\n",
            current_dir.string().c_str(), current_br_file.empty() ? "[None]" : current_br_file.c_str());
}

fs::path MyFileBrowser::getDir() const {
    return current_dir;
}

// --- FileManager ---

#define HEADER_TAB_H 25
#define BORDER_W 4
#define BORDER_H 8
#define STATUS_ZONE_W 200


Fl_Group *FileManager::CreateProgrammTab(int x, int y, int W, int H )
{
    // Programm tab
    int w_inner = W - 2*BORDER_W;
    int grp_prg_h = H - HEADER_TAB_H;
    //Fl_Group *programm_tab = new Fl_Group(0, HEADER_TAB_H , W-2, grp_prg_h, "Programm");
    Fl_Group *programm_tab = new Fl_Group(0, HEADER_TAB_H , W, grp_prg_h, "Programm");
    programm_tab->color( FL_MAGENTA );
    programm_tab->begin();
    int hZ = grp_prg_h - BUTTON_H - 2*SPACING_H - BORDER_H ;
   // Fl_Group *vertical_zone = new Fl_Group(BORDER_W, HEADER_TAB_H, w_inner, hZ );
   Fl_Group *vertical_zone = new Fl_Group(0, HEADER_TAB_H, W, hZ );
    vertical_zone->begin();
    
    //browser = new MyFileBrowser(BORDER_W, HEADER_TAB_H + BORDER_H, w_inner, hZ );
    //editor = new Fl_Multiline_Input(BORDER_W, HEADER_TAB_H + BORDER_H, w_inner, hZ ); 
    browser = new MyFileBrowser(0, HEADER_TAB_H + BORDER_H, W, hZ );
    editor = new Fl_Multiline_Input(0, HEADER_TAB_H + BORDER_H, W, hZ ); 
    vertical_zone->end();



  
    int btn_w = 110, btn_h = BUTTON_H, spacing = SPACING_H;
    int btn_top = HEADER_TAB_H + BORDER_H + hZ  + SPACING_H ;

    Fl_Group *hor_zone = new Fl_Group(BORDER_W, btn_top-SPACING_H, w_inner, btn_h+2*SPACING_H );
    hor_zone->color( FL_YELLOW );

    run_btn    = new Fl_Button(BORDER_W, btn_top, btn_w, btn_h, "Run");
    edit_btn   = new Fl_Button(BORDER_W+btn_w+spacing, btn_top, btn_w, btn_h, "Edit");  
    new_btn    = new Fl_Button(BORDER_W+2*(btn_w+spacing), btn_top, btn_w, btn_h, "New");

    save_btn   = new Fl_Button(BORDER_W+btn_w+spacing, btn_top, btn_w, btn_h, "Save");
    close_btn = new Fl_Button(BORDER_W+2*(btn_w+spacing), btn_top, btn_w, btn_h, "Close");
  
    
    Fl_Box* dummy_h = new Fl_Box(w_inner-2,0,1,1);
    hor_zone->resizable(dummy_h);

    hor_zone->end();


    programm_tab->end();
    programm_tab->resizable(vertical_zone);
    //vertical_zone->resizable(browser);
    //vertical_zone->resizable(editor);

        
    browser->callback(browser_cb, this);
    browser->update_files_list();
    browser->type(FL_HOLD_BROWSER);
    run_btn->callback(run_cb, this);
    close_btn->callback(close_cb, this);
    edit_btn->callback(edit_cb, this);
    new_btn->callback(new_cb, this);
    save_btn->callback(save_cb, this);
    setMode(modeLIST);
    return programm_tab;
}

/////////
Fl_Group *FileManager::CreatMoveTab(int x, int y, int W, int H)
{
    Fl_Group *move_tab = new Fl_Group(x, y + 30, W, H - 30, "Move");

    Fl_Box* dummy = new Fl_Box(W-1,W - 31,1,1);
    move_tab->resizable(dummy);

    // Группа для слайдеров с жёстко фиксированной позицией/размером (НЕ resizable!)
    //int grp_y = 40, grp_h = MAX_JOINTS * SL_HEIGHT + (MAX_JOINTS - 1) * SL_SPACING + 2 * SL_Y0;
    //Fl_Group *slider_group = new Fl_Group(10, grp_y, SL_WIDTH, grp_h);

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
    //slider_group->end();
    move_tab->end();
    return move_tab;
}    
//////////


FileManager::FileManager(int W, int H, SyncExchange *g_r_d, const char* L)
    :  Fl_Window(W, H, L)
{
    FileManager::gui_run_data = g_r_d;
    begin();


    int tabs_h = H - STATUS_H;

    tabs = new Fl_Tabs(0, 0, W, tabs_h);
    Fl_Group *programm_tab = CreateProgrammTab(0, HEADER_TAB_H, W - STATUS_ZONE_W, tabs_h);
    Fl_Group *move_tab = CreatMoveTab(0, 30, W - STATUS_ZONE_W ,tabs_h);
    
 // Move tab
   
    tabs->end();
   
    status_bar = new StatusBar(10, H - STATUS_H,  W-20, STATUS_H, MAX_JOINTS + 1 );
    status_bar->set_line(0, "");
    status_bar->set_line(1, "No error");
    
    feedback_bar = new StatusBar( W-STATUS_ZONE_W, HEADER_TAB_H,  STATUS_ZONE_W, tabs_h -HEADER_TAB_H , MAX_JOINTS + 1 );
    feedback_bar->set_line(0, "POS");
    feedback_bar->set_line(1, "VEL");
    feedback_bar->setColor(  FL_BLACK, FL_GREEN );
    

    resizable(tabs);
    size_range(380, 220);
    end();
}

void FileManager::setMode(Mode md)
{
    mode = md;
    if (mode == modeLIST ) {
        browser->show();
        edit_btn->show();
        new_btn->show();
        editor->hide();
        save_btn->hide();
        close_btn->hide();

    }
    else if (mode == modeEDIT ) {
        browser->hide();        
        edit_btn->hide();
        new_btn->hide();
        editor->show();
        save_btn->show();
        close_btn->show();
    }
}

void FileManager::UpdateStatus(int ln)
{
    std::string status = browser->getDir().string();
    status += " File: ";
    if ( browser->getBrowseFile().empty() )
        status += "[None]";
    else
        status += browser->getBrowseFile();
    setStatusText(ln, status.c_str());
}

void FileManager::setStatusText(int i, const char *s) {
    status_bar->set_line(i, s);
}

void FileManager::update_file_list()
{
    browser->update_files_list(nullptr);
}

/*
void FileManager::resize(int X, int Y, int W, int H)
{
    Fl_Window::resize(X, Y, W, H);
    browser->resize(10, 10, W-20, H - STATUS_H - BUTTON_H - 2*SPACING_H );
    editor->resize(10, 10, W-20, H - STATUS_H - BUTTON_H - 2*SPACING_H );

    int btn_w = 110, btn_h = BUTTON_H, spacing = 12;
    int btn_top = H - BUTTON_H - STATUS_H  - SPACING_H ;
    run_btn->resize(10, btn_top, btn_w, btn_h);
    close_btn->resize(10+btn_w+spacing, btn_top, btn_w, btn_h);
    edit_btn->resize(10+2*(btn_w+spacing), btn_top, btn_w, btn_h);
    new_btn->resize(10+3*(btn_w+spacing), btn_top, btn_w, btn_h);         
    status_bar->resize(10, H - STATUS_H,  W-20, STATUS_H );     
}
*/
void FileManager::resize(int X, int Y, int W, int H){
    Fl_Window::resize(X, Y, W, H); 
    if (tabs) tabs->size(W - STATUS_ZONE_W, H - STATUS_H);
    if (status_bar) status_bar->resize(0, H - STATUS_H, W, STATUS_H);
    //if (feedback_bar) feedback_bar->resize( W-STATUS_ZONE_W, 0,  STATUS_ZONE_W,  H - STATUS_H );
    
    redraw();
}


void FileManager::show_editor(const std::string& filename, bool is_new) {
    printf("show_editor: %s", filename.c_str() );
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
    setMode(modeEDIT);        
    selected_file = filename;
}

void FileManager::save_file() {
    if (!selected_file.empty()) {
        std::ofstream f((browser->getDir() / selected_file).string());
        if (f)
            f << editor->value();
    }
}

// Колбэки для кнопок и браузера
void FileManager::browser_cb(Fl_Widget *w, void *data) {
    FileManager *fm = static_cast<FileManager*>(data);
    MyFileBrowser* b = static_cast<MyFileBrowser*>(w);
    int idx = b->value();
    if ( idx <= 0 )
    {
        printf( "Selected UKNOWN\n" );
        return;
    }        
    std::string item = b->text(idx);                                            
    if (Fl::event_clicks()) {
        printf( "Double Click %s\n", item.c_str() );
        b->processItem(idx);
    }
    else
        b->selectFileName(item);
}

void FileManager::run_cb(Fl_Widget*, void *data) 
{        
    FileManager *fm = static_cast<FileManager*>(data);
    std::string str_data;
    if ( fm->mode == modeLIST )
    {
      std::string fname = fm->browser->getBrowseFile();
      if ( fname.empty() )
        return;
    }
    else
    {
        std::unique_lock<std::mutex> lock(gui_run_data->mtx);
        gui_run_data->str_prg = fm->editor->value();
        gui_run_data->fname.clear();
        gui_run_data->do_run = true;
        gui_run_data->cv_gui.notify_one();
    }    
}

void FileManager::edit_cb(Fl_Widget*, void *data) {        
    FileManager *fm = static_cast<FileManager*>(data);
    std::string fname = fm->browser->getBrowseFile();
    if (!fname.empty())
        fm->show_editor(fname);
}

void FileManager::hide_editor()
{
    if ( mode == modeLIST )
        return;
    editor->value("");
    setMode(modeLIST);
    selected_file.clear();
}

void FileManager::close_cb(Fl_Widget*, void *data) {
    FileManager *fm = static_cast<FileManager*>(data);
    fm->hide_editor();   
}

void FileManager::new_cb(Fl_Widget*, void *data) {
    FileManager *fm = static_cast<FileManager*>(data);
    fm->show_editor("newfile.txt", true);
}

void FileManager::save_cb(Fl_Widget*, void *data) {
    FileManager *fm = static_cast<FileManager*>(data);
    fm->save_file();
}


void FileManager::slider_cb(Fl_Widget* w, void* data) {
    int idx = (int)(intptr_t)data;    
    double pos = ((Fl_Hor_Value_Slider*)w)->value();
    mr_setpos(idx, pos);  // IMotorDriver::setTargetPosition(double position) override;
    printf("Joint #%d: %.5f\n", idx, ((Fl_Hor_Value_Slider*)w)->value());
}



void FileManager::setFeedback(const FeedbackGUIData  &map_data)
{
    int index = 1;
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
        setStatusText(index++, data.c_str() );            
        if ( fb.index >= 0 && fb.index < sliders.size() )
        {
            Fl_Hor_Value_Slider *sl = sliders.at(fb.index );
            sl->value(fb.pos);
        }
            
    }
}

// --- Status line update function ---
void UpdateStatusLine()
{
    if( g_mainWnd )
        g_mainWnd->UpdateStatus(0);
}

