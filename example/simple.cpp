#include "prompt.hpp"

int main(){
  ot::Prompt shell("ot> "); 

  shell.autocomplete("read_celllib");
  shell.autocomplete("asia");
  shell.autocomplete("american");

  for(std::string line;;){
    shell.readline(line);
    std::cout << "\nline = " << line << std::endl;
    if(line.size() == 0){
      break;
    }
  }
}
