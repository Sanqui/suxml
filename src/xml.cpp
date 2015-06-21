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

int any_char_in_string(string s, string chars) {
    for (char c : chars) {
        if (s.find(c) != string::npos) return s.find(c);
    }
    return -1;
}

class XMLNode;

class EditorLine {
    public:
        bool selectable;
        int depth;
        string text;
        XMLNode* node;
        
        bool highlight;
        
        EditorLine(bool selectable, int depth, string text, XMLNode* node)
            :selectable(selectable), depth(depth), text(text), node(node) {
            highlight = false;
        };
            
        EditorLine(bool selectable, int depth, string text, XMLNode* node, bool highlight)
            :selectable(selectable), depth(depth), text(text), node(node), highlight(highlight) {};
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
        bool found = false;
        
        virtual int is_expandable() { return false; }
        virtual int num_settable() { return 1; }
        virtual pair<bool, int> set(int which, string text) { return make_pair(true, -1); }
        virtual bool del(int which) { return false; }
        virtual bool ins_node(XMLNode* node, bool force_after, XMLNode* new_node) { return false; }
        virtual bool del_node(XMLNode* node) { return false; }
        virtual bool find(string str) {
            expanded = false;
            return false;
        }
        virtual void expand_all() { expanded = true; }
        
        
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
        
        pair<bool, int> set(int which, string text) {
            assert (which == 0);
            int invalid = any_char_in_string(text, "<");
            if (invalid != -1) return make_pair(false, invalid);
            content = text;
            return make_pair(true, -1);
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
        
        pair<bool, int> set(int which, string text) {
            found = false;
            which--;
            if (which == -1) {
                if (text.size() == 0) return make_pair(false, -1);
                char c = text[0];
                for (char invalid_char : INVALID_ELEMENT_FIRST_CHARS) {
                    if (c == invalid_char) return make_pair(false, 0);
                }
                int invalid = any_char_in_string(text, WHITESPACE INVALID_ELEMENT_CHARS ">/");
                if (invalid != -1) return make_pair(false, invalid);
                element = text;
            }
            else if (which/2 < (int)attributes.size()) {
                if (which % 2 == 0) {
                    if (text.size() == 0) return make_pair(false, -1);
                    int invalid = any_char_in_string(text, WHITESPACE INVALID_ELEMENT_CHARS ">/");
                    if (invalid != -1) return make_pair(false, invalid);
                    attributes[which/2].attribute = text;
                }
                else {
                    int invalid = any_char_in_string(text, "\"");
                    if (invalid != -1) return make_pair(false, invalid);
                    attributes[which/2].value = text;
                }
            } else {
                if (text.size() == 0) return make_pair(true, -1);
                int invalid = any_char_in_string(text, WHITESPACE INVALID_ELEMENT_CHARS ">/");
                if (invalid != -1) return make_pair(false, invalid);
                attributes.push_back(XMLAttribute(text, ""));
            }
            return make_pair(true, -1);
        }
        
        bool del(int which) {
            which--;
            if (which == -1) return false;
            if (which/2 < (int)attributes.size()) {
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
        bool ins_node(XMLNode* node, bool force_after, XMLNode* new_node) {
            if (node == this && !force_after) {
                children.insert(children.begin(), new_node);
                return true;
            }
            int i = 0;
            for (auto i_node : children) {
                if (node == &*i_node) {
                    bool inserted = false;
                    if (!force_after) {
                        inserted = i_node->ins_node(node, force_after, new_node);
                    }
                    if (!inserted) {
                        children.insert(children.begin()+i+1, new_node);
                    }
                    return true;
                } else if (i_node->ins_node(node, force_after, new_node)) {
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
                lines->push_back(EditorLine(true, depth, get_start_str(), this, found));
                for (auto& child : children) {
                    child->render_into(lines, depth+1);
                }
                lines->push_back(EditorLine(false, depth, get_end_str(), this, found));
                //(*lines)[start_line].child_lines = lines->size()-start_line-1;
            } else if (children.size()) {
                lines->push_back(EditorLine(true, depth, get_start_str()+" ...", this, found));
            } else {
                lines->push_back(EditorLine(true, depth, get_start_str(), this, found));
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
            if (children.size() == 0) line += "/";
            line += ">";
            return make_pair(line, select_x);
        }
        
        bool find(string str) {
            expanded = false;
            for (auto& child : children) {
                if (child->find(str)) {
                    expanded = true;
                    //return true;
                }
            }
            if (element == str) {
                found = true;
                expanded = true;
                return true;
            }
            if (expanded) return true; // found in a child
            return false;
        }
        
        void expand_all() {
            expanded = true;
            for (auto& child : children) {
                child->expand_all();
            }
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
        
        pair<bool, int> set(int which, string text) {
            assert (which == 0);
            comment = text;
            return make_pair(true, -1);
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
                while (true) {
                    string content = read_string_until("\n<");
                    content.erase(content.find_last_not_of(WHITESPACE)+1);
                    if (content.size()) {
                        tag_stack.back()->children.push_back(new XMLContent(content));
                    }
                    if (c == '<') break;
                    read_whitespace();
                    UNREAD();
                }
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
        
        bool ins_node(XMLNode* node, bool force_after, XMLNode* new_node) {
            if (node == &root) return false;
            return root.ins_node(node, force_after, new_node);
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
        
        void find(string str) {
            root.find(str);
        }
        
        void expand_all() {
            root.expand_all();
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
