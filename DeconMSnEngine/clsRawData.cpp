// Written by Navdeep Jaitly and Anoop Mayampurath for the Department of Energy (PNNL, Richland, WA)
// Copyright 2006, Battelle Memorial Institute
// E-mail: navdeep.jaitly@pnl.gov
// Website: http://ncrr.pnl.gov/software
// -------------------------------------------------------------------------------
// 
// Licensed under the Apache License, Version 2.0; you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at 
// http://www.apache.org/licenses/LICENSE-2.0

#using <mscorlib.dll>
#include ".\clsrawdata.h"
#include "Readers\ReaderFactory.h"
#include "Readers\Icr2LSRawData.h" 
#include "DeconEngineUtils.h"

namespace Decon2LS
{
 namespace Readers
	{
		clsRawData::clsRawData(void)
		{
			mobj_raw_data = NULL ; 
			mobj_preprocess_options = new clsRawDataPreprocessOptions() ; 
		}

		clsRawData::~clsRawData(void)
		{
			if (mobj_raw_data != NULL)
				delete mobj_raw_data ; 
		}

		clsRawData::clsRawData(System::String *file_name, Decon2LS::Readers::FileType file_type)
		{
			LoadFile(file_name, file_type) ; 
		}

		int clsRawData::GetFirstScanNum()
		{
			if (mobj_raw_data == NULL)
				return 0 ; 
			return mobj_raw_data->GetFirstScanNum() ; 
		}


		void clsRawData::LoadFile(char *file_name, Decon2LS::Readers::FileType file_type)
		{
			if (mobj_raw_data != NULL)
				delete mobj_raw_data ; 

			// enumerations of file type are the same in Readers namespace and 
			// DeconWrapperManaged namespace.
			try
			{
				mobj_raw_data = Engine::Readers::ReaderFactory::GetMSDataReader((Engine::Readers::FileType)file_type) ; 
				if (file_type == ICR2LSRAWDATA && mobj_preprocess_options != NULL && 
					mobj_preprocess_options->get_ApodizationType() != NOAPODIZATION)
				{
					Engine::Readers::Icr2lsRawData *icr_raw_data = (Engine::Readers::Icr2lsRawData *)mobj_raw_data ; 
					icr_raw_data->SetApodizationZeroFillOptions((Engine::Utilities::ApodizationType)mobj_preprocess_options->get_ApodizationType(),
						mobj_preprocess_options->get_ApodizationMinX(), mobj_preprocess_options->get_ApodizationMaxX(), 
						mobj_preprocess_options->get_ApodizationPercent(), mobj_preprocess_options->get_NumZeroFills()) ; 
				}
				mobj_raw_data->Load(file_name) ; 
				if (file_type == ICR2LSRAWDATA && mobj_preprocess_options != NULL && 
					mobj_preprocess_options->get_ApplyCalibration())
				{
					Engine::Readers::Icr2lsRawData *icr_raw_data = (Engine::Readers::Icr2lsRawData *)mobj_raw_data ; 
					icr_raw_data->OverrideDefaultCalibrator((Engine::Readers::CalibrationType)mobj_preprocess_options->get_CalibrationType(), 
						mobj_preprocess_options->get_A(), mobj_preprocess_options->get_B(), mobj_preprocess_options->get_C()) ; 
				}
			}
			catch (char *mesg)
			{
				System::String *exception_msg = new System::String(mesg) ; 
				throw new System::Exception(exception_msg) ; 
			}

			return ; 
		}

		void clsRawData::LoadFile(char file_name __gc [], Decon2LS::Readers::FileType file_type) 
		{
			char *file_name_ch = new char[file_name->Length+1] ; 
			for (int i = 0 ; i < file_name->Length ; i++)
				file_name_ch[i] = file_name[i] ; 
			file_name_ch[file_name->Length] = '\0' ; 
			LoadFile(file_name_ch, file_type) ; 
		}

		void clsRawData::LoadFile(System::String *file_name, Decon2LS::Readers::FileType file_type)
		{
			char file_name_ch[256] ; 
			DeconEngine::Utils::GetStr(file_name, file_name_ch) ; 
			LoadFile(file_name_ch, file_type) ; 
		}

		void clsRawData::Close() 
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"Cannot close file because no file has been opened.") ; 
			}
			mobj_raw_data->Close() ; 
		}

		int clsRawData::GetNumScans()
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"Cannot get number of scans because no file has been opened") ; 
			}
			return mobj_raw_data->GetNumScans() ; 
		}

		int clsRawData::GetMSLevel(int scan_num)
		{
			if (mobj_raw_data == NULL)
			{
				throw new System::ApplicationException(S"Cannot get MS level because no file has been opened") ; 
			}
			return mobj_raw_data->GetMSLevel(scan_num) ; 
		}

		double clsRawData::GetScanTime(int scan_num)
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"Cannot get scan time because no file has been opened") ; 
			}
			return mobj_raw_data->GetScanTime(scan_num) ; 
		}
		int clsRawData::GetScanSize()
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"No file has been opened") ; 
			}
			return mobj_raw_data->GetScanSize() ; 
		}
		
		int clsRawData::GetSpectrumType(int scan_num) 
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"No file has been opened") ; 
			}
			return mobj_raw_data->GetSpectrumType(scan_num) ; 
		}

		void clsRawData::GetTicFromFile(float  (&intensities) __gc [], float (&scan_times) __gc [], bool base_peak_tic)
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"No file has been opened") ; 
			}

			std::vector<double> vect_intensities ; 
			std::vector<double> vect_scan_times ; 

			mobj_raw_data->GetTicFromFile(&vect_intensities, &vect_scan_times, base_peak_tic) ; 

			int num_pts = (int) vect_intensities.size() ; 
			intensities = new float __gc [num_pts] ;
			scan_times = new float __gc [num_pts] ;

			for (int i = 0 ; i < num_pts ; i++)
			{
				scan_times[i] = (float) vect_scan_times[i] ; 
				intensities[i] = (float) vect_intensities[i] ; 
			}
			return ; 
		}

		void clsRawData::GetSummedSpectra(int start_scan, int stop_scan, double min_mz, double max_mz, float (&mzs) __gc[], float (&intensities) __gc[])
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"No file has been opened") ; 
			}

			std::vector<double> vect_mzs ;
			std::vector<double> vect_intensities; 
			try
			{
				mobj_raw_data->GetSummedSpectra(&vect_mzs, &vect_intensities, start_scan, stop_scan, min_mz, max_mz) ; 
				int num_pts = (int) vect_intensities.size() ; 
				intensities = new float __gc [num_pts] ;
				mzs = new float __gc [num_pts] ;

				for (int i = 0 ; i < num_pts ; i++)
				{
					mzs[i] = (float) vect_mzs[i] ; 
					intensities[i] = (float) vect_intensities[i] ; 
				}
				vect_mzs.clear() ; 
				vect_intensities.clear() ; 
			}
			catch (char *mesg)
			{
				System::String *exception_msg = new System::String(mesg) ; 
				throw new System::Exception(exception_msg) ; 
			}			
		}

		void clsRawData::GetSummedSpectra(int current_scan , int scan_range, float (&mzs) __gc[], float (&intensities) __gc[])
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"No file has been opened") ; 
			}
			
			std::vector<double> vect_mzs ;
			std::vector<double> vect_intensities; 
			try
			{
				mobj_raw_data->GetSummedSpectra(&vect_mzs, &vect_intensities, current_scan, scan_range) ; 			

				int num_pts = (int) vect_intensities.size() ; 
				intensities = new float __gc [num_pts] ;
				mzs = new float __gc [num_pts] ;

				for (int i = 0 ; i < num_pts ; i++)
				{
					mzs[i] = (float) vect_mzs[i] ; 
					intensities[i] = (float) vect_intensities[i] ; 
				}
				vect_mzs.clear() ; 
				vect_intensities.clear() ; 
			}
			catch (char *mesg)
			{
				System::String *exception_msg = new System::String(mesg) ; 
				throw new System::Exception(exception_msg) ; 
			}			
		}

		void clsRawData::GetSpectrum(int scan_num, float  (&mzs) __gc [], float (&intensities) __gc [], bool centroid)
		{
			if (mobj_raw_data == NULL) 
			{
				throw new System::ApplicationException(S"No file has been opened") ; 
			}
			std::vector<double> vect_mzs ;
			std::vector<double> vect_intensities; 
			try
			{
				mobj_raw_data->GetRawData(&vect_mzs, &vect_intensities, scan_num, centroid) ; 
				int num_pts = (int) vect_intensities.size() ; 
				intensities = new float __gc [num_pts] ;
				mzs = new float __gc [num_pts] ;

				for (int i = 0 ; i < num_pts ; i++)
				{
					mzs[i] = (float) vect_mzs[i] ; 
					intensities[i] = (float) vect_intensities[i] ; 
				}
			}
			catch (char *mesg)
			{
				System::String *exception_msg = new System::String(mesg) ; 
				throw new System::Exception(exception_msg) ; 
			}
		}
		int clsRawData::GetParentScan(int scan_num)
		{
			//Given a scan number of an MS2 scan this identifies the parent scan
			if (mobj_raw_data == NULL)
				return 0;
			return mobj_raw_data->GetParentScan(scan_num);
		}
		
		void clsRawData::GetMzsInRange(float (&in_mzs) __gc [], float (&in_intensities) __gc [], float (&out_mzs) __gc [], float (&out_intensities) __gc [], float central_value, float range)
		{
			int index = 0;
			//get count first 
			for (int i = 0; i < in_mzs->Length; i++)
			{
				if (in_mzs[i] >=  (central_value - range) && in_mzs[i]  <= (central_value + range))
				{
					 index++;
				}
			}			
			out_intensities = new float __gc[index];
			out_mzs  = new float __gc[index];
			index = 0;
			for (int i = 0; i < in_mzs->Length; i++)
			{
				if (in_mzs[i] >=  (central_value - range) && in_mzs[i]  <= (central_value + range))
				{
					out_mzs[index] = in_mzs[i];
					out_intensities[index] = in_intensities[i];
					index++;
				}				
			}
		}

		System::String* clsRawData::GetScanDescription(int scan_num)
		{
			if (mobj_raw_data == NULL)
				return S"" ;
			char description[512] ; 
			mobj_raw_data->GetScanDescription(scan_num, description) ; 
			System::String *descriptionStr = new System::String("") ; 
			DeconEngine::Utils::GetStr(description, &descriptionStr) ; 
			return descriptionStr ; 
		}

		double clsRawData::GetFTICRSamplingRate()
		{
			if (get_FileType() != FileType::ICR2LSRAWDATA)
				throw new System::Exception("FileType is not FTICR or Transient information is not available") ; 
			if (mobj_raw_data == NULL) 
				throw new System::Exception("RawData not instantiated") ; 

			Engine::Readers::Icr2lsRawData *icrRawData = (Engine::Readers::Icr2lsRawData *) mobj_raw_data ; 
			return icrRawData->GetSampleRate() ; 
		}
		void clsRawData::GetFTICRTransient(float (&intensities) __gc [])
		{
			if (get_FileType() != FileType::ICR2LSRAWDATA)
				throw new System::Exception("FileType is not FTICR or Transient information is not available") ; 
			if (mobj_raw_data == NULL) 
				throw new System::Exception("RawData not instantiated") ; 

			try
			{
				Engine::Readers::Icr2lsRawData *icrRawData = (Engine::Readers::Icr2lsRawData *) mobj_raw_data ; 
				std::vector<float> vect_intensities; 
				icrRawData->GetFTICRTransient(vect_intensities) ; 

				int num_pts = (int) vect_intensities.size() ; 
				intensities = new float __gc [num_pts] ;

				for (int i = 0 ; i < num_pts ; i++)
				{
					intensities[i] = (float) vect_intensities[i] ; 
				}
			}
			catch (char *mesg)
			{
				System::String *exception_msg = new System::String(mesg) ; 
				throw new System::Exception(exception_msg) ; 
			}
		}

	}
}