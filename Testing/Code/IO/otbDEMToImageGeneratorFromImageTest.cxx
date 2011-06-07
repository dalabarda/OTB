/*=========================================================================

  Program:   ORFEO Toolbox
  Language:  C++
  Date:      $Date$
  Version:   $Revision$


  Copyright (c) Centre National d'Etudes Spatiales. All rights reserved.
  See OTBCopyright.txt for details.


  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "itkExceptionObject.h"


#include "otbStreamingImageFileWriter.h"
#include "otbVectorImage.h"
#include "otbImage.h"
#include "otbImageFileReader.h"

#include "otbDEMToImageGenerator.h"
#include "otbMultiToMonoChannelExtractROI.h"
#include "otbExtractROI.h"

#include "otbStandardFilterWatcher.h"


int otbDEMToImageGeneratorFromImageTest(int argc, char * argv[])
{
  if (argc < 9)
    {
    std::cout << argv[0] <<
    " input filename, folder path, output filename 1, output filename 2, X Output Origin, Y Output Origin, X Output Size, Y Output Size"
              << std::endl;
    return EXIT_FAILURE;
    }

  char * inputName = argv[1];
  char * folderPath = argv[2];
  char * outputName1 = argv[3];
  char * outputName2= argv[4];

  std::cout << argv[5] << ","
            << argv[6] << ","
            << argv[7] << ","
            << argv[8] << "," << std::endl;

  const unsigned int Dimension = 2;
  typedef double                                   PixelType;
  typedef otb::VectorImage<PixelType, Dimension>   VImageType;
  typedef otb::Image<PixelType, Dimension>         ImageType;
  typedef otb::DEMToImageGenerator<ImageType>      DEMToImageGeneratorType;

  typedef otb::ImageFileReader<VImageType> ReaderType;
  typedef otb::StreamingImageFileWriter<ImageType> WriterType;

  typedef otb::MultiToMonoChannelExtractROI<PixelType,PixelType> ExtractVFilterType;
  typedef otb::ExtractROI<PixelType,PixelType> ExtractFilterType;

  // Instantiating object
  ReaderType::Pointer              reader = ReaderType::New();
  DEMToImageGeneratorType::Pointer generatorFilter1 = DEMToImageGeneratorType::New();
  DEMToImageGeneratorType::Pointer generatorFilter2 = DEMToImageGeneratorType::New();
  ExtractVFilterType::Pointer       extract1 = ExtractVFilterType::New();
  ExtractFilterType::Pointer      extract2 = ExtractFilterType::New();

  WriterType::Pointer              writer1 = WriterType::New();
  WriterType::Pointer              writer2 = WriterType::New();


  // Read input image
  reader->SetFileName(inputName);
  reader->GenerateOutputInformation();

  // First pipeline:
  extract1->SetInput(reader->GetOutput());
  extract1->SetSizeX(atoi(argv[7]));
  extract1->SetSizeY(atoi(argv[8]));
  extract1->SetStartX(atoi(argv[5]));
  extract1->SetStartY(atoi(argv[6]));
  extract1->SetChannel(1);

  extract1->UpdateOutputInformation();

  generatorFilter1->SetDEMDirectoryPath(folderPath);
  generatorFilter1->SetOutputParametersFromImage(extract1->GetOutput());

  otb::StandardFilterWatcher watcher1(generatorFilter1, "DEM to image generator");

  writer1->SetFileName(outputName1);
  writer1->SetInput(generatorFilter1->GetOutput());

  writer1->Update();

  // Second pipeline:
  generatorFilter2->SetDEMDirectoryPath(folderPath);
  generatorFilter2->SetOutputParametersFromImage(reader->GetOutput());
  generatorFilter2->InstanciateTransform();

  otb::StandardFilterWatcher watcher2(generatorFilter2, "DEM to image generator");

  extract2->SetInput(generatorFilter2->GetOutput());
  extract2->SetSizeX(atoi(argv[7]));
  extract2->SetSizeY(atoi(argv[8]));
  extract2->SetStartX(atoi(argv[5]));
  extract2->SetStartY(atoi(argv[6]));

  writer2->SetFileName(outputName2);
  writer2->SetInput(extract2->GetOutput());

  writer2->Update();

  return EXIT_SUCCESS;
}
