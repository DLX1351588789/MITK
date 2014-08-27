/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/
#include <vnl/vnl_cross.h>
#include <vnl/vnl_quaternion.h>
#include <mitkRawShModel.h>
#include <boost/math/special_functions.hpp>
#include <itkOrientationDistributionFunction.h>
#include <mitkFiberfoxParameters.h>

using namespace mitk;
using namespace boost::math;

template< class ScalarType >
RawShModel< ScalarType >::RawShModel()
    : m_ShOrder(0)
    , m_ModelIndex(-1)
    , m_MaxNumKernels(1000)
{
    m_RandGen = itk::Statistics::MersenneTwisterRandomVariateGenerator::New();
    m_RandGen->SetSeed();
    m_AdcRange.first = 0;
    m_AdcRange.second = 0.004;
    m_FaRange.first = 0;
    m_FaRange.second = 1;
}

template< class ScalarType >
RawShModel< ScalarType >::~RawShModel()
{

}

template< class ScalarType >
void RawShModel< ScalarType >::Clear()
{
    m_ShCoefficients.clear();
    m_PrototypeMaxDirection.clear();
    m_B0Signal.clear();
}

template< class ScalarType >
void RawShModel< ScalarType >::RandomModel()
{
    m_ModelIndex = m_RandGen->GetIntegerVariate(m_B0Signal.size()-1);
}

template< class ScalarType >
unsigned int RawShModel< ScalarType >::GetNumberOfKernels()
{
    return m_B0Signal.size();
}

// convert cartesian to spherical coordinates
template< class ScalarType >
void RawShModel< ScalarType >::Cart2Sph( GradientListType gradients )
{
    m_SphCoords.set_size(gradients.size(), 2);
    m_SphCoords.fill(0.0);

    for (unsigned int i=0; i<gradients.size(); i++)
    {
        GradientType g = gradients[i];
        if( g.GetNorm() > 0.0001 )
        {
            gradients[i].Normalize();
            m_SphCoords(i,0) = acos(gradients[i][2]); // theta
            m_SphCoords(i,1) = atan2(gradients[i][1], gradients[i][0]); // phi
        }
    }
}

template< class ScalarType >
void RawShModel< ScalarType >::SetFiberDirection(GradientType fiberDirection)
{
    this->m_FiberDirection = fiberDirection;
    this->m_FiberDirection.Normalize();

    RandomModel();
    GradientType axis = itk::CrossProduct(this->m_FiberDirection, m_PrototypeMaxDirection.at(m_ModelIndex));
    axis.Normalize();

    vnl_quaternion<double> rotation(axis.GetVnlVector(), acos(dot_product(this->m_FiberDirection.GetVnlVector(), m_PrototypeMaxDirection.at(m_ModelIndex).GetVnlVector())));
    rotation.normalize();

    GradientListType gradients;
    for (unsigned int i=0; i<this->m_GradientList.size(); i++)
    {
        GradientType dir = this->m_GradientList.at(i);
        if( dir.GetNorm() > 0.0001 )
        {
            dir.Normalize();
            vnl_vector_fixed< double, 3 > vnlDir = rotation.rotate(dir.GetVnlVector());
            dir[0] = vnlDir[0]; dir[1] = vnlDir[1]; dir[2] = vnlDir[2];
            dir.Normalize();
        }
        gradients.push_back(dir);
    }

    Cart2Sph( gradients );
}

template< class ScalarType >
bool RawShModel< ScalarType >::SetShCoefficients(vnl_vector< double > shCoefficients, ScalarType b0 )
{
    m_ShOrder = 2;
    while ( (m_ShOrder*m_ShOrder + m_ShOrder + 2)/2 + m_ShOrder <= shCoefficients.size() )
        m_ShOrder += 2;
    m_ShOrder -= 2;

    m_ModelIndex = m_B0Signal.size();
    m_B0Signal.push_back(b0);
    m_ShCoefficients.push_back(shCoefficients);

//    itk::OrientationDistributionFunction<double, 10000> odf;
//    GradientListType gradients;
//    for (unsigned int i=0; i<odf.GetNumberOfComponents(); i++)
//    {
//        GradientType dir; dir[0]=odf.GetDirection(i)[0]; dir[1]=odf.GetDirection(i)[1]; dir[2]=odf.GetDirection(i)[2];
//        dir.Normalize();
//        gradients.push_back(dir);
//    }
//    Cart2Sph( this->m_GradientList );
//    PixelType signal = SimulateMeasurement();

//    int minDirIdx = 0;
//    double min = itk::NumericTraits<double>::max();
//    for (unsigned int i=0; i<signal.GetSize(); i++)
//    {
//        if (signal[i]>b0 || signal[i]<0)
//        {
//            MITK_INFO << "Corrupted signal value detected. Kernel rejected.";
//            m_B0Signal.pop_back();
//            m_ShCoefficients.pop_back();
//            return false;
//        }
//        if (signal[i]<min)
//        {
//            min = signal[i];
//            minDirIdx = i;
//        }
//    }
//    GradientType maxDir = this->m_GradientList.at(minDirIdx);
//    maxDir.Normalize();
//    m_PrototypeMaxDirection.push_back(maxDir);

    Cart2Sph( this->m_GradientList );
    m_ModelIndex = -1;

    return true;
}

template< class ScalarType >
ScalarType RawShModel< ScalarType >::SimulateMeasurement(unsigned int dir)
{
    ScalarType signal = 0;

    if (m_ModelIndex==-1)
        RandomModel();

    if (dir>=this->m_GradientList.size())
        return signal;

    int j, m; double mag, plm;

    if (this->m_GradientList[dir].GetNorm()>0.001)
    {
        j=0;
        for (int l=0; l<=m_ShOrder; l=l+2)
            for (m=-l; m<=l; m++)
            {
                plm = legendre_p<double>(l,abs(m),cos(m_SphCoords(dir,0)));
                mag = sqrt((double)(2*l+1)/(4.0*M_PI)*factorial<double>(l-abs(m))/factorial<double>(l+abs(m)))*plm;
                double basis;

                if (m<0)
                    basis = sqrt(2.0)*mag*cos(fabs((double)m)*m_SphCoords(dir,1));
                else if (m==0)
                    basis = mag;
                else
                    basis = pow(-1.0, m)*sqrt(2.0)*mag*sin(m*m_SphCoords(dir,1));

                signal += m_ShCoefficients.at(m_ModelIndex)[j]*basis;
                j++;
            }
    }
    else
        signal = m_B0Signal.at(m_ModelIndex);

    m_ModelIndex = -1;

    return signal;
}

template< class ScalarType >
typename RawShModel< ScalarType >::PixelType RawShModel< ScalarType >::SimulateMeasurement()
{
    if (m_ModelIndex==-1)
        RandomModel();

    PixelType signal;
    signal.SetSize(this->m_GradientList.size());

    int M = this->m_GradientList.size();
    int j, m; double mag, plm;

    vnl_matrix< double > shBasis;
    shBasis.set_size(M, m_ShCoefficients.at(m_ModelIndex).size());
    shBasis.fill(0.0);

    for (int p=0; p<M; p++)
    {
        if (this->m_GradientList[p].GetNorm()>0.001)
        {
            j=0;
            for (int l=0; l<=m_ShOrder; l=l+2)
                for (m=-l; m<=l; m++)
                {
                    plm = legendre_p<double>(l,abs(m),cos(m_SphCoords(p,0)));
                    mag = sqrt((double)(2*l+1)/(4.0*M_PI)*factorial<double>(l-abs(m))/factorial<double>(l+abs(m)))*plm;

                    if (m<0)
                        shBasis(p,j) = sqrt(2.0)*mag*cos(fabs((double)m)*m_SphCoords(p,1));
                    else if (m==0)
                        shBasis(p,j) = mag;
                    else
                        shBasis(p,j) = pow(-1.0, m)*sqrt(2.0)*mag*sin(m*m_SphCoords(p,1));

                    j++;
                }


            double val = 0;
            for (unsigned int cidx=0; cidx<m_ShCoefficients.at(m_ModelIndex).size(); cidx++)
                val += m_ShCoefficients.at(m_ModelIndex)[cidx]*shBasis(p,cidx);
            signal[p] = val;
        }
        else
            signal[p] = m_B0Signal.at(m_ModelIndex);
    }

    m_ModelIndex = -1;

    return signal;
}