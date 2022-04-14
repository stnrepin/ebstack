#ifndef EBSTACK_HPP
#define EBSTACK_HPP

#include <atomic>
#include <cstdint>

#include "utils.hpp"

namespace eb
{

static std::atomic< uint64_t > g_next_thread_id;
static __thread uint64_t g_tid;

static inline void InitSystem()
{
    g_next_thread_id.store( 0 );
}

static inline void InitThread()
{
    g_tid = g_next_thread_id.fetch_add( 1 );
}

template< typename T >
class TaggedValue {
public:
    using RawType = std::uint64_t;
    using TagType = std::uint16_t;

    static constexpr TagType kMaxTag = std::numeric_limits< TagType >::max();

    TaggedValue() = default;

    explicit TaggedValue( RawType  raw ) : raw_( raw ) {}

    TaggedValue( T value, TagType tag )
        : raw_( ( reinterpret_cast< RawType >( value ) & kValueMask) |
                ( static_cast< RawType >( tag ) << 8 * ( sizeof( RawType ) - sizeof( TagType ) ) ) )
    { }

    TaggedValue( const TaggedValue& other )
    {
        (*this) = other;
    }

    TaggedValue<T>& operator=( const TaggedValue& other )
    {
        raw_ = other.raw_;
        return *this;
    }

    T GetValue() const
    {
        return reinterpret_cast< T >(
            ( raw_ & kValueMask ) |
            ( ( ( raw_ >> ( kValueBits - 1 ) ) & 0x1 ) * kExtendMask ) );
    }

    TagType GetTag() const
    {
        return static_cast< TagType >( raw_ >> kValueBits );
    }

    RawType GetRaw() const
    {
        return raw_;
    }

    bool operator==( TaggedValue<T>& other ) const
    {
        return raw_ == other.raw_;
    }

    bool operator!=( TaggedValue<T> other ) const
    {
        return !( *this == other );
    }

private:
    static constexpr uint64_t kValueBits = 8 * ( sizeof( RawType )  - sizeof( TagType ) );
    static constexpr uint64_t kValueMask = (1UL << kValueBits) - 1;
    static constexpr uint64_t kExtendMask = std::numeric_limits< RawType >::max() - kValueMask;

    RawType raw_ = 0;
};

} // namespace eb

namespace std
{

template< typename T >
class atomic< eb::TaggedValue< T > >
{
public:
    atomic( const atomic& ) = delete;
    atomic& operator=( const atomic& ) = delete;

    constexpr atomic() : raw_( 0 ) {}
    constexpr atomic( eb::TaggedValue< T > tg ) : raw_( tg.GetRaw() ) {}

    eb::TaggedValue< T > load( std::memory_order order = std::memory_order_seq_cst ) const noexcept
    {
        return eb::TaggedValue< T >( raw_.load( order ) );
    }

    bool compare_exchange_strong( eb::TaggedValue< T > ex, eb::TaggedValue< T > de,
            std::memory_order success = std::memory_order_seq_cst,
            std::memory_order failure = std::memory_order_seq_cst )
    {
        auto v  = ex.GetRaw();
        return raw_.compare_exchange_strong( v, de.GetRaw(), success, failure );
    }

private:
    std::atomic< typename eb::TaggedValue< T >::RawType > raw_;
};

};

namespace eb
{

template< typename T >
class Stack
{
public:
    Stack( int64_t thread_count, int64_t coll_size, uint64_t delay );

    bool Push( T item );
    bool Pop( T *item );
    size_t GetSize() const;

private:
    struct Node
    {
        explicit Node( T v ) : next( nullptr ), value( v ) {}

        Node* next;
        T value;
    };

    struct Operation
    {
        enum class Name
        {
            Push,
            Pop,
        };

        Name name;
        T value;
    };

    static int64_t GetCollisionPos( int64_t size );
    static uint64_t GetTime();
    bool TryCollision( uint64_t tid, uint64_t other, T* item );
    bool Backoff( Operation::Name op, T* item );

    std::unique_ptr< std::atomic< TaggedValue< Node* > > > top_;

    std::vector< std::unique_ptr< Operation > > operations_;
    std::vector< std::unique_ptr< std::atomic< uint64_t > > > location_;
    std::vector< std::unique_ptr< std::atomic< uint64_t > > > collision_;

    uint64_t thread_count_;
    uint64_t coll_size_;
    uint64_t delay_;

    static constexpr uint64_t kEmptyPos = 0;
};

template< typename T >
Stack< T >::Stack( int64_t thread_count, int64_t coll_size, uint64_t delay )
    : thread_count_( thread_count ), coll_size_( coll_size), delay_( delay )
{
    top_ = std::make_unique< std::atomic< TaggedValue< Node* > > >();

    for( uint64_t i = 0; i < thread_count_; ++i )
    {
        location_.emplace_back( std::make_unique< std::atomic< uint64_t > >() );
        operations_.emplace_back( std::make_unique< Operation >() );
    }
    for( uint64_t i = 0; i < coll_size_; ++i )
    {
        collision_.emplace_back( std::make_unique< std::atomic< uint64_t > >() );
    }
}

template< typename T >
int64_t Stack< T >::GetCollisionPos( int64_t size )
{
    return ( Rdtsc() >> 6 ) % size;
}

template< typename T >
uint64_t Stack< T >::GetTime()
{
    return Rdtsc();
}

template< typename T >
bool Stack< T >::TryCollision( uint64_t tid, uint64_t other, T* item )
{
    if( operations_[ tid ]->name == Operation::Name::Push )
    {
        return location_[ other ]->compare_exchange_weak( other, tid );
    }
    else
    {
        if( location_[ other ]->compare_exchange_weak( other, kEmptyPos ) )
        {
            *item = operations_[ other ]->value;
            return true;
        }
        return false;
    }
}

template< typename T >
bool Stack< T >::Backoff( Operation::Name name, T* item )
{
    auto tid = g_tid;
    *operations_[ tid ] = { name, *item };
    location_[ tid ]->store( tid );
    auto pos = GetCollisionPos( coll_size_ );
    auto him = collision_[ pos ]->load();

    while( !collision_[ pos ]->compare_exchange_weak( him, tid ) )
    {
        ;
    }

    if( him != kEmptyPos )
    {
        auto other = location_[ him ]->load();
        if( other == him && operations_[ other ]->name != name )
        {
            auto expected = tid;
            if( location_[ tid ]->compare_exchange_weak( expected, kEmptyPos ) )
            {
                return TryCollision( tid, other, item );
            }
            else
            {
                if( name == Operation::Name::Pop )
                {
                    *item = operations_[ location_[ tid ]->load() ]->value;
                    location_[ tid ]->store( kEmptyPos );
                }
                return true;
            }
        }
    }

    auto wait = GetTime() + delay_;
    while( GetTime() < wait )
    {
        Pause();
    }

    auto expected = tid;
    if( !location_[ tid ]->compare_exchange_strong( expected, kEmptyPos ) )
    {
        if( name == Operation::Name::Pop )
        {
            *item = operations_[ location_[ tid ]->load() ]->value;
            location_[ tid ]->store( kEmptyPos );
        }
        return true;
    }

    return false;
}

template< typename T >
bool Stack< T >::Push( T item )
{
    if( Backoff( Operation::Name::Push, &item ) )
    {
        return true;
    }

    auto n = new Node( item );
    TaggedValue< Node* > top_old;
    TaggedValue< Node* > top_new;
    while( true )
    {
        top_old = top_->load();
        n->next = top_old.GetValue();
        top_new = TaggedValue< Node* >( n, top_old.GetTag() + 1 );
        if( !top_->compare_exchange_strong( top_old, top_new ) )
        {
            if( Backoff( Operation::Name::Push, &item ) )
            {
                return true;
            }
        }
        else
        {
            break;
        }
    }
    return true;
}

template< typename T >
bool Stack< T >::Pop( T* item )
{
    if( Backoff( Operation::Name::Pop, item ) )
    {
        return true;
    }

    TaggedValue< Node* > top_old;
    TaggedValue< Node* > top_new;
    while( true )
    {
        top_old = top_->load();
        if( top_old.GetValue() == NULL )
        {
            return false;
        }

        top_new = TaggedValue< Node* >( top_old.GetValue()->next, top_old.GetTag() + 1 );
        if( !top_->compare_exchange_strong( top_old, top_new ) )
        {
            if( Backoff( Operation::Name::Pop, item ) )
            {
                return true;
            }
        }
        else
        {
            break;
        }
    }
    *item = top_old.GetValue()->value;
    return true;
}

template< typename T >
size_t Stack< T >::GetSize() const
{
    size_t size = 0;
    auto* n = top_->load().GetValue();
    while ( n != nullptr )
    {
        ++size;
        n = n->next;
    }
    return size;
}

} // namespace eb

#endif // !EBSTACK_HPP
