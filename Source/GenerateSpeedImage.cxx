#include "itkCurvatureAnisotropicDiffusionImageFilter.h"
#include "itkGradientMagnitudeRecursiveGaussianImageFilter.h"
#include "itkSigmoidImageFilter.h"
#include "itkImage.h"

#include "itkFastMarchingImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkImageRegionIterator.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkLabelContourImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"


template <unsigned int ImageDimension>
int GenerateSpeedImage( unsigned int argc, char *argv[] )
{
  typedef float InternalPixelType;
  typedef itk::Image<InternalPixelType, ImageDimension>  InternalImageType;

  typedef float OutputPixelType;
  typedef itk::Image<OutputPixelType, ImageDimension> OutputImageType;

  typedef  itk::ImageFileReader< InternalImageType> ReaderType;
  typename ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName( argv[2] );
  reader->Update();

  typedef   itk::CurvatureAnisotropicDiffusionImageFilter< 
                               InternalImageType, 
                               InternalImageType >  SmoothingFilterType;
  typedef   itk::GradientMagnitudeRecursiveGaussianImageFilter< 
                               InternalImageType, 
                               InternalImageType >  GradientFilterType;
  typedef   itk::SigmoidImageFilter<
                               InternalImageType, 
                               InternalImageType >  SigmoidFilterType;
  typename SmoothingFilterType::Pointer smoothing = SmoothingFilterType::New();
  typename GradientFilterType::Pointer  gradientMagnitude = GradientFilterType::New();
  typename SigmoidFilterType::Pointer sigmoid = SigmoidFilterType::New();
  
  sigmoid->SetOutputMinimum(  0.0  );
  sigmoid->SetOutputMaximum(  1.0  );
  
  smoothing->SetInput( reader->GetOutput() );
  if( ImageDimension == 2 )
    {
    smoothing->SetTimeStep( 0.125 );
    }
  else  
    {
    smoothing->SetTimeStep( 0.0625 );
    }
  smoothing->SetNumberOfIterations(  5 );
  smoothing->SetConductanceParameter( 9.0 );

  gradientMagnitude->SetInput( smoothing->GetOutput() );
  gradientMagnitude->SetSigma( atof( argv[4] ) );

  sigmoid->SetInput( gradientMagnitude->GetOutput() );
  sigmoid->SetAlpha( atof( argv[5] ) );
  sigmoid->SetBeta( atof( argv[6] ) );

  typedef  itk::ImageFileWriter<OutputImageType> WriterType;
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetInput( sigmoid->GetOutput() );
  writer->SetFileName( argv[3] );
  writer->Update();

  return 0;
}

int main( int argc, char *argv[] )
{
  if( argc < 6 )
    {
    std::cerr << "Usage: " << argv[0] << " imageDimension";
    std::cerr << " inputImage outputImage sigma sigmoidAlpha sigmoidBeta";
    std::cerr << std::endl;
    return 1;
    }

  switch( atoi( argv[1] ) ) 
   {
   case 2:
     GenerateSpeedImage<2>( argc, argv );
     break;
   case 3:
     GenerateSpeedImage<3>( argc, argv );
     break;
   default:
      std::cerr << "Unsupported dimension" << std::endl;
      exit( EXIT_FAILURE );
   }
}

