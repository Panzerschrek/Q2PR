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

#ifndef MATRIX_H
#define MATRIX_H

#include "vec.h"

class m_Mat3;

class m_Mat4
{
public:
    float value[16];

    m_Mat4	operator*( const m_Mat4& m );//умножение
    m_Mat4&	operator*=( const m_Mat4& m );//домножение
    m_Vec3	operator*( const m_Vec3& v );

    float&	operator[]( int i );
    float	operator[]( int i )const;//операция индексирования

    m_Mat4( m_Mat3& m );
    m_Mat4() {}
    ~m_Mat4() {}
    void	Transpose();

    void Translate( const m_Vec3& v);//домножение матрицы на матрицу перемещения
    void Scale( const m_Vec3& v);//домножение матрицы на матрицу масштабирования
    void Scale( float s );//домножение матрицы на матрицу масштабирования


    void RotateX( float a );//создание из данной матрицы матрицы вращение по 3-м осям
    void RotateY( float a );
    void RotateZ( float a );
    void MakePerspective( float aspect, float fov_y, float z_near, float z_far);//джелает эту матрицу матрицей перспективы
    void MakeProjection( float scale_x, float scale_y, float z_near, float z_far );
    void Identity();//делает матрицу еденичной

    /*True matrix - this
    0	1	2	3
    4	5	6	7
    8	9	10	11
    12	13	14	15
    */

    /*OpenGL matrix
    0	4	8	12
    1	5	9	13
    2	6	10	14
    3	7	11	15
    */

	float Vec3MatMulW( const m_Vec3& v )const; //returns .w component of vector-matrix multiplication
};

m_Vec3 operator*( const m_Vec3& v, const m_Mat4& m );




class m_Mat3
{
public:
    float value[9];

    m_Mat3	operator*( const m_Mat3& m );//умножение
    m_Mat3&	operator*=( const m_Mat3& m );//домножение
    m_Vec3	operator*( const m_Vec3& v );

    float&	operator[]( int i );
    float	operator[]( int i )const;//операция индексирования

	m_Mat3( const m_Mat4& m );
    m_Mat3() {}
    ~m_Mat3() {}
    void	Transpose();
    void Inverse();
    float Determinant();

    void Scale( const m_Vec3& v);//домножение матрицы на матрицу масштабирования
    void Scale( const float s );//домножение матрицы на матрицу масштабирования


    void RotateX( float a );//создание из данной матрицы матрицы вращение по 3-м осям
    void RotateY( float a );
    void RotateZ( float a );
    void Identity();//делает матрицу еденичной

    /*True matrix - this
    0	1	2
    3	4	5
    6	7	8
    */

    /*OpenGL matrix
    0	3	6
    1	4	7
    2	5	8
    */
};

m_Vec3 operator*( const m_Vec3& v, const m_Mat3& m );

#endif//_MATRIX_H_
