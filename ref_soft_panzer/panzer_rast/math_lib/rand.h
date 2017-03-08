#ifndef RAND_HP
#define RAND_HP

class m_Rand
{
public:
	m_Rand();
	m_Rand( unsigned int s );

	void SetSeed( unsigned int s );
	unsigned int Rand();
	float RandIdentity();//returns random value in range [0.0f;1.0f]

	unsigned int operator()();

	const static unsigned int max_rand;
private:
	unsigned int x;
};

inline void m_Rand::SetSeed( unsigned int s )
{
	x= s;
}
inline m_Rand::m_Rand( unsigned int s )
{
	x= s;
}
inline m_Rand::m_Rand()
{
	x= 0;
}

inline unsigned int m_Rand::Rand()
{
	x= ( ( 22695477 * x + 1 ) & 0x7FFFFFFF );
	return x>>16;
}

inline unsigned int m_Rand::operator()()
{
	return Rand();
}

inline float m_Rand::RandIdentity()
{
	return float( Rand() ) / float( max_rand );
}

#endif//RAND_H
