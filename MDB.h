/*
Copyright 2018, Michael R. Hoopmann, Institute for Systems Biology

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef _MDB_H
#define _MDB_H

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "MLog.h"
#include "MStructs.h"

class MDatabase{
public:

  //Constructors & Destructors
  MDatabase();
  ~MDatabase();

  //Operators
  mDB& operator[ ](const int& i);

  //User Functions
  bool  buildDB       (const char* fname, std::string decoyStr, std::string entrapmentStr);                     //Reads FASTA file and populates vDB
  void  buildDecoy(std::string decoy_label);
  void  buildEntrapment(std::string entrapment_label); //Generate entrapment sequences (shuffled targets labeled as targets) for each target sequence
  bool  buildPeptides (double min, double max, int mis, int minP, int maxP); //Make peptide list within mass boundaries and miscleavages.
  void  exportDB(std::string fName);

  //Accessors & Modifiers
  void                addFixedMod         (char mod, double mass);
  mDB&                at                  (const int& i);
  mEnzymeRules&       getEnzymeRules      ();
  int                 getMaxPepLen        (double mass);
  mPeptide&           getPeptide          (int index);
  std::vector<mPeptide>*   getPeptideList      ();
  int                 getPeptideListSize  ();
  bool                getPeptideSeq       (int index, int start, int stop, char* str);
  bool                getPeptideSeq       (int index, int start, int stop, std::string& str);
  bool                getPeptideSeq       (mPeptide& p, std::string& str);
  bool                getPeptideSeq       (int pepIndex, std::string& str);
  mDB&                getProtein          (int index);
  int                 getProteinDBSize    ();
  void                setAAMass           (char aa, double mass);
  bool                setEnzyme           (const char* str);
  void                setLog              (MLog* c);
  void                setAdductSites      (bool* arr);

  double minMass[100];
  int adductPepCount;

private:
  
  //Data Members
  double        AA[128];   //Amino acid masses
  double        fixMassPepC;
  double        fixMassPepN;
  double        fixMassProtC;
  double        fixMassProtN;
  bool          adductSites[128];
  mEnzymeRules  enzyme;    //Where to cut to generate peptides

  std::vector<mDB>      vDB;    //Entire FASTA database stored in memory
  std::vector<mPeptide> vPep;   //List of all peptides

  MLog* mlog;

  void addPeptide(int index, int start, int len, double mass, mPeptide& p, std::vector<mPeptide>& vP, bool bN, bool bC, char xlSites);
  bool checkAA(size_t i, size_t start, size_t n, size_t seqSize, bool& bN, bool& bC);

  // entrapment / decoy generation
  // Do not use addReversedTargets for both entrapment and decoy generation as this will cause decoys to be identical to targets
  void addReversedTargets(std::string label, bool add_alter);
  void addShuffledTargets(std::string label, bool add_alter);

  //Utility functions (for sorting)
  static int compareMass      (const void *p1, const void *p2);
  static int compareSequence  (const void *p1, const void *p2);
  static bool compareSequenceB(const mPepSort& p1, const mPepSort& p2);

};

#endif
