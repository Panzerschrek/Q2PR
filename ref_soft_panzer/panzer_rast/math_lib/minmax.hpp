#ifndef MINMAX_HPP
#define MINMAX_HPP

template< class T>
inline T min( T a, T b )
{
	return ( a > b ) ? b : a;
}

template< class T>
inline T max( T a, T b )
{
	return ( a < b ) ? b : a;
}

#endif//MINMAX_HPP
