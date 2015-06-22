/** \file xml.cpp
 *  Implementation of XML classes.
 *  \author David Labský <labskdav@fit.cvut.cz> */

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

#define UNREAD() in->seekg(-1, ios::cur)
#define READ_CHAR() in->read((char*)&c, 1)

#define WHITESPACE " \t\n"
#define INVALID_ELEMENT_FIRST_CHARS "-.0123456789"
#define INVALID_ELEMENT_CHARS "!\"#$%&'()*+,;<=?@[\\]^`{|}~"
#define TAB '\t'

#define DEBUG(...) printf("\x1b[33m[%3d] ", __LINE__); printf(__VA_ARGS__); printf("\x1b[39;49m")

/// Verify whether a string is only whitespace
/** \return True if string is only whitespace */
bool is_whitespace(string s) {
    for (char c : s) {
        if (!isspace(c)) return false;
    }
    return true;
}

/// Check if the string contains a specified character
/** \return True if one of the chars is present */
int any_char_in_string(string s, string chars) {
    for (char c : chars) {
        if (s.find(c) != string::npos) return s.find(c);
    }
    return -1;
}

class XMLNode;

/// A line of text in the editor
/** This is a supporting class, the purpose of which is to tie together
 *  the editor and the XML document.  It exists mainly to speed up rendering.
 */
class EditorLine {
    public:
        /// Whether the line can be selected and interacted with in the editor
        bool selectable;
        /// How much is the line indented
        int depth;
        /// The actual contents of the line
        string text;
        /// The XMLNode this line represents
        XMLNode* node;
        /// If the line should be highlighted in the editor (e.g. after a search)
        bool highlight;
        
        /// Constructor with highlight off by default
        EditorLine(bool selectable, int depth, string text, XMLNode* node)
            :selectable(selectable), depth(depth), text(text), node(node) {
            highlight = false;
        };
            
        /// Constructor with highlight settable
        EditorLine(bool selectable, int depth, string text, XMLNode* node, bool highlight)
            :selectable(selectable), depth(depth), text(text), node(node), highlight(highlight) {};
};

/// An attribute of an element
/** Only stores the attribute-value pair at the moment, but an editor supporting
 *  e.g. namespaces would want to extend this.
 */
class XMLAttribute {
    public:
        string attribute;
        string value;
        
        XMLAttribute(string attribute, string value) : attribute(attribute), value(value) {};
        
        string to_str() const {
            return attribute+"=\""+value+"\"";
        }
};

/// Abstract XML node
/** This abstract class represents a node in the XML tree.  Various operations
 *  can be performed on a node.
 */
class XMLNode {
    public:
    	/// Implicit constructor
        XMLNode() {};
    	/// Destructor
        virtual ~XMLNode() {};
        
        /// Whether the node has been visually expanded
        /** This is internal to the editor; it doesn't affect output. */
        bool expanded = false;
        /// Whether the node has been found by the last search
        /** This is internal to the editor; it doesn't affect output. */
        bool found = false;
        
        /// Whether it makes sense to expand this node
        /** In other words, whether this node has (or can have) children */
	    /** \return True if node is expandable */
        virtual bool is_expandable() { return false; }
        /// Sets a part of this node
        /** The node can have multiple parts; the first parameter specifies
         *  which part is being set.  The second parameter contains the new
         *  string.
         *
	     *  \return A bool if the set was successful, and an int containing
	     *  the location of the first error found
	     */
        virtual pair<bool, int> set(int which, string text) { return make_pair(true, -1); }
        /// Deletes a part of this node
	    /** \return True if succesful */
        virtual bool del(int which) { return false; }
        /// Inserts a new node, propragates
        /** Attempts to insert new_node into or after node */
	    /** \return True if succesful */
        virtual bool ins_node(XMLNode* node, bool force_after, XMLNode* new_node) { return false; }
        /// Deletes a node, propagates
        /** Attempts to delete node */
	    /** \return True if succesful */
        virtual bool del_node(XMLNode* node) { return false; }
        /// Finds all elements with the specified name, propagates
	    /** \return True if found and the parents should expand */
        virtual bool find(string str) {
            expanded = false;
            return false;
        }
        /// Expands all nodes, propagates
        virtual void expand_all() { expanded = true; }
        
        
        /// Gets the number of settable parts this node has.
        /** A settable part is a string which can be edited in the
         *  editor, e.g. the element name or attributes. */
	    /** \return Number of settable parts */
        virtual int num_settable() { return 1; }
        /// Gets the settable parts this node has.
        /** A settable part is a string which can be edited in the
         *  editor, e.g. the element name or attributes. */
	    /** \return Vector of strings containing the individual parts */
        virtual vector<string> settable_parts() { return vector<string>(); }
        
        /// Returns a string representation of this node, indented by depth
	    /** \return String representation of this node */
        virtual string to_str(int depth) const {
            return "";
        }
        /// Returns a string representation of this node
	    /** \return String representation of this node */
        string to_str() const {
            return to_str(0);
        }
        
        /// Gets the settable line
        /** This method returns the line of this node as it's being edited,
         *  with the part denoted by select_cursor and the current version
         *  in edit_buf.
         *
	     * \return The generated line and the cursor offset */
        virtual pair<string, int> get_settable_line(int select_cursor, string edit_buf) {
            return make_pair(edit_buf, 0);
        }
        
        /// Renders the node as EditorLines into the given vector
        virtual void render_into(vector<EditorLine>* lines, int depth) {
            lines->push_back(EditorLine(true, depth, to_str(), this));
        }
    private:
};

/// XML Content
/** Represents a piece of text between other XML nodes.
 *  For convenience while editing, multiple lines of text are split
 *  into distinct XMLContents during parsing, however this is not necessary.
 */
class XMLContent : public XMLNode {
    public:
        XMLContent(string content) : XMLNode(), content(content) {};
        string content;
        
        pair<bool, int> set(int which, string text) {
            // set the content text
            assert (which == 0); // we only have one settable thing
            // no < allowed
            int invalid = any_char_in_string(text, "<");
            if (invalid != -1) return make_pair(false, invalid);
            content = text;
            return make_pair(true, -1);
        }
        
        bool del(int which) {
            // simply delete our content
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
            // this isn't necessary, because while parsing,
            // we already split newlines into different XMLContents -
            // but just in case one sneaks in there.
            replace(s.begin(), s.end(), '\n', ' ');
            lines->push_back(EditorLine(true, depth, s, this));
        }
};

/// XML Tag
/** Represents a piece of XML tag
 *  A XML tag has attributes and children.  All of these are stored in the
 *  object and can be accessed or edited through the means of set(), etc.
 * 
 *  Example tags: &lt;br />, &lt;b>hello&lt;/b>
 */
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
        /// The attributes on the element
        vector<XMLAttribute> attributes;
        /// Child nodes of this tag
        vector<XMLNode*> children;
        
        pair<bool, int> set(int which, string text) {
            found = false;
            which--;
            if (which == -1) {
                // we're setting the element name.
                // cannot be empty
                if (text.size() == 0) return make_pair(false, -1);
                // first character cannot be invalid, there are specific chars
                // which can't be the first characters of the element name
                char c = text[0];
                for (char invalid_char : INVALID_ELEMENT_FIRST_CHARS) {
                    if (c == invalid_char) return make_pair(false, 0);
                }
                // any other character cannot be invalid either
                int invalid = any_char_in_string(text, WHITESPACE INVALID_ELEMENT_CHARS ">/");
                if (invalid != -1) return make_pair(false, invalid);
                
                element = text;
            }
            else if (which/2 < (int)attributes.size()) {
                if (which % 2 == 0) {
                    // if even, we're setting the attribute
                    if (text.size() == 0) return make_pair(false, -1);
                    int invalid = any_char_in_string(text, WHITESPACE INVALID_ELEMENT_CHARS ">/");
                    if (invalid != -1) return make_pair(false, invalid);
                    
                    attributes[which/2].attribute = text;
                }
                else {
                    // odd, so we're setting the value.  the value doesn't
                    // have any restrictions except for ", since it's quoted
                    int invalid = any_char_in_string(text, "\"");
                    if (invalid != -1) return make_pair(false, invalid);
                    
                    attributes[which/2].value = text;
                }
            } else {
                // this is a new attribute, create it with an empty value
                if (text.size() == 0) return make_pair(true, -1);
                int invalid = any_char_in_string(text, WHITESPACE INVALID_ELEMENT_CHARS ">/");
                if (invalid != -1) return make_pair(false, invalid);
                attributes.push_back(XMLAttribute(text, ""));
            }
            // we did it!
            return make_pair(true, -1);
        }
        
        bool del(int which) {
            which--;
            // nope, we can't delete the element name
            if (which == -1) return false;
            if (which/2 < (int)attributes.size()) {
                if (which % 2 == 0) {
                    // delete the attribute
                    attributes.erase(attributes.begin() + which/2);
                } else {
                    // delete the attribute's value
                    attributes[which/2].value = "";
                }
                return true;
            }
            // probably an attempt to delete a nonexistant attribute - fail
            return false;
        }
        
        bool del_node(XMLNode* node) {
            // simply delete the node if it's our child, otherwise
            // propagate into other children
            // (depth first)
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
            // there are two modes to insert nodes -- we usually want to
            // insert the new node after the selected one, unless it's
            // the start tag line
            if (node == this && !force_after) {
                // simply insert the new node if it's us
                children.insert(children.begin(), new_node);
                return true;
            }
            // depth first insertion of new_node after node
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
            // it wasn't inserted by us
            return false;
        }
        
        bool is_expandable() {
            // don't claim to be expandable if we don't have children
            // (we actually can be expanded, but it's cleaner not to
            // visualize it)
            return children.size() >= 1;
        }
        
        int num_settable() {
            // the length settable_parts() returns
            return 1 + (attributes.size()*2) + 1;
        }
        
        vector<string> settable_parts() {
            vector<string> parts = vector<string>();
            // element name
            parts.push_back(element);
            // attribute names and values
            for (XMLAttribute attr : attributes) {
                parts.push_back(attr.attribute);
                parts.push_back(attr.value);
            }
            // dummy new attribute
            parts.push_back("");
            return parts;
        }
        
        /// Gets the start tag
        /** \return The start tag string */
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
            if (!children.size() and !expanded) out += " /";
            out += ">";
            return out;
        }
        
        /// Gets the end tag
        /** \return The end tag string */
        string get_end_str() const {
            return "</" + element + ">";
        }
        
        virtual string to_str(int depth) const {
            if (!children.size() && !expanded) {
                return get_start_str();
            } else {
                string out = "";
                out += get_start_str();
                // we let the children to_str() too
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
            if (expanded) { // and children.size()
                lines->push_back(EditorLine(true, depth, get_start_str(), this, found));
                for (auto& child : children) {
                    child->render_into(lines, depth+1);
                }
                lines->push_back(EditorLine(false, depth, get_end_str(), this, found));
            } else if (children.size()) {
                // we have children which will be shown if expanded - convey
                // this with ...
                lines->push_back(EditorLine(true, depth, get_start_str()+" ...", this, found));
            } else {
                lines->push_back(EditorLine(true, depth, get_start_str(), this, found));
            }
        }
        
        pair<string, int> get_settable_line(int select_cursor, string edit_buf) {
            // the reason this looks the way it does is so we can
            // easily get the select_x value (offset from left)
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
                } else {
                    line += part;
                }
                if (i != 0 && i % 2 == 0) line += "\"";
                i++;
            }
            if (children.size() == 0 and !expanded) line += "/";
            line += ">";
            return make_pair(line, select_x);
        }
        
        bool find(string str) {
            // we're not expanded by default
            expanded = false;
            for (auto& child : children) {
                if (child->find(str)) {
                    // some of our children or grand-children (...) matched,
                    // so expand us
                    expanded = true;
                }
            }
            if (element == str) {
                // it's us!
                found = true;
                expanded = true;
                // tell (grand...)parents to expand
                return true;
            }
            if (expanded) return true; // found in a child
            return false;
        }
        
        void expand_all() {
            if (children.size()) expanded = true;
            for (auto& child : children) {
                child->expand_all();
            }
        }
};

/// XML Declaration
/** Represents the XML declaration
 *
 *  According to the specification, a XML document may contain a XML declaration
 *  at the start.  We expose this, but do not allow it to be edited
 *  (it wouldn't make sense).
 *
 *  Example declaration: &lt;?xml version="1.0" encoding="UTF-8" standalone="no" ?>
 */
class XMLDeclaration : public XMLNode {
    public:
        XMLDeclaration() : XMLNode() {};
        
        /// The attributes on the declaration
        vector<XMLAttribute> attributes;
        
        string to_str(int depth) const {
            string out = "<?xml";
            for (XMLAttribute attr : attributes) {
                out += " ";
                out += attr.attribute;
                out += "=\"";
                out += attr.value;
                out += "\"";
            }
            out += "?>";
            return out;
        }
        
        virtual void render_into(vector<EditorLine>* lines, int depth) {
            lines->push_back(EditorLine(false, depth, to_str(0), this));
        }
};

/// XML Doctype
/** Represents the XML doctype
 *
 *  There are a few restrictions on the doctype we don't validate right now...
 *
 *  Example doctype: &lt;!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
 * "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
 */
class XMLDoctype : public XMLNode {
    public:
        XMLDoctype() : XMLNode() {};
        
        /// Contents of this doctype
        string text;
        
        pair<bool, int> set(int which, string text_) {
            // we only have one settable thing
            assert (which == 0);
            int invalid = any_char_in_string(text_, ">");
            if (invalid != -1) return make_pair(false, invalid);
            text = text_;
            return make_pair(true, -1);
        }
        
        vector<string> settable_parts() {
            vector<string> parts = vector<string>();
            // only the content is settable
            string s = text;
            replace(s.begin(), s.end(), '\n', ' ');
            parts.push_back(s);
            return parts;
        }
        pair<string, int> get_settable_line(int select_cursor, string edit_buf) {
            return make_pair("<!DOCTYPE "+edit_buf+">", 10);
        }
        
        string to_str(int depth) const {
            return "<!DOCTYPE "+text+">";
        }
        
        virtual void render_into(vector<EditorLine>* lines, int depth) {
            string s = to_str(0);
            replace(s.begin(), s.end(), '\n', ' ');
            lines->push_back(EditorLine(true, depth, s, this));
        }
};

/// XML Comment
/** Represents a XML comment
 *  A XML comment can occur at any place inside content.  We expose comments
 *  and allow them to be edited.
 * 
 *  Example comment: &lt;!-- hello! -->
 */
class XMLComment : public XMLNode {
    public:
        XMLComment(string comment) : XMLNode(), comment(comment) {};
        
        /// The text of the comment
        string comment;
        
        pair<bool, int> set(int which, string text) {
            // we only have one settable thing
            assert (which == 0);
            // TODO no -- can be present
            comment = text;
            return make_pair(true, -1);
        }
        
        vector<string> settable_parts() {
            vector<string> parts = vector<string>();
            // only the comment is settable
            parts.push_back(comment);
            return parts;
        }
        pair<string, int> get_settable_line(int select_cursor, string edit_buf) {
            return make_pair("<!--"+edit_buf+"-->", 4);
        }
        
        string to_str(int depth) const {
            return string(depth, '\t')+"<!--"+comment+"-->";
        }
};

/// XML Document
/** Represents the entire XML document in memory
 *  
 */
class XMLDocument {
    public:
        /// Whether the document has a declaration
        bool have_declaration;
        /// The XML declaration, if any
        XMLDeclaration declaration;
        
        /// Whether the document has a doctype
        bool have_doctype;
        /// The XML doctype, if any
        XMLDoctype doctype;
        
        /// The root tag, which is necessary
        XMLTag root;
        
        /// Constructor
        XMLDocument() {};
        
        /// Parse the XML document inside the provided document
        /** When an error is encountered, according to the XML specification,
         *  no further attempt at parsing should be made.  XMLDocument throws
         *  an exception with an appropriate message.  Nonetheless, the partial
         *  document up to the error has been parsed and is available for
         *  inspection.
         */
        bool parse(string filename) {
            ifstream fin (filename, ios::in);
            if (!fin.is_open() || !fin.good() || !fin) throw "cannot open file";
            in = &fin;
            
            // the tag stack as we work ourselves through the tree
            vector<XMLTag*> tag_stack;
            
            // no content can be present before the root tag
            if (!is_whitespace(read_string_until("<"))) throw "content before root tag or declaration";
            READ_CHAR();
            if (c == '?') {
                // this is a declaration
                string dec_name = read_string_until(WHITESPACE "?>");
                if (c == '>') throw "invalid declaration";
                if (dec_name != "xml") throw "declaration does not start with <?xml";
                
                have_declaration = true;
                declaration.attributes = read_attributes(true);
                if (c != '?') throw "invalid declaration";
                READ_CHAR();
                if (c != '>') throw "invalid declaration";
                if (!is_whitespace(read_string_until("<"))) throw "content between declaration and doctype or root tag";
            } else {
                UNREAD();
            }
            READ_CHAR();
            if (c == '!') {
                // this might be a DOCTYPE
                string name = read_string_until(WHITESPACE);
                if (name != "DOCTYPE") throw "invalid root tag starting with !";
                have_doctype = true;
                doctype.text = read_string_until(">");
                if (!is_whitespace(read_string_until("<"))) throw "content between doctype and root tag";
            } else {
                UNREAD();
            }
            // this is the root tag
            string element_name = read_string_until(WHITESPACE ">");
            UNREAD();
            root = XMLTag(element_name);
            root.attributes = read_attributes();
            if (c != '/') {
                tag_stack.push_back(&root);
            } else {
                // the root tag was empty!
                READ_CHAR();
                if (c != '>') throw "incomplete empty root tag";
            }
            while (tag_stack.size()) {
                read_whitespace();
                if (in->eof()) throw "early eof";
                UNREAD();
                // read any content between tags
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
                // inside a tag
                READ_CHAR();
                if (c == '!') {
                    for (int i=0; i < 2; i++) {
                        READ_CHAR();
                        if (c != '-') throw "errornous tag starting with !";
                    }
                    // this is a comment
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
                    // this is an end tag
                    element_name = read_string_until(">");
                    if (element_name != tag_stack.back()->element) {
                        throw "mismatched end tag";
                    }
                    tag_stack.pop_back();
                } else {
                    // this is a regular element
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
                        // this is an empty-element tag, no need to push it
                        // down the stack
                        READ_CHAR();
                        if (c != '>') throw "characters after / in empty-element tag";
                    }
                }
            }
            read_whitespace(true);
            // there must not be anything else besides the root tag
            if (!in->eof()) throw "root tag isn't alone";
            
            // we parsed it!
            return true;
        }
        
        /// Deletes a node
        /** Attempts to delete node */
	    /** \return True if succesful */
        bool del_node(XMLNode* node) {
            // can't delete the root node...
            if (node == &root) return false;
            return root.del_node(node);
        }
        
        /// Inserts a new node
        /** Attempts to insert new_node into or after node */
	    /** \return True if succesful */
        bool ins_node(XMLNode* node, bool force_after, XMLNode* new_node) {
            // can't insert anything after the root node...
            if (node == &root && force_after) return false;
            return root.ins_node(node, force_after, new_node);
        }
        
        /// Returns a string representation of the XML document
        /** suxml has a particular idea about how whitespace is used in XML.
         *  This results in monolithic output, but doesn't preserve some
         *  user whitespace.  In most cases, this is acceptable, because
         *  consecutive whitespace doesn't convey extra meaning.
         *
	     *  \return String representation of the XML document */
        string to_str() const {
            string out = "";
            if (have_declaration) {
                out += declaration.to_str(0)+"\n";
            }
            if (have_doctype) {
                out += doctype.to_str(0)+"\n";
            }
            return out+root.to_str(0);
        }
        
        /// Renders the XML document into EditorLines
        /** This generates a representation of the document,
         *  respecting things like expanded nodes or searches, for the editor.
         */
        void render() {
            editor_lines.clear();
            if (have_declaration) {
                declaration.render_into(&editor_lines, 0);
            }
            if (have_doctype) {
                doctype.render_into(&editor_lines, 0);
            }
            root.render_into(&editor_lines, 0);
        }
        
        /// Finds and marks all elements with the specified name
        void find(string str) {
            root.find(str);
        }
        
        /// Expands all nodes
        void expand_all() {
            root.expand_all();
        }
        
        /// The last parsed line, for convenience in reporting errors
        int last_parsed_line = 0;
        
        /// The lines of the editor
        vector<EditorLine> editor_lines;
    private:
        ifstream* in;
        char c;
        
        void read_whitespace(bool eof_fine) {
            if (in->eof()) {
                if (eof_fine) return;
                throw "early eof";
            }
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
            if (in->eof()) throw "early eof";
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
        
        vector<XMLAttribute> read_attributes(bool is_declaration) {
            vector<XMLAttribute> attributes;
            while (true) {
                read_whitespace();
                if (c == '>') break;
                if (c == '/') break;
                if (is_declaration and c == '?') break;
                UNREAD();
                string name = read_string_until(WHITESPACE "=");
                if (c != '=') throw "attribute lacks value";
                read_whitespace();
                if (c != '"' and c != '\'') throw "attribute value not in quotes";
                string value = read_string_until(string(1, c));
                attributes.push_back(XMLAttribute(name, value));
            }
            return attributes;
        }
        vector<XMLAttribute> read_attributes() {
            return read_attributes(false);
        }
};
