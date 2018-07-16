#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <iostream>
#include <cstring>
#include <string_view>
#include <random>
#include <unordered_set>

#include "prompt.hpp"



template <typename C>
std::basic_string<C> gen_random(const size_t word_len) {

  const size_t len {rand()%word_len + 1};
  std::basic_string<C> s;

  size_t char_sz {0};
  if constexpr(std::is_signed_v<C>){
    // Use 1UL (type long int) instead of 1 (type int)
    char_sz = (1UL << 7*sizeof(C));
  }
  else{
    char_sz = (1UL << 8*sizeof(C));
  }

	for (size_t i=0; i<len; ++i) {
    // We need to avoid null character (ascii = 0)
    s.push_back(std::max(rand()%char_sz, size_t(1)));
	}
  return s;
}

template <typename C>
bool has_same_prefix(const typename prompt::RadixTree<C>::Node& n){
  std::vector<std::basic_string_view<C>> sv;
  for(const auto& c: n.children){
    sv.emplace_back(c.first);
  }
  
  // Compare first character
  for(size_t i=1; i<sv.size(); i++){
    if(sv[i][0] == sv[0][0]){
      return true;
    }
  }

  for(const auto& c: n.children){
    if(has_same_prefix<C>(*c.second)){
      return true;
    }
  }
  return false;
}

template <typename C>
bool is_prefix(std::basic_string_view<C> str, std::basic_string_view<C> prefix){
  if(str.size() < prefix.size()){
    return false;
  }
  for(size_t i=0; i<prefix.size(); i++){
    if(prefix[i] != str[i]){
      return false;
    }
  }
  return true;
}


template <typename C>
void test_radix_tree_type(){
  prompt::RadixTree<C> tree;
  const size_t word_num {1000};
  const size_t word_len {20};

  std::unordered_set<std::basic_string<C>> words;

  for(size_t i=0; i<word_num; i++){
    words.emplace(gen_random<C>(word_len));
  }

  for(const auto& w: words){
    tree.insert(w);
  }

  // Check every word must exist
  for(const auto& w: words){
    REQUIRE(tree.exist(w));
  }

  // Check a unknown word must not exist
  for(const auto& w: words){
    if(w.size() > 1){
      size_t last_index {rand()%(w.size()) + 1};
      std::basic_string<C> s(w.data(), last_index);
      if(words.find(s) == words.end()){
        REQUIRE(not tree.exist(s));
      }
    }
  }

  auto ret = tree.all_words();
  REQUIRE(ret.size() == words.size());
  for(const auto& w: ret){
    words.erase(w);
  }

  REQUIRE(words.empty());

  const auto& root {tree.root()};
  REQUIRE(not has_same_prefix<C>(root));

  // ---------------------------  Check match_prefix function ------------------------------------- 
  {
    for(auto reverse: {false, true}){
      std::basic_string<C> str;
      const size_t len {1000};
      // Randomly generate a string
      while(str.size() < len){
        str = gen_random<C>(len);
      }

      prompt::RadixTree<C> t;
      // Insert substrings in decreasing or increasing length order. Those substrings that are 
      // the prefix of the random string
      if(reverse){
        for(size_t i=len; i>=1; i--){
          t.insert(str.substr(0, i));
        }
      }
      else{
        for(size_t i=1; i<=len; i++){
          t.insert(str.substr(0, i));
        }
      }

      // Find the matched strings using the match_prefix function
      for(size_t i=1; i<len; i++){
        auto match = t.match_prefix(str.substr(0, i));

        std::sort(match.begin(), match.end(), [](const auto& a, const auto& b){ return a.size() < b.size(); });

        REQUIRE(match.size() == len - i + 1);
        REQUIRE(match.front().size() == i);   
        REQUIRE(match.back().size() == len);
        // Neighbor strings should only differ by one
        REQUIRE(std::adjacent_find(match.begin(), match.end(), 
              [](const auto& a, const auto& b){ return b.size() - a.size() != 1; }) == match.end());

        for(const auto& m: match){
          REQUIRE(str.compare(0, m.size(), m)==0);
        }
      }
    }
  }
}

TEST_CASE("RadixTree") {
  srand(time(nullptr));
  test_radix_tree_type<char>();
  test_radix_tree_type<wchar_t>();
  test_radix_tree_type<char16_t>();
  test_radix_tree_type<char32_t>();
}


