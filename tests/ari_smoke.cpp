#include "../Zeal/raid_autorejoin.h"
#include <cassert>
#include <iostream>

int main() {
  RaidAutoRejoin a;
  assert(a.observe(21, {{"Thalor",1},{"Durok",1}}, true,1000).empty());
  a.enable(true);
  assert(a.observe(21, {{"Thalor",1},{"Durok",1}}, true,2000).empty());
  assert(a.remembered_count()==2);
  assert(a.observe(21, {{"Thalor",3},{"Durok",1}}, true,3000).empty());
  assert(a.observe(21, {{"Thalor",3},{"Durok",1}}, true,4000).empty()); // learns group3
  assert(a.observe(21, {{"Durok",1}}, true,5000).empty());  // leaves
  auto moves=a.observe(21,{{"Thalor",0},{"Durok",1}},true,6000);
  assert(moves.size()==1 && moves[0].name=="Thalor" && moves[0].group==3);
  assert(a.observe(21,{{"Thalor",0},{"Durok",1}},true,7000).empty()); // no spam
  assert(a.observe(21,{{"Thalor",3},{"Durok",1}},true,8000).empty()); // success
  assert(a.observe(21,{{"Durok",1}},true,9000).empty());
  assert(a.observe(21,{{"Thalor",3},{"Durok",1}},true,10000).empty());
  assert(a.observe(21,{{"Durok",1}},true,11000).empty());
  // Non-leaders do not move anyone.
  assert(a.observe(21,{{"Thalor",0},{"Durok",1}},false,12000).empty());
  // Leader on a subsequent tick can restore.
  assert(a.observe(21,{{"Thalor",0},{"Durok",1}},true,13000).size()==1);
  a.enable(false);
  assert(a.observe(21,{{"Thalor",0},{"Durok",1}},true,14000).empty());
  a.enable(true);
  // A new raid never restores someone from an old one.
  assert(a.observe(21,{{"Thalor",1}},true,15000).empty());
  assert(a.observe(22,{{"Thalor",0},{"Kaelen",2}},true,16000).empty());
  // Group full test
  a.enable(true);
  assert(a.observe(99,{{"Thalor",2},{"A",2},{"B",2},{"C",2},{"D",2},{"E",2}},true,17000).empty());
  assert(a.observe(99,{{"A",2},{"B",2},{"C",2},{"D",2},{"E",2}},true,18000).empty());
  assert(a.observe(99,{{"Thalor",0},{"A",2},{"B",2},{"C",2},{"D",2},{"E",2},{"F",2}},true,19000).empty());
  assert(a.observe(99,{{"Thalor",0},{"A",2},{"B",2},{"C",2},{"D",2},{"E",2},{"F",2}},true,20000).empty());
  assert(a.observe(99,{{"Thalor",0},{"A",2},{"B",2},{"C",2},{"D",2},{"E",2}},true,21000).size()==1);
  std::cout<<"All ARI raid state tests passed\n";
}
