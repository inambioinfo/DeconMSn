// Written by Navdeep Jaitly and Anoop Mayampurath for the Department of Energy (PNNL, Richland, WA)
// Copyright 2006, Battelle Memorial Institute
// E-mail: navdeep.jaitly@pnl.gov
// Website: http://ncrr.pnl.gov/software
// -------------------------------------------------------------------------------
// 
// Licensed under the Apache License, Version 2.0; you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at 
// http://www.apache.org/licenses/LICENSE-2.0


//////////////////////////////////////////////////////////////////////////////
//
// FinniganRawData class
//		This class is a wrapper for the XRawFile.ocx COM control that is used
//		to open LCQ and LTQ-FT data.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "rawdata.h"
//set preprocessor declaration XCALIBUR_INSTALLED if Xcalibur is installed on computer
// also copy the following files into the bin folder in DeconEngine folder :
//CFRDBResources.dll
//CFRUtil.dll
//FControl2.dll
//Fglobal.dll
//Fileio.dll
//finDB.dll
//finSSClientLib.dll
//Fregistry.dll
//xrawfile.ocx
//XRawfile2.dll

#ifdef XCALIBUR_INSTALLED
#import "..\bin\xrawfile2.dll"

#include <iostream>
#include <hash_map>

using namespace stdext ;


namespace Engine {
 namespace Readers
{
	class  FinniganRawData : public RawData
	{
		private : 
			typedef struct _datapeak
			{
				double dMass;
				double dIntensity;
			} DataPeak;

			typedef struct _labelPeak : DataPeak
			{
				// Inherited from DataPeak
				//double dMass;
				//double dIntensity;

				// Apparently the additional data is of the following formats
				//float fRes;
				//float fBase;
				//float fNoise;
				//int charge;

				// The array is always in 8 byte chunks, so use 8 byte data formats
				double fRes;
				double fBase;
				double fNoise;
				long long charge;
			} LabelPeak;

			char *marr_rawfileName;

			long mlong_num_spectra ; 
			long mlong_spectra_num_first ;
			long mlong_spectra_num_last ;
			int mint_last_scan_size ; 
			int mint_last_scan_num ; 
			double mdbl_last_scan_time ; 
			double mdbl_signal_range ; 

			int mint_num_points_in_scan ; 
			int *marr_data_block ; 
			float *marr_temp_data_block ; 
			
			
			XRAWFILE2Lib::IXRawfile3 *m_xraw2_class ;
			

			void SetDataSize(int sz) ; 
			int	ReadSpectraFloats(int spectra_num) ; 
			int GetXRawFileInstance() ;

		public:
			~FinniganRawData(void) ;
			FinniganRawData(void) ;

			const char *GetFileName() ;
			FileType GetFileType() { return FINNIGAN ; } ;  

			int GetNumScans();
			int GetScanSize(); 

			int FirstSpectraNumber() ;
			int LastSpectraNumber() ;

			int Open(char *raw_file_name) ;
			void Close() ;
			virtual void Load(char *file_n) ;
			double GetScanTime(int scan_num) ; 
			short GetSpectrumType(int scan_num);
			bool GetRawData(std::vector<double> *mzs, std::vector<double> *intensities, int scan_num, bool centroid) ;  
			bool GetRawData(std::vector<double> *mzs, std::vector<double> *intensities, int scan_num, bool centroid, int num_points) ;  
			double GetSignalRange(int scan_num, bool centroid) ; 
			bool IsProfileScan(int scan_num) ;
			bool IsZoomScan(int scan_num);
			void GetTicFromFile(std::vector<double> *intensities, std::vector<double> *scan_times, bool base_peak_tic) ; 
			int GetParentScan(int scan_num);
			bool IsMSScan(int scan_num);					
			double GetParentMz(int scan_num);
			int GetMSLevel(int scan_num) ;
			virtual void GetScanDescription(int scan, char *description) ;
			double GetMonoMZFromHeader(int scan_num) ;
			short GetMonoChargeFromHeader(int scan_num) ; 
			int GetLastScanNum() { return GetNumScans() ; } 
			bool IsFTScan(int scanNum) ; 
			double GetAGCAccumulationTime(int scan_num) ; 
			double GetTICForScan(int scan_num) ; 			
	};
}
}
#endif 
