//=============================================
// Author: Chris McGinn
// 
// DiJet Initial Skim Class (MC)
//
// !!NOTE: Written for jets sorted by pt, tracks unsorted!!
//
//=============================================

#include "TFile.h"
#include "TTree.h"
#include "/net/hisrv0001/home/cfmcginn/emDiJet/CMSSW_5_3_12_patch3/tempHIFA/HiForestAnalysis/hiForest.h"
#include "commonUtility.h"
#include "cfmDiJetIniSkim.h"
#include "stdlib.h"
#include <iostream>
#include <fstream>

#include "TComplex.h"

const Float_t leadJtPtCut = 120.;
const Float_t subLeadJtPtCut = 50.;
const Float_t jtDelPhiCut = 0;
const Float_t jtEtaCut = 2.0; // Default Max at 2.4 to avoid transition junk, otherwise vary as needed

collisionType getCType(sampleType sType);

Bool_t passesDijet(Jets jtCollection)
{
  Int_t leadJtIndex = -1;
  Int_t subLeadJtIndex = -1;

  for(Int_t jtEntry = 0; jtEntry < jtCollection.nref; jtEntry++){
    if(leadJtIndex < 0){
      if(jtCollection.jtpt[jtEntry] > leadJtPtCut){
	if(TMath::Abs(jtCollection.jteta[jtEntry]) < jtEtaCut)
	  leadJtIndex = jtEntry;
      }
      else 
	return false;
    }
    else if(subLeadJtIndex < 0){
      if(jtCollection.jtpt[jtEntry] > subLeadJtPtCut){
	if(TMath::Abs(jtCollection.jteta[jtEntry]) < jtEtaCut)
	  subLeadJtIndex = jtEntry;
      }
      else
	return false;
    }
    else
      return true;
  }

  return false;
}


int makeDiJetIniSkim(string fList = "", sampleType sType = kHIDATA, const char *outName = "defaultName_CFMSKIM.root", Int_t num = 0)
{
  //Define MC or Data
  bool montecarlo = false;
  if(sType == kPPMC || sType == kPAMC || sType == kHIMC)
    montecarlo = true;

  std::cout << sType << std::endl;
  std::cout << montecarlo << std::endl;

  collisionType cType = getCType(sType);

  string buffer;
  std::vector<string> listOfFiles;
  int nLines = 0;
  ifstream inFile(fList.data());

  std::cout << fList << std::endl;
  std::cout << inFile.is_open() << std::endl;

  if(!inFile.is_open()){
    std::cout << "Error opening file. Exiting." <<std::endl;
    return 1;
  }
  else{
    while(!inFile.eof()){
      inFile >> buffer;
      listOfFiles.push_back(buffer);
      nLines++;
    }
  }

  std::cout << "FileList Loaded" << std::endl;

  //Setup correction tables

  TFile *outFile = new TFile(Form("%s_%d.root", outName, num), "RECREATE");

  InitDiJetIniSkim(montecarlo);

  HiForest *c = new HiForest(listOfFiles[0].data(), "Forest", cType, montecarlo);

  c->InitTree();

  c->LoadNoTrees();

  c->hasSkimTree = true;
  c->hasTrackTree = true;
  c->hasEvtTree = true;
  c->hasAkPu3JetTree = true;
  c->hasAkPu3CaloJetTree = true;
  c->hasAkVs3CaloJetTree = true;

  Float_t meanVz = 0;

  if(montecarlo){
    c->hasGenParticleTree = true;
    //mean mc .16458, mean data -.337
    meanVz = .16458 + .337;
  }

  Long64_t nentries = c->GetEntries();

  std::cout << nentries << std::endl;

  Int_t totEv = 0;
  Int_t selectCut = 0;
  Int_t vzCut = 0;

  std::cout << "Cuts, Lead/Sublead Pt, delphi, eta: " << leadJtPtCut << ", " << subLeadJtPtCut << ", " << jtDelPhiCut << ", " << jtEtaCut << std::endl; 

  for(Long64_t jentry = 0; jentry < 100000; jentry++){
    c->GetEntry(jentry);

    totEv++;

    if(jentry%1000 == 0)
      std::cout << jentry << std::endl;

    if(!c->selectEvent()){
      selectCut++;
      continue;
    }

    if(TMath::Abs(c->evt.vz - meanVz) > 15){
      vzCut++;
      continue;
    }

    InitJetVar(montecarlo);

    //particle flow

    Jets AlgJtCollection[2] = {c->akPu3Calo, c->akVs3Calo};

    Bool_t algPasses[3] = {false, false, false};

    for(Int_t algIter = 0; algIter < 2; algIter++){
      algPasses[algIter] = passesDijet(AlgJtCollection[algIter]);
    }

    //truth, doesn't work w/ getLeadJt because truth doesnt get its own tree

    if(montecarlo){
      Int_t leadJtIndex = -1;
      Int_t subLeadJtIndex = -1;

      for(Int_t jtEntry = 0; jtEntry < c->akPu3PF.ngen; jtEntry++){
	if(leadJtIndex < 0){
	  if(c->akPu3PF.genpt[jtEntry] > leadJtPtCut){
	    if(TMath::Abs(c->akPu3PF.geneta[jtEntry]) < jtEtaCut)
	      leadJtIndex = jtEntry;
	  }
	  else{
	    algPasses[2] = false;
	    break;
	  }
	}
	else if(subLeadJtIndex < 0){
	  if(c->akPu3PF.genpt[jtEntry] > subLeadJtPtCut){
	    if(TMath::Abs(c->akPu3PF.geneta[jtEntry]) < jtEtaCut)
	      subLeadJtIndex = jtEntry;
	  }
	  else{
	    algPasses[2] = false;
	    break;
	  }
	}
	else{
	  algPasses[2] = true;
	  break;
	}
      }
    }
    
    if(algPasses[0] == false && algPasses[1] == false && algPasses[2] == false)
      continue;

    if(montecarlo){
      pthat_ = c->akPu3PF.pthat;
    }

    run_ = c->evt.run;
    evt_ = c->akPu3PF.evt;
    lumi_ = c->evt.lumi;
    hiBin_ = c->evt.hiBin;

    //Iterate over jets

    nPu3Calo_ = 0;
    nVs3Calo_ = 0;
    nT3_ = 0;

    for(Int_t Pu3CaloIter = 0; Pu3CaloIter < c->akPu3Calo.nref; Pu3CaloIter++){
      if(c->akPu3Calo.jtpt[Pu3CaloIter] < subLeadJtPtCut)
	break;
      else if(TMath::Abs(c->akPu3Calo.jteta[Pu3CaloIter]) > jtEtaCut)
	continue;

      Pu3CaloPt_[nPu3Calo_] = c->akPu3Calo.jtpt[Pu3CaloIter];
      Pu3CaloPhi_[nPu3Calo_] = c->akPu3Calo.jtphi[Pu3CaloIter];
      Pu3CaloEta_[nPu3Calo_] = c->akPu3Calo.jteta[Pu3CaloIter];

      nPu3Calo_++;
    }

    for(Int_t Vs3CaloIter = 0; Vs3CaloIter < c->akVs3Calo.nref; Vs3CaloIter++){
      if(c->akVs3Calo.jtpt[Vs3CaloIter] < subLeadJtPtCut)
	break;
      else if(TMath::Abs(c->akVs3Calo.jteta[Vs3CaloIter]) > jtEtaCut)
	continue;

      Vs3CaloPt_[nVs3Calo_] = c->akVs3Calo.jtpt[Vs3CaloIter];
      Vs3CaloPhi_[nVs3Calo_] = c->akVs3Calo.jtphi[Vs3CaloIter];
      Vs3CaloEta_[nVs3Calo_] = c->akVs3Calo.jteta[Vs3CaloIter];

      nVs3Calo_++;
    }

    for(Int_t T3Iter = 0; T3Iter < c->akPu3PF.ngen; T3Iter++){
      if(c->akPu3PF.genpt[T3Iter] < subLeadJtPtCut)
	break;
      else if(TMath::Abs(c->akPu3PF.geneta[T3Iter]) > jtEtaCut)
	continue;

      T3Pt_[nT3_] = c->akPu3PF.genpt[T3Iter];
      T3Phi_[nT3_] = c->akPu3PF.genphi[T3Iter];
      T3Eta_[nT3_] = c->akPu3PF.geneta[T3Iter];

      nT3_++;
    }

    //Iterate over tracks

    nTrk_ = 0;

    Tracks trkCollection;
    trkCollection = c->track;

    for(Int_t trkEntry = 0; trkEntry < trkCollection.nTrk; trkEntry++){

      if(TMath::Abs(trkCollection.trkEta[trkEntry]) > 2.4)
	continue;

      if(trkCollection.trkPt[trkEntry] <= 0.5)
	continue;

      if(!trkCollection.highPurity[trkEntry])
	continue;

      if(TMath::Abs(trkCollection.trkDz1[trkEntry]/trkCollection.trkDzError1[trkEntry]) > 3)
	continue;

      if(TMath::Abs(trkCollection.trkDxy1[trkEntry]/trkCollection.trkDxyError1[trkEntry]) > 3)
	continue;

      if(trkCollection.trkPtError[trkEntry]/trkCollection.trkPt[trkEntry] > 0.1)
	continue;

      trkPt_[nTrk_] = trkCollection.trkPt[trkEntry];
      trkPhi_[nTrk_] = trkCollection.trkPhi[trkEntry];
      trkEta_[nTrk_] = trkCollection.trkEta[trkEntry];
      
      //Grab proj. Pt Spectra For Tracks in each Event Subset    
      
      nTrk_++;
      if(nTrk_ > MAXTRKS - 1){
	printf("ERROR: Trk arrays not large enough.\n");
	return(1);
      }
    }

    if(montecarlo){
      //Iterate over truth
      nGen_ = 0;

      GenParticles genCollection;
      genCollection = c->genparticle;

      for(Int_t genEntry = 0; genEntry < genCollection.mult; genEntry++){
	if(genCollection.chg[genEntry] == 0)
	  continue;
  
	if(TMath::Abs(genCollection.eta[genEntry]) > 2.4)
	  continue;

	if(genCollection.pt[genEntry] < 0.5)
	  continue;

	genPt_[nGen_] = genCollection.pt[genEntry];
	genPhi_[nGen_] = genCollection.phi[genEntry];
	genEta_[nGen_] = genCollection.eta[genEntry];

	nGen_++;
	if(nGen_ > MAXGEN - 1){
	  printf("ERROR: Gen arrays not large enough.\n");
	  return(1);
	}
      }
    }

    jetTreeIni_p->Fill();
    trackTreeIni_p->Fill();

    if(montecarlo)
      genTreeIni_p->Fill();
  }

  std::cout << "totEv: " << totEv << std::endl;
  Int_t tempTot = totEv - selectCut;
  std::cout << "selectCut: " << tempTot << std::endl;
  tempTot = tempTot - vzCut;
  std::cout << "vzCut: " << tempTot << std::endl;

  outFile->cd();
  /*
  jetTreeIni_p->Write("", TObject::kOverwrite);
  trackTreeIni_p->Write("", TObject::kOverwrite);

  if(montecarlo)
    genTreeIni_p->Write("", TObject::kOverwrite);
  */

  outFile->Write();

  outFile->Close();

  delete c;
  delete outFile;

  printf("Done.\n");
  return(0);
}


collisionType getCType(sampleType sType)
{
  switch (sType)
    {
    case kPPDATA:
    case kPPMC:
      return cPP;
    case kPADATA:
    case kPAMC:
      return cPPb;
    case kHIDATA:
    case kHIMC:
      return cPbPb;
    }
  return cPbPb; //probably a bad guess
}


int main(int argc, char *argv[])
{
  if(argc != 5)
    {
      std::cout << "Usage: sortForest <inputFile> <MCBool> <outputFile> <#>" << std::endl;
      return 1;
    }

  int rStatus = -1;

  rStatus = makeDiJetIniSkim(argv[1], sampleType(atoi(argv[2])), argv[3], atoi(argv[4]));

  return rStatus;
}