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
#ifndef VEC_H
#define VEC_H

#include "m_math.h"

class m_Vec2
{
	public:

	float x, y;

	m_Vec2(){}
	~m_Vec2(){}
	m_Vec2( float a, float b );

	float	Length()	const;
	float	LengthSqr() const;
	float	InvLength() const;
	float	InvLengthSqr() const;
	float 	MaxComponent() const;
	m_Vec2&	Normalize();
	float*	ToArr();

	m_Vec2 operator-() const;

	m_Vec2  operator+ ( const m_Vec2& v ) const;
	m_Vec2& operator+=( const m_Vec2& v );

	m_Vec2  operator- ( const m_Vec2& v ) const;
	m_Vec2& operator-=( const m_Vec2& v );

	float operator* ( const m_Vec2& v ) const;//скалярное умножение векторов

	m_Vec2& operator*=( const m_Vec2& v);//покомпонентное умножение векторов

	m_Vec2  operator*( float a ) const;//умножение вектора на число
	m_Vec2& operator*=( float a );//умножение вектора на число
	m_Vec2& operator/=( float a );//деление   вектора на число

	friend m_Vec2 operator*( float a, const m_Vec2& v );

};

class m_Vec3
{

public:
	float x, y, z;

	m_Vec3(){}
	~m_Vec3(){}
	m_Vec3( float a, float b, float c );

	float	Length()	const;
	float	LengthSqr() const;
	float	InvLength() const;
	float	InvLengthSqr() const;
	float 	MaxComponent() const;
	m_Vec3&	Normalize();
	float*	ToArr();

	m_Vec3 operator-() const;

	m_Vec3  operator+ ( const m_Vec3& v ) const;
	m_Vec3& operator+=( const m_Vec3& v );

	m_Vec3  operator- ( const m_Vec3& v ) const;
	m_Vec3& operator-=( const m_Vec3& v );

	float operator* ( const m_Vec3& v ) const;//скалярное умножение векторов

	m_Vec3& operator*=( const m_Vec3& v);//покомпонентное умножение векторов

	m_Vec3  operator*( float a) const;//умножение вектора на число
	m_Vec3 operator/( float a ) const;
	m_Vec3& operator*=( float a );//умножение вектора на число
	m_Vec3& operator/=( float a );//деление   вектора на число

	friend m_Vec3 operator*( float a, const m_Vec3& v );


	m_Vec2  xy() const;
	m_Vec2  xz() const;
	m_Vec2  yz() const;
};

m_Vec3 mVec3Cross( m_Vec3& v1, m_Vec3& v2 );


/*
VEC2
*/

inline m_Vec2::m_Vec2( float a, float b ) :
x(a), y(b) {}

inline m_Vec2  m_Vec2::operator+ ( const m_Vec2& v ) const
{
	return m_Vec2( this->x + v.x, this->y + v.y );
}
inline m_Vec2& m_Vec2::operator+=( const m_Vec2& v )
{
	this->x+= v.x;
	this->y+= v.y;
	return *this;
}

inline m_Vec2  m_Vec2::operator- ( const m_Vec2& v ) const
{
	return m_Vec2( this->x - v.x, this->y - v.y );
}
inline m_Vec2& m_Vec2::operator-=( const m_Vec2& v )
{
	this->x-= v.x;
	this->y-= v.y;
	return *this;
}

inline float m_Vec2::operator* ( const m_Vec2& v ) const
{
 return this->x * v.x + this->y * v.y;
}
inline m_Vec2& m_Vec2::operator*=( float a )
{
	x*= a;
	y*= a;
	return *this;
}

inline m_Vec2& m_Vec2::operator/=( float a )
{
	float r= 1.0f / a;
	x*= r;
	y*= r;
	return *this;
}
inline m_Vec2 m_Vec2::operator-() const
{
	return m_Vec2( -x, -y );
}

inline float m_Vec2::MaxComponent() const
{
	return 		x > y ? x : y;
}

inline m_Vec2  m_Vec2::operator*( float a ) const
{
	return m_Vec2( x * a, y * a );
}

inline m_Vec2& m_Vec2::operator*=( const m_Vec2& v )
{
	this->x*= v.x;
	this->y*= v.y;
	return *this;
}

inline m_Vec2& m_Vec2::Normalize()
{
	float r= m_Math::Sqrt( x * x + y * y );
	if( r != 0.0f )
		r= 1.0f / r;
	x*= r;
	y*= r;
	return *this;
}

inline float* m_Vec2::ToArr()
{
	return &x;
}

inline float m_Vec2::Length() const
{
	return m_Math::Sqrt( x * x + y * y );

}
inline float m_Vec2::LengthSqr() const
{
	return  x * x + y * y;

}

inline float m_Vec2::InvLength() const
{
	return 1.0f / m_Math::Sqrt( x * x + y * y  );
}

inline float m_Vec2::InvLengthSqr() const
{
	return 1.0f / ( x * x + y * y );
}


inline m_Vec2 operator*( float a, const m_Vec2& v )
{
	return m_Vec2( a * v.x, a * v.y );
}


/*
VEC3
*/

inline m_Vec3::m_Vec3( float a, float b, float c) :
x(a), y(b), z(c) {}
inline m_Vec3  m_Vec3::operator+ ( const m_Vec3& v ) const
{
	return m_Vec3( this->x + v.x, this->y + v.y, this->z + v.z );
}
inline m_Vec3& m_Vec3::operator+=( const m_Vec3& v )
{
	this->x+= v.x;
	this->y+= v.y;
	this->z+= v.z;
	return *this;
}

inline m_Vec3  m_Vec3::operator- ( const m_Vec3& v ) const
{
	return m_Vec3( this->x - v.x, this->y - v.y, this->z - v.z );
}
inline m_Vec3& m_Vec3::operator-=( const m_Vec3& v )
{
	this->x-= v.x;
	this->y-= v.y;
	this->z-= v.z;
	return *this;
}

inline float m_Vec3::operator* ( const m_Vec3& v ) const
{
 return this->x * v.x + this->y * v.y + this->z * v.z;
}
inline m_Vec3& m_Vec3::operator*=( float a )
{
	x*= a;
	y*= a;
	z*= a;
	return *this;
}

inline m_Vec3& m_Vec3::operator/=( float a )
{
	float r= 1.0f / a;
	x*= r;
	y*= r;
	z*= r;
	return *this;
}
inline m_Vec3 m_Vec3::operator-() const
{
	return m_Vec3( -x, -y, -z );
}

inline float m_Vec3::MaxComponent() const
{
	float m= 	x > y ? x : y;
	return 		m > z ? m : z;
}

inline m_Vec3  m_Vec3::operator*( float a) const
{
	return m_Vec3( x * a, y * a, z * a );
}

inline m_Vec3 m_Vec3::operator/( float a ) const
{
	float inv_a= 1.0f / a;
	return m_Vec3( x * inv_a, y * inv_a, z * inv_a );
}

inline m_Vec3& m_Vec3::operator*=( const m_Vec3& v)
{
	this->x*= v.x;
	this->y*= v.y;
	this->z*= v.z;
	return *this;
}

inline m_Vec3& m_Vec3::Normalize()
{
	float r= m_Math::Sqrt( x * x + y * y + z * z );
	if( r != 0.0f )
		r= 1.0f / r;
	x*= r;
	y*= r;
	z*= r;
	return *this;
}

inline float* m_Vec3::ToArr()
{
	return &x;
}

inline float m_Vec3::Length() const
{
	return m_Math::Sqrt( x * x + y * y + z * z );

}
inline float m_Vec3::LengthSqr() const
{
	return  x * x + y * y + z * z;

}

inline float m_Vec3::InvLength() const
{
	return 1.0f / m_Math::Sqrt( x * x + y * y + z * z );
}

inline float m_Vec3::InvLengthSqr() const
{
	return 1.0f / ( x * x + y * y + z * z );
}


inline m_Vec3 operator*( float a, const m_Vec3& v )
{
	return m_Vec3( a * v.x, a * v.y, a * v.z );
}

inline m_Vec3 mVec3Cross( m_Vec3& v1, m_Vec3& v2 )
{
	m_Vec3 result;
	result.x= v1.y * v2.z - v1.z * v2.y;
	result.y= v1.z * v2.x - v1.x * v2.z;
	result.z= v1.x * v2.y - v1.y * v2.x;
	return result;
}

inline m_Vec2 m_Vec3::xy() const
{
	return m_Vec2( x, y );
}
inline m_Vec2 m_Vec3::xz() const{
	return m_Vec2( x, z );
}
inline m_Vec2 m_Vec3::yz() const{
	return m_Vec2( y, z );
}
#endif
