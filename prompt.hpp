#ifndef __PROMPT_H
#define __PROMPT_H

#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <pwd.h>

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
#include <optional>
#include <cassert>


namespace std {

namespace filesystem = experimental::filesystem;

};

namespace ot {

// Function: read_line
template <typename C, typename T, typename A>
std::basic_istream<C, T>& read_line(std::basic_istream<C, T>& is, std::basic_string<C, T, A>& line) {

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


  class RadixTree{

    // TODO: unique_ptr to enable automatic memory management...
    struct Node{
      //std::list<std::pair<std::string, Node*>> childs;
      std::list<std::pair<std::string, std::unique_ptr<Node>>> childs;
    };

    // TODO: unittest
    // check for each node no pariwise edges have common prefix

    public:

     RadixTree() = default;
     RadixTree(const std::vector<std::string>&);
     
     bool exist(const std::string&) const;
     void insert(const std::string&);
    
     std::vector<std::string> match_prefix(const std::string&) const;
     std::vector<std::string> all_words() const;
			
		 std::string dump();

    private:

     Node _root;
     void _insert(std::string_view, Node*);
     void _collect_postfix(std::vector<std::string>&, const std::unique_ptr<Node>&, const std::string&) const;
     size_t _count_prefix_match(std::string_view, std::string_view) const;
     std::pair<const Node*, std::string> _search_prefix_node(std::string_view) const;

     void _dump(Node*, size_t, std::string&);
  };

  RadixTree::RadixTree(const std::vector<std::string>& words){
    for(const auto& w: words){
      insert(w);
    }
  }

  std::string RadixTree::dump() {
    std::string t;
    _dump(&_root, 0, t);      
    t.append(1, '\n');
    return t;
  }

  void RadixTree::_dump(Node* n, size_t level, std::string& s) {
    for(auto &[k,v]: n->childs){
      assert(v.get() != nullptr);
      s.append(level, '-');
      s.append(1, ' ');
      s += k;
      s.append(1, '\n');
      _dump(v.get(), level+1, s);
    }
  }

  void RadixTree::_collect_postfix(std::vector<std::string> &vec, const std::unique_ptr<Node>& n, const std::string& s) const {
    if(n->childs.empty()){
      vec.emplace_back(s);
    }
    else{
      for(auto& [k,v]:n->childs){
        _collect_postfix(vec, v, s+k);
      }
    }
  }

  std::vector<std::string> RadixTree::all_words() const {
    std::vector<std::string> postfix;
    for(auto &[k, v]: _root.childs){
      _collect_postfix(postfix, v, k);
    }
    return postfix;
  }

  std::vector<std::string> RadixTree::match_prefix(const std::string& prefix) const {
    if(auto [prefix_node, suffix] = _search_prefix_node(prefix); prefix_node == nullptr){
      return {};
    }
    else{
      std::vector<std::string> matches;
      for(auto &[k, v]: prefix_node->childs){
        _collect_postfix(matches, v, prefix+suffix+k);
      }
      return matches;
    }
  }


  std::pair<const RadixTree::Node*,std::string> RadixTree::_search_prefix_node(std::string_view s) const {
    Node const *n = &_root;
    std::string suffix {""};
    for(size_t pos=0; pos<s.size(); ){ // Search until full match
      bool match = {false};
      for(const auto& [k, v]: n->childs){
        if(auto num = _count_prefix_match(k, s.data()+pos); num>0){
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

  size_t RadixTree::_count_prefix_match(std::string_view sv1, std::string_view sv2) const {
    size_t i {0};
    size_t len {std::min(sv1.size(), sv2.size())};
    for( ;i<len; ++i){
      if(sv1[i] != sv2[i]){
        break;
      }
    }
    return i;
  }

  bool RadixTree::exist(const std::string& s) const {
    size_t pos {0};
    Node const *n = &_root;
    while(not n->childs.empty()){ // Search until reaching the leaf
      bool match {false};
      for(const auto& [k, v]: n->childs){
        if(s.compare(pos, k.size(), k) == 0){
          pos += k.size();
          n = v.get();
          match = true;
          break;
        }
      }
      if(not match){
        return false;
      }
    }
    return true;
  }

  void RadixTree::insert(const std::string& s){
    if(s.empty()){  // Empty string not allowed
      return;
    }
    _insert(s, &_root);
  }
	
  // TODO: avoid create std::string from string_view
  void RadixTree::_insert(std::string_view sv, Node* n){
    size_t match_num {0};
    auto ch = n->childs.begin();
    for(auto &kv: n->childs){
      if(match_num = _count_prefix_match(std::get<0>(kv), sv); match_num > 0){
        break;
      }
      ch ++;
    }

    if(match_num == 0){   // Base case 1 
      if(auto& child = std::get<std::unique_ptr<Node>>(n->childs.emplace_back(std::make_pair(sv, new Node))); sv.size() > 0){
        child->childs.emplace_back(std::make_pair("", new Node));  // insert an empty string to denote end of key
      }
    }
    else if(match_num == ch->first.size()){  // Keep searching along the child
      _insert(sv.data()+match_num, ch->second.get());
    }
    else{  // Base case 2
      // Split the child node into new nodes  
      auto& par = std::get<std::unique_ptr<Node>>(n->childs.emplace_back(std::make_pair(std::string_view(sv.data(),match_num), new Node)));
      par->childs.emplace_back(std::make_pair(ch->first.data()+match_num, std::move(ch->second)));
      auto& rc = std::get<std::unique_ptr<Node>>(par->childs.emplace_back(std::make_pair(sv.data()+match_num, new Node)));
      if(sv.size() > 0){
        rc->childs.emplace_back(std::make_pair("", new Node));  // insert an empty string to denote end of key
      }
      n->childs.erase(ch);
    }
  }




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



// \r  : carriage return => move cursor to the beginning of line

// TODO: make Prompt templated at Tree
class Prompt {

  struct LineInfo{
    LineInfo(size_t);
		
    std::string buf;
    size_t history_trace {0};
    size_t cur_pos {0};   // cursor position
    size_t columns {80};  // default width of terminal is 80

    void operator = (const LineInfo&);
    // TODO
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

    // TODO: rename to _autocomplete_folder
    // avoid throw...
    // handle ~ (user folder)
    void _autocomplete_folder();

    std::vector<std::string> _files_in_folder(const std::filesystem::path&) const;
    std::vector<std::string> _files_match_prefix(const std::filesystem::path&) const;
    std::string _dump_files(const std::vector<std::string>&);
    std::string _next_prefix(const std::vector<std::string>&, const size_t);
    std::filesystem::path _user_home() const;

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

Prompt::LineInfo::LineInfo(size_t cols): columns(cols){
}

// Copy assignment
void Prompt::LineInfo::operator = (const LineInfo& l){
  buf = l.buf;
  cur_pos = l.cur_pos;
  columns = l.columns; 
  history_trace = l.history_trace;   
}


// Procedure: Ctor
Prompt::Prompt(const std::string &p, std::istream& in, std::ostream& out, std::ostream& err, int infd): 
  _prompt(p), 
  _cin(in),
  _cout(out),
  _cerr(err),
  _infd(infd)
{}

// Procedure: Dtor
Prompt::~Prompt(){
  // Restore the original mode if has kept
  if(_has_orig_termios){
    ::tcsetattr(_infd, TCSAFLUSH, &_orig_termios);
  }
}

// Procedure: autocomplete
// This function adds the word into radix tree
void Prompt::autocomplete(const std::string& word){
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
          std::find(_unsupported_terms.begin(), _unsupported_terms.end(), str) != _unsupported_terms.end()){
    return true;
  }
  return false;
}

// Procedure: readline 
// This is the main entry of Prompt/
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
    return true;
  }
}

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
  //if(_cout.write("\x1b[6n", 4); not _cout.good()){
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
void Prompt::_clear_screen() {
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
    int start, cols;
    start = _get_cursor_pos();
    //if(start == -1 or (_cout.write("\x1b[999c", 6) and not _cout.good())){
    if(start == -1 or not (_cout << "\x1b[999c")){
      return 80;
    }
    cols = _get_cursor_pos();
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


// Function: user_home
std::filesystem::path Prompt::_user_home() const{
  auto home = ::getenv("HOME");
  if(home == nullptr) {
    home = ::getpwuid(::getuid())->pw_dir;
  }
  return home ? home : std::filesystem::current_path();
}

std::string Prompt::_next_prefix(const std::vector<std::string>& words, const size_t n){
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

std::vector<std::string> Prompt::_files_match_prefix(const std::filesystem::path& path) const {
  // Need to check in case path is just a file namespace
  auto folder = path.filename() == path ? std::filesystem::current_path() : path.parent_path();
  std::string prefix(path.filename());

  std::vector<std::string> matches; 
  if(std::error_code ec; std::filesystem::is_directory(folder, ec)){
    for(const auto& p: std::filesystem::directory_iterator(folder)){
      std::string fname {p.path().filename()};
      if(fname.compare(0, prefix.size(), prefix) == 0){
        matches.emplace_back(std::move(fname));
      }
    }
  }
  return matches;
}

std::vector<std::string> Prompt::_files_in_folder(const std::filesystem::path& path) const {
  auto p = path.empty() ? std::filesystem::current_path() : path;
  std::vector<std::string> matches;
  for(const auto& p: std::filesystem::directory_iterator(p)){
    matches.emplace_back(p.path().filename());
  }
  return matches;
}


// Function: _dump_files
std::string Prompt::_dump_files(const std::vector<std::string>& v){
  if(v.empty()){
    return {};
  }

  const size_t col_width = std::max_element(v.begin(), v.end(), 
                    [](const auto& a, const auto& b){ return a.size() < b.size() ? true : false; })->size() + 5;
  const size_t col_num {_terminal_columns()/col_width};
  std::string s("\n\r"); // New line and move cursor to left
  for(size_t i=0; i<v.size(); ++i){
    s.append(v[i]);
    if(i%col_num == col_num-1){
      s.append("\x1b[0K\n\r");
    }
    else{
      s.append(col_width - v[i].size(), ' ');
    }
  }
  return s;
}


inline void Prompt::_autocomplete_folder(){
  std::string s;
  size_t ws_index = _line.buf.rfind(' ', _line.cur_pos) + 1;
  std::filesystem::path p(_line.buf[ws_index] != '~' ? 
                          _line.buf.substr(ws_index) : _user_home().native() + _line.buf.substr(ws_index+1));

  if(std::error_code ec; p.empty() or std::filesystem::is_directory(p, ec)){
    s = _dump_files(_files_in_folder(p));
  }
  else{
    auto match = _files_match_prefix(p);  
    if(not match.empty()){
      s = _dump_files(match);
      if(auto suffix = _next_prefix(match, p.filename().native().size()); not suffix.empty()){
        _line.buf.insert(_line.cur_pos, suffix);
        _line.cur_pos += suffix.size();
        p += suffix;
        // Append a '/' if is a folder and not the prefix of other files
        if(std::filesystem::is_directory(p, ec) and 
            std::count_if(match.begin(), match.end(), 
              [p = p.filename().native()](const auto& m){ return (m.size() >= p.size() and m.compare(0, p.size(), p) == 0); }) == 1){
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

inline void Prompt::_key_delete(LineInfo &line){
  if(line.buf.size() > 0){
    line.buf.erase(line.cur_pos, 1);
  }
}

inline void Prompt::_key_backspace(LineInfo &line){
  if(line.cur_pos > 0){
    line.buf.erase(line.cur_pos-1, 1);
    line.cur_pos--;
  }
}

inline void Prompt::_key_prev_history(LineInfo& line){
  _key_history(line, true);
}

inline void Prompt::_key_next_history(LineInfo& line){
  _key_history(line, false);
}

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
// Handle the input character 
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
        if(c=_autocomplete_command(); c==0){
          continue;
        }
        else if(c < 0){
          // something wrong happened
          return;
        }
      }
    }

    // Proceed to process character
    switch(static_cast<KEY>(c)){
      case KEY::ENTER:
        _history.pop_back(); // Remove the emptry string history added in beginning  
        s =  _line.buf;
        return ;
      case KEY::CTRL_A: // Go to the start of the line 
        if(_line.cur_pos != 0){
          _line.cur_pos = 0;
        }
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_B:  // Move cursor to left
        if(_line.cur_pos > 0){
          _line.cur_pos --;
        }
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_C:
        errno = EAGAIN;
        return ;
      case KEY::CTRL_D:  // Remove the char at the right of cursor. If the line is empty, act as end-of-file
        if(_line.buf.size() > 0){
          _key_delete(_line);
          _refresh_single_line(_line);
        }
        else{
          _history.pop_back();
          return;
        }
        break;
      case KEY::CTRL_E: // Move cursor to end of line 
        if(_line.cur_pos != _line.buf.size()){
          _line.cur_pos = _line.buf.size();
        }
         _refresh_single_line(_line);       
        break;
      case KEY::CTRL_F:  // Move cursor to right
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
      case KEY::CTRL_K:  // Delete from current to EOF  
        _line.buf.erase(_line.cur_pos, _line.buf.size()-_line.cur_pos);
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_L:  // Clear screen 
        _clear_screen();
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_N: // Next history command
        _key_next_history(_line);
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_P:  // Previous history command
        _key_prev_history(_line);
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_T:   // Swap current char with previous
        if(_line.cur_pos > 0){
          std::swap(_line.buf[_line.cur_pos], _line.buf[_line.cur_pos-1]);
          if(_line.cur_pos < _line.buf.size()){
            _line.cur_pos ++;
          }
          _refresh_single_line(_line);
        }
        break;
      case KEY::CTRL_U:  // Delete whole line
        _line.cur_pos = 0;
        _line.buf.clear();
        _refresh_single_line(_line);
        break;
      case KEY::CTRL_W:  // Delete previous word 
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



inline void Prompt::_refresh_single_line(LineInfo &l){
  // 1. Append "move cursor to left" in the output buffer
  // 2. Append buf to output buffer
  // 3. Append "erase to  the right" to the output buffer 
  // 4. Append "forward cursor" to the output buffer : Adjust cursor to correct pos
  // 5. Write output buffer to fd  
  
  auto len {l.buf.size()};
  auto pos {l.cur_pos};
  size_t start {0};

  while(_prompt.length() + pos >= l.columns){
    start ++;
    len --;
    pos ++;
  }
  while(_prompt.length() + len > l.columns){
    len --;
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

