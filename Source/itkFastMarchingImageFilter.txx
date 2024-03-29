/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#ifndef __itkFastMarchingImageFilter_txx
#define __itkFastMarchingImageFilter_txx

#include "itkFastMarchingImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkImageRegionIterator.h"
#include "itkNumericTraits.h"
#include "itkRelabelComponentImageFilter.h"
#include "vnl/vnl_math.h"
#include <algorithm>

namespace itk
{
template< class TLevelSet, class TSpeedImage >
FastMarchingImageFilter< TLevelSet, TSpeedImage >
::FastMarchingImageFilter():
  m_TrialHeap()
{
  this->ProcessObject::SetNumberOfRequiredInputs(0);

  OutputSizeType outputSize;
  outputSize.Fill(16);
  typename LevelSetImageType::IndexType outputIndex;
  outputIndex.Fill(0);

  m_OutputRegion.SetSize(outputSize);
  m_OutputRegion.SetIndex(outputIndex);

  m_OutputOrigin.Fill(0.0);
  m_OutputSpacing.Fill(1.0);
  m_OutputDirection.SetIdentity();
  m_OverrideOutputInformation = false;

  m_AlivePoints = NULL;
  m_TrialPoints = NULL;
  m_ProcessedPoints = NULL;

  m_SpeedConstant = 1.0;
  m_InverseSpeed = -1.0;
  m_LabelImage = LabelImageType::New();

  m_LargeValue    = static_cast< PixelType >( NumericTraits< PixelType >::max() / 2.0 );
  m_StoppingValue = static_cast< double >( m_LargeValue );
  m_CollectPoints = false;

  m_NormalizationFactor = 1.0;
  m_TopologyCheck = None;
}

template< class TLevelSet, class TSpeedImage >
void
FastMarchingImageFilter< TLevelSet, TSpeedImage >
::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
  os << indent << "Alive points: " << m_AlivePoints.GetPointer() << std::endl;
  os << indent << "Trial points: " << m_TrialPoints.GetPointer() << std::endl;
  os << indent << "Speed constant: " << m_SpeedConstant << std::endl;
  os << indent << "Stopping value: " << m_StoppingValue << std::endl;
  os << indent << "Large Value: "
     << static_cast< typename NumericTraits< PixelType >::PrintType >( m_LargeValue )
     << std::endl;
  os << indent << "Normalization Factor: " << m_NormalizationFactor << std::endl;
  os << indent << "Collect points: " << m_CollectPoints << std::endl;
  os << indent << "Topology check: ";
  switch( this->m_TopologyCheck )
    {
    default:
    case None:
      {
      os << "None" << std::endl;
      break;
      }
    case NoHandles:
      {
      os << "No handles" << std::endl;
      break;
      }
    case Strict:
      {
      os << "Strict" << std::endl;
      break;
      }
    }
  os << indent << "OverrideOutputInformation: ";
  os << m_OverrideOutputInformation << std::endl;
  os << indent << "OutputRegion: " << m_OutputRegion << std::endl;
  os << indent << "OutputOrigin:  " << m_OutputOrigin << std::endl;
  os << indent << "OutputSpacing: " << m_OutputSpacing << std::endl;
  os << indent << "OutputDirection: " << m_OutputDirection << std::endl;
}

template< class TLevelSet, class TSpeedImage >
void
FastMarchingImageFilter< TLevelSet, TSpeedImage >
::GenerateOutputInformation()
{
  // copy output information from input image
  Superclass::GenerateOutputInformation();

  // use user-specified output information
  if ( this->GetInput() == NULL || m_OverrideOutputInformation )
    {
    LevelSetPointer output = this->GetOutput();
    output->SetLargestPossibleRegion(m_OutputRegion);
    output->SetOrigin(m_OutputOrigin);
    output->SetSpacing(m_OutputSpacing);
    output->SetDirection(m_OutputDirection);
    }
}

template< class TLevelSet, class TSpeedImage >
void
FastMarchingImageFilter< TLevelSet, TSpeedImage >
::EnlargeOutputRequestedRegion(
  DataObject *output)
{
  // enlarge the requested region of the output
  // to the whole data set
  TLevelSet *imgData;

  imgData = dynamic_cast< TLevelSet * >( output );
  if ( imgData )
    {
    imgData->SetRequestedRegionToLargestPossibleRegion();
    }
  else
    {
    // Pointer could not be cast to TLevelSet *
    itkWarningMacro( << "itk::FastMarchingImageFilter"
                     << "::EnlargeOutputRequestedRegion cannot cast "
                     << typeid( output ).name() << " to "
                     << typeid( TLevelSet * ).name() );
    }
}

template< class TLevelSet, class TSpeedImage >
void
FastMarchingImageFilter< TLevelSet, TSpeedImage >
::Initialize(LevelSetImageType *output)
{
  // allocate memory for the output buffer
  output->SetBufferedRegion( output->GetRequestedRegion() );
  output->Allocate();

  // cache some buffered region information
  m_BufferedRegion = output->GetBufferedRegion();
  m_StartIndex = m_BufferedRegion.GetIndex();
  m_LastIndex = m_StartIndex + m_BufferedRegion.GetSize();
  typename LevelSetImageType::OffsetType offset;
  offset.Fill(1);
  m_LastIndex -= offset;

  // allocate memory for the PointTypeImage
  m_LabelImage->CopyInformation(output);
  m_LabelImage->SetBufferedRegion(
    output->GetBufferedRegion() );
  m_LabelImage->Allocate();

  // Checking for handles only requires an image to keep track of
  // connected components.
  if( this->m_TopologyCheck == NoHandles )
    {
    m_ConnectedComponentImage = ConnectedComponentImageType::New();
    m_ConnectedComponentImage->SetOrigin( output->GetOrigin() );
    m_ConnectedComponentImage->SetSpacing( output->GetSpacing() );
    m_ConnectedComponentImage->SetRegions( output->GetBufferedRegion() );
    m_ConnectedComponentImage->SetDirection( output->GetDirection() );
    m_ConnectedComponentImage->Allocate();
    m_ConnectedComponentImage->FillBuffer( 0 );
    }

  // set all output value to infinity
  typedef ImageRegionIterator< LevelSetImageType >
  OutputIterator;

  OutputIterator outIt ( output, output->GetBufferedRegion() );

  PixelType outputPixel;
  outputPixel = m_LargeValue;

  for ( outIt.GoToBegin(); !outIt.IsAtEnd(); ++outIt )
    {
    outIt.Set(outputPixel);
    }

  // set all points type to FarPoint
  typedef ImageRegionIterator< LabelImageType > LabelIterator;

  LabelIterator typeIt( m_LabelImage,
                        m_LabelImage->GetBufferedRegion() );

  for ( typeIt.GoToBegin(); !typeIt.IsAtEnd(); ++typeIt )
    {
    typeIt.Set(FarPoint);
    }

  // process input alive points
  AxisNodeType node;

  if ( m_AlivePoints )
    {
    typename NodeContainer::ConstIterator pointsIter = m_AlivePoints->Begin();
    typename NodeContainer::ConstIterator pointsEnd = m_AlivePoints->End();

    for (; pointsIter != pointsEnd; ++pointsIter )
      {
      // get node from alive points container
      node = pointsIter.Value();

      // check if node index is within the output level set
      if ( !m_BufferedRegion.IsInside( node.GetIndex() ) )
        {
        continue;
        }

      // make this an alive point
      m_LabelImage->SetPixel(node.GetIndex(), AlivePoint);

      //
      if( m_TopologyCheck == NoHandles )
        {
        m_ConnectedComponentImage->SetPixel( node.GetIndex(),
          NumericTraits<typename ConnectedComponentImageType::PixelType>::One );
        }

      outputPixel = node.GetValue();
      output->SetPixel(node.GetIndex(), outputPixel);
      }
    }

  if( m_TopologyCheck == NoHandles )
    {
    // Now create the connected component image and relabel such that labels
    // are 1, 2, 3, ...
    typedef ConnectedComponentImageFilter<ConnectedComponentImageType,
      ConnectedComponentImageType> ConnectedComponentFilterType;
    typename ConnectedComponentFilterType::Pointer connecter
      = ConnectedComponentFilterType::New();
    connecter->SetInput( m_ConnectedComponentImage );

    typedef RelabelComponentImageFilter<ConnectedComponentImageType,
      ConnectedComponentImageType> RelabelerType;
    typename RelabelerType::Pointer relabeler = RelabelerType::New();
    relabeler->SetInput( connecter->GetOutput() );
    relabeler->Update();

    this->m_ConnectedComponentImage = relabeler->GetOutput();
    }

  // make sure the heap is empty
  while ( !m_TrialHeap.empty() )
    {
    m_TrialHeap.pop();
    }

  // process the input trial points
  if ( m_TrialPoints )
    {
    typename NodeContainer::ConstIterator pointsIter = m_TrialPoints->Begin();
    typename NodeContainer::ConstIterator pointsEnd = m_TrialPoints->End();

    for (; pointsIter != pointsEnd; ++pointsIter )
      {
      // get node from trial points container
      node = pointsIter.Value();

      // check if node index is within the output level set
      if ( !m_BufferedRegion.IsInside( node.GetIndex() ) )
        {
        continue;
        }

      // make this an initial trial point
      m_LabelImage->SetPixel(node.GetIndex(), InitialTrialPoint);

      outputPixel = node.GetValue();
      output->SetPixel(node.GetIndex(), outputPixel);

      m_TrialHeap.push(node);

      }
    }

  // initialize indices if this->m_TopologyCheck is activated
  if( m_TopologyCheck != None )
    {
    if( SetDimension == 2 )
      {
      InitializeIndices2D();
      }
    else if( SetDimension == 3 )
      {
      InitializeIndices3D();
      }
    else
      {
      itkExceptionMacro(
        "Topology checking is only valid for level set dimensions of 2 and 3" );
      }
    }
}

template< class TLevelSet, class TSpeedImage >
void
FastMarchingImageFilter< TLevelSet, TSpeedImage >
::GenerateData()
{
  LevelSetPointer        output      = this->GetOutput();
  SpeedImageConstPointer speedImage  = this->GetInput();

  this->Initialize(output);

  if ( m_CollectPoints )
    {
    m_ProcessedPoints = NodeContainer::New();
    }

  // process points on the heap
  AxisNodeType node;
  double       currentValue;
  double       oldProgress = 0;

  this->UpdateProgress(0.0);   // Send first progress event

  while ( !m_TrialHeap.empty() )
    {
    // get the node with the smallest value
    node = m_TrialHeap.top();
    m_TrialHeap.pop();

    // does this node contain the current value ?
    currentValue = (double)output->GetPixel( node.GetIndex() );

    if ( node.GetValue() != currentValue )
      {
      continue;
      }

    // is this node already alive ?
    if ( m_LabelImage->GetPixel( node.GetIndex() ) == AlivePoint )
      {
      continue;
      }

    if( this->m_TopologyCheck != None )
      {
      bool wellComposednessViolation
        = this->DoesVoxelChangeViolateWellComposedness( node.GetIndex() );
      bool strictTopologyViolation
        = this->DoesVoxelChangeViolateStrictTopology( node.GetIndex() );
      if( ( this->m_TopologyCheck == Strict ) &&
          ( wellComposednessViolation || strictTopologyViolation ) )
        {
        output->SetPixel( node.GetIndex(), -0.00000001 );
        this->m_LabelImage->SetPixel( node.GetIndex(), TopologyPoint );
        continue;
        }
      if( this->m_TopologyCheck == NoHandles )
        {
        if( wellComposednessViolation )
          {
					output->SetPixel( node.GetIndex(), -0.00000001 );
					this->m_LabelImage->SetPixel( node.GetIndex(), TopologyPoint );
					continue;
          }
        if( strictTopologyViolation )
					{
					// check for handles
					typename NeighborhoodIteratorType::RadiusType radius;
					radius.Fill( 1 );
					NeighborhoodIteratorType ItL( radius, this->m_LabelImage,
					  this->m_LabelImage->GetBufferedRegion() );
					ItL.SetLocation( node.GetIndex() );
					NeighborhoodIterator<ConnectedComponentImageType> ItC(
					  radius, this->m_ConnectedComponentImage,
						this->m_ConnectedComponentImage->GetBufferedRegion() );
				  ItC.SetLocation( node.GetIndex() );

					typename ConnectedComponentImageType::PixelType minLabel
					  = NumericTraits<typename ConnectedComponentImageType::PixelType>::Zero;
					typename ConnectedComponentImageType::PixelType otherLabel
						= NumericTraits<typename ConnectedComponentImageType::PixelType>::Zero;

					bool doesChangeCreateHandle = false;

					for( unsigned int d = 0; d < SetDimension; d++ )
					  {
						if( ItL.GetNext( d ) == AlivePoint &&
                ItL.GetPrevious( d ) == AlivePoint )
						  {
							if( ItC.GetNext( d ) == ItC.GetPrevious( d ) )
							  {
								doesChangeCreateHandle = true;
								}
						  else
							  {
								minLabel = vnl_math_min( ItC.GetNext( d ), ItC.GetPrevious( d ) );
								otherLabel = vnl_math_max( ItC.GetNext( d ), ItC.GetPrevious( d ) );
								}
						break;
														}
												}
										if( doesChangeCreateHandle )
												{
												output->SetPixel( node.GetIndex(), -0.0001 );
												this->m_LabelImage->SetPixel( node.GetIndex(), TopologyPoint );
												continue;
												}
										else
												{
												for( ItC.GoToBegin(); !ItC.IsAtEnd(); ++ItC )
														{
														if( ItC.GetCenterPixel() == otherLabel )
																{
																ItC.SetCenterPixel( minLabel );
																}
														}
												}
										}
        }
      }

    if ( currentValue > m_StoppingValue )
      {
      this->UpdateProgress(1.0);
      break;
      }

    if ( m_CollectPoints )
      {
      m_ProcessedPoints->InsertElement(m_ProcessedPoints->Size(), node);
      }

    // set this node as alive
    m_LabelImage->SetPixel(node.GetIndex(), AlivePoint);

    // for topology handle checks, we need to update the connected
    // component image at the current node with the appropriate label.
    if( this->m_TopologyCheck == NoHandles )
      {
      typename ConnectedComponentImageType::PixelType neighborhoodLabel
        = NumericTraits<typename ConnectedComponentImageType::PixelType>::Zero;

      typename NeighborhoodIteratorType::RadiusType radius;
      radius.Fill( 1 );
      NeighborhoodIterator<ConnectedComponentImageType> ItC(
        radius, this->m_ConnectedComponentImage,
        this->m_ConnectedComponentImage->GetBufferedRegion() );
      ItC.SetLocation( node.GetIndex() );

      for( unsigned int n = 0; n < ItC.Size(); n++ )
        {
        if( n == static_cast<unsigned int>( 0.5 * ItC.Size() ) )
          {
          continue;
          }
        typename ConnectedComponentImageType::PixelType c = ItC.GetPixel( n );
        if( c > 0 )
          {
          neighborhoodLabel = c;
          break;
          }
        }
						if( neighborhoodLabel > 0 )
								{
								ItC.SetCenterPixel( neighborhoodLabel );
								}
      }

    // update its neighbors
    this->UpdateNeighbors(node.GetIndex(), speedImage, output);

    // Send events every certain number of points.
    const double newProgress = currentValue / m_StoppingValue;
    if ( newProgress - oldProgress > 0.01 )  // update every 1%
      {
      this->UpdateProgress(newProgress);
      oldProgress = newProgress;
      if ( this->GetAbortGenerateData() )
        {
        this->InvokeEvent( AbortEvent() );
        this->ResetPipeline();
        ProcessAborted e(__FILE__, __LINE__);
        e.SetDescription("Process aborted.");
        e.SetLocation(ITK_LOCATION);
        throw e;
        }
      }
    }
}

template< class TLevelSet, class TSpeedImage >
void
FastMarchingImageFilter< TLevelSet, TSpeedImage >
::UpdateNeighbors(
  const IndexType & index,
  const SpeedImageType *speedImage,
  LevelSetImageType *output)
{
  IndexType neighIndex = index;

  for ( unsigned int j = 0; j < SetDimension; j++ )
    {
    // update left neighbor
    if ( index[j] > m_StartIndex[j] )
      {
      neighIndex[j] = index[j] - 1;
      }
    unsigned char label = m_LabelImage->GetPixel(neighIndex);
    if ( label != AlivePoint && label != InitialTrialPoint )
      {
      this->UpdateValue(neighIndex, speedImage, output);
      }

    // update right neighbor
    if ( index[j] < m_LastIndex[j] )
      {
      neighIndex[j] = index[j] + 1;
      }
    label = m_LabelImage->GetPixel(neighIndex);
    if ( label != AlivePoint && label != InitialTrialPoint )
      {
      this->UpdateValue(neighIndex, speedImage, output);
      }

    //reset neighIndex
    neighIndex[j] = index[j];
    }
}

template< class TLevelSet, class TSpeedImage >
double
FastMarchingImageFilter< TLevelSet, TSpeedImage >
::UpdateValue(
  const IndexType & index,
  const SpeedImageType *speedImage,
  LevelSetImageType *output)
{
  IndexType neighIndex = index;

  typename TLevelSet::PixelType neighValue;
  PixelType    outputPixel;
  AxisNodeType node;

  for ( unsigned int j = 0; j < SetDimension; j++ )
    {
    node.SetValue(m_LargeValue);

    // find smallest valued neighbor in this dimension
    for ( int s = -1; s < 2; s = s + 2 )
      {
      neighIndex[j] = index[j] + s;

      if ( neighIndex[j] > m_LastIndex[j]
           || neighIndex[j] < m_StartIndex[j] )
        {
        continue;
        }

      if ( m_LabelImage->GetPixel(neighIndex) == AlivePoint )
        {
        outputPixel = output->GetPixel(neighIndex);
        neighValue = outputPixel;

        if ( node.GetValue() > neighValue )
          {
          node.SetValue(neighValue);
          node.SetIndex(neighIndex);
          }
        }
      }

    // put the minimum neighbor onto the heap
    m_NodesUsed[j] = node;
    m_NodesUsed[j].SetAxis(j);

    // reset neighIndex
    neighIndex[j] = index[j];
    }

  // sort the local list
  std::sort(m_NodesUsed, m_NodesUsed + SetDimension);

  // solve quadratic equation
  double aa, bb, cc;
  double solution = m_LargeValue;

  aa = 0.0;
  bb = 0.0;
  if ( speedImage )
    {
    typedef typename SpeedImageType::PixelType SpeedPixelType;
    cc = (double)speedImage->GetPixel(index) / m_NormalizationFactor;
    cc = -1.0 * vnl_math_sqr(1.0 / cc);
    }
  else
    {
    cc = m_InverseSpeed;
    }

  OutputSpacingType spacing = this->GetOutput()->GetSpacing();

  double discrim;

  for ( unsigned int j = 0; j < SetDimension; j++ )
    {
    node = m_NodesUsed[j];

    if ( solution >= node.GetValue() )
      {
      const int    axis = node.GetAxis();
      const double spaceFactor = vnl_math_sqr(1.0 / spacing[axis]);
      const double value = double( node.GetValue() );
      aa += spaceFactor;
      bb += value * spaceFactor;
      cc += vnl_math_sqr(value) * spaceFactor;

      discrim = vnl_math_sqr(bb) - aa * cc;
      if ( discrim < 0.0 )
        {
        // Discriminant of quadratic eqn. is negative
        ExceptionObject err(__FILE__, __LINE__);
        err.SetLocation(ITK_LOCATION);
        err.SetDescription("Discriminant of quadratic equation is negative");
        throw err;
        }

      solution = ( vcl_sqrt(discrim) + bb ) / aa;
      }
    else
      {
      break;
      }
    }

  if ( solution < m_LargeValue )
    {
    // write solution to m_OutputLevelSet
    outputPixel = static_cast< PixelType >( solution );
    output->SetPixel(index, outputPixel);

    // insert point into trial heap
    m_LabelImage->SetPixel(index, TrialPoint);
    node.SetValue( static_cast< PixelType >( solution ) );
    node.SetIndex(index);
    m_TrialHeap.push(node);
    }

  return solution;
}

/**
 * Topology check functions
 */
template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::DoesVoxelChangeViolateWellComposedness( IndexType idx )
{
  bool isChangeWellComposed = false;
  if( SetDimension == 2 )
    {
    isChangeWellComposed = this->IsChangeWellComposed2D( idx );
    }
  else  // SetDimension == 3
    {
    isChangeWellComposed = this->IsChangeWellComposed3D( idx );
    }

  return !isChangeWellComposed;
}

template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::DoesVoxelChangeViolateStrictTopology( IndexType idx )
{
  typename NeighborhoodIteratorType::RadiusType radius;
  radius.Fill( 1 );
  NeighborhoodIteratorType It( radius, this->m_LabelImage,
    this->m_LabelImage->GetBufferedRegion() );
  It.SetLocation( idx );

  unsigned int numberOfCriticalC3Configurations = 0;
  unsigned int numberOfFaces = 0;
  for( unsigned int d = 0; d < SetDimension; d++ )
    {
    if( It.GetNext( d ) == AlivePoint )
      {
      numberOfFaces++;
      }
    if( It.GetPrevious( d ) == AlivePoint )
      {
      numberOfFaces++;
      }
    if( It.GetNext( d ) == AlivePoint && It.GetPrevious( d ) == AlivePoint )
      {
      numberOfCriticalC3Configurations++;
      }
    }

  if( numberOfCriticalC3Configurations > 0 && numberOfFaces % 2 == 0
      && numberOfCriticalC3Configurations * 2 == numberOfFaces )
    {
    return true;
    }
  return false;
}

template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::IsChangeWellComposed2D( IndexType idx )
{
  typename NeighborhoodIteratorType::RadiusType radius;
  radius.Fill( 1 );
  NeighborhoodIteratorType It( radius, this->m_LabelImage,
    this->m_LabelImage->GetBufferedRegion() );
  It.SetLocation( idx );

  Array<short> neighborhoodPixels( 9 );

  // Check for critical configurations: 4 90-degree rotations

  for ( unsigned int i = 0; i < 4; i++ )
    {
    for ( unsigned int j = 0; j < 9; j++ )
      {
      neighborhoodPixels[j] =
        ( It.GetPixel( this->m_RotationIndices[i][j] ) != AlivePoint );
      if( this->m_RotationIndices[i][j] == 4 )
        {
        neighborhoodPixels[j] = !neighborhoodPixels[j];
        }
      }

    if( this->IsCriticalC1Configuration2D( neighborhoodPixels )
      || this->IsCriticalC2Configuration2D( neighborhoodPixels )
      || this->IsCriticalC3Configuration2D( neighborhoodPixels )
      || this->IsCriticalC4Configuration2D( neighborhoodPixels ) )
      {
      return false;
      }
    }

  // Check for critical configurations: 2 reflections
  //  Note that the reflections for the C1 and C2 cases
  //  are covered by the rotation cases above (except
  //  in the case of FullInvariance == false.

  for ( unsigned int i = 0; i < 2; i++ )
    {
    for ( unsigned int j = 0; j < 9; j++ )
      {
      neighborhoodPixels[j] =
        ( It.GetPixel( this->m_ReflectionIndices[i][j] ) != AlivePoint );
      if( this->m_ReflectionIndices[i][j] == 4 )
        {
        neighborhoodPixels[j] = !neighborhoodPixels[j];
        }
      }
//    if( !this->m_FullInvariance
//      && ( this->IsCriticalC1Configuration2D( neighborhoodPixels )
//        || this->IsCriticalC2Configuration2D( neighborhoodPixels ) ) )
//      {
//      return false;
//      }
    if( this->IsCriticalC3Configuration2D( neighborhoodPixels )
      || this->IsCriticalC4Configuration2D( neighborhoodPixels ) )
      {
      return false;
      }
    }
  return true;
}

template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::IsCriticalC1Configuration2D( Array<short> neighborhood )
{
  return ( !neighborhood[0] &&  neighborhood[1] &&
            neighborhood[3] && !neighborhood[4] &&
           !neighborhood[8] );
}

template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::IsCriticalC2Configuration2D( Array<short> neighborhood )
{
  return ( !neighborhood[0] &&  neighborhood[1] &&
            neighborhood[3] && !neighborhood[4] &&
            neighborhood[8] &&
           ( neighborhood[5] || neighborhood[7] ) );
}

template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::IsCriticalC3Configuration2D( Array<short> neighborhood )
{
  return ( !neighborhood[0] &&  neighborhood[1] &&
            neighborhood[3] && !neighborhood[4] &&
           !neighborhood[5] &&  neighborhood[6] &&
           !neighborhood[7] &&  neighborhood[8] );
}

template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::IsCriticalC4Configuration2D( Array<short> neighborhood )
{
  return ( !neighborhood[0] &&  neighborhood[1] &&
            neighborhood[3] && !neighborhood[4] &&
           !neighborhood[5] && !neighborhood[6] &&
           !neighborhood[7] &&  neighborhood[8] );
}

template <class TLevelSet, class TSpeedImage>
void
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::InitializeIndices2D()
{
  this->m_RotationIndices[0].SetSize( 9 );
  this->m_RotationIndices[1].SetSize( 9 );
  this->m_RotationIndices[2].SetSize( 9 );
  this->m_RotationIndices[3].SetSize( 9 );

  this->m_RotationIndices[0][0] = 0;
  this->m_RotationIndices[0][1] = 1;
  this->m_RotationIndices[0][2] = 2;
  this->m_RotationIndices[0][3] = 3;
  this->m_RotationIndices[0][4] = 4;
  this->m_RotationIndices[0][5] = 5;
  this->m_RotationIndices[0][6] = 6;
  this->m_RotationIndices[0][7] = 7;
  this->m_RotationIndices[0][8] = 8;

  this->m_RotationIndices[1][0] = 2;
  this->m_RotationIndices[1][1] = 5;
  this->m_RotationIndices[1][2] = 8;
  this->m_RotationIndices[1][3] = 1;
  this->m_RotationIndices[1][4] = 4;
  this->m_RotationIndices[1][5] = 7;
  this->m_RotationIndices[1][6] = 0;
  this->m_RotationIndices[1][7] = 3;
  this->m_RotationIndices[1][8] = 6;

  this->m_RotationIndices[2][0] = 8;
  this->m_RotationIndices[2][1] = 7;
  this->m_RotationIndices[2][2] = 6;
  this->m_RotationIndices[2][3] = 5;
  this->m_RotationIndices[2][4] = 4;
  this->m_RotationIndices[2][5] = 3;
  this->m_RotationIndices[2][6] = 2;
  this->m_RotationIndices[2][7] = 1;
  this->m_RotationIndices[2][8] = 0;

  this->m_RotationIndices[3][0] = 6;
  this->m_RotationIndices[3][1] = 3;
  this->m_RotationIndices[3][2] = 0;
  this->m_RotationIndices[3][3] = 7;
  this->m_RotationIndices[3][4] = 4;
  this->m_RotationIndices[3][5] = 1;
  this->m_RotationIndices[3][6] = 8;
  this->m_RotationIndices[3][7] = 5;
  this->m_RotationIndices[3][8] = 2;

  this->m_ReflectionIndices[0].SetSize( 9 );
  this->m_ReflectionIndices[1].SetSize( 9 );

  this->m_ReflectionIndices[0][0] = 6;
  this->m_ReflectionIndices[0][1] = 7;
  this->m_ReflectionIndices[0][2] = 8;
  this->m_ReflectionIndices[0][3] = 3;
  this->m_ReflectionIndices[0][4] = 4;
  this->m_ReflectionIndices[0][5] = 5;
  this->m_ReflectionIndices[0][6] = 0;
  this->m_ReflectionIndices[0][7] = 1;
  this->m_ReflectionIndices[0][8] = 2;

  this->m_ReflectionIndices[1][0] = 2;
  this->m_ReflectionIndices[1][1] = 1;
  this->m_ReflectionIndices[1][2] = 0;
  this->m_ReflectionIndices[1][3] = 5;
  this->m_ReflectionIndices[1][4] = 4;
  this->m_ReflectionIndices[1][5] = 3;
  this->m_ReflectionIndices[1][6] = 8;
  this->m_ReflectionIndices[1][7] = 7;
  this->m_ReflectionIndices[1][8] = 6;
}

template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::IsChangeWellComposed3D( IndexType idx )
{
  Array<short> neighborhoodPixels( 8 );

  typename NeighborhoodIteratorType::RadiusType radius;
  radius.Fill( 1 );
  NeighborhoodIteratorType It( radius, this->m_LabelImage,
    this->m_LabelImage->GetRequestedRegion() );
  It.SetLocation( idx );

  // Check for C1 critical configurations
  for ( unsigned int i = 0; i < 12; i++ )
    {
    for ( unsigned int j = 0; j < 4; j++ )
      {
      neighborhoodPixels[j]
        = ( It.GetPixel( m_C1Indices[i][j] ) == AlivePoint );
      if( m_C1Indices[i][j] == 13 )
        {
        neighborhoodPixels[j] = !neighborhoodPixels[j];
        }
      }
    if( this->IsCriticalC1Configuration3D( neighborhoodPixels ) )
      {
      return false;
      }
    }

  // Check for C2 critical configurations
  for ( unsigned int i = 0; i < 8; i++ )
    {
    for ( unsigned int j = 0; j < 8; j++ )
      {
      neighborhoodPixels[j]
        = ( It.GetPixel( m_C2Indices[i][j] ) == AlivePoint );
      if( m_C2Indices[i][j] == 13 )
        {
        neighborhoodPixels[j] = !neighborhoodPixels[j];
        }
      }
    if( IsCriticalC2Configuration3D( neighborhoodPixels ) )
      {
      return false;
      }
    }

  return true;
}

template <class TLevelSet, class TSpeedImage>
bool
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::IsCriticalC1Configuration3D( Array<short> neighborhood )
{
  return ( (  neighborhood[0] &&  neighborhood[1] &&
             !neighborhood[2] && !neighborhood[3] ) ||
           ( !neighborhood[0] && !neighborhood[1] &&
              neighborhood[2] &&  neighborhood[3] ) );
}

template <class TLevelSet, class TSpeedImage>
unsigned int
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::IsCriticalC2Configuration3D( Array<short> neighborhood )
{
  // Check if Type 1 or Type 2
  for ( unsigned int i = 0; i < 4; i++ )
    {
    bool isC2 = false;
    if( neighborhood[2*i] == neighborhood[2*i+1] )
      {
      isC2 = true;
      for ( unsigned int j = 0; j < 8; j++ )
        {
        if( neighborhood[j] == neighborhood[2*i] &&
               j != 2*i && j != 2*i+1 )
          {
          isC2 = false;
          }
        }
      }
    if( isC2 )
      {
      if( neighborhood[2*i] )
        {
        return 1;
        }
      else
        {
        return 2;
        }
      }
    }

  return 0;
}

template <class TLevelSet, class TSpeedImage>
void
FastMarchingImageFilter<TLevelSet,TSpeedImage>
::InitializeIndices3D()
{
  for ( unsigned int i = 0; i <  12; i++ )
    {
    this->m_C1Indices[i].SetSize( 4 );
    }
  for ( unsigned int i = 0; i <  8; i++ )
    {
    this->m_C2Indices[i].SetSize( 8 );
    }

  this->m_C1Indices[0][0] = 1;
  this->m_C1Indices[0][1] = 13;
  this->m_C1Indices[0][2] = 4;
  this->m_C1Indices[0][3] = 10;

  this->m_C1Indices[1][0] = 9;
  this->m_C1Indices[1][1] = 13;
  this->m_C1Indices[1][2] = 10;
  this->m_C1Indices[1][3] = 12;

  this->m_C1Indices[2][0] = 3;
  this->m_C1Indices[2][1] = 13;
  this->m_C1Indices[2][2] = 4;
  this->m_C1Indices[2][3] = 12;

  this->m_C1Indices[3][0] = 4;
  this->m_C1Indices[3][1] = 14;
  this->m_C1Indices[3][2] = 5;
  this->m_C1Indices[3][3] = 13;

  this->m_C1Indices[4][0] = 12;
  this->m_C1Indices[4][1] = 22;
  this->m_C1Indices[4][2] = 13;
  this->m_C1Indices[4][3] = 21;

  this->m_C1Indices[5][0] = 13;
  this->m_C1Indices[5][1] = 23;
  this->m_C1Indices[5][2] = 14;
  this->m_C1Indices[5][3] = 22;

  this->m_C1Indices[6][0] = 4;
  this->m_C1Indices[6][1] = 16;
  this->m_C1Indices[6][2] = 7;
  this->m_C1Indices[6][3] = 13;

  this->m_C1Indices[7][0] = 13;
  this->m_C1Indices[7][1] = 25;
  this->m_C1Indices[7][2] = 16;
  this->m_C1Indices[7][3] = 22;

  this->m_C1Indices[8][0] = 10;
  this->m_C1Indices[8][1] = 22;
  this->m_C1Indices[8][2] = 13;
  this->m_C1Indices[8][3] = 19;

  this->m_C1Indices[9][0] = 12;
  this->m_C1Indices[9][1] = 16;
  this->m_C1Indices[9][2] = 13;
  this->m_C1Indices[9][3] = 15;

  this->m_C1Indices[10][0] = 13;
  this->m_C1Indices[10][1] = 17;
  this->m_C1Indices[10][2] = 14;
  this->m_C1Indices[10][3] = 16;

  this->m_C1Indices[11][0] = 10;
  this->m_C1Indices[11][1] = 14;
  this->m_C1Indices[11][2] = 11;
  this->m_C1Indices[11][3] = 13;

  this->m_C2Indices[0][0] = 0;
  this->m_C2Indices[0][1] = 13;
  this->m_C2Indices[0][2] = 1;
  this->m_C2Indices[0][3] = 12;
  this->m_C2Indices[0][4] = 3;
  this->m_C2Indices[0][5] = 10;
  this->m_C2Indices[0][6] = 4;
  this->m_C2Indices[0][7] = 9;

  this->m_C2Indices[4][0] = 9;
  this->m_C2Indices[4][1] = 22;
  this->m_C2Indices[4][2] = 10;
  this->m_C2Indices[4][3] = 21;
  this->m_C2Indices[4][4] = 12;
  this->m_C2Indices[4][5] = 19;
  this->m_C2Indices[4][6] = 13;
  this->m_C2Indices[4][7] = 18;

  for ( unsigned int i = 1; i < 4; i++ )
    {
    int addend;
    if( i == 2 )
      {
      addend = 2;
      }
    else
      {
      addend = 1;
      }
    for ( unsigned int j = 0; j < 8; j++ )
      {
      this->m_C2Indices[i  ][j] = this->m_C2Indices[i-1][j] + addend;
      this->m_C2Indices[i+4][j] = this->m_C2Indices[i+3][j] + addend;
      }
    }
}

} // namespace itk


#endif
