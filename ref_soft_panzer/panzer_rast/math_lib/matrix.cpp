/*
*This file is part of FREG.
*
*FREG is free software: you can redistribute it and/or modify
*it under the terms of the GNU General Public License as published by
*the Free Software Foundation, either version 3 of the License, or
*(at your option) any later version.
*
*FREG is distributed in the hope that it will be useful,
*but WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*GNU General Public License for more details.
*
*You should have received a copy of the GNU General Public License
*along with FREG. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MATRIX_CPP
#define MATRIX_CPP

#include "matrix.h"
#include "m_math.h"

//for mmx, sse, sse2
//#include <mmintrin.h>
//#include <xmmintrin.h>
//#include <emmintrin.h>
//#include <mmintrin.h>


float m_Mat4::Vec3MatMulW( const m_Vec3& v )const
{
	return v.x * value[3] + v.y * value[7] + v.z * value[11] + value[15];
}

void m_Mat4::Transpose()
{
    float tmp;

    tmp= value[1];
    value[1]= value[4];
    value[4]= tmp;

    tmp= value[2];
    value[2]= value[8];
    value[8]= tmp;

    tmp= value[3];
    value[3]= value[12];
    value[12]= tmp;

    tmp= value[6];
    value[6]= value[9];
    value[9]= tmp;

    tmp= value[7];
    value[7]= value[13];
    value[13]= tmp;

    tmp= value[11];
    value[11]= value[14];
    value[14]= tmp;

}
m_Mat4 m_Mat4::operator*( const m_Mat4& m )
{
    m_Mat4 r;

    r.value[0 ]= this->value[0]*m.value[0] + this->value[1]*m.value[4] + this->value[2]*m.value[8]  + this->value[3]*m.value[12];
    r.value[1 ]= this->value[0]*m.value[1] + this->value[1]*m.value[5] + this->value[2]*m.value[9]  + this->value[3]*m.value[13];
    r.value[2 ]= this->value[0]*m.value[2] + this->value[1]*m.value[6] + this->value[2]*m.value[10] + this->value[3]*m.value[14];
    r.value[3 ]= this->value[0]*m.value[3] + this->value[1]*m.value[7] + this->value[2]*m.value[11] + this->value[3]*m.value[15];

    r.value[4 ]= this->value[4]*m.value[0] + this->value[5]*m.value[4] + this->value[6]*m.value[8]  + this->value[7]*m.value[12];
    r.value[5 ]= this->value[4]*m.value[1] + this->value[5]*m.value[5] + this->value[6]*m.value[9]  + this->value[7]*m.value[13];
    r.value[6 ]= this->value[4]*m.value[2] + this->value[5]*m.value[6] + this->value[6]*m.value[10] + this->value[7]*m.value[14];
    r.value[7 ]= this->value[4]*m.value[3] + this->value[5]*m.value[7] + this->value[6]*m.value[11] + this->value[7]*m.value[15];

    r.value[8 ]= this->value[8]*m.value[0] + this->value[9]*m.value[4] + this->value[10]*m.value[8]  + this->value[11]*m.value[12];
    r.value[9 ]= this->value[8]*m.value[1] + this->value[9]*m.value[5] + this->value[10]*m.value[9]  + this->value[11]*m.value[13];
    r.value[10]= this->value[8]*m.value[2] + this->value[9]*m.value[6] + this->value[10]*m.value[10] + this->value[11]*m.value[14];
    r.value[11]= this->value[8]*m.value[3] + this->value[9]*m.value[7] + this->value[10]*m.value[11] + this->value[11]*m.value[15];

    r.value[12]= this->value[12]*m.value[0] + this->value[13]*m.value[4] + this->value[14]*m.value[8]  + this->value[15]*m.value[12];
    r.value[13]= this->value[12]*m.value[1] + this->value[13]*m.value[5] + this->value[14]*m.value[9]  + this->value[15]*m.value[13];
    r.value[14]= this->value[12]*m.value[2] + this->value[13]*m.value[6] + this->value[14]*m.value[10] + this->value[15]*m.value[14];
    r.value[15]= this->value[12]*m.value[3] + this->value[13]*m.value[7] + this->value[14]*m.value[11] + this->value[15]*m.value[15];
    return r;
}


m_Mat4& m_Mat4::operator*=( const m_Mat4& m )
{
    m_Mat4 r;

    r.value[0 ]= this->value[0]*m.value[0] + this->value[1]*m.value[4] + this->value[2]*m.value[8]  + this->value[3]*m.value[12];
    r.value[1 ]= this->value[0]*m.value[1] + this->value[1]*m.value[5] + this->value[2]*m.value[9]  + this->value[3]*m.value[13];
    r.value[2 ]= this->value[0]*m.value[2] + this->value[1]*m.value[6] + this->value[2]*m.value[10] + this->value[3]*m.value[14];
    r.value[3 ]= this->value[0]*m.value[3] + this->value[1]*m.value[7] + this->value[2]*m.value[11] + this->value[3]*m.value[15];

    r.value[4 ]= this->value[4]*m.value[0] + this->value[5]*m.value[4] + this->value[6]*m.value[8]  + this->value[7]*m.value[12];
    r.value[5 ]= this->value[4]*m.value[1] + this->value[5]*m.value[5] + this->value[6]*m.value[9]  + this->value[7]*m.value[13];
    r.value[6 ]= this->value[4]*m.value[2] + this->value[5]*m.value[6] + this->value[6]*m.value[10] + this->value[7]*m.value[14];
    r.value[7 ]= this->value[4]*m.value[3] + this->value[5]*m.value[7] + this->value[6]*m.value[11] + this->value[7]*m.value[15];

    r.value[8 ]= this->value[8]*m.value[0] + this->value[9]*m.value[4] + this->value[10]*m.value[8]  + this->value[11]*m.value[12];
    r.value[9 ]= this->value[8]*m.value[1] + this->value[9]*m.value[5] + this->value[10]*m.value[9]  + this->value[11]*m.value[13];
    r.value[10]= this->value[8]*m.value[2] + this->value[9]*m.value[6] + this->value[10]*m.value[10] + this->value[11]*m.value[14];
    r.value[11]= this->value[8]*m.value[3] + this->value[9]*m.value[7] + this->value[10]*m.value[11] + this->value[11]*m.value[15];

    r.value[12]= this->value[12]*m.value[0] + this->value[13]*m.value[4] + this->value[14]*m.value[8]  + this->value[15]*m.value[12];
    r.value[13]= this->value[12]*m.value[1] + this->value[13]*m.value[5] + this->value[14]*m.value[9]  + this->value[15]*m.value[13];
    r.value[14]= this->value[12]*m.value[2] + this->value[13]*m.value[6] + this->value[14]*m.value[10] + this->value[15]*m.value[14];
    r.value[15]= this->value[12]*m.value[3] + this->value[13]*m.value[7] + this->value[14]*m.value[11] + this->value[15]*m.value[15];

    *this= r;
    return *this;
}


m_Vec3	m_Mat4::operator*( const m_Vec3& v )
{
	/*float* vp= &v.x;
	float* mp= this->value;
	asm( "mulps %%xmm1, %%xmm0"
		: //output
		 : "m"(vp), "m"(mp)//input
		 : "%eax"//registers
		 );*/

    m_Vec3 r;
    r.x= value[0] * v.x + value[1] * v.y + value[2] * v.z + value[3];
    r.y= value[4] * v.x + value[5] * v.y + value[6] * v.z + value[7];
    r.z= value[8] * v.x + value[9] * v.y + value[10]* v.z + value[11];
    return r;
}

m_Vec3 operator*( const m_Vec3& v, const m_Mat4& m )
{
    m_Vec3 r;
    r.x= v.x * m.value[0] + v.y * m.value[4] + v.z * m.value[8] + m.value[12];
    r.y= v.x * m.value[1] + v.y * m.value[5] + v.z * m.value[9] + m.value[13];
    r.z= v.x * m.value[2] + v.y * m.value[6] + v.z * m.value[10]+ m.value[14];
    return r;
}

float& m_Mat4::operator[]( int i )
{
    return value[i];
}
float m_Mat4::operator[]( int i )const
{
    return value[i];
}

void m_Mat4::Translate( const m_Vec3& v )
{
    value[12]+= v.x;
    value[13]+= v.y;
    value[14]+= v.z;
}
void m_Mat4::Scale( const m_Vec3& v )
{
    value[0 ]*= v.x;
    value[5 ]*= v.y;
    value[10]*= v.z;
}

void m_Mat4::Scale( float s )
{
    value[0 ]*= s;
    value[5 ]*= s;
    value[10]*= s;
}

void m_Mat4::Identity()
{
    value[0]= value[5]= value[10]= value[15]= 1.0f;

    value[1] = value[2] = value[3] = 0.0f;
    value[4] = value[6] = value[7] = 0.0f;
    value[8] = value[9] = value[11]= 0.0f;
    value[12]= value[13]= value[14]= 0.0f;
}

void m_Mat4::MakePerspective( float aspect, float fov_y, float z_near, float z_far)
{
    float f= 1.0f / m_Math::Tan( fov_y * 0.5f );

    value[0]= f / aspect;
    value[5]= f;

    f= 1.0f / ( z_far - z_near );
    value[14]= -2.0f * f * z_near * z_far;
    value[10]= ( z_near + z_far ) * f;
    value[11]= 1.0f;

    value[1]= value[2]= value[3]= 0.0f;
    value[4]= value[6]= value[7]= 0.0f;
    value[8]= value[9]= 0.0f;
    value[12]= value[13]= value[15]= 0.0f;
}

void m_Mat4::MakeProjection( float scale_x, float scale_y, float z_near, float z_far )
{
    value[0]= scale_x;
    value[5]= scale_y;
    value[10]= 2.0f / ( z_far - z_near );
    value[14]= 1.0f - value[10] * z_far;

    value[1 ]= value[2 ]= value[3 ]= 0.0f;
    value[4 ]= value[6 ]= value[7 ]= 0.0f;
    value[8 ]= value[9 ]= value[11]= 0.0f;
    value[12]= value[13]= value[15]= 0.0f;
    value[15]= 1.0f;

}
void m_Mat4::RotateX( float a )
{
    /*float s, c;
    m_Math::SinCos( a, &s, &c );

    value[5 ]= c;
    value[9 ]= -s;
    value[6 ]= s;
    value[10]= c;

    value[0]= value[15]= 1.0f;
    value[1]= value[2]= value[3]= 0.0f;
    value[4]= value[7]= 0.0f;
    value[8]= value[11]= 0.0f;
    value[12]= value[13]= value[14]= 0.0f;*/

	Identity();
	value[10]= cos(a);
	value[6]= -sin(a);
	value[9]= sin(a);
	value[5]= cos(a);
}

void m_Mat4::RotateY( float a )
{
    float s, c;
    m_Math::SinCos( a, &s, &c );

    value[0]= c;
    value[8]= s;
    value[2]= -s;
    value[10]= c;

    value[5]= value[15]= 1.0f;
    value[1]= value[3]= 0.0f;
    value[4]= value[6]= value[7]= 0.0f;
    value[9]= value[11]= 0.0f;
    value[12]= value[13]= value[14]= 0.0f;
}

void m_Mat4::RotateZ( float a )
{
  /*  float s, c;
    m_Math::SinCos( a, &s, &c );

    value[0]= c;
    value[4]= -s;
    value[1]= s;
    value[5]= c;

    value[10]= value[15]= 1.0f;
    value[2]= value[3]= 0.0f;
    value[6]= value[7]= 0.0f;
    value[8]= value[9]= value[11]= 0.0f;
    value[12]= value[13]= value[14]= 0.0f;*/

	Identity();
	value[5]= cos(a);
	value[1]= -sin(a);
	value[0]= cos(a);
	value[4]= sin(a);
}










m_Mat3::m_Mat3(const  m_Mat4& m )
{
	value[0]= m.value[0];
	value[1]= m.value[1];
	value[2]= m.value[2];

	value[3]= m.value[4];
	value[4]= m.value[5];
	value[5]= m.value[6];

	value[6]= m.value[8];
	value[7]= m.value[9];
	value[8]= m.value[10];
}



void m_Mat3::Transpose()
{
    float tmp;

    tmp= value[3];
    value[3] = value[1];
    value[1]= tmp;

    tmp= value[6];
    value[6] = value[2];
    value[2]= tmp;

    tmp= value[7];
    value[7] = value[5];
    value[5]= tmp;

}

m_Mat3 m_Mat3::operator*( const m_Mat3& m )
{
    m_Mat3 r;

    r.value[0]= this->value[0] * m.value[0] + this->value[1] * m.value[3] + this->value[2] * m.value[6];
    r.value[1]= this->value[0] * m.value[1] + this->value[1] * m.value[4] + this->value[2] * m.value[7];
    r.value[2]= this->value[0] * m.value[2] + this->value[1] * m.value[5] + this->value[2] * m.value[8];

    r.value[3]= this->value[3] * m.value[0] + this->value[4] * m.value[3] + this->value[5] * m.value[6];
    r.value[4]= this->value[3] * m.value[1] + this->value[4] * m.value[4] + this->value[5] * m.value[7];
    r.value[5]= this->value[3] * m.value[2] + this->value[4] * m.value[5] + this->value[5] * m.value[8];

    r.value[6]= this->value[6] * m.value[0] + this->value[7] * m.value[3] + this->value[8] * m.value[6];
    r.value[7]= this->value[6] * m.value[1] + this->value[7] * m.value[4] + this->value[8] * m.value[7];
    r.value[8]= this->value[6] * m.value[2] + this->value[7] * m.value[5] + this->value[8] * m.value[8];

    return r;
}

m_Mat3& m_Mat3::operator*=( const m_Mat3& m )
{
    m_Mat3 r;

    r.value[0]= this->value[0] * m.value[0] + this->value[1] * m.value[3] + this->value[2] * m.value[6];
    r.value[1]= this->value[0] * m.value[1] + this->value[1] * m.value[4] + this->value[2] * m.value[7];
    r.value[2]= this->value[0] * m.value[2] + this->value[1] * m.value[5] + this->value[2] * m.value[8];

    r.value[3]= this->value[3] * m.value[0] + this->value[4] * m.value[3] + this->value[5] * m.value[6];
    r.value[4]= this->value[3] * m.value[1] + this->value[4] * m.value[4] + this->value[5] * m.value[7];
    r.value[5]= this->value[3] * m.value[2] + this->value[4] * m.value[5] + this->value[5] * m.value[8];

    r.value[6]= this->value[6] * m.value[0] + this->value[7] * m.value[3] + this->value[8] * m.value[6];
    r.value[7]= this->value[6] * m.value[1] + this->value[7] * m.value[4] + this->value[8] * m.value[7];
    r.value[8]= this->value[6] * m.value[2] + this->value[7] * m.value[5] + this->value[8] * m.value[8];

    *this= r;
    return *this;
}

m_Vec3	m_Mat3::operator*( const m_Vec3& v )
{
    m_Vec3 r;
    r.x= value[0] * v.x + value[1] * v.y + value[2] * v.z;
    r.y= value[3] * v.x + value[4] * v.y + value[5] * v.z;
    r.z= value[6] * v.x + value[7] * v.y + value[8] * v.z;
    return r;
}

float& m_Mat3::operator[]( int i )
{
    return value[i];
}
float m_Mat3::operator[]( int i )const
{
    return value[i];
}


void m_Mat3::Scale( const m_Vec3& v )
{
    value[0]*= v.x;
    value[4]*= v.y;
    value[8]*= v.z;
}

void m_Mat3::Scale( float s )
{
    value[0]*= s;
    value[4]*= s;
    value[8]*= s;
}

void m_Mat3::Identity()
{
    value[0]= value[4]= value[8]= 1.0f;

    value[1] = value[2] = value[3] =
                              value[5] = value[6] = value[7] = 0.0f;
}


void m_Mat3::RotateX( float a )
{
    float s, c;
    m_Math::SinCos( a, &s, &c );

    value[4]= c;
    value[7]= -s;
    value[5]= s;
    value[8]= c;

    value[0]= 0.0f;
    value[1]= value[2]= value[3]= value[6]= 0.0f;
}

void m_Mat3::RotateY( float a )
{
    float s, c;
    m_Math::SinCos( a, &s, &c );

    value[0]= c;
    value[6]= s;
    value[2]= -s;
    value[8]= c;

    value[4]= 1.0f;
    value[1]= value[3]= value[5]= value[7]= 0.0f;
}

void m_Mat3::RotateZ( float a )
{
    float s, c;
    m_Math::SinCos( a, &s, &c );

    value[0]= c;
    value[3]= -s;
    value[1]= s;
    value[4]= c;

    value[8]= 1.0f;
    value[2]= value[5]= value[6]= value[7]= 0.0f;
}


float m_Mat3::Determinant()
{
    return value[0] * ( value[4] * value[8] - value[7] * value[5] ) -
           value[1] * ( value[3] * value[8] - value[6] * value[5] ) +
           value[2] * ( value[3] * value[7] - value[6] * value[4] );
}

void m_Mat3::Inverse()
{
    m_Mat3 r;

    float det= Determinant();
    if( det == 0.0f )
        return;

    det= 1.0f / det;

    r.value[0]= ( value[4] * value[8] - value[7] * value[5] ) * det;
    r.value[1]= ( value[2] * value[7] - value[1] * value[8] ) * det;
    r.value[2]= ( value[1] * value[5] - value[2] * value[4] ) * det;

    r.value[3]= ( value[5] * value[6] - value[3] * value[8] ) * det;
    r.value[4]= ( value[0] * value[8] - value[2] * value[6] ) * det;
    r.value[5]= ( value[2] * value[3] - value[0] * value[5] ) * det;

    r.value[6]= ( value[3] * value[7] - value[4] * value[6] ) * det;
    r.value[7]= ( value[1] * value[6] - value[0] * value[7] ) * det;
    r.value[8]= ( value[0] * value[4] - value[1] * value[3] ) * det;

   *this= r;
}

m_Vec3 operator*( const m_Vec3& v, const m_Mat3& m )
{
    m_Vec3 r;
    r.x= v.x * m.value[0] + v.y * m.value[3] + v.z * m.value[6];
    r.y= v.x * m.value[1] + v.y * m.value[4] + v.z * m.value[7];
    r.z= v.x * m.value[2] + v.y * m.value[5] + v.z * m.value[8];
    return r;
}
#endif//_MATRIX_CPP_
