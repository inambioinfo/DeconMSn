// Written by Anoop Mayampurath for the Department of Energy (PNNL, Richland, WA)
// Copyright 2006, Battelle Memorial Institute
// E-mail: navdeep.jaitly@pnl.gov
// Website: http://ncrr.pnl.gov/software
// -------------------------------------------------------------------------------
// 
// Licensed under the Apache License, Version 2.0; you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at 
// http://www.apache.org/licenses/LICENSE-2.0

#include "..\Utilities\SavGolSmoother.h" 
#include ".\DTAGeneration.h"
#include ".\MSnInformationRecord.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include<time.h>
#include<sys/types.h>
#include<sys/timeb.h>



namespace Engine
{
	namespace DTAProcessing
	{
		DTAProcessor::DTAProcessor(void)
		{
			mobj_parent_peak_processor = new Engine::PeakProcessing::PeakProcessor( );			
			mobj_msN_peak_processor = new Engine::PeakProcessing::PeakProcessor() ;
			mobj_summed_peak_processor = new Engine::PeakProcessing::PeakProcessor() ; 
			mobj_mass_transform = new Engine::HornTransform::MassTransform() ;					
			mobj_svm_charge_determination  = new Engine::ChargeDetermination::SVMChargeDetermine() ;
			mobj_autocorrelation_charge_determination = new Engine::ChargeDetermination::AutoCorrelationChargeDetermine() ; 
			mobj_raw_data_dta = NULL ;
			mdbl_maxMass = 0.0 ;
			mint_maxScan = 0 ;
			mint_minIonCount = 0 ;
			mdbl_minMass = 0.0 ;
			mdbl_parent_Mz = 0.0 ;
			mdbl_parent_Intensity = 0.0 ;
			mint_minScan = 0 ;					
			mint_window_size = 5 ; 
			mint_msNscanIndex = 0 ; 	       
			mdbl_cc_mass  = 1.00727638 ;			
			mint_consider_charge = 0 ; 
			mdbl_min_fit_for_single_spectra = 0.1 ; 			
			mint_NumDTARecords = 0 ; 
			mint_NumMSnScansProcessed = 0 ;
			mint_isolation_window_size = 3 ;
			mbln_consider_multiple_precursors = false ;
			mbln_centroid_msn = false;				// Warning: the masses reported by GetMassListFromScanNum when centroiding are not properly calibrated and thus could be off by 0.3 m/z or more
			menm_dataset_type = Engine::Readers::FINNIGAN ;
			mbln_is_profile_data_for_mzXML = false ; 
			mbln_first_scan_written = false;

		}

		DTAProcessor::~DTAProcessor(void)
		{
			if (mobj_parent_peak_processor != NULL)
				delete mobj_parent_peak_processor ; 
			if (mobj_summed_peak_processor!= NULL)
				delete mobj_summed_peak_processor ; 			
			if (mobj_msN_peak_processor != NULL)
				delete mobj_msN_peak_processor ; 
			if (mobj_mass_transform != NULL)
				delete mobj_mass_transform ; 
			if (mobj_svm_charge_determination != NULL)
				delete mobj_svm_charge_determination ;
			if (mobj_autocorrelation_charge_determination != NULL)
				delete mobj_autocorrelation_charge_determination ; 

			if (mobj_raw_data_dta != NULL)
			{
				mobj_raw_data_dta->Close() ; 
				delete mobj_raw_data_dta ;
			}
		}

		void DTAProcessor::SetDTAOptions(int minIonCount, int minScan, int maxScan, double minMass, double maxMass, 
			                             bool createLogFileOnly, bool createCompositeDTA, int considerCharge, 
										 bool considerMultiplePrecursors, bool centroid,
										 int isolationWindowSize, bool isProfileDataForMzXML)
		{
			mint_minIonCount = minIonCount ;
			mdbl_maxMass = maxMass ;
			mdbl_minMass = minMass ;
			mint_maxScan = maxScan ;
			mint_minScan = minScan ;	
			mbln_create_composite_dta = createCompositeDTA ; 
			mbln_create_log_file_only = createLogFileOnly ; 
			mint_consider_charge = considerCharge ; 
			mint_isolation_window_size = isolationWindowSize ; 
			mbln_consider_multiple_precursors = considerMultiplePrecursors ; 
			mbln_centroid_msn = centroid;
			mbln_is_profile_data_for_mzXML = isProfileDataForMzXML ; 
			mbln_first_scan_written = false;
		}

		void DTAProcessor::SetPeakParameters(double pkBkgRatio, double peptideMinBkgRatio)
		{
			mdbl_pkBkgRatio = pkBkgRatio ;
			mdbl_peptideMinBkgRatio = peptideMinBkgRatio ;
		}
		
		
		void DTAProcessor::SetPeakProcessorOptions(double s2n, double thresh, bool thresholded, Engine::PeakProcessing::PEAK_FIT_TYPE fit_type)
		{
			mobj_parent_peak_processor->SetOptions(s2n, thresh, thresholded, fit_type) ;
			mobj_msN_peak_processor->SetOptions(s2n, thresh, thresholded, fit_type) ; 
			mobj_summed_peak_processor->SetOptions(s2n, thresh, thresholded, fit_type) ; 
		}

	
		void DTAProcessor::SetMassTransformOptions(short max_charge, double max_mw, double max_fit, double min_s2n, double cc_mass, 
			double delete_threshold_intensity, double min_theoretical_intensity_for_score, short num_peaks_for_shoulder, 
			bool use_mercury_caching, bool o16_o18_media, std::string averagine_mf, std::string tag_mf, 
			bool thrash_or_not, bool complete_fit, bool chk_again_ch1,   Engine::HornTransform::IsotopicFittingType fit_type, Engine::TheoreticalProfile::AtomicInformation atomic_info)
		{

			mobj_mass_transform->SetOptions(max_charge, max_mw, max_fit, min_s2n, cc_mass, 
			delete_threshold_intensity, min_theoretical_intensity_for_score, num_peaks_for_shoulder, chk_again_ch1,use_mercury_caching,
			o16_o18_media) ;			

			mobj_mass_transform->SetIsotopeFitOptions(averagine_mf, tag_mf, thrash_or_not, complete_fit) ;			
			mobj_mass_transform->SetElementalIsotopeComposition(atomic_info) ; 
			mobj_mass_transform->SetIsotopeFitType(fit_type) ; 
		}
		

		bool DTAProcessor::FindPrecursorForChargeStates()
		{
			//simple check for charge state			
			double mono_mz = 0 ;
			double mono_intensity = 0 ; 
			double max_intensity = mdbl_parent_Intensity ;
			double mz_prev = 0 ; 
			double mz_next = 0 ;
			int pk_index = 0 ;
			double fwhm = 0 ;
			
			
			fwhm = mobj_parentPeak.mdbl_FWHM ; 
			
			// This has known to happen
			if (fwhm > 1)
				return false ; 
					
			//preference given to higher charge states
			try
			{
				for (int cs = 4; cs > 0; cs --)
				{
					//Look back first
					mz_prev = mdbl_parent_Mz - mdbl_cc_mass/cs ; 					
					Engine::PeakProcessing::Peak pk_prev ; 
					mobj_parent_peak_processor->mobj_peak_data->FindPeakAbsolute(mz_prev -  fwhm/cs, mz_prev +  fwhm/cs, pk_prev ) ; 
					if (pk_prev.mdbl_mz > 0 && pk_prev.mdbl_intensity>max_intensity/8)
					{				
						while (pk_prev.mdbl_mz > 0 && pk_prev.mdbl_intensity>max_intensity/8)
						{	
							mono_mz = pk_prev.mdbl_mz ;
							mono_intensity = pk_prev.mdbl_intensity;
							mz_prev = pk_prev.mdbl_mz - 1.003/cs;					
							pk_index = pk_prev.mint_peak_index ;
							if (pk_prev.mdbl_intensity > max_intensity)
								max_intensity = pk_prev.mdbl_intensity ; 
							mobj_parent_peak_processor->mobj_peak_data->FindPeakAbsolute(mz_prev -  fwhm/cs, mz_prev +  fwhm/cs, pk_prev ) ; 				
						}

						double parent_mono = (mdbl_parent_Mz - mdbl_cc_mass) * cs ; 
						double mono_mw = (mono_mz  - mdbl_cc_mass) * cs ; 
						if (abs(parent_mono - mono_mw) < 4) // to route out co-eluting peptides
						{
							mobj_transformRecord.mdbl_mz = mono_mz ; 
							mobj_transformRecord.mshort_cs = cs ; 
							mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 			
							mobj_transformRecord.mdbl_fit = 1 ; 
							mobj_transformRecord.mint_peak_index = pk_index ; 
							mobj_transformRecord.mint_mono_intensity = (int) mono_intensity ; 
							mvect_transformRecords.push_back(mobj_transformRecord) ; 
							return true; 
						}					
					}
					// Look ahead
					mz_next = mdbl_parent_Mz + 1.003/cs ;
					Engine::PeakProcessing::Peak pk_next ; 
					mobj_parent_peak_processor->mobj_peak_data->FindPeakAbsolute(mz_next -  fwhm/cs, mz_next +  fwhm/cs, pk_next ) ; 
					if (pk_next.mdbl_mz > 0 && pk_next.mdbl_intensity>max_intensity/8)
					{	
						//this is the first isotope
						mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 
						mobj_transformRecord.mshort_cs = cs ; 
						mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 				
						mobj_transformRecord.mdbl_fit = 1 ; 
						mobj_transformRecord.mint_mono_intensity = (int) mdbl_parent_Intensity ; 
						mobj_transformRecord.mint_peak_index = mobj_parentPeak.mint_peak_index ; 
						mvect_transformRecords.push_back(mobj_transformRecord) ; 
						return true ; 
					}
				}			
			}
			catch(char *mesg)
			{
				return false ; 
			}
				
			//Not found any peaks
			return false ; 
		}


		void DTAProcessor::WriteSpectrumToFile (int scan_num, std::vector <double> *mzs, std::vector <double> *intensities){
			char scanNum[4];
			sprintf(scanNum, "%d", scan_num);
			char fileName[256] ;  
				
			strcpy(fileName, "C:\\DeconMsn\\DeconTestFolder\\TestFile.spectra");
			//strcat(fileName, scanNum);
			//strcat(fileName, ".spectra");

			std::ofstream fout(fileName) ;
			fout.precision(10) ; 
			fout.setf(std::ios::floatfield, std::ios::floatfield);
			for ( int i = 0; i < mzs->size(); i++){
				fout << mzs->at(i)<<"\t"<<intensities->at(i)<<std::endl;
			}

			fout.close();		
		}


		bool DTAProcessor::DeisotopeSummedSpectra(int parent_scan_number, double parent_mz) 
		{
			bool found_transform = false;	
			
			double minMZ = parent_mz - mint_window_size;
			double maxMZ = parent_mz + mint_window_size ; 
			int mono_orig_intensity = 0 ; 

			std::vector<double> vect_mzs_full;
			std::vector<double> vect_intensities_full;
			
			//Setting to see if found_precursor worked
			bool found_precursor = false ; 
			if (mvect_transformRecords.size() > 0) 
				found_precursor = true ; 

			//check to see if it is a MS scan i.e. to say perform summing only on MS scans
			if(!mobj_raw_data_dta->IsMSScan(parent_scan_number))
				return false ; 
						
			//get raw data first
			// Still sticking to old way of summing since the fast summing didnt prove anything
			
			try
			{
				mobj_raw_data_dta->GetSummedSpectra( &vect_mzs_full, &vect_intensities_full, parent_scan_number, 2, minMZ, maxMZ) ; 
			}
			catch(char *mesg){
				return false;
			}
			//write output spectra to file
			//WriteSpectrumToFile (parent_scan_number, &vect_mzs_full, &vect_intensities_full);

			if (vect_intensities_full.size() <=1)
				return false  ; 

			// Condense regions where adjacent ions all have an intensity of 0
			std::vector<double> vect_mzs ;
			std::vector<double> vect_intensities;

			unsigned int num_pts = (unsigned int) vect_intensities_full.size() ;
			int condenseDataThreshold = 10000;
			
			if (num_pts < condenseDataThreshold)
			{
				// Keep all of the data
				vect_mzs = vect_mzs_full;
				vect_intensities = vect_intensities_full;
			}
			else
			{
				double epsilon = 1E-10;
				double mzMergeTolerancePPM = 0.25;
				double previousMZ = -1;
				double previousIntensity = -1;

				for (unsigned int i = 0; i < num_pts; i++)
				{
					bool addPeak = true;

					if (i > 0 && i < num_pts - 1)
					{
						if (vect_intensities_full[i] > epsilon || 
						   (vect_intensities_full[i] < epsilon && vect_intensities_full[i-1] > epsilon) || 
						   (vect_intensities_full[i] < epsilon && vect_intensities_full[i+1] > epsilon) )
						{
						
							if (previousMZ > -1)
							{
								double deltaMZ = vect_mzs_full.at(i) - previousMZ;
								double deltaPPM = deltaMZ / (previousMZ / 1E6);
								if (deltaPPM < mzMergeTolerancePPM)
								{
									if (vect_intensities_full.at(i) > previousIntensity)
									{
										// Replace the previous data point with this data point
										vect_mzs.pop_back();
										vect_intensities.pop_back();
									}
									else
									{
										addPeak = false;
									}
								}
							}	
						} 
						else
						{
							addPeak = false;
						}
					}

					if (addPeak)
					{
						vect_mzs.push_back(vect_mzs_full.at(i));
						vect_intensities.push_back(vect_intensities_full.at(i));

						previousMZ = vect_mzs.back();
						previousIntensity = vect_intensities.back();
					}

				}
			}

			unsigned int num_pts2 = (unsigned int) vect_intensities.size() ; 
			
			//discover peaks
			//computes the average of all points in the spectrum (all points below FLT_MAX)
			double thres =  DeconEngine::Utils::GetAverage(vect_intensities, FLT_MAX) ; 
			
			//compute the average of all points below that are below 5 times the average of all points in the spectrum
			double background_intensity = DeconEngine::Utils::GetAverage(vect_intensities, (float)(thres)*5) ;			

			//ARS added the correct calculation for noise floor level from a Decon2LS presentation
			//double calc_background_intensity = DeconEngine::Utils::GetBackgroundLevel(vect_intensities, FLT_MAX);

			mobj_summed_peak_processor->SetPeakIntensityThreshold( background_intensity *5) ; //hard-coded now, need to change it later
			mobj_summed_peak_processor->SetPeakDataIsProfileMode(mobj_raw_data_dta->IsProfileScan(parent_scan_number)) ;

			int numPeaks = mobj_summed_peak_processor->DiscoverPeaks(&vect_mzs, &vect_intensities) ; 					
			mobj_summed_peak_processor->mobj_peak_data->InitializeUnprocessedPeakData() ;	
			
			//std::cout << parent_scan_number << "\t" << calc_background_intensity << "\t" << numPeaks << std::endl;

			//Get ParentPeak and index in this peak processor
			double parent_Mz_match = mobj_summed_peak_processor->GetClosestPeakMz(parent_mz, mobj_parentPeak);
			if (mobj_parentPeak.mdbl_intensity< mdbl_min_peptide_intensity)
			{				
				return false ; 
			}					

			int parent_index = mobj_parentPeak.mint_peak_index ; 
			

			try
			{
				// Make sure that checking against charge 1 is false
				short maxcs ; 
				double maxmw ; 
				double maxfit ; 
				double mins2n ; 
				double ccmass ; 
				double delthintensity ; 
				double minthscore ; 
				short numpeaks ; 
				bool chkcharge1 ; 
				bool newchkcharge1 ; 
				bool usemercury ; 
				bool o16018media ; 
				mobj_mass_transform->GetOptions(maxcs, maxmw, maxfit, mins2n, ccmass, delthintensity, minthscore, numpeaks, chkcharge1, usemercury, o16018media) ; 

				newchkcharge1 = false ; 

				mobj_mass_transform->SetOptions(maxcs, maxmw, maxfit, mins2n, ccmass, delthintensity, minthscore, 
					numpeaks, newchkcharge1, usemercury, o16018media) ; 


				// now start THRASH all over again
				Engine::PeakProcessing::Peak currentPeak ;			
				Engine::HornTransform::IsotopeFitRecord transformRecord ;
				Engine::HornTransform::IsotopeFitRecord precursorRecord ;
				std::vector <Engine::HornTransform::IsotopeFitRecord> vectTransformRecord ; 

				bool found_peak = mobj_summed_peak_processor->mobj_peak_data->GetNextPeak(minMZ, maxMZ, currentPeak) ;
				double fwhm_SN = currentPeak.mdbl_FWHM ;
				int numDeisotoped = 0 ; 

				while(found_peak)
				{	
					found_transform = mobj_mass_transform->FindTransform(*mobj_summed_peak_processor->mobj_peak_data, currentPeak, transformRecord) ;
					if (found_transform)
					{	
						numDeisotoped++ ;
						vectTransformRecord.push_back(transformRecord) ;					
					}
					found_peak = mobj_summed_peak_processor->mobj_peak_data->GetNextPeak(minMZ, maxMZ, currentPeak) ;
				}

				// not rejecting precusor record here as the indices are going to be off
				// also algo is changed to use only THRASH if both cs are equal
				bool found_transform_record = false ; 			
				for (int recordNum = 0 ; recordNum < numDeisotoped; recordNum++)
				{			
					transformRecord = vectTransformRecord[recordNum] ; 
					int num_isotopes = transformRecord.mint_num_isotopes_observed ; 
					for (int isotope_num = 0 ; isotope_num < num_isotopes && !found_transform_record; isotope_num++)
					{
						int isotopeIndex = transformRecord.marr_isotope_peak_indices[isotope_num] ; 						
						if (isotopeIndex == parent_index) 
						{	
							found_transform_record = true;
							mobj_transformRecord = transformRecord ; 						
							break ; 
						}
					}				
				}
				if (found_transform_record)
				{
					// set mono intensity to replace the one that summed spectra would return
					Engine::PeakProcessing::Peak tempPeak ; 
					double mono_match = mobj_parent_peak_processor->GetClosestPeakMz(mobj_transformRecord.mdbl_mz, tempPeak);						
					if (tempPeak.mdbl_intensity> mdbl_min_peptide_intensity)
					{				
						mono_orig_intensity = (int) tempPeak.mdbl_intensity ; 
					}
					else
					{
						// check to see if the mono is the parent here
						double mono_mz = (mobj_transformRecord.mdbl_mono_mw)/mobj_transformRecord.mshort_cs + mdbl_cc_mass ; 
						if(abs(mono_mz-mdbl_parent_Mz) < 0.01)
							mono_orig_intensity = (int) mdbl_parent_Intensity ; 
						else
							mono_orig_intensity = (int) mdbl_min_peptide_intensity ; 
					}
					
					mobj_transformRecord.mint_mono_intensity = mono_orig_intensity ; 


					// store in vector and clear
					mvect_transformRecords.push_back(mobj_transformRecord) ;							
					vect_intensities.clear() ; 
					vect_mzs.clear() ; 
					mobj_mass_transform->SetOptions(maxcs, maxmw, maxfit, mins2n, ccmass, delthintensity, minthscore, 
					numpeaks, chkcharge1, usemercury, o16018media) ; //reset chk_charge1
					return true ; 				
				}
				
				vect_intensities.clear() ; 
				vect_mzs.clear() ; 
				mobj_mass_transform->SetOptions(maxcs, maxmw, maxfit, mins2n, ccmass, delthintensity, minthscore, 
					numpeaks, chkcharge1, usemercury, o16018media) ; //reset chk_charge1
				return false ; 			
			}
			catch(char*mesg)
			{
				return false ; 
			}
		}


		/**
		Implemented support for zoom scans, the strategy here would be to assume that the peak with the highest intensity 
		was selected for fragmentation. Future work would be to be smart about precursor selection in zoom scans
		*/

		bool DTAProcessor::GenerateDTAZoomScans(int msN_scan_number, int parent_scan_number, int msN_scan_index){

			bool found_precursor = true;

			
			Engine::Utilities::SavGolSmoother smoother; 
			smoother.SetOptions(16, 16, 4);
			smoother.Smooth(&mvect_mzs_parent, &mvect_intensities_parent);
			WriteSpectrumToFile(parent_scan_number,&mvect_mzs_parent, &mvect_intensities_parent);

			//mobj_parent_peak_processor->ApplyMovingAverageFilter(&mvect_mzs_parent, &mvect_intensities_parent, 3) ;

			int numPeaks = mobj_parent_peak_processor->DiscoverPeaks(&mvect_mzs_parent, &mvect_intensities_parent) ; 			

			if ( numPeaks > 0 ){
				found_precursor = GenerateDTALowRes(msN_scan_number, parent_scan_number, msN_scan_index);		
			}			
			else{
				found_precursor = false;
			}
		
			return found_precursor;
		}
		
		bool DTAProcessor::GenerateDTA(int msN_scan_number, int parent_scan_number)
		{
			bool found_transform = false;	
			bool found_transform_record = false ; 
			bool found_precursor = false ; 
			bool found_transform_thru_summing = false ; 
			bool found_other_precursors = false ; 

					
			// Clear previous and reset objects
			mvect_transformRecords.clear() ; 
			mdbl_parent_Intensity = 0;
			mdbl_parent_Mz = 0;
			mobj_transformRecord.mdbl_mz = 0 ; 
			mobj_transformRecord.mshort_cs = 0 ; 
			mobj_transformRecord.mdbl_mono_mw  = 0.0 ; 
			mobj_transformRecord.mdbl_fit = -1 ; 
			mobj_transformRecord.mint_mono_intensity = 0 ;  
			mobj_msn_record.mdbl_mono_mw = 0.0 ; 
			mobj_msn_record.mint_mono_intensity = 0 ; 
			mobj_msn_record.mdbl_mono_mz = 0 ; 
			mobj_msn_record.mshort_cs = 0 ; 
			mobj_msn_record.mdbl_fit = -1 ; 		
						
			//check if we have enough values in the ms2						
			int numPeaks = mvect_mzs_msN.size();
			if (numPeaks < mint_minIonCount)
			{
				std::cout<<"Skipping scan "<<msN_scan_number<<" since too few peaks: "<<numPeaks<<" < "<<mint_minIonCount<<std::endl;
				return found_transform;	
			}

			mint_NumMSnScansProcessed++ ; 

			//get parent
			double parent_mz = mobj_raw_data_dta->GetParentMz(msN_scan_number) ; 
	
			// if -C option
			/*if (mint_consider_charge > 0)
			{
				// see if we can detect the parent first to get intensity right
				double parent_match = mobj_parent_peak_processor->GetClosestPeakMz(parent_mz, mobj_parentPeak);						
				if (mobj_parentPeak.mdbl_intensity> mdbl_min_peptide_intensity)
				{				
					mdbl_parent_Mz = mobj_parentPeak.mdbl_mz;			
					mdbl_parent_Intensity = mobj_parentPeak.mdbl_intensity;
				}
				else
				{
					mdbl_parent_Mz = parent_mz ; 
					mdbl_parent_Intensity = mdbl_min_peptide_intensity ; 
				}				
				mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 				
				mobj_transformRecord.mshort_cs = mint_consider_charge ;				
				mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
				mobj_transformRecord.mdbl_fit = 1 ; 
				mobj_transformRecord.mint_mono_intensity = (int) mdbl_parent_Intensity ; 
				mvect_transformRecords.push_back(mobj_transformRecord) ; 
				return true; 
			}*/			

			//start					
			double parent_Mz_match = mobj_parent_peak_processor->GetClosestPeakMz(parent_mz, mobj_parentPeak);						
			if (mobj_parentPeak.mdbl_intensity> mdbl_min_peptide_intensity)
			{				
				mdbl_parent_Mz = mobj_parentPeak.mdbl_mz;			
				mdbl_parent_Intensity = mobj_parentPeak.mdbl_intensity;			
				int parent_index = mobj_parentPeak.mint_peak_index ; 				
				
				// Check for CS based detection first 
				found_precursor = FindPrecursorForChargeStates() ; 
					
				double minMZ = parent_Mz_match - mint_window_size ; 
				double maxMZ = parent_Mz_match + mint_window_size ; 
					
				Engine::PeakProcessing::Peak currentPeak ;			
				Engine::HornTransform::IsotopeFitRecord transformRecord ;
				Engine::HornTransform::IsotopeFitRecord precursorRecord ;
				std::vector <Engine::HornTransform::IsotopeFitRecord> vectTransformRecord ; 

				bool found_peak = mobj_parent_peak_processor->mobj_peak_data->GetNextPeak(minMZ, maxMZ, currentPeak) ;
				double fwhm_SN = currentPeak.mdbl_FWHM ;
				int numDeisotoped = 0 ; 

 				while(found_peak)
				{
					if (currentPeak.mdbl_intensity < mdbl_min_peptide_intensity)
						break ;					
					found_transform = mobj_mass_transform->FindTransform(*mobj_parent_peak_processor->mobj_peak_data, currentPeak, transformRecord) ;
					if (found_transform)
					{	
						numDeisotoped++ ;
						vectTransformRecord.push_back(transformRecord) ;					
					}
					found_peak = mobj_parent_peak_processor->mobj_peak_data->GetNextPeak(minMZ, maxMZ, currentPeak) ;
				}

				if (numDeisotoped == 1)
				{
					// only one so get it in
					found_transform_record = false ; 
					transformRecord = vectTransformRecord[0] ; 
					int num_isotopes = transformRecord.mint_num_isotopes_observed ; 
					if (transformRecord.mdbl_fit < mdbl_min_fit_for_single_spectra) // AM: to give slight edge to summing
					{

						for (int isotope_num = 0 ; isotope_num < num_isotopes && !found_transform_record; isotope_num++)
						{
							int isotopeIndex = transformRecord.marr_isotope_peak_indices[isotope_num] ; 						
							if (isotopeIndex == parent_index) 
							{	
								found_transform_record = true;
								mobj_transformRecord = transformRecord ; 						
								break ; 
							}
						}		
					}
					if (found_transform_record)
					{					
						mvect_transformRecords.push_back(mobj_transformRecord) ;							
						return true ; 				
					}
				}
				else if (numDeisotoped > 1 )
				{
					if (!mbln_consider_multiple_precursors)
					{
						// more than one and consider only one precursor, so might be co-eluting peptides
						// see if we mave a match first
						found_transform_record = false ; 
						for (int transform_num = 0 ; transform_num < (int)vectTransformRecord.size() && !found_transform_record ; transform_num++)
						{
							transformRecord = vectTransformRecord[transform_num] ; 
							int num_isotopes = transformRecord.mint_num_isotopes_observed ; 
							if (transformRecord.mdbl_fit < mdbl_min_fit_for_single_spectra) // AM: to give slight edge to summing
							{
								for (int isotope_num = 0 ; isotope_num < num_isotopes; isotope_num++)
								{						
									int isotopeIndex = transformRecord.marr_isotope_peak_indices[isotope_num] ; 						
									if (isotopeIndex == parent_index) 
									{	
										found_transform_record = true;
										mobj_transformRecord = transformRecord ; 								
										break ; 
									}						
								}
							}
						}				
						if (found_transform_record)
						{
							if (found_precursor)
							{
								// found both THRASH and precurosr, so if precusor is present in any of the 
								// transform records eliminate it
								bool found_precursor_record = false ; 
								precursorRecord = mvect_transformRecords[0] ; 
								for (int transform_num = 0 ; transform_num < (int)vectTransformRecord.size() && !found_precursor_record ; transform_num++)
								{
									transformRecord = vectTransformRecord[transform_num] ; 
									int num_isotopes = transformRecord.mint_num_isotopes_observed ; 
									for (int isotope_num = 0 ; isotope_num < num_isotopes; isotope_num++)
									{
										int isotopeIndex = transformRecord.marr_isotope_peak_indices[isotope_num] ; 						
										if (isotopeIndex == precursorRecord.mint_peak_index) 
										{	
											//found it, eject it
											found_precursor_record = true;
											mvect_transformRecords.clear()  ; 
											mvect_transformRecords.push_back(mobj_transformRecord) ;
											break ; 
										}
									}
								}		
							}
							else
							{
								mvect_transformRecords.push_back(mobj_transformRecord) ;							
								return true ; 
							}
						}
					}
					else
					{
						//consider multiple precursors in on, so spit dtas for all precursors with isolation_window_szie from parent peak
						for (int transform_num = 0 ; transform_num < (int)vectTransformRecord.size() && !found_transform_record ; transform_num++)
						{
							transformRecord = vectTransformRecord[transform_num] ;
							if (abs(transformRecord.mdbl_mz - parent_Mz_match) < mint_isolation_window_size)
							{
								mvect_transformRecords.push_back(transformRecord) ; 
								found_other_precursors = true ; 
							}
						}

						if (found_other_precursors)
							return true ; 
					}
				}
			}
			else
			{
				// to set parent intensity find the parent that is closest to the reported parent mz
				Engine::PeakProcessing::Peak tempPeak ; 	
				tempPeak.mdbl_intensity = -1 ; 
				tempPeak.mdbl_mz = -1 ; 
				mobj_parent_peak_processor->mobj_peak_data->FindPeak(parent_mz-0.02, parent_mz+0.02, tempPeak) ; 
				if (tempPeak.mdbl_intensity > 0 )
				{
					mdbl_parent_Mz = tempPeak.mdbl_mz ; 
					mdbl_parent_Intensity = tempPeak.mdbl_intensity ; 
				}
				else
				{
					mdbl_parent_Mz = parent_mz ; 
					mdbl_parent_Intensity = mdbl_min_peptide_intensity ; 
				}
			}
			
			
			if (!found_transform_record)
			{
				// Either THRASH failed or Peak was below noise floor for that scan so sum spectra
				GetParentScanSpectra(parent_scan_number, mdbl_pkBkgRatio, mdbl_peptideMinBkgRatio);	
				found_transform_thru_summing = DeisotopeSummedSpectra(parent_scan_number, parent_mz) ; 
			}

			if (found_transform_thru_summing)
			{					
					return true ; 
			}				
							
			// passed conditions, couldn't THRASH nor FindPeak	
			if (!found_precursor)
			{
				// assign default charge states 2 and 3
				mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 
				mobj_transformRecord.mshort_cs = 2 ;				
				mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
				mobj_transformRecord.mdbl_fit = 1 ; 
				mobj_transformRecord.mint_mono_intensity = (int)mdbl_parent_Intensity ; 
				mvect_transformRecords.push_back(mobj_transformRecord) ; 
				mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 
				mobj_transformRecord.mshort_cs = 3 ;				
				mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
				mobj_transformRecord.mdbl_fit = 1 ; 
				mobj_transformRecord.mint_mono_intensity= (int)  mdbl_parent_Intensity ; 
				mvect_transformRecords.push_back(mobj_transformRecord) ; 					
 				return true ; 				
			}	

			return true ; 			
		}
				
		
		bool DTAProcessor::GenerateDTALowRes(int msN_scan_number, int parent_scan_number, int msN_scan_index)
		{
			
			//Apply a heavy smoothing using savitsky golay filter
			Engine::Utilities::SavGolSmoother smoother; 
			smoother.SetOptions(16, 16, 4);
			smoother.Smooth(&mvect_mzs_parent, &mvect_intensities_parent);

			int numPeaks = mobj_parent_peak_processor->DiscoverPeaks(&mvect_mzs_parent, &mvect_intensities_parent) ; 			
			
			if ( numPeaks <= 0 ){
				return false;
			}


			mvect_transformRecords.clear() ; 
			mdbl_parent_Intensity = 0;
			mdbl_parent_Mz = 0;			
			bool chargeOne = false;
			typedef std::pair <int, int> mapEntry;
            			
			//check if we have enough values in the ms2						
			numPeaks = mvect_mzs_msN.size();
			if (numPeaks < mint_minIonCount)
				return false ;

			// Get the parent m/z for that msn scan					
			double parent_Mz = mobj_raw_data_dta->GetParentMz(msN_scan_number);		
			
			
			// Now start
			Engine::PeakProcessing::Peak parentPeak;
			double parent_Mz_match = mobj_parent_peak_processor->GetClosestPeakMz(parent_Mz, mobj_parentPeak);
			if (mobj_parentPeak.mdbl_intensity< mdbl_min_peptide_intensity)
			{
				mdbl_parent_Mz = parent_Mz;
				Engine::PeakProcessing::Peak tempPeak ; 
				tempPeak.mdbl_intensity = -1.0 ; 
				tempPeak.mdbl_mz = -1.0 ; 
				mobj_parent_peak_processor->mobj_peak_data->FindPeak(parent_Mz-0.1, parent_Mz+0.1, tempPeak) ; 
				if (tempPeak.mdbl_intensity > 0 )
					mdbl_parent_Intensity = tempPeak.mdbl_intensity ; 
				else
					mdbl_parent_Intensity = mdbl_min_peptide_intensity ; 
				parentPeak.mdbl_mz = parent_Mz ; 
				parentPeak.mdbl_intensity = mdbl_parent_Intensity ; 						
			}
			else
			{
				mdbl_parent_Mz = parent_Mz_match ;	
				mdbl_parent_Intensity = mobj_parentPeak.mdbl_intensity ; 
				parentPeak.mdbl_mz = parent_Mz_match ; 
				parentPeak.mdbl_intensity = mobj_parentPeak.mdbl_intensity ; 				
			}	

			mint_NumMSnScansProcessed++ ; 

			// if -C option
			/*if (mint_consider_charge > 0)
			{					
				mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 				
				mobj_transformRecord.mshort_cs = mint_consider_charge ;				
				mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
				mobj_transformRecord.mdbl_fit = 1 ; 
				mobj_transformRecord.mint_mono_intensity = mdbl_parent_Intensity ; 
				mvect_transformRecords.push_back(mobj_transformRecord) ; 
				return true; 
			}*/	

								
			//bypassed all test, proceed					
			//check if +1
			chargeOne = mobj_svm_charge_determination->IdentifyIfChargeOne(&mvect_mzs_msN, &mvect_intensities_msN, parentPeak, parent_scan_number);
			if (chargeOne)
			{
				
				mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 
				mobj_transformRecord.mshort_cs = 1 ;				
				mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
				mobj_transformRecord.mdbl_fit = 1 ; 
				mobj_transformRecord.mint_mono_intensity = mdbl_parent_Intensity ; 
				mvect_transformRecords.push_back(mobj_transformRecord) ; 
				return true; 				
			}
			else
			{					
				//get features and add to feature space				
				mobj_svm_charge_determination->GetFeaturesForSpectra(&mvect_mzs_msN, &mvect_intensities_msN, parentPeak, msN_scan_number);
				mapEntry m1(msN_scan_number, mint_msNscanIndex);							
				mmap_msN_parentIndex.insert(m1);
				mint_msNscanIndex++;			

				if (IsZoomScan(parent_scan_number)){
					mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 
					mobj_transformRecord.mshort_cs = 2 ;				
					mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
					mobj_transformRecord.mdbl_fit = 1 ; 
					mobj_transformRecord.mint_mono_intensity = mdbl_parent_Intensity ; 
					mvect_transformRecords.push_back(mobj_transformRecord) ; 
	
					mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 
					mobj_transformRecord.mshort_cs = 3 ;				
					mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
					mobj_transformRecord.mdbl_fit = 1 ; 
					mobj_transformRecord.mint_mono_intensity = mdbl_parent_Intensity ; 
					mvect_transformRecords.push_back(mobj_transformRecord) ; 
					//return true;
				}

				return false ; 
			}
		}

		void DTAProcessor::DetermineChargeForEachScan()
		{			
			mobj_svm_charge_determination->NormalizeDataSet();
			mobj_svm_charge_determination->DetermineClassForDataSet();				
		}

		void DTAProcessor::InitializeSVM(char *file_name)
		{
			mobj_svm_charge_determination->SetSVMParamFile(file_name) ; 
			mobj_svm_charge_determination->LoadSVMFromXml();
			mobj_svm_charge_determination->InitializeLDA();
		}

		
		bool DTAProcessor::ContainsProfileData(int parent_scan)
		{
			return mobj_raw_data_dta->IsProfileScan(parent_scan);
		}

		bool DTAProcessor::IsZoomScan(int parent_scan)
		{
			if (menm_dataset_type == Engine::Readers::FINNIGAN)
				return mobj_raw_data_dta->IsZoomScan(parent_scan) ; 

			return false ; 
		}

		int DTAProcessor::GetSpectraType(int msN_scan_number){
			return mobj_raw_data_dta->GetSpectrumType(msN_scan_number);
		}

		bool DTAProcessor::IsFTData(int parent_scan)
		{
			if (menm_dataset_type == Engine::Readers::FINNIGAN)
				return mobj_raw_data_dta->IsFTScan(parent_scan) ; 
			else if (menm_dataset_type == Engine::Readers::MZXMLRAWDATA)
			{
				bool set_FT = false ; 
				set_FT = mobj_raw_data_dta->IsFTScan(parent_scan);
				if (set_FT)
					return set_FT ; 
				else
				{	
					// return user preference
					return mbln_is_profile_data_for_mzXML ; 
				}
			}

			return false ; 
		}

		
		void DTAProcessor::GetMsNSpectra(int msN_scan_number, double peakBkgRatio, double peptideMinBkgRatio)
		{
			mvect_intensities_msN.clear();
			mvect_mzs_msN.clear();

			mobj_raw_data_dta->GetRawData(&mvect_mzs_msN, &mvect_intensities_msN, msN_scan_number, mbln_centroid_msn);

			double thres =  DeconEngine::Utils::GetAverage(mvect_intensities_msN, FLT_MAX) ; 
			double background_intensity;			

			if (mbln_centroid_msn)
			{
				mobj_msN_peak_processor->SetPeakDataIsProfileMode(false);
				background_intensity = DeconEngine::Utils::GetAverage(mvect_intensities_msN, (float)(5*thres)) ;
			}
			else
			{
				mobj_msN_peak_processor->SetPeakDataIsProfileMode(mobj_raw_data_dta->IsProfileScan(msN_scan_number));
				background_intensity = DeconEngine::Utils::GetAverage(mvect_intensities_msN, (float)(5*thres)) ;
			}

			mobj_msN_peak_processor->SetPeakIntensityThreshold(background_intensity * peakBkgRatio) ; 

			int numPeaks = mobj_msN_peak_processor->DiscoverPeaks(&mvect_mzs_msN, &mvect_intensities_msN) ; 			
			mobj_msN_peak_processor->mobj_peak_data->InitializeUnprocessedPeakData() ;				
		}
		
		void DTAProcessor::GetParentScanSpectra(int parent_scan_number,  double peakBkgRatio, double peptideMinBkgRatio)
		{
			mvect_intensities_parent.clear();
			mvect_mzs_parent.clear();
			
			mobj_raw_data_dta->GetRawData(&mvect_mzs_parent, &mvect_intensities_parent, parent_scan_number, false);
			
			double thres =  DeconEngine::Utils::GetAverage(mvect_intensities_parent, FLT_MAX) ; 
			double background_intensity = DeconEngine::Utils::GetAverage(mvect_intensities_parent, (float)(5*thres)) ;
			mobj_parent_peak_processor->SetPeakIntensityThreshold(background_intensity * peakBkgRatio) ; 
			mobj_parent_peak_processor->SetPeakDataIsProfileMode(mobj_raw_data_dta->IsProfileScan(parent_scan_number)) ;

			int numPeaks = mobj_parent_peak_processor->DiscoverPeaks(&mvect_mzs_parent, &mvect_intensities_parent) ; 
			mdbl_min_peptide_intensity = background_intensity * peptideMinBkgRatio ; 
			mobj_parent_peak_processor->mobj_peak_data->InitializeUnprocessedPeakData() ;											
		}


		void DTAProcessor::WriteToMGF(int msN_scan_num, int parent_scan_num)
		{
			//second line
			char scanNum[16] ;
			char charge[16] ;				
			double massplusH = 0 ;
			int numTransforms = mvect_transformRecords.size() ; 		
			
			//check size, else has failed params
			if (numTransforms == 0)
				return ; 

			try
			{
				mobj_transformRecord = mvect_transformRecords[0] ; 				

				// now there can only be two Entries - one through findPeak @[0] and other through THRASH @[1]
				for (int transformNum = 0 ; transformNum < numTransforms; transformNum++)
				{
					mobj_transformRecord = mvect_transformRecords[transformNum] ; 				
					if(numTransforms > 1 && !mbln_consider_multiple_precursors)
					{
						// if different charges
						if(mvect_transformRecords[0].mshort_cs == mvect_transformRecords[1].mshort_cs)
						{
							//changed so that THRASH is preferred by default 6/12/07
							mobj_transformRecord = mvect_transformRecords[1] ;
							numTransforms-- ; 						
						}	
					}
				}

				//stick to range
				if (mobj_transformRecord.mdbl_mono_mw < mdbl_minMass || mobj_transformRecord.mdbl_mono_mw > mdbl_maxMass || (mint_consider_charge != 0 && mint_consider_charge != mobj_transformRecord.mshort_cs))
				{											
					return ; 					
				}

				mint_NumDTARecords++ ; 

				std::cout<<msN_scan_num<<"."<<msN_scan_num<<"."<<mobj_transformRecord.mshort_cs<<std::endl;			

				int msN_scan_level = mobj_raw_data_dta->GetMSLevel(msN_scan_num) ; 
				int parent_scan_level = mobj_raw_data_dta->GetMSLevel(parent_scan_num) ; 
				CreateMSnRecord(msN_scan_num, msN_scan_level, parent_scan_num, parent_scan_level ) ; 
				CreateProfileRecord(msN_scan_num, parent_scan_num) ; 
				
				if (mbln_create_log_file_only)
					return;			
			
				//first line
				mfile_mgf<<"BEGIN IONS"<<std::endl ; 
								
				//second line			
				sprintf(scanNum, "%d", msN_scan_num);
				sprintf(charge, "%d", (int)mobj_transformRecord.mshort_cs);			
					
				char fileName[256] ; 
				strcpy(fileName, mch_dataset_name) ; 
				strcat(fileName, ".");
				strcat(fileName, scanNum);
				strcat(fileName, ".");
				strcat(fileName, scanNum);
				strcat(fileName, ".");
				strcat(fileName, charge);
				strcat(fileName, ".dta");								
				 
				mfile_mgf<<"TITLE="<<fileName<<std::endl ; 

				//third line
				double monoHmz = (mobj_transformRecord.mdbl_mono_mw + (mdbl_cc_mass*mobj_transformRecord.mshort_cs))/mobj_transformRecord.mshort_cs ; 
				mfile_mgf.precision(12) ; 
				mfile_mgf<<"PEPMASS="<<monoHmz<<std::endl;

				//fourth line
				if (numTransforms > 1)
						mfile_mgf<<"CHARGE="<<mvect_transformRecords[0].mshort_cs<<"+ and "<<mvect_transformRecords[1].mshort_cs<<"+"<<std::endl ;
				else
						mfile_mgf<<"CHARGE="<<mobj_transformRecord.mshort_cs<<"+"<<std::endl ;

				//start spectra		
				int count = (int) mvect_mzs_msN.size();
				for (int i = 0; i < count ; i++)
				{
					double mz = mvect_mzs_msN[i];
					double intensity = mvect_intensities_msN[i];					
					mfile_mgf<<std::setiosflags( std::ios::fixed | std::ios::showpoint )<<std::setprecision(5)<<mz<<" ";					
					mfile_mgf<<std::setiosflags( std::ios::fixed | std::ios::showpoint )<<std::setprecision(2)<<intensity<<std::endl;
				}
				mfile_mgf<<"END IONS"<<std::endl ; 
				mfile_mgf<<std::endl;
			}	
			catch(char *mesg)					
			{
				std::cerr<<"Error in creating ,MGF"<<std::endl ;
			}
		}

		void DTAProcessor::CreateProfileRecord(int msn_scan_num, int parent_scan_num)
		{
			double agc_acc_time = mobj_raw_data_dta->GetAGCAccumulationTime(parent_scan_num) ; 
			double tic_val = mobj_raw_data_dta->GetTICForScan(parent_scan_num) ; 
			
			mobj_profile_record.mint_msn_scan_num = msn_scan_num ; 
			mobj_profile_record.mint_parent_scan_num = parent_scan_num ; 
			mobj_profile_record.mdbl_agc_time = agc_acc_time ; 
			mobj_profile_record.mdbl_tic_val = tic_val ; 

			mvect_profile_records.push_back(mobj_profile_record) ; 
		}

		
		void DTAProcessor::CreateMSnRecord(int msn_scan_num, int msn_scan_level, int parent_scan, int parent_scan_level )
		{
			mobj_msn_record.mint_msn_scan_num = msn_scan_num ; 
			mobj_msn_record.mint_msn_scan_level = msn_scan_level ; 
			
			mobj_msn_record.mint_parent_scan_num = parent_scan ; 
			mobj_msn_record.mint_parent_scan_level = parent_scan_level ;
			
			mobj_msn_record.mdbl_parent_mz = mdbl_parent_Mz ; 
			mobj_msn_record.mdbl_mono_mz = mobj_transformRecord.mdbl_mz ;  
			mobj_msn_record.mshort_cs = mobj_transformRecord.mshort_cs ; 
			mobj_msn_record.mdbl_mono_mw = mobj_transformRecord.mdbl_mono_mw ; 
			mobj_msn_record.mdbl_fit = mobj_transformRecord.mdbl_fit ; 

			mobj_msn_record.mint_parent_intensity = (int) mdbl_parent_Intensity ; 
			if (mobj_transformRecord.mint_mono_intensity <=0)
				mobj_msn_record.mint_mono_intensity = (int) mdbl_parent_Intensity ; 
			else
                mobj_msn_record.mint_mono_intensity = mobj_transformRecord.mint_mono_intensity ; 

			mvect_msn_records.push_back(mobj_msn_record) ; 
		}

		bool SortMSnRecordsByScan(Engine::DTAProcessing::MSnInformationRecord &rec1, Engine::DTAProcessing::MSnInformationRecord &rec2)
		{
			if (rec1.mint_msn_scan_num > rec2.mint_msn_scan_num)
				return false ; 
			if (rec1.mint_msn_scan_num < rec2.mint_msn_scan_num)
				return true ; 
			return (rec1.mshort_cs < rec2.mshort_cs) ; 
		
		}

		bool SortProfileRecordsByScan(Engine::DTAProcessing::ProfileRecord &rec1, Engine::DTAProcessing::ProfileRecord &rec2)
		{
			if (rec1.mint_msn_scan_num > rec2.mint_msn_scan_num)
				return false ; 
			if (rec1.mint_msn_scan_num < rec2.mint_msn_scan_num)
				return true ; 
			
			return true ; 			
		}

		void DTAProcessor::WriteProfileFile()
		{
			std::ofstream fout(mch_profile_filename) ; 
			
			fout<<"MSn_Scan"<<"\t"<<"Parent_Scan"<<"\t"<<"AGC_accumulation_time"<<"\t"<<"TIC"<<std::endl ; 
			
			// sort all records wrt scan
			std::sort(mvect_profile_records.begin(), mvect_profile_records.end(), SortProfileRecordsByScan); 
			
			
			// now sorted output all 
			fout.precision(4) ; 
			fout.setf(std::ios::fixed, std::ios::floatfield);

			for (int i = 0 ; i < mvect_profile_records.size() ; i++)
			{
				mobj_profile_record = mvect_profile_records[i] ; 

				fout<<mobj_profile_record.mint_msn_scan_num<<"\t"<<mobj_profile_record.mint_parent_scan_num ; 
				fout<<"\t"<<mobj_profile_record.mdbl_agc_time<<"\t"<<mobj_profile_record.mdbl_tic_val<<std::endl ; 
			}
			fout.close() ; 
			
		}

		void DTAProcessor::WriteLogFile()
		{
			// get date and time
		
			std::time_t rawtime;
			std::tm* timeinfo;
			char timeStampBuffer [80];

			std::time(&rawtime);
			timeinfo = std::localtime(&rawtime);

			std::strftime(timeStampBuffer,80,"%Y-%m-%d %I:%M:%S %p",timeinfo);
		
			std::ofstream fout(mch_log_filename) ; 

			//TODO: Version number is hardcoded and needs to be read off assembly file
			fout<<"DeconMSn Version:"<<"2.3.1.3"<<std::endl ; 

			fout<<"Dataset:"<<mch_dataset_name<<std::endl ; 
			fout<<"Number of MSn scans processed:"<<mint_NumMSnScansProcessed<<std::endl ;
			fout<<"Number of DTAs generated:"<<mint_NumDTARecords<<std::endl ; 			
			fout<<"Date/Time:"<<timeStampBuffer<<std::endl ; 
			fout<<"-----------------------------------------------------------"<<std::endl<<std::endl<<std::endl ; 
			
			fout<<"MSn_Scan"<<"\t"<<"MSn_Level"<<"\t"<<"Parent_Scan"<<"\t"<<"Parent_Scan_Level"<<"\t"<<"Parent_Mz" ; 
			fout<<"\t"<<"Mono_Mz"<<"\t"<<"Charge_State"<<"\t"<<"Monoisotopic_Mass"<<"\t"<<"Isotopic_Fit"; 
			fout<<"\t"<<"Parent_Intensity"<<"\t"<<"Mono_Intensity"<<"\n";	

			// sort all records wrt scan
			std::sort(mvect_msn_records.begin(), mvect_msn_records.end(), SortMSnRecordsByScan) ; 
			
			// now sorted output all 
			fout.precision(4) ; 
			fout.setf(std::ios::fixed, std::ios::floatfield);

			for (int i = 0 ; i < mvect_msn_records.size() ; i++)
			{
				mobj_msn_record = mvect_msn_records[i] ; 

				fout<<mobj_msn_record.mint_msn_scan_num<<"\t"<<mobj_msn_record.mint_msn_scan_level<<"\t"<<mobj_msn_record.mint_parent_scan_num ; 
				fout<<"\t"<<mobj_msn_record.mint_parent_scan_level<<"\t"<<mobj_msn_record.mdbl_parent_mz ; 
				fout<<"\t"<<mobj_msn_record.mdbl_mono_mz<<"\t"<<mobj_msn_record.mshort_cs; 
				fout<<"\t"<<mobj_msn_record.mdbl_mono_mw<<"\t"<<mobj_msn_record.mdbl_fit ; 
				fout<<"\t"<<mobj_msn_record.mint_parent_intensity<<"\t"<<mobj_msn_record.mint_mono_intensity<<"\n" ; 
			}
			fout.close() ; 
		}

		// percentComplete is a value between 0 and 100
		void DTAProcessor::WriteProgressFile(int scansProcessed, int totalScans, int percentComplete)
		{
			// get date and time
		
			std::time_t rawtime;
			std::tm* timeinfo;
			char timeStampBuffer [80];

			std::time(&rawtime);
			timeinfo = std::localtime(&rawtime);

			std::strftime(timeStampBuffer,80,"%Y-%m-%d %I:%M:%S %p",timeinfo);

			try 
			{
				std::ofstream fout(mch_progress_filename) ; 
				fout<<"Percent complete:              "<<percentComplete<<"%"<<std::endl ; 			
				fout<<"Date/Time:                     "<<timeStampBuffer<<std::endl ;
				fout<<"Number of MSn scans processed: "<<scansProcessed<<std::endl ;
				fout<<"Total scans:                   "<<totalScans<<std::endl ; 
				fout<<"Dataset: "<<mch_dataset_name<<std::endl ; 
				fout.close() ; 
			}
			catch (char *mesg)
			{
				std::cerr<<"Exception writing progress to "<<mch_progress_filename<<": "<<mesg<<std::endl ;
			}
			
		}

		void DTAProcessor::WriteDTAFile(int msN_scan_num, int parent_scan_num)
		{
			//create file_name
			char scanNum[16] ;
			char charge[16] ;				
			double massplusH = 0 ;
			int numTransforms = mvect_transformRecords.size() ; 			

			char chArray[512] ; 
			
			if(mbln_consider_multiple_precursors)
				throw new System::Exception(S"Can only consider multiple precursors for MGF creation. Change param value to false. ") ; 
			
			//check size, else has failed params
			if (numTransforms == 0)
				return ; 

			// now there can only be two Entries - one through findPeak @[0] and other through THRASH @[1]
			for (int transformNum = 0 ; transformNum < numTransforms; transformNum++)
			{
				mobj_transformRecord = mvect_transformRecords[transformNum] ; 				
				if(numTransforms > 1)
				{
					// if same charges
					if(mvect_transformRecords[0].mshort_cs == mvect_transformRecords[1].mshort_cs)
					{						
						//changed so that THRASH is preferred by default 6/12/07
						mobj_transformRecord = mvect_transformRecords[1] ;
						numTransforms-- ; 						
					}	
				}	

				
				if ( mint_consider_charge != 0 && (int)mobj_transformRecord.mshort_cs != mint_consider_charge ){
					return;
				}

				//stick to range
				if (mobj_transformRecord.mdbl_mono_mw < mdbl_minMass || mobj_transformRecord.mdbl_mono_mw > mdbl_maxMass)
				{											
					//return;
					continue ; 
				}

				mint_NumDTARecords++ ; 
				

				// to get mono_mass + H
				massplusH = mobj_transformRecord.mdbl_mono_mw + mdbl_cc_mass;

				
				std::cout<<msN_scan_num<<"."<<msN_scan_num<<"."<<mobj_transformRecord.mshort_cs<<std::endl;

				bool is_profile = false; 

				/* FOR Tom Metz
				if (mobj_raw_data_dta->IsProfileScan(msN_scan_num))
					is_profile  = true ; 
				else
					is_profile = false ; */		
				
				int msN_scan_level = mobj_raw_data_dta->GetMSLevel(msN_scan_num) ; 
				int parent_scan_level = mobj_raw_data_dta->GetMSLevel(parent_scan_num) ; 				
				CreateMSnRecord(msN_scan_num, msN_scan_level, parent_scan_num, parent_scan_level ) ; 
				CreateProfileRecord(msN_scan_num, parent_scan_num) ; 

				if (mbln_create_log_file_only)
					continue ;	
								
				sprintf(scanNum, "%04d", msN_scan_num);


				sprintf(charge, "%d", (int)mobj_transformRecord.mshort_cs);			


				char fileName[256] ;  
				strcpy(fileName, mch_output_file) ; 
				
				/*// Purely for Tom Metz's data
				if (is_profile)
					strcat(fileName, "_FTMS") ; 
				else
					strcat(fileName, "_ITMS") ; */	

				strcat(fileName, ".");
				strcat(fileName, scanNum);
				strcat(fileName, ".");
				strcat(fileName, scanNum);
				strcat(fileName, ".");
				strcat(fileName, charge);
				strcat(fileName, ".dta");				

				// for composite dta
				if (mbln_create_composite_dta)
				{
					if (!mbln_first_scan_written)
					{
						mfile_comb_dta<<std::endl ; 
						mbln_first_scan_written = true;
					}

					//fancy headers
					strcpy(chArray, "=================================== ") ; 
					strcat(chArray, "\"") ;					

					char datasetName[256] ; 
					strcpy(datasetName, mch_dataset_name) ; 
					strcat(datasetName, ".");
					strcat(datasetName, scanNum);
					strcat(datasetName, ".");
					strcat(datasetName, scanNum);
					strcat(datasetName, ".");
					strcat(datasetName, charge);
					strcat(datasetName, ".dta");	
					
					strcat(chArray, datasetName) ; 
					strcat(chArray, "\"") ; 
					strcat(chArray, " ==================================") ; 
					mfile_comb_dta<<chArray<<std::endl ; 


					// massH and cs
					mfile_comb_dta.precision(8) ; 
					mfile_comb_dta<<massplusH<<" "<<mobj_transformRecord.mshort_cs<<" "<<" "<<" " ; 
					mfile_comb_dta<<"scan="<<msN_scan_num<<" "<<"cs="<<mobj_transformRecord.mshort_cs<<std::endl;

					int numPoints = (int) mvect_mzs_msN.size();
					for (int i = 0; i < numPoints; i++)
					{
						double mz = mvect_mzs_msN[i];
						double intensity = mvect_intensities_msN[i];

						if (intensity == 0)
						{
							if (i > 0 && mvect_intensities_msN[i-1] == 0)
							{
								if (i == numPoints - 1)
									continue;
								else if (mvect_intensities_msN[i+1] == 0)
									continue;
							}
						}

						try
						{
							mfile_comb_dta<<std::setiosflags( std::ios::fixed | std::ios::showpoint )<<std::setprecision(5)<<mz<<" ";						
							mfile_comb_dta<<std::setiosflags( std::ios::fixed | std::ios::showpoint )<<std::setprecision(2)<<intensity<<std::endl;
						}
						catch (char *mesg)
						{
							throw new System::Exception(S"Trouble with writing out Peaks in CDTA") ; 
						}
					}
					mfile_comb_dta<<std::endl; //AM: July2009. To get the extra line spacing in. 

					continue ; 				
				}
				
				std::ofstream fout(fileName, std::ios::out) ;
				fout.precision(12);							
				fout<<massplusH<<" "<<mobj_transformRecord.mshort_cs<<std::endl;
				
							
				for (int i = 0; i <(int) mvect_mzs_msN.size(); i++)
				{
					double mz = mvect_mzs_msN[i];
					double intensity = mvect_intensities_msN[i];					
					fout<<std::setiosflags( std::ios::fixed | std::ios::showpoint )<<std::setprecision(5)<<mz<<" ";
					fout<<std::setiosflags( std::ios::fixed | std::ios::showpoint )<<std::setprecision(2)<<intensity<<std::endl;					
				}		
				fout.close();					
			}	
		}

		void DTAProcessor::WriteLowResolutionMGFFile() 
		{
			std::map <int,int> ::const_iterator mapIterator;
			int msN_scan;
			int parent_scan;
			int msN_scan_index;
			int class_val;
			double parent_Mz_match = 0;
			double parent_mz;
			
			int numCharges = 0;			
			int size = mmap_msN_parentIndex.size();	
			
			for (mapIterator = mmap_msN_parentIndex.begin(); mapIterator != mmap_msN_parentIndex.end(); mapIterator++)
			{				
				msN_scan = mapIterator->first;
				msN_scan_index = mapIterator->second;			
				parent_scan = mobj_raw_data_dta->GetParentScan(msN_scan);

				GetMsNSpectra(msN_scan, mdbl_pkBkgRatio, mdbl_peptideMinBkgRatio);
				parent_mz = mobj_raw_data_dta->GetParentMz(msN_scan);	
				GetParentScanSpectra(parent_scan, mdbl_pkBkgRatio, mdbl_peptideMinBkgRatio);	
				Engine::PeakProcessing::Peak parentPeak;
				parent_Mz_match = mobj_parent_peak_processor->GetClosestPeakMz(parent_mz, parentPeak);

				class_val = mobj_svm_charge_determination->GetClassAtScanIndex(msN_scan_index);
				
				mvect_chargeStateList.clear();	
				mvect_transformRecords.clear() ; 
				switch (class_val)
				{
					case 0: mvect_chargeStateList.push_back(2);
							mvect_chargeStateList.push_back(3);
							break;
					case 1: mvect_chargeStateList.push_back(1);
							break;
					case 2: mvect_chargeStateList.push_back(2);							
							break;
					case 3: mvect_chargeStateList.push_back(3);							
							break;
					case 4: mvect_chargeStateList.push_back(4) ;							
							break ; 
					default: break;
				}
				
				numCharges = (int)mvect_chargeStateList.size();

				//deconvolute with all charges states in chargeList				
				for (int chargeNum = 0;  chargeNum < numCharges; chargeNum++)
				{
					int cs = mvect_chargeStateList[chargeNum] ; 
					mobj_transformRecord.mdbl_mz = parent_Mz_match ; 
					mobj_transformRecord.mshort_cs = cs; 
					mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
					mobj_transformRecord.mdbl_fit = 1 ; 		
					mobj_transformRecord.mint_mono_intensity = mdbl_parent_Intensity ; 
					mvect_transformRecords.push_back(mobj_transformRecord) ; 
				}
			
				WriteToMGF(msN_scan, parent_scan) ; 
			}

			mobj_svm_charge_determination->ClearMemory() ; 

		}
		
		void DTAProcessor::WriteLowResolutionDTAFile()
		{
			std::map <int,int> ::const_iterator mapIterator;
			int msN_scan;
			int parent_scan;
			int msN_scan_index;
			int class_val;
			double parent_mz;
			double parent_Mz_match = 0;

			
			int numCharges = 0;			
			int size = mmap_msN_parentIndex.size();	
	
			for (mapIterator = mmap_msN_parentIndex.begin(); mapIterator != mmap_msN_parentIndex.end(); mapIterator++)
			{				
				msN_scan = mapIterator->first;
				msN_scan_index = mapIterator->second;		
				
				GetMsNSpectra(msN_scan, mdbl_pkBkgRatio, mdbl_peptideMinBkgRatio);

				parent_scan = mobj_raw_data_dta->GetParentScan(msN_scan);
				GetParentScanSpectra(parent_scan, mdbl_pkBkgRatio, mdbl_peptideMinBkgRatio);	

				parent_mz = mobj_raw_data_dta->GetParentMz(msN_scan);					
				parent_Mz_match = mobj_parent_peak_processor->GetClosestPeakMz(parent_mz, mobj_parentPeak);
				if (mobj_parentPeak.mdbl_intensity< mdbl_min_peptide_intensity)
				{				
					mdbl_parent_Mz = parent_mz;					
					Engine::PeakProcessing::Peak tempPeak ; 			
					mobj_parent_peak_processor->mobj_peak_data->FindPeak(parent_mz-0.1, parent_mz+0.1, tempPeak) ; 
					if (tempPeak.mdbl_intensity > 0 )
						mdbl_parent_Intensity = tempPeak.mdbl_intensity ; 
					else
						mdbl_parent_Intensity = mdbl_min_peptide_intensity ; 						
				}
				else
				{
					mdbl_parent_Mz = parent_Mz_match ; 
					mdbl_parent_Intensity = mobj_parentPeak.mdbl_intensity ; 
				}
				
				class_val = mobj_svm_charge_determination->GetClassAtScanIndex(msN_scan_index);
				double score  = mobj_svm_charge_determination->GetScoreAtScanIndex(msN_scan_index) ; 
				
				mvect_chargeStateList.clear();	
				mvect_transformRecords.clear() ; 
				switch (class_val)
				{
					case 0: mvect_chargeStateList.push_back(2);
							mvect_chargeStateList.push_back(3);
							break;
					case 1: mvect_chargeStateList.push_back(1);
							break;
					case 2: mvect_chargeStateList.push_back(2);		
							break;
					case 3: mvect_chargeStateList.push_back(3);											
							break;
					case 4: mvect_chargeStateList.push_back(4) ;							
							break ; 
					default: break;
				}
				
				numCharges = (int)mvect_chargeStateList.size();

				//deconvolute with all charges states in chargeList				
				for (int chargeNum = 0;  chargeNum < numCharges; chargeNum++)
				{
					int cs = mvect_chargeStateList[chargeNum] ; 
					mobj_transformRecord.mdbl_mz = mdbl_parent_Mz ; 

					if (mdbl_parent_Mz == 0)
					{
						bool debug = true ; 
						debug = false ; 
					}

					mobj_transformRecord.mshort_cs = cs; 
					mobj_transformRecord.mdbl_mono_mw  = (mobj_transformRecord.mdbl_mz - mdbl_cc_mass) * mobj_transformRecord.mshort_cs ; 
					mobj_transformRecord.mdbl_fit = 1 ; 
					mobj_transformRecord.mint_mono_intensity = mdbl_parent_Intensity ; 
					mvect_transformRecords.push_back(mobj_transformRecord) ; 
				}			
				WriteDTAFile(msN_scan, parent_scan) ; 
			}

			//clear up			
			mobj_svm_charge_determination->ClearMemory() ; 
		}
	}
} 