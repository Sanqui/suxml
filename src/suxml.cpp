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
using namespace std;

#include <ncurses.h>

#define UNREAD() in->seekg(-1, ios::cur)
#define READ_CHAR() in->read((char*)&c, 1)
#define READ_CHAR_NO_EOF() READ_CHAR(); if (in->eof()) throw "Early EOF"
#define WHITESPACE " \t\n"

#define DEBUG(...) printf("\x1b[33m[%3d] ", __LINE__); printf(__VA_ARGS__); printf("\x1b[39;49m")

bool is_whitespace(string s) {
    for (char c : s) {
        if (!isspace(c)) return false;
    }
    return true;
}

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
        bool has_children = false;
        string to_str() const {
            return "";
        }
};

class XMLContent : public XMLNode {
    public:
        XMLContent() {};
        bool has_children = false;
        string content;
};

class XMLTag : public XMLNode {
    public:
        XMLTag() {};
        bool has_children = true;
        string element;
        vector<XMLAttribute> attributes;
        vector<XMLNode> children;
        
        string get_start_str() const {
            return "<todo>";
        }
        
        string get_end_str() const {
            return "</todo>";
        }
        
        string to_str() const {
            return "<todo />";
        }
};

class XMLDeclaration : public XMLTag {
    public:
        XMLDeclaration() {};
        bool has_children = false;
        string to_str() const {
            return "<?todo?>";
        }
};

class XMLComment : public XMLNode {
    public:
        XMLComment() {};
        bool has_children = false;
        string comment;
        string to_str() const {
            return "<!-- todo -->";
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
            
            
            
            /*
            READ_CHAR();
            DEBUG("%c\n", c);
            READ_CHAR();
            DEBUG("%c\n", c);
            READ_CHAR();
            DEBUG("%c\n", c);
            READ_CHAR();
            DEBUG("%c\n", c);*/
            
            if (!is_whitespace(read_string_until("<"))) throw "content before root tag";
            string element_name = read_string_until(WHITESPACE ">");
            UNREAD();
            if (element_name.find("!") == 0) {
                element_name = element_name.substr(1);
                // TODO
                
                //XMLDeclaration declaration = XMLDeclaration();
            } else {
                // this is the root tag
                root = XMLTag();
                root.element = element_name;
                root.attributes = read_attributes();
            }
            /*while (true) {
                READ_CHAR();
                if (c == '>') break;
                else if (c == ' ') {}
            }*/
            return true;
        }
    private:
        ifstream* in;
        char c;
        
        void read_whitespace() {
            while (true) {
                READ_CHAR();
                if (!isspace(c)) return;
            }
        }
        
        string read_string_until(string chars) {
            string result = "";
            while (true) {
                READ_CHAR();
                if (in->eof()) throw "early eof";
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
                string name = read_string_until(WHITESPACE "=");
                if (c != '=') throw "attribute lacks value";
                READ_CHAR();
                if (c != '"') throw "attribute value not in quotes";
                string value = read_string_until("\"");
                attributes.push_back(XMLAttribute(name, value));
            }
            return attributes;
        }
};

int main() {
    /*initscr();
    clear();
    
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
    getch();
    */
    XMLDocument xmldoc = XMLDocument();
    xmldoc.parse("test.xml");
    
    DEBUG("The root element is: %s\n", xmldoc.root.element.c_str());
    
    
    //endwin();
}
