#ifndef STACK_WRAPPER_HPP
#define STACK_WRAPPER_HPP

#include <stack>
#include <mutex>

#include "ebstack.hpp"

template< typename TStack >
class StackWrapper;

template< typename T >
class StackWrapper< eb::Stack< T > >
{
public:
    void Push( T v );
    void Pop();
    size_t GetSize() const;

private:
    eb::Stack< int > s_{ 200, 32, 500 };
};

template< typename T >
void StackWrapper< eb::Stack< T > >::Push( T v )
{
    s_.Push( v );
}

template< typename T >
void StackWrapper< eb::Stack< T > >::Pop()
{
    T r;
    s_.Pop( &r );
}

template< typename T >
size_t StackWrapper< eb::Stack< T > >::GetSize() const
{
    return s_.GetSize();
}

template< typename T >
class StackWrapper< std::stack< T > >
{
public:
    void Push( T v );
    void Pop();
    size_t GetSize() const;

private:
    std::stack< T > s_;
    std::mutex mutex_;
};

template< typename T >
void StackWrapper< std::stack< T > >::Push( T v )
{
    std::lock_guard< std::mutex > guard( mutex_ );
    s_.push( v );
}

template< typename T >
void StackWrapper< std::stack< T > >::Pop()
{
    std::lock_guard< std::mutex > guard( mutex_ );
    s_.pop();
}

template< typename T >
size_t StackWrapper< std::stack< T > >::GetSize() const
{
    return static_cast< int >( s_.size() );
}

#endif // !MAP_WRAPPER_HPP

