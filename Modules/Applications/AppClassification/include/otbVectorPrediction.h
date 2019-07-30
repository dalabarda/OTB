/*
 * Copyright (C) 2005-2019 Centre National d'Etudes Spatiales (CNES)
 *
 * This file is part of Orfeo Toolbox
 *
 *     https://www.orfeo-toolbox.org/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef otbVectorPrediction_h
#define otbVectorPrediction_h

#include "otbWrapperApplication.h"
#include "otbWrapperApplicationFactory.h"

#include "otbOGRDataSourceWrapper.h"
#include "otbOGRFeatureWrapper.h"

#include "itkVariableLengthVector.h"
#include "otbStatisticsXMLFileReader.h"

#include "itkListSample.h"
#include "otbShiftScaleSampleListFilter.h"

#include "otbMachineLearningModelFactory.h"

#include "otbMachineLearningModel.h"

#include <time.h>

namespace otb
{
namespace Wrapper
{

template <bool RegressionMode, class ValueType, class LabelType>
class VectorPrediction : public Application
{
public:
  /** Standard class typedefs. */
  typedef VectorPrediction              Self;
  typedef Application                   Superclass;
  typedef itk::SmartPointer<Self>       Pointer;
  typedef itk::SmartPointer<const Self> ConstPointer;

  /** Standard macro */
  itkNewMacro(Self);

  itkTypeMacro(Self, Application)

  /** Filters typedef */
  //typedef float                                         ValueType;
  //typedef unsigned int                                  LabelType;
  typedef itk::FixedArray<LabelType,1>                  LabelSampleType;
  typedef itk::Statistics::ListSample<LabelSampleType>  LabelListSampleType;

  typedef otb::MachineLearningModel<ValueType,LabelType>          MachineLearningModelType;
  typedef otb::MachineLearningModelFactory<ValueType, LabelType>  MachineLearningModelFactoryType;
  typedef typename MachineLearningModelType::Pointer                       ModelPointerType;
  typedef typename MachineLearningModelType::ConfidenceListSampleType      ConfidenceListSampleType;

  /** Statistics Filters typedef */
  typedef itk::VariableLengthVector<ValueType>                    MeasurementType;
  typedef otb::StatisticsXMLFileReader<MeasurementType>           StatisticsReader;

  typedef itk::VariableLengthVector<ValueType>                    InputSampleType;
  typedef itk::Statistics::ListSample<InputSampleType>            ListSampleType;
  typedef otb::Statistics::ShiftScaleSampleListFilter<ListSampleType, ListSampleType> ShiftScaleFilterType;

  ~VectorPrediction() override
    {
    MachineLearningModelFactoryType::CleanFactories();
    }

private:
  /** Utility function to negate std::isalnum */
  static bool IsNotAlphaNum(char c)
  {
  return !std::isalnum(c);
  }
  
  void DoInit() override
  {
    DoInitSpecialization();
    //TODO add assert to check that parameters has been correctly defined
  }

  void DoInitSpecialization();

  void DoUpdateParameters() override
  {
    if ( HasValue("in") )
    {
      std::string shapefile = GetParameterString("in");

      otb::ogr::DataSource::Pointer ogrDS;

      OGRSpatialReference oSRS("");
      std::vector<std::string> options;

      ogrDS = otb::ogr::DataSource::New(shapefile, otb::ogr::DataSource::Modes::Read);
      otb::ogr::Layer layer = ogrDS->GetLayer(0);
      OGRFeatureDefn &layerDefn = layer.GetLayerDefn();

      ClearChoices("feat");

      for(int iField=0; iField< layerDefn.GetFieldCount(); iField++)
      {
        std::string item = layerDefn.GetFieldDefn(iField)->GetNameRef();
        std::string key(item);
        key.erase( std::remove_if(key.begin(),key.end(),IsNotAlphaNum), key.end());
        std::transform(key.begin(), key.end(), key.begin(), tolower);

        OGRFieldType fieldType = layerDefn.GetFieldDefn(iField)->GetType();
        if(fieldType == OFTInteger ||  fieldType == OFTInteger64 || fieldType == OFTReal)
          {
          std::string tmpKey="feat."+key;
          AddChoice(tmpKey,item);
          }
      }
    }
  }

  /** Create the prediction field in the output layer, this template method should be specialized
   * to create the right type of field (e.g. OGRInteger or OGRReal) */ 
  void CreatePredictionField(OGRFeatureDefn & layerDefn, otb::ogr::Layer & outLayer);

  void DoExecute() override
  {
    clock_t tic = clock();

    std::string shapefile = GetParameterString("in");

    otb::ogr::DataSource::Pointer source = otb::ogr::DataSource::New(shapefile, otb::ogr::DataSource::Modes::Read);
    otb::ogr::Layer layer = source->GetLayer(0);

    typename ListSampleType::Pointer input = ListSampleType::New();

    const int nbFeatures = GetSelectedItems("feat").size();
    input->SetMeasurementVectorSize(nbFeatures);
  
    otb::ogr::Layer::const_iterator it = layer.cbegin();
    otb::ogr::Layer::const_iterator itEnd = layer.cend();
    for( ; it!=itEnd ; ++it)
      {
      MeasurementType mv;
      mv.SetSize(nbFeatures);
      for(int idx=0; idx < nbFeatures; ++idx)
        {
        // Beware that itemIndex differs from ogr layer field index
        unsigned int itemIndex = GetSelectedItems("feat")[idx];
        std::string fieldName = GetChoiceNames( "feat" )[itemIndex];
        switch ((*it)[fieldName].GetType())
        {
        case OFTInteger:
          mv[idx] = static_cast<ValueType>((*it)[fieldName].GetValue<int>());
          break;
        case OFTInteger64:
          mv[idx] = static_cast<ValueType>((*it)[fieldName].GetValue<int>());
          break;
        case OFTReal:
          mv[idx] = static_cast<ValueType>((*it)[fieldName].GetValue<double>());
          break;
        default:
          itkExceptionMacro(<< "incorrect field type: " << (*it)[fieldName].GetType() << ".");
        }
        
        
        }
      input->PushBack(mv);
      }

    // Statistics for shift/scale
    MeasurementType meanMeasurementVector;
    MeasurementType stddevMeasurementVector;
    if (HasValue("instat") && IsParameterEnabled("instat"))
      {
      typename StatisticsReader::Pointer statisticsReader = StatisticsReader::New();
      std::string XMLfile = GetParameterString("instat");
      statisticsReader->SetFileName(XMLfile);
      meanMeasurementVector = statisticsReader->GetStatisticVectorByName("mean");
      stddevMeasurementVector = statisticsReader->GetStatisticVectorByName("stddev");
      }
    else
      {
      meanMeasurementVector.SetSize(nbFeatures);
      meanMeasurementVector.Fill(0.);
      stddevMeasurementVector.SetSize(nbFeatures);
      stddevMeasurementVector.Fill(1.);
      }

    typename ShiftScaleFilterType::Pointer trainingShiftScaleFilter = ShiftScaleFilterType::New();
    trainingShiftScaleFilter->SetInput(input);
    trainingShiftScaleFilter->SetShifts(meanMeasurementVector);
    trainingShiftScaleFilter->SetScales(stddevMeasurementVector);
    trainingShiftScaleFilter->Update();
    otbAppLogINFO("mean used: " << meanMeasurementVector);
    otbAppLogINFO("standard deviation used: " << stddevMeasurementVector);

    otbAppLogINFO("Loading model");
    m_Model = MachineLearningModelFactoryType::CreateMachineLearningModel(GetParameterString("model"),
                                                MachineLearningModelFactoryType::ReadMode);

    if (m_Model.IsNull())
      {
      otbAppLogFATAL(<< "Error when loading model " << GetParameterString("model") << " : unsupported model type");
      }

    m_Model->SetRegressionMode(RegressionMode);

    m_Model->Load(GetParameterString("model"));
    otbAppLogINFO("Model loaded");

    typename ListSampleType::Pointer listSample = trainingShiftScaleFilter->GetOutput();

    typename ConfidenceListSampleType::Pointer quality;

    bool computeConfidenceMap(!m_Model->GetRegressionMode() && GetParameterInt("confmap") && m_Model->HasConfidenceIndex() );

    if (!m_Model->GetRegressionMode() && !m_Model->HasConfidenceIndex() && GetParameterInt("confmap"))
      {
      otbAppLogWARNING("Confidence map requested but the classifier doesn't support it!");
      }

    typename LabelListSampleType::Pointer target;
    if (computeConfidenceMap)
      {
      quality = ConfidenceListSampleType::New();
      target = m_Model->PredictBatch(listSample, quality);
      }
      else
      {
      target = m_Model->PredictBatch(listSample);
      }

    ogr::DataSource::Pointer output;
    ogr::DataSource::Pointer buffer = ogr::DataSource::New();
    bool updateMode = false;
    if (IsParameterEnabled("out") && HasValue("out"))
      {
      // Create new OGRDataSource
      output = ogr::DataSource::New(GetParameterString("out"), ogr::DataSource::Modes::Overwrite);
      otb::ogr::Layer newLayer = output->CreateLayer(
        GetParameterString("out"),
        const_cast<OGRSpatialReference*>(layer.GetSpatialRef()),
        layer.GetGeomType());
      // Copy existing fields
      OGRFeatureDefn &inLayerDefn = layer.GetLayerDefn();
      for (int k=0 ; k<inLayerDefn.GetFieldCount() ; k++)
        {
        OGRFieldDefn fieldDefn(inLayerDefn.GetFieldDefn(k));
        newLayer.CreateField(fieldDefn);
        }
      }
    else
      {
      // Update mode
      updateMode = true;
      otbAppLogINFO("Update input vector data.");
      // fill temporary buffer for the transfer
      otb::ogr::Layer inputLayer = layer;
      layer = buffer->CopyLayer(inputLayer, std::string("Buffer"));
      // close input data source
      source->Clear();
      // Re-open input data source in update mode
      output = otb::ogr::DataSource::New(shapefile, otb::ogr::DataSource::Modes::Update_LayerUpdate);
      }

    otb::ogr::Layer outLayer = output->GetLayer(0);

    OGRErr errStart = outLayer.ogr().StartTransaction();
    if (errStart != OGRERR_NONE)
      {
      itkExceptionMacro(<< "Unable to start transaction for OGR layer " << outLayer.ogr().GetName() << ".");
      }

    OGRFeatureDefn &layerDefn = layer.GetLayerDefn();

    // Add the field of prediction in the output layer if field not exist
    CreatePredictionField(layerDefn, outLayer);

    // Add confidence field in the output layer
    std::string confFieldName("confidence");
    if (computeConfidenceMap)
      {
      int idx = layerDefn.GetFieldIndex(confFieldName.c_str());
      if (idx >= 0)
        {
        if (layerDefn.GetFieldDefn(idx)->GetType() != OFTReal)
          itkExceptionMacro("Field name "<< confFieldName << " already exists with a different type!");
        }
      else
        {
        OGRFieldDefn confidenceField(confFieldName.c_str(), OFTReal);
        confidenceField.SetWidth(confidenceField.GetWidth());
        confidenceField.SetPrecision(confidenceField.GetPrecision());
        ogr::FieldDefn confFieldDefn(confidenceField);
        outLayer.CreateField(confFieldDefn);
        }
      }

    // Fill output layer
    unsigned int count=0;
    std::string classfieldname = GetParameterString("cfield");
    it = layer.cbegin();
    itEnd = layer.cend();
    for( ; it!=itEnd ; ++it, ++count)
      {
      ogr::Feature dstFeature(outLayer.GetLayerDefn());
      dstFeature.SetFrom( *it , TRUE);
      dstFeature.SetFID(it->GetFID());
      switch (dstFeature[classfieldname].GetType())
        {
        case OFTInteger:
          dstFeature[classfieldname].SetValue<int>(target->GetMeasurementVector(count)[0]);
          break;
        case OFTInteger64:
          dstFeature[classfieldname].SetValue<int>(target->GetMeasurementVector(count)[0]);
          break;
        case OFTReal:
          dstFeature[classfieldname].SetValue<double>(target->GetMeasurementVector(count)[0]);
          break;
        case OFTString:
          dstFeature[classfieldname].SetValue<std::string>(std::to_string(target->GetMeasurementVector(count)[0]));
          break;
        default:
          itkExceptionMacro(<< "incorrect field type: " << dstFeature[classfieldname].GetType() << ".");
        }
      if (computeConfidenceMap)
        dstFeature[confFieldName].SetValue<double>(quality->GetMeasurementVector(count)[0]);
      if (updateMode)
        {
        outLayer.SetFeature(dstFeature);
        }
      else
        {
        outLayer.CreateFeature(dstFeature);
        }
      }

    if(outLayer.ogr().TestCapability("Transactions"))
      {
      const OGRErr errCommitX = outLayer.ogr().CommitTransaction();
      if (errCommitX != OGRERR_NONE)
        {
        itkExceptionMacro(<< "Unable to commit transaction for OGR layer " << outLayer.ogr().GetName() << ".");
        }
      }

    output->SyncToDisk();

    clock_t toc = clock();
    otbAppLogINFO( "Elapsed: "<< ((double)(toc - tic) / CLOCKS_PER_SEC)<<" seconds.");

  }

  ModelPointerType m_Model;
};

}
}

#endif
