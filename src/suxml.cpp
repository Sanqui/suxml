#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <algorithm>
#include <memory>
#include <typeinfo>
using namespace std;

#include <ncurses.h>

#include "banner.h"
#include "xml.cpp"


const char* help_text[] = {
    "Q -QUIT", "W -WRITE", "RET -EDIT", "ESC -BACK",
    "DEL -DELETE", "I -INSERT", "N -NEW TAG", "/ -FIND",
    "E -EXPAND ALL"};

int main(int argc, char* argv []) {
    if (argc == 1) {
        printf("usage: %s file.xml\n", argv[0]);
        return 0;
    }
    //signal(SIGINT, finish);
    initscr();
    clear();
    keypad(stdscr, TRUE);
    ESCDELAY = 25;
    start_color();
    init_pair(1, COLOR_BLACK,     COLOR_WHITE);
    init_pair(2, COLOR_BLACK,     COLOR_RED);
    init_pair(3, COLOR_BLACK,     COLOR_GREEN);
    init_pair(4, COLOR_YELLOW,    COLOR_BLACK);
    init_pair(5, COLOR_BLACK,     COLOR_YELLOW);
    
    attrset(COLOR_PAIR(0));
    
    printw(SUXML_BANNER);
    
    printw("Parsing file %s...\n", argv[1]);
    XMLDocument xmldoc = XMLDocument();
    string error = "";
    try {
        xmldoc.parse(argv[1]);
    } catch (char const* message) {
        error = message;
    }
    
    xmldoc.render();
    
    if (error.length() == 0) {
        printw("File parsed successfully\n");
    } else {
        printw("\x1b[31mError while parsing:\x1b[39;49m line %d: %s\n", xmldoc.last_parsed_line, error.c_str());
        printw("Error encountered while parsing.\n");
        printw("suxml will edit the partial file.\n");
    }
    //getch();
    clear();
    
    //if (xmldoc.root.has_attributes) return 0;
    //DEBUG("The root element is: %s with %d children\n", xmldoc.root.element.c_str(), xmldoc.root.children.size());
    //cout << xmldoc.to_str();
    //cout << "\n";
    
    int top = 0;
    int cursor = 0;
    bool select = false;
    bool editing = false;
    bool redraw = true;
    XMLNode* highlighted;
    string edit_buf;
    int select_cursor = 0;
    int edit_col = 0;
    
    int highlight_help_text = -1;
    
    string find_string = "";
    
    //int last_jump = 0;
    
    xmldoc.root.expanded = true;
    xmldoc.render();
    
    while (true) {
        if (!redraw) {
            int command = getch();
            if (command == 'q') break;
            else if (command == 'w') {
                ofstream fout (argv[1], ios::out);
                if (!fout.is_open() || !fout.good() || !fout || fout.fail()) {
                    throw "failed to write";
                }
                fout << xmldoc.to_str();
                fout.close();
                highlight_help_text = 1;
            } else if (command == '\n') {
                if (xmldoc.editor_lines[cursor].selectable) {
                    select = true;
                    select_cursor = 0;
                }
            } else if (command == KEY_UP) {
                cursor--;
                /*while (cursor >= 0
                    && !xmldoc.editor_lines[cursor].selectable) {
                    cursor--;
                }*/
            } else if (command == KEY_DOWN) {
                /*if (!xmldoc.editor_lines[cursor].expanded) {
                    cursor += xmldoc.editor_lines[cursor].child_lines;
                }*/
                cursor++;
                /*while (cursor < xmldoc.editor_lines.size()
                    && !xmldoc.editor_lines[cursor].selectable) {
                    cursor++;
                }*/
            } else if (command == KEY_RIGHT) {
                xmldoc.editor_lines[cursor].node->expanded = true;
                xmldoc.render();
            } else if (command == KEY_LEFT) {
                xmldoc.editor_lines[cursor].node->expanded = false;
                xmldoc.render();
            } else if (command == KEY_DC) { // DELETE
                if (xmldoc.del_node(xmldoc.editor_lines[cursor].node)) {
                    xmldoc.render();
                }
            } else if (command == 'i') {
                xmldoc.editor_lines[cursor].node->expanded = true;
                if (xmldoc.ins_node(xmldoc.editor_lines[cursor].node,
                  !xmldoc.editor_lines[cursor].selectable, new XMLContent(""))) {
                    cursor++;
                    xmldoc.render();
                }
            } else if (command == 'n') {
                xmldoc.editor_lines[cursor].node->expanded = true;
                if (xmldoc.ins_node(xmldoc.editor_lines[cursor].node,
                  !xmldoc.editor_lines[cursor].selectable, new XMLTag(""))) {
                    cursor++;
                    xmldoc.render();
                }
            } else if (command == '/') {
                find_string = "";
                move(LINES-1, 0);
                printw(string(COLS, ' ').c_str());
                move(LINES-1, 0);
                printw(" Search for: ");
                
                while (true) {
                    int c = getch();
                    if (c == 27) { // ESC
                        find_string = "";
                        break;
                    } else if (c == '\n') {
                        break;
                    } else if (c == '\x7f' or c == KEY_BACKSPACE) {
                        if (find_string.length() > 0) {
                            find_string.erase(find_string.end()-1);
                        }
                    } else if (isprint(c)) {
                        find_string += string(1, c);
                    }
                    move(LINES-1, 0);
                    printw(" Search for: ");
                    attrset(COLOR_PAIR(1));
                    printw((find_string).c_str());
                    attrset(COLOR_PAIR(0));
                    printw(" ");
                    move(LINES-1, 13+find_string.length());
                }
                
                if (find_string.length() > 0) {
                    xmldoc.find(find_string);
                    xmldoc.render();
                }
                
            } else if (command == 'e') {
                xmldoc.expand_all();
                xmldoc.render();
            }
        }
        int command = -1;
        bool skip=true;
        int error_at = -1;
        //if (select || edit) {
        while (select || editing) {
            if (select) {
                if (xmldoc.editor_lines[cursor].node->num_settable() > 1) {
                    // selecting...
                    if (!skip) command = getch();
                    if (command == 27 || command == KEY_UP || command == KEY_DOWN) { // esc
                        select = false;
                    } else if (command == '\n') {
                        select = false;
                        editing = true;
                        if (edit_buf == " ") edit_buf = "";
                        edit_col = edit_buf.length();
                        skip = true;
                        //edit_col = select_cursor;
                        //edit_buf = x
                    } else if (command == KEY_LEFT) {
                        select_cursor--;
                        if (select_cursor < 0) select_cursor = 0;
                    } else if (command == KEY_RIGHT) {
                        select_cursor++;
                        if (select_cursor >= xmldoc.editor_lines[cursor].node->num_settable()) {
                            select_cursor = xmldoc.editor_lines[cursor].node->num_settable()-1;
                        }
                    } else if (command == KEY_DC) { // DELETE
                        bool del = xmldoc.editor_lines[cursor].node->del(select_cursor);
                        if (del) xmldoc.render();
                    }
                    
                    //edit_buf = xmldoc.editor_lines[cursor].text;
                    edit_buf = xmldoc.editor_lines[cursor].node->settable_parts()[select_cursor];
                    
                } else {
                    select = false;
                    editing = true;
                    edit_buf = xmldoc.editor_lines[cursor].node->settable_parts()[0];
                    edit_col = edit_buf.length();
                }
            }
            if (editing) {
                //edit_col = 0;
                //edit_buf = xmldoc.editor_lines[cursor].text;
                //move(cursor-top, 2+xmldoc.editor_lines[cursor].depth*2);
                int c = -1;
                if (!skip) c = getch();
                if (c == '\n' or c == 27) { // 27 == ESC
                    //xmldoc.editor_lines[cursor].text = xmldoc.editor_lines[cursor].node->to_start_str();
                    pair<bool, int> set = xmldoc.editor_lines[cursor].node->set(select_cursor, edit_buf);
                    if (set.first) {
                        xmldoc.render();
                        editing = false;
                        if (xmldoc.editor_lines[cursor].node->num_settable() > 1) select = true;
                    } else {
                        error_at = set.second;
                    }
                } else if (c == '\x7f' or c == KEY_BACKSPACE) {
                    if (edit_col >= 1) {
                        edit_buf.erase(edit_col-1, 1);
                        edit_col -= 1;
                    } else {
                        flash();
                    } 
                } else if (c == KEY_DC) { // DELETE
                    if (edit_col < (int)edit_buf.length()) {
                        edit_buf.erase(edit_col, 1);
                    } else {
                        flash();
                    }
                } else if (c == KEY_LEFT) {
                    edit_col -= 1;
                    if (edit_col < 0) edit_col = 0;
                } else if (c == KEY_RIGHT) {
                    edit_col += 1;
                    if (edit_col > (int)edit_buf.length()) edit_col = edit_buf.length();
                } else if (isprint(c)) {
                    edit_buf.insert(edit_col, string(1, c));
                    edit_col++;
                }
            }
            // render line while selecting or editing
            //string line = "";
            //int select_x = 0;
            //string select_part = "";
            auto line_and_select_x = xmldoc.editor_lines[cursor].node->get_settable_line(select_cursor, edit_buf);
            string line = line_and_select_x.first;
            int select_x = line_and_select_x.second;
            
            attrset(COLOR_PAIR(0));
            move(cursor-top, 0);
            if (editing) printw("*");
            else printw(" ");
            
            int chars_fit = COLS - (2+xmldoc.editor_lines[cursor].depth*2);
            int extra_lines = 0;
            int overflow = line.length() - chars_fit;
            while (overflow >= 0) {
                overflow -= COLS;
                extra_lines++;
            }
            
            move(cursor-top, 2+xmldoc.editor_lines[cursor].depth*2);
            printw(string(chars_fit, ' ').c_str());
            for (int i=0; i<extra_lines; i++) {
                printw(string(COLS, ' ').c_str());
            }
            
            move(cursor-top, 2+xmldoc.editor_lines[cursor].depth*2);
            printw(line.c_str());
            move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x);
            move(cursor-top + ((select_x - chars_fit + (COLS))/COLS),
                (2 + (xmldoc.editor_lines[cursor].depth*2) + select_x) % COLS);
            attrset(COLOR_PAIR(1));
            printw(edit_buf.c_str());
            if (error_at != -1) {
                attrset(COLOR_PAIR(2));
                move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x + error_at);
                printw(string(1, edit_buf[error_at]).c_str());
                error_at = -1;
            }
            if (select) {
                if (edit_buf.length() == 0) {
                    move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x);
                } else {
                    move(LINES-1, COLS-1);
                }
            } else if (editing) {
                move(cursor - top + ((select_x+edit_col - chars_fit + (COLS))/COLS),
                    (2 + (xmldoc.editor_lines[cursor].depth*2) + select_x + edit_col) % COLS);
            }
            attrset(COLOR_PAIR(0));
            
            skip = false;
        }
        
        highlighted = xmldoc.editor_lines[cursor].node;
        
        if (cursor < 0) cursor = 0;
        if (cursor >= (int)xmldoc.editor_lines.size()) cursor = xmldoc.editor_lines.size()-1;
        
        while (cursor < top+(LINES/3)) top--;
        if (top < 0) top = 0;
        while (cursor > top+(LINES/3)*2) {
            /*if (!xmldoc.editor_lines[top].expanded) {
                top += xmldoc.editor_lines[top].child_lines;
            }*/
            top++;
        }
        
        clear();
        //int line_num = top;
        for (int y=0; y<LINES-1; y++) {
            int line_num = top+y;
            if (line_num < (int)xmldoc.editor_lines.size()) {
                if ((line_num == cursor or xmldoc.editor_lines[line_num].node == highlighted)
                    && xmldoc.editor_lines[cursor].selectable) {
                    //move(y, 1 + xmldoc.editor_lines[y].depth*2);printw("▶");
                    if (!xmldoc.editor_lines[line_num].highlight) {
                        attrset(COLOR_PAIR(1));
                    } else {
                        attrset(COLOR_PAIR(5));
                    }
                } else if (xmldoc.editor_lines[line_num].highlight) {
                    attrset(COLOR_PAIR(4));
                }
                move(y, 2 + xmldoc.editor_lines[line_num].depth*2);
                int chars_fit = COLS - (2 + xmldoc.editor_lines[line_num].depth*2);
                if ((int)xmldoc.editor_lines[line_num].text.size() > chars_fit) {
                    printw(xmldoc.editor_lines[line_num].text.substr(0, chars_fit-1).c_str());
                    attrset(COLOR_PAIR(1));
                    printw("$");
                    attrset(COLOR_PAIR(0));
                } else if (xmldoc.editor_lines[line_num].text.size()) {
                    printw(xmldoc.editor_lines[line_num].text.c_str());
                } else {
                    printw(" ");
                }
                if (line_num == cursor && !xmldoc.editor_lines[cursor].selectable) {
                    attrset(COLOR_PAIR(1));
                    printw(" ");
                }
                attrset(COLOR_PAIR(0));
                
                if (!xmldoc.editor_lines[line_num].node->expanded
                    && xmldoc.editor_lines[line_num].node->is_expandable()) {
                    move(y, 1+xmldoc.editor_lines[line_num].depth*2);
                    attrset(COLOR_PAIR(1));
                    printw("+");
                    attrset(COLOR_PAIR(0));
                }
            } else {
                move(y, 0);
                //printw("~");
            }
        }
        move(LINES-1, 0);
        printw(" ");
        int i = 0;
        for (auto text : help_text) {
            //printw(" ");
            attrset(COLOR_PAIR(1));
            if (highlight_help_text == i) attrset(COLOR_PAIR(3));
            printw(" ");
            for (auto c : string(text)) {
                if (c != '-') printw(string(1, c).c_str());
                else attrset(COLOR_PAIR(0));
            }
            printw(" ");
            attrset(COLOR_PAIR(0));
            //printw(" ");
            i++;
        }
        highlight_help_text = -1;
        move(LINES-1, COLS-1);
        redraw = false;
    }
          
    endwin();
    
    return 0;
}
