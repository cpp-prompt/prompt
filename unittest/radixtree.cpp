#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "merge.hpp"
#include <iostream>
#include <cstring>
#include <string_view>
#include <random>
#include <unordered_set>

std::string gen_random(size_t range) {
  const size_t len {rand()%range + 1};
	static const char alphanum[] =
		"0123456789 -_"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

  std::string s(len, '0');

	for (size_t i=0; i<len; ++i) {
		s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}
  return s;
}


TEST_CASE("RadixTree") {

  srand(time(nullptr));

  ot::RadixTree tree;
  const size_t word_num {500000};
  const size_t range {20};

  std::unordered_set<std::string> words;

  for(size_t i=0; i<word_num; i++){
    words.emplace(gen_random(range));
  }

  for(const auto& w: words){
    tree.insert(w);
  }

  for(const auto& w: words){
    REQUIRE(tree.exist(w));
  }

  auto ret = tree.all_words();
  REQUIRE(ret.size() == words.size());
  for(const auto& w: ret){
    words.erase(w);
  }

  REQUIRE(words.empty());
}
