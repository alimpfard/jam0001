#pragma once

#include "Vector.h"
#include <AK/HashMap.h>
#include <AK/OwnPtr.h>
#include <AK/String.h>
#include <AK/UFixedBigInt.h>
#include <AK/Variant.h>
#include <cmath>
#include <typeinfo>

enum class NativeType {
    Int,
    String,
    Any,
};

struct Number : public Variant<double, i64, u64> {
    using Variant<double, i64, u64>::Variant;

    Number(bool x)
        : Variant<double, i64, u64>(u64(x))
    {
    }

    u64 to_size() const { return to<u64>(); }

    template<typename T>
    T to() const
    {
        return visit([](auto x) -> T { return x; });
    }

    template<typename T>
    static auto mod(T v, IdentityType<T> u)
    {
        if constexpr (IsFloatingPoint<T>)
            return fmod(v, u);
        else
            return v % u;
    }

    template<typename T>
    static decltype(auto) map(T&& mapper, Number const& a, Number const& b)
    {
        return a.visit([&](auto x) {
            return b.visit([&](auto y) {
                return mapper(x, y);
            });
        });
    }

    template<typename T>
    static decltype(auto) map(T&& mapper, Number const& a)
    {
        return a.visit([&](auto x) {
            return mapper(x);
        });
    }

#define BINARY_OPERATOR_RET_CAST(op, retT)              \
    Number operator op(Number const& other) const       \
    {                                                   \
        return visit([&other](auto x) {                 \
            return other.visit([&x](auto y) -> Number { \
                return retT(x op y);                    \
            });                                         \
        });                                             \
    }

#define FN_OPERATOR_RET_CAST(op, retT, fn)              \
    Number operator op(Number const& other) const       \
    {                                                   \
        return visit([&other](auto x) {                 \
            return other.visit([&x](auto y) -> Number { \
                return retT(fn(x, y));                  \
            });                                         \
        });                                             \
    }

#define BINARY_OPERATOR(op) BINARY_OPERATOR_RET_CAST(op, )
#define OPERATOR_FN(op, fn) FN_OPERATOR_RET_CAST(op, , fn)

    BINARY_OPERATOR(+)

    BINARY_OPERATOR(-)

    BINARY_OPERATOR(*)

    BINARY_OPERATOR(/)

    OPERATOR_FN(%, mod)

    BINARY_OPERATOR_RET_CAST(<, u64)

    BINARY_OPERATOR_RET_CAST(>, u64)

    BINARY_OPERATOR_RET_CAST(==, u64)

#undef BINARY_OPERATOR

    Number operator-() const
    {
        return visit([](auto a) -> Number { return -a; });
    }
};

using NumberType = Number;

struct Type;
struct TypeName {
    String name;
    NonnullRefPtr<Type> type;
};

struct Type : public RefCounted<Type> {
    Type(Variant<Vector<TypeName>, NativeType> decl)
        : decl(move(decl))
    {
    }

    Variant<Vector<TypeName>, NativeType> decl;
};

struct CommentResolutionSet;
struct Context;
struct FunctionNode;

struct Value;
struct RecordValue {
    NonnullRefPtr<Type> type;
    Vector<Value> members;
};

struct NativeFunctionType {
    Value (*fn)(Context&, void*, size_t);

    Vector<String> comments;
};

struct Comment;

struct FunctionValue {
    NonnullRefPtr<FunctionNode> node;
    Vector<HashMap<String, Value>> scope;
    Vector<HashMap<Comment*, Vector<Value>>> comment_scope;
};

struct Value {
    template<typename T>
    Value(T&& v)
        : value(v)
    {
    }

    Value(i64 v)
        : value(NumberType(v))
    {
    }

    Value(int v)
        : value(NumberType((i64)v))
    {
    }

    Value(bool v)
        : value(NumberType((i64)v))
    {
    }

    Value(Value const& v) = default;

    Value(Value&) = default;

    Value(Value&& v) = default;

    Value& operator=(Value&&) = default;

    Value& operator=(Value const&) = default;

    Variant<Empty, NumberType, String, NonnullRefPtr<Type>, FunctionValue, NonnullRefPtr<CommentResolutionSet>, NativeFunctionType, RecordValue> value;
};

struct CommentResolutionSet : public AK::RefCounted<CommentResolutionSet> {
    Vector<Value> values;
};

struct Context {
    Vector<HashMap<String, Value>> scope;
    Vector<HashMap<Comment*, Vector<Value>>> comment_scope;
    Vector<Comment*> unassigned_comments;
    size_t last_call_scope_start { 0 };
};

Value& flatten(Value& input);

NonnullRefPtr<Type> type_from(Value const&);

template<>
struct AK::Formatter<Number> : AK::Formatter<FormatString> {
    ErrorOr<void> format(FormatBuilder& builder, Number const& number)
    {
        return number.visit([&](auto x) {
            return Formatter<decltype(x)> {}.format(builder, x);
        });
    }
};
