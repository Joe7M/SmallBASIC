// -*- c-file-style: "java" -*-
// $Id: MainWindow.cpp,v 1.54 2005-06-14 23:33:58 zeeb90au Exp $
// This file is part of SmallBASIC
//
// Copyright(C) 2001-2005 Chris Warren-Smith. Gawler, South Australia
// cwarrens@twpo.com.au
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
// 

#include "sys.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>

#include <fltk/Choice.h>
#include <fltk/Group.h>
#include <fltk/Item.h>
#include <fltk/MenuBar.h>
#include <fltk/TabGroup.h>
#include <fltk/TiledGroup.h>
#include <fltk/ValueInput.h>
#include <fltk/Window.h>
#include <fltk/ask.h>
#include <fltk/error.h>
#include <fltk/events.h>
#include <fltk/filename.h>
#include <fltk/run.h>

#include "MainWindow.h"
#include "EditorWindow.h"
#include "HelpWidget.h"
#include "sbapp.h"

#if defined(WIN32) 
#include <fltk/win32.h>
#include <shellapi.h>
#endif

extern "C" {
#include "fs_socket_client.h"
}

using namespace fltk;

enum ExecState {
    init_state,
    edit_state, 
    run_state, 
    modal_state,
    break_state, 
    quit_state
} runMode = init_state;

char buff[PATH_MAX];
char *startDir;
char *runfile = 0;
int px,py,pw,ph;
MainWindow* wnd;
Input* findText;

#define MIN_FONT_SIZE 11
#define MAX_FONT_SIZE 22
#define DEF_FONT_SIZE 12
#define SCAN_LABEL "-[ Update ]-"

const char* bashome = "./Bas-Home/";
const char untitled[] = "untitled.bas";
const char aboutText[] =
    "<b>About SmallBASIC...</b><br><br>"
    "Copyright (c) 2000-2005 Nicholas Christopoulos.<br><br>"
    "FLTK Version 0.9.6.3<br>"
    "Copyright (c) 2002-2005 Chris Warren-Smith.<br><br>"
    "<a href=http://smallbasic.sourceforge.net>"
    "http://smallbasic.sourceforge.net</a><br><br>"
    "SmallBASIC comes with ABSOLUTELY NO WARRANTY. "
    "This program is free software; you can use it "
    "redistribute it and/or modify it under the terms of the "
    "GNU General Public License version 2 as published by "
    "the Free Software Foundation.<br><br>"
    "<i>Press F1 for help";

// in dev_fltk.cpp
void getHomeDir(char* fileName);
bool cacheLink(dev_file_t* df, char* localFile);
void updateForm(const char* s);
void closeForm();
bool isFormActive();

//--Menu callbacks--------------------------------------------------------------

void quit_cb(Widget*, void* v) {
    if (runMode == edit_state || runMode == quit_state) {
        if (wnd->editWnd->checkSave(true)) {
            exit(0);
        }
    } else {
        switch (choice("Terminate running program?",
                       "Exit", "Break", "Cancel")) {
        case 0:
            exit(0);
        case 1:
            dev_pushkey(SB_KEY_BREAK);
            runMode = break_state;
        }
    }
}

void statusMsg(const char* msg) {
    const char* filename = wnd->editWnd->getFilename();
    wnd->fileStatus->copy_label(msg && msg[0] ? msg : 
                                filename && filename[0] ? filename : untitled);
    wnd->fileStatus->labelcolor(wnd->rowStatus->labelcolor());
    wnd->fileStatus->redraw();

#if defined(WIN32) 
     ::SetFocus(xid(Window::first()));
#endif
}

void runMsg(const char * msg) {
    wnd->runStatus->copy_label(msg && msg[0] ? msg : "");
    wnd->runStatus->redraw();
}

void busyMessage() {
    statusMsg("Selection unavailable while program is running.");
}

void showEditTab() {
    wnd->tabGroup->selected_child(wnd->editGroup);
}

void showHelpTab() {
    wnd->tabGroup->selected_child(wnd->helpGroup);
}

void showOutputTab() {
    wnd->tabGroup->selected_child(wnd->outputGroup);
}

void browseFile(const char* url) {
#if defined(WIN32) 
    ShellExecute(xid(Window::first()), "open", url, 0,0, SW_SHOWNORMAL);
#else 
    statusMsg("Launching htmlview script...");
    if (fork() == 0) {
        fclose(stderr);
        fclose(stdin);
        fclose(stdout);
        if (execlp("htmlview", "htmlview", url, NULL) == 0) {
            execlp("mozilla", "mozilla", url, NULL);
        }
        ::exit(0); // in case exec failed 
    }
#endif
}

void help_home_cb(Widget*, void* v) {
    strcpy(buff, "http://smallbasic.sf.net");
    browseFile(buff);
}

void help_contents_cb(Widget*, void* v) {
    FILE* fp;
    char buffer[1024];
    char helpFile[250];

    TextEditor *editor = wnd->editWnd->editor;
    TextBuffer* tb = editor->buffer();
    int pos = editor->insert_position();
    int start = tb->word_start(pos);
    int end = tb->word_end(pos);
    const char* selection = tb->text_range(start, end);
    int lenSelection = strlen(selection);

    snprintf(buff, sizeof(buff), "%s/help/help.idx", startDir);
    fp = fopen(buff, "r");
    strcpy(helpFile, "0_0.html"); // default to index page

    // scan for help context
    if (fp) {
        while (feof(fp) == 0) {
            if (fgets(buffer, sizeof(buffer), fp) &&
                strncasecmp(selection, buffer, lenSelection) == 0 &&
                buffer[lenSelection] == ':') {
                strcpy(helpFile, buffer+lenSelection+1);
                helpFile[strlen(helpFile)-1] = 0; // trim \n
                break;
            }
        }
        fclose(fp);
    }

    showHelpTab();
    snprintf(buff, sizeof(buff), "%s/help/", startDir);
    wnd->helpWnd->setDocHome(buff);
    strcat(buff, helpFile);
    wnd->helpWnd->loadFile(buff);

    // cleanup
    free((void*)selection);
}

void help_app_cb(Widget*, void* v) {
    const char* helpFile = dev_getenv("APP_HELP");
    if (helpFile) {
        wnd->helpWnd->loadFile(helpFile);
    } else {
        wnd->helpWnd->loadBuffer("APP_HELP env variable not found");
    }
    showHelpTab();
}

void help_readme_cb(Widget*, void* v) {
    snprintf(buff, sizeof(buff), "file:///%s/readme.html", startDir);
    wnd->helpWnd->loadFile(buff);
    showHelpTab();
}

void help_about_cb(Widget*, void* v) {
    wnd->helpWnd->loadBuffer(aboutText);
    showHelpTab();
}

void break_cb(Widget*, void* v) {
    if (runMode == run_state || runMode == modal_state) {
        runMode = break_state;
    }
}

void fullscreen_cb(Widget *w, void* v) {
    if (w->value()) {
        // store current geometry of the window
        px = wnd->x(); 
        py = wnd->y();
        pw = wnd->w(); 
        ph = wnd->h();
        wnd->fullscreen();
    } else {
        // restore geometry to the window and turn fullscreen off
        wnd->fullscreen_off(px,py,pw,ph);
    }
}

void next_tab_cb(Widget* w, void* v) {
    Widget* current = wnd->tabGroup->selected_child();
    if (runMode == edit_state) {
        // toggle between edit and help
        if (current == wnd->helpGroup) {
            wnd->tabGroup->selected_child(wnd->editGroup);
        } else if (current == wnd->outputGroup) {
            wnd->tabGroup->selected_child(wnd->editGroup);
        } else {
            wnd->tabGroup->selected_child(wnd->helpGroup);
        }
    } else {
        // toggle between edit and output
        if (current == wnd->helpGroup) {
            wnd->tabGroup->selected_child(wnd->outputGroup);
        } else if (current == wnd->outputGroup) {
            wnd->tabGroup->selected_child(wnd->editGroup);
        } else {
            wnd->tabGroup->selected_child(wnd->outputGroup);
        }
    }
}

void turbo_cb(Widget* w, void* v) {
    wnd->isTurbo = w->value();
}

void find_cb(Widget* w, void* v) {
    bool found = wnd->editWnd->findText(findText->value(), (int)v);
    findText->textcolor(found ? BLACK : RED);
    findText->redraw();
}

void goto_cb(Widget* w, void* v) {
    Input* gotoLine = (Input*)v;
    wnd->editWnd->gotoLine(atoi(gotoLine->value()));
    wnd->editWnd->take_focus();
}

void font_size_cb(Widget* w, void* v) {
    ValueInput* sizeBn = (ValueInput*)v;
    int value = (int)sizeBn->value();
    value = value > MAX_FONT_SIZE ? MAX_FONT_SIZE : 
        value < MIN_FONT_SIZE ? MIN_FONT_SIZE : value;
    wnd->out->fontSize(value);
    wnd->editWnd->fontSize(value);
    sizeBn->value(value);
}

void func_list_cb(Widget* w, void* v) {
    const char* label = wnd->funcList->item()->label();
    if (label) {
        if (strcmp(label, SCAN_LABEL) == 0) {
            wnd->funcList->clear();
            wnd->funcList->begin();
            wnd->editWnd->createFuncList();
            new Item(SCAN_LABEL);
            wnd->funcList->end();
        } else {
            wnd->editWnd->findFunc(label);
            wnd->editWnd->take_focus();
        }
    }
}

void basicMain(const char* filename) {
    wnd->editWnd->readonly(true);
    showOutputTab();
    runMode = run_state;

    runMsg("RUN");
    wnd->copy_label("SmallBASIC");

    int success = sbasic_main(filename);

    if (runMode == quit_state) {
        exit(0);
    }

    if (success == false && gsb_last_line) {
        wnd->editWnd->gotoLine(gsb_last_line);
        int len = strlen(gsb_last_errmsg);
        if (gsb_last_errmsg[len-1] == '\n') {
            gsb_last_errmsg[len-1] = 0;
        }
        closeForm(); // unhide the error
        statusMsg(gsb_last_errmsg);
        wnd->fileStatus->labelcolor(RED);
        runMsg("ERR");
    } else {
        statusMsg(wnd->editWnd->getFilename());
        runMsg(0);
    }

    wnd->editWnd->readonly(false);
    if (isFormActive() == false) {
        wnd->editWnd->take_focus();
    }
    runMode = edit_state;
}

void run_cb(Widget*, void*) {
    const char* filename = wnd->editWnd->getFilename();
    if (runMode == edit_state) {
        // inhibit autosave on run function with environment var
        const char* noSave = dev_getenv("NO_RUN_SAVE");
        if (noSave == 0 || noSave[0] != '1') {
            if (filename == 0 || filename[0] == 0) {
                getHomeDir(buff);
                strcat(buff, untitled);
                filename = buff;
                wnd->editWnd->doSaveFile(filename, false);
            } else {
                wnd->editWnd->doSaveFile(filename, true);
            }
        }
        basicMain(filename);
    } else {
        busyMessage();
    }
}

// callback for editor-plug-in plug-ins. we assume the target
// program will be changing the contents of the editor buffer
void editor_cb(Widget* w, void* v) {
    char filename[256];
    strcpy(filename, wnd->editWnd->getFilename());

    if (runMode == edit_state) {
        if (wnd->editWnd->checkSave(false) && filename[0]) {
            int pos = wnd->editWnd->position();
            int row,col,s1r,s1c,s2r,s2c;
            wnd->editWnd->getRowCol(&row, &col);
            wnd->editWnd->getSelStartRowCol(&s1r, &s1c);
            wnd->editWnd->getSelEndRowCol(&s2r, &s2c);
            sprintf(opt_command, "%s|%d|%d|%d|%d|%d|%d",
                    filename, row-1, col, s1r-1, s1c, s2r-1, s2c);
            runMode = run_state;
            runMsg("RUN");
            strcpy(buff, startDir);
            strcat(buff, (const char*)v);
            int success = sbasic_main(buff);
            showEditTab();
            runMsg(success ? " " : "ERR");
            wnd->editWnd->loadFile(filename, -1, true);
            wnd->editWnd->position(pos);
            wnd->editWnd->take_focus();
            runMode = edit_state;
            opt_command[0] = 0;
        }
    } else {
        busyMessage();
    }
}

void tool_cb(Widget* w, void* filename) {
    if (runMode == edit_state) {
        strcpy(opt_command, startDir);
        strcat(opt_command, bashome+1);
        statusMsg((const char*)filename);
        strcpy(buff, startDir);
        strcat(buff, (const char*)filename);
        basicMain(buff);
        statusMsg(wnd->editWnd->getFilename());
        opt_command[0] = 0;
    } else {
        busyMessage();
    }
}

void setSelectionCase(bool upcase) {
    TextBuffer* tb = wnd->editWnd->editor->buffer();
    char* selection = (char*)tb->selection_text();
    int len = strlen(selection);
    int start, end;

    tb->selection_position(&start, &end);
    for (int i=0; i<len; i++) {
        selection[i] = upcase ? toupper(selection[i]) : 
            tolower(selection[i]);
    }
   
    tb->replace_selection(selection);
    tb->select(start, end);
    free((void*)selection);
}

void upcase_cb(Widget* w, void* v) {
    setSelectionCase(true);
}

void downcase_cb(Widget* w, void* v) {
    setSelectionCase(false);    
}

//--EditWindow functions--------------------------------------------------------

void setRowCol(int row, int col) {
    sprintf(buff, "%d", row);
    wnd->rowStatus->copy_label(buff);
    wnd->rowStatus->redraw();
    sprintf(buff, "%d", col);
    wnd->colStatus->copy_label(buff);
    wnd->colStatus->redraw();
}

void setModified(bool dirty) {
    wnd->modStatus->label(dirty ? "MOD" : "");
    wnd->modStatus->redraw();
}

void addHistory(const char* fileName) {
    char buffer[1024];
    getHomeDir(buff);
    strcat(buff, "history.txt");

    FILE* fp = fopen(buff, "r");
    if (fp) {
        // don't add the item if it already exists
        while (feof(fp) == 0) {
            if (fgets(buffer, sizeof(buffer), fp) &&
                strncmp(fileName, buffer, strlen(fileName)-1) == 0) {
                fclose(fp);
                return;
            }
        }
        fclose(fp);
    }

    fp = fopen(buff, "a");
    if (fp) {
        fwrite(fileName, strlen(fileName), 1, fp);
        fwrite("\n", 1, 1, fp);
        fclose(fp);
    }
}

void fileChanged() {
    wnd->funcList->clear();
    wnd->funcList->begin();
    new Item(SCAN_LABEL);
    wnd->funcList->end();
}

//--Startup functions-----------------------------------------------------------

void scanPlugIns(Menu* menu) {
    dirent **files;
    FILE* file;
    char buffer[1024];
    char label[1024];

    snprintf(buff, sizeof(buff), "%s/Bas-Home", startDir);
    int numFiles = filename_list(buff, &files);
    for (int i=0; i<numFiles; i++) {
        const char* filename = (const char*)files[i]->d_name;
        int len = strlen(filename);
        if (strcasecmp(filename+len-4, ".bas") == 0) {
            strcpy(buffer, bashome);
            strcat(buffer, filename);
            file = fopen(buffer, "r");

            if (!file) {
                continue;
            }
            
            if (!fgets(buffer, 1024, file)) {
                fclose(file);
                continue;
            }
            
            bool editorTool = false;
            if (strcmp("'tool-plug-in\n", buffer) == 0) {
                editorTool = true;
            } else if (strcmp("'app-plug-in\n", buffer) != 0) {
                fclose(file);
                continue;
            }
            
            if (fgets(buffer, 1024, file) && 
                strncmp("'menu", buffer, 5) == 0) {
                int offs = 6;
                buffer[strlen(buffer)-1] = 0; // trim new-line
                while (buffer[offs] && (buffer[offs] == '\t' ||
                                        buffer[offs] == ' ')) {
                    offs++;
                }
                sprintf(label, "&Basic/%s", buffer+offs);
                strcpy(buffer, bashome+1); // use an absolute path
                strcat(buffer, filename);
                menu->add(label, 0, (Callback*)
                          (editorTool ? editor_cb : tool_cb), 
                          strdup(buffer));
            }
            fclose(file);
        }
        //this causes a stackdump
        //free(files[i]);
    }
    if (numFiles > 0) {
        free(files);
    }
}

int arg_cb(int argc, char **argv, int &i) {
    const char* s = argv[i];
    int len = strlen(s);
    if (strcasecmp(s+len-4, ".bas") == 0 && 
        access(s, 0) == 0) {
        runfile = strdup(s);
        runMode = run_state;
        i+=1;
        return 1;
    } else if (i+1 >= argc) {
        return 0;
    }

    switch (argv[i][1]) {
    case 'e':
        runfile = strdup(argv[i+1]);
        runMode = edit_state;
        i+=2;
        return 1;
    case 'r':
        runfile = strdup(argv[i+1]);
        runMode = run_state;
        i+=2;
        return 1;
    case 'm':
        opt_loadmod = 1;
        strcpy(opt_modlist, argv[i+1]);
        i+=2;
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int i=0;
    if (args(argc, argv, i, arg_cb) < argc) {
        fatal("Options are:\n"
              " -e[dit] file.bas\n"
              " -r[run] file.bas\n"
              " -m[odule]-home\n\n%s", help);
    }

    getcwd(buff, sizeof (buff));
    startDir = strdup(buff);
    sprintf(buff, "BAS_HOME=%s%s", startDir, bashome+1);
    dev_putenv(buff);

    wnd = new MainWindow(600, 400);
#if defined(WIN32) 
    HICON icon = (HICON)wnd->icon();
    if (!icon) {
        icon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(101),
                                IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR|LR_SHARED);
        if (!icon) icon = LoadIcon(NULL, IDI_APPLICATION);
    }
    wnd->icon((char*)icon);
#endif
    wnd->show(argc, argv);

    check();
    switch (runMode) {
    case run_state:
        wnd->editWnd->loadFile(runfile, -1, true);
        addHistory(runfile);
        basicMain(runfile);
        break;
    case edit_state:
        wnd->editWnd->loadFile(runfile, -1, true);
        break;
    default:
        getHomeDir(buff);
        strcat(buff, untitled);
        if (access(buff, 0) == 0) {
            // continue editing scratch buffer
            wnd->editWnd->loadFile(buff, -1, false);
        }
        statusMsg(buff);
        runMode = edit_state;
    }
    return run();
}

//--MainWindow methods----------------------------------------------------------

MainWindow::MainWindow(int w, int h) : Window(w, h, "SmallBASIC") {
    int mnuHeight = 22;
    int tbHeight = 30;
    int statusHeight = mnuHeight;
    int groupHeight = h-mnuHeight-statusHeight-3;
    int tabBegin = 0; // =mnuHeight for top position tabs
    int pageHeight = groupHeight-mnuHeight;

    isTurbo = 0;
    opt_graphics = 1;
    opt_quite = 1;
    opt_nosave = 1;
    opt_ide = IDE_NONE; // for sberr.c
    opt_command[0] = 0;
    opt_pref_width = 0;
    opt_pref_height = 0;
    opt_pref_bpp = 0;

    int len = runfile ? strlen(runfile) : 0;
    for (int i=0; i<len; i++) {
        if (runfile[i] == '\\') {
            runfile[i] = '/';
        }
    }
    
    begin();
    MenuBar* m = new MenuBar(0, 0, w, mnuHeight);
    m->add("&File/&New File",     0,        (Callback*)EditorWindow::new_cb);
    m->add("&File/&Open File...", CTRL+'o', (Callback*)EditorWindow::open_cb);
    m->add("&File/_&Insert File...",CTRL+'i', (Callback*)EditorWindow::insert_cb);
    m->add("&File/&Save File",    CTRL+'s', (Callback*)EditorWindow::save_cb);
    m->add("&File/_Save File &As...",CTRL+SHIFT+'S', (Callback*)EditorWindow::saveas_cb);
    m->add("&File/E&xit",         CTRL+'q', (Callback*)quit_cb);
    m->add("&Edit/_&Undo",        CTRL+'z', (Callback*)EditorWindow::undo_cb);
    m->add("&Edit/Cu&t",          CTRL+'x', (Callback*)EditorWindow::cut_cb);
    m->add("&Edit/&Copy",         CTRL+'c', (Callback*)EditorWindow::copy_cb);
    m->add("&Edit/_&Paste",       CTRL+'v', (Callback*)EditorWindow::paste_cb);
    m->add("&Edit/&Upcase",       CTRL+'u', (Callback*)upcase_cb);
    m->add("&Edit/_&Downcase",    CTRL+'d', (Callback*)downcase_cb);
    m->add("&Edit/&Replace...",   F2Key,    (Callback*)EditorWindow::replace_cb);
    m->add("&Edit/Replace &Again",CTRL+'t', (Callback*)EditorWindow::replace2_cb);
    m->add("&View/Toggle/&Full Screen",0,   (Callback*)fullscreen_cb)->type(Item::TOGGLE);
    m->add("&View/Toggle/&Turbo", 0,        (Callback*)turbo_cb)->type(Item::TOGGLE);
    m->add("&View/&Next Tab",     F6Key,    (Callback*)next_tab_cb);
    scanPlugIns(m);
    m->add("&Program/&Run",        F9Key,   (Callback*)run_cb);
    m->add("&Program/&Break",      CTRL+'b',(Callback*)break_cb);
    m->add("&Help/&Help Contents", F1Key,   (Callback*)help_contents_cb);
    m->add("&Help/_&Program Help", F11Key,  (Callback*)help_app_cb);
    m->add("&Help/&Release Notes", 0,       (Callback*)help_readme_cb);
    m->add("&Help/_&Home Page",    0,       (Callback*)help_home_cb);
    m->add("&Help/&About SmallBASIC",F12Key,(Callback*)help_about_cb);

    callback(quit_cb);
    shortcut(0); // remove default EscapeKey shortcut

    tabGroup = new TabGroup(0, mnuHeight, w, groupHeight);
    tabGroup->begin();

    editGroup = new Group(0, tabBegin, w, pageHeight, "Editor");
    editGroup->begin();
    editGroup->box(THIN_DOWN_BOX);

    // create the editor edit window
    editWnd = new EditorWindow(2, 3+tbHeight, w-4, pageHeight-5-tbHeight);
    m->user_data(editWnd); // the EditorWindow is callback user data (void*)
    editWnd->box(NO_BOX);
    editWnd->editor->box(NO_BOX);
    editGroup->resizable(editWnd);

    // create the editor toolbar
    Group* toolbar = new Group(2, 2, w-4, tbHeight);
    toolbar->begin();
    toolbar->box(THIN_UP_BOX);

    // find control
    findText = new Input(38, 4, 150, mnuHeight, "Find:");
    findText->align(ALIGN_LEFT|ALIGN_CLIP);
    Button* prevBn = new Button(190, 6, 18, mnuHeight-4, "@-98>;");
    Button* nextBn = new Button(210, 6, 18, mnuHeight-4, "@-92>;");
    prevBn->callback(find_cb, (void*)0);
    nextBn->callback(find_cb, (void*)1);
    findText->labelfont(HELVETICA);

    // goto-line control
    Input* gotoLine = new Input(268, 4, 40, mnuHeight, "Goto:");
    gotoLine->align(ALIGN_LEFT|ALIGN_CLIP);
    Button* gotoBn = new Button(310, 6, 18, mnuHeight-4, "@-92>;");
    gotoBn->callback(goto_cb, gotoLine);
    gotoLine->labelfont(HELVETICA);

    // sub-func jump droplist
    funcList = new Choice(339, 4, 138, mnuHeight);
    funcList->callback(func_list_cb, 0);
    funcList->labelfont(COURIER);
    funcList->begin();
    new Item();
    new Item(SCAN_LABEL);
    funcList->end();
    toolbar->resizable(funcList);

    // font-size control
    ValueInput* sizeBn = new ValueInput(542, 4, 40, mnuHeight, "Font Size:");
    sizeBn->minimum(MIN_FONT_SIZE);
    sizeBn->maximum(MAX_FONT_SIZE);
    sizeBn->value(DEF_FONT_SIZE);
    sizeBn->step(1);
    sizeBn->callback(font_size_cb, sizeBn);
    sizeBn->labelfont(HELVETICA);
    
    // close the tool-bar with a resizeable end-box
    Group* boxEnd = new Group(1000,4,0,0);
    toolbar->resizable(boxEnd);
    toolbar->end();

    editGroup->end();
    tabGroup->resizable(editGroup);

    // create the help tab
    helpGroup = new Group(0, tabBegin, w, pageHeight, "Help");
    helpGroup->box(THIN_DOWN_BOX);
    helpGroup->hide();
    helpGroup->begin();
    helpWnd = new HelpWidget(2, 2, w-4, pageHeight-4);
    helpWnd->loadBuffer(aboutText);
    helpGroup->resizable(helpWnd);
    helpGroup->end();

    // create the output tab
    outputGroup = new Group(0, tabBegin, w, pageHeight, "Output");
    outputGroup->box(THIN_DOWN_BOX);
    outputGroup->hide();
    outputGroup->begin();
    out = new AnsiWindow(2, 2, w-4, pageHeight-4, DEF_FONT_SIZE);
    outputGroup->resizable(out);
    outputGroup->end();

    tabGroup->end();
    resizable(tabGroup);

    Group* statusBar = new Group(0, h-mnuHeight+1, w, mnuHeight-2);
    statusBar->begin();
    statusBar->box(NO_BOX);
    fileStatus = new Widget(0,0, w-137, mnuHeight-2);
    modStatus = new Widget(w-136, 0, 33, mnuHeight-2);
    runStatus = new Widget(w-102, 0, 33, mnuHeight-2);
    rowStatus = new Widget(w-68, 0, 33, mnuHeight-2);
    colStatus = new Widget(w-34, 0, 33, mnuHeight-2);

    for (int n=0; n<statusBar->children(); n++) {
        Widget* w = statusBar->child(n);
        w->labelfont(HELVETICA);
        w->box(THIN_DOWN_BOX);
        w->color(color());
    }

    fileStatus->align(ALIGN_INSIDE_LEFT|ALIGN_CLIP);
    statusBar->resizable(fileStatus);
    statusBar->end();
    end();
    editWnd->take_focus();
}

bool MainWindow::isBreakExec(void) {
    return (runMode == break_state || runMode == quit_state);
}

void MainWindow::setModal(bool modal) {
    runMode = modal ? modal_state : run_state;
}

void MainWindow::setBreak() {
    runMode = break_state;
}

bool MainWindow::isModal() {
    return (runMode == modal_state);
}

bool MainWindow::isEdit() {
    return (runMode == edit_state);
}

void MainWindow::resetPen() {
    penDownX = 0;
    penDownY = 0;
    penState = 0;
}

void MainWindow::execLink(const char* file) {
    if (file == 0 || file[0] == 0) {
        return;
    }

    siteHome.empty();
    bool execFile = false;
    if (file[0] == '!' || file[0] == '|') {
        execFile = true; // exec flag passed with name
        file++;
    }

    // execute a link from the html window
    if (0 == strncasecmp(file, "http://", 7)) {
        char localFile[PATH_MAX];
        dev_file_t df;

        memset(&df, 0, sizeof(dev_file_t));
        strcpy(df.name, file);
        if (http_open(&df) == 0) {
            sprintf(buff, "Failed to open URL: %s", file);
            statusMsg(buff);
            return;
        }

        bool httpOK = cacheLink(&df, localFile);
        char* extn = strrchr(file, '.');

        if (httpOK && extn && 0 == strncasecmp(extn, ".bas", 4)) {
            // run the remote program
            wnd->editWnd->loadFile(localFile, -1, false);
            statusMsg(file);
            addHistory(file);
            basicMain(localFile);
        } else {
            // display as html
            int len = strlen(localFile);
            if (strcasecmp(localFile+len-4, ".gif") == 0 ||
                strcasecmp(localFile+len-4, ".jpeg") == 0 ||
                strcasecmp(localFile+len-4, ".jpg") == 0) {
                sprintf(buff, "<img src=%s>", localFile);
            } else {
                sprintf(buff, "file:%s", localFile);
            }
            siteHome.append(df.name, df.drv_dw[1]);
            statusMsg(siteHome.toString());
            updateForm(buff);
            showOutputTab();
        }
        return;
    }

    char* colon = strrchr(file, ':');
    if (colon && colon-1 != file) {
        file = colon+1; // clean 'file:' but not 'c:'
    }

    char* extn = strrchr(file, '.');
    if (extn && (0 == strncasecmp(extn, ".bas ", 5) ||
                 0 == strncasecmp(extn, ".sbx ", 5))) {
        strcpy(opt_command, extn+5);
        extn[4] = 0; // make args available to bas program
    }

    // if the extension is .sbx and this does not exists or is older 
    // than the matching .bas then rename to .bas and set opt_nosave 
    // to false - otherwise set execFile flag to true
    if (extn && 0 == strncasecmp(extn, ".sbx", 4)) {
        struct stat st_sbx;
        struct stat st_bas;
        bool sbxExists = (stat(file, &st_sbx) == 0);
        strcpy(extn+1, "bas"); // remains .bas unless sbx valid
        opt_nosave = 0; // create/use sbx files
        if (sbxExists) {
            if (stat(file, &st_bas) == 0 &&
                st_sbx.st_mtime > st_bas.st_mtime) {
                strcpy(extn+1, "sbx");
                // sbx exists and is newer than .bas 
                execFile = true;
            }
        }
    }
    if (access(file, 0) == 0) {
        statusMsg(file);
        if (execFile) {
            addHistory(file);
            basicMain(file);
            opt_nosave = 1;
        } else {
            wnd->editWnd->loadFile(file, -1, true);
            showEditTab();
        }
    } else {
        sprintf(buff, "Failed to open: %s", file);
        statusMsg(buff);
    }
}

int MainWindow::handle(int e) {
    if (runMode == run_state && e == KEY && !event_key_state(RightCtrlKey)) {
        int k;
        k = event_key();
        switch (k) {
        case PageUpKey:
            dev_pushkey(SB_KEY_PGUP);
            break;
        case PageDownKey:
            dev_pushkey(SB_KEY_PGDN);
            break;
        case UpKey:
            dev_pushkey(SB_KEY_UP);
            break;
        case DownKey:
            dev_pushkey(SB_KEY_DN);
            break;
        case LeftKey:
            dev_pushkey(SB_KEY_LEFT);
            break;
        case RightKey:
            dev_pushkey(SB_KEY_RIGHT);
            break;
        case BackSpaceKey: 
        case DeleteKey:
            dev_pushkey(SB_KEY_BACKSPACE);
            break;
        case ReturnKey:
            dev_pushkey(13);
            break;
        default:
            if (k>= LeftShiftKey && k<= RightAltKey) {
                break; // ignore caps+shift+ctrl+alt
            }
            dev_pushkey(k);
            break;
        }
        return 1;
    }
    return Window::handle(e);
}

//--Debug support---------------------------------------------------------------

#if defined(WIN32)
// see http://www.sysinternals.com/ntw2k/utilities.shtml
// for the free DebugView program
// an alternative debug method is to use insight.exe which
// is included with cygwin. 
#include <windows.h>
extern "C" void trace(const char *format, ...) {
    char    buf[4096],*p = buf;
    va_list args;
    
    va_start(args, format);
    p += vsnprintf(p, sizeof buf - 1, format, args);
    va_end(args);
    
    while (p > buf && isspace(p[-1])) {
        *--p = '\0';
    }
    
    *p++ = '\r';
    *p++ = '\n';
    *p   = '\0';
    OutputDebugString(buf);
}
#else
void trace(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}
#endif

//--EndOfFile-------------------------------------------------------------------
