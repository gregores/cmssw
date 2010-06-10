//  \class MuScleFit
//  Fitter of momentum scale and resolution from resonance decays to muon track pairs
//
//  $Date: 2010/05/28 08:48:45 $
//  $Revision: 1.85 $
//  \author R. Bellan, C.Mariotti, S.Bolognesi - INFN Torino / T.Dorigo, M.De Mattia - INFN Padova
//
//  Recent additions:
//  - several parameters allow a more flexible use, tests, and control handles
//    for the likelihood. In particular, a set of integers controls the order
//    with which parameters are released; another controls which parameters are
//    fixed. A function allows to smear momenta and angles of muons from the
//    resonance before any correction, using a set of random numbers generated
//    once and for all in the constructor (this way the smearing remains the same
//    for any given muon no matter how many times one loops and what corrections
//    one applies).
//    For a correct use of these flags, please see the function minimizeLikelihood() in
//    MuScleFitUtils.cc
//  - the fit now allows to extract resolution functions simultaneously with the
//    momentum scale. So far a simple parametrization
//    of muon momentum resolution and angle resolution has been implemented, but
//    extensions are straightforward.
//  - It is however advisable to fit separately resolution and scale. The suggested
//    course of action is:
//    1) fit the scale with a simple parametrization
//    2) check results, fit with more complicated forms
//    3) verify which is a sufficiently accurate description of the data
//    4) fix scale parameters and fit for the resolution
//    5) go back to fitting the scale with resolution parameters fixed to fitted values
//  - Also note that resolution fits may fail to converge due to instability of the
//    probability distribution to the limit of large widths. Improvements here are
//    advisable.
//  - The treatment of signal windows in the Y region
//    has to be refined because of overlaps. More work is needed here, assigning a different
//    weight to different hypothesis of the resonance producing a given mass, if there are
//    multiple candidates.
//  - Also, larger windows are to be allowed for fits to SA muons.
//  - File Probs_1000.root contains the probability distribution of lorentzians convoluted
//    with gaussian smearing functions, for the six resonances. A 1000x1000 grid
//    in mass,sigma has been computed (using root macro Probs.C).
//    A wider interval of masses for each resonance should be computed, to be used for standalone muons
//
//
//  Notes on additions, TD 31/3/08
//
//  - background model: at least a couple of different models, with two parameters,
//    should be included in the fitting procedure such that the function massprob(),
//    which produces the probability in the likelihood computation, incorporates the
//    probability that the event is from background. That is, if the fitting function
//    knows the shape of the mass spectrum ( say, a falling exponential plus a gaussian
//    signal) it becomes possible to fit the scale together with the background shape
//    and normalization parameters. Of course, one should do one thing at a time: first
//    a scale fit, then a shape fit with scale parameters fixed, and then a combined
//    fit. Convergence issues should be handled case by case.
//  - The correct implementation of the above idea requires a reorganization of pass
//    parameters (in the cfg) and fit parameters. The user has to be able to smear,
//    bias, fix parameters, choose scale fitting functions, resolution fitting functions,
//    and background functions. It should be possible to separate the fit functions from
//    the biasing ones, which would allow a more thorough testing.
//  - all the above can be obtained by making the .cfg instructions heavier. Since this
//    is a routine operated by experts only, it is a sensible choice.
//  - One should thus envision the following:
//      1) a set of parameters controlling the biasing function: parBias()
//      2) a set of parameters controlling the smearing function: parSmear()
//      3) a set of parameters to define resolution modeling and initial values: parResol()
//      3b) parResol() gets fix and order bits by parResolFix() and parResolOrder()
//      4) a set of parameters to define scale modeling and initial values: parScale()
//      4b) parScale() gets fix and order bits by parScaleFix() and parScaleOrder()
//      5) a set of parameters controlling the background shape and normalization: parNorm()
//      5b) parNorm() gets fix and order bits by parNormFix() and parNormOrder()
//    The likelihood parameters then become a vector which is dynamically composed of
//    sets 3), 4), and 5): parval() = parResol()+parScale()+parNorm()
//  - In order to study better the likelihood behavior it would be advisable to introduce
//    some histogram filling on the last iteration of the likelihood. It is not clear
//    how best to achieve that: probably the simplest way is to make a histogram filling
//    function run just after the likelihood computation, such that the best value of the
//    fit parameters is used.
//  - The muon pair which we call our resonance must be chosen in a way which does not
//    bias our likelihood: we cannot just choose the pair closest to a resonance.
//
//
//    Notes on additions, T.Dorigo 22/12/2008
//    ---------------------------------------
//
//  - File Probs_new_1000_CTEQ.root now contains a set of 24 additional two-dim histograms,
//    defining the probability distribution of Z boson decays as a function of measured mass
//    and expected sigma in 24 different bins of Z rapidity, extracted from CTEQ 6 PDF (at
//    Leading Order) from the convolution in the factorization integral. See programs CTEQ.cpp
//    and Fits.C.
//  - The probability for Z boson events now thus depends on the measured rapidity of the dimuon
//    system. All functions in file MuScleFitUtils.cc have been suitably changed.
//
// ----------------------------------------------------------------------------------
//    Modifications by M. De Mattia 13/3/2009
//    ---------------------------------------
//  - The histograms map was moved to a base class (MuScleFitBase) from which this one inherits.
//
//    Modifications by M. De Mattia 20/7/2009
//    ---------------------------------------
//  - Reworked background fit based on ranges. See comments in the code for more details.
// ---------------------------------------------------------------------------------------------

#include "MuScleFit.h"
#include "MuonAnalysis/MomentumScaleCalibration/interface/Histograms.h"
#include "MuonAnalysis/MomentumScaleCalibration/interface/MuScleFitPlotter.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "HepPDT/defs.h"
#include "HepPDT/TableBuilder.hh"
#include "HepPDT/ParticleDataTable.hh"

#include "HepMC/GenParticle.h"
#include "HepMC/GenEvent.h"
// #include "SimDataFormats/HepMCProduct/interface/HepMCProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"

#include "SimGeneral/HepPDTRecord/interface/ParticleDataTable.h"
#include "SimDataFormats/Track/interface/SimTrackContainer.h"
#include "SimDataFormats/Vertex/interface/SimVertexContainer.h"

#include "TFile.h"
#include "TTree.h"
#include "TMinuit.h"
#include <vector>

#include "DataFormats/Common/interface/TriggerResults.h"
#include "HLTrigger/HLTcore/interface/HLTConfigProvider.h"

// To use callgrind for code profiling uncomment also the following define.
// #define USE_CALLGRIND

#ifdef USE_CALLGRIND
#include "valgrind/callgrind.h"
#endif

#include "MuonAnalysis/MomentumScaleCalibration/interface/Functions.h"
#include "MuonAnalysis/MomentumScaleCalibration/interface/RootTreeHandler.h"

// To read likelihood distributions from the database.
	//#include "CondFormats/RecoMuonObjects/interface/MuScleFitLikelihoodPdf.h"
	//#include "CondFormats/DataRecord/interface/MuScleFitLikelihoodPdfRcd.h"

// Constructor
// -----------
MuScleFit::MuScleFit( const edm::ParameterSet& pset ) : MuScleFitBase( pset ), totalEvents_(0)
{
  MuScleFitUtils::debug = debug_;
  if (debug_>0) std::cout << "[MuScleFit]: Constructor" << std::endl;

  // Service parameters
  // ------------------
  edm::ParameterSet serviceParameters = pset.getParameter<edm::ParameterSet>("ServiceParameters");
  theService = new MuonServiceProxy(serviceParameters);

  if ((theMuonType_<-4 || theMuonType_>5) && theMuonType_!=10) {
    std::cout << "[MuScleFit]: Unknown muon type! Aborting." << std::endl;
    abort();
  }

  loopCounter = 0;

  // Boundaries for h-function computation (to be improved!)
  // -------------------------------------------------------
  minResMass_hwindow[0] = 76.;
  maxResMass_hwindow[0] = 106.;
  minResMass_hwindow[1] = 10.15;
  maxResMass_hwindow[1] = 10.55;
  minResMass_hwindow[2] = 9.8;
  maxResMass_hwindow[2] = 10.2;
  minResMass_hwindow[3] = 9.25;
  maxResMass_hwindow[3] = 9.65;
  minResMass_hwindow[4] = 3.58;
  maxResMass_hwindow[4] = 3.78;
  minResMass_hwindow[5] = 3.0;
  maxResMass_hwindow[5] = 3.2;

  // Max number of loops (if > 2 then try to minimize likelihood more than once)
  // ---------------------------------------------------------------------------
  maxLoopNumber = pset.getUntrackedParameter<int>("maxLoopNumber", 2);
  fastLoop = pset.getUntrackedParameter<bool>("FastLoop", true);

  // Selection of fits according to loop
  MuScleFitUtils::doResolFit = pset.getParameter<std::vector<int> >("doResolFit");
  MuScleFitUtils::doScaleFit = pset.getParameter<std::vector<int> >("doScaleFit");
  MuScleFitUtils::doCrossSectionFit = pset.getParameter<std::vector<int> >("doCrossSectionFit");
  MuScleFitUtils::doBackgroundFit = pset.getParameter<std::vector<int> >("doBackgroundFit");

  // Bias and smear types
  // --------------------
  int biasType = pset.getParameter<int>("BiasType");
  MuScleFitUtils::BiasType = biasType;
  // No error, the scale functions are used also for the bias
  MuScleFitUtils::biasFunction = scaleFunctionVecService( biasType );
  int smearType = pset.getParameter<int>("SmearType");
  MuScleFitUtils::SmearType = smearType;
  MuScleFitUtils::smearFunction = smearFunctionService( smearType );

  // Fit types
  // ---------
  int resolFitType = pset.getParameter<int>("ResolFitType");
  MuScleFitUtils::ResolFitType = resolFitType;
  MuScleFitUtils::resolutionFunction = resolutionFunctionService( resolFitType );
  MuScleFitUtils::resolutionFunctionForVec = resolutionFunctionVecService( resolFitType );
  int scaleType = pset.getParameter<int>("ScaleFitType");
  MuScleFitUtils::ScaleFitType = scaleType;
  MuScleFitUtils::scaleFunction = scaleFunctionService( scaleType );
  MuScleFitUtils::scaleFunctionForVec = scaleFunctionVecService( scaleType );

  // Initial parameters values
  // -------------------------
  MuScleFitUtils::parBias         = pset.getParameter<std::vector<double> >("parBias");
  MuScleFitUtils::parSmear        = pset.getParameter<std::vector<double> >("parSmear");
  MuScleFitUtils::parResol        = pset.getParameter<std::vector<double> >("parResol");
  MuScleFitUtils::parScale        = pset.getParameter<std::vector<double> >("parScale");
  MuScleFitUtils::parCrossSection = pset.getParameter<std::vector<double> >("parCrossSection");
  MuScleFitUtils::parBgr          = pset.getParameter<std::vector<double> >("parBgr");
  MuScleFitUtils::parResolFix        = pset.getParameter<std::vector<int> >("parResolFix");
  MuScleFitUtils::parScaleFix        = pset.getParameter<std::vector<int> >("parScaleFix");
  MuScleFitUtils::parCrossSectionFix = pset.getParameter<std::vector<int> >("parCrossSectionFix");
  MuScleFitUtils::parBgrFix          = pset.getParameter<std::vector<int> >("parBgrFix");
  MuScleFitUtils::parResolOrder        = pset.getParameter<std::vector<int> >("parResolOrder");
  MuScleFitUtils::parScaleOrder        = pset.getParameter<std::vector<int> >("parScaleOrder");
  MuScleFitUtils::parCrossSectionOrder = pset.getParameter<std::vector<int> >("parCrossSectionOrder");
  MuScleFitUtils::parBgrOrder          = pset.getParameter<std::vector<int> >("parBgrOrder");

  MuScleFitUtils::resfind     = pset.getParameter<std::vector<int> >("resfind");
  MuScleFitUtils::FitStrategy = pset.getParameter<int>("FitStrategy");

  // Option to skip unnecessary stuff
  // --------------------------------
  MuScleFitUtils::speedup = pset.getParameter<bool>("speedup");

  // Option to skip simTracks comparison
  compareToSimTracks_ = pset.getParameter<bool>("compareToSimTracks");
  simTracksCollection_ = pset.getUntrackedParameter<edm::InputTag>("SimTracksCollection", edm::InputTag("g4SimHits"));

  triggerResultsLabel_ = pset.getUntrackedParameter<std::string>("TriggerResultsLabel");
  triggerResultsProcess_ = pset.getUntrackedParameter<std::string>("TriggerResultsProcess");
  triggerPath_ = pset.getUntrackedParameter<std::string>("TriggerPath");
  negateTrigger_ = pset.getUntrackedParameter<bool>("NegateTrigger", false);

  PATmuons_ = pset.getUntrackedParameter<bool>("PATmuons", false);
  genParticlesName_ = pset.getUntrackedParameter<std::string>("GenParticlesName", "genParticles");

  // This must be set to true if using events generated with Sherpa
  MuScleFitUtils::sherpa_ = pset.getUntrackedParameter<bool>("Sherpa", false);

  MuScleFitUtils::rapidityBinsForZ_ = pset.getUntrackedParameter<bool>("RapidityBinsForZ", true);

  // Set the cuts on muons to be used in the fit
  MuScleFitUtils::maxMuonPt_ = pset.getUntrackedParameter<double>("MaxMuonPt", 100000000.);
  MuScleFitUtils::minMuonPt_ = pset.getUntrackedParameter<double>("MinMuonPt", 0.);
  MuScleFitUtils::minMuonEtaFirstRange_ = pset.getUntrackedParameter<double>("MinMuonEtaFirstRange", -6.);
  MuScleFitUtils::maxMuonEtaFirstRange_ = pset.getUntrackedParameter<double>("MaxMuonEtaFirstRange", 6.);
  MuScleFitUtils::minMuonEtaSecondRange_ = pset.getUntrackedParameter<double>("MinMuonEtaSecondRange", -100.);
  MuScleFitUtils::maxMuonEtaSecondRange_ = pset.getUntrackedParameter<double>("MaxMuonEtaSecondRange", 100.);

  MuScleFitUtils::debugMassResol_ = pset.getUntrackedParameter<bool>("DebugMassResol", false);
  // MuScleFitUtils::massResolComponentsStruct MuScleFitUtils::massResolComponents;

  // Read the Probs file from database. If false it searches the root file in
  // MuonAnalysis/MomentumScaleCalibration/test of the active release.
  // readPdfFromDB = pset.getParameter<bool>("readPdfFromDB");

  // Check for parameters consistency
  // it will abort in case of errors.

 checkParameters();

  // Generate array of gaussian-distributed numbers for smearing
  // -----------------------------------------------------------
  if (MuScleFitUtils::SmearType>0) {
    std::cout << "[MuScleFit-Constructor]: Generating random values for smearing" << std::endl;
    TF1 G("G", "[0]*exp(-0.5*pow(x,2))", -5., 5.);
    double norm = 1/sqrt(2*TMath::Pi());
    G.SetParameter (0,norm);
    for (int i=0; i<10000; i++) {
      for (int j=0; j<7; j++) {
	MuScleFitUtils::x[j][i] = G.GetRandom();
      }
    }
  }
  MuScleFitUtils::goodmuon = 0;

  if(theMuonType_ >0 &&  theMuonType_<4) {
    MuScleFitUtils::MuonTypeForCheckMassWindow = theMuonType_-1;
    MuScleFitUtils::MuonType = theMuonType_-1;
  }
  else if(theMuonType_ == 4 || theMuonType_ ==10 || theMuonType_==-1 || theMuonType_==-2 || theMuonType_==-3 || theMuonType_==-4) {
    MuScleFitUtils::MuonTypeForCheckMassWindow = 2;
    MuScleFitUtils::MuonType = 2;
  }
  else{
    std::cout<<"Wrong muon type "<<theMuonType_<<std::endl;
    exit(1);
  }


  // Initialize ResMaxSigma And ResHalfWidth - 0 = global, 1 = SM, 2 = tracker
  // -------------------------------------------------------------------------
  MuScleFitUtils::massWindowHalfWidth[0][0] = 20.;
  MuScleFitUtils::massWindowHalfWidth[0][1] = 0.5;
  MuScleFitUtils::massWindowHalfWidth[0][2] = 0.5;
  MuScleFitUtils::massWindowHalfWidth[0][3] = 0.5;
  MuScleFitUtils::massWindowHalfWidth[0][4] = 0.2;
  MuScleFitUtils::massWindowHalfWidth[0][5] = 0.2;
  MuScleFitUtils::massWindowHalfWidth[1][0] = 50.;
  MuScleFitUtils::massWindowHalfWidth[1][1] = 2.5;
  MuScleFitUtils::massWindowHalfWidth[1][2] = 2.5;
  MuScleFitUtils::massWindowHalfWidth[1][3] = 2.5;
  MuScleFitUtils::massWindowHalfWidth[1][4] = 1.5;
  MuScleFitUtils::massWindowHalfWidth[1][5] = 1.5;
  MuScleFitUtils::massWindowHalfWidth[2][0] = 20.;
  MuScleFitUtils::massWindowHalfWidth[2][1] = 0.5;
  MuScleFitUtils::massWindowHalfWidth[2][2] = 0.5;
  MuScleFitUtils::massWindowHalfWidth[2][3] = 0.5;
  MuScleFitUtils::massWindowHalfWidth[2][4] = 0.2;
  MuScleFitUtils::massWindowHalfWidth[2][5] = 0.2;

  MuScleFitUtils::backgroundHandler = new BackgroundHandler( pset.getParameter<std::vector<int> >("BgrFitType"),
                                                             pset.getParameter<std::vector<double> >("LeftWindowBorder"),
                                                             pset.getParameter<std::vector<double> >("RightWindowBorder"),
                                                             MuScleFitUtils::ResMass,
                                                             MuScleFitUtils::massWindowHalfWidth[MuScleFitUtils::MuonTypeForCheckMassWindow] );

  MuScleFitUtils::crossSectionHandler = new CrossSectionHandler( MuScleFitUtils::parCrossSection, MuScleFitUtils::resfind );

  // Build cross section scale factors
  // MuScleFitUtils::resfind

  MuScleFitUtils::normalizeLikelihoodByEventNumber_ = pset.getUntrackedParameter<bool>("NormalizeLikelihoodByEventNumber", true);
  if (debug_>0) std::cout << "End of MuScleFit constructor" << std::endl;

  inputRootTreeFileName_ = pset.getParameter<std::string>("InputRootTreeFileName");
  outputRootTreeFileName_ = pset.getParameter<std::string>("OutputRootTreeFileName");
  maxEventsFromRootTree_ = pset.getParameter<int>("MaxEventsFromRootTree");

  MuScleFitUtils::startWithSimplex_ = pset.getParameter<bool>("StartWithSimplex");
  MuScleFitUtils::computeMinosErrors_ = pset.getParameter<bool>("ComputeMinosErrors");
  MuScleFitUtils::minimumShapePlots_ = pset.getParameter<bool>("MinimumShapePlots");
}

// Destructor
// ----------
MuScleFit::~MuScleFit () {
  if (debug_>0) std::cout << "[MuScleFit]: Destructor" << std::endl;
  std::cout << "Total number of analyzed events = " << totalEvents_ << std::endl;

  if( !(outputRootTreeFileName_.empty()) ) {
    // Save the events to a root tree unless we are reading from the edm root file and the SavedPair size is different from the totalEvents_
    if( !(inputRootTreeFileName_.empty() && (int(MuScleFitUtils::SavedPair.size()) != totalEvents_)) ) {
      std::cout << "Saving muon pairs to root tree" << std::endl;
      RootTreeHandler rootTreeHandler;
      if( MuScleFitUtils::speedup ) {
        rootTreeHandler.writeTree(outputRootTreeFileName_, &(MuScleFitUtils::SavedPair));
      }
      else {
        rootTreeHandler.writeTree(outputRootTreeFileName_, &(MuScleFitUtils::SavedPair), &(MuScleFitUtils::genPair));
      }
    }
    else {
      std::cout << "ERROR: events in the vector = " << MuScleFitUtils::SavedPair.size() << " != totalEvents = " << totalEvents_ << std::endl;
    }
  }
}

// Begin job
// ---------
void MuScleFit::beginOfJob (const edm::EventSetup& eventSetup) {
// void MuScleFit::beginOfJob () {

  if (debug_>0) std::cout << "[MuScleFit]: beginOfJob" << std::endl;
  //if(maxLoopNumber>1)
  readProbabilityDistributionsFromFile();

  if (debug_>0) std::cout << "[MuScleFit]: beginOfJob" << std::endl;

  // Create the root file
  // --------------------
  for (unsigned int i=0; i<(maxLoopNumber); i++) {
    char buffer [2]; // FIXME: the max number of loop has to be < 10, (otherwise not enough char)
    sprintf (buffer, "%d_", i);
    std::string rootFileName = buffer + theRootFileName_;
    theFiles_.push_back (new TFile(rootFileName.c_str(), "RECREATE"));
  }
  if (debug_>0) std::cout << "[MuScleFit]: Root file created" << std::endl;

  plotter = new MuScleFitPlotter(theGenInfoRootFileName_);
  plotter->debug = debug_;
}

// End of job method
// -----------------
void MuScleFit::endOfJob () {
  if (debug_>0) std::cout << "[MuScleFit]: endOfJob" << std::endl;
}

// New loop
// --------
void MuScleFit::startingNewLoop( unsigned int iLoop )
{
  if (debug_>0) std::cout << "[MuScleFit]: Starting loop # " << iLoop << std::endl;

  // Number of muons used
  // --------------------
  MuScleFitUtils::goodmuon = 0;

  // Counters for problem std::cout-ing
  // -----------------------------
  MuScleFitUtils::counter_resprob = 0;

  // Create the root file
  // --------------------
  fillHistoMap(theFiles_[iLoop], iLoop);

  loopCounter = iLoop;
  MuScleFitUtils::loopCounter = loopCounter;

  iev = 0;
  MuScleFitUtils::iev_ = 0;

  MuScleFitUtils::oldNormalization_ = 0;
}

// End of loop routine
// -------------------
edm::EDLooper::Status MuScleFit::endOfLoop( const edm::EventSetup& eventSetup, unsigned int iLoop )
{
  unsigned int iFastLoop = 1;

  // Read the events from the root tree if requested
  if( !(inputRootTreeFileName_.empty()) ) {
    selectMuons(maxEventsFromRootTree_, inputRootTreeFileName_);
    // When reading from local file all the loops are done here
    totalEvents_ = MuScleFitUtils::SavedPair.size();
    iFastLoop = 0;
  }
  else {
    endOfFastLoop(iLoop);
  }

  // If a fastLoop is required we do all the remaining iterations here
  if( fastLoop == true ) {
    for( ; iFastLoop<maxLoopNumber; ++iFastLoop ) {

      std::cout << "Starting fast loop number " << iFastLoop << std::endl;

      // In the first loop is called by the framework
      if( iFastLoop > 0 ) {
        startingNewLoop(iFastLoop);
      }

      // std::vector<std::pair<lorentzVector,lorentzVector> >::const_iterator it = MuScleFitUtils::SavedPair.begin();
      // for( ; it != SavedPair.end(); ++it ) {
      while( iev<totalEvents_ ) {
        std::cout << "Fast looping on event number " << iev << std::endl;
        // This reads muons from SavedPair using iev to keep track of the event
        duringFastLoop();
      }
      std::cout << "End of fast loop number " << iFastLoop << std::endl;
      endOfFastLoop(iFastLoop);
    }
  }

  if (iFastLoop>=maxLoopNumber-1) {
    return kStop;
  } else {
    return kContinue;
  }
}

void MuScleFit::endOfFastLoop( const unsigned int iLoop )
{
  // std::cout<< "Inside endOfFastLoop, iLoop = " << iLoop << " and loopCounter = " << loopCounter << std::endl;

  if( loopCounter == 0 ) {
    // plotter->writeHistoMap();
    // The destructor will call the writeHistoMap after the cd to the output file
    delete plotter;
  }

  std::cout << "Ending loop # " << iLoop << std::endl;

  // Write the histos to file
  // ------------------------
  // theFiles_[iLoop]->cd();
  writeHistoMap(iLoop);

  // Likelihood minimization to compute corrections
  // ----------------------------------------------
  // theFiles_[iLoop]->cd();
  TDirectory * likelihoodDir = theFiles_[iLoop]->mkdir("likelihood");
  likelihoodDir->cd();
  MuScleFitUtils::minimizeLikelihood();

  // ATTENTION, this was put BEFORE the minimizeLikelihood. Check for problems.
  theFiles_[iLoop]->Close();
  // ATTENTION: Check that this delete does not give any problem
  delete theFiles_[iLoop];

  // Clear the histos
  // ----------------
  clearHistoMap();
}

// Stuff to do during loop
// -----------------------
edm::EDLooper::Status MuScleFit::duringLoop( const edm::Event & event, const edm::EventSetup& eventSetup )
{
  edm::Handle<edm::TriggerResults> triggerResults;
  event.getByLabel(edm::InputTag(triggerResultsLabel_.c_str(), "", triggerResultsProcess_.c_str()), triggerResults);
  //event.getByLabel(InputTag(triggerResultsLabel_),triggerResults);
  bool isFired = false;

  if(triggerPath_ == "")
    isFired = true;
  else if(triggerPath_ == "All"){
    isFired =triggerResults->accept();
    if(debug_>0)
      std::cout<<"Trigger "<<isFired<<std::endl;
  }
  else{
   HLTConfigProvider hltConfig;
    hltConfig.init(triggerResultsProcess_);
    unsigned int triggerIndex( hltConfig.triggerIndex(triggerPath_));
    // triggerIndex must be less than the size of HLTR or you get a CMSException: _M_range_check
    if (triggerIndex < triggerResults->size()) {
      isFired = triggerResults->accept(triggerIndex);
      if(debug_>0)
	std::cout<<triggerPath_<<" "<<isFired<<std::endl;
    }
  }

  if( negateTrigger_ && isFired ) return kContinue;
  else if( !(negateTrigger_) && !isFired ) return kContinue;

#ifdef USE_CALLGRIND
  CALLGRIND_START_INSTRUMENTATION;
#endif

  if (debug_>0) {
    std::cout << "[MuScleFit-duringLoop]: loopCounter = " << loopCounter
	 << " Run: " << event.id().run() << " Event: " << event.id().event() << std::endl;
  }

  // On the first iteration we read the bank, otherwise we fetch the information from the muon tree
  // ------------------------------------ Important Note --------------------------------------- //
  // The fillMuonCollection method applies any smearing or bias to the muons, so we NEVER use
  // unbiased muons.
  // ----------------------------------------------------------------------------------------------
  if( loopCounter == 0 ) {

    if( !fastLoop || inputRootTreeFileName_.empty() ) {
      // std::cout << "Reading from edm event" << std::endl;
      selectMuons(event);
      duringFastLoop();
      ++totalEvents_;
    }
  }

  return kContinue;

#ifdef USE_CALLGRIND
  CALLGRIND_STOP_INSTRUMENTATION;
  CALLGRIND_DUMP_STATS;
#endif
}

void MuScleFit::selectMuons(const edm::Event & event)
{
  recMu1 = reco::Particle::LorentzVector(0,0,0,0);
  recMu2 = reco::Particle::LorentzVector(0,0,0,0);

  std::vector<reco::LeafCandidate> muons;

  //================onia cuts===========================/

  if( theMuonType_ <= -1 && PATmuons_) { 
    edm::Handle<pat::CompositeCandidateCollection > collAll;
    try {event.getByLabel("onia2MuMuPatTrkTrk",collAll);} 
    catch (...) {std::cout << "J/psi not present in event!" << std::endl;}
    std::vector<const pat::CompositeCandidate*> collSelGG;
    std::vector<const pat::CompositeCandidate*> collSelGT;
    std::vector<const pat::CompositeCandidate*> collSelTT;
    if (collAll.isValid()) {

      for(std::vector<pat::CompositeCandidate>::const_iterator it=collAll->begin();
	  it!=collAll->end();++it) {
      
	const pat::CompositeCandidate* cand = &(*it);	
	// cout << "Now checking candidate of type " << theJpsiCat << " with pt = " << cand->pt() << endl;
	const pat::Muon* muon1 = dynamic_cast<const pat::Muon*>(cand->daughter("muon1"));
	const pat::Muon* muon2 = dynamic_cast<const pat::Muon*>(cand->daughter("muon2"));
      
	if((muon1->charge() * muon2->charge())>0)
	  continue;

	// global + global?
	if (muon1->isGlobalMuon() && muon2->isGlobalMuon() ) {
	  if (selGlobalMuon(muon1) && selGlobalMuon(muon2) && cand->userFloat("vProb") > 0.001 ) {
	    collSelGG.push_back(cand);
	    continue;
	  }
	}

	// global + tracker? (x2)    
	if (muon1->isGlobalMuon() && muon2->isTrackerMuon() ) {
	  if (selGlobalMuon(muon1) &&  selTrackerMuon(muon2) && cand->userFloat("vProb") > 0.001 ) {
	    collSelGT.push_back(cand);
	    continue;
	  }
	}

	if (muon2->isGlobalMuon() && muon1->isTrackerMuon() ) {
	  if (selGlobalMuon(muon2) && selTrackerMuon(muon1) && cand->userFloat("vProb") > 0.001) {
	    collSelGT.push_back(cand);
	    continue;
	  }
	}

	// tracker + tracker?  
	if (muon1->isTrackerMuon() && muon2->isTrackerMuon() ) {
	  if (selTrackerMuon(muon1) && selTrackerMuon(muon2) && cand->userFloat("vProb") > 0.001) {
	    collSelTT.push_back(cand);
	    continue;
	  }
	}
      }
    }
    // Split them in independent collections if using theMuonType_ == -2, -3 or -4. Take them all if theMuonType_ == -1.
    std::vector<reco::Track> tracks;
    if(collSelGG.size()){
      //CHECK THAT THEY ARE ORDERED BY PT !!!!!!!!!!!!!!!!!!!!!!!
      const pat::Muon* muon1 = dynamic_cast<const pat::Muon*>(collSelGG[0]->daughter("muon1"));
      const pat::Muon* muon2 = dynamic_cast<const pat::Muon*>(collSelGG[0]->daughter("muon2"));
      if( theMuonType_ == -1 || theMuonType_ == -2 ) {
	tracks.push_back(*(muon1->innerTrack()));
	tracks.push_back(*(muon2->innerTrack()));   
      }
    }
    else if(!collSelGG.size() && collSelGT.size()){
      //CHECK THAT THEY ARE ORDERED BY PT !!!!!!!!!!!!!!!!!!!!!!!
      const pat::Muon* muon1 = dynamic_cast<const pat::Muon*>(collSelGT[0]->daughter("muon1"));
      const pat::Muon* muon2 = dynamic_cast<const pat::Muon*>(collSelGT[0]->daughter("muon2"));
      if( theMuonType_ == -1 || theMuonType_ == -3 ) {
	tracks.push_back(*(muon1->innerTrack()));
	tracks.push_back(*(muon2->innerTrack()));   
      }
    }
   else if(!collSelGG.size() && !collSelGT.size() && collSelTT.size()){
      //CHECK THAT THEY ARE ORDERED BY PT !!!!!!!!!!!!!!!!!!!!!!!
      const pat::Muon* muon1 = dynamic_cast<const pat::Muon*>(collSelTT[0]->daughter("muon1"));
      const pat::Muon* muon2 = dynamic_cast<const pat::Muon*>(collSelTT[0]->daughter("muon2"));
      if( theMuonType_ == -1 || theMuonType_ == -4 ) {
	tracks.push_back(*(muon1->innerTrack()));
	tracks.push_back(*(muon2->innerTrack()));   
      }
    }
    if (tracks.size() != 2 && tracks.size() != 0){
      std::cout<<"ERROR strange number of muons selected by onia cuts!"<<std::endl;
      abort();
    }
    muons = fillMuonCollection(tracks); 
  }
  else if( (theMuonType_<4 && theMuonType_>0) || theMuonType_==10 ) { // Muons (glb,sta,trk)
    std::vector<reco::Track> tracks;
    if( PATmuons_ == true ) {
      edm::Handle<pat::MuonCollection> allMuons;
      event.getByLabel( theMuonLabel_, allMuons );
      for( std::vector<pat::Muon>::const_iterator muon = allMuons->begin(); muon != allMuons->end(); ++muon ) {
	//std::cout<<"pat muon is global "<<muon->isGlobalMuon()<<std::endl;
        takeSelectedMuonType(muon, tracks);
      }
      muons = fillMuonCollection(tracks);
    }
    else {
      edm::Handle<reco::MuonCollection> allMuons;
      event.getByLabel (theMuonLabel_, allMuons);
      for( std::vector<reco::Muon>::const_iterator muon = allMuons->begin(); muon != allMuons->end(); ++muon ) {
        takeSelectedMuonType(muon, tracks);
      }
      muons = fillMuonCollection(tracks);
    }
  }
  else if(theMuonType_==4){  //CaloMuons
    edm::Handle<reco::CaloMuonCollection> caloMuons;
    event.getByLabel (theMuonLabel_, caloMuons);
    std::vector<reco::Track> tracks;
    for (std::vector<reco::CaloMuon>::const_iterator muon = caloMuons->begin(); muon != caloMuons->end(); ++muon){
      tracks.push_back(*(muon->track()));
    }
    muons = fillMuonCollection(tracks);
  }

  else if (theMuonType_==5) { // Inner tracker tracks
    edm::Handle<reco::TrackCollection> tracks;
    event.getByLabel (theMuonLabel_, tracks);
    muons = fillMuonCollection(*tracks);
  }

  plotter->fillRec(muons);
  // Compare reco with mc
  // if( loopCounter == 0 && !MuScleFitUtils::speedup ) plotter->fillRecoVsGen(muons, evtMC);

  // Find the two muons from the resonance, and set ResFound bool
  // ------------------------------------------------------------
  std::pair<reco::Particle::LorentzVector, reco::Particle::LorentzVector> recMuFromBestRes =
    MuScleFitUtils::findBestRecoRes(muons);
 
  if (MuScleFitUtils::ResFound) {
    if (debug_>0) {
      std::cout <<std::setprecision(9)<< "Pt after findbestrecores: " << (recMuFromBestRes.first).Pt() << " "
           << (recMuFromBestRes.second).Pt() << std::endl;
      std::cout << "recMu1 = " << recMu1 << std::endl;
      std::cout << "recMu2 = " << recMu2 << std::endl;
    }
    recMu1 = recMuFromBestRes.first;
    recMu2 = recMuFromBestRes.second;
    if (debug_>0) {
      std::cout << "after recMu1 = " << recMu1 << std::endl;
      std::cout << "after recMu2 = " << recMu2 << std::endl;
      std::cout << "mu1.pt = " << recMu1.Pt() << std::endl;
      std::cout << "mu2.pt = " << recMu2.Pt() << std::endl;
    }
    MuScleFitUtils::SavedPair.push_back( std::make_pair( recMu1, recMu2 ) );
  } else {
    MuScleFitUtils::SavedPair.push_back( std::make_pair( lorentzVector(0.,0.,0.,0.), lorentzVector(0.,0.,0.,0.) ) );
  }

  // ------- //
  // MC info //
  // ------- //

  // NB we skip the simulation part if we are in a hurry
  if (!MuScleFitUtils::speedup) {
    // Find and store in histograms the generated and simulated resonance and muons
    // ----------------------------------------------------------------------------
    edm::Handle<edm::HepMCProduct> evtMC;
    edm::Handle<edm::SimTrackContainer> simTracks;
    edm::Handle<reco::GenParticleCollection> genParticles;

    // Fill gen information only in the first loop
    ifHepMC=false;
    ifGenPart=false;

    event.getByLabel( genParticlesName_, evtMC );
    if( evtMC.isValid() ) {

      MuScleFitUtils::genPair.push_back( MuScleFitUtils::findGenMuFromRes(evtMC) );

      plotter->fillGen2(evtMC, MuScleFitUtils::sherpa_);
      ifHepMC = true;
    }
    else {
      // std::cout << "HepMCProduct non existent. Trying with genParticles" << std::endl;
      event.getByLabel( genParticlesName_, genParticles );
      if( genParticles.isValid() ) {

        MuScleFitUtils::genPair.push_back( MuScleFitUtils::findGenMuFromRes(genParticles) );

        plotter->fillGen1(genParticles);
        ifGenPart=true;
        if (debug_>0) std::cout << "Found genParticles" << std::endl;
      }
      else {
        // std::cout << "GenParticles non existent" << std::endl;
        std::cout<<"ERROR "<<"non generation info and speedup true!!!!!!!!!!!!"<<std::endl;
        // Fill it in any case, otherwise it will not be in sync with the event number
        MuScleFitUtils::genPair.push_back( std::make_pair( lorentzVector(0.,0.,0.,0.), lorentzVector(0.,0.,0.,0.) ) );
      }
    }

    if(!(MuScleFitUtils::speedup) && debug_>0) {
      std::cout << "genParticles:" << std::endl;
      std::cout << MuScleFitUtils::genPair.back().first << " , " << MuScleFitUtils::genPair.back().second << std::endl;
    }

    if( compareToSimTracks_ ) {
      bool simTracksFound = false;
      event.getByLabel(simTracksCollection_, simTracks);
      if( simTracks.isValid() ) {
        plotter->fillSim(simTracks);
        if(ifHepMC) {

          MuScleFitUtils::simPair.push_back( MuScleFitUtils::findSimMuFromRes(evtMC,simTracks) );
          simTracksFound = true;
          plotter->fillGenSim(evtMC,simTracks);
        }
      }
      else {
        std::cout << "SimTracks not existent" << std::endl;
      }
      if( !simTracksFound ) {
        MuScleFitUtils::simPair.push_back( std::make_pair( lorentzVector(0.,0.,0.,0.), lorentzVector(0.,0.,0.,0.) ) );
      }
    }
  }
}

void MuScleFit::selectMuons(const int maxEvents, const TString & treeFileName)
{
  std::cout << "Reading muon pairs from Root Tree in " << treeFileName << std::endl;
  RootTreeHandler rootTreeHandler;
  if( MuScleFitUtils::speedup ) {
    rootTreeHandler.readTree(maxEvents, inputRootTreeFileName_, &(MuScleFitUtils::SavedPair));
  }
  else {
    rootTreeHandler.readTree(maxEvents, inputRootTreeFileName_, &(MuScleFitUtils::SavedPair), &(MuScleFitUtils::genPair));
  }
  // Now loop on all the pairs and apply any smearing and bias if needed
  std::vector<std::pair<lorentzVector,lorentzVector> >::iterator it = MuScleFitUtils::SavedPair.begin();
  for( ; it != MuScleFitUtils::SavedPair.end(); ++it ) {

      // Apply any cut if requested
      // Note that cuts here are only applied to already selected muons. They should not be used unless
      // you are sure that the difference is negligible (e.g. the number of events with > 2 muons is negligible).
      double pt1 = it->first.pt();
      std::cout << "pt1 = " << pt1 << std::endl;
      double pt2 = it->second.pt();
      std::cout << "pt2 = " << pt2 << std::endl;
      double eta1 = it->first.eta();
      std::cout << "eta1 = " << eta1 << std::endl;
      double eta2 = it->second.eta();
      std::cout << "eta2 = " << eta2 << std::endl;
      // If they don't pass the cuts set to null vectors
      if( !(pt1 > MuScleFitUtils::minMuonPt_ && pt1 < MuScleFitUtils::maxMuonPt_ &&
	    pt2 > MuScleFitUtils::minMuonPt_ && pt2 < MuScleFitUtils::maxMuonPt_ &&
	    ( (eta1 > MuScleFitUtils::minMuonEtaFirstRange_ && eta1 < MuScleFitUtils::maxMuonEtaFirstRange_ &&
	       eta2 > MuScleFitUtils::minMuonEtaFirstRange_ && eta2 < MuScleFitUtils::maxMuonEtaFirstRange_) ||
	      (eta1 > MuScleFitUtils::minMuonEtaSecondRange_ && eta1 < MuScleFitUtils::maxMuonEtaSecondRange_ &&
	       eta2 > MuScleFitUtils::minMuonEtaSecondRange_ && eta2 < MuScleFitUtils::maxMuonEtaSecondRange_) ) ) ) {
	// std::cout << "removing muons not passing cuts" << std::endl;
	it->first = reco::Particle::LorentzVector(0,0,0,0);
	it->second = reco::Particle::LorentzVector(0,0,0,0);
      }

      // First is always mu-, second mu+
      if( (MuScleFitUtils::SmearType != 0) || (MuScleFitUtils::BiasType != 0) ) {
	applySmearing(it->first);
	applyBias(it->first, -1);
	applySmearing(it->second);
	applyBias(it->second, 1);
      }
  }
  plotter->fillRec(MuScleFitUtils::SavedPair);
  if( !(MuScleFitUtils::speedup) ) {
    plotter->fillGen(MuScleFitUtils::genPair);
  }
}

void MuScleFit::duringFastLoop()
{
  // On loops>0 the two muons are directly obtained from the SavedMuon array
  // -----------------------------------------------------------------------
  MuScleFitUtils::ResFound = false;
  recMu1 = (MuScleFitUtils::SavedPair[iev].first);
  recMu2 = (MuScleFitUtils::SavedPair[iev].second);
  if (recMu1.Pt()>0 && recMu2.Pt()>0) {
    MuScleFitUtils::ResFound = true;
    if (debug_>0) std::cout << "Ev = " << iev << ": found muons in tree with Pt = "
                       << recMu1.Pt() << " " << recMu2.Pt() << std::endl;
  }

  if( debug_>0 ) std::cout << "About to start lik par correction and histo filling; ResFound is "
		    << MuScleFitUtils::ResFound << std::endl;
  // If resonance found, do the hard work
  // ------------------------------------
  if( MuScleFitUtils::ResFound ) {

    // Find weight and reference mass for this muon pair
    // -------------------------------------------------
    // The last parameter = true means that we want to use always the background window to compute the weight,
    // otherwise the probability will be filled only for the resonance region.
    double weight = MuScleFitUtils::computeWeight( (recMu1+recMu2).mass(), iev, true );
    if (debug_>0) {
      std::cout << "Loop #" << loopCounter << "Event #" << iev << ": before correction     Pt1 = "
	   << recMu1.Pt() << " Pt2 = " << recMu2.Pt() << std::endl;
    }
    // For successive iterations, correct the muons only if the previous iteration was a scale fit.
    // --------------------------------------------------------------------------------------------
    if ( loopCounter>0 ) {
      if ( MuScleFitUtils::doScaleFit[loopCounter-1] ) {
        recMu1 = (MuScleFitUtils::applyScale(recMu1, MuScleFitUtils::parvalue[loopCounter-1], -1));
        recMu2 = (MuScleFitUtils::applyScale(recMu2, MuScleFitUtils::parvalue[loopCounter-1],  1));
      }
    }
    if (debug_>0) {
      std::cout << "Loop #" << loopCounter << "Event #" << iev << ": after correction      Pt1 = "
	   << recMu1.Pt() << " Pt2 = " << recMu2.Pt() << std::endl;
    }

    reco::Particle::LorentzVector bestRecRes( recMu1+recMu2 );

    //Fill histograms
    //------------------
    mapHisto_["hRecBestMu"]->Fill(recMu1);
    mapHisto_["hRecBestMuVSEta"]->Fill(recMu1);
    mapHisto_["hRecBestMu"]->Fill(recMu2);
    mapHisto_["hRecBestMuVSEta"]->Fill(recMu2);
    mapHisto_["hDeltaRecBestMu"]->Fill(recMu1, recMu2);
    // Reconstructed resonance
    mapHisto_["hRecBestRes"]->Fill(bestRecRes, weight);
    mapHisto_["hRecBestResAllEvents"]->Fill(bestRecRes, 1.);
    // Fill histogram of Res mass vs muon variables
    mapHisto_["hRecBestResVSMu"]->Fill (recMu1, bestRecRes, -1);
    mapHisto_["hRecBestResVSMu"]->Fill (recMu2, bestRecRes, +1);
    // Fill histogram of Res mass vs Res variables
    mapHisto_["hRecBestResVSRes"]->Fill (bestRecRes, bestRecRes, +1);

    std::vector<double> * parval;
    std::vector<double> initpar;
    // Store a pointer to the vector of parameters of the last iteration, or the initial
    // parameters if this is the first iteration
    if (loopCounter==0) {
      initpar = MuScleFitUtils::parResol;
      initpar.insert( initpar.end(), MuScleFitUtils::parScale.begin(), MuScleFitUtils::parScale.end() );
      initpar.insert( initpar.end(), MuScleFitUtils::parCrossSection.begin(), MuScleFitUtils::parCrossSection.end() );
      initpar.insert( initpar.end(), MuScleFitUtils::parBgr.begin(), MuScleFitUtils::parBgr.end() );
      parval = &initpar;
    } else {
      parval = &(MuScleFitUtils::parvalue[loopCounter-1]);
    }

    //Compute pt resolution w.r.t generated and simulated muons
    //--------------------------------------------------------
    if( !MuScleFitUtils::speedup ) {

      //first is always mu-, second is always mu+
      if(checkDeltaR(MuScleFitUtils::genPair[iev].first,recMu1)) {
        fillComparisonHistograms( MuScleFitUtils::genPair[iev].first, recMu1, "Gen", -1 );
      }
      if(checkDeltaR(MuScleFitUtils::genPair[iev].second,recMu2)){
        fillComparisonHistograms( MuScleFitUtils::genPair[iev].second, recMu2, "Gen", +1 );
      }
      if( compareToSimTracks_ ) {
        //first is always mu-, second is always mu+
        if(checkDeltaR(MuScleFitUtils::simPair[iev].first,recMu1)){
          fillComparisonHistograms( MuScleFitUtils::simPair[iev].first, recMu1, "Sim", -1 );
        }
        if(checkDeltaR(MuScleFitUtils::simPair[iev].second,recMu2)){
          fillComparisonHistograms( MuScleFitUtils::simPair[iev].second, recMu2, "Sim", +1 );
        }
      }
    }

    // ATTENTION: this was done only when a matching was found. Moved it outside because, genInfo or not, we still want to see the resolution function
    // Fill also the resolution histogramsm using the resolution functions:
    // the parameters are those from the last iteration, as the muons up to this point have also the corrections from the same iteration.
    // Need to use a different array (ForVec), containing functors able to operate on std::vector<double>
    mapHisto_["hFunctionResolPt"]->Fill( recMu1, MuScleFitUtils::resolutionFunctionForVec->sigmaPt(recMu1.Pt(), recMu1.Eta(), *parval ), -1 );
    mapHisto_["hFunctionResolCotgTheta"]->Fill( recMu1, MuScleFitUtils::resolutionFunctionForVec->sigmaCotgTh(recMu1.Pt(), recMu1.Eta(), *parval ), -1 );
    mapHisto_["hFunctionResolPhi"]->Fill( recMu1, MuScleFitUtils::resolutionFunctionForVec->sigmaPhi(recMu1.Pt(), recMu1.Eta(), *parval ), -1 );
    mapHisto_["hFunctionResolPt"]->Fill( recMu2, MuScleFitUtils::resolutionFunctionForVec->sigmaPt(recMu2.Pt(), recMu2.Eta(), *parval ), +1 );
    mapHisto_["hFunctionResolCotgTheta"]->Fill( recMu2, MuScleFitUtils::resolutionFunctionForVec->sigmaCotgTh(recMu2.Pt(), recMu2.Eta(), *parval ), +1 );
    mapHisto_["hFunctionResolPhi"]->Fill( recMu2, MuScleFitUtils::resolutionFunctionForVec->sigmaPhi(recMu2.Pt(), recMu2.Eta(), *parval ), +1 );

    // Compute likelihood histograms
    // -----------------------------
    if( debug_ > 0 ) std::cout << "mass = " << bestRecRes.mass() << std::endl;
    if (weight!=0.) {
      double massResol;
      double prob;
      double deltalike;
      if (loopCounter==0) {
	std::vector<double> initpar;
	for (int i=0; i<(int)(MuScleFitUtils::parResol.size()); i++) {
	  initpar.push_back(MuScleFitUtils::parResol[i]);
	}
	for (int i=0; i<(int)(MuScleFitUtils::parScale.size()); i++) {
	  initpar.push_back(MuScleFitUtils::parScale[i]);
	}
// 	for (int i=0; i<(int)(MuScleFitUtils::parCrossSection.size()); i++) {
// 	  initpar.push_back(MuScleFitUtils::parCrossSection[i]);
// 	}
        MuScleFitUtils::crossSectionHandler->addParameters(initpar);

	for (int i=0; i<(int)(MuScleFitUtils::parBgr.size()); i++) {
	  initpar.push_back(MuScleFitUtils::parBgr[i]);
	}
	massResol = MuScleFitUtils::massResolution( recMu1, recMu2, initpar );
	prob      = MuScleFitUtils::massProb( bestRecRes.mass(), bestRecRes.Eta(), bestRecRes.Rapidity(), massResol, initpar, true );
      } else {
	massResol = MuScleFitUtils::massResolution( recMu1, recMu2,
                                                    MuScleFitUtils::parvalue[loopCounter-1] );
	prob      = MuScleFitUtils::massProb( bestRecRes.mass(), bestRecRes.Eta(), bestRecRes.Rapidity(),
                                              massResol, MuScleFitUtils::parvalue[loopCounter-1], true );
      }
      if( debug_ > 0 ) std::cout << "inside weight: mass = " << bestRecRes.mass() << ", prob = " << prob << std::endl;
      if (prob>0) {
        if( debug_ > 0 ) std::cout << "inside prob: mass = " << bestRecRes.mass() << ", prob = " << prob << std::endl;

	deltalike = log(prob)*weight; // NB maximum likelihood --> deltalike is maximized
	mapHisto_["hLikeVSMu"]->Fill(recMu1, deltalike);
	mapHisto_["hLikeVSMu"]->Fill(recMu2, deltalike);
	mapHisto_["hLikeVSMuMinus"]->Fill(recMu1, deltalike);
	mapHisto_["hLikeVSMuPlus"]->Fill(recMu2, deltalike);

        double recoMass = (recMu1+recMu2).mass();
        if( recoMass != 0 ) {
          // IMPORTANT: massResol is not a relative resolution
          mapHisto_["hResolMassVSMu"]->Fill(recMu1, massResol, -1);
          mapHisto_["hResolMassVSMu"]->Fill(recMu2, massResol, +1);
          mapHisto_["hFunctionResolMassVSMu"]->Fill(recMu1, massResol/recoMass, -1);
          mapHisto_["hFunctionResolMassVSMu"]->Fill(recMu2, massResol/recoMass, +1);
        }

        if( MuScleFitUtils::debugMassResol_ ) {
          mapHisto_["hdMdPt1"]->Fill(recMu1, MuScleFitUtils::massResolComponents.dmdpt1, -1);
          mapHisto_["hdMdPt2"]->Fill(recMu2, MuScleFitUtils::massResolComponents.dmdpt2, +1);
          mapHisto_["hdMdPhi1"]->Fill(recMu1, MuScleFitUtils::massResolComponents.dmdphi1, -1);
          mapHisto_["hdMdPhi2"]->Fill(recMu2, MuScleFitUtils::massResolComponents.dmdphi2, +1);
          mapHisto_["hdMdCotgTh1"]->Fill(recMu1, MuScleFitUtils::massResolComponents.dmdcotgth1, -1);
          mapHisto_["hdMdCotgTh2"]->Fill(recMu2, MuScleFitUtils::massResolComponents.dmdcotgth2, +1);
        }

        if( !MuScleFitUtils::speedup ) {
          double genMass = (MuScleFitUtils::genPair[iev].first + MuScleFitUtils::genPair[iev].second).mass();
          // Fill the mass resolution (computed from MC), we use the covariance class to compute the variance
          if( genMass != 0 ) {
	    mapHisto_["hGenResVSMu"]->Fill((MuScleFitUtils::genPair[iev].first), (MuScleFitUtils::genPair[iev].first + MuScleFitUtils::genPair[iev].second), -1);
	    mapHisto_["hGenResVSMu"]->Fill((MuScleFitUtils::genPair[iev].second), (MuScleFitUtils::genPair[iev].first + MuScleFitUtils::genPair[iev].second), +1);
            double diffMass = (recoMass - genMass)/genMass;
            // double diffMass = recoMass - genMass;
            // Fill if for both muons
            double pt1 = recMu1.pt();
            double eta1 = recMu1.eta();
            double pt2 = recMu2.pt();
            double eta2 = recMu2.eta();
            // This is to avoid nan
            if( diffMass == diffMass ) {
              // Mass relative difference vs Pt and Eta. To be used to extract the true mass resolution
              mapHisto_["hDeltaMassOverGenMassVsPt"]->Fill(pt1, diffMass);
              mapHisto_["hDeltaMassOverGenMassVsPt"]->Fill(pt2, diffMass);
              mapHisto_["hDeltaMassOverGenMassVsEta"]->Fill(eta1, diffMass);
              mapHisto_["hDeltaMassOverGenMassVsEta"]->Fill(eta2, diffMass);
              // This is used for the covariance comparison
              mapHisto_["hMassResolutionVsPtEta"]->Fill(pt1, eta1, diffMass, diffMass);
              mapHisto_["hMassResolutionVsPtEta"]->Fill(pt2, eta2, diffMass, diffMass);
            }
            else {
              std::cout << "Error, there is a nan: recoMass = " << recoMass << ", genMass = " << genMass << std::endl;
            }
          }
          // Fill with mass resolution from resolution function
          double massRes = MuScleFitUtils::massResolution(recMu1, recMu2, MuScleFitUtils::parResol);
          mapHisto_["hFunctionResolMass"]->Fill( recMu1, pow(massRes,2), -1 );
          mapHisto_["hFunctionResolMass"]->Fill( recMu2, pow(massRes,2), +1 );
        }

        mapHisto_["hMass_P"]->Fill(bestRecRes.mass(), prob);
        if( debug_ > 0 ) std::cout << "mass = " << bestRecRes.mass() << ", prob = " << prob << std::endl;
        mapHisto_["hMass_fine_P"]->Fill(bestRecRes.mass(), prob);

        mapHisto_["hMassProbVsRes"]->Fill(bestRecRes, bestRecRes, +1, prob);
        mapHisto_["hMassProbVsMu"]->Fill(recMu1, bestRecRes, -1, prob);
        mapHisto_["hMassProbVsMu"]->Fill(recMu2, bestRecRes, +1, prob);
        mapHisto_["hMassProbVsRes_fine"]->Fill(bestRecRes, bestRecRes, +1, prob);
        mapHisto_["hMassProbVsMu_fine"]->Fill(recMu1, bestRecRes, -1, prob);
        mapHisto_["hMassProbVsMu_fine"]->Fill(recMu2, bestRecRes, +1, prob);
      }
    }
  } // end if ResFound

  // Fill the pair
  // -------------
  if (loopCounter>0) {
    if (debug_>0) std::cout << "[MuScleFit]: filling the pair" << std::endl;
    MuScleFitUtils::SavedPair[iev] = std::make_pair( recMu1, recMu2 );
  }

  iev++;
  MuScleFitUtils::iev_++;

  // return kContinue;
}

bool MuScleFit::checkDeltaR(reco::Particle::LorentzVector& genMu, reco::Particle::LorentzVector& recMu){
  //first is always mu-, second is always mu+
  double deltaR = sqrt(MuScleFitUtils::deltaPhi(recMu.Phi(),genMu.Phi()) * MuScleFitUtils::deltaPhi(recMu.Phi(),genMu.Phi()) +
                       ((recMu.Eta()-genMu.Eta()) * (recMu.Eta()-genMu.Eta())));
  if(deltaR<0.01)
    return true;
  else
    std::cout<<"Reco muon "<<recMu<<" with eta "<<recMu.Eta()<<" and phi "<<recMu.Phi()<<std::endl
	<<" DOES NOT MATCH with generated muon from resonance: "<<std::endl
	<<genMu<<" with eta "<<genMu.Eta()<<" and phi "<<genMu.Phi()<<std::endl;
  return false;
}

void MuScleFit::fillComparisonHistograms( const reco::Particle::LorentzVector & genMu, const reco::Particle::LorentzVector & recMu,
					  const std::string & inputName, const int charge )
{
  std::string name(inputName + "VSMu");
  mapHisto_["hResolPt"+name]->Fill(recMu, (-genMu.Pt()+recMu.Pt())/genMu.Pt(), charge);
  mapHisto_["hResolTheta"+name]->Fill(recMu, (-genMu.Theta()+recMu.Theta()), charge);
  mapHisto_["hResolCotgTheta"+name]->Fill(recMu,(-cos(genMu.Theta())/sin(genMu.Theta())
                                                 +cos(recMu.Theta())/sin(recMu.Theta())), charge);
  mapHisto_["hResolEta"+name]->Fill(recMu, (-genMu.Eta()+recMu.Eta()),charge);
  mapHisto_["hResolPhi"+name]->Fill(recMu, MuScleFitUtils::deltaPhiNoFabs(recMu.Phi(), genMu.Phi()), charge);

  // Fill only if it was matched to a genMu and this muon is valid
  if( (genMu.Pt() != 0) && (recMu.Pt() != 0) ) {
    mapHisto_["hPtRecoVsPt"+inputName]->Fill(genMu.Pt(), recMu.Pt());
  }
}

void MuScleFit::applySmearing( reco::Particle::LorentzVector & mu )
{
  if( MuScleFitUtils::SmearType>0 ) {
    mu = MuScleFitUtils::applySmearing( mu );
    if (debug_>0) std::cout << "Muon #" << MuScleFitUtils::goodmuon
                       << ": after smearing  Pt = " << mu.Pt() << std::endl;
  }
}

void MuScleFit::applyBias( reco::Particle::LorentzVector & mu, const int charge )
{
  if( MuScleFitUtils::BiasType>0 ) {
    mu = MuScleFitUtils::applyBias( mu, charge );
    if (debug_>0) std::cout << "Muon #" << MuScleFitUtils::goodmuon
                       << ": after bias      Pt = " << mu.Pt() << std::endl;
  }
}

// Simple method to check parameters consistency. It aborts the job if the parameters
// are not consistent.
void MuScleFit::checkParameters() {

  // Fits selection dimension check
  if( MuScleFitUtils::doResolFit.size() != maxLoopNumber ) {
    std::cout << "[MuScleFit-Constructor]: wrong size of resolution fits selector = " << MuScleFitUtils::doResolFit.size() << std::endl;
    std::cout << "it must have as many values as the number of loops, which is = " << maxLoopNumber << std::endl;
    abort();
  }
  if( MuScleFitUtils::doScaleFit.size() != maxLoopNumber ) {
    std::cout << "[MuScleFit-Constructor]: wrong size of scale fits selector = " << MuScleFitUtils::doScaleFit.size() << std::endl;
    std::cout << "it must have as many values as the number of loops, which is = " << maxLoopNumber << std::endl;
    abort();
  }
  if( MuScleFitUtils::doCrossSectionFit.size() != maxLoopNumber ) {
    std::cout << "[MuScleFit-Constructor]: wrong size of cross section fits selector = " << MuScleFitUtils::doCrossSectionFit.size() << std::endl;
    std::cout << "it must have as many values as the number of loops, which is = " << maxLoopNumber << std::endl;
    abort();
  }
  if( MuScleFitUtils::doBackgroundFit.size() != maxLoopNumber ) {
    std::cout << "[MuScleFit-Constructor]: wrong size of background fits selector = " << MuScleFitUtils::doBackgroundFit.size() << std::endl;
    std::cout << "it must have as many values as the number of loops, which is = " << maxLoopNumber << std::endl;
    abort();
  }

  // Bias parameters: dimension check
  // --------------------------------
  if ((MuScleFitUtils::BiasType==1  && MuScleFitUtils::parBias.size()!=2) || // linear in pt
      (MuScleFitUtils::BiasType==2  && MuScleFitUtils::parBias.size()!=2) || // linear in |eta|
      (MuScleFitUtils::BiasType==3  && MuScleFitUtils::parBias.size()!=4) || // sinusoidal in phi
      (MuScleFitUtils::BiasType==4  && MuScleFitUtils::parBias.size()!=3) || // linear in pt and |eta|
      (MuScleFitUtils::BiasType==5  && MuScleFitUtils::parBias.size()!=3) || // linear in pt and sinusoidal in phi
      (MuScleFitUtils::BiasType==6  && MuScleFitUtils::parBias.size()!=3) || // linear in |eta| and sinusoidal in phi
      (MuScleFitUtils::BiasType==7  && MuScleFitUtils::parBias.size()!=4) || // linear in pt and |eta| and sinusoidal in phi
      (MuScleFitUtils::BiasType==8  && MuScleFitUtils::parBias.size()!=4) || // linear in pt and parabolic in |eta|
      (MuScleFitUtils::BiasType==9  && MuScleFitUtils::parBias.size()!=2) || // exponential in pt
      (MuScleFitUtils::BiasType==10 && MuScleFitUtils::parBias.size()!=3) || // parabolic in pt
      (MuScleFitUtils::BiasType==11 && MuScleFitUtils::parBias.size()!=4) || // linear in pt and sin in phi with chg
      (MuScleFitUtils::BiasType==12 && MuScleFitUtils::parBias.size()!=6) || // linear in pt and para in plus sin in phi with chg
      (MuScleFitUtils::BiasType==13 && MuScleFitUtils::parBias.size()!=8) || // linear in pt and para in plus sin in phi with chg
      MuScleFitUtils::BiasType<0 || MuScleFitUtils::BiasType>13) {
    std::cout << "[MuScleFit-Constructor]: Wrong bias type or number of parameters: aborting!" << std::endl;
    abort();
  }
  // Smear parameters: dimension check
  // ---------------------------------
  if ((MuScleFitUtils::SmearType==1  && MuScleFitUtils::parSmear.size()!=3) ||
      (MuScleFitUtils::SmearType==2  && MuScleFitUtils::parSmear.size()!=4) ||
      (MuScleFitUtils::SmearType==3  && MuScleFitUtils::parSmear.size()!=5) ||
      (MuScleFitUtils::SmearType==4  && MuScleFitUtils::parSmear.size()!=6) ||
      (MuScleFitUtils::SmearType==5  && MuScleFitUtils::parSmear.size()!=7) ||
      (MuScleFitUtils::SmearType==6  && MuScleFitUtils::parSmear.size()!=16) ||
      MuScleFitUtils::SmearType<0 || MuScleFitUtils::SmearType>6) {
    std::cout << "[MuScleFit-Constructor]: Wrong smear type or number of parameters: aborting!" << std::endl;
    abort();
  }
  // Protect against bad size of parameters
  // --------------------------------------
  if (MuScleFitUtils::parResol.size()!=MuScleFitUtils::parResolFix.size() ||
      MuScleFitUtils::parResol.size()!=MuScleFitUtils::parResolOrder.size()) {
    std::cout << "[MuScleFit-Constructor]: Mismatch in number of parameters for Resol: aborting!" << std::endl;
    abort();
  }
  if (MuScleFitUtils::parScale.size()!=MuScleFitUtils::parScaleFix.size() ||
      MuScleFitUtils::parScale.size()!=MuScleFitUtils::parScaleOrder.size()) {
    std::cout << "[MuScleFit-Constructor]: Mismatch in number of parameters for Scale: aborting!" << std::endl;
    abort();
  }
  if (MuScleFitUtils::parCrossSection.size()!=MuScleFitUtils::parCrossSectionFix.size() ||
      MuScleFitUtils::parCrossSection.size()!=MuScleFitUtils::parCrossSectionOrder.size()) {
    std::cout << "[MuScleFit-Constructor]: Mismatch in number of parameters for Bgr: aborting!" << std::endl;
    abort();
  }
  if (MuScleFitUtils::parBgr.size()!=MuScleFitUtils::parBgrFix.size() ||
      MuScleFitUtils::parBgr.size()!=MuScleFitUtils::parBgrOrder.size()) {
    std::cout << "[MuScleFit-Constructor]: Mismatch in number of parameters for Bgr: aborting!" << std::endl;
    abort();
  }

  // Protect against an incorrect number of resonances
  // -------------------------------------------------
  if (MuScleFitUtils::resfind.size()!=6) {
    std::cout << "[MuScleFit-Constructor]: resfind must have 6 elements (1 Z, 3 Y, 2 Psi): aborting!" << std::endl;
    abort();
  }
}

bool MuScleFit::selGlobalMuon(const pat::Muon* aMuon) {

  reco::TrackRef iTrack = aMuon->innerTrack();
  const reco::HitPattern& p = iTrack->hitPattern();

  return (iTrack->found() > 11 &&
	  aMuon->globalTrack()->chi2()/aMuon->globalTrack()->ndof() < 20.0 &&
	  // (p.numberOfValidPixelHits() > 2 || 
	  // (p.numberOfValidPixelHits() > 1 && p.getLayer(p.getHitPattern(0)) == 1)) &&
          p.pixelLayersWithMeasurement() > 1 &&
	  fabs(iTrack->d0()) < 5.0 &&
          fabs(iTrack->dz()) < 20.0 );
}

bool MuScleFit::selTrackerMuon(const pat::Muon* aMuon) {
  
  reco::TrackRef iTrack = aMuon->innerTrack();
  const reco::HitPattern& p = iTrack->hitPattern();

  return (iTrack->found() > 11 &&
	  iTrack->chi2()/iTrack->ndof() < 5.0 &&
	  //   aMuon->muonID("TrackerMuonArbitrated") &&
	  aMuon->muonID("TMLastStationAngTight") &&
	  // (p.numberOfValidPixelHits() > 2 || 
	  // (p.numberOfValidPixelHits() > 1 && p.getLayer(p.getHitPattern(0)) == 1)) &&
          p.pixelLayersWithMeasurement() > 1 &&
	  fabs(iTrack->d0()) < 5.0 &&
          fabs(iTrack->dz()) < 20.0 );
}
