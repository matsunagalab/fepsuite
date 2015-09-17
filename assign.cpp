#include "assign.hpp"
#include <functional>
#include <limits>
#include <queue>

using namespace std;
using namespace Eigen;

static char atomtype(const string& an)
{
  for(size_t i = 0; i < an.length(); ++i) {
    if(isalpha(an[i])){
      return an[i];
    }
  }
  return 0;
}

void assign_atoms(const string& process_atoms,
                  const vector<string>& Anames,
                  const vector<string>& Bnames,
                  const MatrixXd& distmat,
                  vector<int>& assignBofA,
                  vector<int>& assignAofB,
                  double threshold)
{
  vector<bool> Aenable(Anames.size());
  vector<bool> Benable(Bnames.size());

  for(int i = 0; i < (int)Anames.size(); i++) {
    char at = atomtype(Anames[i]);
    Aenable[i] = at && (process_atoms.find(at) != string::npos);
  }

  for(int i = 0; i < (int)Bnames.size(); i++) {
    char at = atomtype(Bnames[i]);
    Benable[i] = at && (process_atoms.find(at) != string::npos);
  }

  for(int i = 0; i < (int)Anames.size(); ++i) {
    if(!Aenable[i]) continue;
    if(assignBofA[i] != -1) continue;

    int minatm = -1;
    double mindist = numeric_limits<double>::max();
    for(int j = 0; j < (int)Bnames.size(); ++j) {
      if(!Benable[j]) continue;
      if(assignAofB[j] != -1) continue;
      double d = distmat(i, j);
      if(d < mindist){
        minatm = j;
        mindist = d;
      }
    }
    if(minatm >= 0 && mindist < threshold) {
      // found the atom
      assignBofA[i] = minatm;
      assignAofB[minatm] = i;
    }
  }
}

static void convert_bonds_to_adj_list(const topology &top, vector<vector<int> > &adj_list)
{
  adj_list.clear();
  adj_list.resize(top.names.size());
  for(const auto& bonditer: top.bonds) {
    int a = get<0>(bonditer.first);
    int b = get<1>(bonditer.first);
    adj_list[a].push_back(b);
    adj_list[b].push_back(a);
  }
  for(auto &v: adj_list) {
    sort(v.begin(), v.end());
  }
}


void assign_atoms_connectivity(const MatrixXd& distmat,
                               const topology& Atop,
                               const topology& Btop,
                               vector<int>& assignBofA,
                               vector<int>& assignAofB,
                               double threshold)
{
  // Turn bonds into adjacent list
  vector<vector<int> > Aadjlist, Badjlist;
  convert_bonds_to_adj_list(Atop, Aadjlist);
  convert_bonds_to_adj_list(Btop, Badjlist);

  vector<bool> Avisited(Aadjlist.size());
  
  // um, indeed, this need not be priority queue (BFS suffice)
  priority_queue<pair<int, int>, 
                 vector<pair<int, int> >,
                 greater<pair<int, int> > > pq;
  for(int i = 0; i < (int) assignBofA.size(); i++) {
    if(assignBofA[i] != -1) {
      pq.emplace(make_pair(0, i));
    }
  }

  while(!pq.empty()) {
    pair<int, int> e = pq.top();
    pq.pop();
    int Ai = e.second;

    if(Avisited[Ai]) continue;
    Avisited[Ai] = true;

    assert(assignBofA[Ai] >= 0);
    int Bi = assignBofA[Ai];
    assert(assignAofB[Bi] >= 0);
    
    for(int An: Aadjlist[Ai]) {
      // An is a neighbor of Ai
      if(assignBofA[An] != -1) continue;

      // Find best maching Bn s.t.
      // Bn is a neighbor of Bi
      // Bn is the closest to An and closer than the threshold
      // Bn is unassigned
      double mindist = numeric_limits<double>::max();
      int Bn = -1;
      for(int Bn_cand: Badjlist[Bi]) {
        if(assignAofB[Bn_cand] != -1) {
          continue;
        }
        double d = distmat(An, Bn_cand);
        if(d < mindist && d < threshold) {
          Bn = Bn_cand;
          mindist = d;
        }
      }
      if(Bn != -1) {
        // found Bn
        assignBofA[An] = Bn;
        assignAofB[Bn] = An;
        pq.emplace(make_pair(e.first + 1, An));
      }
    }
  }
}


