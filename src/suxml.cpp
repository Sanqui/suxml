/** \file suxml.cpp
 *  UI code for suxml, the XML editor
 *  \author David Labský <labskdav@fit.cvut.cz> */
 
/** \mainpage
 *  This is a simple and opinionated interactive xml editor using ncurses.
 **/
 
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
#include <algorithm>
using namespace std;

#include <ncurses.h>

#include "banner.h"
#include "xml.cpp"

/// The help text shown at the bottom of the screen
const char* help_text[] = {
    "Q -QUIT", "W -WRITE", "RET -EDIT", "ESC -BACK",
    "DEL -DELETE", "I -INSERT", "N -NEW TAG", "/ -FIND",
    "E -EXPAND ALL", "C -COMMENT"};

/// Ask for confirmation before an operation
bool ask(const char* question) {
    move(LINES-1, 0);
    printw(string(COLS, ' ').c_str());
    move(LINES-1, 0);
    printw(" ");
    printw(question);
    printw("  Y/N ");
    int c = getch();
    if (c == 'y') return true;
    return false;
}

int main(int argc, char* argv []) {
    
    // Error out if we don't get a file
    if (argc == 1) {
        printf("usage: %s file.xml\n", argv[0]);
        return 0;
    }
    // Setup ncurses stuff
    initscr();
    clear();
    keypad(stdscr, TRUE); // to make some more keys work
    ESCDELAY = 25; // to make ESC near-instant
    
    // set a few colors we'll be using
    start_color();
    init_pair(1, COLOR_BLACK,     COLOR_WHITE);
    init_pair(2, COLOR_BLACK,     COLOR_RED);
    init_pair(3, COLOR_BLACK,     COLOR_GREEN);
    init_pair(4, COLOR_YELLOW,    COLOR_BLACK);
    init_pair(5, COLOR_BLACK,     COLOR_YELLOW);
    init_pair(6, COLOR_RED,       COLOR_BLACK);
    
    // Display the pretty suxml banner I spent like a minute on
    attrset(COLOR_PAIR(0));
    printw(SUXML_BANNER);
    
    printw("Parsing file %s...\n", argv[1]);
    // Attempt to parse the file
    XMLDocument xmldoc = XMLDocument();
    string error = "";
    try {
        xmldoc.parse(argv[1]);
    } catch (char const* message) {
        error = message;
    }
    
    xmldoc.render();
    
    // Report an error if one occurred
    if (error.length() == 0) {
        printw("File parsed successfully\n");
    } else {
        attrset(COLOR_PAIR(6));
        printw("Error while parsing:");
        attrset(COLOR_PAIR(0));
        printw(" line %d: %s\n", xmldoc.last_parsed_line, error.c_str());
        printw("\n");
        printw("Error encountered while parsing.\n");
        printw("suxml will edit the partial file.\n");
    }
    // Wait for the user
    getch();
    clear();
    
    // Window offset from the start of the document (or, the topmost line shown)
    int top = 0;
    // Currently selected line
    int cursor = 0;
    // Is select mode active
    bool select = false;
    // Is editing mode active
    bool editing = false;
    // Should we redraw immediately
    bool redraw = true;
    // The currently highlighted node
    XMLNode* highlighted;
    // The string being edited
    string edit_buf;
    // Horizontal cursor - which settable piece is being selected
    int select_cursor = 0;
    // Psition while editing a string
    int edit_col = 0;
    
    // Which piece of help text should be highlighted in green
    int highlight_help_text = -1;
    
    // expand the root for convenience
    xmldoc.root.expanded = true;
    xmldoc.render();
    
    while (true) {
        if (!redraw) {
            int command = getch();
            if (command == 'q') { // QUIT
                // ask for confirmation when quitting!
                if (ask("Really quit?")) break;
            } else if (command == 'w') { // WRITE
                if (ask("Save?")) {
                    ofstream fout (argv[1], ios::out);
                    if (!fout.is_open() || !fout.good() || !fout || fout.fail()) {
                        throw "failed to write";
                    }
                    fout << xmldoc.to_str();
                    fout.close();
                    highlight_help_text = 1;
                }
            } else if (command == '\n') { // EDIT
                if (xmldoc.editor_lines[cursor].selectable) {
                    select = true;
                    select_cursor = 0;
                }
            } else if (command == KEY_UP) {
                cursor--;
            } else if (command == KEY_DOWN) {
                cursor++;
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
            } else if (command == 'i') { // INSERT
                xmldoc.editor_lines[cursor].node->expanded = true;
                if (xmldoc.ins_node(xmldoc.editor_lines[cursor].node,
                  !xmldoc.editor_lines[cursor].selectable, new XMLContent(""))) {
                    cursor++;
                    xmldoc.render();
                }
            } else if (command == 'n') { // NEW NODE
                xmldoc.editor_lines[cursor].node->expanded = true;
                if (xmldoc.ins_node(xmldoc.editor_lines[cursor].node,
                  !xmldoc.editor_lines[cursor].selectable, new XMLTag(""))) {
                    cursor++;
                    xmldoc.render();
                }
            } else if (command == 'c') { // COMMENT
                xmldoc.editor_lines[cursor].node->expanded = true;
                if (xmldoc.ins_node(xmldoc.editor_lines[cursor].node,
                  !xmldoc.editor_lines[cursor].selectable, new XMLComment(""))) {
                    cursor++;
                    xmldoc.render();
                }
            } else if (command == '/') { // FIND
                string find_string = "";
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
                    
                    edit_buf = xmldoc.editor_lines[cursor].node->settable_parts()[select_cursor];
                    
                } else {
                    select = false;
                    editing = true;
                    edit_buf = xmldoc.editor_lines[cursor].node->settable_parts()[0];
                    edit_col = edit_buf.length();
                }
            }
            if (editing) {
                int c = -1;
                if (!skip) c = getch();
                if (c == '\n' or c == 27) { // 27 == ESC
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
            auto line_and_select_x = xmldoc.editor_lines[cursor].node->get_settable_line(select_cursor, edit_buf);
            string line = line_and_select_x.first;
            int select_x = line_and_select_x.second;
            
            attrset(COLOR_PAIR(0));
            move(cursor-top, 0);
            // show the fact that we're editing a string
            if (editing) printw("*");
            else printw(" ");
            
            // while editing, we want to make it possible to at least
            // gracefully edit lines that are too long.
            // calculate some helper variables for that
            int chars_fit = COLS - (2+xmldoc.editor_lines[cursor].depth*2);
            int extra_lines = 0;
            int overflow = line.length() - chars_fit;
            while (overflow >= 0) {
                overflow -= COLS;
                extra_lines++;
            }
            
            // erase the line and any ones that we're gonna overlap
            move(cursor-top, 2+xmldoc.editor_lines[cursor].depth*2);
            printw(string(chars_fit, ' ').c_str());
            for (int i=0; i<extra_lines; i++) {
                printw(string(COLS, ' ').c_str());
            }
            
            // print the line and the selected part over it, inverted
            move(cursor-top, 2+xmldoc.editor_lines[cursor].depth*2);
            printw(line.c_str());
            move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x);
            move(cursor-top + ((select_x - chars_fit + (COLS))/COLS),
                (2 + (xmldoc.editor_lines[cursor].depth*2) + select_x) % COLS);
            attrset(COLOR_PAIR(1));
            printw(edit_buf.c_str());
            if (error_at != -1) {
                // if there's an error, highlight it in red
                attrset(COLOR_PAIR(2));
                move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x + error_at);
                printw(string(1, edit_buf[error_at]).c_str());
                error_at = -1;
            }
            if (select) {
                // if we're selecting and the part is empty (new attribute),
                // move the cursor there, otherwise place the cursor to the
                // corner (the inverted colors are enough to denote selection)
                if (edit_buf.length() == 0) {
                    move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x);
                } else {
                    move(LINES-1, COLS-1);
                }
            } else if (editing) {
                // if we're editing, move the cursor over the current character
                move(cursor - top + ((select_x+edit_col - chars_fit + (COLS))/COLS),
                    (2 + (xmldoc.editor_lines[cursor].depth*2) + select_x + edit_col) % COLS);
            }
            attrset(COLOR_PAIR(0));
            
            // don't skip getch() next time
            skip = false;
        }
        
        // get the highlighted node, so we can tell if there's an end tag
        // and highlight it too
        highlighted = xmldoc.editor_lines[cursor].node;
        
        // keep the cursor within bounds
        if (cursor < 0) cursor = 0;
        if (cursor >= (int)xmldoc.editor_lines.size()) cursor = xmldoc.editor_lines.size()-1;
        
        // scroll the visible portion of the sceren
        // make sure the cursor is at least 1/3 from the top or bottom
        // of the screen, this makes the viewing area pleasant
        while (cursor < top+(LINES/3)) top--;
        if (top < 0) top = 0;
        while (cursor > top+(LINES/3)*2) top++;
        
        
        // render the screen
        clear();
        for (int y=0; y<LINES-1; y++) {
            int line_num = top+y;
            if (line_num < (int)xmldoc.editor_lines.size()) {
                if ((line_num == cursor or xmldoc.editor_lines[line_num].node == highlighted)
                    && xmldoc.editor_lines[cursor].selectable) {
                    // highlight the line the cursor is over
                    if (!xmldoc.editor_lines[line_num].highlight) {
                        attrset(COLOR_PAIR(1));
                    } else {
                        attrset(COLOR_PAIR(5));
                    }
                } else if (xmldoc.editor_lines[line_num].highlight) {
                    attrset(COLOR_PAIR(4));
                }
                move(y, 2 + xmldoc.editor_lines[line_num].depth*2);
                
                // calculate how many characters fit; if the line doesn't fit,
                // show an inverted $ at the endto portray it
                int chars_fit = COLS - (2 + xmldoc.editor_lines[line_num].depth*2);
                if ((int)xmldoc.editor_lines[line_num].text.size() > chars_fit) {
                    printw(xmldoc.editor_lines[line_num].text.substr(0, chars_fit-1).c_str());
                    attrset(COLOR_PAIR(1));
                    printw("$");
                    attrset(COLOR_PAIR(0));
                } else if (xmldoc.editor_lines[line_num].text.size()) {
                    printw(xmldoc.editor_lines[line_num].text.c_str());
                } else {
                    // if the line is empty, print a single space to make
                    // it possible to hover over it anyway
                    printw(" ");
                }
                if (line_num == cursor && !xmldoc.editor_lines[cursor].selectable) {
                    // if the line isn't selectable, print an inverted space at
                    // the end of it, to visualize the fact that if you
                    // insert or add a tag, it'll get put after the line
                    attrset(COLOR_PAIR(1));
                    printw(" ");
                }
                attrset(COLOR_PAIR(0));
                
                if (!xmldoc.editor_lines[line_num].node->expanded
                    && xmldoc.editor_lines[line_num].node->is_expandable()) {
                    // print an inverted + if the line can be expanded
                    move(y, 1+xmldoc.editor_lines[line_num].depth*2);
                    attrset(COLOR_PAIR(1));
                    printw("+");
                    attrset(COLOR_PAIR(0));
                }
            } else {
                // line is beyond the end of the document
                move(y, 0);
                // could show a vi-style ~ but decided not to
                //printw("~");
            }
        }
        
        // print the help text at the bottom of the screen
        move(LINES-1, 0);
        printw(" ");
        int i = 0;
        for (auto text : help_text) {
            attrset(COLOR_PAIR(1));
            // set the color to green if this help text is to be highlighted
            if (highlight_help_text == i) attrset(COLOR_PAIR(3));
            printw(" ");
            // print the help string up to -, invert colors after it
            // (this is done to save screen estate yet make storage convenient)
            for (auto c : string(text)) {
                if (c != '-') printw(string(1, c).c_str());
                else attrset(COLOR_PAIR(0));
            }
            printw(" ");
            attrset(COLOR_PAIR(0));
            i++;
        }
        highlight_help_text = -1;
        move(LINES-1, COLS-1);
        redraw = false;
    }
    
    // bye!
    endwin();
    
    return 0;
}
