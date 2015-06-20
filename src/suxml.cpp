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
        virtual string to_str() const {
            return "?";
        }
};

class XMLContent : public XMLNode {
    public:
        XMLContent(string content) : XMLNode(), content(content) {};
        bool has_children = false;
        string content;
        
        string to_str() const {
            return content;
        }
};

class XMLTag : public XMLNode {
    public:
        XMLTag() {};
        XMLTag(string element) : XMLNode(), element(element) {};
        bool has_children = true;
        string element;
        vector<XMLAttribute> attributes;
        vector<XMLNode*> children;
        
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
        
        string to_str() const {
            string out = get_start_str();
            if (children.size()) {
                for (auto child_p : children) {
                    out += (*child_p).to_str();
                }
                out += get_end_str();
            }
            return out;
        }
        
};

class XMLDeclaration : public XMLTag {
    public:
        XMLDeclaration() : XMLTag() {};
        bool has_children = false;
        string to_str() const {
            return "<?todo?>";
        }
};

class XMLComment : public XMLNode {
    public:
        XMLComment() : XMLNode() {};
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
                if (c == '/') {
                    element_name = read_string_until(">");
                    if (element_name != tag_stack.back()->element) {
                        throw "mismatched end tag";
                    }
                    tag_stack.pop_back();
                } else {
                    UNREAD();
                    element_name = read_string_until(WHITESPACE ">");
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
            
            
            return true;
        }
        
        string to_str() const {
            return root.to_str();
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
                if (c == '/') break;
                UNREAD();
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
    try {
        xmldoc.parse("test.xml");
    } catch (char const* message) {
        printf("Error while parsing: %s\n", message);
    }
    
    DEBUG("The root element is: %s with %d children\n", xmldoc.root.element.c_str(), xmldoc.root.children.size());
    cout << xmldoc.to_str();
    cout << "\n";
    
    //endwin();
}
