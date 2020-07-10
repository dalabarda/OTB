/*
 * Copyright (C) 2005-2020 Centre National d'Etudes Spatiales (CNES)
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


#include "otbSarImageMetadataInterface.h"
#include "otbSentinel1ImageMetadataInterface.h"

#include "otbMacro.h"
#include "itkMetaDataObject.h"
#include "otbImageKeywordlist.h"
#include "otbXMLMetadataSupplier.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "ossim/ossimTimeUtilities.h"
#pragma GCC diagnostic pop
#else
#include "ossim/ossimTimeUtilities.h"
#endif
#include "itksys/SystemTools.hxx"

// useful constants
#include <otbMath.h>
#include <iomanip>

namespace otb
{

Sentinel1ImageMetadataInterface::Sentinel1ImageMetadataInterface()
{
}

bool Sentinel1ImageMetadataInterface::CanRead() const
{
  const std::string sensorID = GetSensorID();

  return sensorID.find("SENTINEL-1") != std::string::npos;
}

void Sentinel1ImageMetadataInterface::CreateCalibrationLookupData(const short type)
{
  bool sigmaLut = false;
  bool betaLut  = false;
  bool gammaLut = false;
  bool dnLut    = false;

  switch (type)
  {
  case SarCalibrationLookupData::BETA:
  {
    otbMsgDevMacro("betaNought");
    betaLut = true;
  }
  break;

  case SarCalibrationLookupData::GAMMA:
  {
    otbMsgDevMacro("gamma");
    gammaLut = true;
  }
  break;

  case SarCalibrationLookupData::DN:
  {
    otbMsgDevMacro("dn");
    dnLut = true;
  }
  break;

  case SarCalibrationLookupData::SIGMA:
  default:
    otbMsgDevMacro("sigmaNought");
    sigmaLut = true;
    break;
  }

  const ImageKeywordlistType imageKeywordlist = this->GetImageKeywordlist();

  // const double firstLineTime = Utils::LexicalCast<double>(imageKeywordlist.GetMetadataByKey("calibration.startTime"), "calibration.startTime(double)");

  // const double lastLineTime = Utils::LexicalCast<double>(imageKeywordlist.GetMetadataByKey("calibration.stopTime"), "calibration.stopTime(double)");
  using namespace ossimplugins::time;
  const ModifiedJulianDate firstLineTime = toModifiedJulianDate(imageKeywordlist.GetMetadataByKey("calibration.startTime"));
  const ModifiedJulianDate lastLineTime  = toModifiedJulianDate(imageKeywordlist.GetMetadataByKey("calibration.stopTime"));
  otbMsgDevMacro(<< "calibration.startTime: " << std::setprecision(16) << firstLineTime);
  otbMsgDevMacro(<< "calibration.stopTime : " << std::setprecision(16) << lastLineTime);

  // const std::string supportDataPrefix = "support_data."; //make && use GetBandPrefix(subSwath, polarisation)

  const int numOfLines = Utils::LexicalCast<int>(imageKeywordlist.GetMetadataByKey("number_lines"), "number_lines(int)");
  otbMsgDevMacro(<< "numOfLines   : " << numOfLines);

  const int count = Utils::LexicalCast<int>(imageKeywordlist.GetMetadataByKey("calibration.count"), "calibration.count");

  std::vector<Sentinel1CalibrationStruct> calibrationVectorList(count);
  double                                  lastMJD = 0;

  for (int i = 0; i < count; i++)
  {
    Sentinel1CalibrationStruct calibrationVector;

    std::stringstream prefix;
    prefix << "calibration.calibrationVector[" << i << "].";
    const std::string sPrefix = prefix.str();

    calibrationVector.line = Utils::LexicalCast<int>(imageKeywordlist.GetMetadataByKey(sPrefix + "line"), sPrefix + "line");

    // TODO: don't manipulate doubles, but ModifiedJulianDate for a better type
    // safety
    const std::string sAzimuth = imageKeywordlist.GetMetadataByKey(sPrefix + "azimuthTime");
    calibrationVector.timeMJD  = toModifiedJulianDate(sAzimuth).as_day_frac();
    calibrationVector.deltaMJD = calibrationVector.timeMJD - lastMJD;
    lastMJD                    = calibrationVector.timeMJD;

    otbMsgDevMacro(<< sPrefix << "line   : " << calibrationVector.line << " ;\t" << sPrefix << "timeMJD: " << std::setprecision(16) << calibrationVector.timeMJD
                   << " (" << sAzimuth << ")");

    Utils::ConvertStringToVector(imageKeywordlist.GetMetadataByKey(sPrefix + "pixel"), calibrationVector.pixels, sPrefix + "pixel");

    // prepare deltaPixels vector
    int prev_pixels = 0;
    calibrationVector.deltaPixels.resize(calibrationVector.pixels.size());
    for (std::size_t p = 0, N = calibrationVector.pixels.size(); p != N; ++p)
    {
      calibrationVector.deltaPixels[p] = (calibrationVector.pixels[p] - prev_pixels);
      prev_pixels                      = calibrationVector.pixels[p];
    }

    if (sigmaLut)
    {
      Utils::ConvertStringToVector(imageKeywordlist.GetMetadataByKey(sPrefix + "sigmaNought"), calibrationVector.vect, sPrefix + "sigmaNought");
    }

    if (betaLut)
    {
      Utils::ConvertStringToVector(imageKeywordlist.GetMetadataByKey(sPrefix + "betaNought"), calibrationVector.vect, sPrefix + "betaNought");
    }

    if (gammaLut)
    {
      Utils::ConvertStringToVector(imageKeywordlist.GetMetadataByKey(sPrefix + "gamma"), calibrationVector.vect, sPrefix + "gamma");
    }

    if (dnLut)
    {
      Utils::ConvertStringToVector(imageKeywordlist.GetMetadataByKey(sPrefix + "dn"), calibrationVector.vect, sPrefix + "dn");
    }

    calibrationVectorList[i] = calibrationVector;
  }

  Sentinel1CalibrationLookupData::Pointer sarLut = Sentinel1CalibrationLookupData::New();
  sarLut->InitParameters(type, firstLineTime.as_day_frac(), lastLineTime.as_day_frac(), numOfLines, count, calibrationVectorList);
  this->SetCalibrationLookupData(sarLut);
}

void Sentinel1ImageMetadataInterface::ParseDateTime(const char* key, std::vector<int>& dateFields) const
{
  if (dateFields.size() < 1)
  {
    // parse from keyword list
    if (!this->CanRead())
    {
      itkExceptionMacro(<< "Invalid Metadata, not a valid product");
    }

    const ImageKeywordlistType imageKeywordlist = this->GetImageKeywordlist();
    if (!imageKeywordlist.HasKey(key))
    {
      itkExceptionMacro(<< "no key named " << key);
    }

    const std::string date_time_str = imageKeywordlist.GetMetadataByKey(key);
    Utils::ConvertStringToVector(date_time_str, dateFields, key, "T:-.");
  }
}

int Sentinel1ImageMetadataInterface::GetYear() const
{
  int value = 0;
  ParseDateTime("support_data.image_date", m_AcquisitionDateFields);
  if (m_AcquisitionDateFields.size() > 0)
  {
    value = Utils::LexicalCast<int>(m_AcquisitionDateFields[0], "support_data.image_date:year(int)");
  }
  else
  {
    itkExceptionMacro(<< "Invalid year");
  }
  return value;
}

int Sentinel1ImageMetadataInterface::GetMonth() const
{
  int value = 0;
  ParseDateTime("support_data.image_date", m_AcquisitionDateFields);
  if (m_AcquisitionDateFields.size() > 1)
  {
    value = Utils::LexicalCast<int>(m_AcquisitionDateFields[1], "support_data.image_date:month(int)");
  }
  else
  {
    itkExceptionMacro(<< "Invalid month");
  }
  return value;
}

int Sentinel1ImageMetadataInterface::GetDay() const
{
  int value = 0;
  ParseDateTime("support_data.image_date", m_AcquisitionDateFields);
  if (m_AcquisitionDateFields.size() > 2)
  {
    value = Utils::LexicalCast<int>(m_AcquisitionDateFields[2], "support_data.image_date:day(int)");
  }
  else
  {
    itkExceptionMacro(<< "Invalid day");
  }
  return value;
}

int Sentinel1ImageMetadataInterface::GetHour() const
{
  int value = 0;
  ParseDateTime("support_data.image_date", m_AcquisitionDateFields);
  if (m_AcquisitionDateFields.size() > 3)
  {
    value = Utils::LexicalCast<int>(m_AcquisitionDateFields[3], "support_data.image_date:hour(int)");
  }
  else
  {
    itkExceptionMacro(<< "Invalid hour");
  }
  return value;
}

int Sentinel1ImageMetadataInterface::GetMinute() const
{
  int value = 0;
  ParseDateTime("support_data.image_date", m_AcquisitionDateFields);
  if (m_AcquisitionDateFields.size() > 4)
  {
    value = Utils::LexicalCast<int>(m_AcquisitionDateFields[4], "support_data.image_date:minute(int)");
  }
  else
  {
    itkExceptionMacro(<< "Invalid minute");
  }
  return value;
}

int Sentinel1ImageMetadataInterface::GetProductionYear() const
{
  int value = 0;
  ParseDateTime("support_data.date", m_ProductionDateFields);
  if (m_ProductionDateFields.size() > 0)
  {
    value = Utils::LexicalCast<int>(m_ProductionDateFields[0], "support_data.date:year(int)");
  }
  else
  {
    itkExceptionMacro(<< "Invalid production year");
  }
  return value;
}

int Sentinel1ImageMetadataInterface::GetProductionMonth() const
{
  int value = 0;
  ParseDateTime("support_data.date", m_ProductionDateFields);
  if (m_ProductionDateFields.size() > 1)
  {
    value = Utils::LexicalCast<int>(m_ProductionDateFields[1], "support_data.date:month(int)");
  }
  else
  {
    itkExceptionMacro(<< "Invalid production month");
  }
  return value;
}

int Sentinel1ImageMetadataInterface::GetProductionDay() const
{
  int value = 0;
  ParseDateTime("support_data.date", m_ProductionDateFields);
  if (m_ProductionDateFields.size() > 2)
  {
    value = Utils::LexicalCast<int>(m_ProductionDateFields[2], "support_data.date:day(int)");
  }
  else
  {
    itkExceptionMacro(<< "Invalid production day");
  }
  return value;
}

double Sentinel1ImageMetadataInterface::GetPRF() const
{
  double                     value            = 0;
  const ImageKeywordlistType imageKeywordlist = this->GetImageKeywordlist();
  if (!imageKeywordlist.HasKey("support_data.pulse_repetition_frequency"))
  {
    return value;
  }

  value = Utils::LexicalCast<double>(imageKeywordlist.GetMetadataByKey("support_data.pulse_repetition_frequency"),
                                     "support_data.pulse_repetition_frequency(double)");

  return value;
}


Sentinel1ImageMetadataInterface::UIntVectorType Sentinel1ImageMetadataInterface::GetDefaultDisplay() const
{
  UIntVectorType rgb(3);
  rgb[0] = 0;
  rgb[1] = 0;
  rgb[2] = 0;
  return rgb;
}

double Sentinel1ImageMetadataInterface::GetRSF() const
{
  return 0;
}

double Sentinel1ImageMetadataInterface::GetRadarFrequency() const
{
  return 0;
}

double Sentinel1ImageMetadataInterface::GetCenterIncidenceAngle() const
{
  return 0;
}

std::vector<OTB_azimuthFmRate> Sentinel1ImageMetadataInterface::GetAzimuthFmRate(XMLMetadataSupplier xmlMS) const
{
  std::vector<OTB_azimuthFmRate> azimuthFmRateVector;
  // Number of entries in the vector
  int listCount = xmlMS.GetAs<int>("product.generalAnnotation.azimuthFmRateList.count");
  // This streams wild hold the iteration number
  std::ostringstream oss;
  for (int listId = 1 ; listId <= listCount ; ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "product.generalAnnotation.azimuthFmRateList.azimuthFmRate_" + oss.str();
    OTB_azimuthFmRate afr;
    std::istringstream(xmlMS.GetAs<std::string>(path_root + ".azimuthTime")) >> afr.azimuthTime;
    afr.t0 = xmlMS.GetAs<double>((path_root + ".t0").c_str());
    afr.azimuthFmRatePolynomial = xmlMS.GetAsVector<double>(path_root + ".azimuthFmRatePolynomial",
    		' ', xmlMS.GetAs<int>((path_root + ".azimuthFmRatePolynomial.count").c_str()));
    azimuthFmRateVector.push_back(afr);
  }
  return azimuthFmRateVector;
}

std::vector<OTB_dopplerCentroid> Sentinel1ImageMetadataInterface::GetDopplerCentroid(XMLMetadataSupplier xmlMS) const
{
  std::vector<OTB_dopplerCentroid> dopplerCentroidVector;
  // Number of entries in the vector
  int listCount = xmlMS.GetAs<int>("product.dopplerCentroid.dcEstimateList.count");
  // This streams wild hold the iteration number
  std::ostringstream oss;
  for (int listId = 1 ; listId <= listCount ; ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "product.dopplerCentroid.dcEstimateList.dcEstimate_" + oss.str();
    OTB_dopplerCentroid dopplerCent;
    std::istringstream(xmlMS.GetAs<std::string>(path_root + ".azimuthTime")) >> dopplerCent.azimuthTime;
    dopplerCent.t0 = xmlMS.GetAs<double>(path_root + ".t0");
    dopplerCent.dopCoef = xmlMS.GetAsVector<double>(path_root + ".dataDcPolynomial",
    		' ', xmlMS.GetAs<int>((path_root + ".dataDcPolynomial.count").c_str()));
	dopplerCent.geoDopCoef = xmlMS.GetAsVector<double>(path_root + ".geometryDcPolynomial",
    		' ', xmlMS.GetAs<int>((path_root + ".geometryDcPolynomial.count").c_str()));
	dopplerCentroidVector.push_back(dopplerCent);
  }
  return dopplerCentroidVector;
}

std::vector<OTB_Orbit> Sentinel1ImageMetadataInterface::GetOrbits(XMLMetadataSupplier xmlMS) const
{
  std::vector<OTB_Orbit> orbitVector;
  // Number of entries in the vector
  int listCount = xmlMS.GetAs<int>("product.generalAnnotation.orbitList.count");
  // This streams wild hold the iteration number
  std::ostringstream oss;
  for (int listId = 1 ; listId <= listCount ; ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "product.generalAnnotation.orbitList.orbit_" + oss.str();
    OTB_Orbit orbit;
    std::istringstream(xmlMS.GetAs<std::string>(path_root + ".time")) >> orbit.time;
    orbit.posX = xmlMS.GetAs<double>(path_root + ".position.x");
    orbit.posY = xmlMS.GetAs<double>(path_root + ".position.y");
    orbit.posZ = xmlMS.GetAs<double>(path_root + ".position.z");
    orbit.velX = xmlMS.GetAs<double>(path_root + ".velocity.x");
    orbit.velY = xmlMS.GetAs<double>(path_root + ".velocity.y");
    orbit.velZ = xmlMS.GetAs<double>(path_root + ".velocity.z");
    orbitVector.push_back(orbit);
  }
  return orbitVector;
}

std::vector<OTB_calibrationVector> Sentinel1ImageMetadataInterface::GetCalibrationVector(XMLMetadataSupplier xmlMS) const
{
  std::vector<OTB_calibrationVector> calibrationVector;
  // Number of entries in the vector
  int listCount = xmlMS.GetAs<int>("calibration.calibrationVectorList.count");
  // This streams wild hold the iteration number
  std::ostringstream oss;
  for (int listId = 1 ; listId <= listCount ; ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "calibration.calibrationVectorList.calibrationVector_" + oss.str();

    OTB_calibrationVector calVect;
    std::istringstream(xmlMS.GetAs<std::string>(path_root + ".azimuthTime")) >> calVect.azimuthTime;
    calVect.line = xmlMS.GetAs<int>((path_root + ".line").c_str());

    // Same axe for all LUTs
    MetaData::LUTAxis ax1;
    ax1.Size = xmlMS.GetAs<int>((path_root + ".pixel.count").c_str());
    ax1.Values = xmlMS.GetAsVector<double>((path_root + ".pixel").c_str(), ' ', ax1.Size);

    MetaData::LUT1D sigmaNoughtLut;
    sigmaNoughtLut.Axis[0] = ax1;
    sigmaNoughtLut.Array = xmlMS.GetAsVector<double>((path_root + ".sigmaNought").c_str(),
     		' ', xmlMS.GetAs<int>((path_root + ".sigmaNought.count").c_str()));
    calVect.sigmaNought = sigmaNoughtLut;

    MetaData::LUT1D betaNoughtLut;
    betaNoughtLut.Axis[0] = ax1;
    betaNoughtLut.Array = xmlMS.GetAsVector<double>((path_root + ".betaNought").c_str(),
     		' ', xmlMS.GetAs<int>((path_root + ".betaNought.count").c_str()));
    calVect.betaNought = betaNoughtLut;

    MetaData::LUT1D gammaLut;
    gammaLut.Axis[0] = ax1;
    gammaLut.Array = xmlMS.GetAsVector<double>((path_root + ".gamma").c_str(),
     		' ', xmlMS.GetAs<int>((path_root + ".gamma.count").c_str()));
    calVect.gamma = gammaLut;

    MetaData::LUT1D dnLut;
    dnLut.Axis[0] = ax1;
    dnLut.Array = xmlMS.GetAsVector<double>((path_root + ".dn").c_str(),
     		' ', xmlMS.GetAs<int>((path_root + ".dn.count").c_str()));
    calVect.dn = dnLut;

    calibrationVector.push_back(calVect);
  }
  return calibrationVector;
}

std::vector<OTB_SARNoise> Sentinel1ImageMetadataInterface::GetNoiseVector(XMLMetadataSupplier xmlMS) const
{
  std::vector<OTB_SARNoise> noiseVector;
  // Number of entries in the vector
  int listCount = xmlMS.GetAs<int>("noise.noiseVectorList.count");
  // This streams wild hold the iteration number
  std::ostringstream oss;
  for (int listId = 1 ; listId <= listCount ; ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "noise.noiseVectorList.noiseVector_" + oss.str();
    OTB_SARNoise noiseVect;
    std::istringstream(xmlMS.GetAs<std::string>(path_root + ".azimuthTime")) >> noiseVect.azimuthTime;
    MetaData::LUT1D noiseLut;
    MetaData::LUTAxis ax1;
    ax1.Size = xmlMS.GetAs<int>((path_root + ".pixel.count").c_str());
    ax1.Values = xmlMS.GetAsVector<double>((path_root + ".pixel").c_str(), ' ', ax1.Size);
    noiseLut.Axis[0] = ax1;
    noiseLut.Array = xmlMS.GetAsVector<double>((path_root + ".noiseLut").c_str(),
    		' ', xmlMS.GetAs<int>((path_root + ".noiseLut.count").c_str()));
    noiseVect.noiseLut = noiseLut;
    noiseVector.push_back(noiseVect);
  }
  return noiseVector;
}

double Sentinel1ImageMetadataInterface::getBandTerrainHeight(XMLMetadataSupplier xmlMS) const
{
  double heightSum = 0.0;
  // Number of entries in the vector
  int listCount = xmlMS.GetAs<int>("product.generalAnnotation.terrainHeightList.count");
  // This streams wild hold the iteration number
  std::ostringstream oss;
  for (int listId = 1 ; listId <= listCount ; ++listId)
  {
    oss.str("");
    oss << listId;
    // Base path to the data, that depends on the iteration number
    std::string path_root = "product.generalAnnotation.terrainHeightList.terrainHeight_" + oss.str();
    heightSum += xmlMS.GetAs<double>((path_root + ".value").c_str());
  }
  return heightSum / (double)listCount;
}

void Sentinel1ImageMetadataInterface::Parse(const MetadataSupplierInterface *mds)
{
  assert(mds);
  assert(mds->GetNbBands() == this->m_Imd.Bands.size());
  // Metadata read by GDAL
  Fetch(MDTime::AcquisitionStartTime, mds, "ACQUISITION_START_TIME");
  Fetch(MDTime::AcquisitionStopTime, mds, "ACQUISITION_STOP_TIME");
  Fetch(MDStr::BeamMode, mds, "BEAM_MODE");
  Fetch(MDStr::BeamSwath, mds, "BEAM_SWATH");
  Fetch("FACILITY_IDENTIFIER", mds, "FACILITY_IDENTIFIER");
  Fetch(MDNum::LineSpacing, mds, "LINE_SPACING");
  Fetch(MDStr::Mission, mds, "MISSION_ID");
  Fetch(MDStr::Mode, mds, "MODE");
  Fetch(MDStr::OrbitDirection, mds, "ORBIT_DIRECTION");
  Fetch(MDNum::OrbitNumber, mds, "ORBIT_NUMBER");
  Fetch(MDNum::PixelSpacing, mds, "PIXEL_SPACING");
  Fetch(MDStr::ProductType, mds, "PRODUCT_TYPE");
  Fetch(MDStr::Instrument, mds, "SATELLITE_IDENTIFIER");
  Fetch(MDStr::SensorID, mds, "SENSOR_IDENTIFIER");
  Fetch(MDStr::Swath, mds, "SWATH");

  // Manifest file
  std::string ManifestFilePath = mds->GetResourceFile(std::string("manifest\\.safe"));
  if (!ManifestFilePath.empty())
  {
    XMLMetadataSupplier ManifestMS = XMLMetadataSupplier(ManifestFilePath);
    m_Imd.Add(MDTime::ProductionDate,
    		ManifestMS.GetFirstAs<MetaData::Time>("xfdu:XFDU.metadataSection.metadataObject_#.metadataWrap.xmlData.safe:processing.start"));
    m_Imd.Add(MDTime::AcquisitionDate,
    		ManifestMS.GetFirstAs<MetaData::Time>("xfdu:XFDU.metadataSection.metadataObject_#.metadataWrap.xmlData.safe:acquisitionPeriod.safe:startTime"));
  }

  // Band metadata
  for (int bandId = 0 ; bandId < mds->GetNbBands() ; ++bandId)
  {
    Projection::SARParam sarParam;
    Fetch(MDStr::Polarization, mds, "POLARISATION", bandId);
    std::string swath = Fetch(MDStr::Swath, mds, "SWATH", bandId);

    // Annotation file
    std::string AnnotationFilePath = mds->GetResourceFile(std::string("annotation[/\\\\]s1[ab].*-")
                                                          + itksys::SystemTools::LowerCase(swath)
                                                          + std::string("-.*\\.xml"));
    if (AnnotationFilePath.empty())
      otbGenericExceptionMacro(MissingMetadataException,<<"Missing Annotation file for band '"<<swath<<"'");
    XMLMetadataSupplier AnnotationMS = XMLMetadataSupplier(AnnotationFilePath);

    sarParam.azimuthFmRate = this->GetAzimuthFmRate(AnnotationMS);
    sarParam.dopplerCentroid = this->GetDopplerCentroid(AnnotationMS);
    sarParam.orbits = this->GetOrbits(AnnotationMS);
    m_Imd.Add(MDNum::NumberOfLines, AnnotationMS.GetAs<int>("product.imageAnnotation.imageInformation.numberOfLines"));
    m_Imd.Add(MDNum::NumberOfColumns, AnnotationMS.GetAs<int>("product.imageAnnotation.imageInformation.numberOfSamples"));
    m_Imd.Add(MDNum::AverageSceneHeight, this->getBandTerrainHeight(AnnotationFilePath));
    m_Imd.Add(MDNum::RadarFrequency, AnnotationMS.GetAs<double>("product.generalAnnotation.productInformation.radarFrequency"));
    m_Imd.Add(MDNum::PRF, AnnotationMS.GetAs<double>("product.imageAnnotation.imageInformation.azimuthFrequency"));
    m_Imd.Add(MDNum::CenterIncidenceAngle, AnnotationMS.GetAs<double>("product.imageAnnotation.imageInformation.incidenceAngleMidSwath"));

    // Calibration file
    std::string CalibrationFilePath = itksys::SystemTools::GetFilenamePath(AnnotationFilePath)
                                      + "/calibration/calibration-"
                                      + itksys::SystemTools::GetFilenameName(AnnotationFilePath);
    if (CalibrationFilePath.empty())
          otbGenericExceptionMacro(MissingMetadataException,<<"Missing Calibration file for band '"<<swath<<"'");
    XMLMetadataSupplier CalibrationMS = XMLMetadataSupplier(CalibrationFilePath);
    m_Imd.Add(MDNum::CalScale, CalibrationMS.GetAs<double>("calibration.calibrationInformation.absoluteCalibrationConstant"));
    sarParam.calibrationVectors = this->GetCalibrationVector(CalibrationMS);
    std::istringstream(CalibrationMS.GetAs<std::string>("calibration.adsHeader.startTime")) >> sarParam.calibrationStartTime;
    std::istringstream(CalibrationMS.GetAs<std::string>("calibration.adsHeader.stopTime")) >> sarParam.calibrationStopTime;

    // Noise file
    std::string NoiseFilePath = itksys::SystemTools::GetFilenamePath(AnnotationFilePath)
                                            + "/calibration/noise-"
                                            + itksys::SystemTools::GetFilenameName(AnnotationFilePath);
    if (NoiseFilePath.empty())
          otbGenericExceptionMacro(MissingMetadataException,<<"Missing Noise file for band '"<<swath<<"'");
    XMLMetadataSupplier NoiseMS = XMLMetadataSupplier(NoiseFilePath);

    m_Imd.Bands[bandId].Add(MDGeom::SAR, sarParam);
  }
}

} // end namespace otb
