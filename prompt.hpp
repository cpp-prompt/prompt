#ifndef __PROMPT_H
#define __PROMPT_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>

#include <algorithm>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <memory>
#include <list>
#include <vector>
#include <cstring>
#include <fstream>
#include <string_view>
#include <experimental/filesystem>
#include <cassert>


namespace std {

namespace filesystem = experimental::filesystem;

};

namespace prompt {



// Procedure: read_line
// Read one line from an input stream
template <typename C, typename T, typename A>
std::basic_istream<C, T>& read_line(
  std::basic_istream<C, T>& is, 
  std::basic_string<C, T, A>& line
) {

  line.clear();

  typename std::basic_istream<C, T>::sentry se(is, true);        
  std::streambuf* sb = is.rdbuf();

  for(;;) {
    switch (int c = sb->sbumpc(); c) {

      // case 1: newline
      case '\n':
        return is;
      break;

      // case 2: carriage return
      case '\r':
        if(sb->sgetc() == '\n'){
          sb->sbumpc();
        }
        return is;
      break;
      
      // case 3: eof
      case std::streambuf::traits_type::eof():
        // Also handle the case when the last line has no line ending
        if(line.empty()) {
          is.setstate(std::ios::eofbit | std::ios::failbit);
        }
        return is;
      break;

      default:
        line.push_back(static_cast<char>(c));
      break;
    }
  } 

  return is;
}

// ------------------------------------------------------------------------------------------------


inline size_t count_prefix_match(std::string_view s1, std::string_view s2){
  size_t i {0};
  size_t len {std::min(s1.size(), s2.size())};
  for( ;i<len; ++i){
    if(s1[i] != s2[i]){
      break;
    }
  }
  return i;
}


// Class: RadixTree
class RadixTree{

  public:
  
  struct Node {
    bool is_word {false};
    std::list<std::pair<std::string, std::unique_ptr<Node>>> children;
  };

   RadixTree() = default;
   RadixTree(const std::vector<std::string>&);
   
   bool exist(const std::string&) const;
   void insert(const std::string&);
  
   std::vector<std::string> match_prefix(const std::string&) const;
   std::vector<std::string> all_words() const;
   std::string dump();

   // TODO: unittest:  check for each node no children can have the same prefix...
   const Node& root() const;

  private:

   Node _root;
   void _insert(std::string_view, Node*);

   void _match_prefix(std::vector<std::string>&, const Node&, const std::string&) const;
  
   std::pair<const Node*, std::string> _search_prefix_node(std::string_view) const;

   void _dump(Node*, size_t, std::string&);
};


// Procedure: Ctor
inline RadixTree::RadixTree(const std::vector<std::string>& words){
  for(const auto& w: words){
    insert(w);
  }
}


// Procedure: dump 
// Dump the radix tree into a string
inline std::string RadixTree::dump() {
  std::string t;
  _dump(&_root, 0, t);      
  t.append(1, '\n');
  return t;
}

// Procedure: _dump 
// Recursively traverse the tree and append each level to string
inline void RadixTree::_dump(Node* n, size_t level, std::string& s) {
  for(auto &[k,v]: n->children){
    assert(v.get() != nullptr);
    s.append(level, '-');
    s.append(1, ' ');
    s += k;
    s.append(1, '\n');
    _dump(v.get(), level+1, s);
  }
}

// Procedure: _match_prefix 
// Find all words that match the given prefix
inline void RadixTree::_match_prefix(
  std::vector<std::string> &vec, 
  const Node& n, 
  const std::string& s
) const {

  if(n.is_word){
    vec.emplace_back(s);
  }
  for(auto& [k,v]:n.children){
    _match_prefix(vec, *v, s+k);
  }
}

// Procedure: all_words 
// Extract all words in the radix tree
inline std::vector<std::string> RadixTree::all_words() const {
  std::vector<std::string> words;
  for(auto &[k, v]: _root.children){
    _match_prefix(words, *v, k);
  }
  return words;
}

// Procedure: match_prefix 
// Collect all words that match the given prefix
inline std::vector<std::string> RadixTree::match_prefix(const std::string& prefix) const {
  if(auto [prefix_node, suffix] = _search_prefix_node(prefix); prefix_node == nullptr){
    return {};
  }
  else{
    std::vector<std::string> matches;
    for(auto &[k, v]: prefix_node->children){
      _match_prefix(matches, *v, prefix+suffix+k);
    }
    return matches;
  }
}

// Procedure: _search_prefix_node 
// Find the node that matches the given prefix
std::pair<const RadixTree::Node*,std::string> RadixTree::_search_prefix_node(std::string_view s) const {
  Node const *n = &_root;
  std::string suffix {""};
  for(size_t pos=0; pos<s.size(); ){ // Search until full match
    bool match = {false};
    for(const auto& [k, v]: n->children){
      if(auto num = count_prefix_match(k, s.data()+pos); num>0){
        pos += num;
        if(pos == s.size()){
          suffix = k.substr(num, k.size()-num);
        }
        n = v.get();
        match = true;
        break;
      }
    }
    if(not match){
      return {nullptr, suffix};
    }
  }
  return {n, suffix};
}

// Procedure: exist 
// Check the given word in the radix tree or not
bool RadixTree::exist(const std::string& s) const {
  size_t pos {0};
  Node const *n = &_root;
  while(not n->children.empty()){ // Search until reaching the leaf
    bool match {false};
    for(const auto& [k, v]: n->children){
      auto match_num = count_prefix_match(k, s.data()+pos);
      if(match_num > 0){
        if(pos += match_num; pos == s.size()){
           if(match_num == k.size() and v->is_word){
             return true;
           }
        }
        else{
          n = v.get();
          match = true;
        }
        break;
      }
    }
    if(not match){
      return false;
    }
  }
  return false;
}

// Procedure: insert 
// Insert a word into radix tree
inline void RadixTree::insert(const std::string& s){
  if(s.empty()){  // Empty string not allowed
    return;
  }
  _insert(s, &_root);
}


// Procedure: _insert 
// Insert a word into radix tree
inline void RadixTree::_insert(std::string_view sv, Node* n){
  size_t match_num {0};
  auto ch = n->children.begin();
  for(auto &kv: n->children){
    if(match_num = count_prefix_match(std::get<0>(kv), sv); match_num > 0){
      break;
    }
    ch ++;
  }

  if(match_num == 0){   // Base case 1 
    if(auto& child = std::get<1>(n->children.emplace_back(std::make_pair(sv, new Node))); 
       sv.size() > 0){
      child->is_word = true;
    }
  }
  else if(match_num == ch->first.size()){  
    // Keep searching along the child 
    if(match_num != sv.size()){
      _insert(sv.data()+match_num, ch->second.get());
    }
    else{
      ch->second->is_word = true;
    }
  }
  else{  // Base case 2
    // Split the child node into new nodes  
    auto& par = std::get<1>(
        n->children.emplace_back(std::make_pair(std::string_view(sv.data(),match_num), new Node))
    );

    if(match_num != ch->first.size()){
      par->children.emplace_back(std::make_pair(ch->first.data()+match_num, std::move(ch->second)));
    }
    else{
      par->is_word = true;
    }

    if(match_num != sv.size()){
      auto& rc = std::get<1>(
          par->children.emplace_back(std::make_pair(sv.data()+match_num, new Node))
      );
      rc->is_word = true;
    }
    else{
      par->is_word = true;
    }

    n->children.erase(ch);
  }
}


// Procedure: root
// Return the root of RadixTree
inline const RadixTree::Node& RadixTree::root() const{
  return _root;
}

// ------------------------------------------------------------------------------------------------


// http://www.physics.udel.edu/~watson/scen103/ascii.html
enum class KEY{
  KEY_NULL = 0,       /* NULL      */
  CTRL_A   = 1,       /* Ctrl+a    */
  CTRL_B   = 2,       /* Ctrl-b    */
  CTRL_C   = 3,       /* Ctrl-c    */
  CTRL_D   = 4,       /* Ctrl-d    */
  CTRL_E   = 5,       /* Ctrl-e    */
  CTRL_F   = 6,       /* Ctrl-f    */
  CTRL_H   = 8,       /* Ctrl-h    */
  TAB      = 9,       /* Tab       */
  CTRL_K   = 11,      /* Ctrl+k    */
  CTRL_L   = 12,      /* Ctrl+l    */
  ENTER    = 13,      /* Enter     */
  CTRL_N   = 14,      /* Ctrl-n    */
  CTRL_P   = 16,      /* Ctrl-p    */
  CTRL_T   = 20,      /* Ctrl-t    */
  CTRL_U   = 21,      /* Ctrl+u    */
  CTRL_W   = 23,      /* Ctrl+w    */
  ESC      = 27,      /* Escape    */
  BACKSPACE =  127    /* Backspace */
};

enum class COLOR{
  BLACK = 30,
  RED,
  GREEN,
  YELLOW,
  BLUE,
  MAGNETA,
  CYAN,
  WHITE
};



// TODO: make Prompt templated at Tree
class Prompt {

  struct LineInfo{
    LineInfo(size_t);
    
    std::string buf;
    size_t history_trace {0};
    size_t cur_pos {0};   // cursor position
    size_t columns {80};  // default width of terminal is 80

    void operator = (const LineInfo&);
    inline void reset(){ 
      cur_pos = history_trace = 0; 
      buf.clear();
    }
  };

  public:

    Prompt(
      const std::string& ="> ", 
      std::istream& = std::cin, 
      std::ostream& = std::cout, 
      std::ostream& = std::cerr,
      int = STDIN_FILENO
    );

    ~Prompt();

    bool readline(std::string&);

    void load_history(const std::filesystem::path&);
    void save_history(const std::filesystem::path&);
    void add_history(const std::string&);
    void set_history_size(size_t);

    size_t history_size() const { return _history.size(); };
    
    void autocomplete(const std::string&);

    const std::string CR {"\r"};       // Carriage Return (set cursor to left)
    const std::string EL {"\x1b[0K"};   // Erase in Line (clear from cursor to the end of the line)

  private: 
  
    std::string _prompt;  
    std::istream& _cin;
    std::ostream& _cout;
    std::ostream& _cerr;

    int _infd;

    RadixTree _tree;

    bool _unsupported_term();
    void _stdin_not_tty(std::string &);
    bool _set_raw_mode();
    void _disable_raw_mode();

    // History 
    size_t _max_history_size {100};
    std::list<std::string> _history;

    // Unsupported terminals 
    const std::vector<std::string> _unsupported_terms {"dumb", "cons25", "emacs"};

    termios _orig_termios;
    bool _has_orig_termios {false};
    bool _save_orig_termios(int);

    int _get_cursor_pos();
    void _clear_screen();
    size_t _terminal_columns();

    void _edit_line(std::string&);

    void _refresh_single_line(LineInfo&);

    LineInfo _line {_terminal_columns()};
    LineInfo _line_save {_terminal_columns()};

    int _autocomplete_command();

    void _autocomplete_folder();

    std::vector<std::string> _files_in_folder(const std::filesystem::path&) const;
    std::vector<std::string> _files_match_prefix(const std::filesystem::path&) const;
    std::string _dump_files(const std::vector<std::string>&, const std::filesystem::path&);
    std::string _next_prefix(const std::vector<std::string>&, const size_t);


    std::filesystem::path _user_home() const;
    bool _has_read_access(const std::filesystem::path&) const;

    // Key handling subroutine
    void _key_backspace(LineInfo&);
    void _key_delete(LineInfo&);
    void _key_delete_prev_word(LineInfo&);

    void _key_prev_history(LineInfo&);
    void _key_next_history(LineInfo&);
    void _key_history(LineInfo&, bool);
    bool _key_handle_CSI(LineInfo&);

    bool _append_character(LineInfo&, char);
};

inline Prompt::LineInfo::LineInfo(size_t cols): columns(cols){
}

// Copy assignment
inline void Prompt::LineInfo::operator = (const LineInfo& l){
  buf = l.buf;
  cur_pos = l.cur_pos;
  columns = l.columns; 
  history_trace = l.history_trace;   
}


// Procedure: Ctor
inline Prompt::Prompt(
    const std::string &p, 
    std::istream& in, 
    std::ostream& out, 
    std::ostream& err, int infd): 
  _prompt(p), 
  _cin(in),
  _cout(out),
  _cerr(err),
  _infd(infd)
{}

// Procedure: Dtor
inline Prompt::~Prompt(){
  // Restore the original mode if has kept
  if(_has_orig_termios){
    ::tcsetattr(_infd, TCSAFLUSH, &_orig_termios);
  }
}

// Procedure: autocomplete
// This function adds the word into radix tree
inline void Prompt::autocomplete(const std::string& word){
  _tree.insert(word);
}

// Procedure: history_size 
// Change the max history size
inline void Prompt::set_history_size(size_t new_size){
  _max_history_size = new_size;
}

// Procedure: load_history 
// Load history commands from a file
inline void Prompt::load_history(const std::filesystem::path& p){
  if(std::filesystem::exists(p)){
    std::ifstream ifs(p);
    std::string placeholder;
    while(std::getline(ifs, placeholder)){
      _history.emplace_back(std::move(placeholder));
    }
  }
}

// Procedure: save_history 
// Save history commands to a file
inline void Prompt::save_history(const std::filesystem::path& p){
  std::ofstream ofs(p);
  for(const auto& c: _history){
    ofs << c << '\n';
  }
  ofs.close();
}

// Procedure: add_history 
// Add command to history list
inline void Prompt::add_history(const std::string &hist){
  if(_history.size() == _max_history_size){
    _history.pop_front();
  }
  _history.emplace_back(hist);
}

// Procedure: _stdin_not_tty
// Store input in a string if stdin is not from tty (from pipe or redirected file)
inline void Prompt::_stdin_not_tty(std::string& s){
  read_line(_cin, s);  // Read until newline, CR or EOF.
}

// Procedure: _unsupported_term
// Check the terminal is supported or not
inline bool Prompt::_unsupported_term(){
  if(char* term = getenv("TERM"); term == NULL){
  }
  else if(std::string_view str(term); 
          std::find(_unsupported_terms.begin(), 
                    _unsupported_terms.end()  , str) != _unsupported_terms.end()){
    return true;
  }
  return false;
}

// Procedure: readline 
// This is the main entry of Prompt
inline bool Prompt::readline(std::string& s) {
  if(not ::isatty(_infd)) {
    // not a tty, either from file or pipe; we don't limit the line size
    _stdin_not_tty(s);
    return _cin.eof() ? false : true;
  }
  else if(_unsupported_term()){
    read_line(_cin, s);
    return _cin.eof() ? false : true;
  }
  else{
    if(not _set_raw_mode()){
      return {};
    } 
    _edit_line(s);
    _disable_raw_mode();
    std::cout << '\n';
    return true;
  }
}

// Procedure: _save_orig_termios
// Save the original termios for recovering before exit
inline bool Prompt::_save_orig_termios(int fd){
  if(not _has_orig_termios){
    if(::tcgetattr(fd, &_orig_termios) == -1){
      return false;
    }
    _has_orig_termios = true;
  }
  return true;
}


// Procedure: _get_cursor_pos
// Return the current position of cursor
inline int Prompt::_get_cursor_pos(){
  /* Report cursor location */  
  // x1b[6n : DSR - Device Status Report  https://en.wikipedia.org/wiki/ANSI_escape_code 
  // The output will be like : ESC[n;mR, where "n" is the row and "m" is the column.
  // Try this in ur terminal : echo -en "abc \x1b[6n"
  if(not (_cout << "\x1b[6n")){
    return -1;
  }

  char buf[32];
  int cols, rows;

  /* Read the response: ESC [ rows ; cols R */
  for(size_t i=0; i<sizeof(buf)-1; i++){
    if(not(_cin >> buf[i]) or buf[i] == 'R'){
      buf[i] = '\0';
      break;
    }
  }

  /* Parse it. */
  if (static_cast<KEY>(buf[0]) != KEY::ESC or 
      buf[1] != '[' or 
      ::sscanf(buf+2, "%d;%d", &rows, &cols) != 2){
    return -1;
  }
  return cols;
}



// Procedure: _clear_screen (ctrl + l) 
inline void Prompt::_clear_screen() {
  // https://en.wikipedia.org/wiki/ANSI_escape_code 
  // CSI n ;  m H : CUP - Cursor Position
  // CSI n J      : Erase in Display
  //if(_cout.write("\x1b[H\x1b[2J",7); !_cout.good()){
  if(not (_cout << "\x1b[H\x1b[2J")){
    /* nothing to do, just to avoid warning. */
  }
}

// Procedure: _set_raw_mode 
// Set the fd to raw mode
inline bool Prompt::_set_raw_mode(){
  if(::isatty(_infd) and _save_orig_termios(_infd) == true){
    struct termios raw;  
    raw = _orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer.
     * We want read to return every single byte, without timeout. */
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

    /* put terminal in raw mode after flushing */
    if (::tcsetattr(_infd, TCSAFLUSH, &raw) >= 0){
      return true;
    }
  }
  errno = ENOTTY;
  return false;
}


// Procedure: _disable_raw_mode  
// Recover the termios to the original mode
inline void Prompt::_disable_raw_mode(){
  if(_has_orig_termios){
    ::tcsetattr(_infd, TCSAFLUSH, &_orig_termios);
  }
}


// Procedure: _terminal_columns
// Get the number of columns of current line buf
inline size_t Prompt::_terminal_columns(){
  // Use ioctl to get Window size 
  if(winsize ws; ::ioctl(1, TIOCGWINSZ, &ws) == -1 or ws.ws_col == 0){
    int start = _get_cursor_pos();
    if(start == -1 or not (_cout << "\x1b[999c")){
      return 80;
    }
    int cols = _get_cursor_pos();
    if(cols == -1){
      return 80;
    }
    if(cols > start){
      char seq[32];
      // Move cursor back
      ::snprintf(seq, 32, "\x1b[%dD", cols-start);
      if(_cout.write(seq, strlen(seq)); not _cout.good()){
        /* Fail to move cursor back*/
      }
    }
    return cols;
  }
  else{
    return ws.ws_col;
  }
}


// Procedure: user_home
// Return the home folder of user
inline std::filesystem::path Prompt::_user_home() const{
  auto home = ::getenv("HOME");
  if(home == nullptr) {
    home = ::getpwuid(::getuid())->pw_dir;
  }
  return home ? home : std::filesystem::current_path();
}


// Procedure: _has_read_access 
// Check a file has read access grant to the user
inline bool Prompt::_has_read_access(const std::filesystem::path &path) const {
  if(auto rval=::access(path.c_str(), F_OK); rval == 0){
    if(rval = ::access(path.c_str(), R_OK); rval == 0){
      return true;
    }
  }
  return false;
}


// Procedure: _next_prefix
// Find the prefix among a set of strings starting from position n
inline std::string Prompt::_next_prefix(const std::vector<std::string>& words, const size_t n){
  if(words.empty()){
    return {};
  }
  else if(words.size() == 1){
    return words[0].substr(n, words[0].size()-n);
  }
  else{
    for(size_t end=n ;;++end){
      if(words[0].size() <= end){
        return words[0].substr(n, end-n);
      }
      for(size_t i=1; i<words.size(); i++){
        if(words[i].size() <= end or words[i][end] != words[0][end]){
          return words[i].substr(n, end-n);
        }
      }
    }
  }
}


// Procedure: _autocomplete_command
// This is the main entry for command autocomplete
inline int Prompt::_autocomplete_command(){
  if(auto words = _tree.match_prefix(_line.buf); words.empty()){
    return 0;
  }
  else{
    char c {0}; 
    bool stop {false};
    for(size_t i=0; not stop;){
      if(i < words.size()){
        _line_save = _line;
        _line.cur_pos = words[i].size();
        _line.buf = words[i];
        _refresh_single_line(_line);
        _line = _line_save;
      }
      else{
        _refresh_single_line(_line);
      }

      if(_cin.read(&c, 1); not _cin.good()){
        return -1;
      }

      switch(static_cast<KEY>(c)){
        case KEY::TAB:
          i = (i+1) % words.size();
          break;
        case KEY::ESC: // refresh the same thing again 
          _refresh_single_line(_line);
          stop = true;
          break;
        default:
          if(i < words.size()){
            _line.buf = words[i];
            _line.cur_pos = words[i].size();
          }
          stop = true;
          break;
      }
    }
    return c;
  }
}


// Procedure: _files_match_prefix
// Find all the files in a folder that match the prefix
inline std::vector<std::string> Prompt::_files_match_prefix(
    const std::filesystem::path& path
) const {
  // Need to check in case path is a file in current folder (parent_path = "")
  auto folder = path.filename() == path ? std::filesystem::current_path() : path.parent_path();
  std::string prefix(path.filename());

  std::vector<std::string> matches; 
  if(std::error_code ec; _has_read_access(folder) and std::filesystem::is_directory(folder, ec)){
    for(const auto& p: std::filesystem::directory_iterator(folder)){
      std::string fname {p.path().filename()};
      if(fname.compare(0, prefix.size(), prefix) == 0){
        matches.emplace_back(std::move(fname));
      }
    }
  }
  return matches;
}


// Procedure: _files_in_folder
// List all files in a given folder
inline std::vector<std::string> Prompt::_files_in_folder(const std::filesystem::path& path) const {
  auto p = path.empty() ? std::filesystem::current_path() : path;
  std::vector<std::string> matches;
  // Check permission 
  if(_has_read_access(p)){
    for(const auto& p: std::filesystem::directory_iterator(p)){
      matches.emplace_back(p.path().filename());
    }
  }
  return matches;
}


// Procedure: _dump_files 
// Format the strings for pretty print in terminal.
inline std::string Prompt::_dump_files(
    const std::vector<std::string>& v, 
    const std::filesystem::path& path)
{
  if(v.empty()){
    return {};
  }

  const size_t col_width = std::max_element(v.begin(), v.end(), 
                    [](const auto& a, const auto& b){ 
                    return a.size() < b.size() ? true : false; })->size() + 5;
  const size_t col_num {_terminal_columns()/col_width};
  std::string s("\n\r"); // New line and move cursor to left 
  char seq[64];
  snprintf(seq, 64, "\033[%d;1;49m", static_cast<int>(COLOR::BLUE));
  std::error_code ec;
  for(size_t i=0; i<v.size(); ++i){
    if(std::filesystem::is_directory(path.native() + "/" + v[i], ec)){
      // A typical color code example : \033[31;1;4m 
      //   \033[ : begin of color code, 31 : red color,  1 : bold,  4 : underlined
      s.append(seq, strlen(seq));
      s.append(v[i]);
      s.append("\033[0m", 4);
    }
    else{
      s.append(v[i]);
    }
    if(i%col_num == col_num-1){
      s.append("\x1b[0K\n\r");
    }
    else{
      s.append(col_width - v[i].size(), ' ');
    }
  }
  return s;
}


// Procedure: _autocomplete_folder 
// The entry for folder autocomplete
inline void Prompt::_autocomplete_folder(){
  std::string s;
  size_t ws_index = _line.buf.rfind(' ', _line.cur_pos) + 1;
  std::filesystem::path p(_line.buf[ws_index] != '~' ? 
                          _line.buf.substr(ws_index) : 
                          _user_home().native() + _line.buf.substr(ws_index+1));

  if(std::error_code ec; p.empty() or std::filesystem::is_directory(p, ec)){
    s = _dump_files(_files_in_folder(p), p);
  }
  else{
    auto match = _files_match_prefix(p);  
    if(not match.empty()){
      s = _dump_files(match, p.parent_path());
      if(auto suffix = _next_prefix(match, p.filename().native().size()); not suffix.empty()){
        _line.buf.insert(_line.cur_pos, suffix);
        _line.cur_pos += suffix.size();
        p += suffix;
        // Append a '/' if is a folder and not the prefix of other files
        if(std::filesystem::is_directory(p, ec) and 
            std::count_if(match.begin(), match.end(), 
              [p = p.filename().native()](const auto& m)
              { return (m.size() >= p.size() and m.compare(0, p.size(), p) == 0); }) == 1){
          _line.buf.insert(_line.cur_pos, 1, '/');
          _line.cur_pos += 1;
        }
      }
    }
  }
  if(s.size() > 0){
    s.append("\x1b[0K\n");
    _cout << s;
  }

  _refresh_single_line(_line);
}

// Procedure: _key_delete_prev_word
// Remove the word before cursor (including the whitespace between cursor and the word)
inline void Prompt::_key_delete_prev_word(LineInfo& line){
  // Remove all whitespace before cursor
  auto iter = std::find_if_not(line.buf.rbegin()+line.buf.size()-line.cur_pos, line.buf.rend(), 
                               [](const auto& c){ return c == ' ';});
  auto offset = std::distance(iter.base(), line.buf.begin()+line.cur_pos);
  line.buf.erase(iter.base(), line.buf.begin()+line.cur_pos);
  line.cur_pos -= offset;

  // Remove the word before cursor and stop at the first ws
  iter = std::find_if_not(line.buf.rbegin()+line.buf.size()-line.cur_pos, line.buf.rend(), 
                           [](const auto& c){ return c != ' ';});
  offset = std::distance(iter.base(), line.buf.begin()+line.cur_pos);
  line.buf.erase(iter.base(), line.buf.begin()+line.cur_pos);
  line.cur_pos -= offset;
}

// Procedure: _key_delete 
// Remove the character at cursor
inline void Prompt::_key_delete(LineInfo &line){
  if(line.buf.size() > 0){
    line.buf.erase(line.cur_pos, 1);
  }
}

// Procedure: _key_backspace 
// Remove the character before curosr
inline void Prompt::_key_backspace(LineInfo &line){
  if(line.cur_pos > 0){
    line.buf.erase(line.cur_pos-1, 1);
    line.cur_pos--;
  }
}

// Procedure: _key_prev_history
// Set line buffer to previous command in history
inline void Prompt::_key_prev_history(LineInfo& line){
  _key_history(line, true);
}

// Procedure: _key_next_history
// Set line buffer to next command in history
inline void Prompt::_key_next_history(LineInfo& line){
  _key_history(line, false);
}

// Procedure:_key_history
// Set the line buffer to previous/next history command
inline void Prompt::_key_history(LineInfo &line, bool prev){
  if(_history.size() > 1){
    *std::next(_history.begin(), _history.size()-1-line.history_trace) = line.buf;

    if(line.history_trace += prev ? 1 : -1; line.history_trace < 0){
      line.history_trace = 0;
    }
    else if(line.history_trace >= _history.size()){
      line.history_trace = _history.size()-1;
    }
    else{
      auto it = std::next(_history.begin(), _history.size()-1-line.history_trace);
      line.buf = *it;
      line.cur_pos = line.buf.size();
    }
  }
}

// Procedure: _append_character
// Insert a character to the cursor position in line buffer
inline bool Prompt::_append_character(LineInfo& line, char c){
  try{
    line.buf.insert(line.cur_pos, 1, c);
  }
  catch (std::bad_alloc &e){
    _cerr << "Exceed string max size\n";
    assert(false);
  }
  
  if(line.cur_pos++; line.buf.size() == line.cur_pos){
    if(not (_cout << c)){
      return false;
    }
  }
  else{
    _refresh_single_line(line);
  }
  return true;
}

// Procedure: _key_handle_CSI
// Handle Control Sequence Introducer
inline bool Prompt::_key_handle_CSI(LineInfo& line){ 
  char seq[3];
  if(_cin.read(seq,1), _cin.read(seq+1,1); not _cin.good() ){
    return false;
  }
  if(seq[0] == '['){
    if(seq[1] >= '0' and seq[1] <= '9'){
      if(_cin.read(seq+2, 1); not _cin.good()){
        return false;
      }
      if(seq[2] == '~' and seq[1] == '3'){
        _key_delete(line);
      }
    }
    else{
      switch(seq[1]){
        case 'A':  // Prev history
          _key_prev_history(line);
          break;
        case 'B':  // Next history
          _key_next_history(line);
          break;
        case 'C':  // Move cursor to right 
          if(line.cur_pos != line.buf.size()){
            line.cur_pos ++;
          }
          break;
        case 'D':  // Move cursor to left 
          if(line.cur_pos > 0){
            line.cur_pos --;
          }
          break;
        case 'H':  // Home : move cursor to the starting
          line.cur_pos = 0;
          break;
        case 'F':  // End  : move cursor to the end
          line.cur_pos = line.buf.size();
          break;
      }
    }
  }
  else if(seq[0] == 'O'){
    switch(seq[1]){
      case 'H':  // Home : move cursor to the starting
        line.cur_pos = 0;
        break;
      case 'F':  // End  : move cursor to the end
        line.cur_pos = line.buf.size();
        break;
    }
  }
  return true;
}


// Procedure: _edit_line 
// Handle the character input from the user
inline void Prompt::_edit_line(std::string &s){
  if(not (_cout << _prompt)){
    return;
  }

  add_history("");
  _line.reset();
  s.clear();
  for(char c;;){
    if(_cin.read(&c, 1); not _cin.good()){
      s = _line.buf;
      return ;
    }

    // if user hits tab
    if(static_cast<KEY>(c) == KEY::TAB){
      if(_line.buf.empty()){
        continue;
      }
      else if(_line.buf.rfind(' ', _line.cur_pos) != std::string::npos){
        _autocomplete_folder();
        continue;
      }
      else{
        if(c=_autocomplete_command(); c<0){
          // something wrong happened
          return;
        }
        else if(c == 0){
         continue;
        }
      }
    }

    // Proceed to process character
    switch(static_cast<KEY>(c)){
      case KEY::ENTER:
        _history.pop_back(); // Remove the emptry string history added in beginning  
        s =  _line.buf;
        return ;
      case KEY::CTRL_A:    // Go to the start of the line 
        if(_line.cur_pos != 0){
          _line.cur_pos = 0;
        }
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_B:    // Move cursor to left
        if(_line.cur_pos > 0){
          _line.cur_pos --;
        }
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_C:
        errno = EAGAIN;
        return ;
      case KEY::CTRL_D:    // Remove the char at the right of cursor. If the line is empty, act as end-of-file
        if(_line.buf.size() > 0){
          _key_delete(_line);
          _refresh_single_line(_line);
        }
        else{
          _history.pop_back();
          return;
        }
        break;
      case KEY::CTRL_E:    // Move cursor to end of line 
        if(_line.cur_pos != _line.buf.size()){
          _line.cur_pos = _line.buf.size();
        }
         _refresh_single_line(_line);       
        break;
      case KEY::CTRL_F:    // Move cursor to right
        if(_line.cur_pos != _line.buf.size()){
          _line.cur_pos ++;
        }
        _refresh_single_line(_line);
        break;
      case KEY::BACKSPACE:  // CTRL_H is the same as BACKSPACE 
      case KEY::CTRL_H:
        _key_backspace(_line);
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_K:    // Delete from current to EOF  
        _line.buf.erase(_line.cur_pos, _line.buf.size()-_line.cur_pos);
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_L:    // Clear screen 
        _clear_screen();
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_N:    // Next history command
        _key_next_history(_line);
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_P:    // Previous history command
        _key_prev_history(_line);
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_T:    // Swap current char with previous
        if(_line.cur_pos > 0){
          std::swap(_line.buf[_line.cur_pos], _line.buf[_line.cur_pos-1]);
          if(_line.cur_pos < _line.buf.size()){
            _line.cur_pos ++;
          }
          _refresh_single_line(_line);
        }
        break;
      case KEY::CTRL_U:    // Delete whole line
        _line.cur_pos = 0;
        _line.buf.clear();
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_W:    // Delete previous word 
        _key_delete_prev_word(_line);
        _refresh_single_line(_line);
        break;
      case KEY::ESC:
        if(not _key_handle_CSI(_line)){
          return;
        }
        _refresh_single_line(_line);
        break;
      default:
        _append_character(_line, c);
        break;
    }
  }
}


// Procedure: _refresh_single_line
// Flush the current line buffer on screen
inline void Prompt::_refresh_single_line(LineInfo &l){
  // 1. Append "move cursor to left" in the output buffer
  // 2. Append buf to output buffer
  // 3. Append "erase to  the right" to the output buffer 
  // 4. Append "forward cursor" to the output buffer : Adjust cursor to correct pos
  // 5. Write output buffer to fd  
  
  auto len {l.buf.size()};
  auto pos {l.cur_pos};
  size_t start {0};

  if(_prompt.length()+pos >= l.columns){
    start += _prompt.length()+pos-l.columns-1;
    len -= _prompt.length()+pos-l.columns-1;
    pos += (_prompt.length()+pos-l.columns-1);
  }

  if(_prompt.length()+len > l.columns){
    len -= (_prompt.length()+len-l.columns);
  }

  char seq[64];
  ::snprintf(seq, 64, "\r\x1b[%dC", (int)(pos+_prompt.length()));

  std::string obuf;
  obuf.reserve(CR.length()+_prompt.length()+len+EL.length()+strlen(seq));
  // Multiple copies ?
  obuf = CR + _prompt + l.buf.substr(start, len) + EL + std::string(seq);

  if(not (_cout << obuf)){
    _cerr << "Refresh line fail\n";
  }
}

};
#endif /* __PROMPT_H */

