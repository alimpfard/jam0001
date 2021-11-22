#include "parser.h"
#include <AK/Format.h>
#include <AK/Function.h>
#include <AK/Random.h>
#include <AK/StringView.h>
#include <AK/TypeCasts.h>
#include <errno.h>
#include <string.h>

static StringView g_program_name;

int print_help(bool as_failure = false)
{
    outln("{} v0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0", g_program_name);
    outln("  usage: {} <source_file>", g_program_name);
    outln("    <source_file> can also be `-` to read from stdin");
    outln("That's it.");
    return as_failure ? 1 : 0;
}

Value lang$print(Context&, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    bool first = true;
    Function<void(Value const&)> print_value = [&](Value const& value) {
        value.value.visit(
            [](Empty) { out("<empty>"); },
            [](FunctionValue const&) { out("<fn ref>"); }, // FIXME
            [&](NonnullRefPtr<Type> const& type) {
                if (type->decl.has<NativeType>()) {
                    switch (type->decl.get<NativeType>()) {
                    case NativeType::Int:
                        out("int");
                        break;
                    case NativeType::String:
                        out("string");
                        break;
                    case NativeType::Any:
                        out("any");
                        break;
                    }
                } else {
                    auto& rec = type->decl.get<Vector<TypeName>>();
                    out("record {{");
                    for (auto& entry : rec) {
                        out(" {}: ", entry.name);
                        print_value({ entry.type });
                    }
                    out(" }}");
                }
            },
            [&print_value](NonnullRefPtr<CommentResolutionSet> const& rs) {
                out("<Comment resolution set: {{");
                auto first = true;
                for (auto& entry : rs->values) {
                    if (!first)
                        out(", ");
                    first = false;
                    print_value(entry);
                }
                out("}}>");
            },
            [](NativeFunctionType const& fnptr) { out("<fnptr at {:p}>", fnptr.fn); },
            [&](RecordValue const& rv) {
                out("(");
                auto first = true;
                for (auto& entry : rv.members) {
                    if (!first)
                        out(" ");
                    first = false;
                    print_value(entry);
                }
                out(")");
            },
            [](auto const& value) { out("{}", value); });
    };
    for (auto& arg : args) {
        if (!first)
            out(" ");
        print_value(arg);
        first = false;
    }
    outln();
    return { Empty {} };
}

template<typename Operator>
static void fold_append(auto& accumulator, auto&& arg)
{
    Variant<Empty, NumberType, String, NonnullRefPtr<Type>, FunctionValue, NativeFunctionType, RecordValue> value { Empty {} };
    if constexpr (requires { arg.template has<NumberType>(); }) {
        if (arg.template has<NonnullRefPtr<CommentResolutionSet>>()) {
            for (auto& entry : arg.template get<NonnullRefPtr<CommentResolutionSet>>()->values)
                fold_append<Operator>(accumulator, entry.value);
            return;
        } else {
            value = arg.template downcast<Empty, NumberType, String, NonnullRefPtr<Type>, FunctionValue, NativeFunctionType, RecordValue>();
        }
    } else if constexpr (IsSame<RemoveCVReference<decltype(arg)>, NumberType> || IsSame<RemoveCVReference<decltype(arg)>, String>) {
        value = arg;
    }

    accumulator.visit(
        [&](Empty) {
            accumulator = value.template downcast<Empty, NumberType, String, NonnullRefPtr<Type>, FunctionValue, NonnullRefPtr<CommentResolutionSet>, NativeFunctionType, RecordValue>();
        },
        [&]<typename U>(U const& accumulator_value) {
            value.visit(
                [&]<typename T>(T const& value) {
                    if constexpr (IsCallableWithArguments<Operator, U, T>)
                        accumulator = Operator {}(accumulator_value, value);
                });
        });
};

template<typename Operator>
Value lang$fold_op(Context&, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    Variant<Empty, NumberType, String, NonnullRefPtr<Type>, FunctionValue, NonnullRefPtr<CommentResolutionSet>, NativeFunctionType, RecordValue> accumulator { Empty {} };
    for (auto& arg : args)
        fold_append<Operator>(accumulator, arg.value);
    return { accumulator };
}

static void add_append(auto& accumulator, auto&& arg)
{
    Variant<Empty, NumberType, String> value { Empty {} };
    if constexpr (requires { arg.template has<NumberType>(); }) {
        if (arg.template has<Empty>() || arg.template has<NumberType>() || arg.template has<String>())
            value = arg.template downcast<Empty, NumberType, String>();
        else if (arg.template has<NonnullRefPtr<CommentResolutionSet>>()) {
            for (auto& entry : arg.template get<NonnullRefPtr<CommentResolutionSet>>()->values)
                add_append(accumulator, entry.value);
            return;
        }
    } else {
        value = arg;
    }

    if (accumulator.template has<Empty>()) {
        accumulator = value;
    } else if (accumulator.template has<String>()) {
        if (!value.template has<Empty>()) {
            StringBuilder builder;
            builder.append(accumulator.template get<String>());
            value.visit([&](auto& x) { builder.appendff("{}", x); }, [](Empty) {});
            accumulator = builder.build();
        }
    } else if (accumulator.template has<NumberType>()) {
        if (value.template has<NumberType>()) {
            accumulator = accumulator.template get<NumberType>() + value.template get<NumberType>();
        } else if (value.template has<String>()) {
            StringBuilder builder;
            value.visit([&](auto& x) { builder.appendff("{}", x); }, [](Empty) {});
            builder.append(value.get<String>());
            accumulator = builder.build();
        }
    }
};

Value lang$add(Context&, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    Variant<Empty, NumberType, String> accumulator { Empty {} };
    for (auto& arg : args) {
        arg.value.visit(
            [&](Empty) { add_append(accumulator, String("<empty>"sv)); },
            [&](FunctionValue const&) { add_append(accumulator, String("<function>"sv)); },
            [&](NonnullRefPtr<Type> const&) { add_append(accumulator, String("<type>"sv)); },
            [&](NonnullRefPtr<CommentResolutionSet> const& crs) {
                for (auto& entry : crs->values)
                    add_append(accumulator, entry.value);
            },
            [&](NativeFunctionType const&) { add_append(accumulator, String("<fn>"sv)); },
            [&](RecordValue const& rv) { add_append(accumulator, String("<record>"sv)); },
            [&](auto const& value) { add_append(accumulator, value); });
    }
    return { move(accumulator).downcast<Empty, NumberType, String, NonnullRefPtr<Type>, FunctionValue, NonnullRefPtr<CommentResolutionSet>, NativeFunctionType, RecordValue>() };
}

static bool truth(Value const& condition)
{
    return condition.value.visit(
        [](Empty) -> bool { return false; },
        [](FunctionValue const&) -> bool { return true; },
        [](NonnullRefPtr<Type> const&) -> bool { return true; },
        [](NonnullRefPtr<CommentResolutionSet> const& crs) -> bool {
            return all_of(crs->values, truth);
        },
        [](NativeFunctionType const&) -> bool { return true; },
        [](RecordValue const&) { return true; },
        [](auto const& value) -> bool {
            if constexpr (requires { (bool)value; })
                return (bool)value;
            else if constexpr (requires { value.is_empty(); })
                return !value.is_empty();
            else
                return true;
        });
}

Value& flatten(Value& input)
{
    if (auto ptr = input.value.get_pointer<NonnullRefPtr<CommentResolutionSet>>()) {
        if ((*ptr)->values.size() == 1)
            return flatten((*ptr)->values.first());
    }

    return input;
}

Value lang$cond(Context&, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    size_t i = 0;
    for (; i + 1 < count; i += 2) {
        auto& condition = args[i];
        auto& value = args[i + 1];
        if (truth(condition))
            return value;
    }
    if (i < count)
        return args[count - 1];

    return { Empty {} };
}

Value lang$is(Context&, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    if (args.size() < 2)
        return { Empty {} };

    auto& value = args[0];
    if (!value.value.has<FunctionValue>())
        return { Empty {} };

    auto& query = args[1];
    if (!query.value.has<String>())
        return { Empty {} };

    auto words = query.value.get<String>().split(' ');

    auto& fn = value.value.get<FunctionValue>();
    Vector<bool> found;
    found.resize(words.size());

    for (auto entry : fn.node->body()) {
        auto const* ptr = entry.ptr();
        if (is<Statement>(*entry))
            ptr = static_cast<Statement const*>(ptr)->node().ptr();

        if (!is<Comment>(*ptr))
            continue;

        auto comment = static_cast<Comment const*>(ptr);
        for (auto it = words.begin(); it != words.end(); ++it) {
            if (found[it.index()])
                continue;
            if (comment->text().contains(*it))
                found[it.index()] = true;
        }

        if (all_of(found, [](auto x) { return x; }))
            break;
    }

    if (all_of(found, [](auto x) { return x; }))
        return { 1 };

    return { 0 };
}

Value lang$loop(Context& context, void* ptr, size_t count)
{
    // loop(start, step_fn, stop_cond) :: v=start; while(!stop_cond(v)) v = step_cond(v); return v;
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    if (args.size() < 3)
        return { Empty {} };

    auto value = args[0];
    auto& step = args[1];
    auto& stop = args[2];

    auto step_fn = [&] {
        value = make_ref_counted<Call>(
            static_ptr_cast<ASTNode>(make_ref_counted<SyntheticNode>(step)),
            Vector { static_ptr_cast<ASTNode>(make_ref_counted<SyntheticNode>(value)) })
                    ->run(context);
    };

    auto stop_fn = [&] {
        auto res = make_ref_counted<Call>(
            static_ptr_cast<ASTNode>(make_ref_counted<SyntheticNode>(stop)),
            Vector { static_ptr_cast<ASTNode>(make_ref_counted<SyntheticNode>(value)) })
                       ->run(context);
        return truth(res);
    };

    while (!stop_fn())
        step_fn();

    return value;
}

Value lang$get(Context& context, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    if (args.size() < 2)
        return { Empty {} };

    auto& index = flatten(args[0]).value;
    auto& subject = flatten(args[1]);

    return index.visit(
        [&](NumberType index) {
            return subject.value.visit(
                [&](String const& str) {
                    return Value { String::repeated(str[index.to_size()], 1) };
                },
                [&](auto&) {
                    return Value { Empty {} };
                });
        },
        [&](String& field) {
            return make_ref_counted<MemberAccess>(field, static_ptr_cast<ASTNode>(make_ref_counted<SyntheticNode>(subject)))->run(context);
        },
        [](auto&) { return Value { Empty {} }; });
}

Value lang$slice(Context&, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    if (args.size() < 3)
        return { Empty {} };

    auto index = flatten(args[0]).value.get_pointer<NumberType>();
    auto size = flatten(args[1]).value.get_pointer<NumberType>();
    auto subject = flatten(args[2]).value.get_pointer<String>();

    if (!index || !size || !subject)
        return { Empty {} };

    return { subject->substring(index->to_size(), size->to_size()) };
}

Value lang$typeof(Context&, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    if (args.size() != 1)
        return { Empty {} };

    return { type_from(args[0]) };
}

Value lang$append(Context&, void* ptr, size_t count)
{
    Span<Value> args { reinterpret_cast<Value*>(ptr), count };
    if (args.size() < 2)
        return { Empty {} };

    auto& value = flatten(args[0]);
    auto subject = flatten(args[1]);

    if (!subject.value.has<RecordValue>())
        return subject;

    auto& rv = subject.value.get<RecordValue>();
    auto types_ptr = rv.type->decl.get_pointer<Vector<TypeName>>();
    if (!types_ptr)
        return subject;

    auto last_name = types_ptr->is_empty() ? String("_"sv) : types_ptr->last().name;
    size_t field = 0;
    if (sscanf(last_name.characters(), "_%zu", &field) != 1)
        field = types_ptr->size();

    types_ptr->append({ String::formatted("_{}", field + 1), type_from(value) });
    rv.members.append(value);

    if (types_ptr->first().name == "length" && types_ptr->first().type->decl.has<NativeType>())
        rv.members.first() = { rv.members.first().value.get<NumberType>() + NumberType(u64(1)) };
    return subject;
}

struct Sub {
    NumberType operator()(NumberType a, NumberType b) { return a - b; }
    NumberType operator()(String const&, String const&) { return (u64)0; }
};

struct Mul {
    NumberType operator()(NumberType a, NumberType b) { return a * b; }
    NumberType operator()(String const&, String const&) { return (u64)0; }
};

struct Div {
    NumberType operator()(NumberType a, NumberType b) { return a / b; }
    NumberType operator()(String const&, String const&) { return (u64)0; }
};

struct Mod {
    NumberType operator()(NumberType a, NumberType b) { return a % b; }
    NumberType operator()(String const&, String const&) { return (u64)0; }
};

struct Greater {
    NumberType operator()(NumberType a, NumberType b) { return a > b; }
    NumberType operator()(String const& a, String const& b) { return a > b; }
};

struct Equal {
    NumberType operator()(NumberType a, NumberType b) { return a == b; }
    NumberType operator()(String const& a, String const& b) { return u64(a == b); }
    NumberType operator()(NonnullRefPtr<Type> const& a, NonnullRefPtr<Type> const& b)
    {
        if (a.ptr() == b.ptr())
            return true;

        if (auto ptr = a->decl.get_pointer<NativeType>()) {
            auto b_ptr = b->decl.get_pointer<NativeType>();
            return b_ptr && *ptr == *b_ptr;
        }

        if (b->decl.has<NativeType>())
            return false;

        auto& a_type = a->decl.get<Vector<TypeName>>();
        auto& b_type = b->decl.get<Vector<TypeName>>();
        if (a_type.size() != b_type.size())
            return false;

        return all_of(a_type, [&b_type, i = size_t { 0 }](auto& a) {
            auto& b = b_type[const_cast<size_t&>(i)++];
            return a.name == b.name && Equal {}(a.type, b.type).template to<bool>();
        });
    }
};

struct Flat {
    template<typename T>
    T operator()(T a, T b)
    {
        if (get_random<bool>())
            return a;
        return b;
    }
};

struct Max {
    NumberType operator()(NumberType a, NumberType b)
    {
        return Number::map([](auto a, auto b) -> Number { return max(a, b); }, a, b);
    }
    String operator()(String const& a, String const& b) { return max(a, b); }
    String operator()(NumberType a, String const& b)
    {
        return max(Number::map([](auto a) { return String::number(a); }, a), b);
    }
    String operator()(String const& a, NumberType b) { return this->operator()(b, a); }
};

struct Min {
    NumberType operator()(NumberType a, NumberType b)
    {
        return Number::map([](auto a, auto b) -> Number { return min(a, b); }, a, b);
    }
    String operator()(String const& a, String const& b) { return min(a, b); }
    String operator()(NumberType a, String const& b)
    {
        return min(Number::map([](auto a) { return String::number(a); }, a), b);
    }
    String operator()(String const& a, NumberType b) { return this->operator()(b, a); }
};

void initialize_base(Context& context)
{
    context.scope.empend();
    context.comment_scope.empend();
    context.last_call_scope_start = 0;

    auto& scope = context.scope.last();

    scope.set("print", { NativeFunctionType { lang$print, { "print function", "native operation" } } });
    scope.set("add", { NativeFunctionType { lang$add, { "native arithmetic addition operation" } } });
    scope.set("sub", { NativeFunctionType { lang$fold_op<Sub>, { "native arithmetic subtract operation" } } });
    scope.set("mul", { NativeFunctionType { lang$fold_op<Mul>, { "native arithmetic multiply operation" } } });
    scope.set("div", { NativeFunctionType { lang$fold_op<Div>, { "native arithmetic divide operation" } } });
    scope.set("mod", { NativeFunctionType { lang$fold_op<Mod>, { "native arithmetic modulus operation" } } });
    scope.set("cond", { NativeFunctionType { lang$cond, { "native conditional selection operation" } } });
    scope.set("is", { NativeFunctionType { lang$is, { "native comment query operation" } } });
    scope.set("loop", { NativeFunctionType { lang$loop, { "native loop flow operation" } } });
    scope.set("gt", { NativeFunctionType { lang$fold_op<Greater>, { "native comparison greater_than operation" } } });
    scope.set("eq", { NativeFunctionType { lang$fold_op<Equal>, { "native comparison equality operation" } } });
    scope.set("max", { NativeFunctionType { lang$fold_op<Max>, { "native comparison maximum operation" } } });
    scope.set("min", { NativeFunctionType { lang$fold_op<Min>, { "native comparison minimum operation" } } });
    scope.set("collapse", { NativeFunctionType { lang$fold_op<Flat>, { "native probability collapse flatten operation" } } });
    scope.set("get", { NativeFunctionType { lang$get, { "native indexing operation" } } });
    scope.set("slice", { NativeFunctionType { lang$slice, { "native string slicing operation" } } });
    scope.set("append", { NativeFunctionType { lang$append, { "native meta append operation" } } });

    // types
    scope.set("int", { make_ref_counted<Type>(NativeType::Int) });
    scope.set("string", { make_ref_counted<Type>(NativeType::String) });
    scope.set("any", { make_ref_counted<Type>(NativeType::Any) });

    scope.set("typeof", { NativeFunctionType { lang$typeof, { "native meta typeof operation" } } });
}

int main(int argc, char** argv)
{
    bool repl_mode = false;

    g_program_name = argv[0];
    if (argc == 1)
        return print_help();

    if ("--repl"sv == argv[1]) {
        repl_mode = true;
    } else {
        auto source_file = argv[1];
        if (source_file != "-"sv) {
            auto file = freopen(source_file, "r", stdin);
            if (!file) {
                warnln("Failed to open {}: {}", source_file, strerror(errno));
                return 1;
            }
        }
    }

    auto lexer = Lexer {};
#if 0
    for (;;) {
        auto maybe_token = lexer.next();
        if (maybe_token.is_error()) {
            warnln("Lex error: {} at {}:{}", maybe_token.error().error, maybe_token.error().where.line, maybe_token.error().where.column);
            return 1;
        }
        if (maybe_token.value().type == Token::Type::Eof)
            break;

        outln("- {}@{}:{} '{}'", to_underlying(maybe_token.value().type), maybe_token.value().source_range.start.line, maybe_token.value().source_range.start.column, maybe_token.value().text);
    }
#else
    auto parser = Parser { lexer };
    Context context;
    initialize_base(context);

    do {
        if (repl_mode)
            out("> ");
        auto nodes = parser.parse_toplevel(false, repl_mode);
        if (nodes.is_error()) {
            warnln("Parse error: {} at {}:{}", nodes.error().error, nodes.error().where.line, nodes.error().where.column);
            if (!repl_mode)
                return 1;
        }

#    if 0
    for (auto& node : nodes.value())
        node->dump(0);
#    else
        for (auto& node : nodes.value())
            node->run(context);
#    endif
#endif
}
while (repl_mode)
    ;

return 0;
}
