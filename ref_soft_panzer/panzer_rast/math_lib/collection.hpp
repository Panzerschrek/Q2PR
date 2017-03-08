#ifndef COLLECTION_HPP
#define COLLECTION_HPP


template< class T >
class m_Collection
{
public:

    m_Collection( unsigned int init_size= 0 )
    {
        memory_size= ( init_size & 0xFFFFFFF0 ) + 16;
        data= new T[ memory_size ];
        size= init_size;
    }
    ~m_Collection()
    {
        delete[] data;
    }

    m_Collection& operator=( const m_Collection& c )
    {
        size= c.size;
        if( size > memory_size )
        {
            memory_size= size + ( size >> 2 );
            if( data != NULL )
                delete[] data;
            data= new T[ memory_size ];
        }

        for( unsigned int i= 0; i< size; i++ )
            data[i]= c.data[i];
		return *this;
    }

    void Resize( unsigned int new_size )
    {
        if( new_size >= memory_size )
        {
            unsigned int new_memory_size= 1 + new_size * 5 / 4;//+ 20% + 1
            T* new_data;
            new_data= new T[ new_memory_size ];

            for( unsigned int i= 0; i< size; i++ )
                new_data[i]= data[i];

            delete[] data;
            data= new_data;
            memory_size= new_memory_size;
        }
        size= new_size;
    }
    void AddToSize( unsigned int delta )
    {
    	Resize( size + delta );
    }


    unsigned int Size() const
    {
        return size;
    }
    unsigned int MemorySize() const
    {
        return memory_size;
    }
    unsigned int FreeSize() const
    {
        return memory_size - size;
    }

    T* Data()
    {
        return data;
    }
    T* Data() const
    {
        return data;
    }
    T* Last()
    {
    	return data + size - 1;
    }
	T* Last() const
    {
    	return data + size - 1;
    }

    void Add( T x )
    {
        if( size == memory_size )
        {
            unsigned int new_memory_size= 1 + memory_size * 5 / 4;//+ 20% + 1
            T* new_data;
            new_data= new T[ new_memory_size ];

            for( unsigned int i= 0; i< size; i++ )
                new_data[i]= data[i];

            delete[] data;
            data= new_data;
            memory_size= new_memory_size;
        }
        data[ size ]= x;
        size++;
    }

    void Remove( unsigned int n )
    {
        if( n != size - 1 )
        {
            data[n]= data[ size - 1 ];
        }
        size--;
    }

    class ConstIterator
    {
    public:
        ConstIterator( const m_Collection* c ):
            collection(c) {}

        ~ConstIterator() {}
        void Begin()
        {
            p= 0;
        }
        void End()
        {
            p= collection->Size();
        }
        void Next()
        {
            p++;
        }
        bool IsValid()
        {
            //return collection->Size() > p;
            return p < collection->Size();
        }
        const T& operator*()
        {
            return collection->Data()[p];
        }
        const T& operator->()
        {
            return collection->Data()[p];
        }
    private:
        const m_Collection* collection;
        unsigned int p;
    };

    class Iterator
    {
    public:
        Iterator( m_Collection* c )
        {
            collection= c;
            current_removed= false;
        }
        ~Iterator() {}
        void Begin()
        {
            p= 0;
        }
        void End()
        {
            p= collection->Size();
        }
        void Next()
        {
            if( !current_removed )
                p++;
            current_removed= false;
        }
        bool IsValid()
        {
            //return collection->Size() > p;
            return p < collection->Size();
        }
        T& operator*()
        {
            return collection->Data()[p];
        }
        T& operator->()
        {
            return collection->Data()[p];
        }
        void RemoveCurrent()
        {
            if( p <= collection->Size() - 1 )
                current_removed= true;
            collection->Remove(p);
        }
    private:
        m_Collection* collection;
        bool current_removed;
        unsigned int p;
    };
private:

    unsigned int size, memory_size;
    T* data;
};

#endif//COLLECTION_HPP
