#pragma once

#include "bigint.h"
#include <AK/HashMap.h>
#include <AK/OwnPtr.h>
#include <AK/String.h>
#include <AK/UFixedBigInt.h>
#include <AK/Variant.h>
#include <AK/Vector.h>
#include <typeinfo>

enum class NativeType {
    Int,
    String,
    Any,
};

using NumberType = FixedBigInt<u4096>;

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
        : value(NumberType((u64)v))
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
