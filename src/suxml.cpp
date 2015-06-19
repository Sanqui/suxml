#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <list>
#include <algorithm>
#include <memory>

#include <ncurses.h>

class XMLAttribute {
    public:
        string attribute;
        string value;
        
        string to_str() const {
            return "todo=\"todo\"";
        }
}

class XMLNode {
    public:
        bool has_children = false;
        string to_str() const {
            return "";
        }
}

class XMLContent {
    public:
        bool has_children = false;
        string content;
}

class XMLTag : public XMLNode {
    public:
        bool has_children = true;
        string element;
        vector<XMLAttribute> attributes;
        vector<XMLNode> children;
        
        string get_start_str() const {
            return "<todo>"
        }
        
        string get_end_str() const {
            return "</todo>";
        }
        
        string to_str() const {
            return "<todo />";
        }
}

class XMLDeclaration : public XMLTag {
    public:
        bool has_children = false;
        string to_str() const {
            return "<?todo?>";
        }
}

class XMLComment : public XMLNode {
    public:
        bool has_children = false;
        string comment;
        string to_str() const {
            return "<!-- todo -->";
        }
}

class XMLDocument {
    XMLDeclaration declaration = nullptr;
    XMLTag root;
    
    bool parse_stream(ifstream in) {
        
    }
}

int main() {
    initscr();
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
    
    ifstream fin ("test.xml", ios::in);
    
    
    
    endwin();
}
