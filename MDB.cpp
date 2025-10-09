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

#include "MDB.h"

using namespace std;

//==============================
//  Constructors & Destructors
//==============================
MDatabase::MDatabase(){
  for(int i=0;i<128;i++) AA[i]=0;
  AA['A']=71.0371103;
  AA['C']=103.0091803;
  AA['D']=115.0269385;
  AA['E']=129.0425877;
  AA['F']=147.0684087;
  AA['G']=57.0214611;
  AA['H']=137.0589059;
  AA['I']=113.0840579;
  AA['K']=128.0949557;
  AA['L']=113.0840579;
  AA['M']=131.0404787;
  AA['N']=114.0429222;
  AA['P']=97.0527595;
  AA['Q']=128.0585714;
  AA['R']=156.1011021;
  AA['S']=87.0320244;
  AA['T']=101.0476736;
  AA['U']=150.9536303;
  AA['V']=99.0684087;
  AA['W']=186.0793065;
  AA['Y']=163.0633228;

  fixMassPepC=0;
  fixMassPepN=0;
  fixMassProtC=0;
  fixMassProtN=0;
  adductPepCount=0;

  for(int i=0;i<100;i++) minMass[i]=1e6;

}

MDatabase::~MDatabase(){
  mlog = NULL;
}

//==============================
//  Operators
//==============================
mDB& MDatabase::operator[ ](const int& i){
  return vDB[i];
}

//==============================
//  User Functions
//==============================

//buildDB reads in a FASTA file and stores it in memory.
bool  MDatabase::buildDB(const char* fname, string decoyStr, string entrapmentStr) {
  char    str[10240];
  char*   tok;
  FILE*   f;
  mDB    d;
  char   c;

  d.name="NIL";

  vDB.clear();
  f=fopen(fname,"rt");
  if(f==NULL) return false;
  while(!feof(f)){
    if(fgets(str,10240,f)==NULL) continue;
    if(strlen(str)>0){
      tok=strtok(str,"\r\n");
      if(tok==NULL) continue;
      strcpy(str,tok);
    }
    if(str[0]=='>') {
      if(d.name.compare("NIL")!=0) {
        if(d.sequence.length()>65000){
          if (mlog != NULL) mlog->addDBWarning(d.name + " has a sequence that is too long. It will be skipped.");
          else cout << "  WARNING: " << &d.name[0] << " has a sequence that is too long. It will be skipped." << endl;
        } else {
          if (d.name.find(decoyStr) != string::npos) d.decoy = true;
          else d.decoy = false;
          vDB.push_back(d);
        }
      }
      string st=&str[1];
      size_t sp=st.find_first_of(' ');
      d.name=st.substr(0,sp);
      d.description=st.substr(sp+1,st.size());
      d.sequence="";
    } else {
      for(unsigned int i=0;i<strlen(str);i++){
        c=toupper(str[i]);
        if(AA[c]==0) {
          if (mlog != NULL) mlog->addDBWarning(d.name + " has an unexpected amino acid character or errant white space: '" + c + "'");
          else cout << "  WARNING: " << &d.name[0] << " has an unexpected amino acid character or errant white space: '" << c << "'" << endl;
        }
        if(c==' ' || c=='\t') continue;
        if (AA[c] == 0) {
          if (mlog != NULL) {
            string tmpStr = "Mass of '";
            mlog->addDBWarning(tmpStr + c + "' is currently set to 0. Consider revising with the aa_mass parameter.");
          } else cout << "  WARNING: Mass of '" << c << "' is currently set to 0. Consider revising with the aa_mass parameter." << endl;
        }
        d.sequence+=c;
      }
    }
  }
  fclose(f);
  if(d.sequence.length()>SIZE_MAX){
    if (mlog != NULL) mlog->addDBWarning(d.name + " has a sequence that is too long. It will be skipped.");
    else cout << "  WARNING: " << &d.name[0] << " has a sequence that is too long. It will be skipped." << endl;
  } else {
    if (d.name.find(decoyStr) != string::npos) d.decoy = true;
    else d.decoy = false;
    vDB.push_back(d);
  }

  cout << "  Total Proteins: " << vDB.size() << endl;
  return true;
}

//Builds decoys on the fly from sequences in the search space. Decoys are sequences reversed in place between
//enzyme cut points. If the sequence is palindromic, a minor attempt at creating a novel sequence is made.
void MDatabase::buildDecoy(string decoy_label) {
  addShuffledTargets(decoy_label, true);
}

//Builds entrapments on the fly from sequences in the search space. Entrapments are sequences reversed in place between
//enzyme cut points. If the sequence is palindromic, a minor attempt at creating a novel sequence is made.
//Entrapments are labeled as targets later in the processing pipeline.
void MDatabase::buildEntrapment(string entrapment_label)
{
  addReversedTargets(entrapment_label, false);
}

void MDatabase::addReversedTargets(string label, bool add_alter) {
  typedef struct clips {
    int start;
    int stop;
  } clips;

  size_t i, j, sz;
  vector<clips> cut;
  clips c;

  sz = vDB.size();
  char alter = '0';
  for (i = 0; i < sz; i++) {

    cut.clear();
    c.start = -1;
    for (j = 1; j < vDB[i].sequence.size(); j++) {
      if (enzyme.cutN[vDB[i].sequence[j]] || enzyme.cutC[vDB[i].sequence[j]]) {
        if (c.start > -1) { //mark the space in between
          c.stop = (int)j - 1;
          cut.push_back(c);
          c.start = -1; //reset
        }
      } else {
        if (c.start < 0) c.start = (int)j;
      }
    }
    if (!enzyme.cutN[vDB[i].sequence[j - 1]] && !enzyme.cutC[vDB[i].sequence[j - 1]]) { //check last amino acid
      c.stop = (int)j - 1;
      cut.push_back(c);
    }

    //reverse the sequences
    string rev;
    mDB reversed = vDB[i];
    reversed.name = (add_alter ? (label + alter) : label) + "_" + reversed.name;
    if (alter == '0') alter = '1';
    else alter = '0';
    for (j = 0; j < cut.size(); j++) {
      rev.clear();

      //adjust ends for restrictive sites
      while (enzyme.exceptN[reversed.sequence[cut[j].start]] || enzyme.exceptC[reversed.sequence[cut[j].start]]) {
        cut[j].start++;
        if (cut[j].start == reversed.sequence.size()) break;
      }
      while (enzyme.exceptN[reversed.sequence[cut[j].stop]] || enzyme.exceptC[reversed.sequence[cut[j].stop]]) {
        cut[j].stop--;
        if (cut[j].stop == -1) break;
      }
      if (cut[j].start == reversed.sequence.size()) continue; //skip when out of bounds
      if (cut[j].stop == -1) continue; //skip when out of bounds
      if (cut[j].stop <= cut[j].start) continue; //skip if nothing will happen

      for (size_t k = cut[j].stop; k >= cut[j].start; k--) {
        rev += reversed.sequence[k];
        if (k == 0) break;
      }
      if (rev.size() > 2 && rev.compare(reversed.sequence.substr(cut[j].start, (size_t)cut[j].stop - (size_t)cut[j].start + 1)) == 0) { //edge case of palindrome sequence
        char c = rev[0];
        rev[0] = rev[rev.size() / 2];
        rev[rev.size() / 2] = c; //just swap the first and middle characters, see if that breaks up the palindrome
      }
      reversed.sequence.replace(cut[j].start, (size_t)cut[j].stop - (size_t)cut[j].start + 1, rev);
    }

    vDB.push_back(reversed);
  }

  cout << "  Adding Magnum-generated " + label + "s. New Total Proteins: " << vDB.size() << endl;
}

void MDatabase::addShuffledTargets(string label, bool add_alter) {
  size_t sz = vDB.size();
  char alter = '0';
  for (size_t i = 0; i < sz; i++) {
    mDB shuffled = vDB[i];
    shuffled.name = (add_alter ? (label + alter) : label) + "_" + shuffled.name;
    if (alter == '0') alter = '1';
    else alter = '0';

    string seq = shuffled.sequence;
    size_t n = seq.length();

    // Fisher-Yates shuffle algorithm
    for (size_t j = n - 1; j > 0; j--) {
      // Pick a random index from 0 to j (inclusive)
      size_t k = rand() % (j + 1);

      // Swap sequence[j] with sequence[k]
      swap(seq[j], seq[k]);
    }

    shuffled.sequence = seq;
    vDB.push_back(shuffled);
  }
}

//buildPeptides creates lists of peptides to search based on the user-defined enzyme rules
bool MDatabase::buildPeptides(double min, double max, int mis,int minP, int maxP){

  double mass;
  bool bCutMarked;
  bool bNTerm;
  bool bCTerm;

  int mc;
  int next;

  char xlSites;

  mPepMap  pm;
  mPeptide p;
  
  size_t DBSize=vDB.size();
  size_t i;
  size_t k;
  size_t n;
  size_t seqSize;
  size_t start;

  vPep.clear();

  for(i=0;i<DBSize;i++){
    seqSize=vDB[i].sequence.size();
    start=0;
    n=0;
    k=0;
    mc=0;
    mass=18.0105633+fixMassPepN+fixMassProtN;
    if(vDB[i].sequence[0]=='M') next=0; //allow for next start site to be amino acid after initial M.
    else next = -1;

    pm.index=(int)i;
    bNTerm=false;
    bCTerm=false;
    if(adductSites['$'] || adductSites['%']) xlSites=1;
    else xlSites=0;

    while(true){

      bCutMarked=false;

      //Check if we cut n-terminal to this AA
      if(n>0 && enzyme.cutN[vDB[i].sequence[start+n]] && !enzyme.exceptC[vDB[i].sequence[start+n-1]]){
        if(next==-1) next=(int)start+(int)n-1;
        if(!bCutMarked) mc++;
        bCutMarked=true;

        //Add the peptide now (if enough mass and length)
        if ((mass+fixMassPepC)>min && n>=(minP-1)) addPeptide((int)i, (int)start, (int)n - 1, mass+fixMassPepC, p, vPep, bNTerm, bCTerm, xlSites);

      }

      //Add the peptide mass
      mass+=AA[vDB[i].sequence[start+n]];

      //Check if we cut c-terminal to this AA
      if((start+n+1)<seqSize && enzyme.cutC[vDB[i].sequence[start+n]] && !enzyme.exceptN[vDB[i].sequence[start+n+1]]){
        if(next==-1) next=(int)(start+n);
        if(!bCutMarked) mc++;
        bCutMarked=true;

        //Add the peptide now (if enough mass)
        if ((mass + fixMassPepC)>min && (mass + fixMassPepC)<max && n >= (minP - 1) ) addPeptide((int)i, (int)start, (int)n, mass + fixMassPepC, p, vPep, bNTerm, bCTerm, xlSites);

      }

      //Mark sites of adduct formation
      if(checkAA(i,start,n,seqSize,bNTerm,bCTerm)) xlSites++;

      //Check if we are at the end of the sequence
      if((start+n+1)==seqSize) {

        //Add the peptide now (if enough mass)
        if ((mass + fixMassPepC + fixMassProtC)>min && (mass + fixMassPepC + fixMassProtC)<max && n >= (minP - 1) )addPeptide((int)i, (int)start, (int)n, mass + fixMassPepC + fixMassProtC, p, vPep, bNTerm, bCTerm, xlSites);
        if(next>-1) {
          start=next+1;
          n=0;
          mc=0;
          mass=18.0105633+fixMassPepN;
          bNTerm = false;
          bCTerm = false;
          if (adductSites['$'] || adductSites['%']) xlSites = 1;
          else xlSites = 0;
          next=-1;
          continue;
        } else {
          break;
        }

      }

      //Check if we exceeded peptide mass
      //Check if we exceeded the number of missed cleavages
      //Check if we exceeded peptide length
      if((mass+fixMassPepC)>max || mc>mis || n>=(maxP-1)) {

        //if we know next cut site
        if(next>-1) {
          start=next+1;
          n=0;
          mc=0;
          mass=18.0105633+fixMassPepN;
          bNTerm = false;
          bCTerm = false;
          if (adductSites['$'] || adductSites['%']) xlSites = 1;
          else xlSites = 0;
          next=-1;

        //Otherwise, continue scanning until it is found
        } else {
          while((start+n)<seqSize-1){
            n++;
            if(n>0 && enzyme.cutN[vDB[i].sequence[start+n]] && !enzyme.exceptC[vDB[i].sequence[start+n-1]]){
              next=(int)(start+n);
              break;
            } else if((start+n+1)<seqSize && enzyme.cutC[vDB[i].sequence[start+n]] && !enzyme.exceptN[vDB[i].sequence[start+n+1]]){
              next=(int)(start+n);
              break;
            } 
          }
          if(next<0) break;

          start=next+1;
          n=0;
          mc=0;
          mass=18.0105633+fixMassPepN;
          bNTerm = false;
          bCTerm = false;
          if (adductSites['$'] || adductSites['%']) xlSites = 1;
          else xlSites = 0;
          next=-1;
        }  
      } else {
        n++;
        continue;
      }

    }

  }

  //merge duplicates
  mPepSort ps;
  ps.index=0;
  ps.sequence.clear();
  vector<mPepSort> vPS;
  for(i=0;i<vPep.size();i++){
    ps.index=(int)i;
    getPeptideSeq(vPep[i].map->at(0).index,vPep[i].map->at(0).start,vPep[i].map->at(0).stop,ps.sequence);
    vPS.push_back(ps);
  }
  //qsort(&vPS[0],vPS.size(),sizeof(kPepSort),compareSequence);
  sort(vPS.begin(),vPS.end(),compareSequenceB);

  vector<mPeptide> vtp;
  for(i=vPS.size()-1;i>0;i--){
    if(vPS[i].sequence.compare(vPS[i-1].sequence)==0){
      for(k=0;k<vPep[vPS[i].index].map->size();k++){
        vPep[vPS[i-1].index].map->push_back(vPep[vPS[i].index].map->at(k));
        if (vPep[vPS[i].index].cTerm) vPep[vPS[i - 1].index].cTerm = vPep[vPS[i].index].cTerm;
        if (vPep[vPS[i].index].nTerm) vPep[vPS[i - 1].index].nTerm = vPep[vPS[i].index].nTerm;
        if (vPep[vPS[i].index].xlSites>vPep[vPS[i - 1].index].xlSites) vPep[vPS[i - 1].index].xlSites = vPep[vPS[i].index].xlSites;
      }
      vPep[vPS[i].index].mass=-1;
    }
  }
  for(i=0;i<vPep.size();i++){
    if(vPep[i].mass>0) vtp.push_back(vPep[i]);
  }
  vPep.clear();
  n=0;
  for(i=0;i<vtp.size();i++) {
    if (vtp[i].xlSites>0)n++;
    vPep.push_back(vtp[i]);
  }

  //For determining boundaries of E-value precalculations based on peptide length
  for(i=0;i<vPep.size();i++){
    if(vPep[i].mass-1<minMass[vPep[i].map->at(0).stop-vPep[i].map->at(0).start+1]){
      minMass[vPep[i].map->at(0).stop - vPep[i].map->at(0).start + 1] = vPep[i].mass-1;
    }
  }
  for(i=0;i<100;i++){
    if(i<99 && minMass[i+1]<minMass[i]) minMass[i]=minMass[i+1];
    //cout << i << "\t" << minMass[i] << endl;
  }

  cout << "  " << vPep.size() << " peptides to search (" << n << " with binding sites)." << endl;
  qsort(&vPep[0],vPep.size(),sizeof(mPeptide),compareMass);
  adductPepCount=(int)n;

  //Reporting list
  //for (i = 0; i<vPep.size(); i++){
  //  char str[256];
  //  getPeptideSeq(vPep[i].map->at(0).index, vPep[i].map->at(0).start, vPep[i].map->at(0).stop, str);
  //  cout << i << ", " << vPep[i].mass << "\t" << str << "\t" << strlen(str) << endl;
  //}

  return true;

}

void MDatabase::exportDB(string fName) {
  size_t i;
  FILE* f = fopen(fName.c_str(), "wt");
  for (i = 0; i < vDB.size(); i++) {
    fprintf(f, ">%s\n", vDB[i].name.c_str());
    fprintf(f, "%s\n", vDB[i].sequence.c_str());
  }
  fclose(f);
}

//==============================
//  Accessors & Modifiers
//==============================
void MDatabase::addFixedMod(char mod, double mass){
  if (mod == 'n') fixMassPepN=mass;
  else if(mod=='c') fixMassPepC=mass;
  else if(mod=='$') fixMassProtN=mass;
  else if(mod=='%') fixMassProtC=mass;
  else AA[mod]+=mass;
}

mDB& MDatabase::at(const int& i){
  return vDB[i];
}

mEnzymeRules& MDatabase::getEnzymeRules(){
  return enzyme;
}

int MDatabase::getMaxPepLen(double mass){
  for(int a=60;a>6;a--){
    if(mass>minMass[a]) return a;
  }
  return 6;
}

mPeptide& MDatabase::getPeptide(int index){
  return vPep[index];
}

vector<mPeptide>* MDatabase::getPeptideList(){
  return &vPep;
}

int MDatabase::getPeptideListSize(){
  return (int)vPep.size();
}

bool MDatabase::getPeptideSeq(int index, int start, int stop, char* str){
  if ((size_t)index>vDB.size()) return false;
  string str1=vDB[index].sequence.substr(start,stop-start+1);
  strcpy(str,&str1[0]);
  return true;
}

bool MDatabase::getPeptideSeq(int index, int start, int stop, string& str){
  if((size_t)index>vDB.size()) return false;
  str=vDB[index].sequence.substr(start,stop-start+1);
  return true;
}

bool MDatabase::getPeptideSeq(mPeptide& p, string& str){
  str=vDB[p.map->at(0).index].sequence.substr(p.map->at(0).start,p.map->at(0).stop-p.map->at(0).start+1);
  return true;
}

bool MDatabase::getPeptideSeq(int pepIndex, string& str){
  if((size_t)pepIndex>vPep.size()) return false;
  mPeptide p = vPep[(size_t)pepIndex];
  str = vDB[p.map->at(0).index].sequence.substr(p.map->at(0).start, p.map->at(0).stop - p.map->at(0).start + 1);
  return true;
}

int MDatabase::getProteinDBSize(){
  return (int)vDB.size();
}

void MDatabase::setAAMass(char aa, double mass){
  AA[aa] = mass;
}

bool MDatabase::setEnzyme(const char* str){
  bool stateNTerm=false;
  int stateRule=0;

  unsigned int i;
  for(i=0;i<128;i++){
    enzyme.cutC[i]=enzyme.cutN[i]=enzyme.exceptC[i]=enzyme.exceptN[i]=false;
  }

  for(i=0;i<strlen(str);i++){
    switch(str[i]){
      case '[':
        if(stateRule>0){
          cout << "Error in enzyme string: " << str << endl;
          return false;
        }
        stateRule=1;
        break;
      case ']':
        if(stateRule!=1){
          cout << "Error in enzyme string: " << str << endl;
          return false;
        }
        stateRule=0;
        break;
      case '{':
        if(stateRule>0){
          cout << "Error in enzyme string: " << str << endl;
          return false;
        }
        stateRule=2;
        break;
      case '}':
        if(stateRule!=2){
          cout << "Error in enzyme string: " << str << endl;
          return false;
        }
        stateRule=0;
        break;
      case '|':
        if(stateNTerm) {
          cout << "Error in enzyme string: " << str << endl;
          return false;
        }
        stateNTerm=true;
        break;
      default:
        if(stateRule==0){
          cout << "Error in enzyme string: " << str << endl;
          return false;
        }
        if(stateNTerm){
          if(stateRule==1) enzyme.cutN[str[i]]=true;
          else enzyme.exceptN[str[i]]=true;
        } else {
          if(stateRule==1) enzyme.cutC[str[i]]=true;
          else enzyme.exceptC[str[i]]=true;
        }
        break;
    }
  }

  //for(i=65;i<90;i++){
  //  cout << (char)i << "\t" << enzyme.cutC[i] << "\t" << enzyme.cutN[i] << "\t" << enzyme.exceptC[i] << "\t" << enzyme.exceptN[i] << endl;
  //}

  return true;

}

void MDatabase::setLog(MLog* c){
  mlog = c;
}

void MDatabase::setAdductSites(bool* arr){
  for (int i = 0; i < 128; i++) adductSites[i]=arr[i];
}

//==============================
//  Private Functions
//==============================
void MDatabase::addPeptide(int index, int start, int len, double mass, mPeptide& p, vector<mPeptide>& vP, bool bN, bool bC, char xlSites){
  mPepMap  pm;
                           
  pm.index=index;
  pm.start=start;
  pm.stop=start+len;
  
  p.nTerm=bN;
  p.cTerm=bC;
  p.xlSites=xlSites;
  p.mass=mass;
  p.map->clear();
  p.map->push_back(pm);
  vP.push_back(p);

  //char str[256];
  //getPeptideSeq(p.map->at(0).index,p.map->at(0).start,p.map->at(0).stop,str);
  //cout << "Adding: " << str << "\t" << p.vA->size()+p.vB->size() << endl;

}

bool MDatabase::checkAA(size_t i, size_t start, size_t n, size_t seqSize, bool& bN, bool& bC){
  if (start + n == 0){
    bN=true;
    if (adductSites['n']==true) return true;
  }
  if (start + n == 1 && start == 1) {
    bN=true;
    if (adductSites['n']==true) return true;
  }
  if (start + n == seqSize-1) {
    bC=true;
    if (adductSites['c']==true) return true;
  }
  if (adductSites[vDB[i].sequence[start + n]]==true) return true;
  return false;
}

//==============================
//  Utility Functions
//==============================
int MDatabase::compareMass(const void *p1, const void *p2){ //sort high to low
  const mPeptide d1 = *(mPeptide *)p1;
  const mPeptide d2 = *(mPeptide *)p2;
  if(d1.mass<d2.mass) {
		return 1;
	} else if(d1.mass>d2.mass) {
  	return -1;
  } else {
	  return 0;
  }
}

int MDatabase::compareSequence(const void *p1, const void *p2){
  const mPepSort d1 = *(mPepSort *)p1;
  const mPepSort d2 = *(mPepSort *)p2;
  return d1.sequence.compare(d2.sequence);
}

bool MDatabase::compareSequenceB(const mPepSort& p1, const mPepSort& p2){
  return p1.sequence.compare(p2.sequence)<0;
}


