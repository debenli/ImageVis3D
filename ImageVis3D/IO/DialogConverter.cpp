/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.

   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

/**
  \file    DialogConverter.cpp
  \author    Jens Krueger
        SCI Institute
        University of Utah
  \date    December 2008
*/

#include "DialogConverter.h"
#include "../UI/RAWDialog.h"
#include "../Tuvok/Controller/MasterController.h"
#include "../Tuvok/Basics/SysTools.h"
#include "../Tuvok/Basics/LargeRAWFile.h"

#include <QtGui/QMessageBox>

using namespace std;

DialogConverter::DialogConverter(QWidget* parent) :
  m_parent(parent)
{
}

bool DialogConverter::ConvertToRAW(const std::string& strSourceFilename, 
                                   const std::string& strTempDir, bool bNoUserInteraction,
                                   uint64_t& iHeaderSkip, uint64_t& iComponentSize, uint64_t& iComponentCount, 
                                   bool& bConvertEndianess, bool& bSigned, bool& bIsFloat, UINT64VECTOR3& vVolumeSize,
                                   FLOATVECTOR3& vVolumeAspect, std::string& strTitle,
                                   UVFTables::ElementSemanticTable& eType, std::string& strIntermediateFile,
                                   bool& bDeleteIntermediateFile) {

  if (bNoUserInteraction) return false;

  if (QMessageBox::No == QMessageBox::question(nullptr, "RAW Data Loader", "The file was not recognized by ImageVis3D's built-in readers and cannot be converted automatically. Do you want to specify the data set parameters manually?", QMessageBox::Yes, QMessageBox::No)) {
    return false;
  }
  
  LargeRAWFile f(strSourceFilename);
  f.Open(false);
  uint64_t iSize = f.GetCurrentSize();
  f.Close();

#ifdef DETECTED_OS_APPLE
  // Why we need to specify null for the parent on Mac OS X:
  // the PleaseWaitDialog instances in ImageVis3D_FileHandling.cpp in the
  // LoadDataset function.
  //
  // Mac OS X is presumably waiting for the first modal dialog
  // (PleaseWaitDialog) to finish before opening up the next modal dialog
  // (RAWDialog). Since the user cannot interact with PleaseWaitDialog, we
  // are stuck in a modal state with no where to go.
  //
  // Specifying nullptr circumvents this issue by making the dialog a
  // "top-level widget" with no parent.
  RAWDialog rawDialog(strSourceFilename, iSize, nullptr);
#else
  RAWDialog rawDialog(strSourceFilename, iSize, m_parent);
#endif
  if (rawDialog.exec() == QDialog::Accepted) {

    if (rawDialog.ComputeExpectedSize() > iSize) return false;

    strTitle = "Raw data";
    eType             = UVFTables::ES_UNDEFINED;
    iComponentCount = 1; 
    vVolumeSize    = rawDialog.GetSize();
    vVolumeAspect  = rawDialog.GetAspectRatio();
    unsigned int  quantID        = rawDialog.GetQuantization();
    unsigned int  encID          = rawDialog.GetEncoding();
    iHeaderSkip       = rawDialog.GetHeaderSize();
    bConvertEndianess = encID != 1 && rawDialog.IsBigEndian() != EndianConvert::IsBigEndian();
    bSigned           = quantID >= 3 || rawDialog.IsSigned();
    

    iComponentSize = 8;
    if (quantID == 1) iComponentSize = 16;
    if (quantID == 2 || quantID == 3) iComponentSize = 32;
    if (quantID == 4) iComponentSize = 64;

    bIsFloat = quantID == 3 || quantID == 4;

    MESSAGE("setting component size to: %llu", iComponentSize);

    if (encID == 0)  {
      strIntermediateFile = strSourceFilename;
      bDeleteIntermediateFile = false;
      return true;
    } else
    if (encID == 1)  {
        string strBinaryFile = strTempDir+SysTools::GetFilename(strSourceFilename)+".binary";
        bool bResult = ParseTXTDataset(strSourceFilename, strBinaryFile, iHeaderSkip, iComponentSize, iComponentCount, bSigned, bIsFloat, vVolumeSize);
        strIntermediateFile = strBinaryFile;
        bDeleteIntermediateFile = true;
        iHeaderSkip = 0;
        bConvertEndianess = false;
        return bResult;
    } else
    if (encID == 2)  {
        string strUncompressedFile = strTempDir+SysTools::GetFilename(strSourceFilename)+".uncompressed";
        bool bResult = ExtractGZIPDataset(strSourceFilename, strUncompressedFile, iHeaderSkip);
        strIntermediateFile = strUncompressedFile;
        bDeleteIntermediateFile = true;
        iHeaderSkip = 0;
        return bResult;
    } else {
        string strUncompressedFile = strTempDir+SysTools::GetFilename(strSourceFilename)+".uncompressed";
        bool bResult = ExtractBZIP2Dataset(strSourceFilename, strUncompressedFile, iHeaderSkip);
        strIntermediateFile = strUncompressedFile;
        bDeleteIntermediateFile = true;
        iHeaderSkip = 0;
        return bResult;
    } 
  }

  return false;
}
