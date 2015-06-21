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

#define UNREAD() in->seekg(-1, ios::cur)
#define READ_CHAR() in->read((char*)&c, 1)

#define WHITESPACE " \t\n"
#define INVALID_ELEMENT_FIRST_CHARS "-.0123456789"
#define INVALID_ELEMENT_CHARS "!\"#$%&'()*+,;<=?@[\\]^`{|}~"
#define TAB '\t'

#define DEBUG(...) printf("\x1b[33m[%3d] ", __LINE__); printf(__VA_ARGS__); printf("\x1b[39;49m")

bool is_whitespace(string s) {
    for (char c : s) {
        if (!isspace(c)) return false;
    }
    return true;
}

class XMLNode;

class EditorLine {
    public:
        bool selectable;
        int depth;
        string text;
        XMLNode* node;
        
        EditorLine(bool selectable, int depth, string text, XMLNode* node)
            :selectable(selectable), depth(depth), text(text), node(node) {};
};

class XMLAttribute {
    public:
        string attribute;
        string value;
        
        XMLAttribute(string attribute, string value) : attribute(attribute), value(value) {};
        
        string to_str() const {
            return "todo=\"todo\"";
        }
};

class XMLNode {
    public:
        XMLNode() {};
        virtual ~XMLNode() {};
        
        bool expanded = false; // this is internal to the editor
        
        virtual int is_expandable() { return false; }
        virtual int num_settable() { return 1; }
        virtual bool set(int which, string text) { return true; }
        virtual bool del(int which) { return false; }
        virtual bool del_node(XMLNode* node) { return false; }
        
        virtual vector<string> settable_parts() { return vector<string>(); }
        
        virtual string to_str(int depth) const {
            return "";
        }
        string to_str() const {
            return to_str(0);
        }
        
        virtual pair<string, int> get_settable_line(int select_cursor, string edit_buf) {
            return make_pair(edit_buf, 0);
        }
        
        virtual string get_start_str() const { return to_str(); }
        virtual void render_into(vector<EditorLine>* lines, int depth) {
            lines->push_back(EditorLine(true, depth, to_str(), this));
        }
    private:
};

class XMLContent : public XMLNode {
    public:
        XMLContent(string content) : XMLNode(), content(content) {};
        string content;
        
        bool set(int which, string text) {
            assert (which == 0);
            content = text;
            return true;
        }
        
        bool del(int which) {
            assert (which == 0);
            content = "";
            return true;
        }
        
        string to_str(int depth) const {
            return content;
        }
        
        vector<string> settable_parts() {
            vector<string> parts = vector<string>();
            parts.push_back(content);
            return parts;
        }
        
        void render_into(vector<EditorLine>* lines, int depth) {
            string s = to_str(0);
            replace(s.begin(), s.end(), '\n', ' ');
            lines->push_back(EditorLine(true, depth, s, this));
        }
};

class XMLTag : public XMLNode {
    public:
        XMLTag() {};
        XMLTag(string element) : XMLNode(), element(element) {};
        ~XMLTag() {
            for (auto child_p : children) {
                delete child_p;
            }
        }
        
        string element;
        vector<XMLAttribute> attributes;
        vector<XMLNode*> children;
        
        bool set(int which, string text) {
            which--;
            if (which == -1) {
                if (text.size() == 0) return false;
                element = text;
            }
            else if (which/2 < attributes.size()) {
                if (which % 2 == 0) {
                    if (text.size() == 0) return false;
                    attributes[which/2].attribute = text;
                }
                else {
                    attributes[which/2].value = text;
                }
            } else {
                if (text.size() == 0) return true;
                attributes.push_back(XMLAttribute(text, ""));
            }
            return true;
        }
        
        bool del(int which) {
            which--;
            if (which == -1) return false;
            if (which/2 < attributes.size()) {
                if (which % 2 == 0) {
                    attributes.erase(attributes.begin() + which/2);
                } else {
                    attributes[which/2].value = "";
                }
                return true;
            }
            return false;
        }
        
        bool del_node(XMLNode* node) {
            int i = 0;
            for (auto i_node : children) {
                if (node == &*i_node) {
                    children.erase(children.begin() + i);
                    return true;
                } else if (i_node->del_node(node)) {
                    return true;
                }
                i++;
            }
            return false;
        }
        
        virtual int is_expandable() { return children.size() >= 1; }
        int num_settable() {
            //return settable_parts().size();
            return 1 + (attributes.size()*2) + 1;
        }
        
        vector<string> settable_parts() {
            vector<string> parts = vector<string>();
            parts.push_back(element);
            for (XMLAttribute attr : attributes) {
                parts.push_back(attr.attribute);
                parts.push_back(attr.value);
            }
            parts.push_back(""); // dummy new attribute
            return parts;
        }
        
        string get_start_str() const {
            string out = "<";
            out += element;
            for (XMLAttribute attr : attributes) {
                out += " ";
                out += attr.attribute;
                out += "=\"";
                out += attr.value;
                out += "\"";
            }
            if (!children.size()) out += " /";
            out += ">";
            return out;
        }
        
        string get_end_str() const {
            return "</" + element + ">";
        }
        
        string to_str(int depth) const {
            if (!children.size()) {
                return get_start_str();
            } else {
                string out = "";
                out += get_start_str();
                for (auto child_p : children) {
                    string child_str = (*child_p).to_str(depth+1);
                    if (!is_whitespace(child_str)) {
                        out += "\n";
                        out += string(depth+1, TAB);
                        out += child_str;
                    }
                }
                out += "\n";
                out += string(depth, TAB);
                out += get_end_str();
                return out;
            }
        }
        
        void render_into(vector<EditorLine>* lines, int depth) {
            //int start_line = lines->size();
            if (children.size() and expanded) {
                lines->push_back(EditorLine(true, depth, get_start_str(), this));
                for (auto& child : children) {
                    child->render_into(lines, depth+1);
                }
                lines->push_back(EditorLine(false, depth, get_end_str(), this));
                //(*lines)[start_line].child_lines = lines->size()-start_line-1;
            } else if (children.size()) {
                lines->push_back(EditorLine(true, depth, get_start_str()+" ...", this));
            } else {
                lines->push_back(EditorLine(true, depth, get_start_str(), this));
            }
        }
        
        pair<string, int> get_settable_line(int select_cursor, string edit_buf) {
            int i=0;
            int select_x = 0;
            string line = "";
            for (string part : settable_parts()) {
                if (i == 0) line += "<";
                else if (i % 2 == 0) line += "=\"";
                else line += " ";
                if (i == select_cursor) {
                    select_x = line.length();
                    line += edit_buf;
                    //select_part = part;
                } else {
                    line += part;
                }
                if (i != 0 && i % 2 == 0) line += "\"";
                i++;
            }
            line += ">";
            return make_pair(line, select_x);
        }
        
};

class XMLDeclaration : public XMLTag {
    public:
        XMLDeclaration() : XMLTag() {};
        
        string to_str(int depth) const {
            return "<?todo?>";
        }
};

class XMLComment : public XMLNode {
    public:
        XMLComment(string comment) : XMLNode(), comment(comment) {};
        
        string comment;
        
        bool set(int which, string text) {
            assert (which == 0);
            comment = text;
            return true;
        }
        
        vector<string> settable_parts() {
            vector<string> parts = vector<string>();
            parts.push_back(comment);
            return parts;
        }
        pair<string, int> get_settable_line(int select_cursor, string edit_buf) {
            return make_pair("<!--"+edit_buf+"-->", 4);
        }
        
        string to_str(int depth) const {
            return "<!--"+comment+"-->";
        }
};

class XMLDocument {
    public:
        XMLDeclaration declaration;
        XMLTag root;
        
        XMLDocument() {};
        
        bool parse(string filename) {
            ifstream fin (filename, ios::in);
            if (!fin.is_open() || !fin.good() || !fin) throw "cannot open file";
            in = &fin;
            
            vector<XMLTag*> tag_stack;
            
            if (!is_whitespace(read_string_until("<"))) throw "content before root tag";
            string element_name = read_string_until(WHITESPACE ">");
            UNREAD();
            if (element_name.find("!") == 0) {
                element_name = element_name.substr(1);
                // TODO
                
                //XMLDeclaration declaration = XMLDeclaration();
            } else {
                // this is the root tag
                root = XMLTag(element_name);
                root.attributes = read_attributes();
            }
            tag_stack.push_back(&root);
            while (tag_stack.size()) {
                read_whitespace();
                UNREAD();
                tag_stack.back()->children.push_back(new XMLContent(read_string_until("<")));
                READ_CHAR();
                if (c == '!') {
                    for (int i=0; i < 2; i++) {
                        READ_CHAR();
                        if (c != '-') throw "errornous tag starting with !";
                    }
                    // comment
                    string comment_text = "";
                    while (true) {
                        comment_text += read_string_until("-");
                        READ_CHAR();
                        if (c == '-') break;
                        comment_text += "-";
                        UNREAD();
                    }
                    READ_CHAR();
                    if (c != '>') throw "errornous comment, contains --";
                    tag_stack.back()->children.push_back(new XMLComment(comment_text));
                } else if (c == '/') {
                    element_name = read_string_until(">");
                    if (element_name != tag_stack.back()->element) {
                        throw "mismatched end tag";
                    }
                    tag_stack.pop_back();
                } else {
                    for (char invalid_char : INVALID_ELEMENT_FIRST_CHARS) {
                        if (c == invalid_char) throw "invalid first character of element name";
                    }
                    UNREAD();
                    element_name = read_string_until(WHITESPACE "/>" INVALID_ELEMENT_CHARS);
                    for (char invalid_char : INVALID_ELEMENT_CHARS) {
                        if (c == invalid_char) throw "invalid character in element name";
                    }
                    UNREAD();
                    XMLTag* tag_p = new XMLTag(element_name);
                    tag_p->attributes = read_attributes();
                    tag_stack.back()->children.push_back(tag_p);
                    if (c == '>') {
                        tag_stack.push_back(tag_p);
                    } else if (c == '/') {
                        READ_CHAR();
                        if (c != '>') throw "characters after / in empty-element tag";
                    }
                }
            }
            read_whitespace(true);
            if (!in->eof()) throw "root tag isn't alone";
            
            
            return true;
        }
        
        bool del_node(XMLNode* node) {
            if (node == &root) return false;
            return root.del_node(node);
        }
        
        string to_str() const {
            return root.to_str(0);
        }
        void render() {
            //root.expanded = true;
            editor_lines.clear();
            root.render_into(&editor_lines, 0);
            //editor_lines[0].child_lines = 0;
        }
        
        int last_parsed_line = 0;
        
        vector<EditorLine> editor_lines;
    private:
        ifstream* in;
        char c;
        
        void read_whitespace(bool eof_fine) {
            while (true) {
                READ_CHAR();
                if (c == '\n') last_parsed_line++;
                if (!isspace(c)) return;
                if (in->eof()) {
                    if (eof_fine) return;
                    throw "early eof";
                }
            }
        }
        
        void read_whitespace() {
            read_whitespace(false);
        }
        
        string read_string_until(string chars) {
            string result = "";
            while (true) {
                READ_CHAR();
                if (in->eof()) throw "early eof";
                if (c == '\n') last_parsed_line++;
                for (char stop_char : chars) {
                    if (c == stop_char) return result;
                }
                result += c;
            }
        }
        
        vector<XMLAttribute> read_attributes() {
            vector<XMLAttribute> attributes;
            while (true) {
                read_whitespace();
                if (c == '>') break;
                if (c == '/') break;
                UNREAD();
                string name = read_string_until(WHITESPACE "=");
                if (c != '=') throw "attribute lacks value";
                READ_CHAR();
                if (c != '"' and c != '\'') throw "attribute value not in quotes";
                string value = read_string_until(string(1, c));
                attributes.push_back(XMLAttribute(name, value));
            }
            return attributes;
        }
};

const char* help_text[] = {
    "Q - QUIT", "W - WRITE", "RETURN - EDIT", "ESC - BACK", "DEL - DELETE"};

int main(int argc, char* argv []) {
    if (argc == 1) {
        printf("usage: ./suxml file.xml\n");
        return 0;
    }
    //signal(SIGINT, finish);
    initscr();
    clear();
    keypad(stdscr, TRUE);
    ESCDELAY = 25;
    start_color();
    init_pair(1, COLOR_BLACK,     COLOR_WHITE);
    init_pair(2, COLOR_WHITE,     COLOR_BLACK);
    
    attrset(COLOR_PAIR(0));
    
    printw("\n\n\n\
                             ##                          \n\
                              #                          \n\
  ####  #   #  #   #  ## #    #                          \n\
  #     #   #   # #   # # #   #                          \n\
  ###   #   #    #    # # #   #         suxml xml editor \n\
     #  #   #   # #   # # #   #                          \n\
  ####  ####   #   #  # # #   #                          \n\
                                                         \n\
");
    
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
    
    //int last_jump = 0;
    
    xmldoc.root.expanded = true;
    xmldoc.render();
    
    while (true) {
        if (!redraw) {
            int command = getch();
            if (command == 'q') break;
            if (command == '\n') {
                if (xmldoc.editor_lines[cursor].selectable) {
                    select = true;
                    select_cursor = 0;
                }
            }
            if (command == KEY_UP) {
                cursor--;
                while (cursor >= 0
                    && !xmldoc.editor_lines[cursor].selectable) {
                    cursor--;
                }
            }
            if (command == KEY_DOWN) {
                /*if (!xmldoc.editor_lines[cursor].expanded) {
                    cursor += xmldoc.editor_lines[cursor].child_lines;
                }*/
                cursor++;
                while (cursor < xmldoc.editor_lines.size()
                    && !xmldoc.editor_lines[cursor].selectable) {
                    cursor++;
                }
            } else if (command == KEY_RIGHT) {
                xmldoc.editor_lines[cursor].node->expanded = true;
                xmldoc.render();
            } else if (command == KEY_LEFT) {
                xmldoc.editor_lines[cursor].node->expanded = false;
                xmldoc.render();
            } else if (command == KEY_DC) { // DELETE
                xmldoc.del_node(xmldoc.editor_lines[cursor].node);
                xmldoc.render();
            }
        }
        int command = -1;
        bool skip=true;
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
                    bool set = xmldoc.editor_lines[cursor].node->set(select_cursor, edit_buf);
                    if (set) {
                        xmldoc.render();
                        editing = false;
                        if (xmldoc.editor_lines[cursor].node->num_settable() > 1) select = true;
                    }
                } else if (c == '\x7f' or c == KEY_BACKSPACE) {
                    if (edit_col >= 1) {
                        edit_buf.erase(edit_col-1, 1);
                        edit_col -= 1;
                    } else {
                        flash();
                    } 
                } else if (c == KEY_DC) { // DELETE
                    if (edit_col < edit_buf.length()) {
                        edit_buf.erase(edit_col, 1);
                    } else {
                        flash();
                    }
                } else if (c == KEY_LEFT) {
                    edit_col -= 1;
                    if (edit_col < 0) edit_col = 0;
                } else if (c == KEY_RIGHT) {
                    edit_col += 1;
                    if (edit_col > edit_buf.length()) edit_col = edit_buf.length();
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
            move(cursor-top, 2+xmldoc.editor_lines[cursor].depth*2);
            printw(string(COLS - (2+xmldoc.editor_lines[cursor].depth*2), ' ').c_str());
            move(cursor-top, 2+xmldoc.editor_lines[cursor].depth*2);
            printw(line.c_str());
            move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x);
            attrset(COLOR_PAIR(1));
            printw(edit_buf.c_str());
            if (select) {
                if (edit_buf.length() == 0) {
                    move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x);
                } else {
                    move(LINES-1, COLS-1);
                }
            } else if (editing) {
                move(cursor-top, 2 + (xmldoc.editor_lines[cursor].depth*2) + select_x + edit_col);
            }
            attrset(COLOR_PAIR(0));
            
            skip = false;
        }
        
        highlighted = xmldoc.editor_lines[cursor].node;
        
        if (cursor < 0) cursor = 0;
        if (cursor >= xmldoc.editor_lines.size()) cursor = xmldoc.editor_lines.size()-1;
        
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
            if (line_num < xmldoc.editor_lines.size()) {
                if (line_num == cursor or xmldoc.editor_lines[line_num].node == highlighted) {
                    //move(y, 1 + xmldoc.editor_lines[y].depth*2);
                    //printw("▶");
                    attrset(COLOR_PAIR(1));
                }
                move(y, 2 + xmldoc.editor_lines[line_num].depth*2);
                if (xmldoc.editor_lines[line_num].text.size()) {
                    printw(xmldoc.editor_lines[line_num].text.c_str());
                } else {
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
        printw("  ");
        for (auto text : help_text) {
            printw(" ");
            attrset(COLOR_PAIR(1));
            printw(" ");
            printw(text);
            printw(" ");
            attrset(COLOR_PAIR(0));
            printw(" ");
        }
        move(LINES-1, COLS-1);
        redraw = false;
    }
          
    endwin();
    
    return 0;
}
