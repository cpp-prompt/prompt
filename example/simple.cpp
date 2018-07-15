#include "prompt.hpp"

int main(){
  prompt::Prompt shell(
    "Welcome to Prompt", 
    "ot> ",
    std::filesystem::current_path()/"my_prompt_history"
  ); 

  shell.autocomplete("read_celllib");
  shell.autocomplete("asia");
  shell.autocomplete("american");

  std::string line;

  //while(shell.readline(line)){
  //  std::cout << "line = " << line << std::endl;
  //}
  //return 0;


  for(;;){
    shell.readline(line);
    std::cout << "line = " << line << std::endl;
    // Hit enter to exit
    if(line.empty()){
      break;
    }
  }
}
