#pragma once

#include <string>

struct no_result {};

template <class T = no_result, class E = std::string>
class Result {
private:
    bool m_succesful;
    union {
        T m_value;
        E m_error;
    };
    Result(const T value) : m_succesful(true), m_value(value) {}
    Result(const E error, int dummy) : m_succesful(false), m_error(error) {}

public:
    ~Result() {
        if (m_succesful) {
            if (std::is_destructible<T>::value) {
                m_value.~T();
            }
        } else {
            if (std::is_destructible<E>::value) {
                m_error.~E();
            }
        }
    }
    Result(Result<T, E> const& other) {
        if (other.m_succesful) {
            this->m_succesful = true;
            new (&this->m_value) T(other.value());
        } else {
            this->m_succesful = false;
            new (&this->m_error) E(other.error());
        }
    }
    template<class T2, class E2>
    Result(Result<T2, E2> const& other) {
        if (other.is_value()) {
            this->m_succesful = true;
            if constexpr (!std::is_same<T, no_result>::value) {
                new (&this->m_value) T(other.value());
            }
        } else {
            this->m_succesful = false;
            new (&this->m_error) E(other.error());
        }
    }

    bool ok() const { return m_succesful; }
    auto value() const { return m_value; }
    auto error() const { return m_error; }

    operator bool() const { return this->m_succesful; }

    static auto Ok(const T value) { return Result<T>(value); }
    static auto Err(E error) { return Result<T>(error, 0); }
};

template <class T = no_result>
Result<T> Ok(T value = T()) {
    return Result<T>::Ok(value);
}

template <class E = std::string>
struct Err {
    const E _value;
    Err(const E value) : _value(value) {}
    template <class T>
    operator Result<T>() const {
        return Result<T>::Err(_value);
    }
};
