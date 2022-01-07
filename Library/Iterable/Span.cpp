#pragma once

#include "Iterable/Iterable.cpp"

namespace Core
{
    namespace Iterable
    {
        // Destructor must be deterministic
        template <typename T>
        class Span
        {
        private:
            T *_Content = nullptr;
            size_t _Length = 0;

            _FORCE_INLINE inline T &_ElementAt(size_t Index)
            {
                return this->_Content[Index];
            }

            _FORCE_INLINE inline const T &_ElementAt(size_t Index) const
            {
                return this->_Content[Index];
            }

        public:
            Span() = default;
            Span(size_t Size) : _Content(new T[Size]), _Length(Size) {}
            Span(Span &&Other) : _Content(Other._Content), _Length(Other._Length) {}
            Span(const Span &Other) : _Content(new T[Other._Length]), _Length(Other._Length)
            {
                for (size_t i = 0; i < Other._Length; i++)
                {
                    _Content[i] = Other._Content[i];
                }
            }

            Span(const T *Array, size_t Size, bool AutoFree = true) : _Content(new T[Size]), _Length(Size) 
            {
                for (size_t i = 0; i < Size; i++)
                {
                    _Content[i] = Array[i];
                }
                
            }

            ~Span()
            {
                Free();
            }

            T *Content() const
            {
                return _Content;
            }

            size_t Length() const
            {
                return _Length;
            }

            void Free()
            {
                delete[] _Content;
                _Content = nullptr;
            }

            bool Contains(const T &Item) const
            {
                for (size_t i = 0; i < _Length; i++)
                {
                    if (_ElementAt(i) == Item)
                        return true;
                }

                return false;
            }

            bool Contains(const T &Item, int &Index) const
            {
                for (size_t i = 0; i < _Length; i++)
                {
                    if (_ElementAt(i) == Item)
                    {
                        Index = i;
                        return true;
                    }
                }

                return false;
            }

            void ForEach(std::function<void(T &)> Action)
            {
                for (size_t i = 0; i < _Length; i++)
                {
                    Action(_ElementAt(i));
                }
            }

            void ForEach(std::function<void(size_t, T &)> Action)
            {
                for (size_t i = 0; i < _Length; i++)
                {
                    Action(i, _ElementAt(i));
                }
            }

            void ForEach(std::function<void(const T &)> Action) const
            {
                for (size_t i = 0; i < _Length; i++)
                {
                    Action(_ElementAt(i));
                }
            }

            void ForEach(std::function<void(size_t, const T &)> Action) const
            {
                for (int i = 0; i < _Length; i++)
                {
                    Action(i, _ElementAt(i));
                }
            }

            T &operator[](const size_t &Index)
            {
                if (Index >= _Length)
                    throw std::out_of_range("");

                return _ElementAt(Index);
            }

            const T &operator[](const size_t &Index) const
            {
                if (Index >= _Length)
                    throw std::out_of_range("");

                return _ElementAt(Index);
            }

            Span &operator=(const Span &Other)
            {
                _Length = Other._Length;

                delete[] _Content;
                _Content = new T[_Length];

                for (size_t i = 0; i < _Length; i++)
                {
                    _Content[i] = Other._Content[i];
                }

                return *this;
            }

            Span &operator=(Span &&Other)
            {
                _Length = Other._Length;
                std::swap(_Content, Other._Content);

                return *this;
            }

            // Funtionalities

            std::string ToString()
            {
                std::stringstream ss;

                for (size_t i = 0; i < this->_Length; i++)
                {
                    ss << this->_Content[i] << '\n';
                }

                return ss.str();
            }

            // Operators

            bool operator==(const Span &Other) noexcept
            {
                if (_Content == Other._Content)
                    return true;

                for (size_t i = 0; i < _Length; i++)
                {
                    if (_Content[i] != Other._Content[i])
                        return false;
                }

                return true;
            }

            bool operator!=(const Span &Other) noexcept
            {
                if (_Content != Other._Content)
                    return true;

                for (size_t i = 0; i < _Length; i++)
                {
                    if (_Content[i] == Other._Content[i])
                        return false;
                }

                return true;
            }

            friend std::ostream &operator<<(std::ostream &os, const Span &List)
            {
                // Check for << opeartor

                for (size_t i = 0; i < List._Length; i++)
                {
                    os << List._ElementAt(i);
                }

                return os;
            }
        };
    }
}